#include "drd_rdp_graphics_pipeline.h"

DrdQtRdpGraphicsPipeline::DrdQtRdpGraphicsPipeline(QObject *parent)
    : QObject(parent) {}

bool DrdQtRdpGraphicsPipeline::drd_rdp_graphics_pipeline_start(
    QString *error_message) {
  if (error_message) {
    error_message->clear();
  }
  running_ = true;
  return true;
}

void DrdQtRdpGraphicsPipeline::drd_rdp_graphics_pipeline_stop() {
  running_ = false;
}

bool DrdQtRdpGraphicsPipeline::drd_rdp_graphics_pipeline_is_running() const {
  return running_;
}
