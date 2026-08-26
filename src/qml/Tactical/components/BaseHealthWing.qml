import QtQuick 2.15
import "../.."

Item {
    id: root

    property bool enemy: false
    property int hp: 0
    property int maxHp: 5000
    property int outpostHp: 0
    property int outpostMax: 1500
    property bool outpostDestroyed: outpostHp <= 0
    property bool invincible: false
    property int defenseBonus: 0

    readonly property bool rightSide: enemy
    readonly property real uiScale: Math.max(1.0, height / 80)
    property bool isBlueTeam: rightSide
    readonly property color accent: isBlueTeam ? "#18b9ff" : "#ff2738"
    property string teamName: isBlueTeam ? "蓝方基地" : "红方基地"
    property string logoSource: isBlueTeam
        ? "qrc:/images/top_mid/blue_team_logo.png"
        : "qrc:/images/top_mid/red_team_logo.png"
    property int logoSize: Math.round(72 * uiScale)
    property int logoOverlap: Math.round(28 * uiScale)
    readonly property real outpostScale: 1.2
    readonly property int outpostWidth: Math.round(92 * uiScale * outpostScale)
    readonly property int gap: Math.round(12 * uiScale)
    readonly property int barWidth: width - outpostWidth - gap
    readonly property int barHeight: Math.round(30 * uiScale)
    readonly property int barTop: Math.round(22 * uiScale)
    readonly property bool invincibleEffective: invincible || !outpostDestroyed

    Item {
        id: barWrap
        x: root.rightSide ? 0 : root.outpostWidth + root.gap
        y: root.barTop
        width: root.barWidth
        height: root.barHeight
        BaseHealthBar {
            id: officialBar
            anchors.fill: parent
            currentHealth: root.hp
            maxHealth: root.maxHp
            isBlue: root.isBlueTeam
            invincible: root.invincibleEffective
            showHealthText: false
        }

        Text {
            anchors.left: root.rightSide ? parent.left : undefined
            anchors.right: root.rightSide ? undefined : parent.right
            anchors.bottom: parent.top
            anchors.bottomMargin: -2
            anchors.leftMargin: root.rightSide ? root.logoSize + 10 : 0
            anchors.rightMargin: root.rightSide ? 0 : root.logoSize + 10
            width: Math.min(280, parent.width * 0.56)
            text: root.teamName
            color: "#f5f9ff"
            font.pixelSize: Math.round(17 * root.uiScale)
            font.bold: true
            minimumPixelSize: Math.round(13 * root.uiScale)
            fontSizeMode: Text.HorizontalFit
            horizontalAlignment: root.rightSide ? Text.AlignLeft : Text.AlignRight
            elide: Text.ElideRight
            style: Text.Outline
            styleColor: "#101214"
            z: 4
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: root.rightSide ? parent.left : undefined
            anchors.right: root.rightSide ? undefined : parent.right
            anchors.leftMargin: root.rightSide ? root.logoSize + 24 : 0
            anchors.rightMargin: root.rightSide ? 0 : root.logoSize + 24
            width: 150
            text: root.hp.toString()
            color: "#ffffff"
            font.pixelSize: Math.round(22 * root.uiScale)
            font.bold: true
            horizontalAlignment: root.rightSide ? Text.AlignLeft : Text.AlignRight
            style: Text.Outline
            styleColor: "#000000"
            z: 5
        }

        Row {
            anchors.top: parent.bottom
            anchors.topMargin: 5
            anchors.left: root.rightSide ? undefined : parent.left
            anchors.right: root.rightSide ? parent.right : undefined
            anchors.leftMargin: root.rightSide ? 0 : 20
            anchors.rightMargin: root.rightSide ? 20 : 0
            spacing: 4
            visible: root.invincible || root.defenseBonus > 0
            layoutDirection: root.rightSide ? Qt.RightToLeft : Qt.LeftToRight

            Image {
                source: "qrc:/images/buffs/shield.png"
                width: Math.round(16 * root.uiScale)
                height: width
                fillMode: Image.PreserveAspectFit
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: root.invincible ? "护盾激活" : (root.defenseBonus + "%")
                color: "#eeeeee"
                font.pixelSize: Math.round(10 * root.uiScale)
                font.bold: true
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: root.invincible ? "基地状态" : "防御增益"
                color: "#aaaaaa"
                font.pixelSize: Math.round(10 * root.uiScale)
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    Image {
        id: teamLogo
        width: root.logoSize
        height: root.logoSize
        x: barWrap.x + (root.rightSide
            ? (root.logoOverlap - root.logoSize)
            : (barWrap.width - root.logoOverlap))
        y: barWrap.y + Math.round((barWrap.height - height) / 2)
        source: root.logoSource
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        z: 12
    }

    OutpostStatusBadge {
        id: outpost
        x: root.rightSide ? root.width - root.outpostWidth : 0
        y: root.barTop + Math.round((root.barHeight - height) / 2)
        width: root.outpostWidth
        height: Math.round(52 * root.uiScale * root.outpostScale)
        opacity: root.outpostDestroyed ? 0.55 : 1.0
        isBlue: root.isBlueTeam
        hp: root.outpostHp
        maxHp: root.outpostMax
    }
}
