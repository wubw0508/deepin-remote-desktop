#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>

class DrdQtX11Capture : public QObject {
public:
  explicit DrdQtX11Capture(QObject *queue, QObject *parent = nullptr);

  DrdQtX11Capture *drd_x11_capture_new(QObject *queue);
  bool drd_x11_capture_start(const QString &display_name,
                             quint32 requested_width, quint32 requested_height,
                             QString *error_message);
  void drd_x11_capture_stop();
  bool drd_x11_capture_is_running() const;
  bool drd_x11_capture_get_display_size(const QString &display_name,
                                        quint32 *out_width, quint32 *out_height,
                                        QString *error_message) const;

private:
  QObject *queue_ = nullptr;
  bool running_ = false;
};
