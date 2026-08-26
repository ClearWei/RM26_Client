#ifndef VIDEODATAGRAMLOGGER_H
#define VIDEODATAGRAMLOGGER_H

#include <QByteArray>
#include <QDateTime>
#include <QFile>
#include <QString>

namespace RM {

class VideoDatagramLogger {
public:
  VideoDatagramLogger() = default;
  ~VideoDatagramLogger();

  bool startSession(quint16 port);
  void stopSession();
  void logDatagram(const QByteArray &datagram, quint16 port);

  QString logFilePath() const { return m_logFilePath; }

  static QByteArray formatEntry(const QByteArray &datagram, quint16 port,
                                const QDateTime &timestamp);

private:
  static QString resolveLogDirectory();

  QFile m_file;
  QString m_logFilePath;
  int m_pendingEntries = 0;
};

} // namespace RM

#endif // VIDEODATAGRAMLOGGER_H
