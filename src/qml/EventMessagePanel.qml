/**
 * @file EventMessagePanel.qml
 * @brief 事件消息提示样式组件
 */

import QtQuick 2.15
import QtQuick.Layouts 1.15

Item {
    id: root

    width: 420
    height: 96
    property string panelMode: "rune"
    // qmllint disable unqualified
    readonly property var gameDataContext: typeof gameData !== "undefined" ? gameData : null
    // qmllint enable unqualified

    // RuneStatusSync
    property var runeStatusData: root.gameDataContext ? root.gameDataContext.runeStatusData : ({})
    property var dartMessageData: root.gameDataContext ? root.gameDataContext.dartMessageData : ({})
    property bool isRedTeam: boolFromMap(runeStatusData, "isRedTeam", true)
    property real averageRings: numberFromMap(runeStatusData, "averageRings", 6.1)
    property int activatedArms: numberFromMap(runeStatusData, "activatedArms", 10)
    property int runeType: numberFromMap(runeStatusData, "type", 0)
    property int totalRings: numberFromMap(runeStatusData, "totalRings", Math.round(averageRings * activatedArms))
    property bool eventIsRedTeam: boolFromMap(dartMessageData, "isRedTeam", isRedTeam)
    property string dartTargetName: stringFromMap(dartMessageData, "targetName", "")
    property bool isDartMode: panelMode === "dart"
    property bool isLaserTargetedMode: panelMode === "laserTargeted"

    // Buff 取攻击/防御/冷却三个增益的level
    property var buffTimedData: root.gameDataContext ? root.gameDataContext.buffTimedData : ({})
    property var attackBuff: buffTimedData.attack ? buffTimedData.attack : emptyBuffData(1)
    property var defenseBuff: buffTimedData.defense ? buffTimedData.defense : emptyBuffData(2)
    property var coolingBuff: buffTimedData.cooling ? buffTimedData.cooling : emptyBuffData(3)

    // 样式控制
    property color teamColor: eventIsRedTeam ? "#F2555A" : "#4A90E2"
    property bool panelVisible: true

    function emptyBuffData(typeId) {
        return ({ type: typeId, level: 0, maxTime: 0, leftTime: 0 })
    }

    function numberFromMap(map, key, fallback) {
        if (!map || map[key] === undefined || map[key] === null)
            return fallback
        var value = Number(map[key])
        return isFinite(value) ? value : fallback
    }

    function stringFromMap(map, key, fallback) {
        if (!map || map[key] === undefined || map[key] === null)
            return fallback
        return String(map[key])
    }

    function boolFromMap(map, key, fallback) {
        if (!map || map[key] === undefined || map[key] === null)
            return fallback
        return Boolean(map[key])
    }

    function runeTypeLabel() {
        return runeType === 1 ? "大能量机关" : "小能量机关"
    }

    Image {
        id: backgroundImage
        anchors.fill: parent
        source: "qrc:/images/panel/msg_bg.png"
        fillMode: Image.Stretch
    }

    Image {
        id: mechanismIcon
        anchors.left: parent.left
        anchors.leftMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        width: 90
        height: 90
        source: "qrc:/images/panel/" + (root.isRedTeam ? "red_rune.png" : "blue_rune.png")
        fillMode: Image.PreserveAspectFit
        smooth: true
        cache: true
        visible: !root.isDartMode && !root.isLaserTargetedMode
    }

    ColumnLayout {
        id: textBlock
        anchors.left: parent.left
        anchors.leftMargin: (root.isDartMode || root.isLaserTargetedMode) ? 28 : 126
        anchors.right: parent.right
        anchors.rightMargin: (root.isDartMode || root.isLaserTargetedMode) ? 28 : 20
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: -1
        spacing: 4

        RowLayout {
            visible: !root.isDartMode && !root.isLaserTargetedMode
            Layout.fillWidth: true
            Layout.preferredHeight: 20
            spacing: 12

            Row {
                Layout.preferredWidth: 120
                spacing: 4

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.isRedTeam ? "红方" : "蓝方"
                    color: root.isRedTeam ? "#F2555A" : "#4A90E2"
                    font.pixelSize: 16
                    font.bold: true
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "总环数" + root.totalRings
                    color: "#E9ECEC"
                    font.pixelSize: 16
                    font.bold: true
                }
            }

            Text {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                text: "激活" + root.runeTypeLabel()
                color: "#24D56F"
                font.pixelSize: 16
                font.bold: true
                elide: Text.ElideRight
            }
        }

        RowLayout {
            visible: root.isDartMode && !root.isLaserTargetedMode
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            spacing: 10

            Text {
                text: root.eventIsRedTeam ? "红方" : "蓝方"
                color: root.teamColor
                font.pixelSize: 18
                font.bold: true
            }

            Text {
                text: "飞镖命中" + root.dartTargetName
                color: "#F3F4F6"
                font.pixelSize: 18
                font.bold: true
            }
        }

        Text {
            visible: root.isLaserTargetedMode
            Layout.fillWidth: true
            text: "我方无人机被照射"
            color: "#F2555A"
            font.pixelSize: 22
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        Text {
            visible: !root.isDartMode && !root.isLaserTargetedMode
            Layout.fillWidth: true
            text: "平均环数/灯臂数 " + root.averageRings.toFixed(1) + " / " + root.activatedArms + "，获" + root.attackBuff.maxTime + "秒的"
            color: "#CAD2D0"
            opacity: 0.9
            font.pixelSize: 12
            font.bold: true
        }

        Text {
            visible: !root.isDartMode && !root.isLaserTargetedMode
            Layout.fillWidth: true
            text: root.attackBuff.level + "%攻击加成，" + root.defenseBuff.level + "%防御加成，" + root.coolingBuff.level + "倍冷却增益"
            color: "#AEB6B4"
            opacity: 0.82
            font.pixelSize: 12
            font.bold: true
        }
    }
}
