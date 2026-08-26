// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

/**
 * RobotStatsTable.qml
 * @brief 单方机器人统计表格组件
 * @details 显示一方（红方或蓝方）所有机器人的详细状态
 * @author Clear
 * @date 2025-12-13
 * @copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    color: "transparent"

    // 队伍属性
    property color teamColor: "#E74C3C"
    property string teamName: "红方"
    property var robotData: []

    ColumnLayout {
        anchors.fill: parent
        spacing: root.height*0.02

        // ===============================================================
        // 表头
        // ===============================================================
        Rectangle {
            id: headerRect
            Layout.preferredWidth: root.width*1
            Layout.preferredHeight: root.height*0.1
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: root.width*0.01
                anchors.rightMargin: root.width*0.01
                spacing: 0

                Text {
                    Layout.preferredWidth: headerRect.width*0.057
                    text: "#"
                    color: "#888888"
                    font.pixelSize: root.height*0.1*0.22
                    horizontalAlignment: Text.AlignHCenter
                }
                RowLayout {
                    Layout.preferredWidth: headerRect.width*0.32
                    Item {
                        Layout.fillWidth: true
                    }
                    Text {
                        Layout.preferredWidth: headerRect.width*0.25
                        text: "机器人信息"
                        color: "#888888"
                        font.pixelSize: root.height*0.1*0.22
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                Text {
                    Layout.preferredWidth: headerRect.width*0.18
                    text: "底盘功率上限"
                    color: "#888888"
                    font.pixelSize: root.height*0.1*0.22
                    horizontalAlignment: Text.AlignHCenter
                    //wrapMode: Text.WordWrap   // 允许文本换行
                }

                Text {
                    Layout.preferredWidth: headerRect.width*0.13
                    text: "热量上限"
                    color: "#888888"
                    font.pixelSize: root.height*0.1*0.22
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    Layout.preferredWidth: headerRect.width*0.13
                    text: "热量冷却"
                    color: "#888888"
                    font.pixelSize: root.height*0.1*0.22
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    Layout.preferredWidth: headerRect.width*0.13
                    text: "射速上限"
                    color: "#888888"
                    font.pixelSize: root.height*0.1*0.22
                    horizontalAlignment: Text.AlignHCenter
                }
                Item{
                    Layout.fillWidth: true
                }
            }
        }

        // ===============================================================
        // 机器人行列表
        // ===============================================================
        ListView {
            id: robotListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: root.height*0.02

            model: (root.robotData && root.robotData.length > 0) ? root.robotData : root.defaultRobotData

            delegate: RobotStatsRow {
                id: robotRow
                required property int index
                required property var modelData

                width: parent ? parent.width : 0
                height: root.height*0.12
                teamColor: root.teamColor
                teamPrefix: root.teamName === "蓝方" ? "blue" : "red"
                // 编号处理：101 -> 1, 102 -> 2, 以此类推
                robotNumber: (robotRow.modelData.number > 100) ? (robotRow.modelData.number % 100) : (robotRow.modelData.number || robotRow.index + 1)
                robotName: robotRow.modelData.name || "机器人"
                robotIcon: robotRow.modelData.icon || ""
                currentHP: robotRow.modelData.currentHP !== undefined ? Number(robotRow.modelData.currentHP) : 0
                maxHP: robotRow.modelData.maxHP !== undefined ? Number(robotRow.modelData.maxHP) : 600
                chassisPower: robotRow.modelData.chassisPower !== undefined ? Number(robotRow.modelData.chassisPower) : 0
                heatLimit: robotRow.modelData.heatLimit !== undefined ? Number(robotRow.modelData.heatLimit) : 0
                heatCooling: robotRow.modelData.heatCooling !== undefined ? Number(robotRow.modelData.heatCooling) : 0
                shootSpeedLimit: robotRow.modelData.shootSpeedLimit !== undefined ? Number(robotRow.modelData.shootSpeedLimit) : (robotRow.modelData.shootSpeed || 0)
                level: robotRow.modelData.level !== undefined ? Number(robotRow.modelData.level) : 1
                isConnected: robotRow.modelData.isConnected !== undefined ? Boolean(robotRow.modelData.isConnected) : true
            }
        }
    }

    // 默认测试数据
    property var defaultRobotData: [
        {
            number: 1,
            name: "英雄",
            icon: "qrc:/images/robots/red_hero.png",
            currentHP: 220,
            maxHP: 250,
            chassisPower: 200,
            heatLimit: 40,
            heatCooling: 16,
            shootSpeedLimit: 12,
            level:5
        },
        {
            number: 2,
            name: "工程",
            icon: "qrc:/images/robots/red_engineer.png",
            currentHP: 46,
            maxHP: 250,
            chassisPower: 0,
            heatLimit: 0,
            heatCooling: 0,
            shootSpeedLimit: 0,
            level:6
        },
        {
            number: 3,
            name: "步兵",
            icon: "qrc:/images/robots/red_infantry.png",
            currentHP: 50,
            maxHP: 200,
            chassisPower: 45,
            heatLimit: 50,
            heatCooling: 40,
            shootSpeedLimit: 25,
            level:7
        },
        {
            number: 7,
            name: "哨兵",
            icon: "qrc:/images/robots/red_sentry.png",
            currentHP: 180,
            maxHP: 400,
            chassisPower: 100,
            heatLimit: 400,
            heatCooling: 80,
            shootSpeedLimit: 25,
            level:8
        }
    ]
}
