#include "qt/system/drd_system.h"

DrdQtSystem::DrdQtSystem(QObject *parent)
    : QObject(parent), module_name_(QStringLiteral("system")) {}

const QString &DrdQtSystem::module_name() const { return module_name_; }
