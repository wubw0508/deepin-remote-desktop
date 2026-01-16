#pragma once

#include <QObject>
#include <QString>

class DrdQtSystem : public QObject {
public:
  explicit DrdQtSystem(QObject *parent = nullptr);

  const QString &module_name() const;

private:
  QString module_name_;
};
