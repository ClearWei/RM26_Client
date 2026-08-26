pragma ComponentBehavior: Bound

import QtQuick 2.15

/**
 * @file CentralAimingHUD.qml
 * @brief 中央瞄准 HUD - 包含准心、热量环、射击信息
 * @details 准心始终位于屏幕几何中心，射击信息面板挂载在准心右侧
 */
Item {
    id: root
    width: 600
    height: 600

    // --- 数据绑定 ---
    // gameData 由 MainWindow 注入，这里统一收口，避免子组件依赖隐式上下文查找。
    // qmllint disable unqualified
    readonly property var gameDataContext: typeof gameData !== "undefined" ? gameData : null
    // qmllint enable unqualified
    property var myRobot: gameDataContext ? gameDataContext.myRobot : ({})
    property int robotType: (myRobot && myRobot.type !== undefined) ? Number(myRobot.type) : 3
    property bool isAerialRobot: robotType === 6 || robotType === 4

    // --- 自定义 UI 数据绑定 ---
    property var customData: gameDataContext ? gameDataContext.myRobotCustomData : ({})
    property bool showCustomStatusPanels: false

    property real currentHeat: (myRobot && myRobot.currentHeat !== undefined) ? Number(myRobot.currentHeat) : 0
    property real heatLimit: (myRobot && myRobot.heatLimit !== undefined && Number(myRobot.heatLimit) > 0) ? Number(myRobot.heatLimit) : 240
    property real heatRatioRaw: heatLimit > 0 ? currentHeat / heatLimit : 0
    property real heatRatio: Math.max(0, Math.min(1, heatRatioRaw))

    property bool isOverheated: (myRobot && myRobot.isHeatOverLimit !== undefined) ? myRobot.isHeatOverLimit : (heatRatioRaw >= 1.0)
    property color heatColor: {
        if (isOverheated)
            return "#FF3B3B";
        if (heatRatioRaw >= 0.75)
            return "#FF4B4B";
        if (heatRatioRaw >= 0.5)
            return "#F5E37B";
        if (heatRatioRaw > 0.0)
            return "#D8F8FF";
        return "#5C636A";
    }

    property real muzzleVelocity: (myRobot && myRobot.muzzleVelocity !== undefined) ? Number(myRobot.muzzleVelocity) : 0
    property real shootSpeedLimit: {
        if (myRobot && myRobot.shootSpeedLimit !== undefined)
            return Math.max(0, Number(myRobot.shootSpeedLimit));
        // GameData 尚未提供该字段时使用协议默认上限
        return 30.0;
    }

    property int speedLockState: (myRobot && myRobot.speedLockState !== undefined) ? Number(myRobot.speedLockState) : 0
    property bool speedOverLimit: speedLockState > 0

    property int configuredSpeedLockSeconds: {
        if (myRobot && myRobot.speedLockSeconds !== undefined)
            return Math.max(0, Number(myRobot.speedLockSeconds));
        return 0;
    }
    // 直接使用配置下发的锁定时长
    property int speedLockSeconds: configuredSpeedLockSeconds

    property bool centerSpeedWarning: speedOverLimit && !isAerialRobot
    property bool panelSpeedWarning: speedOverLimit && isAerialRobot

    property bool isHero: robotType === 1
    property int allowedAmmo17mmRaw: {
        if (myRobot && myRobot.allowedAmmo17mm !== undefined)
            return Math.max(0, Number(myRobot.allowedAmmo17mm));
        return 0;
    }
    property int allowedAmmo42mmRaw: {
        if (myRobot && myRobot.allowedAmmo42mm !== undefined)
            return Math.max(0, Number(myRobot.allowedAmmo42mm));
        return 0;
    }
    property int allowedAmmoRaw: isHero ? allowedAmmo42mmRaw : allowedAmmo17mmRaw
    property int fortressBonusAmmo: {
        if (!myRobot) return 0;
        if (myRobot.fortressBonusAmmo !== undefined && Number(myRobot.fortressBonusAmmo) > 0)
            return Number(myRobot.fortressBonusAmmo);
        return 0;
    }
    property color ammoColor: {
        if (allowedAmmoRaw <= 0)
            return "#FF3B3B";
        return "#E7EDF0";
    }

    // --- 视觉中心点 (固定在组件正中央) ---
    Item {
        id: visualCenter
        anchors.centerIn: parent
        width: 1
        height: 1

        // --- 1. 外圈背景 (超大淡灰圆环) ---
        Image {
            id: outerRing
            anchors.centerIn: parent
            width: 560
            height: 560
            source: "qrc:/images/crosshair/crosshair_bg_single.png"
            fillMode: Image.PreserveAspectFit
            opacity: 0.35
        }

        // --- 横向基准线 ---
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: heatRingContainer.left
            anchors.rightMargin: 4
            width: 178
            height: 1
            color: "#596067"
            opacity: 0.55
        }
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: heatRingContainer.right
            anchors.leftMargin: 4
            width: 178
            height: 1
            color: "#596067"
            opacity: 0.55
        }

        // --- 2. 热量环 (紧贴准星的小圈) ---
        Item {
            id: heatRingContainer
            anchors.centerIn: parent
            width: 116
            height: 116

            // 热量环背景
            Image {
                id: heatRingBg
                anchors.fill: parent
                source: "qrc:/images/crosshair/crosshair_slid_circle_bg.png"
                fillMode: Image.PreserveAspectFit
                opacity: 0.92
            }

            Canvas {
                id: heatProgressCanvas
                anchors.fill: parent
                antialiasing: true

                onPaint: {
                    var context = getContext("2d");
                    context.clearRect(0, 0, width, height);

                    var displayRatio = root.isOverheated ? 1.0 : root.heatRatio;
                    if (displayRatio <= 0) {
                        return;
                    }

                    var lineWidth = 8;
                    var radius = Math.max(0, Math.min(width, height) * 0.5 - lineWidth * 0.7);
                    var centerX = width * 0.5;
                    var centerY = height * 0.5;
                    var startAngle = -Math.PI * 0.5;
                    var endAngle = startAngle + Math.PI * 2 * displayRatio;

                    context.beginPath();
                    context.lineWidth = lineWidth;
                    context.lineCap = "round";
                    context.strokeStyle = root.heatColor;
                    context.shadowColor = root.heatColor;
                    context.shadowBlur = root.isOverheated ? 18 : 10;
                    context.arc(centerX, centerY, radius, startAngle, endAngle, false);
                    context.stroke();
                }

                Component.onCompleted: requestPaint()
            }

            Rectangle {
                anchors.centerIn: parent
                width: 94
                height: 94
                radius: 47
                color: "transparent"
                border.width: 1
                border.color: "#4D545B"
                opacity: 0.7
            }

            Text {
                id: overheatWarningText
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.top
                anchors.bottomMargin: 10
                text: "热量超限，图传断开"
                visible: root.isOverheated
                color: "#FF4747"
                font.pixelSize: 18
                font.bold: true
                font.family: "PingFang SC"
                opacity: 0.92

                SequentialAnimation {
                    running: root.isOverheated
                    loops: Animation.Infinite
                    NumberAnimation {
                        target: overheatWarningText
                        property: "opacity"
                        to: 0.35
                        duration: 280
                    }
                    NumberAnimation {
                        target: overheatWarningText
                        property: "opacity"
                        to: 0.92
                        duration: 280
                    }
                }
            }

            Text {
                visible: root.centerSpeedWarning
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.bottom
                anchors.topMargin: 10
                text: {
                    if (root.centerSpeedWarning) {
                        if (root.speedLockState === 3) {
                            return "速度严重超限，永久锁定";
                        } else if (root.speedLockState === 2) {
                            return "速度已超限，锁定 20s";
                        } else if (root.speedLockState === 1) {
                            return "速度已超限，锁定 15s";
                        }
                        return "速度已超限，锁定 " + root.speedLockSeconds + "s";
                    }
                    return "";
                }
                color: "#FF3B3B"
                font.pixelSize: 14
                font.family: "PingFang SC"
                font.bold: true
            }
        }

        // --- 3. 准心 (中心十字) ---
        Image {
            id: crosshair
            anchors.centerIn: parent
            width: 36
            height: 36
            source: "qrc:/images/crosshair/crosshair_middle.png"
            fillMode: Image.PreserveAspectFit
            opacity: 1.0
            visible: !root.centerSpeedWarning
        }
    }

    onHeatRatioChanged: heatProgressCanvas.requestPaint()
    onHeatColorChanged: heatProgressCanvas.requestPaint()
    onIsOverheatedChanged: heatProgressCanvas.requestPaint()

    // --- 4. 射击信息面板 (固定挂载到最大外环右侧) ---
    Item {
        id: shootingInfo
        anchors.left: visualCenter.right
        anchors.leftMargin: 286
        anchors.verticalCenter: visualCenter.verticalCenter
        anchors.verticalCenterOffset: -8
        width: 146
        height: 184

        // 透明容器 - 无背景无边框
        Item {
            width: 146
            height: 178

            Column {
                anchors.fill: parent
                anchors.leftMargin: 17
                anchors.rightMargin: 10
                anchors.topMargin: 14
                anchors.bottomMargin: 11
                spacing: 10

                Column {
                    spacing: 2
                    Text {
                        text: "射击初速度"
                        color: "#8E959C"
                        font.pixelSize:10
                        font.family: "PingFang SC"
                    }
                    Text {
                        text: root.panelSpeedWarning ? (root.muzzleVelocity.toFixed(1) + " " + root.speedLockSeconds + "s") : root.muzzleVelocity.toFixed(1)
                        color: root.panelSpeedWarning ? "#FF3B3B" : "#EEF3F6"
                        font.pixelSize: root.panelSpeedWarning ? 28 : 28
                        font.family: "PingFang SC"
                        font.bold: true
                    }
                }

                Column {
                    spacing: 2
                    Text {
                        text: "允许发弹量"
                        color: "#8E959C"
                        font.pixelSize:10
                        font.family: "PingFang SC"
                    }
                    Text {
                        text: root.allowedAmmoRaw
                        color: root.ammoColor
                        font.pixelSize: 28
                        font.family: "PingFang SC"
                        font.bold: true
                    }
                    Text {
                        visible: root.fortressBonusAmmo > 0
                        text: "+" + root.fortressBonusAmmo
                        color: "#11F5A0"
                        font.pixelSize: 28
                        font.family: "PingFang SC"
                        font.bold: true
                    }
                }
            }
        }
    }

    // ==================== 自定义 UI 元素 (通过 CustomByteBlock) ====================

    // --- 摩擦轮状态 (左上角) ---
    Rectangle {
        id: frictionStatus
        anchors.left: parent.left
        anchors.leftMargin: 20
        anchors.top: parent.top
        anchors.topMargin: 50
        width: 120
        height: 70
        color: "#1A1A1A"
        opacity: 0.85
        border.color: "#FF4444"
        border.width: 1
        radius: 4
        visible: root.showCustomStatusPanels && root.customData.fricEnabled !== undefined

        Column {
            anchors.centerIn: parent
            spacing: 8

            Text {
                text: "FRICTION STATUS"
                color: "#FF4444"
                font.pixelSize: 11
                font.bold: true
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Row {
                spacing: 15
                anchors.horizontalCenter: parent.horizontalCenter

                // FRIC 状态
                Row {
                    spacing: 4
                    Text {
                        text: "FRIC"
                        color: "#00CED1"
                        font.pixelSize: 10
                    }
                    Rectangle {
                        width: 10
                        height: 10
                        radius: 5
                        color: root.customData.fricEnabled ? "#00FF00" : "#666666"
                        border.color: root.customData.fricEnabled ? "#00FF00" : "#444444"
                        border.width: 1
                    }
                }

                // RAMMER 状态
                Row {
                    spacing: 4
                    Text {
                        text: "RAMMER"
                        color: "#00CED1"
                        font.pixelSize: 10
                    }
                    Rectangle {
                        width: 10
                        height: 10
                        radius: 5
                        color: root.customData.rammerEnabled ? "#00FF00" : "#666666"
                        border.color: root.customData.rammerEnabled ? "#00FF00" : "#444444"
                        border.width: 1
                    }
                }
            }
        }
    }

    // --- 底盘状态 (右上角) ---
    Rectangle {
        id: chassisStatus
        anchors.right: parent.right
        anchors.rightMargin: 20
        anchors.top: parent.top
        anchors.topMargin: 50
        width: 130
        height: 110
        color: "#1A1A1A"
        opacity: 0.85
        border.color: "#FF4444"
        border.width: 1
        radius: 4
        visible: root.showCustomStatusPanels && root.customData.chassisMode !== undefined

        Column {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 6

            Text {
                text: "CHASSIS MODE"
                color: "#FF4444"
                font.pixelSize: 11
                font.bold: true
            }

            // 模式列表
            Column {
                spacing: 4
                width: parent.width

                Repeater {
                    model: [
                        { name: "FOLLOW", active: root.customData.followMode },
                        { name: "SPIN", active: root.customData.spinMode },
                        { name: "PROTECT", active: root.customData.chassisProtect },
                        { name: "WARNING", active: root.customData.chassisWarning }
                    ]

                    Row {
                        id: chassisModeRow
                        required property var modelData

                        spacing: 6
                        width: parent.width
                        Text {
                            text: chassisModeRow.modelData.name
                            color: chassisModeRow.modelData.active ? "#00FF00" : "#666666"
                            font.pixelSize: 10
                            font.family: "Consolas"
                        }
                        Rectangle {
                            width: 6
                            height: 6
                            radius: 3
                            color: chassisModeRow.modelData.active ? "#00FF00" : "#333333"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }
        }
    }

    // --- 超级电容能量条 (右侧中部) ---
    Rectangle {
        id: superCapPanel
        anchors.right: parent.right
        anchors.rightMargin: 20
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: 50
        width: 160
        height: 50
        color: "#1A1A1A"
        opacity: 0.85
        border.color: "#00CED1"
        border.width: 1
        radius: 4
        visible: root.customData.superCapEnergy !== undefined && root.customData.superCapEnergy > 0

        Column {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 6

            Text {
                text: "SUPER CAPACITOR"
                color: "#00CED1"
                font.pixelSize: 10
                font.bold: true
            }

            // 能量条
            Rectangle {
                width: parent.width
                height: 16
                color: "#333333"
                radius: 8

                Rectangle {
                    width: parent.width * (Math.min(root.customData.superCapEnergy, 100) / 100.0)
                    height: parent.height
                    radius: 8
                    color: {
                        var pct = root.customData.superCapEnergy;
                        if (pct > 60) return "#00FF00";
                        if (pct > 30) return "#FFFF00";
                        return "#FF0000";
                    }

                    // 低电量闪烁
                    SequentialAnimation on opacity {
                        running: root.customData.superCapEnergy < 30
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.5; duration: 300 }
                        NumberAnimation { to: 1.0; duration: 300 }
                    }
                }

                // 百分比文字
                Text {
                    anchors.centerIn: parent
                    text: Math.round(root.customData.superCapEnergy) + "%"
                    color: "white"
                    font.pixelSize: 10
                    font.bold: true
                }
            }
        }
    }

    // --- 云台-底盘角度 (底部中央) ---
    Item {
        id: angleIndicator
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 30
        width: 120
        height: 80
        visible: root.customData.gimbalChassisAngle !== undefined && Math.abs(root.customData.gimbalChassisAngle) > 0.1

        // 角度数值
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            text: (root.customData.gimbalChassisAngle !== undefined) ?
                  ((root.customData.gimbalChassisAngle >= 0 ? "+" : "") + root.customData.gimbalChassisAngle.toFixed(1) + "°") : "0.0°"
            color: Math.abs(root.customData.gimbalChassisAngle) > 45 ? "#FF4444" : "#00FF00"
            font.pixelSize: 16
            font.bold: true
            font.family: "Consolas"
        }

        // 简易角度指示器
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 10
            width: 80
            height: 4
            color: "#444444"
            radius: 2

            // 中心点
            Rectangle {
                anchors.centerIn: parent
                width: 8
                height: 8
                radius: 4
                color: "#00CED1"
            }

            // 角度指示条
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(Math.abs(root.customData.gimbalChassisAngle) / 180 * 40, 40)
                height: 4
                radius: 2
                color: root.customData.gimbalChassisAngle > 0 ? "#FF4444" : "#4444FF"
                x: root.customData.gimbalChassisAngle > 0 ? parent.width/2 : parent.width/2 - width
            }
        }
    }

    // --- 瞄准辅助框 (根据目标距离动态显示) ---
    Rectangle {
        id: aimAssistBox
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -(root.customData.ballisticCompensation || 0) * 2
        width: 40 + (root.customData.targetDistance || 0) * 3
        height: width
        color: "transparent"
        border.color: "#FF4444"
        border.width: 2
        opacity: 0.7
        visible: root.customData.targetDistance > 0

        // 内部十字
        Rectangle {
            anchors.centerIn: parent
            width: 10
            height: 1
            color: "#FF4444"
        }
        Rectangle {
            anchors.centerIn: parent
            width: 1
            height: 10
            color: "#FF4444"
        }

        // 距离文字
        Text {
            anchors.top: parent.bottom
            anchors.topMargin: 5
            anchors.horizontalCenter: parent.horizontalCenter
            text: (root.customData.targetDistance / 10.0).toFixed(1) + "m"
            color: "#FF4444"
            font.pixelSize: 10
        }
    }
}
