
#include "encoding/drd_encoding_manager.h"

#include <QDebug>
#include <QDateTime>
#include <cstring>

#include <freerdp/codec/color.h>
#include <freerdp/codec/h264.h>
#include <freerdp/codec/progressive.h>
#include <freerdp/codec/rfx.h>
#include <winpr/stream.h>

/**
 * @brief 构造函数
 * 
 * 功能：初始化编码管理器对象。
 * 逻辑：设置默认值。
 * 参数：parent 父对象。
 * 外部接口：Qt QObject 构造函数。
 */
DrdEncodingManager::DrdEncodingManager(QObject *parent)
    : QObject(parent)
    , m_frameWidth(0)
    , m_frameHeight(0)
    , m_ready(false)
    , m_enableDiff(true)
    , m_h264Bitrate(2000000)
    , m_h264Framerate(30)
    , m_h264Qp(20)
    , m_h264HwAccel(true)
    , m_forceKeyframe(true)
    , m_lastCodec(DrdEncodingCodecClass::Unknown)
    , m_configMode(DrdEncodingMode::Auto)  // 默认自动模式
    , m_clientSupportsRfx(false)
    , m_clientSupportsAvc420(false)
    , m_clientSupportsAvc444(false)
    , m_clientSupportsAvc444v2(false)
    , m_clientSupportsProgressive(false)
    , m_remoteFxCodecId(0)
    , m_codecSupportChecked(false)
    , m_h264(nullptr)
    , m_rfx(nullptr)
    , m_progressive(nullptr)
    , m_codecs(0)
{
}

/**
 * @brief 析构函数
 * 
 * 功能：清理编码管理器对象。
 * 逻辑：重置编码器。
 * 参数：无。
 * 外部接口：Qt QObject 析构函数。
 */
DrdEncodingManager::~DrdEncodingManager()
{
    reset();
    
    // 清理 FreeRDP 编码器上下文
    if (m_h264 != nullptr)
    {
        h264_context_free(m_h264);
        m_h264 = nullptr;
    }
    if (m_rfx != nullptr)
    {
        rfx_context_free(m_rfx);
        m_rfx = nullptr;
    }
    if (m_progressive != nullptr)
    {
        progressive_context_free(m_progressive);
        m_progressive = nullptr;
    }
}

/**
 * @brief 准备编码器
 * 
 * 功能：按给定编码参数准备编码器。
 * 逻辑：校验分辨率非零 -> 记录分辨率/模式/差分标记。
 * 参数：options 编码选项（分辨率、模式、差分开关等），error 输出错误。
 * 外部接口：更新成员变量。
 * 返回值：成功返回 true。
 */
bool DrdEncodingManager::prepare(const DrdEncodingOptions *options, QString *error)
{
    if (options == nullptr)
    {
        if (error)
        {
            *error = "Encoding options is null";
        }
        return false;
    }

    if (options->width == 0 || options->height == 0)
    {
        if (error)
        {
            *error = QString("Encoder resolution must be non-zero (width=%1 height=%2)")
                        .arg(options->width)
                        .arg(options->height);
        }
        return false;
    }

    if (m_ready && (m_frameWidth != options->width || m_frameHeight != options->height))
    {
        reset();
    }

    m_enableDiff = options->enableFrameDiff;
    m_h264Bitrate = options->h264Bitrate;
    m_h264Framerate = options->h264Framerate;
    m_h264Qp = options->h264Qp;
    m_h264HwAccel = options->h264HwAccel;
    m_forceKeyframe = true;
    m_frameWidth = options->width;
    m_frameHeight = options->height;
    m_configMode = options->mode;  // 保存配置模式
    m_ready = true;

    qInfo() << "Encoding manager configured for" << options->width << "x" << options->height
           << "stream (mode=" << drdEncodingModeToString(options->mode) << " diff=" << (options->enableFrameDiff ? "on" : "off") << ")";
    return true;
}

/**
 * @brief 重置编码器
 * 
 * 功能：重置编码管理器状态。
 * 逻辑：若未准备好直接返回；清零分辨率/模式/状态。
 * 参数：无。
 * 外部接口：更新成员变量。
 */
void DrdEncodingManager::reset()
{
    if (!m_ready)
    {
        return;
    }

    qInfo() << "Encoding manager reset";
    m_codecs = 0;
    m_frameWidth = 0;
    m_frameHeight = 0;
    m_enableDiff = true;
    m_ready = false;
    m_previousFrame.clear();
    m_forceKeyframe = true;
    m_lastCodec = DrdEncodingCodecClass::Unknown;
    m_configMode = DrdEncodingMode::Auto;  // 重置为默认模式
    
    // 重置编码器支持状态缓存
    m_clientSupportsRfx = false;
    m_clientSupportsAvc420 = false;
    m_clientSupportsAvc444 = false;
    m_clientSupportsAvc444v2 = false;
    m_clientSupportsProgressive = false;
    m_remoteFxCodecId = 0;
    m_codecSupportChecked = false;
}

/**
 * @brief 初始化 H264 编码器
 */
bool DrdEncodingManager::initH264()
{
    if (m_h264 == nullptr)
    {
        m_h264 = h264_context_new(true);
    }

    if (m_h264 == nullptr)
    {
        return false;
    }

    if (!h264_context_reset(m_h264, m_frameWidth, m_frameHeight))
    {
        return false;
    }

    if (!h264_context_set_option(m_h264, H264_CONTEXT_OPTION_RATECONTROL, H264_RATECONTROL_VBR))
    {
        return false;
    }
    if (!h264_context_set_option(m_h264, H264_CONTEXT_OPTION_BITRATE, m_h264Bitrate))
    {
        return false;
    }
    if (!h264_context_set_option(m_h264, H264_CONTEXT_OPTION_FRAMERATE, m_h264Framerate))
    {
        return false;
    }
    if (!h264_context_set_option(m_h264, H264_CONTEXT_OPTION_QP, m_h264Qp))
    {
        return false;
    }

    m_codecs |= FREERDP_CODEC_AVC420 | FREERDP_CODEC_AVC444;
    return true;
}

/**
 * @brief 初始化 RFX 编码器
 */
bool DrdEncodingManager::initRfx(rdpSettings *settings)
{
    if (m_rfx == nullptr)
    {
        m_rfx = rfx_context_new_ex(true, freerdp_settings_get_uint32(settings, FreeRDP_ThreadingFlags));
    }
    if (m_rfx == nullptr)
    {
        return false;
    }

    if (!rfx_context_reset(m_rfx, m_frameWidth, m_frameHeight))
    {
        return false;
    }

    rfx_context_set_mode(m_rfx, static_cast<RLGR_MODE>(freerdp_settings_get_uint32(settings, FreeRDP_RemoteFxRlgrMode)));
    rfx_context_set_pixel_format(m_rfx, PIXEL_FORMAT_BGRX32);
    m_codecs |= FREERDP_CODEC_REMOTEFX;
    return true;
}

/**
 * @brief 初始化 Progressive 编码器
 */
bool DrdEncodingManager::initProgressive()
{
    if (m_progressive == nullptr)
    {
        m_progressive = progressive_context_new(true);
    }

    if (m_progressive == nullptr)
    {
        return false;
    }

    if (!progressive_context_reset(m_progressive))
    {
        return false;
    }

    m_codecs |= FREERDP_CODEC_PROGRESSIVE;
    return true;
}

/**
 * @brief 准备编码器
 */
bool DrdEncodingManager::prepareEncoder(quint32 codecs, rdpSettings *settings)
{
    bool success = true;
    
    if ((codecs & FREERDP_CODEC_REMOTEFX) && !(m_codecs & FREERDP_CODEC_REMOTEFX))
    {
        qInfo() << "Initializing RemoteFX encoder";
        if (!initRfx(settings))
        {
            qWarning() << "Failed to initialize RemoteFX encoder";
            success = false;
        }
    }

    if ((codecs & (FREERDP_CODEC_AVC420 | FREERDP_CODEC_AVC444)) &&
        !(m_codecs & (FREERDP_CODEC_AVC420 | FREERDP_CODEC_AVC444)))
    {
        qInfo() << "Initializing H.264 encoder";
        if (!initH264())
        {
            qWarning() << "Failed to initialize H.264 encoder";
            success = false;
        }
    }

    if ((codecs & FREERDP_CODEC_PROGRESSIVE) && !(m_codecs & FREERDP_CODEC_PROGRESSIVE))
    {
        qInfo() << "Initializing progressive encoder";
        if (!initProgressive())
        {
            qWarning() << "Failed to initialize progressive encoder";
            success = false;
        }
    }

    // 如果请求的编解码器都初始化失败，尝试初始化任何可用的编解码器
    if (!success)
    {
        qWarning() << "Requested codecs failed to initialize, trying fallback codecs";
        
        // 尝试初始化 RemoteFX 作为回退
        if (!(m_codecs & FREERDP_CODEC_REMOTEFX))
        {
            qInfo() << "Trying RemoteFX as fallback";
            if (initRfx(settings))
            {
                qInfo() << "RemoteFX fallback initialized successfully";
                success = true;
            }
        }
        
        // 尝试初始化 Progressive 作为回退
        if (!success && !(m_codecs & FREERDP_CODEC_PROGRESSIVE))
        {
            qInfo() << "Trying Progressive as fallback";
            if (initProgressive())
            {
                qInfo() << "Progressive fallback initialized successfully";
                success = true;
            }
        }
        
        // 尝试初始化 H.264 作为回退
        if (!success && !(m_codecs & (FREERDP_CODEC_AVC420 | FREERDP_CODEC_AVC444)))
        {
            qInfo() << "Trying H.264 as fallback";
            if (initH264())
            {
                qInfo() << "H.264 fallback initialized successfully";
                success = true;
            }
        }
    }

    return success;
}

/**
 * @brief 估算 H264 AVC420 流大小
 *
 * 功能：估算 AVC420 编码后的数据大小。
 * 逻辑：计算元数据大小加上实际数据长度。
 * 参数：havc420 AVC420 位图流。
 * 外部接口：用于 AVC444 编码。
 * 返回值：估算的大小。
 */
static inline UINT32 estimate_h264_avc420(RDPGFX_AVC420_BITMAP_STREAM *havc420)
{
    /* H264 metadata + H264 stream. See rdpgfx_write_h264_avc420 */
    return sizeof(UINT32) /* numRegionRects */
           + 10ULL /* regionRects + quantQualityVals */
                     * havc420->meta.numRegionRects +
           havc420->length;
}

/**
 * @brief 编码帧为 Surface GFX
 * 
 * 功能：将帧编码为 Rdpgfx 格式并发送。
 * 逻辑：获取帧数据，构造 Rdpgfx 命令，调用 FreeRDP API 发送。
 * 参数：settings FreeRDP 设置，context Rdpgfx 上下文，surfaceId Surface ID，input 输入帧，frameId 帧序号，h264 输出是否使用 H264，autoSwitch 自动切换编码策略，error 错误输出。
 * 外部接口：FreeRDP Rdpgfx API。
 * 返回值：成功返回 true。
 */
bool DrdEncodingManager::encodeSurfaceGfx(rdpSettings *settings,
                                           RdpgfxServerContext *context,
                                           quint16 surfaceId,
                                           DrdFrame *input,
                                           quint32 frameId,
                                           bool *h264,
                                           bool autoSwitch,
                                           QString *error)
{
    if (!m_ready)
    {
        if (error)
        {
            *error = "Encoding manager not prepared";
        }
        return false;
    }

    if (settings == nullptr || context == nullptr || input == nullptr)
    {
        if (error)
        {
            *error = "Invalid parameters";
        }
        return false;
    }

    m_frameWidth = input->width();
    m_frameHeight = input->height();

    const quint32 stride = input->stride();
    qint64 dataSize = 0;
    const quint8 *data = input->data(&dataSize);
    *h264 = false;

    // 获取客户端支持的编码器（只在第一次时获取并缓存）
    if (!m_codecSupportChecked)
    {
        m_clientSupportsAvc420 = freerdp_settings_get_bool(settings, FreeRDP_GfxH264);
        m_clientSupportsAvc444 = freerdp_settings_get_bool(settings, FreeRDP_GfxAVC444);
        m_clientSupportsAvc444v2 = freerdp_settings_get_bool(settings, FreeRDP_GfxAVC444v2);
        m_clientSupportsRfx = freerdp_settings_get_bool(settings, FreeRDP_RemoteFxCodec);
        m_clientSupportsProgressive = freerdp_settings_get_bool(settings, FreeRDP_GfxProgressive);
        m_remoteFxCodecId = freerdp_settings_get_uint32(settings, FreeRDP_RemoteFxCodecId);
        
        qInfo() << "Client codec support - AVC420:" << m_clientSupportsAvc420
                << "AVC444:" << m_clientSupportsAvc444
                << "AVC444v2:" << m_clientSupportsAvc444v2
                << "RemoteFX:" << m_clientSupportsRfx
                << "Progressive:" << m_clientSupportsProgressive
                << "RemoteFxCodecId:" << m_remoteFxCodecId;
        m_codecSupportChecked = true;
    }

    RDPGFX_SURFACE_COMMAND cmd = {};
    RDPGFX_START_FRAME_PDU cmdStart = {};
    RDPGFX_END_FRAME_PDU cmdEnd = {};

    cmd.surfaceId = surfaceId;
    cmd.format = PIXEL_FORMAT_BGRX32;
    cmd.left = 0;
    cmd.top = 0;
    cmd.right = static_cast<UINT16>(cmd.left + m_frameWidth);
    cmd.bottom = static_cast<UINT16>(cmd.top + m_frameHeight);
    cmd.width = static_cast<UINT16>(m_frameWidth);
    cmd.height = static_cast<UINT16>(m_frameHeight);

    cmdStart.frameId = frameId;
    cmdStart.timestamp = (static_cast<quint32>(QDateTime::currentDateTime().time().hour()) << 22) |
                         (static_cast<quint32>(QDateTime::currentDateTime().time().minute()) << 16) |
                         (static_cast<quint32>(QDateTime::currentDateTime().time().second()) << 10) |
                         (static_cast<quint32>(QDateTime::currentDateTime().time().msec()));
    cmdEnd.frameId = cmdStart.frameId;

    bool success = false;
    bool useAvc444 = false;
    bool useAvc420 = false;
    bool useProgressive = false;
    bool useRemotefx = false;

    // 根据配置模式优先选择编码器（只在第一次选择时输出日志）
    static bool firstCodecSelection = true;
    
    switch (m_configMode)
    {
        case DrdEncodingMode::Rfx:
            // RFX 模式优先：如果客户端支持 RFX 则使用 RFX
            if (m_clientSupportsRfx && m_remoteFxCodecId != 0)
            {
                useRemotefx = true;
                if (firstCodecSelection)
                {
                    qInfo() << "Selected RemoteFX codec (config mode=rfx)";
                    firstCodecSelection = false;
                }
            }
            else if (m_clientSupportsAvc444v2)
            {
                useAvc444 = true;
                if (firstCodecSelection)
                {
                    qInfo() << "Selected AVC444v2 codec (RFX not supported by client)";
                    firstCodecSelection = false;
                }
            }
            else if (m_clientSupportsAvc444)
            {
                useAvc444 = true;
                if (firstCodecSelection)
                {
                    qInfo() << "Selected AVC444 codec (RFX not supported by client)";
                    firstCodecSelection = false;
                }
            }
            else if (m_clientSupportsAvc420)
            {
                useAvc420 = true;
                if (firstCodecSelection)
                {
                    qInfo() << "Selected AVC420 codec (RFX not supported by client)";
                    firstCodecSelection = false;
                }
            }
            else if (m_clientSupportsProgressive)
            {
                useProgressive = true;
                if (firstCodecSelection)
                {
                    qInfo() << "Selected Progressive codec (RFX not supported by client)";
                    firstCodecSelection = false;
                }
            }
            else
            {
                useAvc420 = true;
                if (firstCodecSelection)
                {
                    qInfo() << "Selected AVC420 as fallback (RFX not supported by client)";
                    firstCodecSelection = false;
                }
            }
            break;

        case DrdEncodingMode::H264:
            // H264 模式优先
            if (m_clientSupportsAvc444v2)
            {
                useAvc444 = true;
                if (firstCodecSelection)
                {
                    qInfo() << "Selected AVC444v2 codec (config mode=h264)";
                    firstCodecSelection = false;
                }
            }
            else if (m_clientSupportsAvc444)
            {
                useAvc444 = true;
                if (firstCodecSelection)
                {
                    qInfo() << "Selected AVC444 codec (config mode=h264)";
                    firstCodecSelection = false;
                }
            }
            else if (m_clientSupportsAvc420)
            {
                useAvc420 = true;
                if (firstCodecSelection)
                {
                    qInfo() << "Selected AVC420 codec (config mode=h264)";
                    firstCodecSelection = false;
                }
            }
            else if (m_clientSupportsProgressive)
            {
                useProgressive = true;
                if (firstCodecSelection)
                {
                    qInfo() << "Selected Progressive codec (H264 not supported by client)";
                    firstCodecSelection = false;
                }
            }
            else if (m_clientSupportsRfx && m_remoteFxCodecId != 0)
            {
                useRemotefx = true;
                if (firstCodecSelection)
                {
                    qInfo() << "Selected RemoteFX codec (H264 not supported by client)";
                    firstCodecSelection = false;
                }
            }
            else
            {
                useAvc420 = true;
                if (firstCodecSelection)
                {
                    qInfo() << "Selected AVC420 as fallback (H264 not supported by client)";
                    firstCodecSelection = false;
                }
            }
            break;

        case DrdEncodingMode::Auto:
        default:
            // 自动模式：优先使用 AVC 编解码器
            if (m_clientSupportsAvc444v2)
            {
                useAvc444 = true;
                if (firstCodecSelection)
                {
                    qInfo() << "Selected AVC444v2 codec (auto mode)";
                    firstCodecSelection = false;
                }
            }
            else if (m_clientSupportsAvc444)
            {
                useAvc444 = true;
                if (firstCodecSelection)
                {
                    qInfo() << "Selected AVC444 codec (auto mode)";
                    firstCodecSelection = false;
                }
            }
            else if (m_clientSupportsAvc420)
            {
                useAvc420 = true;
                if (firstCodecSelection)
                {
                    qInfo() << "Selected AVC420 codec (auto mode)";
                    firstCodecSelection = false;
                }
            }
            else if (m_clientSupportsProgressive)
            {
                useProgressive = true;
                if (firstCodecSelection)
                {
                    qInfo() << "Selected Progressive codec (auto mode)";
                    firstCodecSelection = false;
                }
            }
            else if (m_clientSupportsRfx && m_remoteFxCodecId != 0)
            {
                useRemotefx = true;
                if (firstCodecSelection)
                {
                    qInfo() << "Selected RemoteFX codec (auto mode)";
                    firstCodecSelection = false;
                }
            }
            else
            {
                useAvc420 = true;
                if (firstCodecSelection)
                {
                    qInfo() << "Selected AVC420 as fallback (auto mode)";
                    firstCodecSelection = false;
                }
            }
            break;
    }

    if (useAvc420)
    {
        if (!prepareEncoder(FREERDP_CODEC_AVC420, settings))
        {
            if (error)
            {
                *error = "Failed to prepare encoder FREERDP_CODEC_AVC420";
            }
            return false;
        }

        RDPGFX_AVC420_BITMAP_STREAM avc420 = {};
        RECTANGLE_16 regionRect;
        regionRect.left = static_cast<UINT16>(cmd.left);
        regionRect.top = static_cast<UINT16>(cmd.top);
        regionRect.right = static_cast<UINT16>(cmd.right);
        regionRect.bottom = static_cast<UINT16>(cmd.bottom);

        INT32 rc = avc420_compress(m_h264, data, cmd.format, stride, m_frameWidth, m_frameHeight,
                                  &regionRect, &avc420.data, &avc420.length, &avc420.meta);

        if (rc < 0)
        {
            free_h264_metablock(&avc420.meta);
            if (error)
            {
                *error = "avc420_compress failed";
            }
            return false;
        }

        if (rc == 0)
        {
            free_h264_metablock(&avc420.meta);
            if (error)
            {
                *error = "no avc420 frame produced";
            }
            return false;
        }

        if (rc > 0)
        {
            cmd.codecId = RDPGFX_CODECID_AVC420;
            cmd.extra = &avc420;

            UINT ifError = context->SurfaceFrameCommand(context, &cmd, &cmdStart, &cmdEnd);
            free_h264_metablock(&avc420.meta);

            if (ifError != CHANNEL_RC_OK)
            {
                if (error)
                {
                    *error = QString("SurfaceFrameCommand failed with error %1").arg(ifError);
                }
                return false;
            }

            success = true;
            *h264 = true;
            registerCodecResult(DrdEncodingCodecClass::AVC, true);
        }
    }
    else if (useAvc444)
    {
        qInfo() << "avc444 encode";
        BYTE version = m_clientSupportsAvc444v2 ? 2 : 1;
        *h264 = true;

        if (!prepareEncoder(FREERDP_CODEC_AVC444, settings))
        {
            if (error)
            {
                *error = "Failed to prepare encoder FREERDP_CODEC_AVC444";
            }
            return false;
        }

        RDPGFX_AVC444_BITMAP_STREAM avc444 = {};
        RECTANGLE_16 regionRect;
        regionRect.left = static_cast<UINT16>(cmd.left);
        regionRect.top = static_cast<UINT16>(cmd.top);
        regionRect.right = static_cast<UINT16>(cmd.right);
        regionRect.bottom = static_cast<UINT16>(cmd.bottom);

        INT32 rc = avc444_compress(m_h264, data, cmd.format, stride, m_frameWidth, m_frameHeight, version, &regionRect,
                                  &avc444.LC, &avc444.bitstream[0].data, &avc444.bitstream[0].length,
                                  &avc444.bitstream[1].data, &avc444.bitstream[1].length, &avc444.bitstream[0].meta,
                                  &avc444.bitstream[1].meta);

        if (rc < 0)
        {
            if (error)
            {
                *error = "avc444_compress failed";
            }
            return false;
        }

        if (rc == 0)
        {
            free_h264_metablock(&avc444.bitstream[0].meta);
            free_h264_metablock(&avc444.bitstream[1].meta);
            if (error)
            {
                *error = "no avc444 frame produced";
            }
            return false;
        }

        if (rc > 0)
        {
            avc444.cbAvc420EncodedBitstream1 = estimate_h264_avc420(&avc444.bitstream[0]);
            cmd.codecId = m_clientSupportsAvc444v2 ? RDPGFX_CODECID_AVC444v2 : RDPGFX_CODECID_AVC444;
            cmd.extra = &avc444;

            UINT ifError = context->SurfaceFrameCommand(context, &cmd, &cmdStart, &cmdEnd);
            free_h264_metablock(&avc444.bitstream[0].meta);
            free_h264_metablock(&avc444.bitstream[1].meta);

            if (ifError != CHANNEL_RC_OK)
            {
                if (error)
                {
                    *error = QString("SurfaceFrameCommand failed with error %1").arg(ifError);
                }
                return false;
            }

            success = true;
            *h264 = true;
            registerCodecResult(DrdEncodingCodecClass::AVC, true);
        }
    }
    else if (useProgressive)
    {
        qInfo() << "progressive encode";
        if (!prepareEncoder(FREERDP_CODEC_PROGRESSIVE, settings))
        {
            if (error)
            {
                *error = "Failed to prepare encoder FREERDP_CODEC_PROGRESSIVE";
            }
            return false;
        }

        INT32 rc = progressive_compress(m_progressive, data, stride * m_frameHeight, cmd.format, m_frameWidth, m_frameHeight, stride, nullptr,
                                      &cmd.data, &cmd.length);

        if (rc < 0)
        {
            if (error)
            {
                *error = "progressive_compress failed";
            }
            return false;
        }

        if (rc > 0)
        {
            cmd.codecId = RDPGFX_CODECID_CAPROGRESSIVE;

            UINT ifError = context->SurfaceFrameCommand(context, &cmd, &cmdStart, &cmdEnd);
            if (ifError != CHANNEL_RC_OK)
            {
                if (error)
                {
                    *error = QString("SurfaceFrameCommand failed with error %1").arg(ifError);
                }
                return false;
            }

            success = true;
            registerCodecResult(DrdEncodingCodecClass::NonAVC, true);
        }
    }
    else if (useRemotefx)
    {
        qInfo() << "remotefx encode";
        if (!prepareEncoder(FREERDP_CODEC_REMOTEFX, settings))
        {
            if (error)
            {
                *error = "Failed to prepare encoder FREERDP_CODEC_REMOTEFX";
            }
            return false;
        }

        BOOL rc = rfx_compose_message(m_rfx, nullptr, nullptr, 0, data, m_frameWidth, m_frameHeight, stride);
        if (!rc)
        {
            if (error)
            {
                *error = "rfx_compose_message failed";
            }
            return false;
        }

        if (rc > 0)
        {
            cmd.codecId = RDPGFX_CODECID_CAVIDEO;

            UINT ifError = context->SurfaceFrameCommand(context, &cmd, &cmdStart, &cmdEnd);
            if (ifError != CHANNEL_RC_OK)
            {
                if (error)
                {
                    *error = QString("SurfaceFrameCommand failed with error %1").arg(ifError);
                }
                return false;
            }

            success = true;
            registerCodecResult(DrdEncodingCodecClass::NonAVC, true);
        }
    }
    else
    {
        if (error)
        {
            *error = "No supported codec available";
        }
        return false;
    }

    return success;
}

/**
 * @brief 强制关键帧
 *
 * 功能：强制下一个编码帧为关键帧。
 * 逻辑：设置关键帧标志。
 * 参数：无。
 * 外部接口：更新内部状态变量。
 */
void DrdEncodingManager::forceKeyframe()
{
    m_forceKeyframe = true;
}

/**
 * @brief 注册编解码器结果
 *
 * 功能：记录编解码器使用结果，用于后续的编码策略调整。
 * 逻辑：根据编解码器类型更新状态信息。
 * 参数：codecClass 编解码器类型，keyframeEncode 是否为关键帧编码。
 * 外部接口：更新内部状态变量。
 */
void DrdEncodingManager::registerCodecResult(DrdEncodingCodecClass codecClass, bool keyframeEncode)
{
    m_lastCodec = codecClass;
    // 简化实现，后续可以根据需要添加更复杂的逻辑
    Q_UNUSED(keyframeEncode);
}

