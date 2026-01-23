#pragma once

#include <QObject>
#include <QAtomicInt>

#include "utils/drd_frame_queue.h"

// 前置声明，避免在头文件中包含 X11 头文件
class DrdX11Capture;

/**
 * @brief 捕获管理器类
 * 
 * 管理屏幕捕获的生命周期，提供统一的接口
 */
class DrdCaptureManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit DrdCaptureManager(QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~DrdCaptureManager() override;

    /**
     * @brief 启动捕获
     * 
     * @param width 宽度
     * @param height 高度
     * @param error 错误输出
     * @return 成功返回 true
     */
    bool start(quint32 width, quint32 height, QString *error = nullptr);

    /**
     * @brief 停止捕获
     */
    void stop();

    /**
     * @brief 检查是否正在运行
     * 
     * @return 正在运行返回 true
     */
    bool isRunning() const;

    /**
     * @brief 获取显示尺寸
     * 
     * @param outWidth 输出宽度
     * @param outHeight 输出高度
     * @param error 错误输出
     * @return 成功返回 true
     */
    bool getDisplaySize(quint32 *outWidth, quint32 *outHeight, QString *error = nullptr);

    /**
     * @brief 获取帧队列
     * 
     * @return 帧队列指针
     */
    DrdFrameQueue *queue() const { return m_queue; }

    /**
     * @brief 等待帧
     * 
     * @param timeoutUs 超时时间（微秒）
     * @param outFrame 输出帧对象
     * @param error 错误输出
     * @return 成功返回 true
     */
    bool waitFrame(qint64 timeoutUs, DrdFrame **outFrame, QString *error = nullptr);

private:
    QAtomicInt m_running;
    DrdFrameQueue *m_queue;
    DrdX11Capture *m_x11Capture;
};