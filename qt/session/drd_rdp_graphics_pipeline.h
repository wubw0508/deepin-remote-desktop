#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>

class DrdQtRdpGraphicsPipeline : public QObject {
public:
  explicit DrdQtRdpGraphicsPipeline(QObject *parent = nullptr);

  bool drd_rdp_graphics_pipeline_start(QString *error_message);
  void drd_rdp_graphics_pipeline_stop();
  bool drd_rdp_graphics_pipeline_is_running() const;

private:
  bool running_ = false;
};
