#pragma once

#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <freerdp/freerdp.h>
#include <freerdp/server/rdpgfx.h>
#include <winpr/wtypes.h>

// 前向声明
class DrdServerRuntime;

/**
 * @brief Qt 版本的 DrdRdpGraphicsPipeline 类
 * 
 * 管理 RDP Graphics Pipeline (Rdpgfx) 通道，负责创建和管理
 * RdpgfxServerContext，处理帧的发送和确认
 */
class DrdRdpGraphicsPipeline : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param peer FreeRDP peer 对象
     * @param vcm 虚拟通道管理器句柄
     * @param runtime 运行时对象
     * @param surfaceWidth Surface 宽度
     * @param surfaceHeight Surface 高度
     * @param parent 父对象
     */
    explicit DrdRdpGraphicsPipeline(freerdp_peer *peer,
                                    HANDLE vcm,
                                    DrdServerRuntime *runtime,
                                    quint16 surfaceWidth,
                                    quint16 surfaceHeight,
                                    QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~DrdRdpGraphicsPipeline() override;

    /**
     * @brief 尝试初始化 Rdpgfx 通道
     * @return 成功返回 true
     */
    bool maybeInit();

    /**
     * @brief 检查 surface 是否已准备就绪
     * @return 就绪返回 true
     */
    bool isReady() const;

    /**
     * @brief 检查是否可以提交新帧（背压控制）
     * @return 可以提交返回 true
     */
    bool canSubmit() const;

    /**
     * @brief 等待 Rdpgfx 管线具备提交容量
     * @param timeoutUs 超时时间（微秒），-1 表示无限等待
     * @return 具备容量返回 true
     */
    bool waitForCapacity(qint64 timeoutUs);

    /**
     * @brief 获取 Surface ID
     * @return Surface ID
     */
    quint16 surfaceId() const { return m_surfaceId; }

    /**
     * @brief 获取 RdpgfxServerContext
     * @return RdpgfxServerContext 指针
     */
    RdpgfxServerContext *context() const { return m_rdpgfxContext; }

    /**
     * @brief 更新 outstanding_frames 计数
     * @param add true 表示增加，false 表示减少
     */
    void outFrameChange(bool add);

    /**
     * @brief 设置最后一帧是否为 H264
     * @param h264 是否为 H264
     */
    void setLastFrameMode(bool h264);

private:
    /**
     * @brief 重置 Rdpgfx surface 与上下文
     * @return 成功返回 true
     */
    bool resetLocked();

    /**
     * @brief Rdpgfx 通道分配回调
     */
    static BOOL channelAssignedCallback(RdpgfxServerContext *context, UINT32 channelId);

    /**
     * @brief Rdpgfx 能力协商回调
     */
    static UINT capsAdvertiseCallback(RdpgfxServerContext *context,
                                      const RDPGFX_CAPS_ADVERTISE_PDU *capsAdvertise);

    /**
     * @brief Rdpgfx 帧确认回调
     */
    static UINT frameAckCallback(RdpgfxServerContext *context,
                                 const RDPGFX_FRAME_ACKNOWLEDGE_PDU *ack);

private:
    freerdp_peer *m_peer;
    quint16 m_width;
    quint16 m_height;
    RdpgfxServerContext *m_rdpgfxContext;
    bool m_channelOpened;
    bool m_capsConfirmed;
    bool m_surfaceReady;
    quint16 m_surfaceId;
    quint32 m_codecContextId;
    quint32 m_nextFrameId;
    int m_outstandingFrames;
    unsigned int m_maxOutstandingFrames;
    quint32 m_channelId;
    bool m_frameAcksSuspended;
    mutable QMutex m_lock;
    QWaitCondition m_capacityCond;
    DrdServerRuntime *m_runtime;
    bool m_lastFrameH264;
};