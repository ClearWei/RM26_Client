#include "InputHotkeyPolicy.h"

#include <QWidget>

namespace RM::InputHotkeyPolicy {

bool isGlobalPanelHotkey(int key) {
  return key == Qt::Key_H || isSettingsPanelHotkey(key) || key == Qt::Key_M;
}

bool isSettingsPanelHotkey(int key) {
  return key == Qt::Key_P || key == Qt::Key_Plus || key == Qt::Key_Minus ||
         key == Qt::Key_Asterisk || key == Qt::Key_Slash;
}

bool isDamagePanelHotkey(const QKeyEvent *event) {
  if (!event) {
    return false;
  }

  if (event->key() == Qt::Key_QuoteLeft || event->key() == Qt::Key_AsciiTilde
      || event->key() == Qt::Key_Dead_Grave) {
    return true;
  }

  const QString text = event->text();
  if (text.contains(QLatin1Char('`')) || text.contains(QLatin1Char('~'))) {
    return true;
  }

#ifdef Q_OS_WIN
  constexpr quint32 kVkOem3 = 0xC0;
  constexpr quint32 kScanCodeOem3 = 0x29;
  return event->nativeVirtualKey() == kVkOem3 ||
         event->nativeScanCode() == kScanCodeOem3;
#else
  return false;
#endif
}

bool isTacticalOverlayHotkey(const QKeyEvent *event) {
  if (!event) {
    return false;
  }
  return isSettingsPanelHotkey(event->key()) || isDamagePanelHotkey(event);
}

bool isTacticalLargeMapToggleHotkey(const QKeyEvent *event) {
  if (!event || event->isAutoRepeat() || event->modifiers() != Qt::NoModifier) {
    return false;
  }
  return event->key() == Qt::Key_M;
}

bool isSettingsPanelShortcutKey(const QKeyEvent *event) {
  if (!event) {
    return false;
  }

  const Qt::KeyboardModifiers modifiers = event->modifiers();
  if (modifiers != Qt::NoModifier && modifiers != Qt::KeypadModifier) {
    return false;
  }

  const int key = event->key();
  return (key >= Qt::Key_0 && key <= Qt::Key_9) ||
         key == Qt::Key_Return || key == Qt::Key_Enter;
}

int settingsRobotIdForShortcut(const QKeyEvent *event) {
  if (!isSettingsPanelShortcutKey(event) || event->isAutoRepeat()) {
    return 0;
  }

  switch (event->key()) {
  case Qt::Key_1:
    return 1;
  case Qt::Key_2:
    return 2;
  case Qt::Key_3:
    return 3;
  case Qt::Key_4:
    return 4;
  case Qt::Key_5:
    return 6;
  case Qt::Key_6:
    return 101;
  case Qt::Key_7:
    return 102;
  case Qt::Key_8:
    return 103;
  case Qt::Key_9:
    return 104;
  case Qt::Key_0:
    return 106;
  default:
    return 0;
  }
}

bool isSettingsPanelConfirmHotkey(const QKeyEvent *event) {
  if (!isSettingsPanelShortcutKey(event) || event->isAutoRepeat()) {
    return false;
  }
  return event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter;
}

bool shouldCaptureSettingsPanelShortcut(const QKeyEvent *event,
                                        bool panelActive,
                                        bool robotSelectionPending) {
  if (!panelActive || !isSettingsPanelShortcutKey(event)) {
    return false;
  }

  if (event->isAutoRepeat()) {
    return true;
  }

  return settingsRobotIdForShortcut(event) > 0 ||
         (robotSelectionPending && isSettingsPanelConfirmHotkey(event));
}

bool isEngineerConfirmHotkey(const QKeyEvent *event) {
  if (!event || event->isAutoRepeat() || event->modifiers() != Qt::NoModifier) {
    return false;
  }
  return event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter;
}

bool canUseKeyboardMouseControl(bool loginActive, bool tacticalMode) {
  return loginActive && !tacticalMode;
}

bool shouldCapturePointerForRemoteControl(bool controlAvailable,
                                          bool inputBlocked) {
  return controlAvailable && !inputBlocked;
}

void enableMouseTrackingForWidgetTree(QWidget *root) {
  if (!root) {
    return;
  }

  root->setMouseTracking(true);
  const auto descendants = root->findChildren<QWidget *>();
  for (QWidget *widget : descendants) {
    widget->setMouseTracking(true);
  }
}

} // namespace RM::InputHotkeyPolicy
