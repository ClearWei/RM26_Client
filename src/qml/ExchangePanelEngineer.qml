/**
 * ExchangePanelEngineer.qml
 * @brief 工程机器人 H 键装配能量单元面板
 * @details 状态与指令按官方 V2.0.0 TechCoreMotionStateSync / AssemblyCommand 驱动。
 */
import QtQuick
import QtQuick.Layouts
pragma ComponentBehavior: Bound

Rectangle {
    id: root
    anchors.fill: parent
    color: "transparent"

 //---------------------------- 信号定义 ----------------------------
    signal closed()
    signal exchangeRequested(int level)
    signal exchangeValue(int level, int value)
    signal ammoExchangeSucceeded(int level)

    //选择难度与当前装配状态
    property bool isVisible: true
    property int selectedIndex: 1
    property string viewState: "selection"
    property bool assemblyFlowStarted: false
    property int lastBasicState: 0
    // gameData 与 network 由 MainWindow 注入 QML 上下文，统一在面板入口适配。
    // qmllint disable unqualified
    readonly property var gameDataContext: typeof gameData !== "undefined" ? gameData : null
    readonly property var networkContext: typeof network !== "undefined" ? network : null
    // qmllint enable unqualified

    // 协议绑定变量
    readonly property real maxExchangeTime: 45.0
    readonly property int engineerPer10sReward: root.gameDataContext
        ? root.gameDataContext.engineerPer10sReward : 0


    // 协议 V2.0.0: TechCoreMotionStateSync 八个官方字段
    readonly property int maximumDifficultyLevel: root.gameDataContext
        ? root.gameDataContext.maximumDifficultyLevel : 0
    readonly property int basicState: root.gameDataContext
        ? root.gameDataContext.techCoreBasicState : 0
    readonly property int putinState: root.gameDataContext
        ? root.gameDataContext.techCorePutinState : 0
    readonly property int moveState: root.gameDataContext
        ? root.gameDataContext.techCoreMoveState : 0
    readonly property int rotateState: root.gameDataContext
        ? root.gameDataContext.techCoreRotateState : 0
    readonly property int enemyStatus: root.gameDataContext
        ? root.gameDataContext.enemyStatus : 0
    readonly property int remainTimeAll: root.gameDataContext
        ? root.gameDataContext.remainTimeAll : 0
    readonly property int remainTimeStep: root.gameDataContext
        ? root.gameDataContext.remainTimeStep : 0
    readonly property bool enemyLv4Requested: root.enemyStatus === 2
    readonly property int requiredStepCount: Math.min(root.selectedIndex, 3)
    readonly property int completedStepCount: root.putinState + root.moveState + root.rotateState
    readonly property bool allRequiredStepsComplete: root.completedStepCount >= root.requiredStepCount

    readonly property int assemblyCountdownSeconds: remainTimeAll
    property int lv4RemainingSeconds: 15  // Lv4请求倒计时剩余秒数
    readonly property int lv4RequestCountdownSeconds: lv4RemainingSeconds  // 绑定到倒计时变量
    readonly property real lv4RequestCountdownMaxSeconds: 15.0


    // basic_state=3 表示科技核心已到达装配位置，步骤完成情况由三个独立状态字段决定。
    readonly property bool assemblyReady: root.basicState === 3 && !root.allRequiredStepsComplete

    //失败文本显示（根据协议字段计算失败原因，UI 上红色显示）
    readonly property string failtext: {
        switch (root.result) {
        case "pullout": return "装配失败\n能量单元拔出"
        case "timeout": return "装配失败\n装配超时"
        case "leave_area": return "装配失败\n离开装配区域过久"
        case "engineer_dead": return "装配失败\n机器人阵亡"
        case "5s_timeout": return "装配失败\n四级难度未满足完成协作时限"
        case "abort": return "装配失败\n主动退出装配"
        case "no_unit": return "装配失败\n完成装配但结算时未检测到能量单元"
        case "buffer_expired": return "缓冲期到期\n装配流程强制结束"
        }
        return "装配失败\n完成装配但结算时未检测到能量单元"
    }

    //装配结果判断（根据协议字段计算装配结果，UI 上显示成功或失败界面）
    readonly property string result: ""

    //基础大小属性
    readonly property real scaleUnit: Math.max(0.72, Math.min(width / 1530, height / 910))
    readonly property int mainPanelWidth: Math.round(500 * scaleUnit)
    readonly property int mainPanelHeight: Math.round(370 * 1.1 * scaleUnit)
    readonly property int flowPanelWidth: Math.round(500 * scaleUnit)
    readonly property int flowPanelHeight: Math.round(250 * scaleUnit)
    readonly property int cardRadius: Math.round(8 * scaleUnit)
    readonly property int smallFont: Math.round(13 * scaleUnit)
    readonly property int bodyFont: Math.round(15 * scaleUnit)
    readonly property int titleFont: Math.round(19 * scaleUnit)

    readonly property var levelRows: [
        { key: "1", name: "一级", first: "50金币/10s", repeat: "5金币/10s" },
        { key: "2", name: "二级", first: "25金币/10s", repeat: "10金币/10s" },
        { key: "3", name: "三级", first: "25金币/10s", repeat: "15金币/10s" },
        { key: "4", name: "四级", first: "50金币/10s", repeat: "仅限完成一次" }
    ]

    function levelName(level) {
        return ["", "一级难度", "二级难度", "三级难度", "四级难度"][level] || "一级难度"
    }


    function resultFromCode(code) {
        if (code === 0) return "success"
        if (code === 1) return "pullout"
        if (code === 2) return "timeout"
        if (code === 3) return "leave_area"
        if (code === 4) return "engineer_dead"
        if (code === 5) return "5s_timeout"
        if (code === 6) return "abort"
        if (code === 7) return "no_unit"
        if (code === 8) return "buffer_expired"
        return ""
    }

    //是否能选择
    function isUnlocked(level) {
        return level <= maximumDifficultyLevel
    }

    //返回选择界面
    function resetToSelection() {
        root.assemblyFlowStarted = false
        lv4CountdownTimer.stop()  // 停止Lv4倒计时
        root.lv4RemainingSeconds = 15  // 重置计时器
        closeAllPopups()
        root.viewState = "selection"
        root.isVisible = true
    }

    function syncViewFromTechCoreStatus() {
        root.lastBasicState = root.basicState

        if (root.basicState === 2 || root.basicState === 3)
            root.assemblyFlowStarted = true

        if (!root.assemblyFlowStarted)
            return

        if (root.allRequiredStepsComplete) {
            root.viewState = "success"
            root.isVisible = false
        } else if (root.basicState === 2) {
            root.viewState = "moving"
            root.isVisible = false
        } else if (root.basicState === 3) {
            root.viewState = "assembly"
            root.isVisible = false
        } else if (root.basicState === 1 && root.viewState !== "selection") {
            root.resetToSelection()
        }
    }

    //关闭所有非选择界面弹窗
    function closeAllPopups() {
        root.isVisible = root.viewState === "selection"
    }

    function confirmAssemblySelection() {
        if (!isUnlocked(root.selectedIndex))
            return
        // V2.0.0 operation=0：选择难度后开始兑换。
        root.sendAssemblyCommandToServer(0)
        root.assemblyFlowStarted = true
    }


    //------------------------- 发送MQTT命令接口 -------------------------
    function sendAssemblyCommandToServer(operation) {
        if (root.networkContext && root.networkContext.sendAssemblyCommand) {
            console.info("[ExchangePanelEngineer] send AssemblyCommand",
                         "operation=", operation,
                         "difficulty=", root.selectedIndex)
            root.networkContext.sendAssemblyCommand(operation, root.selectedIndex)
        } else {
            console.warn("[ExchangePanelEngineer] network.sendAssemblyCommand not available, drop cmd", operation, root.selectedIndex)
        }
    }

    function handleExitShortcut() {
        if (root.viewState === "selection")
            return false
        root.sendAssemblyCommandToServer(2)
        return true
    }

      //---------------------------- 快捷键定义 ----------------------------
    Shortcut { sequence: "1"; enabled: selectionPanel.visible; onActivated: if (root.isUnlocked(1)) root.selectedIndex = 1 }
    Shortcut { sequence: "2"; enabled: selectionPanel.visible; onActivated: if (root.isUnlocked(2)) root.selectedIndex = 2 }
    Shortcut { sequence: "3"; enabled: selectionPanel.visible; onActivated: if (root.isUnlocked(3)) root.selectedIndex = 3 }
    Shortcut { sequence: "4"; enabled: selectionPanel.visible; onActivated: if (root.isUnlocked(4)) root.selectedIndex = 4 }
    Shortcut { sequence: "Return"; enabled: selectionPanel.visible && root.viewState === "selection"; onActivated: root.confirmAssemblySelection() }
    Shortcut { sequence: "Enter"; enabled: selectionPanel.visible && root.viewState === "selection"; onActivated: root.confirmAssemblySelection() }
    Shortcut {
        sequence: "Y";
        enabled: root.visible && root.viewState === "assembly" && root.assemblyReady;
        onActivated: {
            root.sendAssemblyCommandToServer(1);    // V2.0.0 operation=1：确认当前装配步骤
        }
    }

    Shortcut {
        sequence: "L";
        enabled: root.visible && root.viewState !== "selection";
        onActivated: root.handleExitShortcut()
    }


    //---------------------------- 自动返回计时器 ----------------------------
    Timer {
        id: autoReturnTimer
        interval: 6000  // 6秒
        repeat: false
        onTriggered: {
            root.resetToSelection()
        }
    }

    //---------------------------- 缓冲请求倒计时器 ----------------------------
    Timer {
        id: lv4CountdownTimer
        interval: 1000  // 每秒更新一次
        repeat: true
        onTriggered: {
            if (root.lv4RemainingSeconds > 0) {
                root.lv4RemainingSeconds--
            } else {
                lv4CountdownTimer.stop()
            }
        }
    }

    // 监听enemyLv4Requested状态，自动启动/停止倒计时
    onEnemyLv4RequestedChanged: {
        if (root.enemyLv4Requested) {
            root.lv4RemainingSeconds = 15  // 重置为15秒
            lv4CountdownTimer.restart()
        } else {
            lv4CountdownTimer.stop()
            root.lv4RemainingSeconds = 15  // 重置为初始值
        }
    }

    onBasicStateChanged: root.syncViewFromTechCoreStatus()
    onPutinStateChanged: root.syncViewFromTechCoreStatus()
    onMoveStateChanged: root.syncViewFromTechCoreStatus()
    onRotateStateChanged: root.syncViewFromTechCoreStatus()

    onViewStateChanged: {
        // 在success或no_unit面板显示后启动自动返回计时器
        if ((root.viewState === "success") ||
            (root.viewState === "failure" && root.result === "no_unit")) {
            autoReturnTimer.restart()
        } else {
            autoReturnTimer.stop()
        }
    }

    Component.onCompleted: root.syncViewFromTechCoreStatus()

     //---------------------------- 组件定义 ----------------------------
    // UI 组件（按键）定义
    component KeyCap: Rectangle {
        property string label: ""
        property color borderColor: Qt.rgba(0.55, 0.72, 0.72, 0.75)
        property color fontColor: "#DDEEEE"

        implicitWidth: Math.round(26 * root.scaleUnit)
        implicitHeight: Math.round(22 * root.scaleUnit)
        radius: Math.round(4 * root.scaleUnit)
        color: Qt.rgba(0.20, 0.25, 0.27, 0.75)
        border.color: borderColor
        border.width: 1
        Text {
            anchors.centerIn: parent
            text: parent.label
            color: parent.fontColor
            font.pixelSize: root.smallFont
            font.bold: true
        }
    }

    // 提示条组件定义
    component BottomBar: Rectangle {
        id: bottomBar
        property string leftText: root.levelName(root.selectedIndex)
        property string rightText: "按 L 退出装配"
        height: Math.round(43 * root.scaleUnit)
        color: Qt.rgba(0.18, 0.19, 0.21, 0.58)
        radius: root.cardRadius
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Math.round(38 * root.scaleUnit)
            anchors.rightMargin: Math.round(38 * root.scaleUnit)
            spacing: Math.round(24 * root.scaleUnit)
            Text {
                text: bottomBar.leftText
                color: "#C9CDD2"
                font.pixelSize: root.smallFont
                Layout.fillWidth: true
                horizontalAlignment: bottomBar.rightText !== "" ? Text.AlignLeft : Text.AlignHCenter

            }
            Text {
                text: "|"
                color: "#8B9095"
                font.pixelSize: root.smallFont
                visible: bottomBar.rightText !== ""
            }
            Text {
                text: bottomBar.rightText
                color: "#C9CDD2"
                font.pixelSize: root.smallFont
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                visible: bottomBar.rightText !== ""
            }
        }
    }

    // 基础面板组件定义
    component BasePanel: Rectangle {
        default property alias content: contentHost.data
        width: root.flowPanelWidth
        height: root.flowPanelHeight
        radius: root.cardRadius
        color: Qt.rgba(75 / 255, 77 / 255, 80 / 255, 200 / 255)
        border.color: "#5B6268"
        border.width: 1
        Item {
            id: contentHost
            anchors.fill: parent
        }
    }

    component TimerBadge: Item {
        width: Math.round(48 * root.scaleUnit)
        height: width
        property int seconds: root.assemblyCountdownSeconds
        property real maxSeconds: root.maxExchangeTime
        property color accentColor: "#7DD7C2"
        property color trackColor: "#5E666B"
        property color textColor: "#DDEEEE"

        onSecondsChanged: timerCanvas.requestPaint()
        onMaxSecondsChanged: timerCanvas.requestPaint()
        onAccentColorChanged: timerCanvas.requestPaint()
        onTrackColorChanged: timerCanvas.requestPaint()

        Canvas {
            id: timerCanvas
            anchors.fill: parent
            antialiasing: true
            onPaint: {
                var ctx = getContext("2d")
                var w = width
                var h = height
                var cx = w / 2
                var cy = h / 2
                var radius = Math.min(w, h) / 2 - 4
                var progress = Math.max(0, Math.min(parent.seconds / parent.maxSeconds, 1))
                ctx.clearRect(0, 0, w, h)
                ctx.lineWidth = 3
                ctx.beginPath()
                ctx.arc(cx, cy, radius, 0, Math.PI * 2)
                ctx.strokeStyle = parent.trackColor
                ctx.stroke()
                ctx.beginPath()
                ctx.arc(cx, cy, radius, -Math.PI / 2, -Math.PI / 2 + Math.PI * 2 * progress)
                ctx.strokeStyle = parent.accentColor
                ctx.lineCap = "round"
                ctx.stroke()
            }
        }
        Text {
            anchors.centerIn: parent
            text: parent.seconds + "s"
            color: parent.textColor
            font.pixelSize: root.smallFont
            font.bold: true
        }
    }

    component AssemblyStatusChip: Rectangle {
        id: chipRoot
        property string label: ""
        property bool active: false

        Layout.preferredWidth: Math.round(48 * root.scaleUnit)
        Layout.preferredHeight: Math.round(32 * root.scaleUnit)
        radius: Math.round(4 * root.scaleUnit)
        color: active ? Qt.rgba(0.06, 0.34, 0.25, 0.42)
                      : Qt.rgba(0.35, 0.39, 0.40, 0.30)
        border.width: active ? 1 : 0
        border.color: active ? Qt.rgba(0.18, 1.0, 0.62, 0.84) : "transparent"

        Rectangle {
            anchors.fill: parent
            anchors.margins: -Math.round(3 * root.scaleUnit)
            radius: parent.radius + Math.round(3 * root.scaleUnit)
            color: "transparent"
            border.width: chipRoot.active ? 1 : 0
            border.color: chipRoot.active ? Qt.rgba(0.18, 1.0, 0.62, 0.24)
                                          : Qt.rgba(0.80, 0.88, 0.86, 0.08)
        }

        Text {
            anchors.centerIn: parent
            text: parent.label
            color: parent.active ? "#39F19B" : Qt.rgba(0.82, 0.88, 0.86, 0.32)
            font.pixelSize: root.bodyFont
            font.bold: parent.active
        }
    }

    component AssemblyStatusGrid: ColumnLayout {
        spacing: Math.round(8 * root.scaleUnit)

        GridLayout {
            Layout.alignment: Qt.AlignHCenter
            columns: 4
            rowSpacing: Math.round(4 * root.scaleUnit)
            columnSpacing: Math.round(8 * root.scaleUnit)

            Text {
                text: "红方"
                color: "#F0F2F3"
                font.pixelSize: root.bodyFont
                font.bold: true
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignVCenter
                Layout.preferredWidth: Math.round(44 * root.scaleUnit)
            }
            AssemblyStatusChip { label: "放入"; active: root.putinState === 1 }
            AssemblyStatusChip { label: "平移"; active: root.moveState === 1 }
            AssemblyStatusChip { label: "旋转"; active: root.rotateState === 1 }

            Text {
                text: "蓝方"
                color: "#F0F2F3"
                font.pixelSize: root.bodyFont
                font.bold: true
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignVCenter
                Layout.preferredWidth: Math.round(44 * root.scaleUnit)
            }
            AssemblyStatusChip { label: "放入"; active: root.putinState === 1 }
            AssemblyStatusChip { label: "平移"; active: root.moveState === 1 }
            AssemblyStatusChip { label: "旋转"; active: root.rotateState === 1 }
        }
    }


    // 选难度主面板
    Rectangle {
        id: selectionPanel
        width: root.mainPanelWidth
        height: root.mainPanelHeight
        anchors.centerIn: parent
        visible: root.viewState === "selection"
        radius: root.cardRadius
        color: Qt.rgba(0.08, 0.08, 0.10, 0.92)
        border.color: "#2D3338"
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Math.round(22 * root.scaleUnit)
            spacing: Math.round(16 * root.scaleUnit)

            RowLayout {
                Layout.fillWidth: true
                spacing: Math.round(8 * root.scaleUnit)

                Text {
                    text: "装配难度选择"
                    color: "#F2F5F6"
                    font.pixelSize: root.titleFont
                    font.bold: true
                    Layout.fillWidth: true
                }

                ColumnLayout {
                    Layout.leftMargin: Math.round(10 * root.scaleUnit)
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: Math.round(220 * root.scaleUnit)
                    spacing: 0
                    Text {
                        text: "当前等级上限   " + root.maximumDifficultyLevel
                        color: "#00D5FF"
                        font.pixelSize: root.smallFont
                        font.bold: true
                        horizontalAlignment: Text.AlignLeft
                        Layout.fillWidth: true
                    }
                    Text {
                        text: "当前金币收益   " + root.engineerPer10sReward + "金币/10s"
                        color: "#00D5FF"
                        font.pixelSize: root.smallFont
                        font.bold: true
                        horizontalAlignment: Text.AlignLeft
                        Layout.fillWidth: true
                    }
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: "按Esc键关闭面板"
                    color: "#8B9095"
                    font.pixelSize: root.smallFont
                    Layout.alignment: Qt.AlignVCenter
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Math.round(4 * root.scaleUnit)
                color: "transparent"
                border.color: "#2D3338"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Math.round(16 * root.scaleUnit)
                    spacing: Math.round(8 * root.scaleUnit)

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.round(34 * root.scaleUnit)
                        Text { Layout.preferredWidth: Math.round(68 * root.scaleUnit); text: "按键"; color: "#E7EAEC"; font.pixelSize: root.smallFont; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                        Text { Layout.preferredWidth: Math.round(90 * root.scaleUnit); text: "等级"; color: "#E7EAEC"; font.pixelSize: root.smallFont; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                        Text { Layout.fillWidth: true; text: "首次装配奖励"; color: "#E7EAEC"; font.pixelSize: root.smallFont; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                        Text { Layout.fillWidth: true; text: "非首次装配奖励"; color: "#E7EAEC"; font.pixelSize: root.smallFont; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                    }

                    Repeater {
                        model: root.levelRows
                        delegate: Rectangle {
                            id: levelRow
                            required property int index
                            required property var modelData
                            readonly property bool selected: levelRow.index + 1 === root.selectedIndex
                            readonly property bool locked: !root.isUnlocked(levelRow.index + 1)
                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.round(46 * root.scaleUnit)
                            radius: Math.round(4 * root.scaleUnit)
                            color: levelRow.selected ? Qt.rgba(0.08, 0.32, 0.32, 0.36) : "transparent"
                            opacity: levelRow.locked ? 0.32 : 1.0

                            RowLayout {
                                anchors.fill: parent
                                spacing: 0
                                Item {
                                    Layout.preferredWidth: Math.round(68 * root.scaleUnit)
                                    Layout.fillHeight: true
                                    KeyCap {
                                        anchors.centerIn: parent
                                        label: levelRow.modelData.key
                                    }
                                }
                                Text {
                                    Layout.preferredWidth: Math.round(90 * root.scaleUnit)
                                    text: (levelRow.selected ? "✓  " : "   ") + levelRow.modelData.name
                                    color: levelRow.selected ? "#1FC48C" : "#F0F2F3"
                                    font.pixelSize: root.smallFont
                                    font.bold: levelRow.selected
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                Item {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    RowLayout {
                                        anchors.centerIn: parent
                                        spacing: Math.round(5 * root.scaleUnit)
                                        Image {
                                            Layout.preferredWidth: Math.round(25 * root.scaleUnit)
                                            Layout.preferredHeight: Math.round(25 * root.scaleUnit)
                                            source: "qrc:/images/panel/ic_gold_2x.png"
                                            fillMode: Image.PreserveAspectFit
                                            smooth: true
                                        }
                                        Text {
                                            text: levelRow.modelData.first
                                            color: levelRow.selected ? "#1FC48C" : "#F0F2F3"
                                            font.pixelSize: root.smallFont
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }
                                }
                                Item {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    RowLayout {
                                        anchors.centerIn: parent
                                        spacing: Math.round(5 * root.scaleUnit)
                                        Image {
                                            Layout.preferredWidth: Math.round(25 * root.scaleUnit)
                                            Layout.preferredHeight: Math.round(25 * root.scaleUnit)
                                            source: "qrc:/images/panel/ic_gold_2x.png"
                                            fillMode: Image.PreserveAspectFit
                                            smooth: true
                                        }
                                        Text {
                                            text: levelRow.modelData.repeat
                                            color: levelRow.selected ? "#1FC48C" : "#F0F2F3"
                                            font.pixelSize: root.smallFont
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: if (!levelRow.locked) root.selectedIndex = levelRow.index + 1
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: Math.round(210 * root.scaleUnit)
                Layout.preferredHeight: Math.round(34 * root.scaleUnit)
                radius: Math.round(3 * root.scaleUnit)
                color: Qt.rgba(0, 0.62, 0.91, 0.16)
                border.color: Qt.rgba(0.2, 0.85, 1.0, 0.45)
                Text {
                    anchors.centerIn: parent
                    text: "确定 [Enter]"
                    color: "#EAFBFF"
                    font.pixelSize: root.bodyFont
                    font.bold: true
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.confirmAssemblySelection()
                }
            }
        }
    }

    BasePanel {
        id: movingPanel
        width: Math.round(310 * root.scaleUnit)
        height: Math.round(165 * root.scaleUnit)
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Math.round(12 * root.scaleUnit)
        visible: root.viewState === "moving"
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Math.round(20 * root.scaleUnit)
            spacing: Math.round(10 * root.scaleUnit)
            Item { Layout.fillHeight: true }
            Text {
                Layout.fillWidth: true
                text: "运动中..."
                color: "#ECEFF0"
                font.pixelSize: root.titleFont
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
            }
            Text {
                Layout.fillWidth: true
                text: "前往装配点"
                color: "#ECEFF0"
                font.pixelSize: root.bodyFont
                horizontalAlignment: Text.AlignHCenter
            }
            Item { Layout.fillHeight: true }
            BottomBar {
                Layout.fillWidth: true
                leftText: root.levelName(root.selectedIndex)
                rightText: ""
            }
        }
    }

    BasePanel {
        id: assemblyHeaderPanel
        width: Math.round(310 * root.scaleUnit)
        height: Math.round(48 * root.scaleUnit)
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: assemblyPanel.top
        anchors.bottomMargin: Math.round(1 * root.scaleUnit)
        visible: root.selectedIndex == 4 && (root.viewState === "assembly"|| root.viewState === "failure" )

        RowLayout {
            anchors.centerIn: parent
            spacing: Math.round(10 * root.scaleUnit)

            TimerBadge {
                seconds: root.enemyLv4Requested ? root.lv4RequestCountdownSeconds
                                                : root.assemblyCountdownSeconds
                maxSeconds: root.enemyLv4Requested ? root.lv4RequestCountdownMaxSeconds
                                                   : root.maxExchangeTime
                accentColor: root.enemyLv4Requested ? "#FF725E" : "#7DD7C2"
                textColor: "#DDEEEE"
            }
            Text {
                text: root.enemyLv4Requested ? "请在缓冲结束前取消或完成装配" : "装配倒计时"
                color: root.enemyLv4Requested ? "#FF725E" : "#C9CDD2"
                font.pixelSize: root.smallFont
                font.bold: root.enemyLv4Requested
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    BasePanel {
        id: assemblyPanel
        width: Math.round(310 * root.scaleUnit)
        height: Math.round(165 * root.scaleUnit)
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Math.round(6 * root.scaleUnit)
        visible: root.viewState === "assembly"
        clip: true
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Math.round(8 * root.scaleUnit)
            spacing: Math.round(8 * root.scaleUnit)

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "transparent"

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: Math.round(5 * root.scaleUnit)
                    anchors.verticalCenterOffset: 0

                     Text {
                        Layout.fillWidth: true
                        text: "请装配"
                        color: "#F2F4F5"
                        font.pixelSize: root.titleFont
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: Math.round(8 * root.scaleUnit)
                        visible: root.selectedIndex !== 4
                        Text {
                            text: "放入";
                            color: root.putinState === 1 ? "#1FC48C" : "#E9EDEE";
                            font.pixelSize: root.bodyFont
                            visible: root.selectedIndex >= 1
                            }
                        Text {
                            text: "平移";
                            color: root.moveState === 1 ? "#1FC48C" : "#E9EDEE";
                            font.pixelSize: root.bodyFont
                            visible: root.selectedIndex >= 2
                            }
                        Text {
                            text: "旋转";
                            color: root.rotateState === 1 ? "#1FC48C" : "#E9EDEE";
                            font.pixelSize: root.bodyFont
                            visible: root.selectedIndex >= 3
                            }
                    }

                    AssemblyStatusGrid {
                        Layout.alignment: Qt.AlignHCenter
                        visible: root.selectedIndex === 4
                    }

                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: Math.round(8 * root.scaleUnit)
                        KeyCap {
                            label: "Y"
                            borderColor: root.assemblyReady ? "#1FC48C" : "#E9EDEE"
                            fontColor: root.assemblyReady ? "#1FC48C" : "#DDEEEE"
                        }
                        Text {
                            text: "确定"
                            color: root.assemblyReady ? "#1FC48C" : "#DDEEEE"
                            font.pixelSize: root.bodyFont
                            font.bold: true
                        }
                    }
                }
            }

            BottomBar {
                Layout.fillWidth: true
                leftText: root.levelName(root.selectedIndex)
                rightText: "按 L 退出装配"
            }
        }
    }


    BasePanel {
        id: failurePanel
        width: Math.round(310 * root.scaleUnit)
        height: Math.round(165 * root.scaleUnit)
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Math.round(6 * root.scaleUnit)
        visible: root.viewState === "failure"
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Math.round(18 * root.scaleUnit)
            spacing: Math.round(8 * root.scaleUnit)
            Item { Layout.fillHeight: true }
            Text {
                Layout.fillWidth: true
                text: (root.enemyLv4Requested && root.result === "engineer_dead")
                      ? "已临时激活\n请避让科技后退出激活状态"
                      : root.failtext
                color: "#F06A6A"
                font.pixelSize: root.bodyFont
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
            }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: Math.round(8 * root.scaleUnit)
                visible: root.result !== "no_unit" && root.result !== "success"
                KeyCap {
                    label: "L"
                    borderColor: "#F06A6A"
                    fontColor: "#F06A6A"
                }
                Text { text: "确定"; color: "#F06A6A"; font.pixelSize: root.bodyFont; font.bold: true }
            }
            Item { Layout.fillHeight: true }
            BottomBar {
                Layout.fillWidth: true
                leftText: root.levelName(root.selectedIndex)
                rightText: ""
            }
        }
    }

    BasePanel {
        id: successPanel
        width: Math.round(310 * root.scaleUnit)
        height: Math.round(165 * root.scaleUnit)
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Math.round(12 * root.scaleUnit)
        visible: root.viewState === "success"
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Math.round(18 * root.scaleUnit)
            Item { Layout.fillHeight: true }
            Text {
                Layout.fillWidth: true
                text: root.levelName(root.selectedIndex) + "科技核心装配成功"
                color: "#55E07B"
                font.pixelSize: root.titleFont
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
            }
            Item { Layout.fillHeight: true }
        }
    }
}
