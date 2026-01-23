#pragma once

#include <QObject>
#include <QByteArray>
#include <QAtomicInt>

/**
 * @brief 帧数据类
 * 
 * 存储屏幕捕获的帧数据，包括宽度、高度、步长、时间戳和像素数据
 */
class DrdFrame : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit DrdFrame(QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~DrdFrame() override;

    /**
     * @brief 配置帧参数
     * @param width 宽度
     * @param height 高度
     * @param stride 步长（每行字节数）
     * @param timestamp 时间戳（微秒）
     */
    void configure(quint32 width, quint32 height, quint32 stride, quint64 timestamp);

    /**
     * @brief 获取宽度
     */
    quint32 width() const { return m_width; }

    /**
     * @brief 获取高度
     */
    quint32 height() const { return m_height; }

    /**
     * @brief 获取步长
     */
    quint32 stride() const { return m_stride; }

    /**
     * @brief 获取时间戳
     */
    quint64 timestamp() const { return m_timestamp; }

    /**
     * @brief 确保缓冲区容量
     * @param size 需要的字节数
     * @return 可写的缓冲区指针
     */
    quint8 *ensureCapacity(qint64 size);

    /**
     * @brief 获取数据指针
     * @param size 输出数据大小
     * @return 只读数据指针
     */
    const quint8 *data(qint64 *size = nullptr) const;

    /**
     * @brief 获取数据大小
     */
    qint64 dataSize() const { return m_data.size(); }

private:
    quint32 m_width;
    quint32 m_height;
    quint32 m_stride;
    quint64 m_timestamp;
    QByteArray m_data;
};