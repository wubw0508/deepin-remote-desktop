#include "qt/core/drd_runtime.h"

DrdQtRuntime::DrdQtRuntime(QObject *parent)
    : QObject(parent), module_name_(QStringLiteral("core")) {}

const QString &DrdQtRuntime::module_name() const { return module_name_; }
