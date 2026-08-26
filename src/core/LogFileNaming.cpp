#include "LogFileNaming.h"

#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>

namespace RM {

QString timestampedFileName(const QString &fileName,
                            const QDateTime &timestamp) {
  static const QRegularExpression existingPrefix(
      QStringLiteral("^\\d{8}-\\d{6}_"));
  if (fileName.isEmpty() || existingPrefix.match(fileName).hasMatch()) {
    return fileName;
  }

  const QDateTime effectiveTimestamp =
      timestamp.isValid() ? timestamp : QDateTime::currentDateTime();
  return QStringLiteral("%1_%2")
      .arg(effectiveTimestamp.toString(QStringLiteral("yyyyMMdd-HHmmss")),
           fileName);
}

QString timestampedFilePath(const QString &filePath,
                            const QDateTime &timestamp) {
  QFileInfo info(filePath);
  const QString stampedName = timestampedFileName(info.fileName(), timestamp);
  if (info.path().isEmpty() || info.path() == QStringLiteral(".")) {
    return stampedName;
  }
  return info.dir().filePath(stampedName);
}

} // namespace RM
