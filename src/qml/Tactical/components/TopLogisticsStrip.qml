pragma ComponentBehavior: Bound

import QtQuick 2.15

Item {
    id: root

    property var model: ({})
    property bool allyIsBlue: false
    property bool enemyIsBlue: true
    readonly property real uiScale: Math.max(1.0, height / 62)

    readonly property color allyAccent: allyIsBlue ? "#18b9ff" : "#ff3142"
    readonly property color enemyAccent: enemyIsBlue ? "#18b9ff" : "#ff3142"
    readonly property color fortressGroupAccent: root.allyAccent
    readonly property int leftGroupShift: 24
    readonly property int groupGap: 92
    readonly property int cardGap: 8
    readonly property int groupTopPadding: Math.round(10 * uiScale)
    readonly property int groupInnerMargin: 10
    readonly property int sideBadgeWidth: 74
    readonly property int sideBadgeHeight: Math.round(16 * uiScale)
    readonly property int sideBadgeGap: 8
    readonly property int fortressGroupWidth: Math.round(Math.min(560 * uiScale, width * 0.48))
    readonly property int statusGroupWidth: width - fortressGroupWidth - groupGap
    readonly property int smallCardWidth: Math.max(
        92,
        Math.floor((statusGroupWidth - groupInnerMargin * 2 - cardGap * 3) / 4)
    )
    readonly property int fortressCardWidth: Math.max(
        180,
        Math.floor((fortressGroupWidth - groupInnerMargin * 2 - cardGap) / 2)
    )
    readonly property int innerCardHeight: root.height - root.groupTopPadding - 12
    readonly property var centerCards: ([
        { title: "当前经济", value: root.formatInteger(root.model.allyRemainingEconomy), accent: root.allyAccent },
        { title: "累计总经济", value: root.formatInteger(root.model.allyTotalEconomyObtained), accent: root.allyAccent },
        { title: "科技等级", value: String(Number(root.model.allyTechLevel || 0)), accent: root.allyAccent },
        { title: "加密等级", value: String(Number(root.model.allyEncryptionLevel || 0)), accent: root.allyAccent }
    ])

    function formatInteger(value) {
        var normalized = Math.max(0, Number(value || 0))
        var text = Math.round(normalized).toString()
        return text.replace(/\B(?=(\d{3})+(?!\d))/g, ",")
    }

    function mutedText(accent) {
        return Qt.rgba(accent.r, accent.g, accent.b, 0.92)
    }

    HudPanel {
        id: statusGroup
        x: -root.leftGroupShift
        y: root.groupTopPadding
        width: root.statusGroupWidth
        height: root.height - root.groupTopPadding
        accent: root.allyAccent
        panelOpacity: 0.74
        showDiagonalTexture: false
        cutSizeOverride: 12

        Row {
            anchors.left: parent.left
            anchors.leftMargin: root.groupInnerMargin
            anchors.verticalCenter: parent.verticalCenter
            spacing: root.cardGap

            Repeater {
                model: root.centerCards

                HudPanel {
                    id: logisticsCard
                    required property var modelData

                    width: root.smallCardWidth
                    height: root.innerCardHeight
                    accent: logisticsCard.modelData.accent
                    panelOpacity: 0.82
                    showDiagonalTexture: false
                    cutSizeOverride: 8

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 1
                        text: logisticsCard.modelData.title
                        color: "#cfe8f4"
                        font.pixelSize: Math.round(11 * root.uiScale)
                        font.bold: true
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 1
                        text: logisticsCard.modelData.value
                        color: root.mutedText(logisticsCard.modelData.accent)
                        font.pixelSize: Math.round(17 * root.uiScale)
                        font.bold: true
                    }
                }
            }
        }
    }

    HudPanel {
        id: fortressGroup
        x: statusGroup.x + root.statusGroupWidth + root.groupGap
        y: root.groupTopPadding
        width: root.fortressGroupWidth
        height: root.height - root.groupTopPadding
        accent: root.fortressGroupAccent
        panelOpacity: 0.74
        showDiagonalTexture: false
        cutSizeOverride: 12

        Row {
            anchors.left: parent.left
            anchors.leftMargin: root.groupInnerMargin
            anchors.verticalCenter: parent.verticalCenter
            spacing: root.cardGap

            HudPanel {
                width: root.fortressCardWidth
                height: root.innerCardHeight
                accent: root.allyAccent
                panelOpacity: 0.82
                showDiagonalTexture: false
                cutSizeOverride: 8

                Column {
                    anchors.centerIn: parent
                    width: parent.width - Math.round(20 * root.uiScale)
                    spacing: -Math.round(root.uiScale)

                    Text {
                        width: parent.width
                        text: "己方堡垒被对方占领计时"
                        color: "#dff6ff"
                        font.pixelSize: Math.round(11 * root.uiScale)
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        width: parent.width
                        text: String(Number(root.model.allyFortressOccupationSec || 0)) + "s"
                        color: root.mutedText(root.allyAccent)
                        font.pixelSize: Math.round(18 * root.uiScale)
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }

            HudPanel {
                width: root.fortressCardWidth
                height: root.innerCardHeight
                accent: root.enemyAccent
                panelOpacity: 0.82
                showDiagonalTexture: false
                cutSizeOverride: 8

                Column {
                    anchors.centerIn: parent
                    width: parent.width - Math.round(20 * root.uiScale)
                    spacing: -Math.round(root.uiScale)

                    Text {
                        width: parent.width
                        text: "敌方堡垒被己方占领计时"
                        color: "#dff6ff"
                        font.pixelSize: Math.round(11 * root.uiScale)
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        width: parent.width
                        text: String(Number(root.model.enemyFortressOccupationSec || 0)) + "s"
                        color: root.mutedText(root.enemyAccent)
                        font.pixelSize: Math.round(18 * root.uiScale)
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }
    }

    Rectangle {
        x: statusGroup.x - root.sideBadgeWidth - root.sideBadgeGap
        y: statusGroup.y + 6
        width: root.sideBadgeWidth
        height: root.sideBadgeHeight
        radius: 2
        color: Qt.rgba(root.allyAccent.r, root.allyAccent.g, root.allyAccent.b, 0.16)
        border.width: 1
        border.color: Qt.rgba(root.allyAccent.r, root.allyAccent.g, root.allyAccent.b, 0.28)

        Text {
            anchors.centerIn: parent
            text: "己方战况"
            color: "#f4fbff"
            font.pixelSize: Math.round(10 * root.uiScale)
            font.bold: true
        }
    }

    Rectangle {
        x: fortressGroup.x - root.sideBadgeWidth - root.sideBadgeGap
        y: fortressGroup.y + 6
        width: root.sideBadgeWidth
        height: root.sideBadgeHeight
        radius: 2
        color: Qt.rgba(
            root.fortressGroupAccent.r,
            root.fortressGroupAccent.g,
            root.fortressGroupAccent.b,
            0.16
        )
        border.width: 1
        border.color: Qt.rgba(
            root.fortressGroupAccent.r,
            root.fortressGroupAccent.g,
            root.fortressGroupAccent.b,
            0.28
        )

        Text {
            anchors.centerIn: parent
            text: "堡垒状态"
            color: "#f4fbff"
            font.pixelSize: Math.round(10 * root.uiScale)
            font.bold: true
        }
    }
}
