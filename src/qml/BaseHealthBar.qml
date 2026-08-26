/**
 * @file BaseHealthBar.qml
 * @brief 基地血量条 QML 组件
 * @details 按官方样式实现:
 *          - 双层血条 (深层3000 + 浅层2000)
 *          - 扣血追血段
 *          - 无敌金边
 *          - 低血量呼吸暗化
 */

import QtQuick 2.15

Item {
    id: root

    property int currentHealth: 5000
    property int maxHealth: 5000
    property bool isBlue: false
    property bool invincible: false
    property bool showHealthText: true
    property int deepLayerHealth: 3000

    readonly property int safeMaxHealth: Math.max(1, maxHealth)
    readonly property int clampedHealth: Math.max(0, Math.min(safeMaxHealth, currentHealth))

    // 双层容量: 深层3000, 浅层2000
    readonly property int deepCapacity: Math.max(1, Math.min(deepLayerHealth, safeMaxHealth))
    readonly property int lightCapacity: Math.max(0, safeMaxHealth - deepCapacity)

    readonly property int deepHealth: Math.min(clampedHealth, deepCapacity)
    readonly property int lightHealth: Math.max(0, clampedHealth - deepCapacity)

    readonly property real healthRatio: clampedHealth / safeMaxHealth
    readonly property real deepRatio: deepCapacity > 0 ? deepHealth / deepCapacity : 0
    readonly property real lightRatio: lightCapacity > 0 ? lightHealth / lightCapacity : 0
    readonly property bool lowHealth: healthRatio > 0 && healthRatio < 0.2

    // 方向: 红方剩余血量贴近右侧校徽(右锚定), 蓝方剩余血量贴近左侧校徽(左锚定)
    readonly property real deepFillWidth: width * deepRatio
    readonly property real lightFillWidth: lightCapacity > 0 ? width * lightRatio : 0
    readonly property real totalFillWidth: width * healthRatio

    // 资源映射
    readonly property string depletedSource: isBlue ? "qrc:/images/blue_base_blood/bottom.png" : "qrc:/images/red_base_blood/bottom.png"
    readonly property string deepSource: isBlue ? "qrc:/images/blue_base_blood/middle.png" : "qrc:/images/red_base_blood/middle.png"
    readonly property string lightSource: isBlue ? "qrc:/images/blue_base_blood/top.png" : "qrc:/images/red_base_blood/top.png"
    readonly property string borderSource: isBlue ? "qrc:/images/blue_base_blood/border.png" : "qrc:/images/red_base_blood/border.png"
    readonly property string glowSource: isBlue ? "qrc:/images/blue_base_blood/light.png" : "qrc:/images/red_base_blood/light.png"
    readonly property string invincibleSource: isBlue ? "qrc:/images/blue_base_blood/invincible.png" : "qrc:/images/red_base_blood/invincible.png"
    readonly property string deepTrailSource: isBlue ? "qrc:/images/blue_base_blood/middle_full.png" : "qrc:/images/red_base_blood/middle_full.png"
    readonly property string lightTrailSource: isBlue ? "qrc:/images/blue_base_blood/top_full.png" : "qrc:/images/red_base_blood/top_full.png"

    // 扣血追血段: 用 trailHealth 记录动画前值
    property real trailHealth: clampedHealth
    property real lowHealthPulse: 0
    property real invinciblePulse: 0

    readonly property real trailClampedHealth: Math.max(0, Math.min(safeMaxHealth, trailHealth))
    readonly property real trailDeepHealth: Math.min(trailClampedHealth, deepCapacity)
    readonly property real trailLightHealth: Math.max(0, trailClampedHealth - deepCapacity)
    readonly property real trailDeepRatio: deepCapacity > 0 ? trailDeepHealth / deepCapacity : 0
    readonly property real trailLightRatio: lightCapacity > 0 ? trailLightHealth / lightCapacity : 0

    readonly property real trailDeepWidth: width * trailDeepRatio
    readonly property real trailLightWidth: lightCapacity > 0 ? width * trailLightRatio : 0
    readonly property int healthTextInset: 48

    readonly property real deepTrailDelta: Math.max(0, trailDeepWidth - deepFillWidth)
    readonly property real lightTrailDelta: Math.max(0, trailLightWidth - lightFillWidth)

    readonly property real deepTrailX: isBlue ? deepFillWidth : (width - trailDeepWidth)
    readonly property real lightTrailX: isBlue ? lightFillWidth : (width - trailLightWidth)

    onClampedHealthChanged: {
        if (clampedHealth < trailHealth) {
            trailAnim.stop()
            trailAnim.from = trailHealth
            trailAnim.to = clampedHealth
            trailAnim.start()
        } else {
            trailAnim.stop()
            trailHealth = clampedHealth
        }
    }

    Component.onCompleted: {
        trailHealth = clampedHealth
    }

    NumberAnimation {
        id: trailAnim
        target: root
        property: "trailHealth"
        duration: 260
        easing.type: Easing.OutCubic
    }

    SequentialAnimation {
        running: root.lowHealth
        loops: Animation.Infinite
        NumberAnimation { target: root; property: "lowHealthPulse"; to: 1.0; duration: 700; easing.type: Easing.InOutSine }
        NumberAnimation { target: root; property: "lowHealthPulse"; to: 0.0; duration: 700; easing.type: Easing.InOutSine }
    }

    SequentialAnimation {
        running: root.invincible
        loops: Animation.Infinite
        NumberAnimation { target: root; property: "invinciblePulse"; to: 1.0; duration: 560; easing.type: Easing.InOutSine }
        NumberAnimation { target: root; property: "invinciblePulse"; to: 0.0; duration: 560; easing.type: Easing.InOutSine }
    }

    // 缺失血量底图
    Image {
        anchors.fill: parent
        source: root.depletedSource
        fillMode: Image.Stretch
    }

    // 第一层血条(深层3000)
    Item {
        width: root.deepFillWidth
        height: root.height
        clip: true
        anchors.left: root.isBlue ? parent.left : undefined
        anchors.right: root.isBlue ? undefined : parent.right

        Image {
            width: root.width
            height: root.height
            source: root.deepSource
            fillMode: Image.Stretch
            anchors.left: root.isBlue ? parent.left : undefined
            anchors.right: root.isBlue ? undefined : parent.right
        }
    }

    // 第二层血条(浅层2000), 覆盖在深层之上
    Item {
        visible: root.lightCapacity > 0
        width: root.lightFillWidth
        height: root.height
        clip: true
        anchors.left: root.isBlue ? parent.left : undefined
        anchors.right: root.isBlue ? undefined : parent.right

        Image {
            width: root.width
            height: root.height
            source: root.lightSource
            fillMode: Image.Stretch
            anchors.left: root.isBlue ? parent.left : undefined
            anchors.right: root.isBlue ? undefined : parent.right
        }
    }

    // 第一层追血段
    Item {
        visible: root.deepTrailDelta > 0.5
        x: root.deepTrailX
        width: root.deepTrailDelta
        height: root.height
        clip: true
        opacity: 0.65

        Image {
            anchors.fill: parent
            source: root.deepTrailSource
            fillMode: Image.Stretch
        }
    }

    // 第二层追血段
    Item {
        visible: root.lightTrailDelta > 0.5
        x: root.lightTrailX
        width: root.lightTrailDelta
        height: root.height
        clip: true
        opacity: 0.75

        Image {
            anchors.fill: parent
            source: root.lightTrailSource
            fillMode: Image.Stretch
        }
    }

    // 低血量段暗化 + 呼吸
    Item {
        visible: root.lowHealth && root.totalFillWidth > 0.5
        width: root.totalFillWidth
        height: root.height
        clip: true
        x: root.isBlue ? 0 : (root.width - width)
        opacity: 0.2 + root.lowHealthPulse * 0.25

        Rectangle {
            anchors.fill: parent
            color: "#77000000"
        }
    }

    // 边框和微光
    Image {
        anchors.fill: parent
        source: root.borderSource
        fillMode: Image.Stretch
        opacity: 0.95
    }

    Image {
        anchors.fill: parent
        source: root.glowSource
        fillMode: Image.Stretch
        opacity: 0.28
    }

    // 无敌金边
    Image {
        // 按素材可见区对齐:
        // red: x=[44,880), blue: x=[0,836), 可见宽均为836
        readonly property real nativeW: 880.0
        readonly property real nativeVisibleW: 836.0
        readonly property real nativeLeft: root.isBlue ? 0.0 : 44.0
        readonly property real wScale: nativeW / nativeVisibleW
        readonly property real hScale: 1.25
        width: root.width * wScale
        height: root.height * hScale + 2
        x: -nativeLeft / nativeW * width
        y: (root.height - height) * 0.5 - 1
        source: root.invincibleSource
        fillMode: Image.Stretch
        visible: root.invincible
        opacity: 0.58 + root.invinciblePulse * 0.35
    }

    // 血量数字
    Text {
        visible: root.showHealthText
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: root.isBlue ? parent.left : undefined
        anchors.right: root.isBlue ? undefined : parent.right
        anchors.leftMargin: root.healthTextInset
        anchors.rightMargin: root.healthTextInset
        text: root.currentHealth.toString()
        color: "white"
        font.pixelSize: 22
        font.bold: true
        style: Text.Outline
        styleColor: "#000000"
    }
}
