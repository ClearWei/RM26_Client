/**
 * @file SiloControlPanel.qml
 * @brief 飞镖操控面板
 * @details
 * - 固定尺寸：336x255
 * - 背景：silo_background_1
 * - 除“飞镖在线”标题外，其余内容布局在 _1 区域内部
 * - 状态流转：initial -> confirming -> opening -> ready -> fired -> exhausted
 */

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root

    anchors.fill: parent
    property real panelScale: 1
    property int panelLeftMargin: 14
    property int panelTopMargin: 140

    // gameData 由 MainWindow 注入，面板只通过这个入口读取和调用飞镖状态接口。
    // qmllint disable unqualified
    readonly property var gameDataContext: typeof gameData !== "undefined" ? gameData : null
    // qmllint enable unqualified

    // === 数据属性 ===
    property bool useEnglishTargetAsset: false
    property bool outpostTargetAvailable: true
    property int stationCooldownSec: 30         // 发射站冷却时长 (秒)

    property int openedCount: 0                 // 已开启闸门次数
    property int openChanceLimit: 2             // 最大开启次数
    property int readyStateEnterRemainingSec: -1 // 进入ready态时的引擎剩余时间 (秒)
    property int firedStateEnterRemainingSec: -1 // 进入fired态时的引擎剩余时间 (秒)

    property int targetType: 2                  // 目标类型：1前哨站/2基地固定/3基地随机固定/4基地随机移动/5基地末端移动
    property bool targetSwitching: false        // 目标切换动效标记
    property int pendingSwitchTargetType: 0     // 待同步确认的目标类型（0表示无待确认）

    // 面板状态：initial | confirming | opening | ready | fired | exhausted
    property string panelState: "initial"

    // === 显示逻辑 ===
    readonly property int remainingTime: {
        if (!root.gameDataContext)
            return 420
        var value = Number(root.gameDataContext.remainingTime)
        return isFinite(value) ? Math.max(0, Math.floor(value)) : 420
    }

    readonly property int elapsedTime: Math.max(0, 420 - remainingTime)
    readonly property bool isBattleStage: {
        if (!root.gameDataContext)
            return false
        return String(root.gameDataContext.gamePhase) === "战斗阶段"
    }
    readonly property int earnedOpenChance: elapsedTime >= 240 ? 2 : (elapsedTime >= 30 ? 1 : 0)
    readonly property int availableOpenChance: Math.max(0, Math.min(openChanceLimit, earnedOpenChance) - openedCount)
    readonly property bool operationUnlocked: isBattleStage && elapsedTime >= 30
    readonly property bool canRequestOpen: operationUnlocked && panelState === "initial" && availableOpenChance > 0
    readonly property bool canConfirmOpen: operationUnlocked && panelState === "confirming" && availableOpenChance > 0

    readonly property int readyCloseRemainingSec: {
        if (panelState !== "ready" || readyStateEnterRemainingSec < 0)
            return 0
        var elapsed = Math.max(0, readyStateEnterRemainingSec - remainingTime)
        return Math.max(0, stationCooldownSec - elapsed)
    }

    readonly property int cooldownRemainingSec: {
        if (panelState !== "fired" || firedStateEnterRemainingSec < 0)
            return 0
        var elapsed = Math.max(0, firedStateEnterRemainingSec - remainingTime)
        return Math.max(0, stationCooldownSec - elapsed)
    }

    readonly property bool inCooldownState: panelState === "fired" && cooldownRemainingSec > 0
    readonly property bool waitingSecondChance: availableOpenChance <= 0 && earnedOpenChance < openChanceLimit
    readonly property int secondChanceRemainingSec: Math.max(0, remainingTime - 180)
    readonly property bool noMoreChance: panelState === "exhausted" && availableOpenChance <= 0 && !waitingSecondChance
    // 状态文本集中管理
    readonly property var stateTexts: {
        return {
            initial:    { primary: "开启飞镖闸门", secondary: "", key: "F" },
            confirming: { primary: "确认开启", secondary: "", key: "F" },
            opening:    { primary: "闸门运行中", secondary: "飞镖检测模块运行中...", key: "" },
            ready:      { primary: "发射飞镖", secondary: "飞镖检测模块停止运动", key: "L" },
            fired:      { primary: "发射中", secondary: "", key: "" },
            exhausted:  { primary: "次数已耗尽", secondary: "", key: "" }
        }
    }

    readonly property string actionPrimaryText: (stateTexts[panelState] && stateTexts[panelState].primary) ? stateTexts[panelState].primary : stateTexts.initial.primary
    readonly property string actionSecondaryText: (stateTexts[panelState] && stateTexts[panelState].secondary) ? stateTexts[panelState].secondary : ""
    readonly property string actionKeyText: (stateTexts[panelState] && stateTexts[panelState].key) ? stateTexts[panelState].key : ""
    readonly property bool showActionInstruction: panelState === "initial" || panelState === "opening" || panelState === "ready"

    // === 视觉样式参数 ===
    readonly property color titleColor: "#00B37E"
    readonly property string resPrefix: "qrc:/images/silo/"
    readonly property url targetImageSource: root.resPrefix + root.targetImageName()
    readonly property int keycapSize: 36
    readonly property int keycapRadius: 6
    readonly property int rowSpacing: 12
    readonly property int sectionSpacing: 10

    function targetLabelText() {
        switch (targetType) {
        case 1: return "前哨站"
        case 2: return "基地固定目标"
        case 3: return "基地随机固定目标"
        case 4: return "基地随机移动目标"
        case 5: return "基地末端移动目标"
        default: return "基地固定目标"
        }
    }

    function targetImageName() {
        var suffix = useEnglishTargetAsset ? "_en" : ""
        switch (targetType) {
        case 1: return "silo_outpost" + suffix + ".png"
        case 2: return "silo_defaultposition" + suffix + ".png"
        case 3: return "silo_randomconstantposition" + suffix + ".png"
        case 4: return "silo_randommovingposition" + suffix + ".png"
        case 5: return "silo_endmovingposition" + suffix + ".png"
        default: return "silo_defaultposition" + suffix + ".png"
        }
    }

    function enterReadyState() {
        panelState = "ready"
        readyStateEnterRemainingSec = remainingTime
        firedStateEnterRemainingSec = -1
    }

    function enterFiredCooldownState() {
        panelState = "fired"
        firedStateEnterRemainingSec = remainingTime
        readyStateEnterRemainingSec = -1
    }

    function resolveReadyClose() {
        readyStateEnterRemainingSec = -1
        panelState = availableOpenChance > 0 ? "initial" : "exhausted"
    }

    function switchTarget() {
        // J：切换目标
        targetSwitching = true
        pendingSwitchTargetType = 0

        if (root.gameDataContext && typeof root.gameDataContext.selectNextSiloTarget === "function") {
            var backendTarget = root.gameDataContext.selectNextSiloTarget()
            if (typeof backendTarget === "number") {
                var normalized = Math.max(1, Math.min(5, Math.floor(backendTarget)))
                pendingSwitchTargetType = normalized
                return
            }
        }

        // 后端接口不可用时回退到本地切换，避免交互失效。
        var order = [1, 2, 3, 4, 5]
        var currentIndex = order.indexOf(targetType)
        if (currentIndex < 0)
            currentIndex = 0
        targetType = order[(currentIndex + 1) % order.length]
        pendingSwitchTargetType = 0
        targetSwitching = false
    }

    Component.onCompleted: {
        if (root.gameDataContext) {
            if (typeof root.gameDataContext.siloTargetId === "number")
                targetType = Math.max(1, Math.min(5, Math.floor(root.gameDataContext.siloTargetId)))
        }
    }

    Connections {
        target: root.gameDataContext
        ignoreUnknownSignals: true

        function onSiloStatusChanged() {
            if (!root.gameDataContext)
                return

            if (typeof root.gameDataContext.siloTargetId === "number") {
                var syncedTarget = Math.max(1, Math.min(5, Math.floor(root.gameDataContext.siloTargetId)))
                if (!root.targetSwitching) {
                    root.targetType = syncedTarget
                } else if (root.pendingSwitchTargetType <= 0 || syncedTarget === root.pendingSwitchTargetType) {
                    root.targetType = syncedTarget
                    root.pendingSwitchTargetType = 0
                    root.targetSwitching = false
                } else {
                    // 收到后端同步即结束“切换中”，并以后端状态为准。
                    root.targetType = syncedTarget
                    root.pendingSwitchTargetType = 0
                    root.targetSwitching = false
                }
            }

            if (typeof root.gameDataContext.siloGateState !== "number")
                return

            var gateState = Math.max(0, Math.min(2, Math.floor(root.gameDataContext.siloGateState)))
            if (gateState === 0) {
                if (root.panelState === "ready") {
                    root.readyStateEnterRemainingSec = -1
                    root.panelState = root.availableOpenChance > 0 ? "initial" : "exhausted"
                }
            } else if (gateState === 1) {
                // 协议语义：open=1 为“开启中”。
                if (root.panelState !== "ready" && root.panelState !== "fired")
                    root.panelState = "opening"
            } else if (gateState === 2) {
                // 协议语义：open=2 为“已开启”，进入可发射态。
                if (root.panelState !== "fired") {
                    if (root.panelState !== "ready")
                        root.openedCount = Math.min(root.openChanceLimit, root.openedCount + 1)
                    root.enterReadyState()
                }
            }
        }
    }

    function requestOpen() {
        // F：请求开启闸门（进入二次确认）
        if (!canRequestOpen) {
            if (operationUnlocked && availableOpenChance <= 0)
                panelState = "exhausted"
            return
        }
        panelState = "confirming"
    }

    function cancelConfirm() {
        panelState = "initial"
    }

    function confirmOpen() {
        // Y：确认开启闸门
        if (!canConfirmOpen)
            return

        if (root.gameDataContext && typeof root.gameDataContext.requestSiloOpen === "function") {
            var accepted = root.gameDataContext.requestSiloOpen()
            if (accepted === false)
                return
        }

        panelState = "opening"
    }

    function requestFire() {
        // L：发射飞镖
        if (panelState !== "ready")
            return

        if (root.gameDataContext && typeof root.gameDataContext.requestSiloFire === "function") {
            var ok = root.gameDataContext.requestSiloFire()
            if (ok === false)
                return
        }
    }

    onRemainingTimeChanged: {
        // 非战斗阶段统一复位（每次新开比赛时重置开启次数）
        if (!isBattleStage) {
            openedCount = 0
            readyStateEnterRemainingSec = -1
            firedStateEnterRemainingSec = -1
            panelState = "initial"
            return
        }

        // 未解锁操作期时，强制回到初始态
        if (!operationUnlocked && panelState !== "initial") {
            readyStateEnterRemainingSec = -1
            firedStateEnterRemainingSec = -1
            panelState = "initial"
        }

        if (panelState === "ready" && readyCloseRemainingSec <= 0)
            resolveReadyClose()

        if (panelState === "fired" && cooldownRemainingSec <= 0) {
            firedStateEnterRemainingSec = -1
            panelState = availableOpenChance > 0 ? "initial" : "exhausted"
        }

        if (panelState === "exhausted" && availableOpenChance > 0)
            panelState = "initial"
    }

    Item {
        id: panelFrame
        width: 336
        height: 315
        anchors.left: parent.left
        anchors.leftMargin: root.panelLeftMargin
        anchors.top: parent.top
        anchors.topMargin: root.panelTopMargin
        transform: Scale {
            origin.x: 0
            origin.y: 0
            xScale: root.panelScale
            yScale: root.panelScale
        }

        // === 标题层 ===
        Image {
            id: titleBackground
            width: 336
            height: 60
            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            source: root.resPrefix + "silo_dartstate_background.png"
            z: 2
        }

        Text {
                id: titleText
            text: (root.gameDataContext && root.gameDataContext.siloOnline) ? "飞镖在线" : "飞镖未上场"
            color: (root.gameDataContext && root.gameDataContext.siloOnline) ? root.titleColor : "#FF4D4F"
            font.bold: true
            font.pixelSize: 18
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: titleBackground.verticalCenter
            z: 3
        }

        // === 主内容区 ===
        Item {
            id: contentArea
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            anchors.topMargin: 52
            anchors.bottomMargin: 12
            visible: (root.gameDataContext ? !!root.gameDataContext.siloOnline : false) && !(root.elapsedTime < 30)
            clip: true
            z: 2

            // === 背景层 ===
            Image {
                id: bgLayerBottom
                anchors.fill: parent
                source: root.resPrefix + "silo_background_1.png"
                fillMode: Image.Stretch
            }

            // 主内容排版
            Column {
                id: contentColumn
                anchors.fill: parent
                anchors.margins: 10
                spacing: root.sectionSpacing

            // 目标切换行（J）
            RowLayout {
                width: parent.width
                spacing: root.rowSpacing

                Rectangle {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: root.keycapSize
                    Layout.preferredHeight: root.keycapSize
                    radius: root.keycapRadius
                    color: root.targetSwitching ? "#1B2D3A" : "#101317"
                    border.color: root.targetSwitching ? "#6EA7A2" : "#26313A"
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "J"
                        color: "#D8DEE9"
                        font.bold: true
                        font.pixelSize: 18
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignVCenter
                    text: "切换目标"
                    Layout.preferredHeight: root.keycapSize
                    verticalAlignment: Text.AlignVCenter
                    color: "#D8DEE9"
                    font.pixelSize: 14
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: root.targetSwitching ? "切换中..." : ""
                    color: "#7BA7C7"
                    font.pixelSize: 13
                }
            }

            // 当前目标展示行
            RowLayout {
                width: parent.width
                spacing: root.rowSpacing

                Item {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: root.keycapSize
                    Layout.preferredHeight: root.keycapSize

                    Image {
                        anchors.centerIn: parent
                        source: root.resPrefix + "silo_icon_aim.png"
                        width: 30
                        height: 30
                        fillMode: Image.PreserveAspectFit
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignVCenter
                    text: "当前目标"
                    Layout.preferredHeight: root.keycapSize
                    verticalAlignment: Text.AlignVCenter
                    color: "#D8DEE9"
                    font.pixelSize: 14
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    Layout.preferredWidth: 146
                    Layout.preferredHeight: 30
                    color: "transparent"
                    border.color: "transparent"
                    border.width: 0
                    radius: 3

                    Image {
                        id: targetBadge
                        anchors.centerIn: parent
                        width: parent.width
                        height: parent.height
                        source: root.targetImageSource
                        fillMode: Image.PreserveAspectFit
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 1
                color: "#2A2E33"
            }

            // 发射控制区
            Column {
                width: parent.width
                spacing: 8

                // opening 加载指示
                Row {
                    width: parent.width
                    spacing: root.rowSpacing
                    visible: root.showActionInstruction

                    Item {
                        width: root.keycapSize
                        height: root.keycapSize

                        BusyIndicator {
                            anchors.centerIn: parent
                            width: root.keycapSize
                            height: root.keycapSize
                            visible: root.panelState === "opening"
                            running: visible
                        }

                        Rectangle {
                            anchors.fill: parent
                            visible: root.actionKeyText !== "" && root.panelState !== "opening"
                            radius: root.keycapRadius
                            color: "#101317"
                            border.color: "#26313A"
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: root.actionKeyText
                                color: "#D8DEE9"
                                font.bold: true
                                font.pixelSize: 18
                            }
                        }
                    }

                    Item {
                        width: parent.width - root.keycapSize - root.rowSpacing
                        height: root.actionSecondaryText !== "" ? 52 : root.keycapSize

                        Text {
                            anchors.top: root.actionSecondaryText !== "" ? parent.top : undefined
                            anchors.verticalCenter: root.actionSecondaryText !== "" ? undefined : parent.verticalCenter
                            width: parent.width
                            wrapMode: root.actionSecondaryText !== "" ? Text.WordWrap : Text.NoWrap
                            color: "#D8DEE9"
                            font.pixelSize: 14
                            font.bold: true
                            lineHeight: 1.2
                            lineHeightMode: Text.ProportionalHeight
                            text: root.actionSecondaryText !== "" ? (root.actionPrimaryText + "\n" + root.actionSecondaryText) : root.actionPrimaryText
                        }
                    }
                }

                // exhausted 提示
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    visible: root.noMoreChance
                    text: "飞镖闸门开启次数已达上限"
                    color: "#FF4D4F"
                    font.bold: true
                    font.pixelSize: 12
                }

                // fired 冷却提示
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    visible: root.inCooldownState
                    text: "飞镖发射站冷却中 " + root.cooldownRemainingSec + " 秒"
                    color: "#D8DEE9"
                    font.bold: true
                    font.pixelSize: 13
                }

                // 次数恢复等待提示(当前无开启次数且比赛小于四分钟触发)
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    visible: root.panelState === "exhausted" && root.waitingSecondChance && root.secondChanceRemainingSec > 0
                    text: root.secondChanceRemainingSec + " 秒后可再次开启闸门"
                    color: "#D8DEE9"
                    font.bold: true
                    font.pixelSize: 13
                }

                // confirming 态二次确认
                Row {
                    spacing: 40
                    visible: root.panelState === "confirming"
                    anchors.horizontalCenter: parent.horizontalCenter

                    //Y按键
                    Item {
                        width: yRow.implicitWidth
                        height: yRow.implicitHeight

                        Row {
                            id: yRow
                            spacing: 8

                            Rectangle {
                                width: 34
                                height: 34
                                radius: 4
                                color: "transparent"
                                border.width: 1
                                border.color: "#00B37E"

                                Text {
                                    anchors.centerIn: parent
                                    text: "Y"
                                    color: "#00B37E"
                                    font.bold: true
                                    font.pixelSize: 16
                                }
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "确定"
                                color: "#00B37E"
                                font.bold: true
                                font.pixelSize: 14
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            z: 1
                            onClicked: root.confirmOpen()
                        }
                    }

                    //N按键
                    Item {
                        width: nRow.implicitWidth
                        height: nRow.implicitHeight

                        Row {
                            id: nRow
                            spacing: 8

                            Rectangle {
                                width: 34
                                height: 34
                                radius: 4
                                color: "transparent"
                                border.width: 1
                                border.color: "#E04545"

                                Text {
                                    anchors.centerIn: parent
                                    text: "N"
                                    color: "#E04545"
                                    font.bold: true
                                    font.pixelSize: 16
                                }
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "取消"
                                color: "#E04545"
                                font.bold: true
                                font.pixelSize: 14
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            z: 1
                            onClicked: root.cancelConfirm()
                        }
                    }
                }

                // 发射控制点击区（非 opening/confirming 时可用）
                MouseArea {
                    width: parent.width
                    height: root.showActionInstruction ? 52 : 0
                    enabled: root.panelState !== "opening" && root.panelState !== "confirming" && root.actionKeyText !== ""
                    onClicked: {
                        if (root.actionKeyText === "L")
                            root.requestFire()
                        else if (root.actionKeyText === "F")
                            root.requestOpen()
                    }
                }
            }

                Item { width: 1; height: 1; Layout.fillHeight: true }
            }
        }
    }

    // 按键由 MainWindow::keyPressEvent 统一分发到本组件方法
}
