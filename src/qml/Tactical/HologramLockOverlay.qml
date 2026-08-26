import QtQuick 2.15

// 全息锁定覆盖层 — 叠加在图传区上的 HUD 锁定指示
Item {
    id: root
    property var model: ({})
    property bool active: root.model.hasHologram !== undefined ? root.model.hasHologram : false
    property string targetId: root.model.targetId || ""
    property real lockQuality: root.model.lockQuality || 0
    property string compensationText: root.model.compensationText || ""

    // 全息锁定环 (四角 L 形标线)
    Rectangle {
        anchors.centerIn: parent
        width: 200; height: 140
        color: "transparent"
        visible: root.active

        // 四角 L 形
        Repeater {
            model: [
                { x: 0, y: 0, dx: 20, dy: 2, dx2: 2, dy2: 20 },
                { x: 196, y: 0, dx: 20, dy: 2, dx2: 2, dy2: 20 },
                { x: 0, y: 138, dx: 20, dy: 2, dx2: 2, dy2: -20 },
                { x: 196, y: 138, dx: 20, dy: 2, dx2: 2, dy2: -20 }
            ]
            delegate: Rectangle {
                id: lockCorner
                required property var modelData

                x: lockCorner.modelData.x
                y: lockCorner.modelData.y
                width: lockCorner.modelData.dx; height: lockCorner.modelData.dy
                color: "#00FFCC"
                Rectangle {
                    x: 0; y: 0
                    width: lockCorner.modelData.dx2; height: Math.abs(lockCorner.modelData.dy2)
                    color: "#00FFCC"
                }
            }
        }

        // 锁定品质条
        Rectangle {
            anchors.right: parent.right; anchors.rightMargin: 4
            anchors.top: parent.top; anchors.topMargin: 4
            width: 6; height: 60; radius: 3
            color: "#2200FFCC"; border.color: "#44FFCC"; border.width: 1
            Rectangle {
                anchors.bottom: parent.bottom
                width: 6; height: parent.height * root.lockQuality
                radius: 3
                color: root.lockQuality > 0.7 ? "#00FFCC" : (root.lockQuality > 0.4 ? "#FFAA00" : "#FF4444")
            }
        }

        // 距离/补偿文本
        Text {
            anchors.bottom: parent.bottom; anchors.bottomMargin: -18
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.compensationText
            color: "#00FFCC"; font.pixelSize: 10; font.bold: true
            style: Text.Outline; styleColor: "#000000"
        }
    }

    // 目标 ID 角标
    Rectangle {
        anchors.left: parent.left; anchors.leftMargin: 8
        anchors.top: parent.top; anchors.topMargin: 8
        color: "#CC0A0D14"; radius: 4
        border.color: root.lockQuality > 0.7 ? "#00FFCC" : "#FFAA00"
        border.width: 1
        visible: root.active && root.targetId !== ""
        width: targetLabel.contentWidth + 24; height: 22

        Text {
            id: targetLabel
            anchors.centerIn: parent
            text: "锁定: " + root.targetId
            color: root.lockQuality > 0.7 ? "#00FFCC" : "#FFAA00"
            font.pixelSize: 10; font.bold: true
        }
    }
}
