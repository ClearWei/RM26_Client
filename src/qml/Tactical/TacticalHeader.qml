import QtQuick 2.15

Rectangle {
    id: root
    color: "#111622"
    border.color: "#1A2A4A"
    border.width: 1

    property var model: ({})

    Row {
        anchors.centerIn: parent
        spacing: 24

        Text { text: "FDU EGA"; color: "#4488CC"; font.pixelSize: 14; font.bold: true; anchors.verticalCenter: parent.verticalCenter }

        Rectangle { width: 1; height: 20; color: "#1A2A4A" }

        Text { text: "模式: " + (root.model.stage || "BATTLE"); color: "#8899AA"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }

        Rectangle { width: 1; height: 20; color: "#1A2A4A" }

        Text { text: "红 " + (root.model.redScore || 0); color: "#FF4444"; font.pixelSize: 16; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
        Text { text: ":"; color: "#8899AA"; font.pixelSize: 14; anchors.verticalCenter: parent.verticalCenter }
        Text { text: (root.model.blueScore || 0) + " 蓝"; color: "#4488FF"; font.pixelSize: 16; font.bold: true; anchors.verticalCenter: parent.verticalCenter }

        Rectangle { width: 1; height: 20; color: "#1A2A4A" }

        Text { text: root.model.timeRemaining || "4:00"; color: "#CCCCCC"; font.pixelSize: 18; font.bold: true; anchors.verticalCenter: parent.verticalCenter }

        Rectangle { width: 1; height: 20; color: "#1A2A4A" }

        Row {
            spacing: 4; anchors.verticalCenter: parent.verticalCenter
            Rectangle { width: 8; height: 8; radius: 4; color: root.model.linkLatency < 50 ? "#44FF44" : "#FFAA00"; anchors.verticalCenter: parent.verticalCenter }
            Text { text: root.model.linkStatus || "链路 OK"; color: "#6688AA"; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
        }
    }
}
