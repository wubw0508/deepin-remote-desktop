#include "drd_nla_sam.h"

DrdQtNlaSam::DrdQtNlaSam(QObject *parent) : QObject(parent) {}

bool DrdQtNlaSam::drd_nla_sam_generate(const QString &username,
                                       const QString &nt_hash_hex,
                                       QString *error_message) {
  Q_UNUSED(username);
  Q_UNUSED(nt_hash_hex);
  if (error_message) {
    error_message->clear();
  }
  path_.clear();
  return true;
}

QString DrdQtNlaSam::drd_nla_sam_path() const { return path_; }
