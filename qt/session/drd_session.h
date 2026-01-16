#pragma once

#include <QObject>
#include <QString>

class DrdQtSession : public QObject {
public:
  explicit DrdQtSession(QObject *parent = nullptr);

  const QString &module_name() const;

private:
  QString module_name_;
};
