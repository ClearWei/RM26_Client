import QtQuick 2.15

Rectangle {
    id: root
    width: 140
    height: 50
    color: "#CC1A1A1A"
    radius: 5
    border.color: "#5500CED1"
    border.width: 1

    property var customData: ({})
    property real angle: customData.gimbalChassisAngle !== undefined ? customData.gimbalChassisAngle : 0
    property real absAngle: Math.abs(angle)
    visible: customData.gimbalChassisAngle !== undefined

    Column {
        anchors.centerIn: parent
        spacing: 2

        Text {
            text: (root.angle >= 0 ? "+" : "") + root.angle.toFixed(1) + "°"
            color: root.absAngle > 45 ? "#FF4444" : "#00FF00"
            font.pixelSize: 14
            font.bold: true
            font.family: "Courier"
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Text {
            text: "GIMBAL-CHASSIS ANGLE"
            color: "#00CED1"
            font.pixelSize: 9
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }
}
