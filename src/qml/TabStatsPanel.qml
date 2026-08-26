// SPDX-License-Identifier: MIT
/**
 * TabStatsPanel.qml
 * @brief Tab键触发的机器人统计面板
 * @details 按下Tab键显示，松开隐藏。显示红蓝双方机器人详细状态信息。
 * @author Clear
 * @date 2025-12-13
 * @copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    // 面板尺寸
    width: 1100
    height: 650

    // 半透明深色背景
    color: Qt.rgba(0.08, 0.08, 0.12, 0.95)
    radius: root.height * 0.01

    // gameData 由 MainWindow 注入 QML 上下文，统一在面板入口适配。
    // qmllint disable unqualified
    readonly property var gameDataContext: typeof gameData !== "undefined" ? gameData : null
    // qmllint enable unqualified

    // 数据属性 - 与 C++ GameData 绑定（所有值由协议触发更新，中文注释）
    property var redTeamData: root.gameDataContext ? root.gameDataContext.redTeamData : []
    property var blueTeamData: root.gameDataContext ? root.gameDataContext.blueTeamData : []
    // 统一绑定到 C++ GameData（所有值由协议消息触发更新，中文注释）
    property int redDartHits: root.gameDataContext ? root.gameDataContext.redDartHits : 0
    property int blueDartHits: root.gameDataContext ? root.gameDataContext.blueDartHits : 0
    property int dartTotal: root.gameDataContext ? root.gameDataContext.dartTotal : 4
    property int redTotalDamage: root.gameDataContext ? root.gameDataContext.redTotalDamage : 0
    property int blueTotalDamage: root.gameDataContext ? root.gameDataContext.blueTotalDamage : 915
    property int redRobotTotalHP: root.gameDataContext ? root.gameDataContext.redRobotTotalHP : 526
    property int blueRobotTotalHP: root.gameDataContext ? root.gameDataContext.blueRobotTotalHP : 1450

    // 主布局
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.width*0.02
        spacing: root.height * 0.02

        // ===============================================================
        // 顶部信息栏：飞镖命中数 | 总伤害对比 | 机器人总血量
        // ===============================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.height*0.1
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                spacing: root.width*0.02

                // 左侧 - 红方飞镖命中数
                RowLayout {
                    spacing: root.width*0.008
                    Image {
                        source: "qrc:/images/silo_icon_darts.png"
                        sourceSize.width: Math.round(root.height * 0.1)
                        sourceSize.height: Math.round(root.height * 0.1)
                        visible: status === Image.Ready
                    }
                    Text {
                        text: root.redDartHits.toString() + " | " + root.dartTotal.toString()
                        color: "#AAAAAA"
                        font.pixelSize:root.height*0.1*0.25
                        font.bold: true
                    }
                    Item {
                        Layout.preferredWidth: root.width*0.008*4
                    }
                    Text {
                        text: "机器人总剩余血量"
                        color: "#AAAAAA"
                        font.pixelSize: root.height*0.1*0.25
                    }
                    Text {
                        text: root.redRobotTotalHP
                        color: "#E74C3C"
                        font.pixelSize: root.height*0.1*0.3
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                // 中央 - 总伤害对比
                Rectangle {
                    Layout.preferredWidth: root.width*0.3
                    Layout.preferredHeight: root.height*0.12
                    color:  Qt.rgba(0.08, 0.08, 0.12, 0.95)
                    radius: root.height * 0.01

                    Image {
                        source: "qrc:/images/tab/tab_header_hurt.png"
                        anchors.centerIn: parent
                        width: root.width*0.4
                        height: root.height*0.053
                        fillMode: Image.PreserveAspectCrop
                        visible: status === Image.Ready
                    }

                    RowLayout {
                        anchors.fill: parent
                        spacing: root.height * 0.00

                        // 左侧红色数字容器
                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Text {
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                text: root.redTotalDamage.toString()
                                color: "#E74C3C"
                                font.pixelSize: root.height*0.1*0.6
                                font.bold: true
                            }
                        }

                        // 中间占位符（控制数字与中心图标的间距）
                        Item {
                            Layout.preferredWidth: root.width * 0.12 // 调整这个宽度来控制间距
                            Layout.fillHeight: true
                        }

                        // 右侧蓝色数字容器
                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Text {
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                text: root.blueTotalDamage.toString()
                                color: "#3498DB"
                                font.pixelSize: root.height*0.1*0.6
                                font.bold: true
                            }
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                // 右侧 - 蓝方飞镖命中数
                RowLayout {
                    spacing: root.width*0.008
                    Text {
                        text: "机器人总剩余血量"
                        color: "#AAAAAA"
                        font.pixelSize: root.height*0.1*0.25
                    }
                    Text {
                        text: root.blueRobotTotalHP
                        color: "#3498DB"
                        font.pixelSize: root.height*0.1*0.3
                    }
                    Item {
                        Layout.preferredWidth: root.width*0.008*4
                    }
                    Image {
                        source: "qrc:/images/silo_icon_darts.png"
                        sourceSize.width: Math.round(root.height * 0.1)
                        sourceSize.height: Math.round(root.height * 0.1)
                        visible: status === Image.Ready
                    }
                    Text {
                        text: root.blueDartHits.toString() + " | " + root.dartTotal.toString()
                        color: "#AAAAAA"
                        font.pixelSize: root.height*0.1*0.25
                        font.bold: true
                    }
                }
            }
        }

        // ===============================================================
        // 双列机器人统计表格
        // ===============================================================
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: root.width*0.01
            // 左侧 - 红方机器人列表
            RobotStatsTable {
                id: redTeamTable
                Layout.fillWidth: true
                Layout.fillHeight: true
                teamColor: "#E74C3C"
                teamName: "红方"
                robotData: root.redTeamData
            }

            // 分隔线
            Rectangle {
                Layout.preferredWidth: root.width*0.001
                Layout.fillHeight: true
                color:"transparent"
            }

            // 右侧 - 蓝方机器人列表
            RobotStatsTable {
                id: blueTeamTable
                Layout.fillWidth: true
                Layout.fillHeight: true
                teamColor: "#3498DB"
                teamName: "蓝方"
                robotData: root.blueTeamData
            }
        }

        // ===============================================================
        // 底部图例说明
        // ===============================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.height * 0.01
            color: "transparent"

            RowLayout {
                anchors.centerIn: parent
                spacing: root.width * 0.02

            }
        }
    }
}
