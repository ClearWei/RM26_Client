#ifndef LOGFILENAMING_H
#define LOGFILENAMING_H

#include <QDateTime>
#include <QString>

namespace RM {

QString timestampedFileName(
    const QString &fileName,
    const QDateTime &timestamp = QDateTime::currentDateTime());

QString timestampedFilePath(
    const QString &filePath,
    const QDateTime &timestamp = QDateTime::currentDateTime());

} // namespace RM

#endif // LOGFILENAMING_H
