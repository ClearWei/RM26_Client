import QtQuick 2.15
import "components"

Item {
    id: root

    width: 540
    height: 132
    readonly property real uiScale: Math.min(width / 540.0, height / 132.0)

    property string eventTitle: "战术事件预告"
    property string triggerTimeText: "6:30"
    property string countdownPrefix: "还有"
    property string countdownSuffix: "秒"
    property int countdownSeconds: 5
    property string eventLabel: "可开启飞镖闸门"
    property int displayToken: 0

    HudPanel {
        id: panel
        width: 540
        height: 132
        anchors.centerIn: parent
        scale: root.uiScale

        title: root.eventTitle
        accent: "#36d6ff"
        eyebrow: "TIMER"
        panelOpacity: 0.88

        Item {
            anchors.fill: parent
            anchors.margins: 16

            Rectangle {
                anchors.top: parent.top
                anchors.right: parent.right
                width: 74
                height: 24
                radius: 2
                color: Qt.rgba(0.11, 0.57, 0.79, 0.13)
                border.color: "#36d6ff"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: root.triggerTimeText
                    color: "#dff8ff"
                    font.pixelSize: 12
                    font.bold: true
                }
            }

            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                Text {
                    width: panel.width - 48
                    textFormat: Text.RichText
                    text: root.countdownPrefix + " <b>" + root.countdownSeconds + "</b> " + root.countdownSuffix
                    color: "#f4fbff"
                    font.pixelSize: 30
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    width: panel.width - 48
                    text: root.eventLabel
                    color: "#d4e8f2"
                    font.pixelSize: 20
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }
            }
        }
    }
}
