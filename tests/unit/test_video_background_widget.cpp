#include "widgets/VideoBackgroundWidget.h"
#include <QtTest>

class TestVideoBackgroundWidget : public QObject {
  Q_OBJECT

private slots:
  void testShouldRotateVideo180ForRobotId_data() {
    QTest::addColumn<int>("robotId");
    QTest::addColumn<bool>("expected");

    QTest::newRow("red-aerial") << 6 << true;
    QTest::newRow("blue-aerial") << 106 << true;
    QTest::newRow("red-hero") << 1 << false;
    QTest::newRow("blue-infantry") << 103 << false;
  }

  void testShouldRotateVideo180ForRobotId() {
    QFETCH(int, robotId);
    QFETCH(bool, expected);

    QCOMPARE(RM::VideoBackgroundWidget::shouldRotateVideo180ForRobotId(robotId),
             expected);
  }

  void testSetCurrentRobotIdUpdatesRotationState() {
    RM::VideoBackgroundWidget widget;

    widget.setCurrentRobotId(6);
    QVERIFY(widget.rotatesVideo180());

    widget.setCurrentRobotId(106);
    QVERIFY(widget.rotatesVideo180());

    widget.setCurrentRobotId(3);
    QVERIFY(!widget.rotatesVideo180());
  }
};

QTEST_MAIN(TestVideoBackgroundWidget)
#include "test_video_background_widget.moc"
