import QtQuick 2.15

Rectangle {
    id: root
    width: 140
    height: 90
    color: "#CC1A1A1A"
    radius: 5
    border.color: "#55FF4444"
    border.width: 1

    property var customData: ({})
    property int chassisMode: customData.chassisMode !== undefined ? customData.chassisMode : -1
    property bool followMode: customData.chassisMode === 1
    property bool spinMode: customData.chassisMode === 2
    property bool protectMode: customData.chassisMode === 3
    visible: customData.chassisMode !== undefined

    function modeName(mode) {
        switch(mode) {
            case 0: return "NORMAL"
            case 1: return "FOLLOW"
            case 2: return "SPIN"
            case 3: return "PROTECT"
            default: return "--"
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: 5

        Text {
            text: "CHASSIS MODE"
            color: "#FF4444"
            font.pixelSize: 10
            font.bold: true
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Text {
            text: root.modeName(root.chassisMode)
            color: root.chassisMode === 0 ? "#888888"
                 : root.chassisMode === 3 ? "#FF4444"
                 : "#00FF00"
            font.pixelSize: 16
            font.bold: true
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }
}
