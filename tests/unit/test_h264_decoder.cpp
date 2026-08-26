#include "network/H264Decoder.h"
#include "network/H264AnnexB.h"

#include <QSignalSpy>
#include <QtTest>

namespace {

QByteArray threeFrameH264Fixture() {
  // 固定样例由 16x16 红色视频源通过 x264 生成，包含三帧 IDR 和
  // Annex-B 起始码；第一个访问单元同时包含 SPS/PPS/SEI/IDR。
  return QByteArray::fromHex(
      "000000016742c00addec044000000300400000030123c489e00000000168ce0fc8"
      "0000010605ffff4ddc45e9bde6d948b7962cd820d923eeef78323634202d20636f"
      "7265203136352072333232322062333536303561202d20482e3236342f4d504547"
      "2d342041564320636f646563202d20436f70796c65667420323030332d32303235"
      "202d20687474703a2f2f7777772e766964656f6c616e2e6f72672f783236342e68"
      "746d6c202d206f7074696f6e733a2063616261633d30207265663d31206465626c"
      "6f636b3d303a303a3020616e616c7973653d303a30206d653d646961207375626d"
      "653d30207073793d31207073795f72643d312e30303a302e3030206d697865645f"
      "7265663d30206d655f72616e67653d3136206368726f6d615f6d653d3120747265"
      "6c6c69733d30203878386463743d302063716d3d3020646561647a6f6e653d3231"
      "2c313120666173745f70736b69703d31206368726f6d615f71705f6f6666736574"
      "3d3020746872656164733d31206c6f6f6b61686561645f746872656164733d3120"
      "736c696365645f746872656164733d30206e723d3020646563696d6174653d3120"
      "696e7465726c616365643d3020626c757261795f636f6d7061743d3020636f6e73"
      "747261696e65645f696e7472613d3020626672616d65733d302077656967687470"
      "3d30206b6579696e743d31206b6579696e745f6d696e3d31207363656e65637574"
      "3d3020696e7472615f726566726573683d302072633d637266206d62747265653d"
      "30206372663d32332e302071636f6d703d302e36302071706d696e3d302071706d"
      "61783d3639207170737465703d342069705f726174696f3d312e34302061713d30"
      "00800000016588843a118a000218f1c00040f63800087960000000016742c00add"
      "ec044000000300400000030123c489e00000000168ce0fc8000001658882010a11"
      "8a000284b1c000465638000a7860000000016742c00addec044000000300400000"
      "030123c489e00000000168ce0fc800000165888404284628000a12c700011958e0"
      "0029e180");
}

QImage decodeFragmented(RM::H264Decoder &decoder, const QByteArray &stream) {
  QImage latest;
  constexpr int kFragmentSize = 73;
  for (int offset = 0; offset < stream.size(); offset += kFragmentSize) {
    const QImage decoded = decoder.decode(stream.mid(offset, kFragmentSize));
    if (!decoded.isNull()) {
      latest = decoded;
    }
  }
  return latest;
}

} // namespace

class TestH264Decoder : public QObject {
  Q_OBJECT

private slots:
  void decodesMultiNalAccessUnitFromSmallFragments() {
    RM::H264Decoder decoder;
    QVERIFY(decoder.initialize());
    QSignalSpy frameSpy(&decoder, &RM::H264Decoder::frameDecoded);

    const QImage image = decodeFragmented(decoder, threeFrameH264Fixture());

    QVERIFY(!image.isNull());
    QCOMPARE(image.size(), QSize(16, 16));
    QVERIFY(frameSpy.count() >= 1);
  }

  void resetRecoversOnNextMultiNalIdr() {
    RM::H264Decoder decoder;
    QVERIFY(decoder.initialize());
    QVERIFY(!decodeFragmented(decoder, threeFrameH264Fixture()).isNull());

    decoder.resetParserAndCodec();
    const QImage recovered =
        decodeFragmented(decoder, threeFrameH264Fixture());

    QVERIFY(decoder.isInitialized());
    QVERIFY(!recovered.isNull());
    QCOMPARE(recovered.size(), QSize(16, 16));
  }

  void resetUsesCachedParameterSetsForBareIdr() {
    RM::H264Decoder decoder;
    QVERIFY(decoder.initialize());
    const QByteArray fixture = threeFrameH264Fixture();
    QVERIFY(!decodeFragmented(decoder, fixture).isNull());
    const QByteArray idr = RM::h264AnnexBExtractNalUnit(
        reinterpret_cast<const uint8_t *>(fixture.constData()), fixture.size(),
        5);
    QVERIFY(!idr.isEmpty());

    decoder.resetParserAndCodec();
    // 此处不重复提供 SPS/PPS。恢复依赖 reset 时回灌的缓存参数集；
    // 连续 IDR 用于让 FFmpeg 排出解析器中的剩余输出。
    const QImage recovered = decodeFragmented(decoder, idr + idr + idr);

    QVERIFY(!recovered.isNull());
    QCOMPARE(recovered.size(), QSize(16, 16));
  }
};

QTEST_MAIN(TestH264Decoder)
#include "test_h264_decoder.moc"
