import QtQuick 2.15

Rectangle {
    id: root
    color: "#0D1522"
    border.color: "#1A2A4A"
    border.width: 1
    radius: 4

    property var model: ([])

    Text {
        anchors.left: parent.left; anchors.leftMargin: 8; anchors.top: parent.top; anchors.topMargin: 4
        text: "目标排序 TOP 4"; color: "#8899AA"; font.pixelSize: 10; font.bold: true
    }

    Column {
        anchors.top: parent.top; anchors.topMargin: 22; anchors.left: parent.left; anchors.leftMargin: 8; anchors.right: parent.right; anchors.rightMargin: 8
        spacing: 6

        Repeater {
            model: Array.isArray(root.model) ? root.model.slice(0, 4) : root.model
            delegate: Rectangle {
                id: targetRow
                required property var modelData

                width: parent.width; height: 56
                color: targetRow.modelData.rank === 1 ? "#11FFAA00" : "transparent"
                border.color: targetRow.modelData.rank === 1 ? "#33FFAA00" : "#0D1522"
                radius: 4

                Row {
                    anchors.fill: parent; anchors.margins: 6; spacing: 8

                    Rectangle { width: 22; height: 22; radius: 11; color: targetRow.modelData.rank === 1 ? "#FFAA00" : "#334455"
                        Text { anchors.centerIn: parent; text: targetRow.modelData.rank; color: targetRow.modelData.rank === 1 ? "#000" : "#8899AA"; font.pixelSize: 12; font.bold: true }
                    }

                    Column {
                        width: 90
                        Text { text: targetRow.modelData.label; color: "#FF4444"; font.pixelSize: 12; font.bold: true }
                        Rectangle { width: 80; height: 4; radius: 2; color: "#1A2A4A"
                            Rectangle { width: parent.width * (targetRow.modelData.hp / targetRow.modelData.maxHp); height: 4; radius: 2; color: targetRow.modelData.hp / targetRow.modelData.maxHp < 0.3 ? "#FF4444" : "#FFAA00" }
                        }
                        Text { text: targetRow.modelData.hp + "/" + targetRow.modelData.maxHp; color: "#8899AA"; font.pixelSize: 9 }
                    }

                    Column {
                        Text { text: "威胁"; color: "#667788"; font.pixelSize: 9 }
                        Text { text: ((targetRow.modelData.threat || 0) * 100).toFixed(0) + "%"; color: targetRow.modelData.threat > 0.7 ? "#FF4444" : "#FFAA00"; font.pixelSize: 13; font.bold: true }
                    }
                }
            }
        }
    }
}
