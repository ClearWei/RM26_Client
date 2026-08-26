// SPDX-License-Identifier: MIT
/**
 * SettingsPanel.qml
 * @file SettingsPanel.qml
 * @brief P键触发的设置面板
 * @details 按下P键显示，再次按下隐藏。
 *          包含登录、图传、性能、硬件、UI/操作、调试设置六大区块。
 * @author Clear
 * @date 2025-12-13
 * @copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// =============================================================================
// 根容器 - 设置面板主窗口
// =============================================================================
Rectangle {
    id: root
    // -------------------------------------------------------------------------
    // 面板尺寸配置
    // -------------------------------------------------------------------------
    width: 1280
    height: 450

    // -------------------------------------------------------------------------
    // 视觉样式 - 半透明深色背景
    // -------------------------------------------------------------------------
    color: Qt.rgba(0.08, 0.08, 0.12, 0.94)
    radius: root.height * 0.009
    border.color: "#646464"
    border.width: root.height * 0.002

    // -------------------------------------------------------------------------
    // 对外信号 - 用于通知C++端设置变更
    // -------------------------------------------------------------------------
    property bool isVideoConnected: false // 图传连接状态
    property int comboBoxHeight: root.height*0.07 // 统一下拉框高度，避免破坏现有布局结构
    property string selectedRobotType: ""
    property string activeRobotLabel: ""
    property string chassisSelectionText: "初始设置"    //底盘类型文本
    property string shooterSelectionText: "初始设置"    //发射机构类型文本
    readonly property bool hasActiveRobot: activeRobotLabel !== ""
    readonly property bool currentSelectionActive: selectedRobotType !== "" && selectedRobotType === robotSelector.currentText
    readonly property string currentRobotSelection: robotSelector.currentText
    readonly property var shortcutRobotTypes: [
        "R1 - Hero", "R2 - Engineer", "R3 - Standard", "R4 - Standard",
        "R6 - Aerial", "B1 - Hero", "B2 - Engineer", "B3 - Standard",
        "B4 - Standard", "B6 - Aerial"
    ]
    property bool performanceControlsReady: false
    property bool robotShortcutPending: false

    onSelectedRobotTypeChanged: {
        clearRobotShortcutSelection()
        if (robotSelector && robotSelector.syncFromSelectedRobot) {
            robotSelector.syncFromSelectedRobot()
        }
    }

    onVisibleChanged: {
        if (!visible)
            clearRobotShortcutSelection()
    }

    //当外部的底盘选择文本变化时，同步选择框的底盘类型
    onChassisSelectionTextChanged: {
        if (chassisTypeCombo) {
            applyComboTextSilently(chassisTypeCombo, chassisSelectionText)
        }
    }

    onShooterSelectionTextChanged: {
        if (launchTypeCombo) {
            applyComboTextSilently(launchTypeCombo, shooterSelectionText)
        }
    }

    // 避免 ComboBox 初始化 currentText 时误发性能体系指令
    Component.onCompleted: {
        applyComboTextSilently(chassisTypeCombo, chassisSelectionText)
        applyComboTextSilently(launchTypeCombo, shooterSelectionText)
        performanceControlsReady = true
    }

    //同步选择框底盘类型
    function applyComboTextSilently(comboBox, text) {
        if (!comboBox || comboBox.count <= 0)
            return

        var targetText = text && text !== "" ? text : "初始设置"
        if (comboBox.currentText === targetText)
            return

        //在下拉框找到与目标对应的索引
        var nextIndex = -1
        for (var i = 0; i < comboBox.count; ++i) {
            if (comboBox.textAt(i) === targetText) {
                nextIndex = i
                break
            }
        }
        if (nextIndex < 0)
            return

        var wasReady = performanceControlsReady     //避免触发发送MQTT协议
        performanceControlsReady = false
        comboBox.currentIndex = nextIndex       //更新下拉框的状态
        performanceControlsReady = wasReady
    }

    function shooterSelectionValue(text) {
        if (text === "冷却优先")
            return 1
        if (text === "爆发优先")
            return 2
        if (text === "英雄近战优先")
            return 3
        if (text === "英雄远程优先")
            return 4
        return 0
    }

    function chassisSelectionValue(text) {
        if (text === "血量优先")
            return 1
        if (text === "功率优先")
            return 2
        if (text === "英雄近战优先")
            return 3
        if (text === "英雄远程优先")
            return 4
        return 0
    }

    function sentryControlValue(text) {
        return text === "半自动" ? 1 : 0
    }

    function emitPerformanceSelectionIfReady() {
        if (!performanceControlsReady)
            return

        var shooter = shooterSelectionValue(launchTypeCombo.currentText)
        var chassis = chassisSelectionValue(chassisTypeCombo.currentText)
        // 协议层允许 0 表示“初始设置/未选择”，因此只在两项都仍为初始设置时不发送。
        if (shooter === 0 && chassis === 0)
            return

        performanceSelectionChanged(shooter, chassis, sentryControlValue(operationComboBox.currentText))
    }

    // 数字快捷键只改变下拉框的待确认选择；真正登录仍复用现有 loginRequested 链路。
    function selectRobotByShortcut(robotType) {
        robotShortcutPending = false
        if (!robotType || shortcutRobotTypes.indexOf(robotType) < 0)
            return false

        for (var i = 0; i < robotSelector.count; ++i) {
            if (robotSelector.textAt(i) === robotType) {
                robotSelector.currentIndex = i
                robotShortcutPending = true
                return true
            }
        }
        return false
    }

    function confirmRobotShortcutLogin() {
        if (!robotShortcutPending || robotSelector.currentText === "")
            return false

        robotShortcutPending = false
        root.loginRequested(robotSelector.currentText)
        return true
    }

    function clearRobotShortcutSelection() {
        robotShortcutPending = false
    }

    //登录机器人设置
    signal loginRequested(string robotType)
    signal logoutRequested()
    //图传设置
    signal vtChanged(string vtType)
    signal vtDirectionChanged(string direction)
    //性能设置
    signal performanceSelectionChanged(int shooter, int chassis, int sentryControl)
    //硬件/图形设置
    signal sensitivityChanged(int value)
    signal volumeChanged(int value)
    signal bgmChanged(int value)
    signal fpsChanged(string fps)
    //ui/操作设置
    signal uiVisibilityChanged(string type)
    signal crosshairVisibilityChanged(bool visible)
    signal miniMapVisibilityChanged(bool visible)
    signal displayModeChanged(string mode)

    signal closed

    // -------------------------------------------------------------------------
    // 主布局 - 垂直方向排列
    // -------------------------------------------------------------------------
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.height * 0.04
        spacing: root.height * 0.03

        // =====================================================================
        // 顶部栏 - 标题 + 状态信息
        // =====================================================================
        RowLayout {
            Layout.fillWidth: true
            spacing: root.height * 0.02

            Text {
                text: "设置面板"
                color: "white"
                font.pixelSize: root.height * 0.05
                font.bold: true
            }

            Text {
                text: "v2.0"
                color: "#888888"
                font.pixelSize: root.height * 0.03
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                text: "按 P 键关闭面板"
                color: "#888888"
                font.pixelSize: root.height * 0.03
            }
        }

        // =====================================================================
        // 主内容区 - 三列布局
        // =====================================================================
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: root.height * 0.03

            // -----------------------------------------------------------------
            // 左列：登录 + 图传设置
            // -----------------------------------------------------------------
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Qt.rgba(0, 0, 0, 0.3)
                border.color: "#888888"
                border.width: root.height * 0.002
                radius: root.height * 0.009

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: root.height * 0.03
                    spacing: root.height * 0.03

                    // 机器人视角区块标题
                    Text {
                        text: "机器人视角"
                        color: "white"
                        font.bold: true
                        font.pixelSize: root.height * 0.04
                    }

                    // 机器人选择下拉框
                    ComboBox {
                        id: robotSelector
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.comboBoxHeight
                        model: ["R1 - Hero", "R2 - Engineer", "R3 - Standard",
                                "R4 - Standard", "R6 - Aerial", "R7 - Sentry",
                                "B1 - Hero", "B2 - Engineer", "B3 - Standard",
                                "B4 - Standard", "B6 - Aerial", "B7 - Sentry"]

                        function syncFromSelectedRobot() {
                            if (!root.selectedRobotType)
                                return
                            for (var i = 0; i < robotSelector.count; ++i) {
                                if (robotSelector.textAt(i) === root.selectedRobotType) {
                                    robotSelector.currentIndex = i
                                    return
                                }
                            }
                        }

                        Component.onCompleted: syncFromSelectedRobot()
                        onActivated: root.clearRobotShortcutSelection()

                        background: Rectangle {
                            color: "#222222"
                            border.color: "#444444"
                            radius: root.height * 0.007
                        }

                        contentItem: RowLayout{
                            Text {
                            text: robotSelector.displayText
                            color: "white"
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: root.width * 0.008
                            }

                            Item{
                                Layout.fillWidth: true
                            }
                        }
                    }

                     //登录/登出按键
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.height * 0.08
                        radius: root.height * 0.009

                        // 1. 蓝色边框
                        border.color: "#00A0E9"
                        border.width: root.height * 0.002

                        // 2. 中间渐变背景
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: "#1000A0E9" }
                            GradientStop { position: 0.5; color: "#4000A0E9" }
                            GradientStop { position: 1.0; color: "#1000A0E9" }
                        }

                        //3.内部高光描边
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: root.height * 0.002
                            radius: root.height * 0.007
                            color: "transparent"
                            border.color: "#50FFFFFF" // 半透明白/亮蓝
                            border.width: root.height * 0.002
                            opacity: 0.3
                        }

                        //4.鼠标交互
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (root.hasActiveRobot) {
                                    root.logoutRequested()
                                } else {
                                    root.loginRequested(robotSelector.currentText)
                                }
                            }
                        }

                        // 5. 文字
                        Text {
                            text: root.hasActiveRobot ? "退 出" : "登 录"
                            color: "white"
                            font.bold: true
                            font.pixelSize: root.height * 0.03
                            anchors.centerIn: parent
                            style: Text.Outline
                            styleColor: "#80000000"
                        }
                    }

                    // 登录状态指示
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: root.activeRobotLabel !== "" ? "✅ 当前机器人: " + root.activeRobotLabel : "🔴 未选择机器人"
                        color: root.activeRobotLabel !== "" ? "#00FF00" : "red"
                        font.bold: true
                    }

                    // 分隔线
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.height * 0.002
                        color: "#444444"
                    }

                    // 图传区块标题
                    Text {
                        text: " 图传设置"
                        color: "white"
                        font.bold: true
                        font.pixelSize: root.height * 0.04
                    }

                    // 图传源选择
                    ComboBox {
                        id: videoSourceCombo
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.comboBoxHeight
                        model: ["新图传", "外部通道"]
                        onCurrentTextChanged: root.vtChanged(videoSourceCombo.currentText)

                        background: Rectangle {
                            color: "#222222"
                            border.color: "#444444"
                            radius: root.height * 0.007
                        }

                        contentItem: RowLayout{
                            Text {
                            text: videoSourceCombo.displayText
                            color: "white"
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: root.width * 0.008
                            }

                            Item{
                                Layout.fillWidth: true
                            }

                        }
                    }

                    // 图传状态
                    Text {
                        text: root.isVideoConnected ? "✅ 图传接入正常" : "🔴 未检测到图传"
                        color: root.isVideoConnected ? "#00FF00" : "red"
                        font.pixelSize: root.height * 0.03
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Item {
                        Layout.fillHeight: true
                    }
                }
            }
            // -----------------------------------------------------------------
            // 第二列：性能/图传设置
            // -----------------------------------------------------------------
            Rectangle{
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Qt.rgba(0, 0, 0, 0.3)
                border.color: "#646464"
                border.width: root.height * 0.002
                radius: root.height * 0.009

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: root.height * 0.03
                    spacing: root.height * 0.03

                    Text {
                        text: "性能设置"
                        color: "white"
                        font.bold: true
                        font.pixelSize: root.height * 0.04
                    }

                    //底盘类型
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "底盘类型"
                            color: "white"
                            Layout.preferredWidth: root.width * 0.08
                        }
                        Image{
                            Layout.fillWidth: true
                        }
                        ComboBox {
                            id: chassisTypeCombo
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.comboBoxHeight
                            model: ["初始设置", "血量优先", "功率优先","英雄近战优先","英雄远程优先"]
                            onCurrentTextChanged: {
                                if (root.chassisSelectionText !== chassisTypeCombo.currentText)
                                    root.chassisSelectionText = chassisTypeCombo.currentText    //保持底盘类型文本与选择框内一致
                                root.emitPerformanceSelectionIfReady()
                            }
                            background: Rectangle {
                                color: "#222222"
                                radius: root.height * 0.007
                            }
                            contentItem: RowLayout{
                                Text {
                                text: chassisTypeCombo.displayText
                                color: "white"
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: root.width * 0.008
                                }

                            }
                        }
                    }

                    //17mm发射机构类型
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "17mm发射机构类型"
                            color: "white"
                            Layout.preferredWidth: root.width * 0.08
                        }
                        Image{
                            Layout.fillWidth: true
                        }
                        ComboBox {
                            id: launchTypeCombo
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.comboBoxHeight
                            model: ["初始设置", "冷却优先", "爆发优先","英雄近战优先","英雄远程优先"]
                            onCurrentTextChanged: {
                                if (root.shooterSelectionText !== launchTypeCombo.currentText)
                                    root.shooterSelectionText = launchTypeCombo.currentText
                                root.emitPerformanceSelectionIfReady()
                            }
                            background: Rectangle {
                                color: "#222222"
                                radius: root.height * 0.007
                            }
                            contentItem: RowLayout{
                                Text {
                                text: launchTypeCombo.displayText
                                color: "white"
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: root.width * 0.008
                                }


                            }
                        }
                    }

                    // 分隔线
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.height * 0.002
                        color: "#444444"
                    }

                    // 图传区块标题
                    Text {
                        text: " 图传"
                        color: "white"
                        font.bold: true
                        font.pixelSize: root.height * 0.04
                    }

                    //图传方向
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "图传方向"
                            color: "white"
                            Layout.preferredWidth: root.width * 0.08
                        }
                        Image{
                            Layout.fillWidth: true
                        }
                        ComboBox {
                            id: directionCombo
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.comboBoxHeight
                            model: ["正向", "反向"]
                            onCurrentTextChanged: root.vtDirectionChanged(currentText)
                            background: Rectangle {
                                color: "#222222"
                                radius: root.height * 0.007
                            }
                            contentItem: RowLayout{
                                Text {
                                text: directionCombo.displayText
                                color: "#00BFFF"
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: root.width * 0.008
                                }



                            }
                        }
                    }
                }
            }
            // -----------------------------------------------------------------
            // 中列：硬件设置
            // -----------------------------------------------------------------
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Qt.rgba(0, 0, 0, 0.3)
                border.color: "#646464"
                border.width: root.height * 0.002
                radius: root.height * 0.009

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: root.height * 0.03
                    spacing: root.height * 0.03

                    Text {
                        text: "硬件/图形设置"
                        color: "white"
                        font.bold: true
                        font.pixelSize: root.height * 0.04
                    }

                    // 控制灵敏度
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "控制灵敏度"
                            color: "white"
                            Layout.preferredWidth: root.width * 0.08
                        }

                        Item{
                            Layout.fillWidth: true
                        }

                        Text {
                            text: sensitivitySlider.value.toFixed(0)
                            color: "#00BFFF"
                            Layout.preferredWidth: root.width * 0.02
                        }
                    }

                    Slider{
                            id: sensitivitySlider
                            Layout.fillWidth: true
                            from: 0
                            to: 100
                            value: 50
                            onValueChanged: root.sensitivityChanged(value)
                    }

                    // 音量设置
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "音量设置"
                            color: "white"
                            Layout.preferredWidth: root.width * 0.08
                        }

                        Image{
                            Layout.fillWidth: true
                        }

                        Text {
                            text: volumeSlider.value.toFixed(0)
                            color: "#00BFFF"
                            Layout.preferredWidth: root.width * 0.02
                        }
                    }

                    Slider {
                            id: volumeSlider
                            Layout.fillWidth: true
                            from: 0
                            to: 100
                            value: 50
                            onValueChanged: root.volumeChanged(value)
                    }

                    //背景音量设置
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "背景音量设置"
                            color: "white"
                            Layout.preferredWidth: root.width * 0.08
                        }

                        Image{
                            Layout.fillWidth: true
                        }

                        Text {
                            text: bgvolumeSlider.value.toFixed(0)
                            color: "#00BFFF"
                            Layout.preferredWidth: root.width * 0.02
                        }
                    }

                    Slider {
                        id: bgvolumeSlider
                        Layout.fillWidth: true
                        from: 0
                        to: 100
                        value: 50
                        onValueChanged: root.bgmChanged(value)
                    }

                    // 帧率设置
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "帧率设置"
                            color: "#AAAAAA"
                            Layout.preferredWidth: root.width * 0.08
                        }
                        Image{
                            Layout.fillWidth: true
                        }
                        ComboBox {
                            id: fpsCombo
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.comboBoxHeight
                            model: ["60 FPS", "120 FPS", "144 FPS"]
                            onCurrentTextChanged: root.fpsChanged(currentText)
                            background: Rectangle {
                                color: "#222222"
                                radius: root.height * 0.007
                            }
                            contentItem: RowLayout{
                                Text {
                                text: fpsCombo.displayText
                                color: "white"
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: root.width * 0.008
                                }

                             }
                        }
                    }

                }
            }

            // -----------------------------------------------------------------
            // 右列：UI设置 + 调试
            // -----------------------------------------------------------------
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Qt.rgba(0, 0, 0, 0.3)
                border.color: "#646464"
                border.width: root.height * 0.002
                radius: root.height * 0.009

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: root.height * 0.03
                    spacing: root.height * 0.03

                    Text {
                        text: "UI设置/操作设置"
                        color: "white"
                        font.bold: true
                        font.pixelSize: root.height * 0.04
                    }

                    // 自定义UI
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "自定义UI"
                            color: "white"
                            Layout.preferredWidth: root.width * 0.08
                        }
                        Image{
                            Layout.fillWidth: true
                        }
                        ComboBox {
                            id: uiComboBox
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.comboBoxHeight
                            model: ["显示", "隐藏","清除"]
                            onCurrentTextChanged: root.uiVisibilityChanged(currentText)
                            background: Rectangle {
                                color: "#222222"
                                radius: root.height * 0.007
                            }
                            contentItem: RowLayout{
                                Text {
                                text: uiComboBox.displayText
                                color: "#00BFFF"
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: root.width * 0.008
                                }

                            }
                        }
                    }

                    // 准心显示
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "准心显示"
                            color: "white"
                            Layout.preferredWidth: root.width * 0.08
                        }
                        Image{
                            Layout.fillWidth: true
                        }
                        ComboBox {
                            id: crosshair
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.comboBoxHeight
                            model: ["显示", "隐藏"]
                            onCurrentTextChanged: root.crosshairVisibilityChanged(currentText === "显示")
                            background: Rectangle {
                                color: "#222222"
                                radius: root.height * 0.007
                            }
                            contentItem: RowLayout{
                                Text {
                                text: crosshair.displayText
                                color: "#00BFFF"
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: root.width * 0.008
                                }

                            }
                        }
                    }

                    // 小地图设置
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "小地图设置"
                            color: "white"
                            Layout.preferredWidth: root.width * 0.08
                        }
                        Image{
                            Layout.fillWidth: true
                        }
                        ComboBox {
                            id:minimap
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.comboBoxHeight
                            model: ["显示", "隐藏"]
                            onCurrentTextChanged: root.miniMapVisibilityChanged(currentText === "显示")
                            background: Rectangle {
                                color: "#222222"
                                radius: root.height * 0.007
                            }
                            contentItem: RowLayout{
                                Text {
                                text: minimap.displayText
                                color: "#00BFFF"
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: root.width * 0.008
                                }

                            }
                        }
                    }

                    // 操作方式
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "操作方式"
                            color: "white"
                            Layout.preferredWidth: root.width * 0.08
                        }
                        Image{
                            Layout.fillWidth: true
                        }
                        ComboBox {
                            id:operationComboBox
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.comboBoxHeight
                            model: ["自动", "半自动"]
                            onCurrentTextChanged: {
                                root.emitPerformanceSelectionIfReady()
                            }
                            background: Rectangle {
                                color: "#222222"
                                radius: root.height * 0.007
                            }
                            contentItem: RowLayout{
                                Text {
                                text: operationComboBox.displayText
                                color: "#00BFFF"
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: root.width * 0.008
                                }
                            }
                        }
                    }

                    // 显示模式
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "显示模式"
                            color: "white"
                            Layout.preferredWidth: root.width * 0.08
                        }
                        Image{
                            Layout.fillWidth: true
                        }
                        ComboBox {
                            id:displayComboBox
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.comboBoxHeight
                            model: ["全屏", "窗口化"]
                            onCurrentTextChanged: root.displayModeChanged(currentText)
                            background: Rectangle {
                                color: "#222222"
                                radius: root.height * 0.007
                            }
                            contentItem: RowLayout{
                                Text {
                                text: displayComboBox.displayText
                                color: "#00BFFF"
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: root.width * 0.008
                                }

                            }
                        }
                    }

                    // 底部状态栏
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: root.height * 0.02
                        Item{
                            Layout.fillWidth:true
                        }
                        Text {
                            text: "温度: 45°C"
                            color: "#888888"
                            font.pixelSize: root.height * 0.02
                        }
                        Text {
                            text: "|"
                            color: "#444444"
                        }
                        Text {
                            text: "模式: 正赛"
                            color: "#888888"
                            font.pixelSize: root.height * 0.02
                        }
                        Text {
                            text: "|"
                            color: "#444444"
                        }
                        Text {
                            text: "通道: 0"
                            color: "#888888"
                            font.pixelSize: root.height * 0.02
                        }
                        Item{
                            Layout.fillWidth:true
                        }
                    }
                }
            }
        }
    }
}
