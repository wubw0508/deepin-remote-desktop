#include "drd_rdp_graphics_pipeline.h"
#include "drd_rdp_graphics_pipeline.moc"

#include <QDateTime>
#include <QDebug>

#include <freerdp/freerdp.h>
#include <freerdp/peer.h>
#include <freerdp/server/rdpgfx.h>
#include <freerdp/channels/rdpgfx.h>
#include <freerdp/codec/color.h>

#include "core/drd_server_runtime.h"
#include <QDebug>

DrdQtRdpGraphicsPipeline::DrdQtRdpGraphicsPipeline(freerdp_peer *peer,
                                                   HANDLE vcm,
                                                   DrdQtServerRuntime *runtime,
                                                   quint16 surfaceWidth,
                                                   quint16 surfaceHeight,
                                                   QObject *parent)
    : QObject(parent)
    , peer_(peer)
    , width_(surfaceWidth)
    , height_(surfaceHeight)
    , rdpgfxContext_(nullptr)
    , channelOpened_(false)
    , capsConfirmed_(false)
    , surfaceReady_(false)
    , surfaceId_(1)
    , codecContextId_(1)
    , nextFrameId_(1)
    , outstandingFrames_(0)
    , maxOutstandingFrames_(3)
    , channelId_(0)
    , frameAcksSuspended_(false)
    , runtime_(runtime)
    , lastFrameH264_(false)
{
    if (!peer_ || !peer_->context || !vcm || vcm == INVALID_HANDLE_VALUE) {
        qWarning() << "Invalid parameters for graphics pipeline";
        return;
    }

    rdpgfxContext_ = rdpgfx_server_context_new(vcm);
    if (!rdpgfxContext_) {
        qWarning() << "Failed to allocate Rdpgfx server context";
        return;
    }

    rdpgfxContext_->rdpcontext = peer_->context;
    rdpgfxContext_->custom = this;
    rdpgfxContext_->ChannelIdAssigned = [](RdpgfxServerContext *context, UINT32 channelId) -> BOOL {
        auto self = static_cast<DrdQtRdpGraphicsPipeline*>(context->custom);
        if (self) {
            QMutexLocker locker(&self->lock_);
            self->channelId_ = channelId;
        }
        return TRUE;
    };

    rdpgfxContext_->FrameAcknowledge = [](RdpgfxServerContext *context, const RDPGFX_FRAME_ACKNOWLEDGE_PDU *ack) -> UINT {
        auto self = static_cast<DrdQtRdpGraphicsPipeline*>(context->custom);
        if (!self || !ack) return CHANNEL_RC_OK;
        
        QMutexLocker locker(&self->lock_);
        if (ack->queueDepth == SUSPEND_FRAME_ACKNOWLEDGEMENT) {
            if (!self->frameAcksSuspended_) {
                qDebug() << "RDPGFX client suspended frame acknowledgements";
            }
            self->frameAcksSuspended_ = true;
            self->outstandingFrames_ = 0;
            self->capacityCond_.wakeAll();
            return CHANNEL_RC_OK;
        }

        if (self->frameAcksSuspended_) {
            qDebug() << "RDPGFX client resumed frame acknowledgements";
        }
        self->frameAcksSuspended_ = false;
        
        if (self->outstandingFrames_ > 0) {
            if (self->lastFrameH264_) {
                self->outstandingFrames_ = 0;
            } else {
                self->outstandingFrames_--;
            }
        }
        self->capacityCond_.wakeAll();
        
        return CHANNEL_RC_OK;
    };

    // TODO: Implement CapsAdvertise callback
}

DrdQtRdpGraphicsPipeline::~DrdQtRdpGraphicsPipeline() {
    if (rdpgfxContext_) {
        if (surfaceReady_ && rdpgfxContext_->DeleteSurface) {
            RDPGFX_DELETE_SURFACE_PDU del = {0};
            del.surfaceId = surfaceId_;
            rdpgfxContext_->DeleteSurface(rdpgfxContext_, &del);
            surfaceReady_ = false;
            capacityCond_.wakeAll();
        }

        if (channelOpened_ && rdpgfxContext_->Close) {
            rdpgfxContext_->Close(rdpgfxContext_);
            channelOpened_ = false;
        }

        rdpgfx_server_context_free(rdpgfxContext_);
    }
}

quint32 DrdQtRdpGraphicsPipeline::buildTimestamp() {
    quint32 timestamp = 0;
    QDateTime now = QDateTime::currentDateTime();
    
    timestamp = (static_cast<quint32>(now.time().hour()) << 22) |
                (static_cast<quint32>(now.time().minute()) << 16) |
                (static_cast<quint32>(now.time().second()) << 10) |
                (static_cast<quint32>(now.time().msec()));
    
    return timestamp;
}

bool DrdQtRdpGraphicsPipeline::resetLocked() {
    if (!rdpgfxContext_) return false;

    if (surfaceReady_) return true;

    RDPGFX_RESET_GRAPHICS_PDU reset;
    memset(&reset, 0, sizeof(reset));
    reset.width = width_;
    reset.height = height_;
    reset.monitorCount = 0;
    reset.monitorDefArray = nullptr;

    if (!rdpgfxContext_->ResetGraphics ||
        rdpgfxContext_->ResetGraphics(rdpgfxContext_, &reset) != CHANNEL_RC_OK) {
        qWarning() << "Graphics pipeline failed to reset graphics";
        return false;
    }
RDPGFX_CREATE_SURFACE_PDU create;
memset(&create, 0, sizeof(create));
create.surfaceId = surfaceId_;
create.width = width_;
create.height = height_;
create.pixelFormat = GFX_PIXEL_FORMAT_XRGB_8888;


    if (!rdpgfxContext_->CreateSurface ||
        rdpgfxContext_->CreateSurface(rdpgfxContext_, &create) != CHANNEL_RC_OK) {
        qWarning() << "Graphics pipeline failed to create surface" << surfaceId_;
        return false;
    }

    RDPGFX_MAP_SURFACE_TO_OUTPUT_PDU map;
    memset(&map, 0, sizeof(map));
    map.surfaceId = surfaceId_;
    map.outputOriginX = 0;
    map.outputOriginY = 0;
    map.reserved = 0;
    if (!rdpgfxContext_->MapSurfaceToOutput ||
        rdpgfxContext_->MapSurfaceToOutput(rdpgfxContext_, &map) != CHANNEL_RC_OK) {
        qWarning() << "Graphics pipeline failed to map surface" << surfaceId_ << "to output";
        return false;
    }

    nextFrameId_ = 1;
    outstandingFrames_ = 0;
    surfaceReady_ = true;
    lastFrameH264_ = false;
    frameAcksSuspended_ = false;
    capacityCond_.wakeAll();
    return true;
}

bool DrdQtRdpGraphicsPipeline::maybeInit() {
    if (!rdpgfxContext_) return false;

    QMutexLocker locker(&lock_);

    if (!channelOpened_) {
        locker.unlock();
        
        if (!rdpgfxContext_->Open || !rdpgfxContext_->Open(rdpgfxContext_)) {
            qWarning() << "Failed to open Rdpgfx channel";
            return false;
        }
        
        locker.relock();
        channelOpened_ = true;
    }

    if (!capsConfirmed_) {
        return false;
    }

    bool ok = resetLocked();
    // TODO: Set transport mode
    return ok;
}

bool DrdQtRdpGraphicsPipeline::isReady() const {
    QMutexLocker locker(&lock_);
    return surfaceReady_;
}

bool DrdQtRdpGraphicsPipeline::canSubmit() const {
    QMutexLocker locker(&lock_);
    return surfaceReady_ && 
           (frameAcksSuspended_ || 
            outstandingFrames_ < static_cast<qint32>(maxOutstandingFrames_) ||
            lastFrameH264_);
}

bool DrdQtRdpGraphicsPipeline::waitForCapacity(qint64 timeoutUs) {
    qint64 deadline = 0;
    if (timeoutUs > 0) {
        deadline = QDateTime::currentMSecsSinceEpoch() * 1000 + timeoutUs;
    }

    QMutexLocker locker(&lock_);
    if (lastFrameH264_) {
        return true;
    }

    while (surfaceReady_ && !frameAcksSuspended_ &&
           outstandingFrames_ >= static_cast<qint32>(maxOutstandingFrames_)) {
        if (timeoutUs < 0) {
            capacityCond_.wait(&lock_);
        } else {
            if (!capacityCond_.wait(&lock_, deadline / 1000)) {
                break;
            }
        }
    }

    bool ready = surfaceReady_ &&
                 (frameAcksSuspended_ ||
                  outstandingFrames_ < static_cast<qint32>(maxOutstandingFrames_));
    return ready;
}

quint16 DrdQtRdpGraphicsPipeline::getSurfaceId() const {
    return surfaceId_;
}

void DrdQtRdpGraphicsPipeline::outFrameChange(bool add) {
    QMutexLocker locker(&lock_);
    if (add) {
        if (!frameAcksSuspended_) {
            outstandingFrames_++;
        }
    } else {
        if (!frameAcksSuspended_ && outstandingFrames_ > 0) {
            outstandingFrames_--;
        }
        capacityCond_.wakeAll();
    }
}

RdpgfxServerContext *DrdQtRdpGraphicsPipeline::getContext() {
    return rdpgfxContext_;
}

void DrdQtRdpGraphicsPipeline::setLastFrameMode(bool h264) {
    QMutexLocker locker(&lock_);
    lastFrameH264_ = h264;
}
