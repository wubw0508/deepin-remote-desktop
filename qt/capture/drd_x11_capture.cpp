#include "drd_x11_capture.h"

DrdQtX11Capture::DrdQtX11Capture(QObject *queue, QObject *parent)
    : QObject(parent), queue_(queue) {}

DrdQtX11Capture *DrdQtX11Capture::drd_x11_capture_new(QObject *queue) {
  return new DrdQtX11Capture(queue);
}

bool DrdQtX11Capture::drd_x11_capture_start(const QString &display_name,
                                            quint32 requested_width,
                                            quint32 requested_height,
                                            QString *error_message) {
  Q_UNUSED(display_name);
  Q_UNUSED(requested_width);
  Q_UNUSED(requested_height);
  if (error_message) {
    error_message->clear();
  }
  running_ = true;
  return true;
}

void DrdQtX11Capture::drd_x11_capture_stop() { running_ = false; }

bool DrdQtX11Capture::drd_x11_capture_is_running() const { return running_; }

bool DrdQtX11Capture::drd_x11_capture_get_display_size(
    const QString &display_name, quint32 *out_width, quint32 *out_height,
    QString *error_message) const {
  Q_UNUSED(display_name);
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
