#include "qt/session/drd_session.h"

DrdQtSession::DrdQtSession(QObject *parent)
    : QObject(parent), module_name_(QStringLiteral("session")) {}

const QString &DrdQtSession::module_name() const { return module_name_; }
