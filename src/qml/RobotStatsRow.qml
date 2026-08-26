// SPDX-License-Identifier: MIT
/**
 * RobotStatsRow.qml
 * @brief 单个机器人统计行组件
 * @details 显示单个机器人的编号、头像、血量条和各项属性数值
 * @author Clear
 * @date 2025-12-13
 * @copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    color: Qt.rgba(0.1, 0.1, 0.15, 0.6)
    radius: 4
    border.color: "#333333"
    border.width: 1

    // 机器人属性
    property color teamColor: "#E74C3C"
    property int robotNumber: 1
    property string robotName: "英雄"
    property string robotIcon: ""
    property int currentHP: 200
    property int maxHP: 600
    property int chassisPower: 120
    property int heatLimit: 240
    property int heatCooling: 40
    property int shootSpeedLimit: 16
    property bool isConnected: false

    property int level: 1
    property string teamPrefix: "red"
    // 计算血量百分比 (限制在 0.0 - 1.0 之间，防止 UI 溢出)
    property real hpPercent: maxHP > 0 ? Math.min(1.0, currentHP / maxHP) : 0

    // 根据血量选择颜色
    property color hpColor: {
        if (hpPercent >= 0.5)
            return teamColor;
        if (hpPercent > 0.25)
            return "#F39C12";
        return "#E74C3C";
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: root.width * 0.01
        anchors.rightMargin: root.width * 0.01
        spacing: 0

        // 编号 （圆形框）
        Item{
            Layout.preferredWidth: root.width*0.057
            Layout.fillHeight: true
            Rectangle{
                width: root.width*0.03
                height: root.width*0.03
                anchors.centerIn: parent
                radius:root.width*0.03*0.5
                color:"black"

                Text {
                    anchors.centerIn: parent
                    text: root.robotNumber.toString()
                    color: "#FFFFFF"
                    font.pixelSize: root.height*0.2
                    font.bold: true
                }
            }
        }

        // 机器人信息（头像 + 名称 + 血条）
        Item {
            Layout.preferredWidth: root.width*0.34
            Layout.fillHeight: true

            RowLayout {
                anchors.centerIn: parent
                spacing: root.width*0.023

                // 头像占位
                Rectangle {
                    Layout.preferredWidth: root.height * 0.7
                    Layout.preferredHeight: root.height * 0.7
                    radius: root.height * 0.7*0.5
                    color: "black"

                    Image {
                        id: robotImage
                        anchors.centerIn: parent
                        source: root.robotIcon
                        sourceSize.width: Math.round(root.height * 0.8)
                        sourceSize.height: Math.round(root.height * 0.8)
                        opacity: root.isConnected ? 0.8 : 0.45
                        visible: status === Image.Ready
                        }

                    Image {
                        id: disconnectIcon
                            anchors.centerIn: parent
                            width: parent.width * 1
                            height: parent.height * 1
                            source: "qrc:/images/tab/message_disconnect.png"
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            mipmap: true
                            visible: status === Image.Ready && !root.isConnected
                            z: robotImage.z + 1
                        }
                }

                // 名称 + 血条
                ColumnLayout {
                    spacing: root.height*0.07

                    // 名称
                    RowLayout{
                        Layout.alignment: Qt.AlignHCenter
                        Image{
                            Layout.preferredWidth: root.height*0.2
                            Layout.preferredHeight: root.height*0.2
                            source: "qrc:/images/tab/tab_level_" + Math.min(Math.max(root.level, 1), 10) + ".png"
                            sourceSize.width: 28
                            sourceSize.height: 28
                            visible: status === Image.Ready
                        }
                        Text{
                            text: root.robotName
                            color: "#CCCCCC"
                            font.pixelSize: root.height*0.15
                        }
                    }

                    // 血量条
                    Rectangle {
                        Layout.preferredWidth: root.width*0.18
                        Layout.preferredHeight: root.height*0.10        //0.15
                        color: "#333333"
                        radius: 2


                        //血量条显示
                        Rectangle {
                            id: hpBarInner
                            width: parent.width*root.hpPercent
                            height:  parent.height
                            radius: 3
                            antialiasing: true
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0.0; color: Qt.lighter(root.hpColor, 1.2) }
                                GradientStop { position: 0.5; color: root.hpColor }
                                GradientStop { position: 1.0; color: Qt.darker(root.hpColor, 1.1) }
                            }

                            // 边缘发光（仅边框）
                            Rectangle {
                                anchors.fill: parent
                                color: "transparent"
                                radius: hpBarInner.radius
                                border.color: root.hpColor
                                border.width: 1
                                z: 1000
                            }

                            // 顶部高光，增强立体感
                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                height: parent.height * 0.35
                                radius: hpBarInner.radius
                                color: "#FFFFFF"
                                opacity: 0.18
                            }

                            //血量条动画过渡
                            Behavior on width {
                                NumberAnimation { duration: 220; easing.type: Easing.OutQuad }
                            }
                        }
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: root.currentHP + " / " + root.maxHP
                        color: "#FFFFFF"
                        font.bold:true
                        font.pixelSize: root.height*0.15
                    }
                }
            }
        }


        // 底盘功率上限
        Text {
            Layout.preferredWidth: root.width*0.18
            text: root.chassisPower.toString()
            color: "#FFFFFF"
            font.pixelSize: root.height*0.25
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
        }

        // 热量上限
        Text {
            Layout.preferredWidth: root.width*0.13
            text: root.heatLimit.toString()
            color: "#FFFFFF"
            font.pixelSize: root.height*0.25
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
        }

        // 热量冷却
        Text {
            Layout.preferredWidth: root.width*0.13
            text: root.heatCooling.toString()
            color: "#FFFFFF"
            font.pixelSize: root.height*0.25
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
        }

        // 射速上限
        Text {
            Layout.preferredWidth: root.width*0.13
            text: root.shootSpeedLimit.toString()
            color: "#FFFFFF"
            font.pixelSize: root.height*0.25
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
        }
        Item{
            Layout.fillWidth: true
        }
    }
}
