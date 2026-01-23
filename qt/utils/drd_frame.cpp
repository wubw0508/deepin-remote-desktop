#include "utils/drd_frame.h"

#include <QElapsedTimer>
#include <cstring>

/**
 * @brief 构造函数
 * 
 * 功能：初始化帧对象。
 * 逻辑：设置默认值。
 * 参数：parent 父对象。
 * 外部接口：Qt QObject 构造函数。
 */
DrdFrame::DrdFrame(QObject *parent)
    : QObject(parent)
    , m_width(0)
    , m_height(0)
    , m_stride(0)
    , m_timestamp(0)
{
}

/**
 * @brief 析构函数
 * 
 * 功能：清理帧对象。
 * 逻辑：Qt 会自动清理成员变量。
 * 参数：无。
 * 外部接口：Qt QObject 析构函数。
 */
DrdFrame::~DrdFrame()
{
}

/**
 * @brief 配置帧参数
 * 
 * 功能：设置帧的几何参数和时间戳。
 * 逻辑：更新成员变量。
 * 参数：width 宽度，height 高度，stride 步长，timestamp 时间戳。
 * 外部接口：无。
 */
void DrdFrame::configure(quint32 width, quint32 height, quint32 stride, quint64 timestamp)
{
    m_width = width;
    m_height = height;
    m_stride = stride;
    m_timestamp = timestamp;
}

/**
 * @brief 确保缓冲区容量
 * 
 * 功能：确保内部存储有足够的容量。
 * 逻辑：调整 QByteArray 大小并返回可写指针。
 * 参数：size 需要的字节数。
 * 外部接口：Qt QByteArray API。
 * 返回值：可写的缓冲区指针。
 */
quint8 *DrdFrame::ensureCapacity(qint64 size)
{
    m_data.resize(static_cast<int>(size));
    return reinterpret_cast<quint8 *>(m_data.data());
}

/**
 * @brief 获取数据指针
 * 
 * 功能：获取只读数据指针。
 * 逻辑：返回 QByteArray 的数据指针。
 * 参数：size 输出数据大小（可选）。
 * 外部接口：Qt QByteArray API。
 * 返回值：只读数据指针。
 */
const quint8 *DrdFrame::data(qint64 *size) const
{
    if (size != nullptr)
    {
        *size = m_data.size();
    }
    return reinterpret_cast<const quint8 *>(m_data.constData());
}