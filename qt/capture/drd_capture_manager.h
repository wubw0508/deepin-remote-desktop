#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>

class DrdQtCaptureManager : public QObject {
public:
  explicit DrdQtCaptureManager(QObject *parent = nullptr);

  DrdQtCaptureManager *drd_capture_manager_new();
  bool drd_capture_manager_start(quint32 width, quint32 height,
                                 QString *error_message);
  void drd_capture_manager_stop();
  bool drd_capture_manager_is_running() const;
  bool drd_capture_manager_get_display_size(quint32 *out_width,
                                            quint32 *out_height,
                                            QString *error_message) const;
  QObject *drd_capture_manager_get_queue() const;
  bool drd_capture_manager_wait_frame(qint64 timeout_us, QObject **out_frame,
                                      QString *error_message);

private:
  bool running_ = false;
};
