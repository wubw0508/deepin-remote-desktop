#include "drd_handover_daemon.h"

#include <QByteArray>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QRandomGenerator>
#include <QTcpSocket>
#include <QVariant>

#include "core/drd_dbus_constants.h"
#include "security/drd_tls_credentials.h"
#include "transport/drd_transport.h"

namespace {

constexpr char kDispatcherInterface[] = "org.deepin.RemoteDesktop.Rdp.Dispatcher";
constexpr char kHandoverInterface[] = "org.deepin.RemoteDesktop.Rdp.Handover";
constexpr char kDispatcherObjectPath[] =
    "/org/deepin/RemoteDesktop/Rdp/Dispatcher";

QString generate_token(const QString &prefix, int random_bytes) {
  QByteArray buffer;
  buffer.resize(random_bytes);
  for (int i = 0; i < random_bytes; ++i) {
    buffer[i] = static_cast<char>(QRandomGenerator::global()->generate() & 0xFF);
  }
  return prefix + QString::fromLatin1(buffer.toHex());
}

QString read_string_setting(QObject *config, const QString &key,
                            const QString &fallback) {
  if (!config) {
    return fallback;
  }
  const QVariant property_value = config->property(key.toUtf8().constData());
  if (property_value.isValid()) {
    const QString value = property_value.toString();
    if (!value.isEmpty()) {
      return value;
    }
  }
  return fallback;
}

int read_int_setting(QObject *config, const QString &key, int fallback) {
  if (!config) {
    return fallback;
  }
  const QVariant property_value = config->property(key.toUtf8().constData());
  if (property_value.isValid()) {
    bool ok = false;
    const int value = property_value.toInt(&ok);
    if (ok) {
      return value;
    }
  }
  return fallback;
}

bool read_bool_setting(QObject *config, const QString &key, bool fallback) {
  if (!config) {
    return fallback;
  }
  const QVariant property_value = config->property(key.toUtf8().constData());
  if (property_value.isValid()) {
    return property_value.toBool();
  }
  return fallback;
}

} // namespace

DrdQtHandoverDaemon::DrdQtHandoverDaemon(QObject *config, QObject *runtime,
                                         QObject *tls_credentials,
                                         QObject *parent)
    : QObject(parent), config_(config), runtime_(runtime),
      tls_credentials_(tls_credentials),
      transport_(new DrdQtTransport(this)) {}

bool DrdQtHandoverDaemon::start(QString *error_message) {
  if (running_) {
    if (error_message) {
      error_message->clear();
    }
    return true;
  }
  if (!config_ || !runtime_) {
    if (error_message) {
      *error_message =
          QStringLiteral("Handover daemon missing required dependencies");
    }
    return false;
  }

  if ((nla_username_.isEmpty() || nla_password_.isEmpty()) &&
      !refresh_nla_credentials(error_message)) {
    return false;
  }

  if (!dispatcher_proxy_) {
    dispatcher_proxy_ = new QDBusInterface(
        DrdQtDbusConstants::drd_dbus_remote_desktop_name(),
        QString::fromLatin1(kDispatcherObjectPath),
        QString::fromLatin1(kDispatcherInterface),
        QDBusConnection::systemBus(), this);
  }
  if (!dispatcher_proxy_ || !dispatcher_proxy_->isValid()) {
    if (error_message) {
      *error_message = QStringLiteral("Failed to connect to dispatcher");
    }
    return false;
  }

  if (!listener_) {
    const QString bind_address = read_string_setting(
        config_, QStringLiteral("server.bind_address"),
        QStringLiteral("0.0.0.0"));
    const int port_value = read_int_setting(config_,
                                            QStringLiteral("server.port"), 3390);
    const bool nla_enabled =
        read_bool_setting(config_, QStringLiteral("auth.enable_nla"), true);
    const QString pam_service = read_string_setting(
        config_, QStringLiteral("auth.pam_service"),
        QStringLiteral("deepin-remote-desktop"));
    const QVariantMap encoding_options;

    listener_ = transport_->drd_rdp_listener_new(
        bind_address, static_cast<quint16>(port_value), runtime_,
        encoding_options, nla_enabled, nla_username_, nla_password_,
        pam_service, QStringLiteral("handover"));
    if (!listener_) {
      if (error_message) {
        *error_message =
            QStringLiteral("Failed to create handover listener");
      }
      return false;
    }

    transport_->drd_rdp_listener_set_session_callback(
        listener_,
        [this](QObject *, QObject *session, QIODevice *, QObject *) {
          active_session_ = session;
        },
        this);
  }

  QDBusReply<QDBusObjectPath> handover_reply =
      dispatcher_proxy_->call(QStringLiteral("RequestHandover"));
  if (!handover_reply.isValid()) {
    if (error_message) {
      *error_message = handover_reply.error().message();
    }
    return false;
  }
  handover_object_path_ = handover_reply.value().path();
  handover_proxy_ = new QDBusInterface(
      DrdQtDbusConstants::drd_dbus_remote_desktop_name(), handover_object_path_,
      QString::fromLatin1(kHandoverInterface), QDBusConnection::systemBus(),
      this);
  if (!handover_proxy_ || !handover_proxy_->isValid()) {
    if (error_message) {
      *error_message = QStringLiteral("Failed to bind handover object");
    }
    return false;
  }

  QDBusMessage start_reply = handover_proxy_->call(
      QStringLiteral("StartHandover"), nla_username_, nla_password_);
  if (start_reply.type() == QDBusMessage::ErrorMessage) {
    if (error_message) {
      *error_message = start_reply.errorMessage();
    }
    return false;
  }
  const QList<QVariant> start_args = start_reply.arguments();
  if (start_args.size() < 2) {
    if (error_message) {
      *error_message =
          QStringLiteral("Dispatcher did not provide TLS material");
    }
    return false;
  }
  const QString certificate_pem = start_args.at(0).toString();
  const QString key_pem = start_args.at(1).toString();
  if (!tls_credentials_) {
    tls_credentials_ = new DrdQtTlsCredentials(this);
  }
  auto *credentials = qobject_cast<DrdQtTlsCredentials *>(tls_credentials_);
  if (!credentials) {
    if (error_message) {
      *error_message =
          QStringLiteral("TLS credentials unavailable for handover listener");
    }
    return false;
  }
  if (!credentials->drd_tls_credentials_reload_from_pem(certificate_pem,
                                                        key_pem,
                                                        error_message)) {
    return false;
  }

  if (error_message) {
    error_message->clear();
  }
  running_ = true;
  return true;
}

void DrdQtHandoverDaemon::stop() {
  if (!running_) {
    return;
  }
  dispatcher_proxy_.clear();
  handover_proxy_.clear();
  handover_object_path_.clear();
  listener_.clear();
  active_session_.clear();
  if (main_loop_ && main_loop_->isRunning()) {
    main_loop_->quit();
  }
  running_ = false;
}

bool DrdQtHandoverDaemon::set_main_loop(QEventLoop *loop) {
  main_loop_ = loop;
  return true;
}

QObject *DrdQtHandoverDaemon::config() const { return config_; }

QObject *DrdQtHandoverDaemon::runtime() const { return runtime_; }

QObject *DrdQtHandoverDaemon::tls_credentials() const {
  return tls_credentials_;
}

bool DrdQtHandoverDaemon::running() const { return running_; }

void DrdQtHandoverDaemon::handle_redirect_client(const QString &routing_token,
                                                 const QString &username,
                                                 const QString &password) {
  if (!redirect_active_client(routing_token, username, password)) {
    return;
  }
  stop();
  request_shutdown();
}

void DrdQtHandoverDaemon::handle_take_client_ready(
    bool use_system_credentials) {
  Q_UNUSED(use_system_credentials);
  QString error_message;
  take_client(&error_message);
}

void DrdQtHandoverDaemon::handle_restart_handover() {}

bool DrdQtHandoverDaemon::take_client(QString *error_message) {
  if (!handover_proxy_ || !listener_) {
    if (error_message) {
      *error_message =
          QStringLiteral("Handover proxy or listener is not ready");
    }
    return false;
  }
  QDBusMessage reply =
      handover_proxy_->call(QStringLiteral("TakeClient"));
  if (reply.type() == QDBusMessage::ErrorMessage) {
    if (error_message) {
      *error_message = reply.errorMessage();
    }
    return false;
  }
  const QList<QVariant> args = reply.arguments();
  if (args.isEmpty()) {
    if (error_message) {
      *error_message = QStringLiteral("TakeClient did not return a descriptor");
    }
    return false;
  }
  const QDBusUnixFileDescriptor fd =
      qvariant_cast<QDBusUnixFileDescriptor>(args.at(0));
  if (!fd.isValid()) {
    if (error_message) {
      *error_message = QStringLiteral("Invalid file descriptor from TakeClient");
    }
    return false;
  }
  auto *socket = new QTcpSocket(this);
  if (!socket->setSocketDescriptor(fd.takeFileDescriptor())) {
    if (error_message) {
      *error_message =
          QStringLiteral("Failed to adopt handover socket descriptor");
    }
    socket->deleteLater();
    return false;
  }
  if (!transport_->drd_rdp_listener_adopt_connection(listener_, socket,
                                                     error_message)) {
    socket->deleteLater();
    return false;
  }
  return true;
}

bool DrdQtHandoverDaemon::redirect_active_client(
    const QString &routing_token, const QString &username,
    const QString &password) {
  if (!active_session_) {
    return false;
  }
  if (routing_token.isEmpty() || username.isEmpty() || password.isEmpty()) {
    return false;
  }
  if (!tls_credentials_) {
    return false;
  }
  active_session_.clear();
  return true;
}

void DrdQtHandoverDaemon::request_shutdown() {
  if (main_loop_ && main_loop_->isRunning()) {
    main_loop_->quit();
  }
}

void DrdQtHandoverDaemon::clear_nla_credentials() {
  nla_username_.clear();
  nla_password_.clear();
}

bool DrdQtHandoverDaemon::refresh_nla_credentials(QString *error_message) {
  clear_nla_credentials();
  nla_username_ = generate_token(QStringLiteral("handover-"), 8);
  nla_password_ = generate_token(QStringLiteral("handover-"), 16);
  if (nla_username_.isEmpty() || nla_password_.isEmpty()) {
    if (error_message) {
      *error_message =
          QStringLiteral("Failed to generate NLA credentials for handover");
    }
    clear_nla_credentials();
    return false;
  }
  return true;
}
