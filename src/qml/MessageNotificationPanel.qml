pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

/**
 * @file MessageNotificationPanel.qml
 * @brief 系统和机器人消息通知面板 (协议绑定)
   空中机器人获得、等级到达、获得金币、
 * @author Clear
 * @date 2025-01-10
 */
Item {
    id: root
    width: 350
    height: 200

    // === 绑定 GameData 消息列表 ===
    // gameData 由 MainWindow 注入 QML 上下文，统一在消息面板入口适配。
    // qmllint disable unqualified
    readonly property var gameDataContext: typeof gameData !== "undefined" ? gameData : null
    // qmllint enable unqualified
    property var systemMsgList: root.gameDataContext ? root.gameDataContext.systemMessages : []
    property var robotMsgList: root.gameDataContext ? root.gameDataContext.robotMessages : []
    property int remainTime: root.gameDataContext && root.gameDataContext.remainTime !== undefined
        ? Number(root.gameDataContext.remainTime) : 0

    function formatRemainTime(seconds) {
        var total = Math.max(0, Number(seconds))
        var mm = Math.floor(total / 60)
        var ss = Math.floor(total % 60)
        var mmStr = String(mm)
        var ssStr = ss < 10 ? "0" + ss : String(ss)
        return mmStr + ":" + ssStr
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // --- 1.1 系统消息 ---
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            spacing: 4

            // 垂直标签
            Rectangle {
                id: labelSystem
                Layout.preferredWidth: 22
                Layout.preferredHeight: 55
                Layout.alignment: Qt.AlignTop
                color: "transparent"
                border.color: "#5080DAD8" // 青色边框
                border.width: 1
                radius: 2

                // 渐变背景
                Rectangle {
                    anchors.fill: parent
                    radius: 4
                    gradient: Gradient {
                        GradientStop {
                            position: 0.0
                            color: "#992D3034"
                        }
                        GradientStop {
                            position: 0.5
                            color: "#801E242C"
                        }
                        GradientStop {
                            position: 1.0
                            color: "#6634383C"
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: "系\n统"
                    color: "#CCCCCC"
                    font.pixelSize: 11
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    lineHeight: 1
                }
            }

            // 消息内容区
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "transparent"
                radius: 4
                border.color: "#40888888"
                border.width: 1

                ListView {
                    id: sysListView
                    anchors.fill: parent
                    anchors.margins: 6
                    model: root.systemMsgList
                    clip: true
                    spacing: 2

                    // 新消息时自动滚动到顶部
                    onCountChanged: positionViewAtEnd()

                    delegate: Item {
                        id: systemMessageRow
                        required property var modelData
                        readonly property var entry: systemMessageRow.modelData || ({})

                        width: sysListView.width
                        height: messageText.implicitHeight

                        Text {
                            id: messageText
                            width: parent.width
                            text: systemMessageRow.entry.text !== undefined
                                ? String(systemMessageRow.entry.text) : ""
                            color: systemMessageRow.entry.color !== undefined
                                   && String(systemMessageRow.entry.color).length > 0
                                   ? String(systemMessageRow.entry.color)
                                   : "#E5E5E5"
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            lineHeight: 1.2
                            textFormat: Text.PlainText
                        }
                    }

                    // 空状态显示
                    Text {
                        anchors.centerIn: parent
                        visible: sysListView.count === 0
                        text: "暂无系统消息"
                        color: "#666666"
                        font.pixelSize: 11
                    }
                }
            }
        }

        // --- 1.2 机器人消息 ---
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            spacing: 4

            // 垂直标签
            Rectangle {
                id: labelRobot
                Layout.preferredWidth: 22
                Layout.preferredHeight: 55
                Layout.alignment: Qt.AlignTop
                color: "transparent"
                border.color: "#5080DAD8"
                border.width: 1
                radius: 2

                // 渐变背景
                Rectangle {
                    anchors.fill: parent
                    radius: 4
                    gradient: Gradient {
                        GradientStop {
                            position: 0.0
                            color: "#992D3034"
                        }
                        GradientStop {
                            position: 0.5
                            color: "#801E242C"
                        }
                        GradientStop {
                            position: 1.0
                            color: "#6634383C"
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: "机\n器\n人"
                    color: "#CCCCCC"
                    font.pixelSize: 10
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    lineHeight: 1
                }
            }

            // 消息内容区
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "transparent"
                radius: 4
                border.color: "#40888888"
                border.width: 1

                ListView {
                    id: robotListView
                    anchors.fill: parent
                    anchors.margins: 6
                    model: root.robotMsgList
                    clip: true
                    spacing: 2

                    onCountChanged: positionViewAtEnd()

                    delegate: Text {
                        id: robotMessageRow
                        required property var modelData

                        width: robotListView.width
                        text: root.formatRemainTime(root.remainTime) + " " + robotMessageRow.modelData
                        color: "#E5E5E5"
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        lineHeight: 1.2
                        textFormat: Text.RichText
                    }

                    // 空状态显示
                    Text {
                        anchors.centerIn: parent
                        visible: robotListView.count === 0
                        text: "暂无机器人消息"
                        color: "#666666"
                        font.pixelSize: 11
                    }
                }
            }
        }
    }
}
