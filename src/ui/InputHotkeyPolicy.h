#ifndef RM_INPUTHOTKEYPOLICY_H
#define RM_INPUTHOTKEYPOLICY_H

#include <QKeyEvent>

class QWidget;

namespace RM::InputHotkeyPolicy {

bool isGlobalPanelHotkey(int key);
bool isSettingsPanelHotkey(int key);
bool isDamagePanelHotkey(const QKeyEvent *event);
bool isTacticalOverlayHotkey(const QKeyEvent *event);
bool isTacticalLargeMapToggleHotkey(const QKeyEvent *event);
bool isSettingsPanelShortcutKey(const QKeyEvent *event);
int settingsRobotIdForShortcut(const QKeyEvent *event);
bool isSettingsPanelConfirmHotkey(const QKeyEvent *event);
bool shouldCaptureSettingsPanelShortcut(const QKeyEvent *event,
                                        bool panelActive,
                                        bool robotSelectionPending);
bool isEngineerConfirmHotkey(const QKeyEvent *event);
bool canUseKeyboardMouseControl(bool loginActive, bool tacticalMode);
bool shouldCapturePointerForRemoteControl(bool controlAvailable,
                                          bool inputBlocked);
void enableMouseTrackingForWidgetTree(QWidget *root);

} // namespace RM::InputHotkeyPolicy

#endif // RM_INPUTHOTKEYPOLICY_H
