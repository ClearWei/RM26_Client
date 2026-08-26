import QtQuick 2.15

Item {
    id: root
    anchors.fill: parent

    // gameData 由 MainWindow 注入 QML 上下文，统一在叠加层入口适配。
    // qmllint disable unqualified
    readonly property var gameDataContext: typeof gameData !== "undefined" ? gameData : null
    // qmllint enable unqualified
    property var customData: root.gameDataContext ? root.gameDataContext.myRobotCustomData : ({})
    property bool uiEnabled: root.gameDataContext ? root.gameDataContext.customUIEnabled : false
    visible: root.uiEnabled

    FrictionWheelStatus {
        id: frictionPanel
        anchors.left: parent.left
        anchors.leftMargin: 15
        anchors.top: parent.top
        anchors.topMargin: 15
        customData: root.customData
    }

    ChassisModePanel {
        id: chassisPanel
        anchors.right: parent.right
        anchors.rightMargin: 15
        anchors.top: parent.top
        anchors.topMargin: 15
        customData: root.customData
    }

    SuperCapacitorBar {
        id: capPanel
        anchors.right: parent.right
        anchors.rightMargin: 15
        anchors.verticalCenter: parent.verticalCenter
        customData: root.customData
    }

    GimbalAngleIndicator {
        id: anglePanel
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        customData: root.customData
    }
}
