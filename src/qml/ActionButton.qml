import QtQuick 2.15
import QtQuick.Controls 2.15

Button {
    id: control
    property color baseColor: "white"
    property color textColor: "black"

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.textColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitWidth: 100
        implicitHeight: 40
        color: control.pressed ? Qt.darker(control.baseColor, 1.2) : (control.hovered ? Qt.lighter(control.baseColor, 1.1) : control.baseColor)
        border.color: "#666666"
        border.width: 1
        radius: 4
    }
}
