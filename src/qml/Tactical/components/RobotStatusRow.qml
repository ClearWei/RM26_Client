import QtQuick 2.15

Item {
    id: root

    property var robot: ({})
    property bool enemy: false
    property bool blueTeam: false
    property var pageRoot: null

    height: 52
    opacity: root.robot.online ? (root.robot.stale ? 0.68 : 1.0) : 0.88

    readonly property int hp: Number(root.robot.hp || 0)
    readonly property int maxHp: Math.max(1, Number(root.robot.maxHp || 0))
    readonly property real hpRatio: Math.max(0, Math.min(1, hp / maxHp))
    readonly property bool criticalLowHealth: root.robot.online && root.hp > 0 && root.hpRatio < 0.2
    readonly property bool reviveFlashVisible: Boolean(root.robot.reviveFlash || false)
    readonly property color sideColor: root.blueTeam ? "#21c8ff" : "#ff4b42"
    readonly property string sideName: root.blueTeam ? "blue" : "red"
    readonly property int slotNumber: Number(root.robot.slot || 0)
    readonly property string statusText: root.robot.online
        ? (root.robot.alive ? (root.robot.stale ? "STALE" : "ONLINE") : "DOWN")
        : "OFFLINE"

    function avatarSource() {
        var type = String(root.robot.type || "infantry")
        if (type === "aerial") {
            type = "airplane"
        }
        if (type === "drone") {
            type = "airplane"
        }
        if (type === "infantry") {
            type = "soldier"
        }
        if (type !== "hero" && type !== "engineer" && type !== "soldier"
                && type !== "airplane" && type !== "guard") {
            type = "soldier"
        }
        if (type === "guard") {
            return "qrc:/images/top_robots/" + root.sideName + "_guard_avatar.png"
        }
        return "qrc:/images/top_robots/" + root.sideName + "_teammate_avatar_" + type + ".png"
    }

    function levelSource() {
        var level = Math.max(1, Math.min(10, Number(root.robot.level || root.slotNumber || 1)))
        return "qrc:/images/top_robots/levels/tab_level_" + level + ".png"
    }

    readonly property bool isSelected: root.pageRoot && (
        root.enemy
            ? root.pageRoot.selectedEnemyId === root.robot.id
            : root.pageRoot.selectedRobotId === root.robot.id
    )
    readonly property color hoveredBg: Qt.rgba(
        root.sideColor.r * 0.18,
        root.sideColor.g * 0.18,
        root.sideColor.b * 0.18,
        0.78
    )
    readonly property color selectedBg: Qt.rgba(
        root.sideColor.r * 0.24,
        root.sideColor.g * 0.24,
        root.sideColor.b * 0.24,
        0.85
    )
    readonly property color normalBg: root.robot.online
        ? Qt.rgba(0.012, 0.04, 0.07, 0.78)
        : Qt.rgba(0.035, 0.04, 0.045, 0.78)
    readonly property color selectedBorder: Qt.rgba(
        root.sideColor.r,
        root.sideColor.g,
        root.sideColor.b,
        0.82
    )

    // 记录鼠标悬停高亮状态
    property bool hovered: false

    MouseArea {
        anchors.fill: parent
        hoverEnabled: root.robot.online
        cursorShape: root.robot.online ? Qt.PointingHandCursor : Qt.ArrowCursor
        onEntered: {
            if (root.robot.online) {
                root.hovered = true
            }
        }
        onExited: {
            root.hovered = false
        }
        onClicked: {
            if (root.robot.online && root.pageRoot) {
                if (root.isSelected) {
                    root.pageRoot.clearSelection()
                } else {
                    root.pageRoot.selectRobot(root.robot.id, root.enemy)
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: 4
        color: root.isSelected ? root.selectedBg
            : (root.hovered ? root.hoveredBg : root.normalBg)
        border.width: root.isSelected ? 2 : 1
        border.color: root.isSelected ? root.selectedBorder
            : (root.robot.online ? Qt.rgba(root.sideColor.r, root.sideColor.g, root.sideColor.b, 0.42)
                                 : Qt.rgba(0.32, 0.36, 0.38, 0.42))
    }

    Rectangle {
        id: criticalFlash

        anchors.fill: parent
        radius: 4
        visible: root.criticalLowHealth
        color: "#d80d18"
        opacity: root.criticalLowHealth ? 0.22 : 0

        SequentialAnimation on opacity {
            running: root.criticalLowHealth
            loops: Animation.Infinite
            NumberAnimation { from: 0.12; to: 0.48; duration: 360; easing.type: Easing.InOutQuad }
            NumberAnimation { from: 0.48; to: 0.12; duration: 520; easing.type: Easing.InOutQuad }
        }
    }

    Rectangle {
        id: reviveFlash

        anchors.fill: parent
        radius: 4
        visible: root.reviveFlashVisible
        color: "#16c95b"
        opacity: root.reviveFlashVisible ? 0.22 : 0

        SequentialAnimation on opacity {
            running: root.reviveFlashVisible
            loops: Animation.Infinite
            NumberAnimation { from: 0.10; to: 0.50; duration: 280; easing.type: Easing.InOutQuad }
            NumberAnimation { from: 0.50; to: 0.10; duration: 360; easing.type: Easing.InOutQuad }
        }
    }

    Image {
        anchors.fill: parent
        source: "qrc:/images/top_robots/" + root.sideName + "_teammate_bg.png"
        fillMode: Image.Stretch
        opacity: root.reviveFlashVisible || root.criticalLowHealth ? 0.16
            : (root.robot.online ? 0.26 : 0.22)
    }

    Canvas {
        id: rowFrame
        anchors.fill: parent
        opacity: root.robot.online ? 0.90 : 0.80
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.strokeStyle = "rgba("
                + Math.round(root.sideColor.r * 255) + ", "
                + Math.round(root.sideColor.g * 255) + ", "
                + Math.round(root.sideColor.b * 255) + ", 0.26)"
            ctx.lineWidth = 1
            ctx.beginPath()
            if (root.enemy) {
                ctx.moveTo(5, 1)
                ctx.lineTo(width - 10, 1)
                ctx.lineTo(width - 1, height / 2)
                ctx.lineTo(width - 10, height - 1)
                ctx.lineTo(5, height - 1)
            } else {
                ctx.moveTo(10, 1)
                ctx.lineTo(width - 5, 1)
                ctx.lineTo(width - 5, height - 1)
                ctx.lineTo(10, height - 1)
                ctx.lineTo(1, height / 2)
            }
            ctx.stroke()
        }
    }

    onBlueTeamChanged: rowFrame.requestPaint()
    onEnemyChanged: rowFrame.requestPaint()
    onSideColorChanged: rowFrame.requestPaint()

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 3
        radius: 2
        color: root.sideColor
        opacity: root.robot.online ? 1.0 : 0.85
    }

    Image {
        id: avatar
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        width: 38
        height: 38
        source: root.avatarSource()
        fillMode: Image.PreserveAspectFit
        opacity: root.robot.online ? 0.96 : 0.88
    }

    Image {
        anchors.left: avatar.left
        anchors.bottom: avatar.bottom
        width: 16
        height: 16
        source: root.levelSource()
        fillMode: Image.PreserveAspectFit
        opacity: root.robot.online ? 0.95 : 0.85
    }

    Text {
        id: slotText
        anchors.left: avatar.right
        anchors.leftMargin: 7
        anchors.top: parent.top
        anchors.topMargin: 7
        text: String(root.robot.slot || "-")
        color: root.robot.online ? "#ffffff" : "#d0d0d0"
        font.pixelSize: 20
        font.bold: true
    }

    Column {
        anchors.left: slotText.right
        anchors.leftMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 3

        Row {
            width: parent.width
            spacing: 6

            Text {
                width: parent.width - hpPercent.width - 12
                text: root.robot.label || "-"
                color: root.robot.online ? "#f3fbff" : "#c4cdd5"
                font.pixelSize: 12
                font.bold: true
                elide: Text.ElideRight
            }

            Text {
                id: hpPercent
                text: root.robot.online ? Math.round(root.hpRatio * 100) + "%" : "--"
                color: root.robot.online ? (root.hpRatio > 0.55 ? "#dfffee" : "#ffc36a") : "#a0a8b0"
                font.pixelSize: 11
                font.bold: true
            }
        }

        Rectangle {
            width: parent.width
            height: 6
            radius: 2
            color: Qt.rgba(1, 1, 1, 0.075)

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width * root.hpRatio
                radius: 2
                color: root.hpRatio > 0.55 ? (root.blueTeam ? "#178cff" : "#ff2d3a")
                    : (root.hpRatio > 0.25 ? "#ffb22e" : "#ff4e4e")
                opacity: root.robot.online ? 1.0 : 0.70
            }
        }

        Row {
            width: parent.width
            spacing: 10
            Text {
                text: root.hp + "/" + (root.robot.maxHp || "-")
                color: "#c8d5df"
                font.pixelSize: 9
            }
            Text {
                text: root.statusText
                color: root.robot.online ? (root.robot.stale ? "#ffbc58" : "#49ff83") : "#a8b4be"
                font.pixelSize: 9
                font.bold: true
            }
        }
    }
}
