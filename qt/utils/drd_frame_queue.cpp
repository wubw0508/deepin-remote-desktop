#include "utils/drd_frame_queue.h"

#include <QElapsedTimer>

/**
 * @brief 构造函数
 * 
 * 功能：初始化帧队列对象。
 * 逻辑：初始化成员变量。
 * 参数：parent 父对象。
 * 外部接口：Qt QObject 构造函数。
 */
DrdFrameQueue::DrdFrameQueue(QObject *parent)
    : QObject(parent)
    , m_droppedFrames(0)
{
    m_stopped.storeRelease(0);
}

/**
 * @brief 析构函数
 * 
 * 功能：清理帧队列对象。
 * 逻辑：停止队列，清空队列。
 * 参数：无。
 * 外部接口：Qt QObject 析构函数。
 */
DrdFrameQueue::~DrdFrameQueue()
{
    stop();
    reset();
}

/**
 * @brief 重置队列
 * 
 * 功能：清空队列并重置状态。
 * 逻辑：清空队列，重置丢弃计数。
 * 参数：无。
 * 外部接口：Qt QQueue API。
 */
void DrdFrameQueue::reset()
{
    QMutexLocker locker(&m_mutex);
    m_queue.clear();
    m_droppedFrames = 0;
}

/**
 * @brief 推入帧
 * 
 * 功能：将帧推入队列，如果队列已满则丢弃最旧的帧。
 * 逻辑：如果队列已满，移除最旧的帧并增加丢弃计数；然后推入新帧并唤醒等待线程。
 * 参数：frame 帧对象。
 * 外部接口：Qt QQueue API，QWaitCondition。
 */
void DrdFrameQueue::push(DrdFrame *frame)
{
    QMutexLocker locker(&m_mutex);

    if (m_stopped.loadAcquire())
    {
        return;
    }

    if (m_queue.size() >= DRD_FRAME_QUEUE_MAX_FRAMES)
    {
        DrdFrame *oldFrame = m_queue.dequeue();
        oldFrame->deleteLater();
        m_droppedFrames++;
    }

    m_queue.enqueue(frame);
    m_condition.wakeOne();
}

/**
 * @brief 等待帧
 * 
 * 功能：等待队列中有可用的帧。
 * 逻辑：如果队列已停止，返回 false；否则等待直到有帧可用或超时。
 * 参数：timeoutUs 超时时间（微秒），-1 表示无限等待；outFrame 输出帧对象。
 * 外部接口：Qt QWaitCondition，QElapsedTimer。
 * 返回值：成功返回 true。
 */
bool DrdFrameQueue::wait(qint64 timeoutUs, DrdFrame **outFrame)
{
    QMutexLocker locker(&m_mutex);

    if (m_stopped.loadAcquire())
    {
        return false;
    }

    if (m_queue.isEmpty())
    {
        if (timeoutUs < 0)
        {
            m_condition.wait(&m_mutex);
        }
        else
        {
            if (!m_condition.wait(&m_mutex, timeoutUs / 1000))
            {
                return false;
            }
        }
    }

    if (m_stopped.loadAcquire() || m_queue.isEmpty())
    {
        return false;
    }

    *outFrame = m_queue.dequeue();
    return true;
}

/**
 * @brief 停止队列
 * 
 * 功能：停止队列，唤醒所有等待的线程。
 * 逻辑：设置停止标志，唤醒所有等待线程。
 * 参数：无。
 * 外部接口：Qt QWaitCondition。
 */
void DrdFrameQueue::stop()
{
    m_stopped.storeRelease(1);
    m_condition.wakeAll();
}