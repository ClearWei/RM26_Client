import QtQuick 2.15

Rectangle {
    id: root
    color: "#0D1522"
    border.color: "#FFAA00"
    border.width: 2
    radius: 6

    property var model: ({})

    Column {
        anchors.fill: parent; anchors.margins: 10; spacing: 6

        Row {
            spacing: 10
            Rectangle { width: 40; height: 22; radius: 3; color: "#FFAA00"
                Text { anchors.centerIn: parent; text: root.model.priority || "P1"; color: "#000000"; font.pixelSize: 12; font.bold: true }
            }
            Text { text: root.model.title || "待分析"; color: "#FFAA00"; font.pixelSize: 16; font.bold: true }
            Rectangle { width: 60; height: 18; radius: 3; color: "#22FFAA00"
                Text { anchors.centerIn: parent; text: "置信度 " + (root.model.confidence || 0) + "%"; color: "#FFAA00"; font.pixelSize: 9 }
            }
            Text { text: root.model.windowText || ""; color: "#6688AA"; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
        }

        Row {
            spacing: 16
            Text { text: "理由:"; color: "#8899AA"; font.pixelSize: 10 }
            Repeater {
                model: root.model.reasons || []
                delegate: Text {
                    required property var modelData
                    text: "· " + modelData; color: "#AABBCC"; font.pixelSize: 10
                }
            }
        }

        Row {
            spacing: 16
            Text { text: "备选:"; color: "#556677"; font.pixelSize: 10 }
            Repeater {
                model: root.model.fallbackActions || []
                delegate: Text {
                    required property var modelData
                    text: "· " + modelData; color: "#556677"; font.pixelSize: 10
                }
            }
        }
    }
}
