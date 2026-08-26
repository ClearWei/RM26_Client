/**
 * @file OfficialEventPopup.qml
 * @brief 空中机器人官方事件提示弹窗
 */

import QtQuick 2.15
import "Tactical/components"

Item {
    id: root

    readonly property real uiScale: Math.min(width / 640.0, height / 180.0)
    width: 640
    height: 180

    property string eventTitle: "EVENT"
    property string eventMessage: ""
    property string borderColor: "#36d6ff"
    property string accentColor: borderColor
    property string eyebrowText: "OFFICIAL EVENT"
    property int displayToken: 0

    HudPanel {
        id: panel
        width: 640
        height: 180
        scale: root.uiScale
        transformOrigin: Item.TopLeft

        title: root.eventTitle
        accent: root.accentColor
        eyebrow: root.eyebrowText
        panelOpacity: 0.90

        Item {
            anchors.fill: parent
            anchors.margins: 18

            Rectangle {
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                width: 170
                height: 26
                radius: 2
                color: Qt.rgba(0.11, 0.57, 0.79, 0.13)
                border.color: root.borderColor
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "战术事件提示"
                    color: "#dff8ff"
                    font.pixelSize: 12
                    font.bold: true
                }
            }

            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 10

                Text {
                    width: parent.width
                    text: root.eventTitle
                    color: "#f2fbff"
                    font.pixelSize: 22
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    text: root.eventMessage
                    color: "#d5eaf4"
                    font.pixelSize: 18
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }
            }
        }
    }
}
