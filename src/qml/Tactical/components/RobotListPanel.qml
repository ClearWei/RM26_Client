pragma ComponentBehavior: Bound

import QtQuick 2.15

HudPanel {
    id: root

    property var robots: []
    property bool enemy: false
    property bool blueTeam: false
    property string emptyText: "等待机器人登录"
    property var pageRoot: null

    function onlineCount() {
        var count = 0
        var list = root.robots || []
        for (var i = 0; i < list.length; ++i) {
            if (list[i].online) {
                count += 1
            }
        }
        return count
    }

    function totalCount() {
        return (root.robots || []).length
    }

    function robotRowHeight(containerHeight) {
        var count = Math.max(1, (root.robots || []).length)
        var spacingTotal = Math.max(0, count - 1) * 4
        return Math.max(52, Math.min(68, Math.floor((containerHeight - spacingTotal) / count)))
    }

    title: (root.enemy ? "敌方机器人" : "我方机器人") + "  (" + onlineCount() + "/" + totalCount() + ")"
    accent: root.blueTeam ? "#21c8ff" : "#ff4b42"
    eyebrow: root.enemy ? "ENEMY UNIT" : "ALLY UNIT"
    showDiagonalTexture: false
    cutSizeOverride: 10

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 38
        anchors.bottom: parent.bottom
        anchors.margins: 9
        spacing: 4

        Repeater {
            model: root.robots || []
            RobotStatusRow {
                required property var modelData

                width: parent.width
                height: root.robotRowHeight(parent.height)
                robot: modelData
                enemy: root.enemy
                blueTeam: root.blueTeam
                pageRoot: root.pageRoot
            }
        }

        Text {
            visible: !root.robots || root.robots.length === 0
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.emptyText
            color: "#7c8793"
            font.pixelSize: 12
        }
    }
}
