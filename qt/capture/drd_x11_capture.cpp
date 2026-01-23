#include "capture/drd_x11_capture.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <cstring>

// X11 头文件必须在 cpp 文件中包含，避免在头文件中与 Qt 冲突
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/XShm.h>

// 捕获指标常量
#define DRD_CAPTURE_TARGET_FPS 30
#define DRD_CAPTURE_TARGET_INTERVAL_US (1000000 / DRD_CAPTURE_TARGET_FPS)
#define DRD_CAPTURE_STATS_INTERVAL_US 5000000 // 5秒

/**
 * @brief 私有实现结构体
 */
struct DrdX11Capture::Private
{
    DrdFrameQueue *queue;
    QThread *captureThread;
    QMutex stateMutex;
    QAtomicInt running;
    QString displayName;

    Display *display;
    int screen;
    Window root;
    XImage *image;
    XShmSegmentInfo shmInfo;
    bool attached;
    Damage damage;
    int damageEventBase;

    quint32 width;
    quint32 height;
    int wakeupPipe[2];

    Private(DrdFrameQueue *q)
        : queue(q)
        , captureThread(nullptr)
        , running(0)
        , display(nullptr)
        , screen(0)
        , root(0)
        , image(nullptr)
        , attached(false)
        , damage(0)
        , damageEventBase(0)
        , width(0)
        , height(0)
    {
        memset(&shmInfo, 0, sizeof(shmInfo));
        shmInfo.shmid = -1;
        wakeupPipe[0] = -1;
        wakeupPipe[1] = -1;
    }
};

/**
 * @brief 构造函数
 * 
 * 功能：初始化 X11 捕获对象。
 * 逻辑：初始化成员变量。
 * 参数：queue 帧队列，parent 父对象。
 * 外部接口：Qt QObject 构造函数。
 */
DrdX11Capture::DrdX11Capture(DrdFrameQueue *queue, QObject *parent)
    : QObject(parent)
    , d(new Private(queue))
{
}

/**
 * @brief 析构函数
 * 
 * 功能：清理 X11 捕获对象。
 * 逻辑：停止捕获，清理资源。
 * 参数：无。
 * 外部接口：Qt QObject 析构函数。
 */
DrdX11Capture::~DrdX11Capture()
{
    stop();
    delete d;
}

/**
 * @brief 获取显示尺寸
 * 
 * 功能：读取当前显示的实际分辨率。
 * 逻辑：打开 X11 Display，读取屏幕宽高后关闭连接。
 * 参数：displayName 显示名称，outWidth/outHeight 输出值，error 错误输出。
 * 外部接口：X11 XOpenDisplay/DisplayWidth/DisplayHeight/XCloseDisplay。
 * 返回值：成功返回 true。
 */
bool DrdX11Capture::getDisplaySize(const QString &displayName, quint32 *outWidth, quint32 *outHeight, QString *error)
{
    if (outWidth == nullptr || outHeight == nullptr)
    {
        return false;
    }

    Display *display = XOpenDisplay(displayName.isEmpty() ? nullptr : displayName.toUtf8().constData());
    if (display == nullptr)
    {
        if (error)
        {
            *error = "Failed to open X11 display for resolution query";
        }
        return false;
    }

    const int screen = DefaultScreen(display);
    *outWidth = static_cast<quint32>(DisplayWidth(display, screen));
    *outHeight = static_cast<quint32>(DisplayHeight(display, screen));
    XCloseDisplay(display);

    if (*outWidth == 0 || *outHeight == 0)
    {
        if (error)
        {
            *error = QString("Invalid display size %1x%2").arg(*outWidth).arg(*outHeight);
        }
        return false;
    }

    return true;
}

/**
 * @brief 准备显示资源
 * 
 * 功能：打开 X11 连接并准备共享内存截图资源。
 * 逻辑：依次打开 Display，检测 XShm/XDamage 扩展；获取屏幕/root 窗口与目标尺寸；创建 XShm 图像与共享内存段并附加；创建 Damage 句柄。
 * 参数：displayName 显示名称，requestedWidth/height 期望尺寸，error 错误输出。
 * 外部接口：X11/XShm/XDamage 相关 API。
 * 返回值：成功返回 true。
 */
bool DrdX11Capture::prepareDisplay(const QString &displayName, quint32 requestedWidth, quint32 requestedHeight, QString *error)
{
    d->display = XOpenDisplay(displayName.isEmpty() ? nullptr : displayName.toUtf8().constData());
    if (d->display == nullptr)
    {
        if (error)
        {
            *error = "Failed to open X11 display";
        }
        return false;
    }

    if (!XShmQueryExtension(d->display))
    {
        if (error)
        {
            *error = "XShm extension not available on X server";
        }
        return false;
    }

    int damageEvent = 0;
    int damageError = 0;
    if (!XDamageQueryExtension(d->display, &damageEvent, &damageError))
    {
        if (error)
        {
            *error = "XDamage extension not available on X server";
        }
        return false;
    }
    d->damageEventBase = damageEvent;

    d->screen = DefaultScreen(d->display);
    d->root = RootWindow(d->display, d->screen);

    d->width = (requestedWidth > 0) ? requestedWidth : static_cast<quint32>(DisplayWidth(d->display, d->screen));
    d->height = (requestedHeight > 0) ? requestedHeight : static_cast<quint32>(DisplayHeight(d->display, d->screen));

    d->image = XShmCreateImage(d->display,
                               DefaultVisual(d->display, d->screen),
                               DefaultDepth(d->display, d->screen),
                               ZPixmap,
                               nullptr,
                               &d->shmInfo,
                               static_cast<int>(d->width),
                               static_cast<int>(d->height));
    if (d->image == nullptr)
    {
        if (error)
        {
            *error = "Failed to create XShm image";
        }
        return false;
    }

    const size_t image_size = static_cast<size_t>(d->image->bytes_per_line) * static_cast<size_t>(d->image->height);
    d->shmInfo.shmid = shmget(IPC_PRIVATE, image_size, IPC_CREAT | 0600);
    if (d->shmInfo.shmid < 0)
    {
        if (error)
        {
            *error = QString("shmget failed: %1").arg(strerror(errno));
        }
        return false;
    }

    d->shmInfo.shmaddr = reinterpret_cast<char *>(shmat(d->shmInfo.shmid, nullptr, 0));
    if (d->shmInfo.shmaddr == reinterpret_cast<char *>(-1))
    {
        if (error)
        {
            *error = QString("shmat failed: %1").arg(strerror(errno));
        }
        return false;
    }

    d->shmInfo.readOnly = False;
    d->image->data = d->shmInfo.shmaddr;

    if (!XShmAttach(d->display, &d->shmInfo))
    {
        if (error)
        {
            *error = "XShmAttach failed";
        }
        return false;
    }
    d->attached = true;

    d->damage = XDamageCreate(d->display, d->root, XDamageReportNonEmpty);
    if (d->damage == 0)
    {
        if (error)
        {
            *error = "Failed to create XDamage handle";
        }
        return false;
    }

    XSync(d->display, False);
    return true;
}

/**
 * @brief 清理显示资源
 * 
 * 功能：清理 X11 捕获持有的底层资源。
 * 逻辑：销毁 Damage 句柄；卸载共享内存与图像；分离并回收 SysV 共享内存；关闭 X Display。
 * 参数：无。
 * 外部接口：XDamageDestroy/XShmDetach/XDestroyImage/shmdt/shmctl/XCloseDisplay 等 X11 与 SysV 共享内存 API。
 */
void DrdX11Capture::cleanupDisplay()
{
    if (d->damage != 0 && d->display != nullptr)
    {
        XDamageDestroy(d->display, d->damage);
        d->damage = 0;
    }

    if (d->attached && d->display != nullptr)
    {
        XShmDetach(d->display, &d->shmInfo);
        d->attached = false;
    }

    if (d->image != nullptr)
    {
        d->image->data = nullptr;
        XDestroyImage(d->image);
        d->image = nullptr;
    }

    if (d->shmInfo.shmaddr != nullptr)
    {
        shmdt(d->shmInfo.shmaddr);
        d->shmInfo.shmaddr = nullptr;
    }

    if (d->shmInfo.shmid >= 0)
    {
        shmctl(d->shmInfo.shmid, IPC_RMID, nullptr);
        d->shmInfo.shmid = -1;
        d->shmInfo.shmseg = 0;
    }

    if (d->display != nullptr)
    {
        XCloseDisplay(d->display);
        d->display = nullptr;
    }
}

/**
 * @brief 设置唤醒管道
 * 
 * 功能：创建唤醒管道供线程退出时使用。
 * 逻辑：通过 pipe 创建管道并缓存 fd。
 * 参数：error 错误输出。
 * 外部接口：POSIX pipe。
 * 返回值：成功返回 true。
 */
bool DrdX11Capture::setupWakeupPipe(QString *error)
{
    if (d->wakeupPipe[0] >= 0 && d->wakeupPipe[1] >= 0)
    {
        return true;
    }

    if (pipe(d->wakeupPipe) < 0)
    {
        if (error)
        {
            *error = QString("Failed to create wakeup pipe: %1").arg(strerror(errno));
        }
        return false;
    }

    return true;
}

/**
 * @brief 关闭唤醒管道
 * 
 * 功能：关闭并清理唤醒管道 fd。
 * 逻辑：遍历两个 fd，若有效则 close 并置为 -1。
 * 参数：无。
 * 外部接口：POSIX close。
 */
void DrdX11Capture::closeWakeupPipe()
{
    for (int i = 0; i < 2; ++i)
    {
        if (d->wakeupPipe[i] >= 0)
        {
            close(d->wakeupPipe[i]);
            d->wakeupPipe[i] = -1;
        }
    }
}

/**
 * @brief 清空唤醒管道
 * 
 * 功能：清空唤醒管道中的残留数据。
 * 逻辑：若 fd 有效则循环读取直到无数据。
 * 参数：无。
 * 外部接口：POSIX read。
 */
void DrdX11Capture::drainWakeupPipe()
{
    if (d->wakeupPipe[0] < 0)
    {
        return;
    }

    char buffer[64];
    while (read(d->wakeupPipe[0], buffer, sizeof(buffer)) > 0)
    {
    }
}

/**
 * @brief 启动捕获
 *
 * 功能：启动 X11 捕获线程并准备资源。
 * 逻辑：持锁检查运行状态；记录 display 名称；创建唤醒管道并准备显示资源；成功后标记 running 并启动线程。
 * 参数：displayName 目标显示，requestedWidth/height 期望尺寸，error 错误输出。
 * 外部接口：Qt QThread，内部调用 setupWakeupPipe 与 prepareDisplay。
 * 返回值：成功返回 true。
 */
bool DrdX11Capture::start(const QString &displayName, quint32 requestedWidth, quint32 requestedHeight, QString *error)
{
    qInfo() << "[PRODUCER] DrdX11Capture::start() - Starting X11 capture";
    qInfo() << "[PRODUCER] Requested size:" << requestedWidth << "x" << requestedHeight;
    qInfo() << "[PRODUCER] Display name:" << (displayName.isEmpty() ? "default" : displayName);
    
    QMutexLocker locker(&d->stateMutex);

    if (d->running.loadAcquire())
    {
        qInfo() << "[PRODUCER] X11 capture already running";
        return true;
    }

    d->displayName = displayName;

    qInfo() << "[PRODUCER] Setting up wakeup pipe...";
    if (!setupWakeupPipe(error))
    {
        qWarning() << "[PRODUCER] Failed to setup wakeup pipe:" << (error ? *error : "unknown error");
        return false;
    }
    qInfo() << "[PRODUCER] Wakeup pipe setup completed";

    qInfo() << "[PRODUCER] Preparing display resources...";
    if (!prepareDisplay(displayName, requestedWidth, requestedHeight, error))
    {
        qWarning() << "[PRODUCER] Failed to prepare display:" << (error ? *error : "unknown error");
        cleanupDisplay();
        closeWakeupPipe();
        return false;
    }
    qInfo() << "[PRODUCER] Display resources prepared - actual size:" << d->width << "x" << d->height;

    d->running.storeRelease(1);
    qInfo() << "[PRODUCER] Creating and starting capture thread...";
    d->captureThread = QThread::create([this]() {
        captureThreadFunc();
    });
    d->captureThread->start();

    qInfo() << "[PRODUCER] X11 capture started at" << d->width << "x" << d->height;
    return true;
}

/**
 * @brief 停止捕获
 * 
 * 功能：停止捕获线程并回收资源。
 * 逻辑：持锁检查 running；清除运行标志，先尝试唤醒阻塞线程，再 join 线程；随后持锁调用 cleanup 与关闭唤醒管道。
 * 参数：无。
 * 外部接口：XSync 同步 X 连接；write 唤醒管道；Qt QThread。
 */
void DrdX11Capture::stop()
{
    QMutexLocker locker(&d->stateMutex);

    if (!d->running.loadAcquire())
    {
        return;
    }

    d->running.storeRelease(0);

    Display *display = d->display;
    locker.unlock();

    if (display != nullptr)
    {
        XSync(display, False);
    }

    if (d->wakeupPipe[1] >= 0)
    {
        const char signal_byte = 'x';
        if (write(d->wakeupPipe[1], &signal_byte, 1) < 0)
        {
            // 忽略错误
        }
    }

    if (d->captureThread != nullptr)
    {
        d->captureThread->wait();
        delete d->captureThread;
        d->captureThread = nullptr;
    }

    locker.relock();
    cleanupDisplay();
    closeWakeupPipe();

    qInfo() << "X11 capture stopped";
}

/**
 * @brief 检查是否正在运行
 * 
 * 功能：查询捕获线程是否运行。
 * 逻辑：持锁读取 running 标志并返回。
 * 返回值：正在运行返回 true。
 */
bool DrdX11Capture::isRunning() const
{
    return d->running.loadAcquire() != 0;
}

/**
 * @brief 捕获线程函数
 *
 * 功能：捕获线程主循环，从 X11 拉帧并写入队列。
 * 逻辑：循环读取运行状态与资源；按 target_interval 驱动一次事件消费与抓帧，期间用 poll 监听 X 连接和唤醒管道；每个间隔都会触发一次抓帧，XDamage 事件仅用于清理队列与统计。
 * 参数：无。
 * 外部接口：XPending/XNextEvent/XDamageSubtract 处理 Damage 事件；poll 监听文件描述符；XShmGetImage 抓帧；DrdFrame API 与 drd_frame_queue_push。
 */
void DrdX11Capture::captureThreadFunc()
{
    qInfo() << "[PRODUCER] X11 capture thread started";
    
    const quint32 target_fps = DRD_CAPTURE_TARGET_FPS;
    const qint64 target_interval = DRD_CAPTURE_TARGET_INTERVAL_US;
    const qint64 stats_interval = DRD_CAPTURE_STATS_INTERVAL_US;
    qint64 stats_window_start = 0;
    quint32 stats_frames = 0;
    qint64 next_capture_deadline = 0;
    qint64 now = 0;
    
    qInfo() << "[PRODUCER] Capture thread parameters - target_fps:" << target_fps
            << "target_interval_us:" << target_interval
            << "stats_interval_us:" << stats_interval;

    while (true)
    {
        Display *display = nullptr;
        XImage *image = nullptr;
        Window root = 0;
        int damage_event_base = 0;
        quint32 width = 0;
        quint32 height = 0;
        bool running = false;
        int wake_fd = -1;
        bool damage = false;

        {
            QMutexLocker locker(&d->stateMutex);
            running = d->running.loadAcquire() != 0;
            display = d->display;
            image = d->image;
            root = d->root;
            damage_event_base = d->damageEventBase;
            width = d->width;
            height = d->height;
            wake_fd = d->wakeupPipe[0];
        }

        if (!running || display == nullptr || image == nullptr)
        {
            qInfo() << "[PRODUCER] X11 capture thread exiting - running:" << running
                    << "display:" << (display != nullptr)
                    << "image:" << (image != nullptr);
            break;
        }

        if (next_capture_deadline == 0)
        {
            next_capture_deadline = QDateTime::currentMSecsSinceEpoch() * 1000;
            qInfo() << "[PRODUCER] First capture deadline initialized";
        }

        // 使用 poll 监听 X 连接和唤醒管道
        struct pollfd pfds[2];
        int poll_count = 0;
        int wake_index = -1;
        const int connection_fd = XConnectionNumber(display);
        pfds[poll_count].fd = connection_fd;
        pfds[poll_count].events = POLLIN;
        pfds[poll_count].revents = 0;
        poll_count++;

        if (wake_fd >= 0)
        {
            wake_index = poll_count;
            pfds[poll_count].fd = wake_fd;
            pfds[poll_count].events = POLLIN;
            pfds[poll_count].revents = 0;
            poll_count++;
        }

        int poll_result = poll(pfds, poll_count, target_interval / 1000);
        if (poll_result < 0)
        {
            qWarning() << "[PRODUCER] poll() failed, continuing";
            continue;
        }

        if (poll_result > 0 && wake_index >= 0 && (pfds[wake_index].revents & POLLIN))
        {
            qInfo() << "[PRODUCER] Wakeup pipe signaled, draining";
            drainWakeupPipe();
        }

        // 处理 X11 事件
        int pending_events = 0;
        while (XPending(display) > 0)
        {
            XEvent event;
            XNextEvent(display, &event);
            if (event.type == damage_event_base + XDamageNotify)
            {
                XDamageSubtract(display, d->damage, None, None);
                damage = true;
                pending_events++;
            }
        }
        
        if (!damage)
        {
            continue;
        }

        now = QDateTime::currentMSecsSinceEpoch() * 1000;
        if (now < next_capture_deadline)
        {
            continue;
        }

        // 抓取帧
        if (!XShmGetImage(display, root, image, 0, 0, AllPlanes))
        {
            qWarning() << "[PRODUCER] XShmGetImage failed, retrying";
            next_capture_deadline = now + target_interval;
            continue;
        }

        // qDebug() << "[PRODUCER] Frame captured successfully - size:" << width << "x" << height;

        stats_frames++;

        // 创建帧对象
        DrdFrame *frame = new DrdFrame();
        now = QDateTime::currentMSecsSinceEpoch() * 1000;
        frame->configure(width, height, static_cast<quint32>(image->bytes_per_line), static_cast<quint64>(now));

        const qint64 frame_size = static_cast<qint64>(image->bytes_per_line) * static_cast<qint64>(image->height);
        quint8 *buffer = frame->ensureCapacity(frame_size);
        if (buffer != nullptr)
        {
            memcpy(buffer, image->data, static_cast<size_t>(frame_size));
            d->queue->push(frame);
            // qDebug() << "[PRODUCER] Frame pushed to queue - size:" << frame_size << "bytes";
        }
        else
        {
            qWarning() << "[PRODUCER] Failed to allocate frame buffer, dropping frame";
            delete frame;
        }

        // 统计信息
        if (stats_window_start == 0)
        {
            stats_window_start = now;
        }
        else
        {
            const qint64 stats_elapsed = now - stats_window_start;
            if (stats_elapsed >= stats_interval)
            {
                const double actual_fps = static_cast<double>(stats_frames) * 1000000.0 / static_cast<double>(stats_elapsed);
                const bool reached_target = actual_fps >= static_cast<double>(target_fps);
                qInfo() << "[PRODUCER] X11 capture fps=" << actual_fps << "(target=" << target_fps << "):" << (reached_target ? "reached target" : "below target");
                stats_frames = 0;
                stats_window_start = now;
            }
        }

        next_capture_deadline += target_interval;
        if (next_capture_deadline < now)
        {
            next_capture_deadline = now + target_interval;
        }
    }
    
    qInfo() << "[PRODUCER] X11 capture thread exited";
}
