pragma ComponentBehavior: Bound

import QtQuick 2.15

/**
 * @file MiniMap.qml
 * @brief 小地图组件
 * @details 将 C++ MiniMapWidget 转化为 QML 实现
 * @author Clear
 * @date 2026-03-04
 */

Item {
    id: root
    width: 270
    height: 160

    // gameData 由 MainWindow 注入，小地图统一从这里读取比赛状态。
    // qmllint disable unqualified
    readonly property var gameDataContext: typeof gameData !== "undefined" ? gameData : null
    // qmllint enable unqualified

    // --- 逻辑状态 ---
    readonly property int runeStatusValue: gameDataContext ? Number(gameDataContext.runeStatus) : 1
    readonly property int runeTypeValue: gameDataContext ? Number(gameDataContext.runeType) : 0
    readonly property int runeActivationStartRemainingTime: gameDataContext ? Number(gameDataContext.runeActivationStartRemainingTime) : -1
    readonly property bool runeEventVisible: runeStatusValue === 2 || runeStatusValue === 3
    readonly property int runeEventType: runeTypeValue
    readonly property int runeEventStatus: runeStatusValue
    property int currentRobotId: gameDataContext ? gameDataContext.currentRobotId : -1 // 当前操控的机器人ID
    property bool redBaseAttacked: false        // 红方基地是否被攻击
    property bool blueBaseAttacked: false      // 蓝方基地是否被攻击
    property bool redOutpostDead: gameDataContext ? gameDataContext.redOutpostDead : false // 红方前哨站是否死亡
    property bool blueOutpostDead: gameDataContext ? gameDataContext.blueOutpostDead : false // 蓝方前哨站是否死亡
    property var buffTimedData: gameDataContext ? gameDataContext.buffTimedData : ({})
    property var attackBuff: buffTimedData.attack ? buffTimedData.attack : emptyBuffData(1)

    // --- 动画与缩放常量 ---
    property int remainTime: gameDataContext && gameDataContext.remainTime !== undefined ? Number(gameDataContext.remainTime) : 0 // 剩余时间（秒）
    property int escapeTime: 420 - remainTime                     // 游戏时间（秒）
    property real breathPhase: 0.0               // 呼吸动画相位
    property real autoRotation: 0.0              // 自动旋转角度
    readonly property real breathFactor: 1.0 + 0.2 * Math.sin(breathPhase) // 呼吸缩放系数
    readonly property real scaleFactor: 1.0 / 3.0 // 基础缩放比例
    readonly property real relativeRedX: 0.06    // 红方基地相对X坐标
    readonly property real relativeBlueX: 0.94   // 蓝方基地相对X坐标

    //哨兵路径规划显示
    property real sentryStartX: gameDataContext ? gameDataContext.sentryStartX : 0.7
    property real sentryStartY: gameDataContext ? gameDataContext.sentryStartY : 0.55
    property var sentryPathDeltasX: gameDataContext ? gameDataContext.sentryPathDeltasX : [-0.4, 0.4, -0.5, -0.05]
    property var sentryPathDeltasY: gameDataContext ? gameDataContext.sentryPathDeltasY : [0.15, -0.4, 0.3, -0.15]

    // --- 信号 ---
    signal mapClicked(real x, real y)            // 地图点击信号（归一化坐标）
    property real normX: 0.0                    //云台标记x显示
    property real normY: 0.0                     //云台标记y显示
    property bool clickEnabled: false
    property int pendingMarkType: -1
    property string pendingMarkLabel: ""
    property double commandMarkerNowMs: Date.now()

    //本次能量机关激活提示时间戳
    property double runeEventStartedAtMs: 0
    property double runeClockNowMs: Date.now()
    property int lastObservedRuneStatus: 1

    onRuneStatusValueChanged: {
        if (runeStatusValue >= 2) {
            syncRuneEventClock()
        } else {
            runeEventStartedAtMs = 0
            runeClockNowMs = Date.now()
        }
        lastObservedRuneStatus = runeStatusValue
    }

    readonly property int runeDisplaySeconds: {
        if (!root.runeEventVisible)
            return 0;

        if (root.runeEventStatus === 2) {
            return Math.max(0, Math.ceil(20 - root.runeStatusElapsedSeconds()));
        }

        if (root.runeEventStatus !== 3)
            return 0;

        if (root.runeEventType === 0) {
            return Math.max(0, Math.ceil(45 - root.runeActivatedElapsedSeconds()));
        }

        //直接使用增益剩余时间
        var attackLeft = Number(root.attackBuff.leftTime) || 0;
        if (attackLeft > 0)
            return attackLeft;

        var attackMax = Number(root.attackBuff.maxTime) || 0;
        return Math.max(0, Math.ceil(attackMax - root.runeActivatedElapsedSeconds()));
    }
    readonly property string runeDisplayText: root.runeDisplaySeconds > 0 ? root.runeDisplaySeconds.toString() : ""

    function emptyBuffData(typeId) {
        return ({ type: typeId, level: 0, maxTime: 0, leftTime: 0 })
    }

    function syncRuneEventClock() {
        var nowMs = Date.now()
        runeClockNowMs = nowMs

        if (runeActivationStartRemainingTime >= 0 && remainTime >= 0) {
            var elapsedSeconds = Math.max(0, runeActivationStartRemainingTime - remainTime)
            runeEventStartedAtMs = nowMs - elapsedSeconds * 1000
            return
        }

        if (runeEventStartedAtMs <= 0 || lastObservedRuneStatus !== runeStatusValue)
            runeEventStartedAtMs = nowMs
    }

    function runeStatusElapsedSeconds() {
        if (runeActivationStartRemainingTime >= 0 && remainTime >= 0)
            return Math.max(0, runeActivationStartRemainingTime - remainTime)

        if (runeEventStartedAtMs <= 0)
            return 0

        return Math.max(0, (runeClockNowMs - runeEventStartedAtMs) / 1000.0)
    }

    function runeActivatedElapsedSeconds() {
        if (runeEventStartedAtMs <= 0)
            return 0

        return Math.max(0, (runeClockNowMs - runeEventStartedAtMs) / 1000.0)
    }


    // --- 机器人数据模型 ---
    //传入的id在listmodel中转换。1->1  101->8
    ListModel {
        id: markersModel
        // 数据结构: { key, robotId, isRed, posX, posY, angle, isHighLight, isDead }
    }

    ListModel {
        id: commandMarkersModel
        // 数据结构: { normX, normY, markType, expiresAt }
    }

    //全局同步
    function reloadMarkersFromGameData() {
        markersModel.clear();
        if (!root.gameDataContext || !root.gameDataContext.miniMapMarkers) {
            return;
        }

        let markers = root.gameDataContext.miniMapMarkers;
        for (let i = 0; i < markers.length; ++i) {
            markersModel.append(markers[i]);
        }
    }

    //当组件创建完毕，先初始化机器人数据
    Component.onCompleted: {
        reloadMarkersFromGameData()
        lastObservedRuneStatus = runeStatusValue
        if (runeStatusValue >= 2)
            syncRuneEventClock()
    }

    // 自动同步 GameData 数据到本地模型
    Connections {
        target: root.gameDataContext
        function onRobotPositionUpdated(robotId, posX, posY, angle, isHighLight) {
            root.reloadMarkersFromGameData();
        }
        function onRobotDataUpdated(robotId) {
            root.reloadMarkersFromGameData();
        }
        function onRedBaseAttackedChanged() {
            root.redBaseAttacked = true;
            redBaseTimer.restart();
        }
        function onBlueBaseAttackedChanged() {
            root.blueBaseAttacked = true;
            blueBaseTimer.restart();
        }
        function onRuneActived(runeType,runeStatus) {
            root.runeEventStartedAtMs = Date.now();
            root.runeClockNowMs = root.runeEventStartedAtMs;
        }
    }

    //能量机关图片来源(当前都是己方，则直接设置，后续若协议变化可更改)
    function runeSource(){
        //正在激活
        if (root.runeEventStatus === 2)
            return "qrc:/images/minimap/map_island_small.png";
        else if (root.runeEventStatus === 3)
        {
            //获取己方阵营
            var isRedTeam = root.currentRobotId > 0 && root.currentRobotId < 100;
            return isRedTeam ? "qrc:/images/minimap/map_island_red.png"
                            : "qrc:/images/minimap/map_island_blue.png"
        }
        return "";
    }

    // --- 定时器 ---
    // 呼吸动画定时器 (10ms)
    Timer {
        id: breathTimer
        interval: 10
        running: true
        repeat: true
        onTriggered: {
            root.breathPhase += 0.1;
            if (root.breathPhase >= 2 * Math.PI) {
                root.breathPhase -= 2 * Math.PI;
            }
        }
    }

    // 基地警报自动停止定时器 (6s)
    Timer {
        id: redBaseTimer
        interval: 6000
        repeat: false
        running: root.redBaseAttacked
        onTriggered: root.redBaseAttacked = false
    }

    Timer {
        id: blueBaseTimer
        interval: 6000
        repeat: false
        running: root.blueBaseAttacked
        onTriggered: root.blueBaseAttacked = false
    }

    Timer {
        id: runeOverlayTimer
        interval: 200
        running: root.runeEventVisible
        repeat: true
        onTriggered: root.runeClockNowMs = Date.now()
    }

    //能量机关旋转效果
    Timer {
        id: rotationTimer
        interval: 100
        running: true
        repeat: true
        onTriggered: {
            root.autoRotation += 10;
            if (root.autoRotation >= 360) {
                root.autoRotation -= 360;
            }
        }
    }

    Timer {
        id: commandMarkerCleanupTimer
        interval: 100
        repeat: true
        running: commandMarkersModel.count > 0
        onTriggered: {
            root.commandMarkerNowMs = Date.now();
            root.cleanupExpiredCommandMarkers();
        }
    }


    /**
     * @brief 设置当前操作的机器人 ID
     */

    function setPendingMarkMode(markType, label) {
        root.pendingMarkType = Number(markType);
        root.pendingMarkLabel = String(label);
    }

    function clearPendingMarkMode() {
        root.pendingMarkType = -1;
        root.pendingMarkLabel = "";
    }

    function cleanupExpiredCommandMarkers() {
        let now = Date.now();
        for (let i = commandMarkersModel.count - 1; i >= 0; --i) {
            if (commandMarkersModel.get(i).expiresAt <= now) {
                commandMarkersModel.remove(i);
            }
        }
    }

    function addCommandMarker(x, y, markType, label) {
        root.commandMarkerNowMs = Date.now();
        cleanupExpiredCommandMarkers();
        commandMarkersModel.append({
            "normX": Math.max(0.0, Math.min(1.0, Number(x))),
            "normY": Math.max(0.0, Math.min(1.0, Number(y))),
            "markType": Number(markType),
            "bornAt": root.commandMarkerNowMs,
            "expiresAt": root.commandMarkerNowMs + 1500
        });
    }

    function markerColor(markType) {
        switch (markType) {
        case 1:
            return "#18D26E";
        case 3:
            return "#FFB340";
        case 2:
            return "#15D7C7";
        default:
            return "#F25B6A";
        }
    }

    function markerSource(markType) {
        switch (markType) {
        case 1:
            return "qrc:/images/minimap/ic_fpv_map_cursor_attack_1.png";
        case 3:
            return "qrc:/images/minimap/ic_fpv_map_cursor_warning_1.png";
        case 2:
            return "qrc:/images/minimap/ic_fpv_map_cursor_defense_1.png";
        default:
            return "";
        }
    }

    // --- 界面布局 ---

    // 1. 背景层
    Image {
        id: bgImage
        anchors.fill: parent
        source: "qrc:/images/minimap_bg.png"
        opacity: 0.7
        fillMode: Image.Stretch
        z: 0
    }

    //哨兵路径规划显示
    Canvas {
        id: sentryPathCanvas
        anchors.fill: parent
        z: 0.5
        visible: root.sentryPathDeltasX.length > 0 && root.sentryPathDeltasX.length === root.sentryPathDeltasY.length

        onPaint: {
            var ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);

            if (root.sentryPathDeltasX.length === 0 || root.sentryPathDeltasX.length !== root.sentryPathDeltasY.length) {
                return;
            }

            ctx.strokeStyle = "#00FF00";
            ctx.lineWidth = 2.0;
            ctx.lineJoin = "round";
            ctx.lineCap = "round";
            ctx.shadowColor = "black";
            ctx.shadowBlur = 3;
            ctx.beginPath();

            var startX = root.sentryStartX;
            var startY = root.sentryStartY;
            ctx.moveTo(startX * root.width, (1 - startY) * root.height);
            var addX = startX;
            var addY = startY;

            for (var i = 0; i < root.sentryPathDeltasX.length; i++) {
                addX += root.sentryPathDeltasX[i];
                addY += root.sentryPathDeltasY[i];
                var pointX = addX * root.width;
                var pointY = (1 - addY) * root.height;
                ctx.lineTo(pointX, pointY);
            }

            ctx.stroke();
        }

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        Connections {
            target: root
            function onSentryStartXChanged() {
                sentryPathCanvas.requestPaint();
            }
            function onSentryStartYChanged() {
                sentryPathCanvas.requestPaint();
            }
            function onSentryPathDeltasXChanged() {
                sentryPathCanvas.requestPaint();
            }
            function onSentryPathDeltasYChanged() {
                sentryPathCanvas.requestPaint();
            }
        }
    }

    // 2. 固定组件层 (前哨站、基地)

    // 前哨站图标
    Image {
        source: "qrc:/images/minimap/red_map_outpost.png"
        x: 0.4 * root.width - width / 2
        y: 0.75 * root.height - height / 2
        opacity: root.redOutpostDead ? 0.5 : 1.0
        width: sourceSize.width * 0.38 * root.width / 270.0
        height: sourceSize.height * 0.38 * root.height / 160.0
    }
    Image {
        source: "qrc:/images/minimap/blue_map_outpost.png"
        x: 0.608 * root.width - width / 2
        y: 0.24 * root.height - height / 2
        opacity: root.blueOutpostDead ? 0.5 : 1.0
        width: sourceSize.width * 0.38 * root.width / 270.0
        height: sourceSize.height * 0.38 * root.height / 160.0
    }

    // 基地动画锚点（用于受击动画定位）
    Item {
        id: redBaseIcon
        x: root.relativeRedX * root.width - width / 5
        y: 0.5 * root.height - height / 2
        width: 24 * root.width / 270.0
        height: 24 * root.height / 160.0
        visible: false
    }
    Item {
        id: blueBaseIcon
        x: root.relativeBlueX * root.width - width / 1.1
        y: 0.5 * root.height - height / 2
        width: 24 * root.width / 270.0
        height: 24 * root.height / 160.0
        visible: false
    }

    // 基地被攻击动画 (4层呼吸渐进效果)
    Repeater {
        model: [1, 2, 3, 4]
        delegate: Image {
            id: redBaseAnimation

            required property int modelData
            visible: root.redBaseAttacked
            source: "qrc:/images/minimap/red_map_base_animation_" + redBaseAnimation.modelData + ".png"
            anchors.centerIn: redBaseIcon
            //四层图片不同的大小
            property real animationScale: 1.0 + (redBaseAnimation.modelData - 1) * 0.1 + (redBaseAnimation.modelData === 1 ? 0 : 0.1)
            //breathFactor控制呼吸效果
            width: sourceSize.width * root.scaleFactor * root.breathFactor * redBaseAnimation.animationScale * root.width / 270.0
            height: sourceSize.height * root.scaleFactor * root.breathFactor * redBaseAnimation.animationScale * root.height / 160.0
        }
    }

    Repeater {
        model: [1, 2, 3, 4]
        delegate: Image {
            id: blueBaseAnimation

            required property int modelData
            visible: root.blueBaseAttacked
            source: "qrc:/images/minimap/blue_map_base_animation_" + blueBaseAnimation.modelData + ".png"
            anchors.centerIn: blueBaseIcon
            property real animationScale: 1.0 + (blueBaseAnimation.modelData - 1) * 0.1 + (blueBaseAnimation.modelData === 1 ? 0 : 0.1)
            width: sourceSize.width * root.scaleFactor * root.breathFactor * blueBaseAnimation.animationScale * root.width / 270.0
            height: sourceSize.height * root.scaleFactor * root.breathFactor * blueBaseAnimation.animationScale * root.height / 160.0
        }
    }

    // 3. 机器人标记层
    Repeater {
        model: markersModel
        z: 2
        delegate: Item {
            id: robotMarker

            required property int key
            required property int robotId
            required property bool isRed
            required property real posX
            required property real posY
            required property real angle
            required property bool isHighLight
            required property bool isDead

            //归一化
            // 兼容两种坐标输入：归一化(0~1) 或 米坐标(28m x 15m)
            property real normalizedPosX: {
                let v = robotMarker.posX;
                if (v > 1.0 || v < 0.0)
                    v = v / 28.0;
                return Math.max(0.0, Math.min(1.0, v));
            }
            property real normalizedPosY: {
                let v = robotMarker.posY;
                if (v > 1.0 || v < 0.0)
                    v = v / 15.0;
                return Math.max(0.0, Math.min(1.0, v));
            }
            x: robotMarker.normalizedPosX * root.width
            y: robotMarker.normalizedPosY * root.height //坐标中心是左上角，比赛显示来看
            z: 2
            opacity: robotMarker.isDead ? 0.5 : 1.0

            // 锁定线 (高亮显示)
            Image {
                z: 2
                visible: robotMarker.isHighLight
                source: "qrc:/images/minimap/map_robot_lockline.png"
                anchors.centerIn: parent
                width: sourceSize.width * root.scaleFactor * root.breathFactor * 0.75 * root.width / 270.0
                height: sourceSize.height * root.scaleFactor * root.breathFactor * 0.75 * root.height / 160.0
            }

            // 机器人底座图标
            Image {
                id: robotBaseImage

                z: 2
                visible: robotMarker.key !== 6 && robotMarker.key !== 13 // 非空中机器人
                source: {
                    if (robotMarker.isHighLight) {
                        return robotMarker.isRed ? "qrc:/images/minimap/red_map_robot_lockbg.png" : "qrc:/images/minimap/blue_map_robot_lockbg.png";
                    }
                    if (robotMarker.robotId === root.currentRobotId || (robotMarker.robotId - 7 + 100) === root.currentRobotId) {
                        return "qrc:/images/minimap/self_map_robot.png";
                    }
                    return robotMarker.isRed ? "qrc:/images/minimap/red_map_robot_lockbg.png" : "qrc:/images/minimap/blue_map_robot_lockbg.png";
                }
                anchors.centerIn: parent
                //被锁定时机器人有呼吸效果
                property real robotScale: robotMarker.isHighLight ? root.scaleFactor * 4.0 / 7.0 * root.breathFactor : root.scaleFactor * 4.0 / 3.5
                width: sourceSize.width * robotBaseImage.robotScale * root.width / 270.0
                height: sourceSize.height * robotBaseImage.robotScale * root.height / 160.0
            }

            // 方向箭头
            Image {
                id: directionArrow

                z: 2
                visible: robotMarker.key !== 6 && robotMarker.key !== 13 // 非空中机器人
                source: {
                    if (robotMarker.robotId === root.currentRobotId || (robotMarker.robotId - 7 + 100) === root.currentRobotId) {
                        return "qrc:/images/minimap/self_map_arrow.png";
                    }
                    return robotMarker.isRed ? "qrc:/images/minimap/red_map_arrow.png" : "qrc:/images/minimap/blue_map_arrow.png";
                }
                anchors.centerIn: parent
                rotation: robotMarker.angle
                property real arrowScale: robotMarker.isHighLight ? root.breathFactor : 1.0
                width: sourceSize.width * root.scaleFactor * directionArrow.arrowScale * root.width / 270.0
                height: sourceSize.height * root.scaleFactor * directionArrow.arrowScale * root.height / 160.0
                //方向箭头需要根据机器人方向调整位置
                transform: Translate {
                    id: arrowOffset

                    property real arrowScale: robotMarker.isHighLight ? root.breathFactor : 1.0
                    x: Math.sin(robotMarker.angle / 180.0 * Math.PI) * 10 ** arrowOffset.arrowScale * root.width / 270.0
                    y: -Math.cos(robotMarker.angle / 180.0 * Math.PI) * 10 ** arrowOffset.arrowScale * root.height / 160.0
                }
            }

            // 机器人编号 / 特殊单位图标 (Radar, Guard, Airplane)
            Image {
                id: robotIdImage

                z: 2
                source: {
                    switch (robotMarker.key) {
                    case 1:
                    case 8:
                        return "qrc:/images/minimap/id_1.png";
                    case 2:
                    case 9:
                        return "qrc:/images/minimap/id_2.png";
                    case 3:
                    case 10:
                        return "qrc:/images/minimap/id_3.png";
                    case 4:
                    case 11:
                        return "qrc:/images/minimap/id_4.png";
                    case 5:
                    case 12:
                        return "qrc:/images/minimap/id_5.png";
                    case 6:
                    case 13:
                        return robotMarker.isRed ? "qrc:/images/minimap/red_map_airplane.png" : "qrc:/images/minimap/blue_map_airplane.png";
                    case 7:
                    case 14:
                        return robotMarker.isRed ? "qrc:/images/minimap/red_map_guard.png" : "qrc:/images/minimap/blue_map_guard.png";
                    default:
                        return "";
                    }
                }
                anchors.centerIn: parent
                property real idScale: robotMarker.isHighLight ? root.breathFactor : 1.0
                width: sourceSize.width * root.scaleFactor * robotIdImage.idScale * root.width / 270.0
                height: sourceSize.height * root.scaleFactor * robotIdImage.idScale * root.height / 160.0
            }
        }
    }

    Repeater {
        model: commandMarkersModel
        z: 3
        delegate: Item {
            id: commandMarker

            required property real normX
            required property real normY
            required property int markType
            required property double bornAt
            required property double expiresAt

            width: 22
            height: 22
            x: commandMarker.normX * root.width - width / 2
            y: commandMarker.normY * root.height - height / 2
            z: 3
            property double ageMs: root.commandMarkerNowMs - commandMarker.bornAt
            property double leftMs: commandMarker.expiresAt - root.commandMarkerNowMs
            scale: commandMarker.ageMs < 220 ? 0.55 + (Math.max(0, commandMarker.ageMs) / 220.0) * 0.45 : (commandMarker.leftMs < 220 ? 1.0 - (1.0 - Math.max(0.0, commandMarker.leftMs / 220.0)) * 0.12 : 1.0)
            opacity: commandMarker.ageMs < 140 ? Math.max(0.0, commandMarker.ageMs / 140.0) : (commandMarker.leftMs < 260 ? Math.max(0.0, commandMarker.leftMs / 260.0) : 1.0)
            rotation: commandMarker.ageMs < 180 ? -8 + (Math.max(0, commandMarker.ageMs) / 180.0) * 8 : 0

            Rectangle {
                anchors.centerIn: parent
                width: 18
                height: 18
                radius: 9
                color: root.markerColor(commandMarker.markType)
                opacity: 0.18
            }

            Rectangle {
                anchors.centerIn: parent
                width: 26
                height: 26
                radius: 13
                color: "transparent"
                border.width: 1
                border.color: root.markerColor(commandMarker.markType)
                opacity: commandMarker.ageMs < 360 ? 0.75 * (1.0 - Math.max(0, commandMarker.ageMs) / 360.0) : 0.0
                scale: commandMarker.ageMs < 360 ? 0.65 + (Math.max(0, commandMarker.ageMs) / 360.0) * 0.95 : 1.6
            }

            Image {
                anchors.centerIn: parent
                source: root.markerSource(commandMarker.markType)
                width: 18 * root.width / 270.0
                height: 18 * root.height / 160.0
                smooth: true
                opacity: parent.opacity
            }
        }
    }

    // 4. 能量机关
    Rectangle {
        width: 30 * root.width / 270.0
        height: 30 * root.height / 160.0
        color: "transparent"
        anchors.centerIn: parent
        Image {
            source: root.runeSource()
            anchors.centerIn: parent
            anchors.fill: parent
            rotation: root.autoRotation
        }
        Text {
            anchors.centerIn: parent
            visible: root.runeDisplayText !== ""
            text: root.runeDisplayText
            color: "white"
            font.family: "Roboto"
            font.pixelSize: root.height * 0.1
            font.bold: true
            style: Text.Outline
            styleColor: "#A0000000"
        }
        visible: root.runeEventVisible
    }

    // 5. HUD 标题
    Text {
        text: "MINI MAP"
        color: "#E5E5EA"
        font.family: "Roboto"
        font.pixelSize: root.height * 0.05
        font.bold: true
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: root.height * 0.05
    }

    // 6. 交互层
    MouseArea {
        id: mapMouseArea

        anchors.fill: parent
        enabled: root.clickEnabled
        onPressed: mouse => {
            root.normX = mouse.x / mapMouseArea.width;
            root.normY = mouse.y / mapMouseArea.height;
            root.mapClicked(Math.max(0, Math.min(1, root.normX)), Math.max(0, Math.min(1, root.normY)));
        }
    }


}
