#include "capture/drd_capture_manager.h"
#include "capture/drd_x11_capture.h"

#include <QDebug>

/**
 * @brief 构造函数
 * 
 * 功能：初始化捕获管理器对象。
 * 逻辑：创建帧队列和 X11 捕获对象。
 * 参数：parent 父对象。
 * 外部接口：Qt QObject 构造函数。
 */
DrdCaptureManager::DrdCaptureManager(QObject *parent)
    : QObject(parent)
    , m_running(0)
{
    m_queue = new DrdFrameQueue(this);
    m_x11Capture = new DrdX11Capture(m_queue, this);
}

/**
 * @brief 析构函数
 * 
 * 功能：清理捕获管理器对象。
 * 逻辑：停止捕获，Qt 会自动清理子对象。
 * 参数：无。
 * 外部接口：Qt QObject 析构函数。
 */
DrdCaptureManager::~DrdCaptureManager()
{
    stop();
}

/**
 * @brief 启动捕获
 *
 * 功能：启动 X11 捕获线程并准备帧队列。
 * 逻辑：若已运行直接返回；重置队列后启动 X11 捕获，失败则停止队列并返回错误；成功时更新 running 标志。
 * 参数：width/height 期望分辨率，error 输出错误信息。
 * 外部接口：调用 DrdFrameQueue::reset / DrdFrameQueue::stop 控制队列，DrdX11Capture::start 启动捕获。
 * 返回值：成功返回 true。
 */
bool DrdCaptureManager::start(quint32 width, quint32 height, QString *error)
{
    qInfo() << "[PRODUCER] DrdCaptureManager::start() - Starting capture with size" << width << "x" << height;
    
    if (m_running.loadAcquire())
    {
        qInfo() << "[PRODUCER] Capture manager already running";
        return true;
    }

    qInfo() << "[PRODUCER] Resetting frame queue";
    m_queue->reset();

    qInfo() << "[PRODUCER] Starting X11 capture...";
    if (!m_x11Capture->start(QString(), width, height, error))
    {
        qWarning() << "[PRODUCER] Failed to start X11 capture:" << (error ? *error : "unknown error");
        m_queue->stop();
        return false;
    }
    qInfo() << "[PRODUCER] X11 capture started successfully";

    qInfo() << "[PRODUCER] Capture manager entering running state";
    m_running.storeRelease(1);
    return true;
}

/**
 * @brief 停止捕获
 * 
 * 功能：停止捕获线程并清理队列。
 * 逻辑：若未运行直接返回；先停止 X11 捕获与队列，再输出丢帧统计并清除 running 标志。
 * 参数：无。
 * 外部接口：调用 DrdX11Capture::stop、DrdFrameQueue::stop、DrdFrameQueue::droppedFrames。
 */
void DrdCaptureManager::stop()
{
    if (!m_running.loadAcquire())
    {
        return;
    }

    m_x11Capture->stop();
    m_queue->stop();

    const quint64 dropped = m_queue->droppedFrames();
    if (dropped > 0)
    {
        qWarning() << "Capture manager dropped" << dropped << "frame(s) due to backpressure";
    }

    qInfo() << "Capture manager leaving running state";
    m_running.storeRelease(0);
}

/**
 * @brief 检查是否正在运行
 * 
 * 功能：查询捕获管理器是否处于运行状态。
 * 逻辑：返回 running 标志。
 * 返回值：正在运行返回 true。
 */
bool DrdCaptureManager::isRunning() const
{
    return m_running.loadAcquire() != 0;
}

/**
 * @brief 获取显示尺寸
 * 
 * 功能：获取当前显示的实际分辨率。
 * 逻辑：委托 X11 捕获模块读取 Display 宽高。
 * 参数：outWidth/outHeight 输出值，error 错误输出。
 * 外部接口：DrdX11Capture::getDisplaySize。
 * 返回值：成功返回 true。
 */
bool DrdCaptureManager::getDisplaySize(quint32 *outWidth, quint32 *outHeight, QString *error)
{
    if (outWidth == nullptr || outHeight == nullptr)
    {
        return false;
    }

    return DrdX11Capture::getDisplaySize(QString(), outWidth, outHeight, error);
}

/**
 * @brief 等待帧
 *
 * 功能：在运行状态下等待捕获帧输出。
 * 逻辑：若未运行则报错；调用帧队列等待接口获取帧，超时或失败返回错误；成功时返回帧对象。
 * 参数：timeoutUs 超时时间（微秒），outFrame 输出帧，error 错误输出。
 * 外部接口：依赖 DrdFrameQueue::wait 阻塞等待。
 * 返回值：成功返回 true。
 */
bool DrdCaptureManager::waitFrame(qint64 timeoutUs, DrdFrame **outFrame, QString *error)
{
    if (outFrame == nullptr)
    {
        qWarning() << "[CONSUMER] waitFrame: outFrame is null";
        return false;
    }

    if (!m_running.loadAcquire())
    {
        qWarning() << "[CONSUMER] waitFrame: Capture manager is not running";
        if (error)
        {
            *error = "Capture manager is not running";
        }
        return false;
    }

    if (!m_queue->wait(timeoutUs, outFrame))
    {
        if (error)
        {
            *error = "Timed out waiting for capture frame";
        }

        // 减少超时日志输出频率，避免刷屏
        // static quint32 waitFrameTimeoutLogCounter = 0;
        // if (waitFrameTimeoutLogCounter % 100 == 0) // 每100次超时输出一次日志
        // {
        //     qDebug() << "[CONSUMER] waitFrame: Timed out waiting for frame (timeout:" << timeoutUs << "us, timeout" << waitFrameTimeoutLogCounter << ")";
        // }
        // waitFrameTimeoutLogCounter++;
        return false;
    }
    
    // 减少帧接收日志输出频率，避免刷屏
    static quint32 waitFrameframeReceivedLogCounter = 0;
    if (waitFrameframeReceivedLogCounter % 100 == 0) // 每100帧输出一次日志
    {
        qDebug() << "[CONSUMER] waitFrame: Frame received from queue (frame" << waitFrameframeReceivedLogCounter << ")";
    }
    waitFrameframeReceivedLogCounter++;

    return true;
}