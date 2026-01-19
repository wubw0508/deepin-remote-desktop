#include "core/drd_application.h"
#include "core/drd_config.h"

#include <QCoreApplication>
#include <QDebug>
#include <QStringList>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    
    // 测试 DrdQtApplication 类
    qDebug() << "Testing DrdQtApplication class...";
    
    DrdQtApplication qt_app;
    
    // 测试默认配置
    QSharedPointer<DrdQtConfig> config(new DrdQtConfig());
    qDebug() << "Default bind address:" << config->drd_config_get_string("bind_address");
    qDebug() << "Default port:" << config->drd_config_get_int("port");
    qDebug() << "Default encoding mode:" << config->drd_config_get_string("encoding_mode");
    qDebug() << "Default NLA enabled:" << config->drd_config_get_bool("nla_enabled");
    qDebug() << "Default runtime mode:" << config->drd_config_get_string("runtime_mode");
    
    // 测试命令行参数合并
    QVariantMap cli_values;
    cli_values["bind_address"] = "127.0.0.1";
    cli_values["port"] = 3389;
    cli_values["encoding_mode"] = "h264";
    cli_values["nla_enabled"] = false;
    cli_values["runtime_mode"] = "system";
    
    QString error_message;
    if (config->drd_config_merge_cli(cli_values, &error_message)) {
        qDebug() << "CLI merge successful";
        qDebug() << "New bind address:" << config->drd_config_get_string("bind_address");
        qDebug() << "New port:" << config->drd_config_get_int("port");
        qDebug() << "New encoding mode:" << config->drd_config_get_string("encoding_mode");
        qDebug() << "New NLA enabled:" << config->drd_config_get_bool("nla_enabled");
        qDebug() << "New runtime mode:" << config->drd_config_get_string("runtime_mode");
    } else {
        qDebug() << "CLI merge failed:" << error_message;
    }
    
    qDebug() << "Test completed successfully!";
    return 0;
}