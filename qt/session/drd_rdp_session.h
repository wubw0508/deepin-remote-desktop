#pragma once

#include <QObject>
#include <QString>
#include <QThread>
#include <QAtomicInt>
#include <freerdp/listener.h>
#include <freerdp/server/rdpgfx.h>
#include <winpr/wtypes.h>

// 前向声明
class DrdServerRuntime;
class DrdRdpGraphicsPipeline;

/**
 * @brief Qt 版本的 DrdRdpSession 类
 * 
 * 替代 GObject 版本的 DrdRdpSession，使用 Qt 的对象系统
 * 管理 RDP 会话的生命周期和事件循环
 */
class DrdRdpSession : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 会话关闭回调函数类型
     */
    typedef void (*SessionClosedFunc)(DrdRdpSession *session, void *user_data);

    /**
     * @brief 构造函数
     * @param peer FreeRDP peer 对象
     * @param parent 父对象
     */
    explicit DrdRdpSession(freerdp_peer *peer, QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~DrdRdpSession() override;

    /**
     * @brief 设置对端地址
     * @param peerAddress 对端地址字符串
     */
    void setPeerAddress(const QString &peerAddress);

    /**
     * @brief 获取对端地址
     * @return 对端地址字符串
     */
    QString peerAddress() const { return m_peerAddress; }

    /**
     * @brief 设置对端状态
     * @param state 状态字符串
     */
    void setPeerState(const QString &state);

    /**
     * @brief 获取对端状态
     * @return 状态字符串
     */
    QString peerState() const { return m_state; }

    /**
     * @brief 设置运行时
     * 
     * @param runtime 运行时对象
     */
    void setRuntime(DrdServerRuntime *runtime);

    /**
     * @brief 获取运行时
     *
     * @return 运行时对象
     */
    DrdServerRuntime *runtime() const { return m_runtime; }

    /**
     * @brief 设置被动模式
     * @param passive 是否启用被动模式
     */
    void setPassiveMode(bool passive);

    /**
     * @brief 获取被动模式状态
     * @return 是否处于被动模式
     */
    bool isPassiveMode() const { return m_passiveMode; }

    /**
     * @brief 设置会话关闭回调
     * @param callback 回调函数
     * @param userData 用户数据
     */
    void setClosedCallback(SessionClosedFunc callback, void *userData);

    /**
     * @brief 启动事件线程
     * @return 成功返回 true
     */
    bool startEventThread();

    /**
     * @brief 停止事件线程
     */
    void stopEventThread();

    /**
     * @brief 启动 VCM 线程
     * @return 成功返回 true
     */
    bool startVcmThread();

    /**
     * @brief 停止 VCM 线程
     */
    void stopVcmThread();

    /**
     * @brief PostConnect 钩子，更新状态
     *
     * @return 成功返回 true
     */
    bool postConnect();

    /**
     * @brief 激活会话
     *
     * @return 成功返回 true
     */
    bool activate();

    /**
     * @brief 断开会话连接
     * @param reason 断开原因
     */
    void disconnect(const QString &reason = QString());

    /**
     * @brief 设置虚拟通道管理器
     * @param vcm 虚拟通道管理器句柄
     */
    void setVirtualChannelManager(HANDLE vcm);

    /**
     * @brief 获取 FreeRDP peer 对象
     * @return peer 对象
     */
    freerdp_peer *peer() const { return m_peer; }

    /**
     * @brief 获取 graphics pipeline
     * @return graphics pipeline 对象
     */
    DrdRdpGraphicsPipeline *graphicsPipeline() const { return m_graphicsPipeline; }

    /**
     * @brief 检查连接是否存活
     * @return 存活返回 true
     */
    bool isConnectionAlive() const { return m_connectionAlive.loadAcquire(); }

private:
    /**
     * @brief 通知会话已关闭
     */
    void notifyClosed();

    /**
     * @brief VCM 线程函数
     */
    void vcmThreadFunc();

    /**
     * @brief 事件线程函数
     */
    void eventThreadFunc();

    /**
     * @brief 渲染线程函数
     */
    void renderThreadFunc();

private:
    /**
     * @brief 尝试初始化 graphics pipeline
     */
    void maybeInitGraphics();

    /**
     * @brief 禁用 graphics pipeline
     */
    void disableGraphicsPipeline();

    /**
     * @brief 启动渲染线程
     * @return 成功返回 true
     */
    bool startRenderThread();

    /**
     * @brief 强制客户端桌面分辨率匹配服务器编码分辨率
     * @return 成功返回 true
     */
    bool enforcePeerDesktopSize();

    /**
     * @brief 刷新表面负载限制
     */
    void refreshSurfacePayloadLimit();

private:
    freerdp_peer *m_peer;
    QString m_peerAddress;
    QString m_state;
    DrdServerRuntime *m_runtime;
    DrdRdpGraphicsPipeline *m_graphicsPipeline;
    bool m_graphicsPipelineReady;
    HANDLE m_vcm;
    QThread *m_eventThread;
    QThread *m_vcmThread;
    QThread *m_renderThread;
    void *m_stopEvent;
    QAtomicInt m_connectionAlive;
    QAtomicInt m_renderRunning;
    quint32 m_frameSequence;
    SessionClosedFunc m_closedCallback;
    void *m_closedCallbackData;
    QAtomicInt m_closedCallbackInvoked;
    bool m_isActivated;
    bool m_passiveMode;
};