#pragma once

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QAtomicInt>

#include "utils/drd_frame_queue.h"

// 前向声明 X11 类型，避免在头文件中包含 X11 头文件
struct _XDisplay;
typedef struct _XDisplay Display;
typedef unsigned long Window;
typedef struct _XImage XImage;
typedef unsigned long Damage;

/**
 * @brief X11 屏幕捕获类
 * 
 * 使用 X11 XShm 和 XDamage 扩展进行高效的屏幕捕获
 */
class DrdX11Capture : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param queue 帧队列
     * @param parent 父对象
     */
    explicit DrdX11Capture(DrdFrameQueue *queue, QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~DrdX11Capture() override;

    /**
     * @brief 启动捕获
     * 
     * @param displayName 显示名称（NULL 使用默认）
     * @param requestedWidth 请求的宽度（0 使用屏幕宽度）
     * @param requestedHeight 请求的高度（0 使用屏幕高度）
     * @param error 错误输出
     * @return 成功返回 true
     */
    bool start(const QString &displayName, quint32 requestedWidth, quint32 requestedHeight, QString *error = nullptr);

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
     * @param displayName 显示名称
     * @param outWidth 输出宽度
     * @param outHeight 输出高度
     * @param error 错误输出
     * @return 成功返回 true
     */
    static bool getDisplaySize(const QString &displayName, quint32 *outWidth, quint32 *outHeight, QString *error = nullptr);

private:
    /**
     * @brief 捕获线程函数
     */
    void captureThreadFunc();

    /**
     * @brief 准备显示资源
     */
    bool prepareDisplay(const QString &displayName, quint32 requestedWidth, quint32 requestedHeight, QString *error);

    /**
     * @brief 清理显示资源
     */
    void cleanupDisplay();

    /**
     * @brief 设置唤醒管道
     */
    bool setupWakeupPipe(QString *error);

    /**
     * @brief 关闭唤醒管道
     */
    void closeWakeupPipe();

    /**
     * @brief 清空唤醒管道
     */
    void drainWakeupPipe();

    // 私有实现结构体，隐藏 X11 头文件
    struct Private;
    Private *d;
};