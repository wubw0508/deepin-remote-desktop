#pragma once

#include <QObject>
#include <QString>

class DrdQtLocalSession : public QObject {
public:
  explicit DrdQtLocalSession(QObject *parent = nullptr);

  bool drd_local_session_open(const QString &pam_service,
                              const QString &username, const QString &domain,
                              const QString &password,
                              const QString &remote_host,
                              QString *error_message);
  void drd_local_session_close();

private:
  QString pam_service_;
  QString username_;
  QString domain_;
  QString password_;
  QString remote_host_;
};
