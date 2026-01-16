#pragma once

#include <QObject>
#include <QString>

class DrdQtTransport : public QObject {
public:
  explicit DrdQtTransport(QObject *parent = nullptr);

  const QString &module_name() const;

private:
  QString module_name_;
};
