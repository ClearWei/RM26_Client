import QtQuick 2.15

Item {
    id: root
    width: 92
    height: 52
    clip: true

    readonly property real contentScale: Math.min(width / 92, height / 52)

    property bool isBlue: false
    property int hp: 1500
    property int maxHp: 1500
    property int rebuildCount: 2
    property int maxRebuildCount: 2
    readonly property bool destroyed: hp <= 0
    readonly property real hpRatio: Math.max(0.0, Math.min(1.0, hp / Math.max(1, maxHp)))
    readonly property string teamPrefix: isBlue ? "blue" : "red"
    readonly property string cardBackgroundSource: "qrc:/images/top_robots/" + teamPrefix + "_teammate_bg.png"
    readonly property string barSource: "qrc:/images/top_robots/" + teamPrefix + "_tab_bar.png"
    readonly property string outpostNameSource: isBlue ? "qrc:/images/top_outpost/blue_name.png"
                                                       : "qrc:/images/top_outpost/red_name.png"
    readonly property string outpostIconSource: isBlue ? "qrc:/images/top_outpost/blue_avatar.png"
                                                       : "qrc:/images/top_outpost/red_avatar.png"

    Image {
        id: bg
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        width: Math.round(92 * root.contentScale)
        height: Math.round(46 * root.contentScale)
        source: root.cardBackgroundSource
        fillMode: Image.Stretch
        opacity: root.destroyed ? 0.6 : 1.0
    }

    Image {
        source: root.outpostNameSource
        fillMode: Image.PreserveAspectFit
        width: Math.round(50 * root.contentScale)
        height: Math.round(15 * root.contentScale)
        anchors.left: root.isBlue ? undefined : bg.left
        anchors.right: root.isBlue ? bg.right : undefined
        anchors.leftMargin: Math.round(10 * root.contentScale)
        anchors.rightMargin: Math.round(10 * root.contentScale)
        anchors.top: bg.top
        anchors.topMargin: Math.round(5 * root.contentScale)
        opacity: root.destroyed ? 0.7 : 1.0
    }

    Text {
        id: hpText
        anchors.left: root.isBlue ? undefined : bg.left
        anchors.right: root.isBlue ? bg.right : undefined
        anchors.leftMargin: Math.round(24 * root.contentScale)
        anchors.rightMargin: Math.round(24 * root.contentScale)
        anchors.bottom: hpBar.top
        anchors.bottomMargin: Math.round(2 * root.contentScale)
        width: Math.round(32 * root.contentScale)
        text: root.hp.toString()
        color: root.destroyed ? "#D4D7DC" : "#FFFFFF"
        font.pixelSize: Math.round(13 * root.contentScale)
        font.bold: true
        horizontalAlignment: root.isBlue ? Text.AlignRight : Text.AlignLeft
        elide: Text.ElideRight
        style: Text.Outline
        styleColor: "#101214"
    }

    Image {
        source: root.outpostIconSource
        fillMode: Image.PreserveAspectFit
        width: Math.round(12 * root.contentScale)
        height: Math.round(14 * root.contentScale)
        anchors.left: root.isBlue ? undefined : hpText.right
        anchors.right: root.isBlue ? hpText.left : undefined
        anchors.leftMargin: Math.round(13 * root.contentScale)
        anchors.rightMargin: Math.round(13 * root.contentScale)
        anchors.verticalCenter: hpText.verticalCenter
        anchors.verticalCenterOffset: Math.round(root.contentScale)
        opacity: root.destroyed ? 0.7 : 1.0
    }

    Item {
        id: hpBar
        anchors.bottom: bg.bottom
        anchors.bottomMargin: 0
        anchors.left: root.isBlue ? bg.left : undefined
        anchors.right: root.isBlue ? undefined : bg.right
        anchors.leftMargin: Math.round(3 * root.contentScale)
        anchors.rightMargin: Math.round(3 * root.contentScale)
        width: Math.round(64 * root.contentScale)
        height: Math.round(6 * root.contentScale)

        Image {
            anchors.fill: parent
            source: root.barSource
            fillMode: Image.Stretch
            opacity: root.destroyed ? 0.5 : 0.95
        }

        Rectangle {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width * root.hpRatio
            height: Math.max(2, Math.round(2 * root.contentScale))
            radius: height / 2
            color: root.destroyed ? "#999999" : (root.isBlue ? "#31BAFF" : "#FF2E45")
            opacity: root.destroyed ? 0.6 : 0.95
        }
    }

    // 剩余重建次数
    Item {
        id: rebuildBadge
        anchors.left: root.isBlue ? bg.left : undefined
        anchors.right: root.isBlue ? undefined : bg.right
        anchors.leftMargin: Math.round(4 * root.contentScale)
        anchors.rightMargin: Math.round(4 * root.contentScale)
        anchors.top: bg.top
        anchors.topMargin: Math.round(5 * root.contentScale)
        width: Math.round(28 * root.contentScale)
        height: Math.round(18 * root.contentScale)
        visible: root.destroyed && root.rebuildCount > 0

        Rectangle {
            anchors.fill: parent
            radius: Math.round(4 * root.contentScale)
            color: root.isBlue ? "#2A5F8F" : "#8F2A2A"
            border.color: root.isBlue ? "#4FA8E0" : "#E04F4F"
            border.width: 1
            opacity: 0.85
        }

        Text {
            anchors.centerIn: parent
            text: root.rebuildCount
            color: "#FFFFFF"
            font.pixelSize: Math.round(11 * root.contentScale)
            font.bold: true
        }
    }

    Rectangle {
        anchors.fill: bg
        color: "#44000000"
        visible: root.destroyed
    }
}
