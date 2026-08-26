pragma ComponentBehavior: Bound

import QtQuick 2.15
import "components"

Rectangle {
    id: root

    signal loginButtonClicked()

    anchors.fill: parent
    color: "#02070d"

    TacticalMock { id: mockData }

    // 上下文对象由 MainWindow 注入，统一在页面入口适配，避免内部节点依赖隐式作用域。
    // qmllint disable unqualified
    readonly property var tacticalAnalyzerContext: typeof tacticalAnalyzer !== "undefined" ? tacticalAnalyzer : null
    readonly property var gameDataContext: typeof gameData !== "undefined" ? gameData : null
    readonly property var mainWindowContext: typeof mainWindow !== "undefined" ? mainWindow : null
    // qmllint enable unqualified

    readonly property int designWidth: 1672
    readonly property int designHeight: 850
    readonly property int designSurfaceTop: Math.round(104 * height / 1080)
    readonly property int designSurfaceBottomMargin: 2
    readonly property real designScale: Math.min(
        width / designWidth,
        Math.max(
            0.1,
            (height - root.designSurfaceTop - root.designSurfaceBottomMargin) / designHeight
        )
    )
    readonly property int centerPanelY: 0
    readonly property var dataSource: root.tacticalAnalyzerContext
        ? root.tacticalAnalyzerContext : mockData
    readonly property var header: root.dataSource.headerData || mockData.headerData
    readonly property var resources: root.dataSource.resourceData || mockData.resourceData
    readonly property var topStatus: root.dataSource.topStatusData || mockData.topStatusData
    readonly property var linkHealth: root.dataSource.linkHealth || mockData.linkHealth
    readonly property bool allyIsRed: root.gameDataContext ? (
        root.gameDataContext.myRobot && root.gameDataContext.myRobot.isRedTeam !== undefined
            ? Boolean(root.gameDataContext.myRobot.isRedTeam)
            : Number(root.gameDataContext.currentRobotId || 1) < 100
    ) : true
    readonly property bool allyIsBlue: !root.allyIsRed
    readonly property bool enemyIsBlue: root.allyIsRed
    readonly property bool tacticalMapViewMirrored: root.allyIsBlue
    readonly property string tacticalMapBackgroundSource: root.allyIsBlue
        ? "qrc:/images/minimap_bg_blue_left.png"
        : "qrc:/images/minimap_bg_red_left.png"
    readonly property string redTeamName: root.gameDataContext
        ? (root.gameDataContext.redTeamName || "复旦大学 星云EGA") : "复旦大学 星云EGA"
    readonly property string blueTeamName: root.gameDataContext
        ? (root.gameDataContext.blueTeamName || "上海交通大学 交龙") : "上海交通大学 交龙"
    readonly property string allyTeamName: root.allyIsRed ? root.redTeamName : root.blueTeamName
    readonly property string enemyTeamName: root.allyIsRed ? root.blueTeamName : root.redTeamName
    readonly property int allyScore: root.allyIsRed
        ? Number(root.header.redScore || 0)
        : Number(root.header.blueScore || 0)
    readonly property int enemyScore: root.allyIsRed
        ? Number(root.header.blueScore || 0)
        : Number(root.header.redScore || 0)
    readonly property string layoutShortcut: "Ctrl+Y Layout"
    readonly property string layoutMode: root.dataSource.layoutMode || mockData.layoutMode || "map_primary"
    readonly property var allyRobotsForMap: (root.dataSource.allyRobotList
        && root.dataSource.allyRobotList.length > 0) ? root.dataSource.allyRobotList : mockData.allyRobotList
    readonly property var enemyRobotsForMap: (root.dataSource.enemyRobotList
        && root.dataSource.enemyRobotList.length > 0) ? root.dataSource.enemyRobotList : mockData.enemyRobotList
    readonly property var tacticalMapModel: root.dataSource.mapData
        || root.dataSource.radarData || mockData.radarData
    readonly property bool largeMapMode: root.mainWindowContext
        ? Boolean(root.mainWindowContext.tacticalLargeMapMode) : false
    readonly property bool largeMapOverlayReady: tacticalLargeMapLoader.active
        && tacticalLargeMapLoader.status === Loader.Ready
        && tacticalLargeMapLoader.item !== null
        && tacticalLargeMapLoader.visible
    property string selectedRobotId: ""
    property string selectedEnemyId: ""
    // 工业相机帧独立于 100 ms 战术快照刷新；仅在指挥屏显示时启用，
    // 避免隐藏页面仍持续拉取 image provider。
    property bool cameraRefreshEnabled: false
    property bool heroCameraGridVisible: false
    property real heroCameraFps: 0
    property real heroCameraLastRevision: 0
    property bool heroCameraFpsInitialized: false
    readonly property bool liveCameraSourceAllowed: root.cameraRefreshEnabled
        && !root.largeMapMode
        && !(root.tacticalAnalyzerContext
             ? Boolean(root.tacticalAnalyzerContext.useMockData) : false)
    readonly property bool hasHeroCameraFrame: root.liveCameraSourceAllowed
        && root.gameDataContext
        && Boolean(root.gameDataContext.hasHeroFrame)
    readonly property string heroCameraFrameSource: root.hasHeroCameraFrame
        ? String(root.gameDataContext.heroFrameSource) : ""

    function resetHeroCameraFpsMeter() {
        root.heroCameraFps = 0
        root.heroCameraLastRevision = 0
        root.heroCameraFpsInitialized = false
    }

    function sampleHeroCameraFps() {
        if (!root.hasHeroCameraFrame) {
            root.resetHeroCameraFpsMeter()
            return
        }
        var revision = Number(root.gameDataContext.heroFrameRevision)
        if (root.heroCameraFpsInitialized) {
            root.heroCameraFps = Math.max(0, revision - root.heroCameraLastRevision)
        } else {
            root.heroCameraFpsInitialized = true
            root.heroCameraFps = 0
        }
        root.heroCameraLastRevision = revision
    }

    onLiveCameraSourceAllowedChanged: resetHeroCameraFpsMeter()

    Timer {
        interval: 1000
        repeat: true
        running: root.liveCameraSourceAllowed
        onTriggered: root.sampleHeroCameraFps()
    }

    function selectRobot(robotId, isEnemy) {
        if (isEnemy) {
            root.selectedEnemyId = robotId
            root.selectedRobotId = ""
        } else {
            root.selectedRobotId = robotId
            root.selectedEnemyId = ""
        }
    }

    function clearSelection() {
        root.selectedRobotId = ""
        root.selectedEnemyId = ""
    }
    readonly property bool videoPrimary: root.layoutMode === "video_primary"
    // 参考画布按 1920x1080 设计。横纵缩放分别计算，避免超宽屏或高分屏拉伸 HUD。
    readonly property real topHudScaleX: width / 1920
    readonly property real topHudScaleY: height / 1080
    readonly property real topHudScale: Math.min(topHudScaleX, topHudScaleY)
    readonly property int topHudCenterWidth: Math.round(376 * topHudScaleX)
    readonly property int topHudWingWidth: Math.round(484 * topHudScaleX)
    readonly property int topHudWingHeight: Math.round(84 * topHudScaleY)
    readonly property int topHudLogisticsWidth: Math.round(1260 * topHudScaleX)
    readonly property int topHudLogisticsHeight: Math.round(68 * topHudScaleY)
    // 中部和底部面板整体在顶部 HUD 与屏幕底边之间居中，内部间距仍使用紧凑的设计值。
    readonly property int lowerPanelsGapY: 12
    readonly property int topHudContentBottomY: Math.round(164 * topHudScaleY)
    readonly property real unusedDesignHeight: Math.max(0,
        (height - designSurfaceTop - designSurfaceBottomMargin) / designScale - designHeight)
    // 保留原有战场区和地图区的行高比例。
    readonly property int bottomPanelExtraHeight: Math.round(unusedDesignHeight * 0.35)
    readonly property int mapMainPanelHeight: 474
    readonly property int videoMainPanelHeight: 496
    readonly property int mapBottomPanelHeight: 268 + bottomPanelExtraHeight
    readonly property int videoBottomPanelHeight: 252 + bottomPanelExtraHeight
    readonly property int activeMainPanelHeight: videoPrimary
        ? videoMainPanelHeight : mapMainPanelHeight
    readonly property int activeBottomPanelHeight: videoPrimary
        ? videoBottomPanelHeight : mapBottomPanelHeight
    readonly property int middleRowOffsetY: -10
    readonly property int bottomRowOffsetY: 4
    readonly property int lowerPanelsTopY: Math.max(0, Math.round(
        (height - designSurfaceBottomMargin - 2 * designSurfaceTop
            + topHudContentBottomY
            - designScale * (activeMainPanelHeight + lowerPanelsGapY
                + activeBottomPanelHeight))
            / (2 * designScale)
    ))
    readonly property var mapLayout: ({
        left: { x: 0, y: root.lowerPanelsTopY + root.middleRowOffsetY, w: 404, h: root.mapMainPanelHeight },
        center: { x: 416, y: root.lowerPanelsTopY + root.middleRowOffsetY, w: 840, h: root.mapMainPanelHeight },
        right: { x: 1268, y: root.lowerPanelsTopY + root.middleRowOffsetY, w: 404, h: root.mapMainPanelHeight },
        events: { x: 0, y: root.lowerPanelsTopY + root.mapMainPanelHeight + root.lowerPanelsGapY + root.bottomRowOffsetY, w: 404, h: root.mapBottomPanelHeight },
        analysis: { x: 416, y: root.lowerPanelsTopY + root.mapMainPanelHeight + root.lowerPanelsGapY + root.bottomRowOffsetY, w: 764, h: root.mapBottomPanelHeight },
        aux: { x: 1192, y: root.lowerPanelsTopY + root.mapMainPanelHeight + root.lowerPanelsGapY + root.bottomRowOffsetY, w: 480, h: root.mapBottomPanelHeight }
    })
    readonly property var videoLayout: ({
        left: { x: 0, y: root.lowerPanelsTopY + root.middleRowOffsetY, w: 382, h: root.videoMainPanelHeight },
        center: { x: 394, y: root.lowerPanelsTopY + root.middleRowOffsetY, w: 884, h: root.videoMainPanelHeight },
        right: { x: 1290, y: root.lowerPanelsTopY + root.middleRowOffsetY, w: 382, h: root.videoMainPanelHeight },
        events: { x: 0, y: root.lowerPanelsTopY + root.videoMainPanelHeight + root.lowerPanelsGapY + root.bottomRowOffsetY, w: 392, h: root.videoBottomPanelHeight },
        analysis: { x: 404, y: root.lowerPanelsTopY + root.videoMainPanelHeight + root.lowerPanelsGapY + root.bottomRowOffsetY, w: 640, h: root.videoBottomPanelHeight },
        aux: { x: 1056, y: root.lowerPanelsTopY + root.videoMainPanelHeight + root.lowerPanelsGapY + root.bottomRowOffsetY, w: 616, h: root.videoBottomPanelHeight }
    })

    function layoutRect(name) {
        return (root.videoPrimary ? root.videoLayout : root.mapLayout)[name]
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#02070d" }
            GradientStop { position: 0.48; color: "#050b11" }
            GradientStop { position: 1.0; color: "#02080d" }
        }
    }

    // ═══════════════════════════════════════════════════
    // designSurface 只承载下方面板，并受 designScale 缩放
    // ═══════════════════════════════════════════════════
    Item {
        id: designSurface
        width: root.designWidth
        height: root.designHeight
        x: Math.round((root.width - width * root.designScale) / 2)
        y: root.designSurfaceTop
        scale: root.designScale
        transformOrigin: Item.TopLeft
        z: 0

        Rectangle {
            anchors.fill: parent
            color: "#02070d"
        }

        Image {
            anchors.fill: parent
            source: root.tacticalMapBackgroundSource
            fillMode: Image.PreserveAspectCrop
            opacity: 0.055
        }

        Canvas {
            anchors.fill: parent
            opacity: 0.24
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.strokeStyle = "rgba(46, 177, 230, 0.10)"
                ctx.lineWidth = 1
                for (var x = 0; x < width; x += 96) {
                    ctx.beginPath()
                    ctx.moveTo(x, 0)
                    ctx.lineTo(x, height)
                    ctx.stroke()
                }
                for (var y = 0; y < height; y += 64) {
                    ctx.beginPath()
                    ctx.moveTo(0, y)
                    ctx.lineTo(width, y)
                    ctx.stroke()
                }
            }
        }

        RobotListPanel {
            x: root.layoutRect("left").x
            y: root.layoutRect("left").y
            width: root.layoutRect("left").w
            height: root.layoutRect("left").h
            robots: root.allyRobotsForMap
            enemy: false
            blueTeam: root.allyIsBlue
            pageRoot: root
        }

        RobotListPanel {
            x: root.layoutRect("right").x
            y: root.layoutRect("right").y
            width: root.layoutRect("right").w
            height: root.layoutRect("right").h
            robots: root.enemyRobotsForMap
            enemy: true
            blueTeam: root.enemyIsBlue
            pageRoot: root
        }

        TacticalRadarMap {
            objectName: "tacticalCenterMap"
            x: root.layoutRect("center").x
            y: root.layoutRect("center").y
            width: root.layoutRect("center").w
            height: root.layoutRect("center").h
            visible: !root.videoPrimary && !root.largeMapMode
            model: root.dataSource.mapData || root.dataSource.radarData || mockData.radarData
            allyStatusModel: root.allyRobotsForMap
            enemyStatusModel: root.enemyRobotsForMap
            allyIsBlue: root.allyIsBlue
            enemyIsBlue: root.enemyIsBlue
            backgroundSource: root.tacticalMapBackgroundSource
            viewMirrored: root.tacticalMapViewMirrored
            pageRoot: root
        }

        CameraPreviewPanel {
            objectName: "tacticalCenterCamera"
            x: root.layoutRect("center").x
            y: root.layoutRect("center").y
            width: root.layoutRect("center").w
            height: root.layoutRect("center").h
            visible: root.videoPrimary && !root.largeMapMode
            centralMode: true
            showRealVideo: root.videoPrimary && !root.largeMapMode
            showGrid: root.heroCameraGridVisible
            liveFrameSource: root.heroCameraFrameSource
            liveFps: root.hasHeroCameraFrame ? root.heroCameraFps : -1
            status: root.dataSource.cameraPreviewData || mockData.cameraPreviewData
            gameActive: root.gameDataContext
                && root.gameDataContext.gamePhase !== "未开始"
        }

        BattleEventPanel {
            x: root.layoutRect("events").x
            y: root.layoutRect("events").y
            width: root.layoutRect("events").w
            height: root.layoutRect("events").h
            events: root.dataSource.eventData || root.dataSource.keyEvents || mockData.keyEvents
        }

        BattleAnalysisPanel {
            x: root.layoutRect("analysis").x
            y: root.layoutRect("analysis").y
            width: root.layoutRect("analysis").w
            height: root.layoutRect("analysis").h
            metrics: root.dataSource.analysisMetrics || mockData.analysisMetrics
            decision: root.dataSource.mainDecision || mockData.mainDecision
        }

        CameraPreviewPanel {
            objectName: "tacticalAuxCamera"
            x: root.layoutRect("aux").x
            y: root.layoutRect("aux").y
            width: root.layoutRect("aux").w
            height: root.layoutRect("aux").h
            visible: !root.videoPrimary && !root.largeMapMode
            showRealVideo: !root.videoPrimary && !root.largeMapMode
            showGrid: root.heroCameraGridVisible
            liveFrameSource: root.heroCameraFrameSource
            liveFps: root.hasHeroCameraFrame ? root.heroCameraFps : -1
            status: root.dataSource.cameraPreviewData || mockData.cameraPreviewData
            gameActive: root.gameDataContext
                && root.gameDataContext.gamePhase !== "未开始"
        }

        TacticalRadarMap {
            objectName: "tacticalAuxMap"
            x: root.layoutRect("aux").x
            y: root.layoutRect("aux").y
            width: root.layoutRect("aux").w
            height: root.layoutRect("aux").h
            visible: root.videoPrimary && !root.largeMapMode
            model: root.dataSource.mapData || root.dataSource.radarData || mockData.radarData
            allyStatusModel: root.allyRobotsForMap
            enemyStatusModel: root.enemyRobotsForMap
            allyIsBlue: root.allyIsBlue
            enemyIsBlue: root.enemyIsBlue
            backgroundSource: root.tacticalMapBackgroundSource
            viewMirrored: root.tacticalMapViewMirrored
            pageRoot: root
        }
    }

    // ═══════════════════════════════════════════════════
    // 顶部区域位于根层级，不参与缩放，与 TopInfoBar 保持 1:1
    // ═══════════════════════════════════════════════════

    Rectangle {
        x: 0
        y: 0
        width: parent.width
        height: Math.round(148 * root.topHudScaleY)
        z: 1
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.88) }
            GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.12) }
        }
    }

    Item {
        id: topHeader
        anchors.left: parent.left
        anchors.leftMargin: 24
        anchors.right: parent.right
        anchors.rightMargin: 24
        y: 5
        height: 20
        z: 2

        Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 14

            Repeater {
                model: [
                    { name: "DATA", value: (root.linkHealth.mqttLatencyMs || "--") + "ms", ok: true },
                    { name: "CAM", value: root.linkHealth.videoStatus === "ok" ? "LIVE" : "WAIT", ok: root.linkHealth.videoStatus === "ok" },
                    { name: "MAP", value: (root.linkHealth.radarAgeMs || "--") + "ms", ok: root.linkHealth.radarStatus === "ok" }
                ]

                Row {
                    id: linkStatusBadge
                    required property var modelData

                    spacing: 5

                    Rectangle {
                        width: 6
                        height: 6
                        radius: 3
                        anchors.verticalCenter: parent.verticalCenter
                        color: linkStatusBadge.modelData.ok ? "#18ff62" : "#ffae23"
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: linkStatusBadge.modelData.name + " " + linkStatusBadge.modelData.value
                        color: linkStatusBadge.modelData.ok ? "#e6fff2" : "#ffd28b"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }
            }

        }
    }

    HudPanel {
        id: respawnEconomyCard

        x: Math.round(24 * root.topHudScaleX)
        y: Math.round(31 * root.topHudScaleY)
        width: Math.round(230 * root.topHudScaleX)
        height: Math.round(48 * root.topHudScaleY)
        z: 3
        visible: Boolean(root.topStatus.respawnEconomyVisible)
        accent: root.allyIsBlue ? "#18b9ff" : "#ff3142"
        panelOpacity: 0.86
        showDiagonalTexture: false
        cutSizeOverride: Math.max(6, Math.round(9 * root.topHudScale))

        Row {
            anchors.fill: parent
            anchors.leftMargin: Math.round(10 * root.topHudScaleX)
            anchors.rightMargin: Math.round(10 * root.topHudScaleX)
            spacing: Math.round(8 * root.topHudScaleX)

            Column {
                width: Math.round(100 * root.topHudScaleX)
                anchors.verticalCenter: parent.verticalCenter
                spacing: 0

                Text {
                    width: parent.width
                    text: "买活金币"
                    color: "#b9cfdb"
                    font.pixelSize: Math.max(9, Math.round(10 * root.topHudScale))
                    font.bold: true
                }

                Text {
                    width: parent.width
                    text: Number(root.topStatus.respawnGoldCost || 0) > 0
                        ? String(Number(root.topStatus.respawnGoldCost))
                        : ""
                    color: "#ffd45d"
                    font.pixelSize: Math.max(14, Math.round(18 * root.topHudScale))
                    font.bold: true
                }
            }

            Rectangle {
                width: 1
                height: Math.round(28 * root.topHudScaleY)
                anchors.verticalCenter: parent.verticalCenter
                color: "#315066"
            }

            Column {
                width: Math.round(76 * root.topHudScaleX)
                anchors.verticalCenter: parent.verticalCenter
                spacing: 0

                Text {
                    width: parent.width
                    text: "可买活"
                    color: "#b9cfdb"
                    font.pixelSize: Math.max(9, Math.round(10 * root.topHudScale))
                    font.bold: true
                }

                Text {
                    width: parent.width
                    text: Number(root.topStatus.respawnGoldCost || 0) > 0
                        ? String(Number(root.topStatus.affordableRespawnCount || 0)) + " 次"
                        : ""
                    color: "#f3fbff"
                    font.pixelSize: Math.max(14, Math.round(18 * root.topHudScale))
                    font.bold: true
                }
            }
        }
    }

    Item {
        id: centerInfo
        anchors.horizontalCenter: parent.horizontalCenter
        y: root.centerPanelY
        width: root.topHudCenterWidth
        height: Math.round(98 * root.topHudScaleY)
        z: 2

        Image {
            anchors.fill: parent
            source: "qrc:/images/top_middle_background_v2.png"
            fillMode: Image.PreserveAspectFit
            opacity: 0.94
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 5
            text: "Round " + (root.header.currentRound || "-") + "/" + (root.header.totalRounds || 5)
            color: "#edf6ff"
            font.pixelSize: Math.round(11 * root.topHudScale)
        }

        Text {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: 11
            text: root.header.timeRemaining || "0:00"
            color: "#ffffff"
            font.pixelSize: Math.round(26 * root.topHudScale)
            font.bold: true
        }

        Text {
            anchors.left: parent.left
            anchors.leftMargin: Math.round(44 * root.topHudScaleX)
            anchors.verticalCenter: parent.verticalCenter
            width: Math.round(72 * root.topHudScaleX)
            text: String(root.allyScore)
            color: root.allyIsBlue ? "#178cff" : "#ff2637"
            font.pixelSize: Math.round(26 * root.topHudScale)
            font.bold: true
            style: Text.Outline
            styleColor: "#001018"
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }

        Text {
            anchors.right: parent.right
            anchors.rightMargin: Math.round(44 * root.topHudScaleX)
            anchors.verticalCenter: parent.verticalCenter
            width: Math.round(72 * root.topHudScaleX)
            text: String(root.enemyScore)
            color: root.enemyIsBlue ? "#178cff" : "#ff2637"
            font.pixelSize: Math.round(26 * root.topHudScale)
            font.bold: true
            style: Text.Outline
            styleColor: "#180006"
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }
    }

    BaseHealthWing {
        id: allyWing
        anchors.right: centerInfo.left
        anchors.rightMargin: Math.round(10 * root.topHudScaleX)
        y: Math.round(4 * root.topHudScaleY)
        width: root.topHudWingWidth
        height: root.topHudWingHeight
        z: 2
        enemy: false
        isBlueTeam: root.allyIsBlue
        teamName: root.allyTeamName
        hp: root.resources.allyBaseHp || 0
        maxHp: root.resources.allyBaseMax || 5000
        outpostHp: root.resources.allyOutpostHp || 0
        outpostMax: root.resources.allyOutpostMax || 1500
        outpostDestroyed: root.resources.allyOutpostDestroyed || false
        invincible: root.resources.allyBaseInvincible || false
        defenseBonus: root.resources.allyDefenseBonus || 0
    }

    BaseHealthWing {
        id: enemyWing
        anchors.left: centerInfo.right
        anchors.leftMargin: Math.round(10 * root.topHudScaleX)
        y: Math.round(4 * root.topHudScaleY)
        width: root.topHudWingWidth
        height: root.topHudWingHeight
        z: 2
        enemy: true
        isBlueTeam: root.enemyIsBlue
        teamName: root.enemyTeamName
        hp: root.resources.enemyBaseHp || 0
        maxHp: root.resources.enemyBaseMax || 5000
        outpostHp: root.resources.enemyOutpostHp || 0
        outpostMax: root.resources.enemyOutpostMax || 1500
        outpostDestroyed: root.resources.enemyOutpostDestroyed || false
        invincible: root.resources.enemyBaseInvincible || false
        defenseBonus: root.resources.enemyDefenseBonus || 0
    }

    TopLogisticsStrip {
        anchors.horizontalCenter: parent.horizontalCenter
        y: allyWing.y + allyWing.height + Math.round(8 * root.topHudScaleY)
        width: Math.min(parent.width - Math.round(120 * root.topHudScaleX),
                        root.topHudLogisticsWidth)
        height: root.topHudLogisticsHeight
        z: 2
        model: root.topStatus
        allyIsBlue: root.allyIsBlue
        enemyIsBlue: root.enemyIsBlue
    }

    Rectangle {
        id: loginButton

        width: Math.round(80 * root.topHudScale)
        height: Math.round(30 * root.topHudScale)
        x: Math.max(24, root.width - width - Math.round(66 * root.topHudScaleX))
        y: Math.round(108 * root.topHudScaleY)
        z: 10
        radius: 6
        color: loginMouseArea.pressed ? "#1e7aa5"
                                      : (loginMouseArea.containsMouse ? "#164f6d" : "#0d3248")
        border.color: "#4fd8ff"
        border.width: 1

        Text {
            anchors.centerIn: parent
            text: "登录"
            color: "#effbff"
            font.pixelSize: Math.round(11 * root.topHudScale)
            font.bold: true
        }

        MouseArea {
            id: loginMouseArea

            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                console.info("[tactical-login] qml login clicked")
                if (root.mainWindowContext) {
                    root.mainWindowContext.toggleSettingsPanel()
                } else {
                    console.warn("[tactical-login] mainWindow context property is unavailable")
                }
            }
        }
    }

    Loader {
        id: tacticalLargeMapLoader

        objectName: "tacticalLargeMapLoader"
        anchors.fill: parent
        z: 1000
        active: root.largeMapMode
        visible: active
        source: "TacticalLargeMapOverlay.qml"
        onLoaded: item.pageRoot = root
    }
}
