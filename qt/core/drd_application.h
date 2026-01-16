#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class DrdQtApplication : public QObject {
public:
  explicit DrdQtApplication(QObject *parent = nullptr);

  int drd_application_run(const QStringList &arguments, QString *error_message);
};
