#include "drd_dbus_constants.h"

namespace DrdQtDbusConstants {

QString drd_dbus_remote_desktop_name() {
  return QStringLiteral("org.deepin.RemoteDesktop");
}

QString drd_dbus_remote_desktop_path() {
  return QStringLiteral("/org/deepin/RemoteDesktop");
}

QString drd_dbus_lightdm_name() {
  return QStringLiteral("org.deepin.DisplayManager");
}

QString drd_dbus_lightdm_path() {
  return QStringLiteral("/org/deepin/DisplayManager/RemoteDisplayFactory");
}

} // namespace DrdQtDbusConstants
