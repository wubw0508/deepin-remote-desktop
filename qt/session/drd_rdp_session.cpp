
#include "drd_rdp_session.h"
#include "drd_rdp_session.moc"

#include <QThread>
#include <QDateTime>
#include <QDebug>

#include <freerdp/channels/drdynvc.h>
#include <freerdp/channels/wtsvc.h>
#include <freerdp/codec/bitmap.h>
#include <freerdp/constants.h>
#include <freerdp/crypto/certificate.h>
#include <freerdp/crypto/crypto.h>
#include <freerdp/freerdp.h>
#include <freerdp/redirection.h>
#include <freerdp/update.h>

#include <winpr/crypto.h>
#include <winpr/stream.h>
#include <winpr/string.h>
#include <winpr/synch.h>
#include <winpr/wtypes.h>

#include "core/drd_server_runtime.h"
#include "security/drd_local_session.h"
#include "drd_rdp_graphics_pipeline.h"
#include <QDebug>

#define ELEMENT_TYPE_CERTIFICATE 32

namespace {

constexpr quint8 kTpktVersion = 3;
constexpr quint32 kProtocolRdstls = 0x00000004;
constexpr int kMinTpktLength = 4 + 7;
constexpr char kRoutingTokenPrefix[] = "Cookie: msts=";

int findCrlf(const QByteArray &buffer, int start) {
    for (int i = start; i + 1 < buffer.size(); ++i) {
        if (buffer.at(i) == '\r' && buffer.at(i + 1) == '\n') {
            return i;
        }
    }
    return -1;
}

QString readRoutingToken(const QByteArray &buffer, int start, int *lineEnd) {
    const QByteArray prefix = QByteArrayLiteral(kRoutingTokenPrefix);
    if (buffer.size() - start < prefix.size()) {
        return QString();
    }
    if (buffer.mid(start, prefix.size()) != prefix) {
        return QString();
    }
    int end = findCrlf(buffer, start);
    if (end < 0) {
        return QString();
    }
    *lineEnd = end;
    const int tokenStart = start + prefix.size();
    return QString::fromLatin1(buffer.mid(tokenStart, end - tokenStart));
}

WCHAR *getUtf16String(const char *str, size_t *size) {
    if (!str || !size) return nullptr;
    
    *size = 0;
    WCHAR *utf16 = ConvertUtf8ToWCharAlloc(str, size);
    if (!utf16) return nullptr;
    
    *size = (*size + 1) * sizeof(WCHAR);
    return utf16;
}

// TODO: Implement generateRedirectionGuid
WCHAR *generateRedirectionGuid(size_t *size) {
    Q_UNUSED(size);
    return nullptr;
}

// TODO: Implement getCertificateContainer
BYTE *getCertificateContainer(const char *certificate, size_t *size) {
    Q_UNUSED(certificate);
    Q_UNUSED(size);
    return nullptr;
}

} // namespace

DrdQtRdpSession::DrdQtRdpSession(freerdp_peer *peer, QObject *parent)
    : QObject(parent)
    , peer_(peer)
    , peerAddress_("unknown")
    , state_("created")
    , runtime_(nullptr)
    , vcm_(INVALID_HANDLE_VALUE)
    , vcmThread_(nullptr)
    , graphicsPipeline_(nullptr)
    , graphicsPipelineReady_(false)
    , frameSequence_(1)
    , maxSurfacePayload_(0)
    , isActivated_(false)
    , eventThread_(nullptr)
    , stopEvent_(nullptr)
    , connectionAlive_(1)
    , renderThread_(nullptr)
    , renderRunning_(0)
    , closedCallbackInvoked_(false)
    , localSession_(nullptr)
    , passiveMode_(false)
    , lastError_(ErrorNone)
    , framePullErrors_(0)
    , congestionRecoveryAttempts_(0)
    , congestionDisableTime_(0)
    , congestionPermanentDisabled_(false)
    , refreshTimeoutSource_(0)
    , refreshTimeoutDue_(0)
    , running_(false)
{
    if (peer_) {
        peerAddress_ = QString::fromUtf8(peer_->hostname);
    }
}

DrdQtRdpSession::~DrdQtRdpSession() {
    stopEventThread();
    disableGraphicsPipeline();
    cancelRefreshTimer();
    
    if (vcmThread_) {
        vcmThread_->wait();
        delete vcmThread_;
    }
    
    if (peer_ && peer_->Disconnect) {
        peer_->Disconnect(peer_);
        peer_ = nullptr;
    }
    
    delete localSession_;
    delete graphicsPipeline_;
}

void DrdQtRdpSession::setPeerAddress(const QString &peerAddress) {
    peerAddress_ = !peerAddress.isEmpty() ? peerAddress : "unknown";
}

QString DrdQtRdpSession::peerAddress() const {
    return peerAddress_;
}

void DrdQtRdpSession::setPeerState(const QString &state) {
    state_ = !state.isEmpty() ? state : "unknown";
    qDebug() << "Session" << peerAddress_ << "transitioned to state" << state_;
}

QString DrdQtRdpSession::peerState() const {
    return state_;
}

void DrdQtRdpSession::setRuntime(DrdQtServerRuntime *runtime) {
    if (runtime_ == runtime) return;
    
    runtime_ = runtime;
    maybeInitGraphics();
}

DrdQtServerRuntime *DrdQtRdpSession::runtime() const {
    return runtime_;
}

void DrdQtRdpSession::setVirtualChannelManager(HANDLE vcm) {
    vcm_ = vcm;
    maybeInitGraphics();
}

HANDLE DrdQtRdpSession::virtualChannelManager() const {
    return vcm_;
}

void DrdQtRdpSession::setClosedCallback(std::function<void(DrdQtRdpSession*)> callback) {
    closedCallback_ = callback;
    closedCallbackInvoked_ = false;
    
    if (callback && connectionAlive_ == 0) {
        notifyClosed();
    }
}

void DrdQtRdpSession::setPassiveMode(bool passive) {
    passiveMode_ = passive;
}

bool DrdQtRdpSession::isPassiveMode() const {
    return passiveMode_;
}

void DrdQtRdpSession::attachLocalSession(DrdQtLocalSession *session) {
    if (!session) return;
    
    delete localSession_;
    localSession_ = session;
}

DrdQtLocalSession *DrdQtRdpSession::localSession() const {
    return localSession_;
}

bool DrdQtRdpSession::postConnect() {
    setPeerState("post-connect");
    return true;
}

bool DrdQtRdpSession::activate() {
    if (passiveMode_) {
        setPeerState("activated-passive");
        isActivated_ = true;
        qDebug() << "Session" << peerAddress_ << "running in passive system mode, skipping transport";
        return true;
    }
    
    if (!enforcePeerDesktopSize()) {
        setPeerState("desktop-resize-blocked");
        disconnect("client does not support desktop resize");
        return false;
    }
    
    if (!runtime_) {
        return false;
    }
    
    // TODO: Implement encoding options and stream preparation
    // For now, just mark as activated
    setPeerState("activated");
    isActivated_ = true;
    
    if (!startRenderThread()) {
        qWarning() << "Session" << peerAddress_ << "failed to start renderer thread";
    }
    
    return true;
}

bool DrdQtRdpSession::pump() {
    // TODO: Implement session pumping logic
    return true;
}

void DrdQtRdpSession::disconnect(const QString &reason) {
    if (!reason.isEmpty()) {
        qDebug() << "Disconnecting session" << peerAddress_ << ":" << reason;
    }
    
    stopEventThread();
    disableGraphicsPipeline();
    
    delete localSession_;
    localSession_ = nullptr;
    
    connectionAlive_ = 0;
    renderRunning_ = 0;
    
    if (peer_ && peer_->Disconnect) {
        peer_->Disconnect(peer_);
        peer_ = nullptr;
    }
    
    isActivated_ = false;
    running_ = false;
}

void DrdQtRdpSession::notifyError(Error error) {
    if (error == ErrorNone) return;
    
    lastError_ = error;
    QString reason;
    
    switch (error) {
        case ErrorBadCaps:
            reason = "client reported invalid capabilities";
            break;
        case ErrorBadMonitorData:
            reason = "client monitor layout invalid";
            break;
        case ErrorCloseStackOnDriverFailure:
            reason = "graphics driver requested close";
            break;
        case ErrorGraphicsSubsystemFailed:
            reason = "graphics subsystem failed";
            break;
        case ErrorServerRedirection:
            reason = "server redirection requested";
            break;
        case ErrorNone:
        default:
            reason = "unknown";
            break;
    }
    
    qWarning() << "Session" << peerAddress_ << "reported error:" << reason;
    
    if (error == ErrorServerRedirection) {
        disconnect(reason);
    }
}

bool DrdQtRdpSession::startEventThread() {
    if (!peer_) return false;
    
    if (eventThread_) return true;
    
    if (!stopEvent_) {
        stopEvent_ = CreateEvent(nullptr, true, false, nullptr);
        if (!stopEvent_) {
            qWarning() << "Session" << peerAddress_ << "failed to create stop event";
            return false;
        }
    }
    
    connectionAlive_ = 1;
    
    if (vcm_ && vcm_ != INVALID_HANDLE_VALUE && !vcmThread_) {
        vcmThread_ = QThread::create(vcmThread, this);
        connect(vcmThread_, &QThread::finished, this, &DrdQtRdpSession::onVcmThreadFinished);
        vcmThread_->start();
    }
    
    running_ = true;
    return true;
}

void DrdQtRdpSession::stopEventThread() {
    stopRenderThread();
    connectionAlive_ = 0;
    
    if (stopEvent_) {
        SetEvent(stopEvent_);
    }
    
    if (vcmThread_) {
        vcmThread_->wait();
        delete vcmThread_;
        vcmThread_ = nullptr;
    }
    
    if (stopEvent_) {
        CloseHandle(stopEvent_);
        stopEvent_ = nullptr;
    }
    
    notifyClosed();
    running_ = false;
}

bool DrdQtRdpSession::sendServerRedirection(const QString &routingToken,
                                           const QString &username,
                                           const QString &password,
                                           const QString &certificate) {
    qDebug() << "drd_rdp_session_send_server_redirection";
    
    if (!peer_ || routingToken.isEmpty() || username.isEmpty() || password.isEmpty() || certificate.isEmpty()) {
        return false;
    }
    
    rdpSettings *settings = peer_->context ? peer_->context->settings : nullptr;
    if (!settings) {
        qWarning() << "Session" << peerAddress_ << "missing settings, cannot send redirection";
        return false;
    }
    
    // TODO: Implement full redirection logic
    // For now, just log the attempt
    qDebug() << "Session" << peerAddress_ << "sending server redirection";
    return true;
}

bool DrdQtRdpSession::clientIsMstsc() const {
    if (!peer_ || !peer_->context || !peer_->context->settings) {
        return false;
    }
    
    rdpSettings *settings = peer_->context->settings;
    const quint32 osMajor = freerdp_settings_get_uint32(settings, FreeRDP_OsMajorType);
    const quint32 osMinor = freerdp_settings_get_uint32(settings, FreeRDP_OsMinorType);
    return osMajor == OSMAJORTYPE_WINDOWS && osMinor == OSMINORTYPE_WINDOWS_NT;
}

bool DrdQtRdpSession::getPeerResolution(quint32 *width, quint32 *height) const {
    if (!width || !height) return false;
    
    if (!peer_ || !peer_->context || !peer_->context->settings) {
        return false;
    }
    
    rdpSettings *settings = peer_->context->settings;
    *width = freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth);
    *height = freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight);
    return true;
}

void DrdQtRdpSession::maybeInitGraphics() {
    if (graphicsPipeline_ || !peer_ || !peer_->context || !runtime_ || 
        !vcm_ || vcm_ == INVALID_HANDLE_VALUE) {
        return;
    }
    
    // TODO: Implement graphics pipeline initialization based on encoding options
    // For now, just create a basic pipeline
    graphicsPipeline_ = new DrdQtRdpGraphicsPipeline(peer_, vcm_, runtime_, 1024, 768);
    graphicsPipelineReady_ = false;
    qDebug() << "Session" << peerAddress_ << "graphics pipeline created";
}

void DrdQtRdpSession::disableGraphicsPipeline(const QString &reason) {
    if (!graphicsPipeline_) return;
    
    if (!reason.isEmpty()) {
        qWarning() << "Session" << peerAddress_ << "disabling graphics pipeline:" << reason;
    }
    
    // TODO: Set transport to surface bits
    graphicsPipelineReady_ = false;
    delete graphicsPipeline_;
    graphicsPipeline_ = nullptr;
}

bool DrdQtRdpSession::enforcePeerDesktopSize() {
    if (!peer_ || !peer_->context || !runtime_) {
        return true;
    }
    
    // TODO: Implement desktop size enforcement
    // For now, just return true
    return true;
}

void DrdQtRdpSession::refreshSurfacePayloadLimit() {
    quint32 maxPayload = 0;
    if (peer_ && peer_->context && peer_->context->settings) {
        maxPayload = freerdp_settings_get_uint32(peer_->context->settings, FreeRDP_MultifragMaxRequestSize);
    }
    maxSurfacePayload_ = static_cast<qint32>(maxPayload);
}

bool DrdQtRdpSession::waitForGraphicsCapacity(qint64 /*timeoutUs*/) {
    if (!graphicsPipeline_ || !graphicsPipelineReady_) {
        return false;
    }
    
    // TODO: Implement capacity waiting logic
    return true;
}

bool DrdQtRdpSession::startRenderThread() {
    if (renderThread_) return true;
    
    if (!runtime_) return false;
    
    renderRunning_ = 1;
    renderThread_ = QThread::create(renderThread, this);
    connect(renderThread_, &QThread::finished, this, &DrdQtRdpSession::onRenderThreadFinished);
    renderThread_->start();
    
    return true;
}

void DrdQtRdpSession::stopRenderThread() {
    if (!renderThread_) return;
    
    renderRunning_ = 0;
    renderThread_->wait();
    delete renderThread_;
    renderThread_ = nullptr;
}

void DrdQtRdpSession::notifyClosed() {
    if (!closedCallback_ || closedCallbackInvoked_) return;
    
    closedCallbackInvoked_ = true;
    closedCallback_(this);
}

void DrdQtRdpSession::updateRefreshTimerState() {
    // TODO: Implement refresh timer state management
}

void DrdQtRdpSession::cancelRefreshTimer() {
    // TODO: Implement refresh timer cancellation
}

void *DrdQtRdpSession::vcmThread(void *userData) {
    DrdQtRdpSession *self = static_cast<DrdQtRdpSession*>(userData);
    freerdp_peer *peer = self->peer_;
    HANDLE vcm = self->vcm_;
    HANDLE channelEvent = nullptr;
    
    if (!vcm || vcm == INVALID_HANDLE_VALUE || !peer) {
        return nullptr;
    }
    
    channelEvent = WTSVirtualChannelManagerGetEventHandle(vcm);
    
    while (self->connectionAlive_) {
        HANDLE events[32];
        quint32 peerEventsHandles = 0;
        DWORD nEvents = 0;
        
        if (self->stopEvent_) {
            events[nEvents++] = self->stopEvent_;
        }
        if (channelEvent) {
            events[nEvents++] = channelEvent;
        }
        
        peerEventsHandles = peer->GetEventHandles(peer, &events[nEvents], 32 - nEvents);
        if (!peerEventsHandles) {
            qDebug() << "[RDP] peer_events_handles 0, stopping session";
            self->connectionAlive_ = 0;
            break;
        }
        nEvents += peerEventsHandles;
        
        DWORD status = WAIT_TIMEOUT;
        if (nEvents > 0) {
            status = WaitForMultipleObjects(nEvents, events, false, INFINITE);
        }
        
        if (status == WAIT_FAILED) {
            break;
        }
        
        if (!peer->CheckFileDescriptor(peer)) {
            qDebug() << "[RDP] CheckFileDescriptor error, stopping session";
            self->connectionAlive_ = 0;
            break;
        }
        
        if (!peer->connected) {
            continue;
        }
        
        if (!WTSVirtualChannelManagerIsChannelJoined(vcm, DRDYNVC_SVC_CHANNEL_NAME)) {
            continue;
        }
        
        switch (WTSVirtualChannelManagerGetDrdynvcState(vcm)) {
            case DRDYNVC_STATE_NONE:
                SetEvent(channelEvent);
                break;
            case DRDYNVC_STATE_READY:
                if (self->graphicsPipeline_ && self->connectionAlive_) {
                    // TODO: Implement graphics pipeline initialization
                }
                break;
        }
        
        if (!self->connectionAlive_) {
            break;
        }
        
        if (channelEvent && WaitForSingleObject(channelEvent, 0) == WAIT_OBJECT_0) {
            if (!WTSVirtualChannelManagerCheckFileDescriptor(vcm)) {
                qDebug() << "Session" << self->peerAddress_ << "failed to check VCM descriptor";
                self->connectionAlive_ = 0;
                break;
            }
        }
    }
    
    self->renderRunning_ = 0;
    self->notifyClosed();
    return nullptr;
}

void *DrdQtRdpSession::renderThread(void *userData) {
    DrdQtRdpSession *self = static_cast<DrdQtRdpSession*>(userData);
    
    const quint32 targetFps = 30; // TODO: Replace with actual capture metrics
    const qint64 statsInterval = 1000000; // 1 second in microseconds
    quint32 statsFrames = 0;
    qint64 statsWindowStart = 0;
    
    while (self->renderRunning_) {
        if (!self->connectionAlive_) {
            break;
        }
        
        if (!self->isActivated_ || !self->runtime_) {
            QThread::usleep(1000);
            continue;
        }
        
        // TODO: Implement frame rendering logic
        // For now, just simulate frame processing
        bool sent = false;
        
        if (sent) {
            const qint64 now = QDateTime::currentMSecsSinceEpoch() * 1000;
            if (statsWindowStart == 0) {
                statsWindowStart = now;
            }
            statsFrames++;
            
            const qint64 elapsed = now - statsWindowStart;
            if (elapsed >= statsInterval) {
                const double actualFps = static_cast<double>(statsFrames) * 1000000.0 / static_cast<double>(elapsed);
                const bool reachedTarget = actualFps >= static_cast<double>(targetFps);
                qDebug() << "Session" << self->peerAddress_ << "render fps=" << actualFps
                         << "(target=" << targetFps << "):" << (reachedTarget ? "reached target" : "below target");
                statsFrames = 0;
                statsWindowStart = now;
            }
        }
        
        self->updateRefreshTimerState();
        self->frameSequence_++;
        if (self->frameSequence_ == 0) {
            self->frameSequence_ = 1;
        }
        
        QThread::usleep(1000000 / targetFps); // Sleep to maintain target FPS
    }
    
    return nullptr;
}

bool DrdQtRdpSession::onRefreshTimeout(void *userData) {
    DrdQtRdpSession *self = static_cast<DrdQtRdpSession*>(userData);
    self->refreshTimeoutSource_ = 0;
    
    // TODO: Implement refresh timeout logic
    return false; // Remove source
}

void DrdQtRdpSession::onVcmThreadFinished() {
    // Cleanup VCM thread resources
    vcmThread_->deleteLater();
    vcmThread_ = nullptr;
}

void DrdQtRdpSession::onRenderThreadFinished() {
    // Cleanup render thread resources
    renderThread_->deleteLater();
    renderThread_ = nullptr;
}
