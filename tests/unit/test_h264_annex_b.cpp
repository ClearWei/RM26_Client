#include "network/H264AnnexB.h"

#include <QtTest>

class TestH264AnnexB : public QObject {
  Q_OBJECT

private slots:
  void detectsIdrBehindParameterSets() {
    const QByteArray accessUnit = QByteArray::fromHex(
        "000000016764001f00000168ee3c8000000001658899aabb");

    QVERIFY(RM::h264AnnexBContainsNalType(
        reinterpret_cast<const uint8_t *>(accessUnit.constData()),
        accessUnit.size(), 7));
    QVERIFY(RM::h264AnnexBContainsNalType(
        reinterpret_cast<const uint8_t *>(accessUnit.constData()),
        accessUnit.size(), 8));
    QVERIFY(RM::h264AnnexBContainsNalType(
        reinterpret_cast<const uint8_t *>(accessUnit.constData()),
        accessUnit.size(), 5));
    QVERIFY(!RM::h264AnnexBContainsNalType(
        reinterpret_cast<const uint8_t *>(accessUnit.constData()),
        accessUnit.size(), 1));
  }

  void extractsOnlyRequestedParameterSet() {
    const QByteArray accessUnit = QByteArray::fromHex(
        "000000016764001f00000168ee3c80000001658899");

    const QByteArray sps = RM::h264AnnexBExtractNalUnit(
        reinterpret_cast<const uint8_t *>(accessUnit.constData()),
        accessUnit.size(), 7);
    const QByteArray pps = RM::h264AnnexBExtractNalUnit(
        reinterpret_cast<const uint8_t *>(accessUnit.constData()),
        accessUnit.size(), 8);

    QCOMPARE(sps, QByteArray::fromHex("000000016764001f"));
    QCOMPARE(pps, QByteArray::fromHex("00000168ee3c80"));
  }

  void rejectsInvalidInput() {
    QVERIFY(!RM::h264AnnexBContainsNalType(nullptr, 0, 5));
    QCOMPARE(RM::h264AnnexBExtractNalUnit(nullptr, 0, 7), QByteArray());
  }
};

QTEST_MAIN(TestH264AnnexB)
#include "test_h264_annex_b.moc"
