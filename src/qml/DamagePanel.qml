// SPDX-License-Identifier: MIT
/**
 * DamagePanel.qml
 * @brief ~键触发的伤害统计和模块状态面板
 * @details 按下~键显示，松开隐藏。显示伤害统计和各模块在线状态。
 * @author Clear
 * @date 2025-12-13
 * @copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    // 面板尺寸
    width: 260
    height: 480

    // 半透明深色背景
    color: Qt.rgba(0.08, 0.08, 0.12, 0.9)
    radius: root.height * 0.01

    // 数据属性 - 由外部绑定
    // gameData 由 MainWindow 注入，面板内统一从这个入口读取。
    // qmllint disable unqualified
    readonly property var gameDataContext: typeof gameData !== "undefined" ? gameData : null
    // qmllint enable unqualified
    property int damage17mm: gameDataContext ? gameDataContext.damage17mm : 0
    property int damage17mmPercent: gameDataContext ? gameDataContext.damage17mmPercent : 0
    property int damage42mm: gameDataContext ? gameDataContext.damage42mm : 0
    property int damage42mmPercent: gameDataContext ? gameDataContext.damage42mmPercent : 0
    property int damageCollision: gameDataContext ? gameDataContext.damageCollision : 0
    property int damageCollisionPercent: gameDataContext ? gameDataContext.damageCollisionPercent : 0
    property int damageOffline: gameDataContext ? gameDataContext.damageOffline : 0
    property int damageOfflinePercent: gameDataContext ? gameDataContext.damageOfflinePercent : 0
    property int damageWarning: gameDataContext ? gameDataContext.damageWarning : 0
    property int damageWarningPercent: gameDataContext ? gameDataContext.damageWarningPercent : 0

    // 模块状态数据
    // 当服务器未启动时，使用本地测试数据（部分在线、部分离线），以便 UI 验证
    property var mockModuleData: [
        { name: "装甲0", online: true },
        { name: "装甲1", online: false },
        { name: "装甲2", online: true },
        { name: "装甲3", online: false },
        { name: "17mm测速", online: true },
        { name: "图传", online: false },
        { name: "RFID", online: true },
        { name: "UWB", online: false },
        { name: "WIFI", online: false },
        { name: "灯条0", online: true },
        { name: "电容", online: false }
    ]
    // 优先使用真实数据；若为空则回退到 mockModuleData（服务器未启动或数据尚未下发）
    property var moduleData: (gameDataContext && gameDataContext.moduleData && gameDataContext.moduleData.length > 0)
                             ? gameDataContext.moduleData : mockModuleData

    // 主布局
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.height * 0.03
        spacing: root.height * 0.00

        // ===============================================================
        // 伤害面板
        // ===============================================================
        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: root.height*0.51
            spacing: root.height*0.02

            // 标题
            Text {
                text: "伤害面板"
                color: "#CCAA44"
                font.pixelSize: root.height*0.04
                font.bold: true
            }

            // 伤害类型列表
            ColumnLayout {
                Layout.fillWidth: true
                spacing: root.height*0.018
                DamageRow {
                    icon: "qrc:/images/sundry_bullet.png"
                    name: "17mm 弹丸"
                    value: root.damage17mm
                    percent: root.damage17mmPercent
                }
                DamageRow {
                    icon: "qrc:/images/sundry_golf.png"
                    name: "42mm 弹丸"
                    value: root.damage42mm
                    percent: root.damage42mmPercent
                }
                DamageRow {
                    icon: "qrc:/images/sundry_icon.png"
                    name: "撞击"
                    value: root.damageCollision
                    percent: root.damageCollisionPercent
                }
                DamageRow {
                    icon: "qrc:/images/sundry_icon.png"
                    name: "模块离线"
                    value: root.damageOffline
                    percent: root.damageOfflinePercent
                }
                DamageRow {
                    icon: "qrc:/images/sundry_icon.png"
                    name: "警告"
                    value: root.damageWarning
                    percent: root.damageWarningPercent
                }
                Item{
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }
        }
        // ===============================================================
        // 模块状态面板
        // ===============================================================
        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: root.height*0.39
            spacing: root.height*0.02

            // 标题
            Text {
                text: "模块状态"
                color: "#CCAA44"
                font.pixelSize: root.height*0.04
                font.bold: true
            }

            // 模块网格 - 2列布局
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: root.width*0.09
                rowSpacing: root.height*0.009

                Repeater {
                    model: root.moduleData

                    Rectangle {
                        id: moduleStatusItem
                        required property var modelData

                        Layout.preferredWidth: root.width*0.4
                        Layout.preferredHeight: root.height*0.04
                        color: Qt.rgba(0.16, 0.16, 0.2, 0.6)
                        radius: root.height * 0.01
                        border.width: root.height * 0.002
                        border.color: moduleStatusItem.modelData.online ? "#00AA00" : "#AA0000"

                        Text {
                            anchors.centerIn: parent
                            text: moduleStatusItem.modelData.name
                            color: "#DDDDDD"
                            font.pixelSize:  root.height*0.03
                        }
                    }
                }
            }

            // 提示文字
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "按下~键收起该面板"
                color: "#555555"
                font.pixelSize: root.height*0.02
                font.bold:true
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }

    // ===============================================================
    // 伤害行组件
    // ===============================================================
    component DamageRow: RowLayout {
        id: damageRow

        property string icon: ""
        property string name: ""
        property int value: 0
        property int percent: 0

        Layout.fillWidth: true
        spacing: root.height*0.01
        Layout.preferredHeight: root.height*0.04

        Image {
            source: damageRow.icon
            Layout.preferredWidth: root.height*0.04
            Layout.preferredHeight: root.height*0.04
        }

        Text {
            text: damageRow.name
            color: "#BBBBBB"
            font.pixelSize: root.height*0.03
            font.bold: true
        }

        Item {
            Layout.fillWidth: true
        }

        Text {
            text: damageRow.value + "  |  " + damageRow.percent + "%"
            color:"#BBBBBB"
            // "#777777"
            font.pixelSize: root.height*0.03
            horizontalAlignment: Text.AlignHCenter
            font.bold: true
        }
        Item {
            Layout.preferredWidth: root.width*0.015
        }
    }
}
