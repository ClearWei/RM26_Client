import QtQuick 2.15

Rectangle {
    id: root
    width: 130
    height: 60
    color: "#CC1A1A1A"
    radius: 5
    border.color: "#55FF4444"
    border.width: 1

    property var customData: ({})
    property bool fricOn: customData.fricEnabled !== undefined ? customData.fricEnabled : false
    property bool rammerOn: customData.rammerEnabled !== undefined ? customData.rammerEnabled : false
    visible: customData.fricEnabled !== undefined

    Column {
        anchors.centerIn: parent
        spacing: 4

        Text {
            text: "FRICTION STATUS"
            color: "#FF4444"
            font.pixelSize: 10
            font.bold: true
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 12

            Row {
                spacing: 4
                Text { text: "FRIC"; color: "#00CED1"; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
                Rectangle {
                    width: 10; height: 10; radius: 5; anchors.verticalCenter: parent.verticalCenter
                    color: root.fricOn ? "#00FF00" : "#555555"
                }
            }

            Row {
                spacing: 4
                Text { text: "RAM"; color: "#00CED1"; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
                Rectangle {
                    width: 10; height: 10; radius: 5; anchors.verticalCenter: parent.verticalCenter
                    color: root.rammerOn ? "#00FF00" : "#555555"
                }
            }
        }
    }
}
