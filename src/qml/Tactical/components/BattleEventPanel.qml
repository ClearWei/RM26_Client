import QtQuick 2.15

HudPanel {
    id: root

    property var events: []

    title: "战场事件"
    accent: "#ffb13b"
    eyebrow: "LATEST"

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 30
        anchors.bottom: parent.bottom
        anchors.margins: 10
        spacing: 7

        Repeater {
            model: root.events || []
            Rectangle {
                id: eventRow
                required property var modelData

                width: parent.width
                height: 28
                radius: 2
                color: Qt.rgba(0, 0, 0, 0.16)
                border.color: Qt.rgba(1, 1, 1, 0.035)

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 8

                    Text {
                        width: 42
                        anchors.verticalCenter: parent.verticalCenter
                        text: eventRow.modelData.time || "--"
                        color: "#8fa6b8"
                        font.pixelSize: 11
                    }


                    Text {
                        width: parent.width - 82
                        anchors.verticalCenter: parent.verticalCenter
                        text: eventRow.modelData.text || ""
                        color: eventRow.modelData.color || "#d9e7ef"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }

    // 空状态 — 面板中央
    Text {
        anchors.centerIn: parent
        visible: !root.events || root.events.length === 0
        text: "暂无信息"
        color: "#5a6e7e"
        font.pixelSize: 13
        font.bold: true
    }
}
