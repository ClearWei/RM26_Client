#include "core/LogFileNaming.h"

#include <QDir>
#include <QtTest>

class TestLogFileNaming : public QObject {
  Q_OBJECT

private slots:
  void testAddsTimestampBeforeFileName() {
    const QDateTime ts(QDate(2026, 5, 28), QTime(22, 5, 1));

    QCOMPARE(RM::timestampedFileName(QStringLiteral("runtime.log"), ts),
             QStringLiteral("20260528-220501_runtime.log"));
  }

  void testKeepsDirectoryAndFileExtension() {
    const QDateTime ts(QDate(2026, 5, 28), QTime(22, 5, 1));
    const QString path =
        QDir::fromNativeSeparators(QStringLiteral("tmp/log/runtime.log"));

    QCOMPARE(RM::timestampedFilePath(path, ts),
             QDir::fromNativeSeparators(
                 QStringLiteral("tmp/log/20260528-220501_runtime.log")));
  }

  void testAlreadyTimestampedNameIsNotDoublePrefixed() {
    const QDateTime ts(QDate(2026, 5, 28), QTime(22, 5, 1));

    QCOMPARE(RM::timestampedFileName(
                 QStringLiteral("20260528-215900_mqtt_rx.log"), ts),
             QStringLiteral("20260528-215900_mqtt_rx.log"));
  }
};

QTEST_MAIN(TestLogFileNaming)
#include "test_log_file_naming.moc"
