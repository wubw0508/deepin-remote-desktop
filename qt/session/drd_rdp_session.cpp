#include "session/drd_rdp_session.h"
#include "session/drd_rdp_graphics_pipeline.h"

#include "core/drd_server_runtime.h"

#include <freerdp/channels/drdynvc.h>
#include <freerdp/channels/wtsvc.h>
#include <freerdp/freerdp.h>
#include <freerdp/settings.h>
#include <winpr/synch.h>
#include <winpr/wtypes.h>
#include <winpr/string.h>
#include <QThread>
#include <QDebug>
#include <QDateTime>
#include <unistd.h>
#include <errno.h>

/**
 * @brief 构造函数
 * 
 * 功能：初始化 RDP 会话对象。
 * 逻辑：保存 peer 引用，初始化成员变量。
 * 参数：peer FreeRDP peer 对象，parent 父对象。
 * 外部接口：Qt QObject 构造函数。
 */
DrdRdpSession::DrdRdpSession(freerdp_peer *peer, QObject *parent)
    : QObject(parent)
    , m_peer(peer)
    , m_peerAddress("unknown")
    , m_state("created")
    , m_runtime(nullptr)
    , m_graphicsPipeline(nullptr)
    , m_graphicsPipelineReady(false)
    , m_vcm(INVALID_HANDLE_VALUE)
    , m_eventThread(nullptr)
    , m_vcmThread(nullptr)
    , m_renderThread(nullptr)
    , m_stopEvent(nullptr)
    , m_closedCallback(nullptr)
    , m_closedCallbackData(nullptr)
{
    m_connectionAlive.storeRelease(0);
    m_renderRunning.storeRelease(0);
    m_closedCallbackInvoked.storeRelease(0);
    m_frameSequence = 1;
    m_isActivated = false;
    m_passiveMode = false;

    if (peer != nullptr)
    {
        m_peerAddress = QString::fromUtf8(peer->hostname);
    }
}

/**
 * @brief 析构函数
 * 
 * 功能：清理 RDP 会话对象。
 * 逻辑：停止事件线程，清理资源。
 * 参数：无。
 * 外部接口：Qt QObject 析构函数。
 */
DrdRdpSession::~DrdRdpSession()
{
    disableGraphicsPipeline();
    stopEventThread();
    stopVcmThread();
}

/**
 * @brief 设置对端地址
 * 
 * 功能：设置对端地址字符串。
 * 逻辑：更新成员变量。
 * 参数：peerAddress 对端地址字符串。
 * 外部接口：无。
 */
void DrdRdpSession::setPeerAddress(const QString &peerAddress)
{
    m_peerAddress = peerAddress.isEmpty() ? "unknown" : peerAddress;
}

/**
 * @brief 设置对端状态
 *
 * 功能：设置对端状态字符串。
 * 逻辑：更新成员变量并记录日志。
 * 参数：state 状态字符串。
 * 外部接口：无。
 */
void DrdRdpSession::setPeerState(const QString &state)
{
    m_state = state.isEmpty() ? "unknown" : state;
    qInfo() << "Session" << m_peerAddress << "state" << m_state;
}

/**
 * @brief 设置会话关闭回调
 * 
 * 功能：设置会话关闭时的回调函数。
 * 逻辑：保存回调函数和用户数据，如果连接已关闭则立即调用。
 * 参数：callback 回调函数，userData 用户数据。
 * 外部接口：无。
 */
void DrdRdpSession::setRuntime(DrdServerRuntime *runtime)
{
    m_runtime = runtime;
    maybeInitGraphics();
}

void DrdRdpSession::setVirtualChannelManager(HANDLE vcm)
{
    m_vcm = vcm;
    maybeInitGraphics();
}

/**
 * @brief 设置被动模式
 *
 * 功能：设置会话是否为被动模式。
 * 逻辑：更新被动模式状态。
 * 参数：passive 是否启用被动模式。
 * 外部接口：无。
 */
void DrdRdpSession::setPassiveMode(bool passive)
{
    m_passiveMode = passive;
    qInfo() << "Session" << m_peerAddress << "passive mode:" << (passive ? "enabled" : "disabled");
}

void DrdRdpSession::setClosedCallback(SessionClosedFunc callback, void *userData)
{
    m_closedCallback = callback;
    m_closedCallbackData = userData;

    if (callback == nullptr)
    {
        m_closedCallbackInvoked.storeRelease(0);
        return;
    }

    if (!m_connectionAlive.loadAcquire())
    {
        qInfo() << Q_FUNC_INFO << " notifyClosed.";
        notifyClosed();
    }
}

/**
 * @brief 启动事件线程
 * 
 * 功能：启动事件处理线程。
 * 逻辑：创建停止事件，启动线程处理 FreeRDP 事件。
 * 参数：无。
 * 外部接口：WinPR CreateEvent，Qt QThread。
 * 返回值：成功返回 true。
 */
bool DrdRdpSession::startEventThread()
{
    if (m_peer == nullptr)
    {
        return false;
    }

    if (m_eventThread != nullptr)
    {
        return true;
    }

    if (m_stopEvent == nullptr)
    {
        m_stopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        if (m_stopEvent == nullptr)
        {
            qWarning() << "Session" << m_peerAddress << "failed to create stop event";
            return false;
        }
    }

    m_connectionAlive.storeRelease(1);
    m_eventThread = QThread::create([this]() {
        eventThreadFunc();
    });
    m_eventThread->start();

    return m_eventThread != nullptr;
}

/**
 * @brief 停止事件线程
 * 
 * 功能：停止事件处理线程。
 * 逻辑：设置停止事件，等待线程结束，清理资源。
 * 参数：无。
 * 外部接口：WinPR SetEvent/CloseHandle，Qt QThread。
 */
void DrdRdpSession::stopEventThread()
{
    m_connectionAlive.storeRelease(0);
    m_renderRunning.storeRelease(0);

    if (m_stopEvent != nullptr)
    {
        SetEvent(m_stopEvent);
    }

    if (m_renderThread != nullptr)
    {
        m_renderThread->wait();
        delete m_renderThread;
        m_renderThread = nullptr;
    }

    if (m_vcmThread != nullptr)
    {
        m_vcmThread->wait();
        delete m_vcmThread;
        m_vcmThread = nullptr;
    }

    if (m_eventThread != nullptr)
    {
        m_eventThread->wait();
        delete m_eventThread;
        m_eventThread = nullptr;
    }

    if (m_stopEvent != nullptr)
    {
        CloseHandle(m_stopEvent);
        m_stopEvent = nullptr;
    }

    qInfo() << Q_FUNC_INFO << " notifyClosed.";
    notifyClosed();
}

void DrdRdpSession::maybeInitGraphics()
{
    if (m_graphicsPipeline != nullptr || m_peer == nullptr || m_peer->context == nullptr ||
        m_runtime == nullptr || m_vcm == nullptr || m_vcm == INVALID_HANDLE_VALUE)
    {
        return;
    }

    // 获取编码选项
    DrdEncodingOptions encodingOpts;
    if (!m_runtime->getEncodingOptions(&encodingOpts))
    {
        return;
    }

    // 只为 RFX/H264 模式创建 graphics pipeline
    if (encodingOpts.mode != DrdEncodingMode::Rfx && encodingOpts.mode != DrdEncodingMode::Auto &&
        encodingOpts.mode != DrdEncodingMode::H264)
    {
        return;
    }

    m_graphicsPipeline = new DrdRdpGraphicsPipeline(
        m_peer, m_vcm, m_runtime, static_cast<quint16>(encodingOpts.width),
        static_cast<quint16>(encodingOpts.height), this);

    if (m_graphicsPipeline != nullptr)
    {
        m_graphicsPipelineReady = false;
        qDebug() << "Graphics pipeline created for session" << m_peerAddress;
    }
    else
    {
        qWarning() << "Failed to create graphics pipeline for session" << m_peerAddress;
    }
}

void DrdRdpSession::disableGraphicsPipeline()
{
    if (m_graphicsPipeline == nullptr)
    {
        return;
    }

    qDebug() << "Disabling graphics pipeline for session" << m_peerAddress;
    m_graphicsPipelineReady = false;
    delete m_graphicsPipeline;
    m_graphicsPipeline = nullptr;
}

/**
 * @brief 启动渲染线程
 * @return 成功返回 true
 */
bool DrdRdpSession::startRenderThread()
{
    if (m_renderThread != nullptr)
    {
        return true;
    }

    if (m_runtime == nullptr)
    {
        return false;
    }

    m_renderRunning.storeRelease(1);
    m_renderThread = QThread::create([this]() {
        renderThreadFunc();
    });
    m_renderThread->start();
    
    if (m_renderThread == nullptr)
    {
        m_renderRunning.storeRelease(0);
        return false;
    }
    
    return true;
}

/**
 * @brief 强制客户端桌面分辨率匹配服务器编码分辨率
 * @return 成功返回 true
 */
bool DrdRdpSession::enforcePeerDesktopSize()
{
    if (m_peer == nullptr || m_peer->context == nullptr || m_runtime == nullptr)
    {
        return true;
    }

    DrdEncodingOptions encodingOpts;
    if (!m_runtime->getEncodingOptions(&encodingOpts))
    {
        return true;
    }

    rdpContext *context = m_peer->context;
    rdpSettings *settings = context->settings;
    if (settings == nullptr)
    {
        return true;
    }

    const quint32 desiredWidth = encodingOpts.width;
    const quint32 desiredHeight = encodingOpts.height;
    
    // 获取客户端当前分辨率（需要实现获取方法）
    // const quint32 clientWidth = ...;
    // const quint32 clientHeight = ...;
    // const bool clientAllowsResize = ...;
    
    // 暂时返回 true，待实现完整逻辑
    qInfo() << "[ENFORCE] Desktop resolution enforcement not fully implemented, returning true";
    return true;
}

/**
 * @brief 刷新表面负载限制
 */
void DrdRdpSession::refreshSurfacePayloadLimit()
{
    quint32 maxPayload = 0;
    
    if (m_peer != nullptr && m_peer->context != nullptr && m_peer->context->settings != nullptr)
    {
        // 需要实现获取 MultifragMaxRequestSize 的逻辑
        // maxPayload = freerdp_settings_get_uint32(m_peer->context->settings, FreeRDP_MultifragMaxRequestSize);
    }
    
    // 暂时记录日志，待实现完整逻辑
    qDebug() << "[PAYLOAD] Surface payload limit refresh not fully implemented, maxPayload:" << maxPayload;
}

/**
 * @brief 断开会话连接
 *
 * 功能：断开 RDP 会话连接。
 * 逻辑：停止事件线程，调用 peer 的 Disconnect 回调。
 * 参数：reason 断开原因。
 * 外部接口：FreeRDP peer Disconnect 回调。
 */
void DrdRdpSession::disconnect(const QString &reason)
{
    if (!reason.isEmpty())
    {
        qInfo() << "Disconnecting session" << m_peerAddress << ":" << reason;
    }

    stopEventThread();

    if (m_peer != nullptr && m_peer->Disconnect != nullptr)
    {
        m_peer->Disconnect(m_peer);
        m_peer = nullptr;
    }
}

/**
 * @brief 通知会话已关闭
 * 
 * 功能：调用关闭回调函数。
 * 逻辑：使用原子操作确保回调只调用一次。
 * 参数：无。
 * 外部接口：无。
 */
void DrdRdpSession::notifyClosed()
{
    if (m_closedCallback == nullptr)
    {
        return;
    }

    if (!m_closedCallbackInvoked.testAndSetAcquire(0, 1))
    {
        return;
    }

    m_closedCallback(this, m_closedCallbackData);
}

/**
 * @brief VCM 线程函数
 *
 * 功能：监控虚拟通道管理器状态，驱动 drdynvc/Rdpgfx 生命周期。
 * 逻辑：获取 VCM 事件句柄，循环等待 stop_event、channel_event 以及 peer 事件；
 *       调用 peer->CheckFileDescriptor 驱动 FreeRDP，监测 drdynvc 状态并触发 graphics 管线初始化，
 *       直到连接终止。
 */
void DrdRdpSession::vcmThreadFunc()
{
    freerdp_peer *peer = m_peer;
    HANDLE vcm = m_vcm;
    HANDLE channelEvent = nullptr;

    if (vcm == nullptr || vcm == INVALID_HANDLE_VALUE || peer == nullptr)
    {
        qWarning() << "[VCM] VCM thread: invalid parameters - vcm=" << vcm << "peer=" << peer;
        return;
    }

    channelEvent = WTSVirtualChannelManagerGetEventHandle(vcm);
    qInfo() << "[VCM] VCM thread started for session" << m_peerAddress << "channelEvent=" << channelEvent;
    
    while (m_connectionAlive.loadAcquire())
    {
        HANDLE events[32];
        uint32_t peerEventsHandles = 0;
        DWORD nEvents = 0;

        if (peer == nullptr)
        {
            qWarning() << "[VCM] Peer is null, stopping VCM thread";
            m_connectionAlive.storeRelease(0);
            break;
        }

        if (m_stopEvent != nullptr)
        {
            events[nEvents++] = m_stopEvent;
        }
        if (channelEvent != nullptr)
        {
            events[nEvents++] = channelEvent;
        }
        
        peerEventsHandles = peer->GetEventHandles(peer, &events[nEvents], 32 - nEvents);
        if (!peerEventsHandles)
        {
            qWarning() << "[VCM] No peer events available, stopping session";
            m_connectionAlive.storeRelease(0);
            break;
        }
        nEvents += peerEventsHandles;
        
        DWORD status = WAIT_TIMEOUT;
        if (nEvents > 0)
        {
            status = WaitForMultipleObjects(nEvents, events, FALSE, INFINITE);
        }

        if (status == WAIT_FAILED)
        {
            qWarning() << "[VCM] WaitForMultipleObjects failed for session" << m_peerAddress;
            break;
        }

        if (!peer->CheckFileDescriptor(peer))
        {
            qWarning() << "[VCM] CheckFileDescriptor error, stopping session";
            m_connectionAlive.storeRelease(0);
            break;
        }

        if (!peer->connected)
        {
            continue;
        }

        if (!WTSVirtualChannelManagerIsChannelJoined(vcm, DRDYNVC_SVC_CHANNEL_NAME))
        {
            continue;
        }

        UINT32 drdynvcState = WTSVirtualChannelManagerGetDrdynvcState(vcm);
        
        // 只在状态变化时记录日志，避免刷屏
        static UINT32 lastDrdynvcState = DRDYNVC_STATE_NONE;
        if (drdynvcState != lastDrdynvcState)
        {
            qDebug() << "[VCM] drdynvc state:" << drdynvcState;
            lastDrdynvcState = drdynvcState;
        }
        
        switch (drdynvcState)
        {
            case DRDYNVC_STATE_NONE:
                if (lastDrdynvcState != DRDYNVC_STATE_NONE)
                {
                    qInfo() << "[VCM] drdynvc state NONE, setting channel event";
                }
                if (channelEvent != nullptr)
                {
                    SetEvent(channelEvent);
                }
                break;
            case DRDYNVC_STATE_READY:
                if (lastDrdynvcState != DRDYNVC_STATE_READY)
                {
                    qInfo() << "[VCM] drdynvc state READY, initializing graphics pipeline";
                }
                if (m_graphicsPipeline != nullptr && m_connectionAlive.loadAcquire())
                {
                    // 只在图形管道未就绪时初始化，避免重复初始化
                    if (!m_graphicsPipelineReady)
                    {
                        m_graphicsPipeline->maybeInit();
                    }
                }
                break;
            default:
                if (lastDrdynvcState != drdynvcState)
                {
                    qInfo() << "[VCM] drdynvc state:" << drdynvcState;
                }
                break;
        }

        if (!m_connectionAlive.loadAcquire())
        {
            break;
        }

        if (channelEvent != nullptr && WaitForSingleObject(channelEvent, 0) == WAIT_OBJECT_0)
        {
            if (!WTSVirtualChannelManagerCheckFileDescriptor(vcm))
            {
                qWarning() << "[VCM] Failed to check VCM descriptor for session" << m_peerAddress;
                m_connectionAlive.storeRelease(0);
                break;
            }
        }
    }

    qInfo() << "[VCM] VCM thread exiting for session" << m_peerAddress;
    m_renderRunning.storeRelease(0);
    notifyClosed();
}

/**
 * @brief 启动 VCM 线程
 * @return 成功返回 true
 */
bool DrdRdpSession::startVcmThread()
{
    if (m_peer == nullptr)
    {
        return false;
    }

    if (m_vcmThread != nullptr)
    {
        return true;
    }

    if (m_vcm == nullptr || m_vcm == INVALID_HANDLE_VALUE)
    {
        qWarning() << "[VCM] VCM not available, cannot start VCM thread";
        return false;
    }

    m_vcmThread = QThread::create([this]() {
        vcmThreadFunc();
    });
    m_vcmThread->start();

    return m_vcmThread != nullptr;
}

/**
 * @brief 停止 VCM 线程
 */
void DrdRdpSession::stopVcmThread()
{
    if (m_vcmThread != nullptr)
    {
        m_vcmThread->wait();
        delete m_vcmThread;
        m_vcmThread = nullptr;
    }
}

/**
 * @brief 事件线程函数
 *
 * 功能：处理 FreeRDP 事件循环。
 * 逻辑：等待事件，调用 CheckFileDescriptor 处理。
 * 参数：无。
 * 外部接口：FreeRDP peer GetEventHandles/CheckFileDescriptor，WinPR WaitForMultipleObjects。
 */
void DrdRdpSession::eventThreadFunc()
{
    qInfo() << "[EVENT] Event thread started for session" << m_peerAddress;
    // 事件线程现在为空，因为 VCM 线程已经负责处理所有事件
    // 这样可以避免两个线程同时处理事件导致的冲突
    while (m_connectionAlive.loadAcquire())
    {
        QThread::msleep(1000); // 休眠 1 秒，避免占用过多资源
    }
    qInfo() << "[EVENT] Event thread exiting for session" << m_peerAddress;
    notifyClosed();
}

/**
 * @brief PostConnect 钩子，更新状态
 *
 * 功能：PostConnect 钩子，更新状态。
 * 逻辑：设置状态为 post-connect 后返回 true。
 * 返回值：成功返回 true。
 */
bool DrdRdpSession::postConnect()
{
    setPeerState("post-connect");
    return true;
}

/**
 * @brief 激活会话
 *
 * 功能：执行 RDP 会话激活，启动编码/渲染流程。
 * 逻辑：获取编码配置并准备 runtime 流；启动渲染线程并更新状态。
 * 返回值：成功返回 true。
 */
bool DrdRdpSession::activate()
{
    qInfo() << "[PRODUCER] DrdRdpSession::activate() - Starting activation for session" << m_peerAddress;
    
    if (m_runtime == nullptr)
    {
        qWarning() << "[PRODUCER] Runtime is null";
        return false;
    }

    const bool streamRunning = m_runtime->isStreamRunning();
    qInfo() << "[PRODUCER] Stream running status:" << streamRunning;
    
    DrdEncodingOptions encodingOpts;
    if (!m_runtime->getEncodingOptions(&encodingOpts))
    {
        qWarning() << "[PRODUCER] Missing encoding options before stream start";
        setPeerState("encoding-opts-missing");
        disconnect("encoding-opts-missing");
        return false;
    }
    
    qInfo() << "[PRODUCER] Encoding options - width:" << encodingOpts.width
            << "height:" << encodingOpts.height
            << "mode:" << drdEncodingModeToString(encodingOpts.mode);

    bool startedStream = false;
    if (!streamRunning)
    {
        qInfo() << "[PRODUCER] Stream not running, preparing stream...";
        QString streamError;
        if (!m_runtime->prepareStream(&encodingOpts, &streamError))
        {
            qWarning() << "[PRODUCER] Failed to prepare runtime stream:" << streamError;
            setPeerState("stream-prepare-failed");
            disconnect("stream-prepare-failed");
            return false;
        }
        qInfo() << "[PRODUCER] Stream prepared successfully";
        startedStream = true;
    }
    else
    {
        qInfo() << "[PRODUCER] Stream already running, skipping prepare";
    }

    if (startedStream)
    {
        qInfo() << "[PRODUCER] Requesting keyframe";
        m_runtime->requestKeyframe();
    }

    // 刷新表面负载限制（需要实现完整的 refreshSurfacePayloadLimit 方法）
    // 暂时跳过此步骤，因为需要实现 FreeRDP settings 访问
    // refreshSurfacePayloadLimit();

    // 注意：不要在这里初始化 graphics pipeline
    // graphics pipeline 的初始化由 VCM 线程在 drdynvc 通道 READY 时触发
    // 这样可以确保在通道完全就绪后才尝试打开 Rdpgfx 通道
    if (m_graphicsPipeline != nullptr)
    {
        qInfo() << "[PRODUCER] Graphics pipeline will be initialized by VCM thread when drdynvc is ready";
    }

    setPeerState("activated");
    m_isActivated = true;
    qInfo() << "[PRODUCER] Session activated, starting render thread";

    // 启动渲染线程
    if (!startRenderThread())
    {
        qWarning() << "[PRODUCER] Session" << m_peerAddress << "failed to start renderer thread";
    }

    qInfo() << "[PRODUCER] Activation completed successfully";

    return true;
}

/**
 * @brief 渲染线程函数
 *
 * 功能：从 runtime 拉取编码帧并发送到客户端。
 * 逻辑：在连接/激活有效时，从 runtime 取帧，编码并发送。
 */
void DrdRdpSession::renderThreadFunc()
{
    qInfo() << "[CONSUMER] Render thread started for session" << m_peerAddress;
    
    const quint32 target_fps = 30;
    const qint64 stats_interval = 5000000; // 5秒
    quint32 stats_frames = 0;
    qint64 stats_window_start = 0;
    
    qInfo() << "[CONSUMER] Render thread parameters - target_fps:" << target_fps
            << "stats_interval_us:" << stats_interval;

    while (m_renderRunning.loadAcquire())
    {
        if (!m_connectionAlive.loadAcquire())
        {
            qInfo() << "[CONSUMER] Connection not alive, exiting render thread";
            break;
        }

        if (m_runtime == nullptr || m_peer == nullptr || m_peer->context == nullptr)
        {
            qWarning() << "[CONSUMER] Missing required objects - runtime:" << (m_runtime != nullptr)
                      << "peer:" << (m_peer != nullptr)
                      << "context:" << (m_peer && m_peer->context != nullptr);
            QThread::msleep(1);
            continue;
        }

        // 检查 graphics pipeline 是否就绪
        if (m_graphicsPipeline != nullptr && !m_graphicsPipelineReady)
        {
            if (m_graphicsPipeline->isReady())
            {
                m_graphicsPipelineReady = true;
                qInfo() << "[CONSUMER] Graphics pipeline became ready";
            }
            else
            {
                // 减少频繁初始化，增加延迟（由 VCM 线程处理）
                QThread::msleep(100);
                continue;
            }
        }

        QString error;
        bool h264 = false;
        RdpgfxServerContext *rdpgfxContext = nullptr;
        quint16 surfaceId = 1;

        // 如果 graphics pipeline 就绪，使用它
        if (m_graphicsPipeline != nullptr && m_graphicsPipelineReady)
        {
            rdpgfxContext = m_graphicsPipeline->context();
            surfaceId = m_graphicsPipeline->surfaceId();

            // 等待容量
            if (!m_graphicsPipeline->waitForCapacity(-1))
            {
                qWarning() << "[CONSUMER] Failed to wait for graphics pipeline capacity";
                QThread::msleep(1);
                continue;
            }
        }
        else if (m_graphicsPipeline != nullptr)
        {
            // Graphics pipeline 存在但未就绪，等待初始化（由 VCM 线程处理）
            QThread::msleep(100);
            continue;
        }
        
        // 如果没有 graphics pipeline，跳过编码
        if (rdpgfxContext == nullptr)
        {
            QThread::msleep(100);
            continue;
        }
        
        // 拉取编码帧
        if (!m_runtime->pullEncodedFrameSurfaceGfx(
                m_peer->context->settings,
                rdpgfxContext,
                surfaceId,
                16 * 1000, // timeout 16ms
                m_frameSequence,
                &h264,
                &error))
        {
            if (error.contains("Timed out") || error.contains("no data"))
            {
                // 超时是正常的，减少日志输出频率
                // static quint32 timeoutLogCounter = 0;
                // if (timeoutLogCounter % 100 == 0) // 每100次超时输出一次日志
                // {
                //     qDebug() << "[CONSUMER] Frame pull timed out (normal), continuing (timeout" << timeoutLogCounter << ")";
                // }
                // timeoutLogCounter++;
                continue;
            }
            qWarning() << "[CONSUMER] Failed to pull encoded frame:" << error;
            
            // 如果使用 graphics pipeline 且失败，更新 outstanding_frames
            if (m_graphicsPipeline != nullptr && m_graphicsPipelineReady)
            {
                m_graphicsPipeline->outFrameChange(false);
            }
            continue;
        }
        
        // 减少帧成功拉取的日志输出频率，避免刷屏
        // static quint32 frameLogCounter = 0;
        // if (frameLogCounter % 100 == 0) // 每100帧输出一次日志
        // {
        //     qDebug() << "[CONSUMER] Frame pulled successfully - frameId:" << m_frameSequence
        //             << "h264:" << h264 << "(frame" << frameLogCounter << ")";
        // }
        // frameLogCounter++;

        // 更新 graphics pipeline 状态
        if (m_graphicsPipeline != nullptr && m_graphicsPipelineReady)
        {
            m_graphicsPipeline->outFrameChange(true);
            m_graphicsPipeline->setLastFrameMode(h264);
        }

        // 统计信息
        const qint64 now = QDateTime::currentMSecsSinceEpoch() * 1000;
        if (stats_window_start == 0)
        {
            stats_window_start = now;
        }
        stats_frames++;

        const qint64 elapsed = now - stats_window_start;
        if (elapsed >= stats_interval)
        {
            const double actual_fps = static_cast<double>(stats_frames) * 1000000.0 / static_cast<double>(elapsed);
            const bool reached_target = actual_fps >= static_cast<double>(target_fps);
            qInfo() << "[CONSUMER] Session" << m_peerAddress << "render fps=" << actual_fps
                   << "(target=" << target_fps << "):" << (reached_target ? "reached target" : "below target");
            stats_frames = 0;
            stats_window_start = now;
        }

        m_frameSequence++;
        if (m_frameSequence == 0)
        {
            m_frameSequence = 1;
        }
    }
    
    qInfo() << "[CONSUMER] Render thread exited for session" << m_peerAddress;
}
