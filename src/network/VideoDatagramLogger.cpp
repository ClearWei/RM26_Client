#include "VideoDatagramLogger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

namespace RM {
namespace {
constexpr int VIDEO_HEAD_SIZE = 8;
constexpr int FLUSH_ENTRY_COUNT = 200;
}

VideoDatagramLogger::~VideoDatagramLogger() { stopSession(); }

QString VideoDatagramLogger::resolveLogDirectory() {
  const QString configured = qEnvironmentVariable("RM_LOG_DIR").trimmed();
  if (!configured.isEmpty()) {
    return QDir(configured).absolutePath();
  }

  QDir dir(QCoreApplication::applicationDirPath());
  for (int i = 0; i < 8; ++i) {
    if (QFileInfo::exists(dir.filePath(QStringLiteral("config.json"))) &&
        QFileInfo(dir.filePath(QStringLiteral("src"))).isDir()) {
      return dir.filePath(QStringLiteral("log"));
    }
    if (!dir.cdUp()) {
      break;
    }
  }
  return QDir(QCoreApplication::applicationDirPath())
      .filePath(QStringLiteral("log"));
}

bool VideoDatagramLogger::startSession(quint16 port) {
  stopSession();

  const QString logDirectory = resolveLogDirectory();
  if (!QDir().mkpath(logDirectory)) {
    qWarning() << "VideoDatagramLogger: failed to create log directory"
               << logDirectory;
    return false;
  }

  const QString timestamp =
      QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
  m_logFilePath = QDir(logDirectory).filePath(
      QStringLiteral("%1_udp%2_video_rx.log").arg(timestamp).arg(port));
  m_file.setFileName(m_logFilePath);
  if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
    qWarning() << "VideoDatagramLogger: failed to open log file"
               << m_logFilePath;
    m_logFilePath.clear();
    return false;
  }

  qInfo() << "VideoDatagramLogger: logging UDP video headers to"
          << m_logFilePath;
  return true;
}

void VideoDatagramLogger::stopSession() {
  if (m_file.isOpen()) {
    m_file.flush();
    m_file.close();
  }
  m_pendingEntries = 0;
}

QByteArray VideoDatagramLogger::formatEntry(const QByteArray &datagram,
                                            quint16 port,
                                            const QDateTime &timestamp) {
  QJsonObject entry;
  entry.insert(QStringLiteral("timestamp"),
               timestamp.toString(Qt::ISODateWithMs));
  entry.insert(QStringLiteral("port"), static_cast<int>(port));
  entry.insert(QStringLiteral("bytes"), datagram.size());
  entry.insert(QStringLiteral("head"),
               QString::fromLatin1(datagram.left(VIDEO_HEAD_SIZE).toHex()));
  return QJsonDocument(entry).toJson(QJsonDocument::Compact) + '\n';
}

void VideoDatagramLogger::logDatagram(const QByteArray &datagram, quint16 port) {
  if (!m_file.isOpen()) {
    return;
  }

  m_file.write(formatEntry(datagram, port, QDateTime::currentDateTime()));
  if (++m_pendingEntries >= FLUSH_ENTRY_COUNT) {
    m_file.flush();
    m_pendingEntries = 0;
  }
}

} // namespace RM
