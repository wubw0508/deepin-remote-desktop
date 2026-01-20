#pragma once

#include <QObject>
#include <QString>

#include "core/drd_encoding_options.h"

// 前向声明
class DrdCaptureManager;
class DrdEncodingManager;
class DrdInputDispatcher;
class DrdTlsCredentials;

/**
 * @brief 帧传输模式枚举
 */
enum class DrdFrameTransport
{
    SurfaceBits = 0,
    GraphicsPipeline
};

/**
 * @brief Qt 版本的 DrdServerRuntime 类
 * 
 * 替代 GObject 版本的 DrdServerRuntime，使用 Qt 的对象系统
 */
class DrdServerRuntime : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit DrdServerRuntime(QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~DrdServerRuntime() override;

    /**
     * @brief 获取捕获管理器
     */
    DrdCaptureManager *capture() const { return m_capture; }

    /**
     * @brief 获取编码管理器
     */
    DrdEncodingManager *encoder() const { return m_encoder; }

    /**
     * @brief 获取输入分发器
     */
    DrdInputDispatcher *input() const { return m_input; }

    /**
     * @brief 准备流
     * @param encodingOptions 编码选项
     * @param error 错误输出
     * @return 成功返回 true
     */
    bool prepareStream(const DrdEncodingOptions *encodingOptions, QString *error = nullptr);

    /**
     * @brief 停止运行时
     */
    void stop();

    /**
     * @brief 设置传输模式
     */
    void setTransport(DrdFrameTransport transport);

    /**
     * @brief 获取传输模式
     */
    DrdFrameTransport transport() const { return m_transportMode; }

    /**
     * @brief 获取编码选项
     * @param outOptions 输出编码选项
     * @return 成功返回 true
     */
    bool getEncodingOptions(DrdEncodingOptions *outOptions) const;

    /**
     * @brief 设置编码选项
     */
    void setEncodingOptions(const DrdEncodingOptions *encodingOptions);

    /**
     * @brief 检查流是否正在运行
     */
    bool isStreamRunning() const { return m_streamRunning; }

    /**
     * @brief 设置 TLS 凭据
     */
    void setTlsCredentials(DrdTlsCredentials *credentials);

    /**
     * @brief 获取 TLS 凭据
     */
    DrdTlsCredentials *tlsCredentials() const { return m_tlsCredentials; }

    /**
     * @brief 请求关键帧
     */
    void requestKeyframe();

private:
    DrdCaptureManager *m_capture;
    DrdEncodingManager *m_encoder;
    DrdInputDispatcher *m_input;
    DrdTlsCredentials *m_tlsCredentials;
    DrdEncodingOptions m_encodingOptions;
    bool m_hasEncodingOptions;
    bool m_streamRunning;
    DrdFrameTransport m_transportMode;
};