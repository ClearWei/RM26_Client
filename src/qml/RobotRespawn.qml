/**
 * @file RobotRespawn.qml
 * @brief 复活阶段弹窗
 * @details 接收服务器端发送的信号，弹出弹窗，标题为"复活中"，下方显示读条进度，进度条下方为两个复活选项：“[复活所需金币数]金币买活”（红色，如不可选则选项为灰色,显示"[复活所需金币数]金币不足"）和“免费复活[Y]”（如不可选则选项为灰色）
 *          选项下方为文字说明"TIPS"（选择较小的字号）
 *          选择对应选项则发送相应的CommonCommand指令给服务器
 *          读条结束后标题变为"复活读条完成"，"免费复活"选项将变为可选
 *          弹窗背景图片使用"qrc:/images/prep_phase_frame.png"，带半透明遮罩背景，弹窗布局以及代码格式均参考PrepPhasePopup.qml
 *     指令名：RobotRespawnStatus，机器人复活状态同步，服务器→自定义客户端（在mqtt_publisher里增加发布信息）
 *     message RobotRespawnStatus {
 *          bool is_pending_respawn = 1; // 是否处于待复活状态
 *          uint32 total_respawn_progress = 2; // 复活所需总读条
 *          uint32 current_respawn_progress = 3; // 当前复活读条进度
 *          bool can_free_respawn = 4; // 是否可以免费复活
 *          uint32 gold_cost_for_respawn = 5; // 花费金币复活所需金币数
 *          bool can_pay_for_respawn = 6; // 是否允许花费金币复活
 *          }
 *     指令名：RobotRespawnStatus
 *     指令名：CommonCommand，机器人多种常用指令，自定义客户端→服务器，触发式发送，发送频率最高为 10Hz
 *     cmd_type 枚举值：
 *     3：确认复活（若此时复活读条完成将立即复活）
 *     4：兑换立即复活（若此时符合兑换立即复活的规则要求，则会立即消耗金币兑换立即复活）
 *     message CommonCommand{
 *          uint32 cmd_type = 1;
 *          uint32 param = 2;
 *          }
 */

import QtQuick 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    anchors.fill: parent
    property real resolutionScale: 1.0
    readonly property real uiScale: resolutionScale
    z: 200

    // gameData 与 network 由 MainWindow 注入 QML 上下文，统一在组件入口适配。
    // qmllint disable unqualified
    readonly property var gameDataContext: typeof gameData !== "undefined" ? gameData : null
    readonly property var networkContext: typeof network !== "undefined" ? network : null
    // qmllint enable unqualified

    // === 数据绑定 ===
    property bool isPendingRespawn: false
    property int totalRespawnProgress: 100
    property int currentRespawnProgress: 0
    property bool canFreeRespawn: false
    property int goldCostForRespawn: 0
    property bool canPayForRespawn: false

    // 目标机器人索引（由外部设置，默认为 0）
    property int robotIndex: 0

    // 期望签名: function(cmd_type, param)
    property var sendCommonCommandImpl: undefined
    // 机器人在线状态（基于 GameData.isRobotConnected）
    property bool robotOnline: true

    // 辅助计算
    property real progressPercent: totalRespawnProgress > 0 ? (currentRespawnProgress / totalRespawnProgress) * 100 : 0
    property bool isReadComplete: currentRespawnProgress >= totalRespawnProgress
    property bool sending: false
    // 仅基于当前登录机器人的 RobotStaticStatus 在线态判断离线。
    property bool isRobotOffline: !robotOnline

    function refreshRobotOnlineStatus() {
        try {
            if (!root.gameDataContext || !root.gameDataContext.isRobotConnected || !root.gameDataContext.myRobot)
                return
            var myId = Number(root.gameDataContext.myRobot.robotId)
            if (!isNaN(myId) && myId > 0) {
                root.robotOnline = Boolean(root.gameDataContext.isRobotConnected(myId))
            }
        } catch (e) {
            root.robotOnline = true
        }
    }

    visible: isPendingRespawn
    focus: visible

    Component.onCompleted: {
        root.refreshRobotOnlineStatus()
        if (root.visible) Qt.callLater(function() { dialog.forceActiveFocus() })
        console.log("[RobotRespawn] Component.onCompleted: currentRespawnProgress=", currentRespawnProgress, " totalRespawnProgress=", totalRespawnProgress)
        if (!sendCommonCommandImpl) {
            if (root.networkContext) {
                sendCommonCommandImpl = function(cmd, param) {
                    console.log("[RobotRespawn] invoke network.sendCommonCommand", cmd, param)
                    root.networkContext.sendCommonCommand(cmd, param)
                }
                console.log("[RobotRespawn] sendCommonCommandImpl auto-bound to network.sendCommonCommand")
            } else {
                // 降级防护：未绑定 network 时使用安全的 no-op 实现，避免运行时报错
                sendCommonCommandImpl = function(cmd, param) {
                    console.warn("[RobotRespawn] network not available, drop command", cmd, param)
                }
                console.warn("[RobotRespawn] network not bound; using no-op sendCommonCommandImpl")
            }
        }
    }

    onVisibleChanged: {
        if (visible) Qt.callLater(function() { dialog.forceActiveFocus() })
    }

    Connections {
        target: root.gameDataContext
        function onMyRobotUpdated() {
            root.refreshRobotOnlineStatus()
        }
    }

    //=== 半透明遮罩背景 ===（可改为黑白蒙版
    Rectangle {
        anchors.fill: parent
        color: "#66000000"
        visible: root.visible
        MouseArea { anchors.fill: parent }
    }

    // 防重复发送定时器
    Timer {
        id: sendCooldown
        interval: 600
        repeat: false
        onTriggered: {
            root.sending = false
            console.log("[RobotRespawn] send cooldown cleared")
        }
    }

    // === 主弹窗容器 ===
    Item {
        id: dialog
        width: 650
        height: 250
        scale: root.uiScale
        transformOrigin: Item.Top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 190 * root.uiScale
        focus: root.visible

        Keys.onReleased: function(event) {
            // F1 快捷键：金币买活（cmd_type=4）
            if (event.key === Qt.Key_F1 && root.canPayForRespawn && !root.sending && root.robotOnline) {
                root.sending = true
                if (root.sendCommonCommandImpl) {
                    root.sendCommonCommandImpl.call(root, 4, root.robotIndex)
                }
                sendCooldown.start()
                event.accepted = true
                return
            }

            // 键盘 Y 快捷键：免费复活（cmd_type=3）
            if (event.key === Qt.Key_Y && root.canFreeRespawn && !root.isRobotOffline && !root.sending) {
                root.sending = true
                if (root.sendCommonCommandImpl) {
                    root.sendCommonCommandImpl.call(root, 3, root.robotIndex)
                }
                sendCooldown.start()
                event.accepted = true
            }
        }

        Image {
            anchors.fill: parent
            source: (root.robotOnline ? "qrc:/images/gamephase/MaskTips-Ad-320.png" : "qrc:/images/gamephase/MaskTips-Ad-Yellow-320.png")
            fillMode: Image.Stretch
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            Item { Layout.fillWidth: true }

            ColumnLayout {
                id: contentColumn
                Layout.preferredWidth: 520
                Layout.maximumWidth: 520
                Layout.fillHeight: true
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Text {
                        id: titleText
                        text: (!root.robotOnline && root.isPendingRespawn) ? "机器人离线，复活暂停" : (root.isReadComplete ? "复活读条完成" : "复活中")
                        color: "#FFFFFF"
                        font.family: "Microsoft YaHei"
                        font.pixelSize: 20
                        font.bold: true
                    }

                    Item { Layout.fillWidth: true }

                    // 进度数值（可选）
                    Text {
                        text: String(root.currentRespawnProgress) + " / " + String(root.totalRespawnProgress)
                        color: "#CCCCCC"
                        font.pixelSize: 14
                    }
                }

                // 离线提示：当机器人被判定为离线时显示黄色文字并暂停交互
                Text {
                    visible: root.isRobotOffline
                    text: "机器人离线，复活暂停"
                    color: "#FFD54F"  // 浅黄色/琥珀色
                    font.pixelSize: 16
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                // 读条进度条
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    radius: 6
                    color: "#333333"

                    Rectangle {
                        id: bar
                        height: parent.height
                        width: Math.max(2, parent.width * Math.min(1.0, root.progressPercent / 100.0))
                        radius: 6
                        color: "#00FFFF"
                        Behavior on width { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: Math.floor(root.progressPercent) + "%"
                        color: "#FFFFFF"
                        font.pixelSize: 14
                    }
                }

                // 两个选项按钮
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    // 金币复活
                    Rectangle {
                        id: payBtn
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44
                        radius: 6
                        // 若机器人离线，则按钮置灰
                        color: (root.canPayForRespawn && root.robotOnline) ? "#D32F2F" : "#777777"
                        scale: 1.0
                        Behavior on scale { NumberAnimation { duration: 120 } }
                        opacity: (root.sending && root.canPayForRespawn) ? 0.7 : 1.0
                        border.width: 0

                        MouseArea {
                            anchors.fill: parent
                            enabled: (root.canPayForRespawn && !root.sending && root.robotOnline)
                            onPressed: { parent.scale = 0.97 }
                            onReleased: { parent.scale = 1.0 }
                            onClicked: {
                                // 防重复点击
                                root.sending = true
                                // cmd_type 4 = 兑换立即复活
                                console.log("[RobotRespawn] send cmd", 4, root.robotIndex)
                                if (root.sendCommonCommandImpl) {
                                    root.sendCommonCommandImpl.call(root, 4, root.robotIndex)
                                } else {
                                    console.log("[RobotRespawn] sendCommonCommandImpl not set: cmd=4")
                                }
                                sendCooldown.start()
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            text: (!root.robotOnline) ? (root.goldCostForRespawn + " 已离线") : (root.canPayForRespawn ? (root.goldCostForRespawn + "金币买活[F1]") : (root.goldCostForRespawn + "金币不足"))
                            color: "#FFFFFF"
                            font.pixelSize: 16
                            font.bold: true
                        }
                    }

                    // 免费复活 / 确认复活
                    Rectangle {
                        id: freeBtn
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44
                        radius: 6
                        color: (root.canFreeRespawn && root.robotOnline) ? "#00FFFF" : "#777777"
                        scale: 1.0
                        Behavior on scale { NumberAnimation { duration: 120 } }
                        opacity: (root.sending && root.canFreeRespawn) ? 0.7 : 1.0

                        MouseArea {
                            anchors.fill: parent
                            enabled: (root.canFreeRespawn && root.robotOnline) && !root.sending
                            onPressed: { parent.scale = 0.97 }
                            onReleased: { parent.scale = 1.0 }
                            onClicked: {
                                root.sending = true
                                // cmd_type 3 = 确认复活
                                console.log("[RobotRespawn] send cmd", 3, root.robotIndex)
                                if (root.sendCommonCommandImpl) {
                                    root.sendCommonCommandImpl.call(root, 3, root.robotIndex)
                                } else {
                                    console.log("[RobotRespawn] sendCommonCommandImpl not set: cmd=3")
                                }
                                sendCooldown.start()
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            text: root.robotOnline ? "免费复活[Y]" : "等待恢复"
                            color: "#FFFFFF"
                            font.pixelSize: 16
                            font.bold: true
                        }
                    }
                }

                // TIPS文字备注
                Text {
                    Layout.fillWidth: true
                    color: "#BBBBBB"
                    font.pixelSize: 10
                    wrapMode: Text.WordWrap
                }
            }

            Item { Layout.fillWidth: true }
        }
    }

    // 提供从外部一次性更新状态的方法，方便网络层直接调用
    // robot_id 过滤已在 C++ 侧 processRobotRespawnStatusMap 中完成
    function updateFromStatus(status) {
        if (status === undefined || status === null) return

        if (status.is_pending_respawn !== undefined) root.isPendingRespawn = status.is_pending_respawn
        if (status.total_respawn_progress !== undefined) root.totalRespawnProgress = status.total_respawn_progress
        if (status.current_respawn_progress !== undefined) root.currentRespawnProgress = status.current_respawn_progress
        if (status.can_free_respawn !== undefined) root.canFreeRespawn = status.can_free_respawn
        if (status.gold_cost_for_respawn !== undefined) root.goldCostForRespawn = status.gold_cost_for_respawn
        if (status.can_pay_for_respawn !== undefined) root.canPayForRespawn = status.can_pay_for_respawn

        // 在线状态由 Connections.onMyRobotUpdated 驱动刷新，此处仅做兜底
        root.refreshRobotOnlineStatus()
    }
}
