#pragma once

#include <QObject>
#include <QString>

class DrdQtNlaSam : public QObject {
public:
  explicit DrdQtNlaSam(QObject *parent = nullptr);

  bool drd_nla_sam_generate(const QString &username, const QString &nt_hash_hex,
                            QString *error_message);
  QString drd_nla_sam_path() const;

private:
  QString path_;
};
