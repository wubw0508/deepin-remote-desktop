#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>

#include <freerdp/listener.h>
#include <freerdp/peer.h>
#include <winpr/wtypes.h>

class DrdQtServerRuntime;
class DrdQtLocalSession;
class DrdQtRdpGraphicsPipeline;

class DrdQtRdpSession : public QObject {
    Q_OBJECT
public:
    enum Error {
        ErrorNone = 0,
        ErrorBadCaps,
        ErrorBadMonitorData,
        ErrorCloseStackOnDriverFailure,
        ErrorGraphicsSubsystemFailed,
        ErrorServerRedirection
    };

    explicit DrdQtRdpSession(freerdp_peer *peer, QObject *parent = nullptr);
    ~DrdQtRdpSession();

    void setPeerAddress(const QString &peerAddress);
    QString peerAddress() const;

    void setPeerState(const QString &state);
    QString peerState() const;

    void setRuntime(DrdQtServerRuntime *runtime);
    DrdQtServerRuntime *runtime() const;

    void setVirtualChannelManager(HANDLE vcm);
    HANDLE virtualChannelManager() const;

    void setClosedCallback(std::function<void(DrdQtRdpSession*)> callback);
    void setPassiveMode(bool passive);
    bool isPassiveMode() const;

    void attachLocalSession(DrdQtLocalSession *session);
    DrdQtLocalSession *localSession() const;

    bool postConnect();
    bool activate();
    bool pump();
    void disconnect(const QString &reason = QString());
    void notifyError(Error error);

    bool startEventThread();
    void stopEventThread();

    bool sendServerRedirection(const QString &routingToken,
                              const QString &username,
                              const QString &password,
                              const QString &certificate);

    bool clientIsMstsc() const;
    bool getPeerResolution(quint32 *width, quint32 *height) const;

    bool isRunning() const { return running_; }

private slots:
    void onVcmThreadFinished();
    void onRenderThreadFinished();

private:
    void maybeInitGraphics();
    void disableGraphicsPipeline(const QString &reason = QString());
    bool enforcePeerDesktopSize();
    void refreshSurfacePayloadLimit();
    bool waitForGraphicsCapacity(qint64 timeoutUs);
    bool startRenderThread();
    void stopRenderThread();
    void notifyClosed();
    void updateRefreshTimerState();
    void cancelRefreshTimer();

    static void *vcmThread(void *userData);
    static void *renderThread(void *userData);
    static bool onRefreshTimeout(void *userData);

    freerdp_peer *peer_;
    QString peerAddress_;
    QString state_;
    DrdQtServerRuntime *runtime_;
    HANDLE vcm_;
    QThread *vcmThread_;
    DrdQtRdpGraphicsPipeline *graphicsPipeline_;
    bool graphicsPipelineReady_;
    quint32 frameSequence_;
    qint32 maxSurfacePayload_;
    bool isActivated_;
    QThread *eventThread_;
    HANDLE stopEvent_;
    qint32 connectionAlive_;
    QThread *renderThread_;
    qint32 renderRunning_;
    std::function<void(DrdQtRdpSession*)> closedCallback_;
    bool closedCallbackInvoked_;
    DrdQtLocalSession *localSession_;
    bool passiveMode_;
    Error lastError_;
    quint64 framePullErrors_;
    quint32 congestionRecoveryAttempts_;
    qint64 congestionDisableTime_;
    bool congestionPermanentDisabled_;
    quint32 refreshTimeoutSource_;
    qint32 refreshTimeoutDue_;
    bool running_;
};
