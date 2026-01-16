#pragma once

#include <QObject>
#include <QString>

class DrdQtRuntime : public QObject {
public:
  explicit DrdQtRuntime(QObject *parent = nullptr);

  const QString &module_name() const;

private:
  QString module_name_;
};
