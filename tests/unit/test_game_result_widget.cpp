#include <QtTest>

#include "widgets/GameResultWidget.h"

class TestGameResultWidget : public QObject {
  Q_OBJECT

private slots:
  void testVictorySoundEnabledByDefault() {
    GameResultWidget widget;
    QVERIFY(widget.playVictoryOnShow());
  }

  void testVictorySoundCanBeDisabledExplicitly() {
    GameResultWidget widget;
    widget.setPlayVictoryOnShow(false);
    QVERIFY(!widget.playVictoryOnShow());
  }

  void testResolutionScalingUsesArbitraryViewport() {
    GameResultWidget widget;

    widget.setResolutionViewport(QSize(1600, 1000));

    QCOMPARE(widget.size(), QSize(1000, 667));
    QCOMPARE(widget.resolutionViewport(), QSize(1600, 1000));
  }

  void testResolutionScalingUsesLimitingDimension() {
    GameResultWidget widget;

    widget.setResolutionViewport(QSize(3440, 1440));

    QCOMPARE(widget.size(), QSize(1600, 1067));
  }

  void testInvalidResolutionViewportFallsBackToBaseline() {
    GameResultWidget widget;
    widget.setResolutionViewport(QSize(2560, 1440));

    widget.setResolutionViewport(QSize());

    QCOMPARE(widget.size(), GameResultWidget::designSize());
    QCOMPARE(widget.resolutionViewport(), QSize(1920, 1080));
  }
};

QTEST_MAIN(TestGameResultWidget)
#include "test_game_result_widget.moc"
