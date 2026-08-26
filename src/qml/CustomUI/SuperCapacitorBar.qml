import QtQuick 2.15

Rectangle {
    id: root
    width: 160
    height: 40
    color: "#CC1A1A1A"
    radius: 5
    border.color: "#5500CED1"
    border.width: 1

    property var customData: ({})
    property real energy: customData.superCapEnergy !== undefined ? customData.superCapEnergy : 0
    property real energyPct: Math.min(100, Math.max(0, energy))
    visible: customData.superCapEnergy !== undefined && customData.superCapEnergy > 0

    Column {
        anchors.centerIn: parent
        spacing: 4
        width: parent.width - 16

        Text {
            text: "SUPER CAP"
            color: "#00CED1"
            font.pixelSize: 9
            font.bold: true
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Rectangle {
            width: parent.width
            height: 12
            color: "#333333"
            radius: 6
            anchors.horizontalCenter: parent.horizontalCenter

            Rectangle {
                width: parent.width * (root.energyPct / 100.0)
                height: parent.height
                radius: 6
                color: root.energyPct > 60 ? "#00FF00" : (root.energyPct > 30 ? "#FFFF00" : "#FF0000")
            }

            Text {
                anchors.centerIn: parent
                text: root.energyPct.toFixed(0) + "%"
                color: "#FFFFFF"
                font.pixelSize: 9
                font.bold: true
            }
        }
    }
}
