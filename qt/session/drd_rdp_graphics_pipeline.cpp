#include "session/drd_rdp_graphics_pipeline.h"

#include "core/drd_server_runtime.h"

#include <QDebug>
#include <QDateTime>
#include <freerdp/peer.h>
#include <freerdp/channels/rdpgfx.h>
#include <freerdp/codec/color.h>

/**
 * @brief 构造函数
 * 
 * 功能：创建绑定指定 peer/VCM 的图形管线实例。
 * 逻辑：校验参数有效后分配 Rdpgfx server context 并设置自定义回调，
 *       保存 surface 尺寸与 peer/context。
 */
DrdRdpGraphicsPipeline::DrdRdpGraphicsPipeline(freerdp_peer *peer,
                                               HANDLE vcm,
                                               DrdServerRuntime *runtime,
                                               quint16 surfaceWidth,
                                               quint16 surfaceHeight,
                                               QObject *parent)
    : QObject(parent)
    , m_peer(peer)
    , m_width(surfaceWidth)
    , m_height(surfaceHeight)
    , m_rdpgfxContext(nullptr)
    , m_channelOpened(false)
    , m_capsConfirmed(false)
    , m_surfaceReady(false)
    , m_surfaceId(1)
    , m_codecContextId(1)
    , m_nextFrameId(1)
    , m_outstandingFrames(0)
    , m_maxOutstandingFrames(3)
    , m_channelId(0)
    , m_frameAcksSuspended(false)
    , m_runtime(runtime)
    , m_lastFrameH264(false)
{
    if (peer == nullptr || peer->context == nullptr)
    {
        qWarning() << "Invalid peer or peer context";
        return;
    }

    if (vcm == nullptr || vcm == INVALID_HANDLE_VALUE)
    {
        qWarning() << "Invalid VCM handle";
        return;
    }

    // 分配 Rdpgfx server context
    m_rdpgfxContext = rdpgfx_server_context_new(vcm);
    if (m_rdpgfxContext == nullptr)
    {
        qWarning() << "Failed to allocate Rdpgfx server context";
        return;
    }

    // 设置回调
    m_rdpgfxContext->rdpcontext = peer->context;
    m_rdpgfxContext->custom = this;
    m_rdpgfxContext->ChannelIdAssigned = channelAssignedCallback;
    m_rdpgfxContext->CapsAdvertise = capsAdvertiseCallback;
    m_rdpgfxContext->FrameAcknowledge = frameAckCallback;

    qDebug() << "Graphics pipeline created for surface" << m_width << "x" << m_height;
}

/**
 * @brief 析构函数
 * 
 * 功能：释放 Rdpgfx 管线持有的上下文与 surface。
 */
DrdRdpGraphicsPipeline::~DrdRdpGraphicsPipeline()
{
    if (m_rdpgfxContext != nullptr)
    {
        if (m_surfaceReady && m_rdpgfxContext->DeleteSurface)
        {
            RDPGFX_DELETE_SURFACE_PDU del = {0};
            del.surfaceId = m_surfaceId;
            m_rdpgfxContext->DeleteSurface(m_rdpgfxContext, &del);
            m_surfaceReady = false;
            m_capacityCond.wakeAll();
        }

        if (m_channelOpened && m_rdpgfxContext->Close)
        {
            m_rdpgfxContext->Close(m_rdpgfxContext);
            m_channelOpened = false;
        }

        rdpgfx_server_context_free(m_rdpgfxContext);
        m_rdpgfxContext = nullptr;
    }
}

/**
 * @brief 尝试初始化 Rdpgfx 渠道与 surface
 * 
 * 功能：确保通道能力准备就绪。
 */
bool DrdRdpGraphicsPipeline::maybeInit()
{
    QMutexLocker locker(&m_lock);

    if (m_rdpgfxContext == nullptr)
    {
        return false;
    }

    if (!m_channelOpened)
    {
        RdpgfxServerContext *rdpgfxContext = m_rdpgfxContext;

        locker.unlock();

        if (rdpgfxContext == nullptr ||
            rdpgfxContext->Open == nullptr ||
            !rdpgfxContext->Open(rdpgfxContext))
        {
            qWarning() << "Failed to open Rdpgfx channel";
            return false;
        }

        locker.relock();

        if (m_rdpgfxContext != rdpgfxContext)
        {
            return false;
        }

        m_channelOpened = true;
        qDebug() << "Rdpgfx channel opened";
    }

    if (!m_capsConfirmed)
    {
        return false;
    }

    return resetLocked();
}

/**
 * @brief 重置 Rdpgfx surface 与上下文
 * 
 * 功能：发送 ResetGraphics、CreateSurface、MapSurfaceToOutput 三个 PDU。
 */
bool DrdRdpGraphicsPipeline::resetLocked()
{
    Q_ASSERT(m_rdpgfxContext != nullptr);

    if (m_surfaceReady)
    {
        return true;
    }

    RDPGFX_RESET_GRAPHICS_PDU reset = {0};
    reset.width = m_width;
    reset.height = m_height;
    reset.monitorCount = 0;
    reset.monitorDefArray = nullptr;

    if (!m_rdpgfxContext->ResetGraphics ||
        m_rdpgfxContext->ResetGraphics(m_rdpgfxContext, &reset) != CHANNEL_RC_OK)
    {
        qWarning() << "Graphics pipeline failed to reset graphics";
        return false;
    }

    RDPGFX_CREATE_SURFACE_PDU create = {0};
    create.surfaceId = m_surfaceId;
    create.width = m_width;
    create.height = m_height;
    create.pixelFormat = GFX_PIXEL_FORMAT_XRGB_8888;

    if (!m_rdpgfxContext->CreateSurface ||
        m_rdpgfxContext->CreateSurface(m_rdpgfxContext, &create) != CHANNEL_RC_OK)
    {
        qWarning() << "Graphics pipeline failed to create surface" << m_surfaceId;
        return false;
    }

    RDPGFX_MAP_SURFACE_TO_OUTPUT_PDU map = {0};
    map.surfaceId = m_surfaceId;
    map.outputOriginX = 0;
    map.outputOriginY = 0;
    map.reserved = 0;
    if (!m_rdpgfxContext->MapSurfaceToOutput ||
        m_rdpgfxContext->MapSurfaceToOutput(m_rdpgfxContext, &map) != CHANNEL_RC_OK)
    {
        qWarning() << "Graphics pipeline failed to map surface" << m_surfaceId << "to output";
        return false;
    }

    m_nextFrameId = 1;
    m_outstandingFrames = 0;
    m_surfaceReady = true;
    m_lastFrameH264 = false;
    m_frameAcksSuspended = false;
    m_capacityCond.wakeAll();

    qDebug() << "Graphics pipeline surface" << m_surfaceId << "ready";
    return true;
}

/**
 * @brief 检查 surface 是否已准备就绪
 */
bool DrdRdpGraphicsPipeline::isReady() const
{
    QMutexLocker locker(&m_lock);
    return m_surfaceReady;
}

/**
 * @brief 检查是否可以提交新帧（背压控制）
 */
bool DrdRdpGraphicsPipeline::canSubmit() const
{
    QMutexLocker locker(&m_lock);
    return m_surfaceReady &&
           (m_frameAcksSuspended ||
            m_outstandingFrames < static_cast<int>(m_maxOutstandingFrames) ||
            m_lastFrameH264);
}

/**
 * @brief 等待 Rdpgfx 管线具备提交容量
 */
bool DrdRdpGraphicsPipeline::waitForCapacity(qint64 timeoutUs)
{
    QMutexLocker locker(&m_lock);

    if (m_lastFrameH264)
    {
        return true;
    }

    qint64 deadline = 0;
    if (timeoutUs > 0)
    {
        deadline = QDateTime::currentMSecsSinceEpoch() + (timeoutUs / 1000);
    }

    while (m_surfaceReady && !m_frameAcksSuspended &&
           m_outstandingFrames >= static_cast<int>(m_maxOutstandingFrames))
    {
        if (timeoutUs < 0)
        {
            m_capacityCond.wait(&m_lock);
        }
        else
        {
            if (!m_capacityCond.wait(&m_lock, timeoutUs))
            {
                break;
            }
        }
    }

    bool ready = m_surfaceReady &&
                 (m_frameAcksSuspended ||
                  m_outstandingFrames < static_cast<int>(m_maxOutstandingFrames));
    return ready;
}

/**
 * @brief 更新 outstanding_frames 计数
 */
void DrdRdpGraphicsPipeline::outFrameChange(bool add)
{
    QMutexLocker locker(&m_lock);
    if (add)
    {
        if (!m_frameAcksSuspended)
        {
            m_outstandingFrames++;
        }
    }
    else
    {
        if (!m_frameAcksSuspended && m_outstandingFrames > 0)
        {
            m_outstandingFrames--;
        }
        m_capacityCond.wakeAll();
    }
}

/**
 * @brief 设置最后一帧是否为 H264
 */
void DrdRdpGraphicsPipeline::setLastFrameMode(bool h264)
{
    QMutexLocker locker(&m_lock);
    m_lastFrameH264 = h264;
}

/**
 * @brief Rdpgfx 通道分配回调
 */
BOOL DrdRdpGraphicsPipeline::channelAssignedCallback(RdpgfxServerContext *context, UINT32 channelId)
{
    DrdRdpGraphicsPipeline *self = context != nullptr ? static_cast<DrdRdpGraphicsPipeline *>(context->custom) : nullptr;

    if (self == nullptr)
    {
        return CHANNEL_RC_OK;
    }

    QMutexLocker locker(&self->m_lock);
    self->m_channelId = channelId;
    qDebug() << "Rdpgfx channel assigned:" << channelId;
    return TRUE;
}

/**
 * @brief Rdpgfx 能力协商回调
 */
UINT DrdRdpGraphicsPipeline::capsAdvertiseCallback(RdpgfxServerContext *context,
                                                    const RDPGFX_CAPS_ADVERTISE_PDU *capsAdvertise)
{
    DrdRdpGraphicsPipeline *self = context != nullptr ? static_cast<DrdRdpGraphicsPipeline *>(context->custom) : nullptr;

    if (self == nullptr || capsAdvertise == nullptr || capsAdvertise->capsSetCount == 0)
    {
        return CHANNEL_RC_OK;
    }

    qDebug() << "Rdpgfx caps advertise received, capsSetCount:" << capsAdvertise->capsSetCount;

    // 简化实现：直接确认第一个能力集
    // 实际实现需要根据客户端能力选择合适的版本
    if (capsAdvertise->capsSets != nullptr && capsAdvertise->capsSetCount > 0)
    {
        RDPGFX_CAPSET caps = capsAdvertise->capsSets[0];
        RDPGFX_CAPS_CONFIRM_PDU pdu = {0};
        pdu.capsSet = &caps;

        if (context->CapsConfirm)
        {
            UINT rc = context->CapsConfirm(context, &pdu);
            if (rc == CHANNEL_RC_OK)
            {
                QMutexLocker locker(&self->m_lock);
                self->m_capsConfirmed = true;
                qDebug() << "Rdpgfx caps confirmed, version:" << caps.version;
            }
            return rc;
        }
    }

    return CHANNEL_RC_UNSUPPORTED_VERSION;
}

/**
 * @brief Rdpgfx 帧确认回调
 */
UINT DrdRdpGraphicsPipeline::frameAckCallback(RdpgfxServerContext *context,
                                               const RDPGFX_FRAME_ACKNOWLEDGE_PDU *ack)
{
    DrdRdpGraphicsPipeline *self = context != nullptr ? static_cast<DrdRdpGraphicsPipeline *>(context->custom) : nullptr;

    if (self == nullptr || ack == nullptr)
    {
        return CHANNEL_RC_OK;
    }

    QMutexLocker locker(&self->m_lock);

    if (ack->queueDepth == SUSPEND_FRAME_ACKNOWLEDGEMENT)
    {
        if (!self->m_frameAcksSuspended)
        {
            qDebug() << "RDPGFX client suspended frame acknowledgements";
        }
        self->m_frameAcksSuspended = true;
        self->m_outstandingFrames = 0;
        self->m_capacityCond.wakeAll();
        return CHANNEL_RC_OK;
    }

    if (self->m_frameAcksSuspended)
    {
        qDebug() << "RDPGFX client resumed frame acknowledgements";
    }
    self->m_frameAcksSuspended = false;

    if (self->m_outstandingFrames > 0)
    {
        if (self->m_lastFrameH264)
            self->m_outstandingFrames = 0;
        else
            self->m_outstandingFrames--;
    }
    self->m_capacityCond.wakeAll();

    return CHANNEL_RC_OK;
}