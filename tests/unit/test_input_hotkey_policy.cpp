#include "ui/InputHotkeyPolicy.h"
#include <QtTest>

class TestInputHotkeyPolicy : public QObject {
  Q_OBJECT

private slots:
  void testGlobalPanelHotkeys() {
    QVERIFY(RM::InputHotkeyPolicy::isGlobalPanelHotkey(Qt::Key_H));
    QVERIFY(RM::InputHotkeyPolicy::isGlobalPanelHotkey(Qt::Key_M));
    QVERIFY(RM::InputHotkeyPolicy::isGlobalPanelHotkey(Qt::Key_Plus));
    QVERIFY(RM::InputHotkeyPolicy::isGlobalPanelHotkey(Qt::Key_Minus));
    QVERIFY(RM::InputHotkeyPolicy::isGlobalPanelHotkey(Qt::Key_Asterisk));
    QVERIFY(RM::InputHotkeyPolicy::isGlobalPanelHotkey(Qt::Key_Slash));
    QVERIFY(RM::InputHotkeyPolicy::isGlobalPanelHotkey(Qt::Key_P));
    QVERIFY(!RM::InputHotkeyPolicy::isGlobalPanelHotkey(Qt::Key_Y));
  }

  void testDamagePanelHotkeyByQtKey() {
    QKeyEvent quoteEvent(QEvent::KeyPress, Qt::Key_QuoteLeft, Qt::NoModifier);
    QKeyEvent tildeEvent(QEvent::KeyPress, Qt::Key_AsciiTilde, Qt::ShiftModifier);
    QKeyEvent textFallbackEvent(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier,
                                QStringLiteral("~"));

    QVERIFY(RM::InputHotkeyPolicy::isDamagePanelHotkey(&quoteEvent));
    QVERIFY(RM::InputHotkeyPolicy::isDamagePanelHotkey(&tildeEvent));
    QVERIFY(RM::InputHotkeyPolicy::isDamagePanelHotkey(&textFallbackEvent));
  }

  void testTacticalOverlayHotkeys() {
    QKeyEvent settingsEvent(QEvent::KeyPress, Qt::Key_Plus, Qt::ShiftModifier,
                            QStringLiteral("+"));
    QKeyEvent settingsPEvent(QEvent::KeyPress, Qt::Key_P, Qt::NoModifier,
                             QStringLiteral("p"));
    QKeyEvent damageEvent(QEvent::KeyPress, Qt::Key_QuoteLeft, Qt::NoModifier);
    QKeyEvent ignoredEvent(QEvent::KeyPress, Qt::Key_H, Qt::NoModifier);

    QVERIFY(RM::InputHotkeyPolicy::isTacticalOverlayHotkey(&settingsEvent));
    QVERIFY(RM::InputHotkeyPolicy::isTacticalOverlayHotkey(&settingsPEvent));
    QVERIFY(RM::InputHotkeyPolicy::isTacticalOverlayHotkey(&damageEvent));
    QVERIFY(!RM::InputHotkeyPolicy::isTacticalOverlayHotkey(&ignoredEvent));
  }

  void testTacticalLargeMapToggleHotkeyAcceptsPlainM() {
    QKeyEvent event(QEvent::KeyPress, Qt::Key_M, Qt::NoModifier);

    QVERIFY(RM::InputHotkeyPolicy::isTacticalLargeMapToggleHotkey(&event));
  }

  void testTacticalLargeMapToggleHotkeyRejectsNullAndOtherKeys() {
    QKeyEvent otherKeyEvent(QEvent::KeyPress, Qt::Key_N, Qt::NoModifier);

    QVERIFY(!RM::InputHotkeyPolicy::isTacticalLargeMapToggleHotkey(nullptr));
    QVERIFY(
        !RM::InputHotkeyPolicy::isTacticalLargeMapToggleHotkey(&otherKeyEvent));
  }

  void testTacticalLargeMapToggleHotkeyRejectsAllModifiers() {
    const QList<Qt::KeyboardModifiers> modifiers{
        Qt::ShiftModifier,
        Qt::ControlModifier,
        Qt::AltModifier,
        Qt::MetaModifier,
        Qt::KeypadModifier,
        Qt::GroupSwitchModifier,
        Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier |
            Qt::MetaModifier | Qt::KeypadModifier | Qt::GroupSwitchModifier,
    };

    for (Qt::KeyboardModifiers modifier : modifiers) {
      QKeyEvent event(QEvent::KeyPress, Qt::Key_M, modifier);
      QVERIFY2(
          !RM::InputHotkeyPolicy::isTacticalLargeMapToggleHotkey(&event),
          qPrintable(
              QStringLiteral("modifier=%1").arg(static_cast<int>(modifier))));
    }
  }

  void testTacticalLargeMapToggleHotkeyRejectsAutoRepeat() {
    QKeyEvent event(QEvent::KeyPress, Qt::Key_M, Qt::NoModifier, QString(),
                    true);

    QVERIFY(!RM::InputHotkeyPolicy::isTacticalLargeMapToggleHotkey(&event));
  }

  void testSettingsRobotShortcutMapping_data() {
    QTest::addColumn<int>("key");
    QTest::addColumn<int>("robotId");

    QTest::newRow("1-red-hero") << static_cast<int>(Qt::Key_1) << 1;
    QTest::newRow("2-red-engineer") << static_cast<int>(Qt::Key_2) << 2;
    QTest::newRow("3-red-standard") << static_cast<int>(Qt::Key_3) << 3;
    QTest::newRow("4-red-standard") << static_cast<int>(Qt::Key_4) << 4;
    QTest::newRow("5-red-aerial") << static_cast<int>(Qt::Key_5) << 6;
    QTest::newRow("6-blue-hero") << static_cast<int>(Qt::Key_6) << 101;
    QTest::newRow("7-blue-engineer") << static_cast<int>(Qt::Key_7) << 102;
    QTest::newRow("8-blue-standard") << static_cast<int>(Qt::Key_8) << 103;
    QTest::newRow("9-blue-standard") << static_cast<int>(Qt::Key_9) << 104;
    QTest::newRow("0-blue-aerial") << static_cast<int>(Qt::Key_0) << 106;
  }

  void testSettingsRobotShortcutMapping() {
    QFETCH(int, key);
    QFETCH(int, robotId);

    QKeyEvent topRowEvent(QEvent::KeyPress, key, Qt::NoModifier);
    QKeyEvent keypadEvent(QEvent::KeyPress, key, Qt::KeypadModifier);

    QCOMPARE(RM::InputHotkeyPolicy::settingsRobotIdForShortcut(&topRowEvent),
             robotId);
    QCOMPARE(RM::InputHotkeyPolicy::settingsRobotIdForShortcut(&keypadEvent),
             robotId);
  }

  void testSettingsRobotShortcutRejectsUnsafeInput() {
    QKeyEvent otherKey(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier);
    QKeyEvent shiftedDigit(QEvent::KeyPress, Qt::Key_1, Qt::ShiftModifier);
    QKeyEvent controlledDigit(QEvent::KeyPress, Qt::Key_1,
                              Qt::ControlModifier);
    QKeyEvent repeatedDigit(QEvent::KeyPress, Qt::Key_1, Qt::NoModifier,
                            QString(), true);

    QCOMPARE(RM::InputHotkeyPolicy::settingsRobotIdForShortcut(nullptr), 0);
    QCOMPARE(RM::InputHotkeyPolicy::settingsRobotIdForShortcut(&otherKey), 0);
    QCOMPARE(
        RM::InputHotkeyPolicy::settingsRobotIdForShortcut(&shiftedDigit), 0);
    QCOMPARE(
        RM::InputHotkeyPolicy::settingsRobotIdForShortcut(&controlledDigit),
        0);
    QCOMPARE(
        RM::InputHotkeyPolicy::settingsRobotIdForShortcut(&repeatedDigit), 0);
    QVERIFY(RM::InputHotkeyPolicy::isSettingsPanelShortcutKey(&repeatedDigit));
  }

  void testSettingsPanelConfirmHotkeys() {
    QKeyEvent returnEvent(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QKeyEvent keypadEnterEvent(QEvent::KeyPress, Qt::Key_Enter,
                               Qt::KeypadModifier);
    QKeyEvent modifiedEvent(QEvent::KeyPress, Qt::Key_Return,
                            Qt::AltModifier);
    QKeyEvent repeatedEvent(QEvent::KeyPress, Qt::Key_Return,
                            Qt::NoModifier, QString(), true);

    QVERIFY(RM::InputHotkeyPolicy::isSettingsPanelConfirmHotkey(&returnEvent));
    QVERIFY(RM::InputHotkeyPolicy::isSettingsPanelConfirmHotkey(
        &keypadEnterEvent));
    QVERIFY(!RM::InputHotkeyPolicy::isSettingsPanelConfirmHotkey(
        &modifiedEvent));
    QVERIFY(!RM::InputHotkeyPolicy::isSettingsPanelConfirmHotkey(
        &repeatedEvent));
    QVERIFY(!RM::InputHotkeyPolicy::isSettingsPanelConfirmHotkey(nullptr));
  }

  void testSettingsPanelShortcutCaptureIsStrictlyScoped() {
    QKeyEvent digitEvent(QEvent::KeyPress, Qt::Key_1, Qt::NoModifier);
    QKeyEvent returnEvent(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QKeyEvent repeatedReturn(QEvent::KeyPress, Qt::Key_Return,
                             Qt::NoModifier, QString(), true);

    QVERIFY(!RM::InputHotkeyPolicy::shouldCaptureSettingsPanelShortcut(
        &digitEvent, false, false));
    QVERIFY(RM::InputHotkeyPolicy::shouldCaptureSettingsPanelShortcut(
        &digitEvent, true, false));
    QVERIFY(!RM::InputHotkeyPolicy::shouldCaptureSettingsPanelShortcut(
        &returnEvent, true, false));
    QVERIFY(RM::InputHotkeyPolicy::shouldCaptureSettingsPanelShortcut(
        &returnEvent, true, true));
    QVERIFY(RM::InputHotkeyPolicy::shouldCaptureSettingsPanelShortcut(
        &repeatedReturn, true, false));
    QVERIFY(!RM::InputHotkeyPolicy::shouldCaptureSettingsPanelShortcut(
        nullptr, true, true));
  }

  void testEngineerConfirmHotkeys() {
    QKeyEvent returnEvent(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QKeyEvent keypadEnterEvent(QEvent::KeyPress, Qt::Key_Enter, Qt::NoModifier);
    QKeyEvent modifiedEvent(QEvent::KeyPress, Qt::Key_Return,
                            Qt::ControlModifier);
    QKeyEvent autoRepeatEvent(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier,
                              QString(), true);
    QKeyEvent otherEvent(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier);

    QVERIFY(RM::InputHotkeyPolicy::isEngineerConfirmHotkey(&returnEvent));
    QVERIFY(RM::InputHotkeyPolicy::isEngineerConfirmHotkey(&keypadEnterEvent));
    QVERIFY(!RM::InputHotkeyPolicy::isEngineerConfirmHotkey(&modifiedEvent));
    QVERIFY(!RM::InputHotkeyPolicy::isEngineerConfirmHotkey(&autoRepeatEvent));
    QVERIFY(!RM::InputHotkeyPolicy::isEngineerConfirmHotkey(&otherEvent));
    QVERIFY(!RM::InputHotkeyPolicy::isEngineerConfirmHotkey(nullptr));
  }

  void testKeyboardMouseControlRequiresOperationScreen() {
    QVERIFY(RM::InputHotkeyPolicy::canUseKeyboardMouseControl(true, false));
    QVERIFY(!RM::InputHotkeyPolicy::canUseKeyboardMouseControl(true, true));
    QVERIFY(!RM::InputHotkeyPolicy::canUseKeyboardMouseControl(false, false));
    QVERIFY(!RM::InputHotkeyPolicy::canUseKeyboardMouseControl(false, true));
  }

  void testPointerCaptureOnlyAppliesToActiveRemoteControl() {
    QVERIFY(RM::InputHotkeyPolicy::shouldCapturePointerForRemoteControl(true,
                                                                        false));
    QVERIFY(!RM::InputHotkeyPolicy::shouldCapturePointerForRemoteControl(false,
                                                                         false));
    QVERIFY(!RM::InputHotkeyPolicy::shouldCapturePointerForRemoteControl(true,
                                                                         true));
  }

  void testMouseTrackingIsEnabledForEntireWidgetTree() {
    QWidget root;
    QWidget child(&root);
    QWidget grandchild(&child);

    QVERIFY(!root.hasMouseTracking());
    QVERIFY(!child.hasMouseTracking());
    QVERIFY(!grandchild.hasMouseTracking());

    RM::InputHotkeyPolicy::enableMouseTrackingForWidgetTree(&root);

    QVERIFY(root.hasMouseTracking());
    QVERIFY(child.hasMouseTracking());
    QVERIFY(grandchild.hasMouseTracking());
  }

  void testNullEventsAreIgnored() {
    QVERIFY(!RM::InputHotkeyPolicy::isDamagePanelHotkey(nullptr));
    QVERIFY(!RM::InputHotkeyPolicy::isTacticalOverlayHotkey(nullptr));
    QVERIFY(!RM::InputHotkeyPolicy::isSettingsPanelShortcutKey(nullptr));
  }

  void testMouseTrackingAcceptsNullRoot() {
    RM::InputHotkeyPolicy::enableMouseTrackingForWidgetTree(nullptr);
  }
};

QTEST_MAIN(TestInputHotkeyPolicy)
#include "test_input_hotkey_policy.moc"
