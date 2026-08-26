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
        text: "关键事件"; color: "#8899AA"; font.pixelSize: 10; font.bold: true
    }

    Column {
        anchors.top: parent.top; anchors.topMargin: 22; anchors.left: parent.left; anchors.leftMargin: 8; anchors.right: parent.right; anchors.rightMargin: 8
        spacing: 6

        Repeater {
            model: Array.isArray(root.model) ? root.model.slice(0, 3) : root.model
            delegate: Row {
                id: eventRow
                required property var modelData

                spacing: 6
                Rectangle {
                    width: 20; height: 20; radius: 10; color: eventRow.modelData.priority === "P0" ? "#44FF4444" : (eventRow.modelData.priority === "P1" ? "#33FFAA00" : "#228899AA")
                    Text { anchors.centerIn: parent; text: eventRow.modelData.icon || "!"; color: eventRow.modelData.color || "#FF4444"; font.pixelSize: 11; font.bold: true }
                }
                Column {
                    Text { text: eventRow.modelData.text; color: eventRow.modelData.color || "#CCCCCC"; font.pixelSize: 11 }
                    Text { text: eventRow.modelData.time + " · " + eventRow.modelData.priority; color: "#556677"; font.pixelSize: 9 }
                }
            }
        }
    }
}
