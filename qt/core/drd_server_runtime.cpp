#include "core/drd_server_runtime.h"

#include "capture/drd_capture_manager.h"
#include "encoding/drd_encoding_manager.h"
#include "security/drd_tls_credentials.h"

#include <QDebug>

/**
 * @brief 构造函数
 * 
 * 功能：初始化运行时对象的成员。
 * 逻辑：创建捕获/编码/输入子模块，初始化标志位与默认传输模式。
 * 参数：parent 父对象。
 * 外部接口：Qt QObject 构造函数。
 */
DrdServerRuntime::DrdServerRuntime(QObject *parent)
    : QObject(parent)
    , m_capture(new DrdCaptureManager(this))
    , m_encoder(new DrdEncodingManager(this))
    , m_input(nullptr)
    , m_tlsCredentials(nullptr)
    , m_hasEncodingOptions(false)
    , m_streamRunning(false)
    , m_transportMode(DrdFrameTransport::GraphicsPipeline)
{
}

/**
 * @brief 析构函数
 * 
 * 功能：清理运行时持有的模块资源。
 * 逻辑：Qt 会自动清理子对象。
 * 参数：无。
 * 外部接口：Qt QObject 析构函数。
 */
DrdServerRuntime::~DrdServerRuntime()
{
    // Qt 会自动清理子对象
}

/**
 * @brief 准备流
 *
 * 功能：准备捕获/编码/输入流水线并启动捕获线程。
 * 逻辑：若已运行则直接返回；缓存编码配置并设置默认传输模式；依次准备编码器、输入分发器与捕获管理器。
 * 参数：encodingOptions 编码选项，error 错误输出。
 * 外部接口：调用子模块的 prepare/start 方法。
 */
bool DrdServerRuntime::prepareStream(const DrdEncodingOptions *encodingOptions, QString *error)
{
    qInfo() << "[PRODUCER] DrdServerRuntime::prepareStream() - Starting stream preparation";
    
    if (m_streamRunning)
    {
        qInfo() << "[PRODUCER] Stream already running, skipping preparation";
        return true;
    }

    if (encodingOptions == nullptr)
    {
        qWarning() << "[PRODUCER] Encoding options is null";
        if (error)
        {
            *error = "Encoding options is null";
        }
        return false;
    }

    m_encodingOptions = *encodingOptions;
    m_hasEncodingOptions = true;
    m_transportMode = DrdFrameTransport::GraphicsPipeline;
    
    qInfo() << "[PRODUCER] Encoding options cached - width:" << encodingOptions->width
            << "height:" << encodingOptions->height
            << "mode:" << drdEncodingModeToString(encodingOptions->mode);

    // 准备编码器
    qInfo() << "[PRODUCER] Preparing encoder...";
    if (!m_encoder->prepare(encodingOptions, error))
    {
        qWarning() << "[PRODUCER] Failed to prepare encoder:" << (error ? *error : "unknown error");
        return false;
    }
    qInfo() << "[PRODUCER] Encoder prepared successfully";

    // 启动捕获
    qInfo() << "[PRODUCER] Starting capture with size" << encodingOptions->width << "x" << encodingOptions->height;
    if (!m_capture->start(encodingOptions->width, encodingOptions->height, error))
    {
        qWarning() << "[PRODUCER] Failed to start capture:" << (error ? *error : "unknown error");
        m_encoder->reset();
        return false;
    }
    qInfo() << "[PRODUCER] Capture started successfully";

    m_streamRunning = true;
    qInfo() << "[PRODUCER] Stream preparation completed, stream running:" << m_streamRunning;
    return true;
}

/**
 * @brief 停止运行时
 * 
 * 功能：停止正在运行的捕获/编码流水线。
 * 逻辑：若未运行则返回；清除运行标志后停止捕获、重置编码器并刷新/停止输入分发器。
 * 参数：无。
 * 外部接口：调用子模块的 stop/reset 方法。
 */
void DrdServerRuntime::stop()
{
    if (!m_streamRunning)
    {
        return;
    }

    m_streamRunning = false;

    m_capture->stop();
    m_encoder->reset();
}

/**
 * @brief 设置传输模式
 * 
 * 功能：切换帧传输模式并在变更时请求关键帧。
 * 逻辑：更新传输模式；若实际发生切换则触发编码器关键帧。
 * 参数：transport 目标传输模式。
 * 外部接口：更新成员变量，调用编码器方法。
 */
void DrdServerRuntime::setTransport(DrdFrameTransport transport)
{
    if (m_transportMode == transport)
    {
        return;
    }

    m_transportMode = transport;

    if (m_encoder)
    {
        m_encoder->forceKeyframe();
    }
}

/**
 * @brief 获取编码选项
 * 
 * 功能：获取已缓存的编码参数。
 * 逻辑：若未设置编码选项则返回 false；否则将结构体复制到输出参数。
 * 参数：outOptions 输出编码选项。
 * 外部接口：无额外外部库。
 */
bool DrdServerRuntime::getEncodingOptions(DrdEncodingOptions *outOptions) const
{
    if (outOptions == nullptr || !m_hasEncodingOptions)
    {
        return false;
    }

    *outOptions = m_encodingOptions;
    return true;
}

/**
 * @brief 设置编码选项
 * 
 * 功能：写入编码参数并检测几何变化。
 * 逻辑：缓存新配置并标记已设置；若几何或模式变化且流已运行则提示需要重启。
 * 参数：encodingOptions 新编码配置。
 * 外部接口：更新成员变量，日志输出。
 */
void DrdServerRuntime::setEncodingOptions(const DrdEncodingOptions *encodingOptions)
{
    if (encodingOptions == nullptr)
    {
        return;
    }

    const bool hadOptions = m_hasEncodingOptions;
    const bool optionsChanged = hadOptions &&
                               (m_encodingOptions.width != encodingOptions->width ||
                                m_encodingOptions.height != encodingOptions->height ||
                                m_encodingOptions.mode != encodingOptions->mode ||
                                m_encodingOptions.enableFrameDiff != encodingOptions->enableFrameDiff ||
                                m_encodingOptions.h264Bitrate != encodingOptions->h264Bitrate ||
                                m_encodingOptions.h264Framerate != encodingOptions->h264Framerate ||
                                m_encodingOptions.h264Qp != encodingOptions->h264Qp ||
                                m_encodingOptions.h264HwAccel != encodingOptions->h264HwAccel ||
                                m_encodingOptions.h264VmSupport != encodingOptions->h264VmSupport ||
                                m_encodingOptions.gfxLargeChangeThreshold != encodingOptions->gfxLargeChangeThreshold ||
                                m_encodingOptions.gfxProgressiveRefreshInterval != encodingOptions->gfxProgressiveRefreshInterval ||
                                m_encodingOptions.gfxProgressiveRefreshTimeoutMs != encodingOptions->gfxProgressiveRefreshTimeoutMs);

    m_encodingOptions = *encodingOptions;
    m_hasEncodingOptions = true;

    if (optionsChanged && m_streamRunning)
    {
        // TODO: 输出警告日志
        // DRD_LOG_WARNING("Server runtime encoding options changed while stream active, restart required");
    }
}

/**
 * @brief 设置 TLS 凭据
 * 
 * 功能：设置 TLS 凭据。
 * 逻辑：引用计数新凭据并替换旧值。
 * 参数：credentials TLS 凭据。
 * 外部接口：Qt 对象管理。
 */
void DrdServerRuntime::setTlsCredentials(DrdTlsCredentials *credentials)
{
    m_tlsCredentials = credentials;
}

/**
 * @brief 请求关键帧
 * 
 * 功能：请求编码器生成关键帧。
 * 逻辑：直接调用编码管理器的强制关键帧接口。
 * 参数：无。
 * 外部接口：调用编码管理器方法。
 */
void DrdServerRuntime::requestKeyframe()
{
    if (m_encoder)
    {
        m_encoder->forceKeyframe();
    }
}

/**
 * @brief 拉取编码帧（Surface GFX）
 *
 * 功能：从捕获队列拉取帧并编码为 Surface GFX 格式。
 * 逻辑：从捕获管理器等待帧，然后使用编码管理器编码并发送。
 * 参数：settings FreeRDP 设置，context Rdpgfx 上下文，surfaceId Surface ID，timeoutUs 超时时间，frameId 帧序号，h264 输出是否使用 H264，error 错误输出。
 * 外部接口：DrdCaptureManager::waitFrame，DrdEncodingManager::encodeSurfaceGfx。
 * 返回值：成功返回 true。
 */
bool DrdServerRuntime::pullEncodedFrameSurfaceGfx(rdpSettings *settings,
                                                   RdpgfxServerContext *context,
                                                   quint16 surfaceId,
                                                   qint64 timeoutUs,
                                                   quint32 frameId,
                                                   bool *h264,
                                                   QString *error)
{
    if (m_capture == nullptr || m_encoder == nullptr)
    {
        qWarning() << "[CONSUMER] Capture or encoder not initialized - capture:" << (m_capture != nullptr)
                  << "encoder:" << (m_encoder != nullptr);
        if (error)
        {
            *error = "Capture or encoder not initialized";
        }
        return false;
    }

    // 从捕获队列等待帧
    DrdFrame *frame = nullptr;
    if (!m_capture->waitFrame(timeoutUs, &frame, error))
    {
        if (error && !error->contains("Timed out") && !error->contains("no data"))
        {
            qWarning() << "[CONSUMER] Failed to wait for frame:" << (error ? *error : "unknown error");
        }
        return false;
    }

    if (frame == nullptr)
    {
        qWarning() << "[CONSUMER] Frame is null after waitFrame";
        if (error)
        {
            *error = "Frame is null";
        }
        return false;
    }
    
    // qDebug() << "[CONSUMER] Frame received - frameId:" << frameId
    //         << "width:" << frame->width()
    //         << "height:" << frame->height()
    //         << "stride:" << frame->stride();

    // 编码帧
    bool success = m_encoder->encodeSurfaceGfx(settings, context, surfaceId, frame, frameId, h264, true, error);
    
    if (success)
    {
        // qDebug() << "[CONSUMER] Frame encoded successfully - frameId:" << frameId
        //         << "h264:" << (h264 ? *h264 : false);
    }
    else
    {
        qWarning() << "[CONSUMER] Failed to encode frame - frameId:" << frameId
                  << "error:" << (error ? *error : "unknown error");
    }

    // 释放帧
    frame->deleteLater();

    return success;
}