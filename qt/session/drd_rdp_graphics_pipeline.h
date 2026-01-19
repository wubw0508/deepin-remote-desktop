#pragma once

#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <QtGlobal>

#include <freerdp/freerdp.h>
#include <freerdp/server/rdpgfx.h>
#include <winpr/wtypes.h>

class DrdQtServerRuntime;

class DrdQtRdpGraphicsPipeline : public QObject {
    Q_OBJECT
public:
    explicit DrdQtRdpGraphicsPipeline(freerdp_peer *peer,
                                     HANDLE vcm,
                                     DrdQtServerRuntime *runtime,
                                     quint16 surfaceWidth,
                                     quint16 surfaceHeight,
                                     QObject *parent = nullptr);
    ~DrdQtRdpGraphicsPipeline();

    bool maybeInit();
    bool isReady() const;
    bool canSubmit() const;
    bool waitForCapacity(qint64 timeoutUs);
    quint16 getSurfaceId() const;
    void outFrameChange(bool add);
    RdpgfxServerContext *getContext();
    void setLastFrameMode(bool h264);

private:
    quint32 buildTimestamp();
    bool resetLocked();
    static BOOL channelAssigned(RdpgfxServerContext *context, quint32 channelId);
    static quint32 frameAck(RdpgfxServerContext *context, const RDPGFX_FRAME_ACKNOWLEDGE_PDU *ack);
    static quint32 capsAdvertise(RdpgfxServerContext *context, const RDPGFX_CAPS_ADVERTISE_PDU *capsAdvertise);

    freerdp_peer *peer_;
    quint16 width_;
    quint16 height_;

    RdpgfxServerContext *rdpgfxContext_;
    bool channelOpened_;
    bool capsConfirmed_;
    bool surfaceReady_;

    quint16 surfaceId_;
    quint32 codecContextId_;
    quint32 nextFrameId_;
    qint32 outstandingFrames_;
    quint32 maxOutstandingFrames_;
    quint32 channelId_;
    bool frameAcksSuspended_;
    mutable QMutex lock_;
    QWaitCondition capacityCond_;

    DrdQtServerRuntime *runtime_;
    bool lastFrameH264_;
};
