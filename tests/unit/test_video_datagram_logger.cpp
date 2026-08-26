#include <QtTest>

#include "network/VideoDatagramLogger.h"

#include <QJsonDocument>
#include <QJsonObject>

class TestVideoDatagramLogger : public QObject {
  Q_OBJECT

private slots:
  void testEntryContainsTimestampPortSizeAndEightByteHead() {
    const QByteArray datagram = QByteArray::fromHex(
        "1234000200000400deadbeefcafebabefeedface");
    const QDateTime timestamp =
        QDateTime::fromString(QStringLiteral("2026-08-05T10:20:30.456+08:00"),
                              Qt::ISODateWithMs);

    const QJsonObject entry = QJsonDocument::fromJson(
                                  RM::VideoDatagramLogger::formatEntry(
                                      datagram, 3334, timestamp))
                                  .object();

    QCOMPARE(entry.value(QStringLiteral("timestamp")).toString(),
             timestamp.toString(Qt::ISODateWithMs));
    QCOMPARE(entry.value(QStringLiteral("port")).toInt(), 3334);
    QCOMPARE(entry.value(QStringLiteral("bytes")).toInt(), datagram.size());
    QCOMPARE(entry.value(QStringLiteral("head")).toString(),
             QStringLiteral("1234000200000400"));
    QVERIFY(!RM::VideoDatagramLogger::formatEntry(datagram, 3334, timestamp)
                 .contains("deadbeef"));
  }

  void testShortDatagramLogsOnlyAvailableHeadBytes() {
    const QByteArray datagram = QByteArray::fromHex("010203");
    const QJsonObject entry = QJsonDocument::fromJson(
                                  RM::VideoDatagramLogger::formatEntry(
                                      datagram, 3334, QDateTime::currentDateTime()))
                                  .object();

    QCOMPARE(entry.value(QStringLiteral("head")).toString(),
             QStringLiteral("010203"));
    QCOMPARE(entry.value(QStringLiteral("bytes")).toInt(), 3);
  }
};

QTEST_APPLESS_MAIN(TestVideoDatagramLogger)
#include "test_video_datagram_logger.moc"
