#pragma once

#include <QObject>
#include <QString>
#include <QThread>
#include <QAtomicInt>
#include <freerdp/listener.h>

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
     * @brief 断开会话连接
     * @param reason 断开原因
     */
    void disconnect(const QString &reason = QString());

    /**
     * @brief 获取 FreeRDP peer 对象
     * @return peer 对象
     */
    freerdp_peer *peer() const { return m_peer; }

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
     * @brief 事件线程函数
     */
    void eventThreadFunc();

private:
    freerdp_peer *m_peer;
    QString m_peerAddress;
    QString m_state;
    QThread *m_eventThread;
    void *m_stopEvent;
    QAtomicInt m_connectionAlive;
    SessionClosedFunc m_closedCallback;
    void *m_closedCallbackData;
    QAtomicInt m_closedCallbackInvoked;
};