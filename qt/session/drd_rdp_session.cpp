#include "session/drd_rdp_session.h"

#include "utils/drd_log.h"
#include <winpr/synch.h>
#include <winpr/wtypes.h>
#include <QThread>
#include <QDebug>

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
    , m_eventThread(nullptr)
    , m_stopEvent(nullptr)
    , m_closedCallback(nullptr)
    , m_closedCallbackData(nullptr)
{
    m_connectionAlive.storeRelease(0);
    m_closedCallbackInvoked.storeRelease(0);

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
    stopEventThread();
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
    DRD_LOG_MESSAGE("Session %s state %s", m_peerAddress.toUtf8().constData(), m_state.toUtf8().constData());
}

/**
 * @brief 设置会话关闭回调
 * 
 * 功能：设置会话关闭时的回调函数。
 * 逻辑：保存回调函数和用户数据，如果连接已关闭则立即调用。
 * 参数：callback 回调函数，userData 用户数据。
 * 外部接口：无。
 */
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
            DRD_LOG_WARNING("Session %s failed to create stop event", m_peerAddress.toUtf8().constData());
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

    if (m_stopEvent != nullptr)
    {
        SetEvent(m_stopEvent);
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

    notifyClosed();
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
        DRD_LOG_MESSAGE("Disconnecting session %s: %s", m_peerAddress.toUtf8().constData(), reason.toUtf8().constData());
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
 * @brief 事件线程函数
 * 
 * 功能：处理 FreeRDP 事件循环。
 * 逻辑：等待事件，调用 CheckFileDescriptor 处理。
 * 参数：无。
 * 外部接口：FreeRDP peer GetEventHandles/CheckFileDescriptor，WinPR WaitForMultipleObjects。
 */
void DrdRdpSession::eventThreadFunc()
{
    freerdp_peer *peer = m_peer;

    if (peer == nullptr)
    {
        return;
    }

    while (m_connectionAlive.loadAcquire())
    {
        HANDLE events[32];
        DWORD count = 0;

        if (m_stopEvent != nullptr)
        {
            events[count++] = m_stopEvent;
        }

        const uint32_t peer_events = peer->GetEventHandles(peer, &events[count], 32 - count);
        if (!peer_events)
        {
            DRD_LOG_WARNING("Session %s missing peer events", m_peerAddress.toUtf8().constData());
            m_connectionAlive.storeRelease(0);
            break;
        }
        count += peer_events;

        DWORD status = WAIT_TIMEOUT;
        if (count > 0)
        {
            status = WaitForMultipleObjects(count, events, FALSE, INFINITE);
        }

        if (status == WAIT_FAILED)
        {
            m_connectionAlive.storeRelease(0);
            break;
        }

        if (!peer->CheckFileDescriptor(peer))
        {
            DRD_LOG_WARNING("Session %s CheckFileDescriptor failed", m_peerAddress.toUtf8().constData());
            m_connectionAlive.storeRelease(0);
            break;
        }
    }

    notifyClosed();
}