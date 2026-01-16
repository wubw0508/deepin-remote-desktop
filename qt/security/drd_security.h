#pragma once

#include <QObject>
#include <QString>

class DrdQtSecurity : public QObject {
public:
  explicit DrdQtSecurity(QObject *parent = nullptr);

  const QString &module_name() const;

private:
  QString module_name_;
};
