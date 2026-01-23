#pragma once

#include <QObject>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QAtomicInt>

#include "utils/drd_frame.h"

#define DRD_FRAME_QUEUE_MAX_FRAMES 3

/**
 * @brief 帧队列类
 * 
 * 管理捕获帧的队列，支持线程安全的推入和等待操作
 */
class DrdFrameQueue : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit DrdFrameQueue(QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~DrdFrameQueue() override;

    /**
     * @brief 重置队列
     * 
     * 清空队列并重置状态
     */
    void reset();

    /**
     * @brief 推入帧
     * 
     * 将帧推入队列，如果队列已满则丢弃最旧的帧
     * @param frame 帧对象
     */
    void push(DrdFrame *frame);

    /**
     * @brief 等待帧
     * 
     * 等待队列中有可用的帧
     * @param timeoutUs 超时时间（微秒），-1 表示无限等待
     * @param outFrame 输出帧对象
     * @return 成功返回 true
     */
    bool wait(qint64 timeoutUs, DrdFrame **outFrame);

    /**
     * @brief 停止队列
     * 
     * 停止队列，唤醒所有等待的线程
     */
    void stop();

    /**
     * @brief 获取丢弃的帧数
     * 
     * @return 丢弃的帧数
     */
    quint64 droppedFrames() const { return m_droppedFrames; }

private:
    QQueue<DrdFrame *> m_queue;
    QMutex m_mutex;
    QWaitCondition m_condition;
    QAtomicInt m_stopped;
    quint64 m_droppedFrames;
};