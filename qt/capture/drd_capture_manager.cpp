#include "drd_capture_manager.h"

DrdQtCaptureManager::DrdQtCaptureManager(QObject *parent) : QObject(parent) {}

DrdQtCaptureManager *DrdQtCaptureManager::drd_capture_manager_new() {
  return new DrdQtCaptureManager();
}

bool DrdQtCaptureManager::drd_capture_manager_start(quint32 width,
                                                    quint32 height,
                                                    QString *error_message) {
  Q_UNUSED(width);
  Q_UNUSED(height);
  if (error_message) {
    error_message->clear();
  }
  running_ = true;
  return true;
}

void DrdQtCaptureManager::drd_capture_manager_stop() { running_ = false; }

bool DrdQtCaptureManager::drd_capture_manager_is_running() const {
  return running_;
}

bool DrdQtCaptureManager::drd_capture_manager_get_display_size(
    quint32 *out_width, quint32 *out_height, QString *error_message) const {
  if (out_width) {
    *out_width = 0;
  }
  if (out_height) {
    *out_height = 0;
  }
  if (error_message) {
    error_message->clear();
  }
  return false;
}

QObject *DrdQtCaptureManager::drd_capture_manager_get_queue() const {
  return nullptr;
}

bool DrdQtCaptureManager::drd_capture_manager_wait_frame(
    qint64 timeout_us, QObject **out_frame, QString *error_message) {
  Q_UNUSED(timeout_us);
  if (out_frame) {
    *out_frame = nullptr;
  }
  if (error_message) {
    error_message->clear();
  }
  return false;
}
