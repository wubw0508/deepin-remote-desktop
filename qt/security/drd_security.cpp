#include "qt/security/drd_security.h"

DrdQtSecurity::DrdQtSecurity(QObject *parent)
    : QObject(parent), module_name_(QStringLiteral("security")) {}

const QString &DrdQtSecurity::module_name() const { return module_name_; }
