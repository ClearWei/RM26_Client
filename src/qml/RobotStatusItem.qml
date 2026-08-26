/**
 * @file RobotStatusItem.qml
 * @brief 单个机器人状态组件 (用于顶部栏列表)
 * @details 使用 docs/sprites 的顶部机器人卡片风格素材：
 *   - 底板 + 机器人头像 + 编号贴片 + 底部血条
 */

import QtQuick 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    width: 82
    height: 72
    implicitWidth: 82
    implicitHeight: 72

    // 固定卡片尺寸，避免被布局压缩
    Layout.preferredWidth: 82
    Layout.preferredHeight: 72
    Layout.minimumWidth: 82
    Layout.maximumWidth: 82
    Layout.minimumHeight: 72
    Layout.maximumHeight: 72

    // qmllint disable unqualified
    readonly property var gameDataContext: typeof gameData !== "undefined" ? gameData : null
    // qmllint enable unqualified
    property string iconSource: ""
    property string avatarSource: ""
    property int robotId: 1
    property bool isBlue: false
    property int normalizedRobotId: robotId > 100 ? (robotId - 100) : robotId
    property string teamPrefix: isBlue ? "blue" : "red"
    property string cardBackgroundSource: "qrc:/images/top_robots/" + teamPrefix + "_teammate_bg.png"
    property string hpBarSource: "qrc:/images/top_robots/" + teamPrefix + "_tab_bar.png"
    property int levelValue: robotInfo && robotInfo.panelLevel !== undefined ? Number(robotInfo.panelLevel) :
                             (robotInfo && robotInfo.level !== undefined ? Number(robotInfo.level) : 1)
    property int safeLevelValue: Math.max(1, Math.min(10, levelValue))
    property string levelIconSource: "qrc:/images/top_robots/levels/tab_level_" + safeLevelValue + ".png"

    // 动态绑定 GameData 中的机器人状态
    property var robotInfo: root.gameDataContext
        ? root.gameDataContext.getRobotInfo(root.robotId) : null
    property int currentHP: robotInfo && robotInfo.panelHp !== undefined ? Number(robotInfo.panelHp) :
                            (robotInfo ? robotInfo.hp : 100)
    property int maxHP: robotInfo && robotInfo.panelMaxHp !== undefined ? Number(robotInfo.panelMaxHp) :
                        (robotInfo ? robotInfo.maxHp : 100)
    property bool isDead: robotInfo && robotInfo.panelIsDead !== undefined ? Boolean(robotInfo.panelIsDead) :
                          (robotInfo && robotInfo.isDead !== undefined ? Boolean(robotInfo.isDead) : currentHP <= 0)
    property bool isOffline: robotInfo && robotInfo.panelIsOffline !== undefined ? Boolean(robotInfo.panelIsOffline) :
                             (robotInfo && robotInfo.isOffline !== undefined ? Boolean(robotInfo.isOffline) : true)
    readonly property bool isConnected: robotInfo && robotInfo.panelIsConnected !== undefined
                                        ? Boolean(robotInfo.panelIsConnected)
                                        : (robotInfo ? (!Boolean(robotInfo.isOffline) && Boolean(robotInfo.isClientConnected)) : false)
    readonly property real cardOpacity: root.isConnected ? (root.isDead ? 0.65 : 1.0) : 0.56
    readonly property real avatarOpacity: root.isConnected ? (root.isDead ? 0.45 : 1.0) : 0.0

    // 组件加载后记录一次机器人状态，便于排查数据绑定问题
    Component.onCompleted: {
        console.log("[RobotStatusItem] bot=" + robotId
            + " isOffline=" + isOffline
            + " isConnected=" + isConnected
            + " isDead=" + isDead
            + " HP=" + currentHP)
    }
    onIsOfflineChanged: {
        console.log("[RobotStatusItem] bot=" + robotId
            + " isOffline=" + isOffline
            + " isConnected=" + isConnected)
    }

    Connections {
        target: root.gameDataContext
        function onRobotDataUpdated(id) {
            if (id === root.robotId) {
                root.robotInfo = root.gameDataContext.getRobotInfo(root.robotId);
            }
        }
    }

    // 卡片底板
    Image {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        width: 82
        height: 44
        source: root.cardBackgroundSource
        fillMode: Image.PreserveAspectFit
        opacity: root.cardOpacity
    }

    // 机器人头像
    Image {
        id: icon
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        width: 62
        height: 50
        source: root.avatarSource !== "" ? root.avatarSource : root.iconSource
        fillMode: Image.PreserveAspectFit
        opacity: root.avatarOpacity
        visible: root.isConnected
    }

    // 统一数字样式，避免 1-7 大小不一致
    Text {
        anchors.left: root.isBlue ? undefined : parent.left
        anchors.right: root.isBlue ? parent.right : undefined
        anchors.leftMargin: -2
        anchors.rightMargin: 2
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        text: root.normalizedRobotId
        color: root.isConnected ? "#F5F7FA" : "#7D848C"
        font.pixelSize: 15
        font.bold: true
        style: Text.Outline
        styleColor: "#101214"
    }

    // 等级角标（右下角）
    Image {
        anchors.left: root.isBlue ? parent.left : undefined
        anchors.right: root.isBlue ? undefined : parent.right
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        width: 14
        height: 12
        source: root.levelIconSource
        fillMode: Image.PreserveAspectFit
        visible: !root.isDead && root.isConnected
    }

    // 底部血条
    Item {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 2
        anchors.left: root.isBlue ? parent.left : undefined
        anchors.right: root.isBlue ? undefined : parent.right
        anchors.leftMargin: 1
        anchors.rightMargin: 1
        width: 59
        height: 6

        Image {
            anchors.fill: parent
            source: root.hpBarSource
            fillMode: Image.Stretch
            opacity: root.isDead ? 0.35 : (root.isConnected ? 0.95 : 0.45)
        }

        Rectangle {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(0, Math.min(parent.width, parent.width * (root.currentHP / Math.max(1, root.maxHP))))
            height: 3
            radius: 1.5
            color: (!root.isConnected || root.isDead) ? "#777777" : (root.isBlue ? "#2DB5FF" : "#FF2C44")
            opacity: root.isDead ? 0.45 : (root.isConnected ? 0.95 : 0.0)
        }
    }
}
