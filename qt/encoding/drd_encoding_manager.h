#pragma once

#include <QObject>
#include <QByteArray>

#include <freerdp/freerdp.h>
#include <freerdp/server/rdpgfx.h>

#include "core/drd_encoding_options.h"
#include "utils/drd_frame.h"

/**
 * @brief 编解码器类别枚举
 */
enum class DrdEncodingCodecClass
{
    Unknown = 0,
    AVC,
    NonAVC
};

/**
 * @brief 编码管理器类
 * 
 * 管理视频编码，支持多种编码格式
 */
class DrdEncodingManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit DrdEncodingManager(QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~DrdEncodingManager() override;

    /**
     * @brief 准备编码器
     * 
     * @param options 编码选项
     * @param error 错误输出
     * @return 成功返回 true
     */
    bool prepare(const DrdEncodingOptions *options, QString *error = nullptr);

    /**
     * @brief 重置编码器
     */
    void reset();

    /**
     * @brief 检查是否已准备
     * 
     * @return 已准备返回 true
     */
    bool isReady() const { return m_ready; }

    /**
     * @brief 编码帧为 Surface GFX
     * 
     * @param settings FreeRDP 设置
     * @param context Rdpgfx 上下文
     * @param surfaceId Surface ID
     * @param input 输入帧
     * @param frameId 帧序号
     * @param h264 输出是否使用 H264
     * @param autoSwitch 自动切换编码策略
     * @param error 错误输出
     * @return 成功返回 true
     */
    bool encodeSurfaceGfx(rdpSettings *settings,
                          RdpgfxServerContext *context,
                          quint16 surfaceId,
                          DrdFrame *input,
                          quint32 frameId,
                          bool *h264,
                          bool autoSwitch,
                          QString *error = nullptr);

    /**
     * @brief 强制关键帧
     */
    void forceKeyframe();

    /**
     * @brief 注册编码结果
     *
     * @param codecClass 编解码器类别
     * @param keyframeEncode 是否关键帧编码
     */
    void registerCodecResult(DrdEncodingCodecClass codecClass, bool keyframeEncode);

private:
    /**
     * @brief 初始化 H264 编码器
     */
    bool initH264();

    /**
     * @brief 初始化 RFX 编码器
     */
    bool initRfx(rdpSettings *settings);

    /**
     * @brief 初始化 Progressive 编码器
     */
    bool initProgressive();

    /**
     * @brief 准备编码器
     */
    bool prepareEncoder(quint32 codecs, rdpSettings *settings);

private:
    quint32 m_frameWidth;
    quint32 m_frameHeight;
    bool m_ready;
    bool m_enableDiff;
    quint32 m_h264Bitrate;
    quint32 m_h264Framerate;
    quint32 m_h264Qp;
    bool m_h264HwAccel;

    QByteArray m_previousFrame;
    bool m_forceKeyframe;
    DrdEncodingCodecClass m_lastCodec;
    DrdEncodingMode m_configMode;  // 保存的配置模式
    bool m_clientSupportsRfx;      ///< 客户端是否支持RFX
    bool m_clientSupportsAvc420;   ///< 客户端是否支持AVC420
    bool m_clientSupportsAvc444;   ///< 客户端是否支持AVC444
    bool m_clientSupportsAvc444v2; ///< 客户端是否支持AVC444v2
    bool m_clientSupportsProgressive; ///< 客户端是否支持Progressive
    quint32 m_remoteFxCodecId;     ///< RemoteFX编解码器ID
    bool m_codecSupportChecked;    ///< 是否已检查客户端编码器支持
    
    // FreeRDP 编码器上下文
    H264_CONTEXT *m_h264;
    RFX_CONTEXT *m_rfx;
    PROGRESSIVE_CONTEXT *m_progressive;
    quint32 m_codecs;
};