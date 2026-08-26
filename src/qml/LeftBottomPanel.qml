pragma ComponentBehavior: Bound

import QtQuick 2.15

/**
 * @file LeftBottomPanel.qml
 * @brief 左下角“我的机器人”面板（第 5 版视觉调整：透明背景）
 * @details

    1.Q按键测试金币兑换复活
 */
Item {
    id: root
    width: 460
    height: 200
    //确保组件本身透明
    clip: false

    // gameData 由 MainWindow 注入，面板内统一从这个入口读取机器人状态。
    // qmllint disable unqualified
    readonly property var gameDataContext: typeof gameData !== "undefined" ? gameData : null
    // qmllint enable unqualified

    // === 数据属性（绑定 gameData 上下文） ===
    property int currentHP: gameDataContext ? gameDataContext.currentHealth : 1 //当前血量
    property int maxHP: gameDataContext ? gameDataContext.maxHealth : 500 //最大血量值
    property int chassisEnergy: gameDataContext ? gameDataContext.chassisEnergy : 60 //底盘输出能量
    property int maxChassisEnergy: gameDataContext ? gameDataContext.maxChassisEnergy : 120 // 使用最大能量作为参考
    property int bufferEnergy: gameDataContext ? gameDataContext.bufferEnergy : 250 //缓冲能量条
    property int maxBufferEnergy: gameDataContext && gameDataContext.maxBufferEnergy !== undefined ? Number(gameDataContext.maxBufferEnergy) : 250

    // 机器人信息
    property int robotLevel: gameDataContext ? gameDataContext.robotLevel : 2
    property int maxlevel: gameDataContext && gameDataContext.maxlevel !== undefined ? Number(gameDataContext.maxlevel) : 10
    property int robotId: gameDataContext ? gameDataContext.robotId : 2
    property int displayRobotId: robotId % 100
    property string robotType:{
        switch(displayRobotId){
        case 1:
            return "hero";
        case 2:
            return "engineer";
        case 6:
            return "aerial";
        default:
            return "infantry";
        }
    }
    property int currentExp: gameDataContext ? gameDataContext.currentExp : 90
    property int maxExp: gameDataContext ? gameDataContext.maxExp : 100
    property bool isRed: (robotId < 100)
    property bool isAerialRobot: displayRobotId === 6
    property int yellowCardCount: gameDataContext ? gameDataContext.yellowCardCount : 0
    property string outOfCombatStatus: gameDataContext ? gameDataContext.outOfCombatStatus : "已脱战"

    property string chassisStatus: gameDataContext && gameDataContext.chassisStatus !== "" ? gameDataContext.chassisStatus : ""
    property bool chassisOverPowerCut: chassisStatus.indexOf("超限断电") === 0 //超限断电提示
    property bool isOutOfCombat: gameDataContext ? gameDataContext.isOutOfCombat : true
    readonly property real statsLeftMargin: root.isAerialRobot ? root.width * -0.03 : root.width * -0.10
    readonly property real statsVerticalOffset: root.isAerialRobot ? root.height * 0.02 : root.height * 0.08
    readonly property real hpBarTopMargin: root.isAerialRobot ? root.height * 0.11 : root.height * 0.18
    property int aerialHealthSegments: {
        var estimatedSegments = Math.round(Math.max(1, maxHP) / 30);
        return Math.max(15, Math.min(15, estimatedSegments));
    }
    property color aerialHealthActiveColor: healthRatio <= 0.20 ? "#FF8A8A" : "#FFFFFF"
    property color aerialHealthInactiveColor: Qt.rgba(1, 1, 1, 0.18)



    // 比例设置
    // 血量比例 - 限制在 0.0 ~ 1.0 之间
    property real healthRatio: {
        if (maxHP <= 0)
            return 0;
        return Math.min(1.0, Math.max(0.0, currentHP / maxHP));
    }
    //缓冲能量比例 - 限制在 0.0 ~ 1.0 之间
    property real bufferEnergyRatio: {
        if (maxBufferEnergy <= 0)
            return 0;
        return Math.min(1.0, Math.max(0.0, bufferEnergy / maxBufferEnergy));
    }
    //底盘输出比例 - 限制在 0.0 ~ 1.0 之间
    property real chassisEnergyRatio: {
        if (maxChassisEnergy <= 0)
            return 0;
        return Math.min(1.0, Math.max(0.0, chassisEnergy / maxChassisEnergy));
    }
    //经验比例 - 限制在 0.0 ~ 1.0 之间，避免经验溢出时圆弧绕出头像区域
    property real expRatio: {
        if (maxExp <= 0)
            return 0;
        return Math.min(1.0, Math.max(0.0, currentExp / maxExp));
    }

    // Buffs：LeftBottom 直接使用 GameData 传来的打包结构
    function emptyBuffData(typeId) {
        return ({ type: typeId, level: 0, maxTime: 0, leftTime: 0 })
    }
    property var buffTimedData: gameDataContext ? gameDataContext.buffTimedData : ({})
    property var attackBuff: buffTimedData.attack ? buffTimedData.attack : emptyBuffData(1)
    property var defenceBuff: buffTimedData.defense ? buffTimedData.defense : emptyBuffData(2)
    property var vulnerabilityBuff: buffTimedData.vulnerability ? buffTimedData.vulnerability : emptyBuffData(2)
    property var coolingBuff: buffTimedData.cooling ? buffTimedData.cooling : emptyBuffData(3)
    property var recoveryBuff: buffTimedData.recovery ? buffTimedData.recovery : emptyBuffData(5)
    property var terrainPrewarnBuff: buffTimedData.terrainPrewarn ? buffTimedData.terrainPrewarn : emptyBuffData(7)

    // 增益图标映射表：buff_level → 图标后缀
    // 协议 buff_level (1-5) 映射到实际图标数值后缀
    readonly property var coolingIconMap:   ({1: "2", 2: "2", 3: "3", 4: "3", 5: "5"})
    readonly property var attackIconMap:    ({1: "150", 2: "150", 3: "200", 4: "200", 5: "300"})
    readonly property var defenceIconMap:   ({1: "25", 2: "50", 3: "100", 4: "100", 5: "100"})
    readonly property var debuffIconMap:    ({1: "15", 2: "25", 3: "40", 4: "100", 5: "100"})
    readonly property var recoveryIconMap:  ({1: "10", 2: "25", 3: "25", 4: "25", 5: "25"})

    // 增益图标职责说明:
    //   cooling (冷却增益):    枪口热量冷却速度提升，绿色温度计图标
    //   attack (攻击增益):     伤害输出提升，红色攻击图标（能量机关激活获得）
    //   recovery (回血增益):   血量回复速度提升，绿色+号图标
    //   defence (防御增益):    承受伤害降低，蓝色护盾图标
    //   debuff (易伤/负防):   承受伤害增加，红色碎盾图标（debuff）

    function buffIconPath(prefix, level, levelMap) {
        if (level <= 0) return ""
        var suffix = levelMap[level] || level
        return "qrc:/images/buffs/" + prefix + "_" + suffix + ".png"
    }

    component BuffIcon: Item {
        id: buffIcon

        property var buffData: root.emptyBuffData(0)
        property string iconSource: ""
        property real iconScale: 0.9
        readonly property int buffLevel: Number(buffIcon.buffData.level) || 0
        readonly property int buffLeftTime: Number(buffIcon.buffData.leftTime) || 0
        readonly property int buffMaxTime: Number(buffIcon.buffData.maxTime) || 0

        width: root.height * 0.15
        height: root.height * 0.15
        visible: buffIcon.buffLevel > 0 && buffIcon.buffLeftTime > 1 && buffIcon.iconSource !== ""

        Canvas {
            anchors.fill: parent
            property int buffLeftTime: parent.buffLeftTime
            property int buffMaxTime: parent.buffMaxTime
            onBuffLeftTimeChanged: requestPaint()
            onBuffMaxTimeChanged: requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();
                ctx.beginPath();
                ctx.arc(parent.width / 2, parent.height / 2, parent.width / 2 - 2, 0, 2 * Math.PI);
                ctx.lineWidth = 2;
                ctx.strokeStyle = "#1A000000";
                ctx.stroke();
                if (buffLeftTime > 0 && buffMaxTime > 0) {
                    var end = -Math.PI / 2 + 2 * Math.PI * buffLeftTime / buffMaxTime;
                    ctx.beginPath();
                    ctx.arc(parent.width / 2, parent.height / 2, parent.width / 2 - 2, -Math.PI / 2, end);
                    ctx.lineWidth = 2;
                    ctx.strokeStyle = "#b3ecec";
                    ctx.stroke();
                }
            }
        }

        Image {
            height: parent.height * parent.iconScale
            width: parent.width * parent.iconScale
            anchors.centerIn: parent
            fillMode: Image.PreserveAspectFit
            source: parent.iconSource
        }
    }

    component AerialHealthDart: Item {
        property bool active: false
        property color strokeColor: "#FFFFFF"
        property color inactiveStrokeColor: Qt.rgba(1, 1, 1, 0.18)

        clip: false
        rotation: 30

        Canvas {
            width: parent.width
            height: parent.height * 1.5
            x: 0
            y: parent.height * 0.02
            antialiasing: true

            onPaint: {
                var ctx = getContext("2d");
                var w = width;
                var h = height;
                var outlineColor = parent.active ? parent.strokeColor : parent.inactiveStrokeColor;
                var lineWidth = Math.max(1.0, Math.min(w, h) * 0.12);

                function traceDartPath() {
                    ctx.beginPath();
                    ctx.moveTo(w * 0.50, 0);
                    ctx.quadraticCurveTo(w * 0.88, h * 0.10, w * 0.84, h * 0.28);
                    ctx.lineTo(w * 0.84, h * 0.84);
                    ctx.quadraticCurveTo(w * 0.84, h, w * 0.66, h);
                    ctx.lineTo(w * 0.34, h);
                    ctx.quadraticCurveTo(w * 0.16, h, w * 0.16, h * 0.84);
                    ctx.lineTo(w * 0.16, h * 0.28);
                    ctx.quadraticCurveTo(w * 0.12, h * 0.10, w * 0.50, 0);
                    ctx.closePath();
                }

                ctx.reset();
                ctx.clearRect(0, 0, w, h);
                ctx.strokeStyle = outlineColor;
                ctx.lineWidth = lineWidth;
                ctx.lineJoin = "round";
                ctx.lineCap = "round";

                traceDartPath();
                ctx.stroke();

                ctx.beginPath();
                ctx.moveTo(w * 0.31, h * 0.24);
                ctx.lineTo(w * 0.71, h * 0.24);
                ctx.moveTo(w * 0.25, h * 0.43);
                ctx.lineTo(w * 0.75, h * 0.43);
                ctx.moveTo(w * 0.22, h * 0.82);
                ctx.lineTo(w * 0.58, h * 0.82);
                ctx.moveTo(w * 0.30, h * 0.96);
                ctx.lineTo(w * 0.70, h * 0.96);
                ctx.stroke();
            }
        }
    }

    // 模块状态
    property var moduleStates: gameDataContext ? gameDataContext.moduleStates : [1, 1, 1, 1, 1, 1, 1, 1, 1, 1]

    property string teamPrefix: isRed ? "red" : "blue"
    property int safeRobotLevel: Math.max(1, Math.min(maxlevel, Math.round(Number(robotLevel) || 1)))
    property string levelIconPrimarySource: "qrc:/images/my_robot/" + teamPrefix + "_statusbar_level_" + safeRobotLevel + ".png"
    property string levelIconFallbackSource: "qrc:/images/top_robots/levels/tab_level_" + safeRobotLevel + ".png"
    property string avatarSource:"qrc:/images/my_robot/" + teamPrefix +
                        "_" + robotType + ".png"



    // =============================================
    // 机器人状态区域（贴底显示）
    // =============================================
    Item {
        id: robotStatusArea
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.height * 1.00 //230

        // 2.1 头像区域 (放大 + 紧凑)
        Item {
            id: avatarArea
            width: root.height * 0.80
            height: root.height * 0.80
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: root.width * 0.01 //10

            // 经验值圆弧
            Canvas {
                anchors.fill: parent
                property real progress: root.expRatio
                onProgressChanged: requestPaint()
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                onPaint: {
                    var ctx = getContext("2d");
                    ctx.reset();
                    ctx.clearRect(0, 0, width, height);
                    var centerX = width / 2;
                    var centerY = height / 2;
                    var radius = width / 2 - root.height * 0.10;

                    ctx.beginPath();
                    ctx.arc(centerX, centerY, radius, Math.PI * 0.75, Math.PI * 1.18, false);
                    ctx.lineWidth = root.height * 0.04;
                    ctx.strokeStyle = "#40000000";
                    ctx.stroke();

                    if (progress > 0) {
                        var end = Math.PI * 0.75 + (Math.PI * 0.43 * progress);
                        ctx.beginPath();
                        ctx.arc(centerX, centerY, radius, Math.PI * 0.75, end, false);
                        ctx.lineWidth = root.height * 0.03;
                        ctx.strokeStyle = "#FFD700";
                        ctx.stroke();
                    }
                }
            }

            // 机器人头像
            Image {
                anchors.centerIn: parent
                width: root.height * 0.60
                height: root.height * 0.60
                source: root.avatarSource
                fillMode: Image.PreserveAspectFit
            }

            // 等级图标
            Image {
                id: levelIconPrimary
                width: root.height * 0.20
                height: root.height * 0.20
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.leftMargin: root.width * 0.04
                anchors.topMargin: root.height * 0.10
                source: root.levelIconPrimarySource
                fillMode: Image.PreserveAspectFit
            }

            // 资源回退：如果 my_robot 等级图标缺失，则使用 top_robots 等级图标
            Image {
                width: root.height * 0.20
                height: root.height * 0.20
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.leftMargin: root.width * 0.03
                anchors.topMargin: root.height * 0.08
                source: root.levelIconFallbackSource
                fillMode: Image.PreserveAspectFit
                visible: levelIconPrimary.status === Image.Error
            }

            // ID
            Item {
                width: root.height * 0.23
                height: root.height * 0.23
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.leftMargin: root.width * 0.10
                anchors.bottomMargin: root.height * 0.09
                Image{
                    anchors.fill: parent
                    source: "qrc:/images/my_robot/" + root.teamPrefix + "_statusbar_id_border.png"
                    fillMode: Image.PreserveAspectFit
                }

                Text {
                    anchors.centerIn: parent
                    text: root.displayRobotId
                    color: "white"
                    font.bold: true
                    font.pixelSize: root.height * 0.10
                }
            }

            // 黄牌警告 (机器人头像左下角)
            Item {
                width: root.width * 0.08
                height: root.height * 0.10
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.leftMargin: root.width * 0.00//-10
                anchors.bottomMargin: root.height * 0.08
                visible: root.yellowCardCount > 0

                Rectangle {
                    anchors.fill: parent
                    color: "black"
                    opacity: 0.6
                }

                Row {
                    anchors.centerIn: parent
                    spacing: root.width * 0.0043
                    Image {
                        width: root.height * 0.09
                        height: root.height * 0.09
                        source: "qrc:/images/my_robot/statusbar_warning_yellow.png"
                        fillMode: Image.PreserveAspectFit
                    }
                    Text {
                        text: "x" + root.yellowCardCount
                        color: "white"
                        font.pixelSize: root.height * 0.05
                        font.bold: true
                        style: Text.Outline
                        styleColor: "black"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }

        // 2.2 数据条 (紧贴头像)
        Item {
            id: statsArea
            anchors.left: avatarArea.right
            anchors.leftMargin: root.statsLeftMargin
            anchors.right: parent.right
            anchors.verticalCenter: avatarArea.verticalCenter
            anchors.verticalCenterOffset: root.statsVerticalOffset
            height: root.height * 0.50
            z: -1 // 置于头像下方

            // Buffs 顺序(冷却、攻击、回血、防御、负防御、飞坡)
            Row {
                anchors.left: hpBar.left
                anchors.leftMargin: root.width * 0.03 // 为倾斜造型预留偏移
                anchors.bottom: hpBar.top
                anchors.bottomMargin: root.height * 0.03  //0.00
                spacing: root.width * 0.03
                BuffIcon {
                    buffData: root.coolingBuff
                    iconSource: root.buffIconPath("cool", root.coolingBuff.level, root.coolingIconMap)
                }
                BuffIcon {
                    buffData: root.attackBuff
                    iconSource: root.buffIconPath("attack", root.attackBuff.level, root.attackIconMap)
                }
                BuffIcon {
                    buffData: root.recoveryBuff
                    iconScale: 0.72
                    iconSource: root.buffIconPath("addhp", root.recoveryBuff.level, root.recoveryIconMap)
                }
                BuffIcon {
                    buffData: root.defenceBuff
                    iconScale: 0.72
                    iconSource: root.buffIconPath("defence", root.defenceBuff.level, root.defenceIconMap)
                }
                BuffIcon {
                    buffData: root.vulnerabilityBuff
                    iconScale: 0.72
                    iconSource: root.buffIconPath("debuff", root.vulnerabilityBuff.level, root.debuffIconMap)
                }
            }

            // 血条：参照官方客户端比例缩小
            Item {
                id: hpBar
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.topMargin: root.hpBarTopMargin
                anchors.leftMargin: root.width * -0.01

                width: root.width * 0.51
                height: root.isAerialRobot ? root.height * 0.24 : root.height * 0.15

                // 脱战提示 (血量条右方)
                Item {
                    anchors.left: parent.right
                    anchors.leftMargin: root.width * 0.01
                    anchors.verticalCenter: parent.verticalCenter
                    width: root.width * 0.22
                    height: root.height * 0.14

                    Row {
                        spacing: root.width * 0.0043
                        anchors.verticalCenter: parent.verticalCenter

                        // 图标
                        Image {
                            width: root.height * 0.14
                            height: root.height * 0.14
                            source: "qrc:/images/my_robot/" + (root.isOutOfCombat ? "outofcombat_icon.png" : "combat_icon.png")
                            fillMode: Image.PreserveAspectFit
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        // 文字
                        Text {
                            text: root.outOfCombatStatus
                            color: root.isOutOfCombat ? "#00FF7F" : "#FFD700"
                            font.pixelSize: root.height * 0.07
                            font.bold: true
                            style: Text.Outline
                            styleColor: "black"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }

                // 血条边框背景
                Image {
                    id: bgImage
                    anchors.fill: parent
                    visible: !root.isAerialRobot
                    source: "qrc:/images/my_robot/"  + root.teamPrefix + "_statusbar_blood_background.png"
                    fillMode: Image.Stretch
                }
                // 血量增益背景
                Image {
                    id: recoveryBuffImage
                    anchors.top: parent.top
                    anchors.topMargin: root.height * -0.07
                    anchors.left: parent.left
                    anchors.leftMargin: root.width * (-0.02)
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: root.height * (-0.07)
                    width: parent.width *1.075
                    visible: !root.isAerialRobot && root.recoveryBuff.level > 0 && root.recoveryBuff.leftTime > 0
                    source: "qrc:/images/my_robot/blood_addbuff.png"
                    fillMode: Image.Stretch
                }

                // 血量填充层（裁剪显示）
                Item {
                    id: fillContainer
                    anchors.top: parent.top
                    anchors.topMargin: root.height * -0.03
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    // 宽度根据血量比例计算
                    width: parent.width * root.healthRatio  // 使用限制后的血量比例
                    clip: true
                    visible: !root.isAerialRobot

                    Image {
                        width: hpBar.width + root.width * 0.01    // 填充层宽度与血条宽度一致
                        height: hpBar.height + root.height * 0.05
                        anchors.left: parent.left
                        anchors.top: parent.top
                        source: "qrc:/images/my_robot/" + root.teamPrefix + "_statusbar_blood_slid.png"
                        fillMode: Image.Stretch
                    }
                }

                Item {
                    id: aerialHealthTrack
                    visible: root.isAerialRobot
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: root.width * 0.01
                    anchors.rightMargin: root.width * 0.01
                    anchors.top: parent.top
                    anchors.topMargin: root.height * 0.04
                    height: root.height * 0.20



                    Row {
                        id: aerialHealthRow
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: root.width * 0.0065

                        Repeater {
                            model: root.aerialHealthSegments

                            delegate: AerialHealthDart {
                                id: aerialHealthDart

                                required property int index
                                readonly property real filledSegments: root.healthRatio * root.aerialHealthSegments

                                width: Math.max(root.width * 0.018, (aerialHealthTrack.width - (root.aerialHealthSegments - 1) * aerialHealthRow.spacing) / root.aerialHealthSegments)
                                height: aerialHealthTrack.height
                                active: aerialHealthDart.index < aerialHealthDart.filledSegments
                                strokeColor: root.aerialHealthActiveColor
                                inactiveStrokeColor: root.aerialHealthInactiveColor
                            }
                        }
                    }
                }


                // HP 数值文本
                Text {
                    visible: !root.isAerialRobot
                    anchors.centerIn: parent
                    text: root.currentHP + " / " + root.maxHP
                    color: "white"
                    font.pixelSize: root.height * 0.10
                    font.bold: true
                    style: Text.Outline
                    styleColor: "black"
                }

            }

            // 能量条宽度不超过血条
            Item {
                id: energyBarsContainer
                visible: !root.isAerialRobot
                anchors.left: hpBar.left
                anchors.top: hpBar.bottom
                anchors.topMargin: root.height * 0.01
                width: hpBar.width               // 宽度与血条一致
                height: root.height * 0.10

                 // 缓冲能量条
                Item {
                    id: bufferBar
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.topMargin: root.height * 0.01
                    anchors.leftMargin: root.width * -0.03

                    width: energyBarsContainer.width*0.80
                    height: root.height * 0.03
                    z: -1 // 置于头像下方

                    // 边框背景
                    Image {
                        id: bgImage1
                        anchors.fill: parent
                        source: "qrc:/images/my_robot/statusbar_energy_background.png"
                        fillMode: Image.Stretch
                    }

                    // 填充层（裁剪显示）
                    Item {
                        id: fillContainer1
                        anchors.top: parent.top
                        anchors.topMargin: root.height * -0.03
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        // 宽度根据血量比例计算
                        width: parent.width * root.bufferEnergyRatio
                        clip: true

                        Image {
                            width: bufferBar.width + root.width * 0.01    // 填充层宽度与血条宽度一致
                            height: bufferBar.height + root.height * 0.05
                            anchors.left: parent.left
                            anchors.top: parent.top
                            source: "qrc:/images/my_robot/statusbar_energy.png"
                            fillMode: Image.Stretch
                        }
                    }

                }
                // 功率文本
                Text {
                    id: powerText
                    anchors.top: parent.top
                    anchors.left: bufferBar.right
                    anchors.leftMargin: root.width * 0.01
                    text: root.chassisStatus
                    color: root.chassisOverPowerCut ? "red" : "white"   //超限断电显示红色，底盘功率增益显示白色
                    font.pixelSize: root.height * 0.04
                    font.bold: true
                }
                // 底盘能量条 (青色)
                Item {
                    id: bufferBar2
                    anchors.left: parent.left
                    anchors.top: bufferBar.bottom
                    anchors.topMargin: root.height * 0.02
                    anchors.leftMargin: root.width * -0.04
                    width: energyBarsContainer.width*0.80
                    height: root.height * 0.03
                    z: -1 // 置于头像下方

                    // 边框背景
                    Image {
                        id: bgImage2
                        anchors.fill: parent
                        source: "qrc:/images/my_robot/statusbar_energy_background.png"
                        fillMode: Image.Stretch
                    }

                    // 填充层（裁剪显示）
                    Item {
                        id: fillContainer2
                        anchors.top: parent.top
                        anchors.topMargin: root.height * -0.03
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        // 宽度根据血量比例计算
                        width: parent.width * root.chassisEnergyRatio
                        clip: true

                        Image {
                            width: bufferBar2.width + root.width * 0.01    // 填充层宽度与血条宽度一致
                            height: bufferBar2.height + root.height * 0.05
                            anchors.left: parent.left
                            anchors.top: parent.top
                            source: "qrc:/images/my_robot/statusbar_energy.png"
                            fillMode: Image.Stretch
                        }
                    }
                }
            }
        }

        // 2.3 模块状态 (底部)
        Item {
            id: moduleStatusArea
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: statsArea.bottom
            anchors.leftMargin: root.width * 0.08
            anchors.rightMargin: root.width * 0.03
            height: root.height * 0.20

            property var moduleNames: ["机器人", "遥控器", "自定义", "图传", "RFID识别",
                                             "模块", "UWB", "底盘", "17mm", "42mm"]

            Grid {
                anchors.fill: parent
                columns: 5
                spacing: root.width * 0.0043

                Repeater {
                    model: 10
                    Item {
                        id: moduleStatusItem

                        required property int index
                        width: root.width * 0.13
                        height: root.height * 0.10
                        Image{
                            id: moduleStateImage

                            width: root.width * 0.03
                            height: root.height * 0.09
                            anchors.left: parent.left
                            //记录颜色
                            property string moduleState: (root.moduleStates && root.moduleStates[moduleStatusItem.index] === 0) ? "green" : "red"
                            source: "qrc:/images/my_robot/statusbar_signallamp_" + moduleStateImage.moduleState + "_single.png"
                            fillMode: Image.Stretch
                        }
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: root.width * 0.03
                            anchors.verticalCenter: parent.verticalCenter
                            text: moduleStatusArea.moduleNames[moduleStatusItem.index]
                            color: "white"
                            font.pixelSize: root.height * 0.06
                            style: Text.Outline
                            styleColor: "black"
                        }
                    }
                }
            }
        }
    }
}
