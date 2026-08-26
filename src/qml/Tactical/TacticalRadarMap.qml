pragma ComponentBehavior: Bound

import QtQuick 2.15

// 战术雷达地图 — 三层：官方小地图底图 + 机器人位置/方向 + 战术叠加
Rectangle {
    id: root
    color: "#111A2A"
    border.color: "#1E3355"
    border.width: root.visualScale
    radius: 4 * root.visualScale

    property var model: ({})
    property var allyStatusModel: []
    property var enemyStatusModel: []
    property bool allyIsBlue: false
    property bool enemyIsBlue: true
    property string backgroundSource: ""
    property bool viewMirrored: false
    property var pageRoot: null
    // 大地图按目标逻辑分辨率缩放；普通战术页保持 1.0，大屏上的文字、标记和血条随地图放大。
    property real visualScale: 1.0
    // 4K 大地图的 Canvas 像素量较大，使用 Qt 私有 Canvas 线程，避免 10 Hz 雷达刷新占满 GUI 线程。
    property bool threadedCanvasRendering: false
    property int radarAgeMs: root.model.radarAgeMs || 0
    readonly property color allyColor: allyIsBlue ? "#4488FF" : "#FF4444"
    readonly property color allyLabelColor: allyIsBlue ? "#AACCFF" : "#FFCCCC"
    readonly property color allyStrokeColor: allyIsBlue ? "#6699FF" : "#FF6666"
    readonly property color enemyColor: enemyIsBlue ? "#4488FF" : "#FF4444"
    readonly property color enemyLabelColor: enemyIsBlue ? "#AACCFF" : "#FFCCCC"
    readonly property color enemyStrokeColor: enemyIsBlue ? "#6699FF" : "#FF6666"
    readonly property color healthBarFrameColor: "#ffd15c"
    readonly property color healthBarTrackColor: Qt.rgba(0.32, 0.24, 0.02, 0.92)
    readonly property color healthBarPanelColor: Qt.rgba(0.20, 0.14, 0.02, 0.90)

    onAllyIsBlueChanged: {
        fallbackGrid.requestPaint()
        mapCanvas.requestPaint()
    }

    onEnemyIsBlueChanged: {
        fallbackGrid.requestPaint()
        mapCanvas.requestPaint()
    }
    onViewMirroredChanged: {
        fallbackGrid.requestPaint()
        mapCanvas.requestPaint()
    }
    onVisualScaleChanged: {
        fallbackGrid.requestPaint()
        mapCanvas.requestPaint()
    }
    // gameData 由 MainWindow 注入，地图内统一从这个入口读取当前机器人编号。
    // qmllint disable unqualified
    readonly property var gameDataContext: typeof gameData !== "undefined" ? gameData : null
    // qmllint enable unqualified
    readonly property int currentRobotNumber: root.gameDataContext
        ? Number(root.gameDataContext.currentRobotId || 0) % 100 : 0
    readonly property color selfRobotColor: "#18FF62"
    readonly property color selfRobotStrokeColor: "#9BFFBE"
    readonly property color selfRobotLabelColor: "#EFFFF4"
    function robotNumberText(robot) {
        var number = Number(robot && robot.number !== undefined ? robot.number : 0)
        return number > 0 ? number.toString() : ""
    }

    function clampNormalized(value, fallbackValue) {
        var number = Number(value)
        if (isNaN(number))
            number = fallbackValue !== undefined ? Number(fallbackValue) : 0
        return Math.max(0, Math.min(1, number))
    }

    function normalizeAngle(angle) {
        var value = Number(angle)
        if (isNaN(value))
            return 0
        value = value % 360
        if (value < 0)
            value += 360
        return value
    }

    function mapNormX(x) {
        var normalized = root.clampNormalized(x, 0)
        return root.viewMirrored ? 1.0 - normalized : normalized
    }

    function mapNormY(y) {
        var normalized = root.clampNormalized(y, 0)
        return root.viewMirrored ? 1.0 - normalized : normalized
    }

    function mapAngle(angle) {
        var normalized = root.normalizeAngle(angle)
        return root.viewMirrored ? root.normalizeAngle(normalized + 180) : normalized
    }

    function transformRobot(robot) {
        if (!robot)
            return robot
        var transformed = {}
        for (var key in robot)
            transformed[key] = robot[key]
        transformed.x = root.mapNormX(robot.x)
        transformed.y = root.mapNormY(robot.y)
        if (robot.angle !== undefined)
            transformed.angle = root.mapAngle(robot.angle)
        return transformed
    }

    function transformRobots(robots) {
        var transformed = []
        for (var i = 0; i < robots.length; ++i)
            transformed.push(root.transformRobot(robots[i]))
        return transformed
    }

    function transformRoute(route) {
        if (!route)
            return route
        var transformed = {}
        for (var key in route)
            transformed[key] = route[key]
        transformed.fromX = root.mapNormX(route.fromX)
        transformed.toX = root.mapNormX(route.toX)
        transformed.fromY = root.mapNormY(route.fromY)
        transformed.toY = root.mapNormY(route.toY)
        return transformed
    }

    function transformRoutes(routes) {
        var transformed = []
        for (var i = 0; i < routes.length; ++i)
            transformed.push(root.transformRoute(routes[i]))
        return transformed
    }

    function transformDangerZone(zone) {
        if (!zone)
            return zone
        var transformed = {}
        for (var key in zone)
            transformed[key] = zone[key]
        transformed.centerX = root.mapNormX(zone.centerX)
        transformed.centerY = root.mapNormY(zone.centerY)
        return transformed
    }

    function transformDangerZones(zones) {
        var transformed = []
        for (var i = 0; i < zones.length; ++i)
            transformed.push(root.transformDangerZone(zones[i]))
        return transformed
    }

    function transformBuffZone(zone) {
        if (!zone)
            return zone
        var transformed = {}
        for (var key in zone)
            transformed[key] = zone[key]
        transformed.x = root.mapNormX(zone.x)
        transformed.y = root.mapNormY(zone.y)
        return transformed
    }

    function transformBuffZones(zones) {
        var transformed = []
        for (var i = 0; i < zones.length; ++i)
            transformed.push(root.transformBuffZone(zones[i]))
        return transformed
    }

    function isSelfRobot(robot) {
        if (!robot)
            return false
        return Number(robot.number || 0) === root.currentRobotNumber
    }

    function normalizedRobotScreenY(robot) {
        return 1 - Number(robot && robot.y !== undefined ? robot.y : 0)
    }

    function isRobotHit(robot, normalizedX, normalizedY) {
        if (!robot)
            return false
        var dx = normalizedX - Number(robot.x || 0)
        var dy = normalizedY - root.normalizedRobotScreenY(robot)
        return dx * dx + dy * dy < 0.0025
    }

    function mergedRobotModel(positionRobots, statusRobots) {
        var statusById = {}
        for (var i = 0; i < statusRobots.length; ++i) {
            var statusRobot = statusRobots[i]
            statusById[String(statusRobot.id || "")] = statusRobot
        }

        var merged = []
        for (var j = 0; j < positionRobots.length; ++j) {
            var positionedRobot = positionRobots[j]
            var robotId = String(positionedRobot.id || "")
            var mergedRobot = {}
            for (var key in positionedRobot) {
                mergedRobot[key] = positionedRobot[key]
            }
            var matchedStatus = statusById[robotId]
            if (matchedStatus) {
                for (var statusKey in matchedStatus) {
                    if (mergedRobot[statusKey] === undefined) {
                        mergedRobot[statusKey] = matchedStatus[statusKey]
                    }
                }
            }
            merged.push(mergedRobot)
        }
        return merged
    }

    // 标题
    Text {
        anchors.left: parent.left; anchors.leftMargin: 6 * root.visualScale
        anchors.top: parent.top; anchors.topMargin: 4 * root.visualScale
        text: "战术地图"; color: "#8899AA"; font.pixelSize: 10 * root.visualScale; font.bold: true
    }

    // 雷达数据更新时间
    Rectangle {
        anchors.right: parent.right; anchors.rightMargin: 6 * root.visualScale
        anchors.top: parent.top; anchors.topMargin: 4 * root.visualScale
        width: 80 * root.visualScale
        height: 16 * root.visualScale
        radius: 2 * root.visualScale
        color: "#111622"
        Text {
            anchors.centerIn: parent
            text: "雷达 " + root.radarAgeMs + "ms"
            color: root.radarAgeMs > 0 ? "#44AA66" : "#667788"
            font.pixelSize: 9 * root.visualScale
        }
    }

    // 地图区域
    Rectangle {
        id: mapArea
        anchors.top: parent.top; anchors.topMargin: 24 * root.visualScale
        anchors.left: parent.left; anchors.leftMargin: 4 * root.visualScale
        anchors.right: parent.right; anchors.rightMargin: 4 * root.visualScale
        anchors.bottom: parent.bottom; anchors.bottomMargin: 4 * root.visualScale
        color: "#0C1420"
        border.color: "#1E3355"
        border.width: root.visualScale
        clip: true

        // === 第 1 层：官方小地图背景 ===
        // 地图图片来源由外部设置 (model.mapImageSource 或 C++ 注入)
        // 未提供时自动降级为 Canvas 绘制的简化场地标识
        Image {
            id: mapBgImg
            anchors.fill: parent
            source: root.backgroundSource || root.model.mapImageSource || ""
            fillMode: Image.PreserveAspectFit
            cache: false
        }

        // 官方地图未加载时绘制简化场地标识。
        Canvas {
            id: fallbackGrid
            anchors.fill: parent
            renderStrategy: root.threadedCanvasRendering
                ? Canvas.Threaded : Canvas.Immediate
            visible: mapBgImg.source === "" || mapBgImg.status !== Image.Ready
            onPaint: {
                var ctx = getContext("2d")
                var w = width; var h = height
                var s = root.visualScale
                ctx.clearRect(0, 0, w, h)
                // 网格
                ctx.strokeStyle = "#142436"; ctx.lineWidth = s
                for (var i = 0; i < 6; i++) {
                    ctx.beginPath(); ctx.moveTo(i*w/5, 0); ctx.lineTo(i*w/5, h); ctx.stroke()
                }
                for (var j = 0; j < 4; j++) {
                    ctx.beginPath(); ctx.moveTo(0, j*h/3); ctx.lineTo(w, j*h/3); ctx.stroke()
                }
                // 己方基地区
                ctx.fillStyle = root.allyIsBlue ? "rgba(68,136,255,0.12)" : "rgba(255,68,68,0.12)"
                ctx.fillRect(0, h*0.25, w*0.1, h*0.5)
                // 敌方基地区
                ctx.fillStyle = root.enemyIsBlue ? "rgba(68,136,255,0.12)" : "rgba(255,68,68,0.12)"
                ctx.fillRect(w*0.9, h*0.25, w*0.1, h*0.5)
                // 缓冲区
                ctx.strokeStyle = "rgba(0,255,136,0.12)"
                ctx.lineWidth = s
                ctx.setLineDash([2 * s, 6 * s])
                ctx.beginPath(); ctx.arc(w*0.45, h*0.35, w*0.05, 0, Math.PI*2); ctx.stroke()
                ctx.setLineDash([])
            }
        }

        // === 第 2、3 层：战术叠加画布 ===
        Canvas {
            id: mapCanvas
            anchors.fill: parent
            renderStrategy: root.threadedCanvasRendering
                ? Canvas.Threaded : Canvas.Immediate

            property var allyRobots: root.transformRobots(
                root.mergedRobotModel(root.model.allyRobots || [], root.allyStatusModel || [])
            )
            property var enemyRobots: root.transformRobots(
                root.mergedRobotModel(root.model.enemyRobots || [], root.enemyStatusModel || [])
            )
            property var routes: root.transformRoutes(root.model.routes || [])
            property var dangerZones: root.transformDangerZones(root.model.dangerZones || [])
            property var buffZones: root.transformBuffZones(root.model.buffZones || [])

            onPaint: {
                var ctx = getContext("2d")
                var w = width; var h = height
                var s = root.visualScale
                ctx.clearRect(0, 0, w, h)

                // --- 危险区 (红色半透明圆) ---
                for (var d = 0; d < dangerZones.length; d++) {
                    var dz = dangerZones[d]
                    ctx.fillStyle = "rgba(255,50,50,0.15)"
                    ctx.strokeStyle = "rgba(255,50,50,0.4)"
                    ctx.lineWidth = s
                    ctx.beginPath()
                    ctx.arc(dz.centerX * w, dz.centerY * h, (dz.radius || 0.05) * w, 0, Math.PI * 2)
                    ctx.fill()
                    ctx.stroke()
                    // 危险标签
                    ctx.fillStyle = "rgba(255,80,80,0.6)"
                    ctx.font = (8 * s) + "px sans-serif"
                    ctx.fillText("⚠", dz.centerX * w - 6 * s, dz.centerY * h - (dz.radius || 0.05) * w - 2 * s)
                }

                // --- 推荐路线 (金色虚线) ---
                ctx.strokeStyle = "rgba(255,170,0,0.7)"
                ctx.lineWidth = 2 * s
                ctx.setLineDash([4 * s, 4 * s])
                for (var r = 0; r < routes.length; r++) {
                    var rt = routes[r]
                    ctx.beginPath()
                    ctx.moveTo(rt.fromX * w, rt.fromY * h)
                    ctx.lineTo(rt.toX * w, rt.toY * h)
                    ctx.stroke()
                    // 箭头
                    var midX = (rt.fromX + rt.toX) / 2 * w
                    var midY = (rt.fromY + rt.toY) / 2 * h
                    ctx.fillStyle = "rgba(255,170,0,0.6)"
                    ctx.font = (9 * s) + "px sans-serif"
                    ctx.fillText("→", midX - 4 * s, midY + 4 * s)
                }
                ctx.setLineDash([])

                // --- 增益点 (绿色) ---
                for (var b = 0; b < buffZones.length; b++) {
                    var bz = buffZones[b]
                    // 外圈
                    ctx.strokeStyle = "rgba(0,255,136,0.5)"
                    ctx.lineWidth = s
                    ctx.beginPath()
                    ctx.arc(bz.x * w, bz.y * h, 7 * s, 0, Math.PI * 2)
                    ctx.stroke()
                    // 内点
                    ctx.fillStyle = "#00FF88"
                    ctx.beginPath()
                    ctx.arc(bz.x * w, bz.y * h, 4 * s, 0, Math.PI * 2)
                    ctx.fill()
                    // 标签
                    ctx.fillStyle = "rgba(0,255,136,0.7)"
                    ctx.font = "bold " + (7 * s) + "px sans-serif"
                    ctx.fillText("BUFF", bz.x * w - 12 * s, bz.y * h - 10 * s)
                }

                // --- 敌方机器人 (改为根据team阵营) ---
                for (var e = 0; e < enemyRobots.length; e++) {
                    var er = enemyRobots[e]
                    var ex = er.x * w
                    var ey = root.normalizedRobotScreenY(er) * h


                    ctx.strokeStyle = root.enemyColor
                    ctx.lineWidth = s
                    ctx.beginPath()
                    ctx.arc(ex, ey, 8 * s, 0, Math.PI * 2)
                    ctx.stroke()


                    // 机器人主体
                    ctx.fillStyle = root.enemyColor
                    ctx.beginPath()
                    ctx.arc(ex, ey, 12 * s, 0, Math.PI * 2)
                    ctx.fill()

                    // 方向三角
                    if (er.angle !== undefined) {
                        var eAng = (er.angle - 90) * Math.PI / 180
                        drawDirectionTriangle(ctx, ex, ey, 15 * s, 9 * s, 5 * s, eAng,
                            root.enemyStrokeColor)
                    }

                    // 标签
                    ctx.fillStyle = root.enemyLabelColor
                    ctx.font = (22 * s) + "px sans-serif"
                    ctx.textAlign = "center"
                    ctx.textBaseline = "middle"
                    ctx.fillText(root.robotNumberText(er), ex , ey )
                }

                // --- 我方机器人 ---
                 for (var a = 0; a < allyRobots.length; a++) {
                    var ar = allyRobots[a]
                    var ax = ar.x * w
                    var ay = root.normalizedRobotScreenY(ar) * h
                    var isSelf = root.isSelfRobot(ar)

                    // 外圈
                    ctx.strokeStyle = isSelf
                        ? Qt.rgba(root.selfRobotColor.r, root.selfRobotColor.g, root.selfRobotColor.b, 0.75)
                        : Qt.rgba(root.allyColor.r, root.allyColor.g, root.allyColor.b, 0.5)
                    ctx.lineWidth = s
                    ctx.beginPath()
                    ctx.arc(ax, ay, 7 * s, 0, Math.PI * 2)
                    ctx.stroke()

                    // 机器人主体
                    ctx.fillStyle = isSelf ? root.selfRobotColor : root.allyColor
                    ctx.beginPath()
                    ctx.arc(ax, ay, 12 * s, 0, Math.PI * 2)
                    ctx.fill()

                    // 方向三角
                    if (ar.angle !== undefined) {
                        var aAng = (ar.angle - 90) * Math.PI / 180
                        drawDirectionTriangle(ctx, ax, ay, 15 * s, 9 * s, 5 * s, aAng,
                            isSelf ? root.selfRobotStrokeColor : root.allyStrokeColor)
                    }

                    // 标签
                    ctx.fillStyle = isSelf ? root.selfRobotLabelColor : root.allyLabelColor
                    ctx.font = "bold " + (22 * s) + "px sans-serif"
                    ctx.textAlign = "center"
                    ctx.textBaseline = "middle"
                    ctx.fillText(root.robotNumberText(ar), ax , ay )
                }
            }

            // 绘制机器人朝向三角。
            function drawDirectionTriangle(ctx, cx, cy, offsetDist, tipDist, baseDist, angleRad, color) {
                var markerCx = cx + Math.cos(angleRad) * offsetDist
                var markerCy = cy + Math.sin(angleRad) * offsetDist
                var tipX = markerCx + Math.cos(angleRad) * tipDist
                var tipY = markerCy + Math.sin(angleRad) * tipDist
                var leftX = markerCx + Math.cos(angleRad + 4.5) * baseDist
                var leftY = markerCy + Math.sin(angleRad + 4.5) * baseDist
                var rightX = markerCx + Math.cos(angleRad - 4.5) * baseDist
                var rightY = markerCy + Math.sin(angleRad - 4.5) * baseDist
                ctx.fillStyle = color
                ctx.beginPath()
                ctx.moveTo(tipX, tipY)
                ctx.lineTo(leftX, leftY)
                ctx.lineTo(rightX, rightY)
                ctx.closePath()
                ctx.fill()
            }

            // 数据变化触发重绘
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onAllyRobotsChanged: requestPaint()
            onEnemyRobotsChanged: requestPaint()
            onRoutesChanged: requestPaint()
            onDangerZonesChanged: requestPaint()
            onBuffZonesChanged: requestPaint()
        }

        Repeater {
            model: mapCanvas.enemyRobots

            Item {
                id: enemyHealthBar
                required property var modelData
                property var robot: enemyHealthBar.modelData
                readonly property bool online: Boolean(enemyHealthBar.robot.online)
                readonly property int hp: Number(enemyHealthBar.robot.hp || 0)
                readonly property int maxHp: Math.max(1, Number(enemyHealthBar.robot.maxHp || 0))
                readonly property real hpRatio: Math.max(0, Math.min(1, enemyHealthBar.hp / enemyHealthBar.maxHp))
                readonly property bool criticalLowHealth: enemyHealthBar.online && enemyHealthBar.hp > 0 && enemyHealthBar.hpRatio < 0.2
                readonly property color teamColor: root.enemyIsBlue ? "#178cff" : "#ff2d3a"
                readonly property color fillColor: enemyHealthBar.hpRatio > 0.55 ? enemyHealthBar.teamColor
                    : (enemyHealthBar.hpRatio > 0.25 ? "#ffb22e" : "#ff4e4e")

                readonly property real markerX: Number(enemyHealthBar.modelData.x || 0) * mapArea.width
                readonly property real markerY: root.normalizedRobotScreenY(enemyHealthBar.modelData) * mapArea.height

                visible: enemyHealthBar.modelData !== undefined
                    && enemyHealthBar.modelData.online !== undefined
                    && enemyHealthBar.markerX >= 0 && enemyHealthBar.markerX <= mapArea.width
                    && enemyHealthBar.markerY >= 0 && enemyHealthBar.markerY <= mapArea.height
                x: Math.round(Math.max(0, Math.min(mapArea.width - enemyHealthBar.width, enemyHealthBar.markerX - enemyHealthBar.width / 2)))
                y: Math.round(Math.max(0, Math.min(mapArea.height - enemyHealthBar.height, enemyHealthBar.markerY - 31 * root.visualScale)))
                z: 2
                width: 42 * root.visualScale
                height: 10 * root.visualScale

                Rectangle {
                    anchors.fill: parent
                    radius: 3 * root.visualScale
                    color: root.healthBarPanelColor
                    border.width: root.visualScale
                    border.color: root.healthBarFrameColor
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: root.visualScale
                    radius: 2 * root.visualScale
                    color: root.healthBarTrackColor

                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: Math.max(enemyHealthBar.hpRatio > 0 ? 2 * root.visualScale : 0, parent.width * enemyHealthBar.hpRatio)
                        radius: 2 * root.visualScale
                        color: enemyHealthBar.fillColor
                        visible: enemyHealthBar.hpRatio > 0
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: 3 * root.visualScale
                    visible: enemyHealthBar.criticalLowHealth
                    color: "#d80d18"
                    opacity: enemyHealthBar.criticalLowHealth ? 0.16 : 0

                    SequentialAnimation on opacity {
                        running: enemyHealthBar.criticalLowHealth
                        loops: Animation.Infinite
                        NumberAnimation { from: 0.10; to: 0.35; duration: 360; easing.type: Easing.InOutQuad }
                        NumberAnimation { from: 0.35; to: 0.10; duration: 520; easing.type: Easing.InOutQuad }
                    }
                }
            }
        }

        Repeater {
            model: mapCanvas.allyRobots

            Item {
                id: allyHealthBar
                required property var modelData
                property var robot: allyHealthBar.modelData
                readonly property bool online: Boolean(allyHealthBar.robot.online)
                readonly property int hp: Number(allyHealthBar.robot.hp || 0)
                readonly property int maxHp: Math.max(1, Number(allyHealthBar.robot.maxHp || 0))
                readonly property real hpRatio: Math.max(0, Math.min(1, allyHealthBar.hp / allyHealthBar.maxHp))
                readonly property bool criticalLowHealth: allyHealthBar.online && allyHealthBar.hp > 0 && allyHealthBar.hpRatio < 0.2
                readonly property bool selfRobot: root.isSelfRobot(allyHealthBar.modelData)
                readonly property color teamColor: allyHealthBar.selfRobot ? "#18FF62"
                    : (root.allyIsBlue ? "#178cff" : "#ff2d3a")
                readonly property color fillColor: allyHealthBar.hpRatio > 0.55 ? allyHealthBar.teamColor
                    : (allyHealthBar.hpRatio > 0.25 ? "#ffb22e" : "#ff4e4e")

                readonly property real markerX: Number(allyHealthBar.modelData.x || 0) * mapArea.width
                readonly property real markerY: root.normalizedRobotScreenY(allyHealthBar.modelData) * mapArea.height

                visible: allyHealthBar.modelData !== undefined
                    && allyHealthBar.modelData.online !== undefined
                    && allyHealthBar.markerX >= 0 && allyHealthBar.markerX <= mapArea.width
                    && allyHealthBar.markerY >= 0 && allyHealthBar.markerY <= mapArea.height
                x: Math.round(Math.max(0, Math.min(mapArea.width - allyHealthBar.width, allyHealthBar.markerX - allyHealthBar.width / 2)))
                y: Math.round(Math.max(0, Math.min(mapArea.height - allyHealthBar.height, allyHealthBar.markerY + 17 * root.visualScale)))
                z: 2
                width: 42 * root.visualScale
                height: 10 * root.visualScale

                Rectangle {
                    anchors.fill: parent
                    radius: 3 * root.visualScale
                    color: root.healthBarPanelColor
                    border.width: root.visualScale
                    border.color: root.healthBarFrameColor
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: root.visualScale
                    radius: 2 * root.visualScale
                    color: root.healthBarTrackColor

                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: Math.max(allyHealthBar.hpRatio > 0 ? 2 * root.visualScale : 0, parent.width * allyHealthBar.hpRatio)
                        radius: 2 * root.visualScale
                        color: allyHealthBar.fillColor
                        visible: allyHealthBar.hpRatio > 0
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: 3 * root.visualScale
                    visible: allyHealthBar.criticalLowHealth
                    color: "#d80d18"
                    opacity: allyHealthBar.criticalLowHealth ? 0.16 : 0

                    SequentialAnimation on opacity {
                        running: allyHealthBar.criticalLowHealth
                        loops: Animation.Infinite
                        NumberAnimation { from: 0.10; to: 0.35; duration: 360; easing.type: Easing.InOutQuad }
                        NumberAnimation { from: 0.35; to: 0.10; duration: 520; easing.type: Easing.InOutQuad }
                    }
                }
            }
        }

        // 地图点击 — 命中检测机器人
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: mouse => {
                if (!root.pageRoot) return
                var mx = mouse.x / width
                var my = mouse.y / height

                // 遍历敌方机器人（优先命中）
                var enemies = mapCanvas.enemyRobots
                for (var ei = 0; ei < enemies.length; ei++) {
                    var er = enemies[ei]
                    if (root.isRobotHit(er, mx, my)) { // 半径 ~0.05 归一化
                        root.pageRoot.selectRobot(er.id, true)
                        return
                    }
                }

                // 遍历我方机器人
                var allies = mapCanvas.allyRobots
                for (var allyIndex = 0; allyIndex < allies.length; allyIndex++) {
                    var ar = allies[allyIndex]
                    if (root.isRobotHit(ar, mx, my)) {
                        root.pageRoot.selectRobot(ar.id, false)
                        return
                    }
                }

                // 点击空白处取消选中
                root.pageRoot.clearSelection()
            }
        }
    }

    // 无数据提示（仅在从未收到过雷达数据时显示，不作为蒙层）
    Text {
        anchors.centerIn: mapArea
        text: "等待雷达数据..."
        color: "#445566"
        font.pixelSize: 12 * root.visualScale
        visible: root.radarAgeMs === 0 || (root.model.radarAgeMs === undefined && (!root.model.allyRobots || root.model.allyRobots.length === 0))
        z: 0
    }
}
