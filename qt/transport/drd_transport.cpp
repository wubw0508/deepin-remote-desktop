#include "qt/transport/drd_transport.h"

DrdQtTransport::DrdQtTransport(QObject *parent)
    : QObject(parent), module_name_(QStringLiteral("transport")) {}

const QString &DrdQtTransport::module_name() const { return module_name_; }
