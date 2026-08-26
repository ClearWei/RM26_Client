import QtQuick 2.15

HudPanel {
    id: root

    property var status: ({})
    property bool centralMode: false
    property bool showRealVideo: false
    property bool showGrid: false
    property string liveFrameSource: ""
    property real liveFps: -1
    property bool gameActive: false  // 父级兼容字段；默认图显示只取决于是否有可见图传帧
    readonly property string frameSource: (
        root.liveFrameSource.length > 0
    ) ? root.liveFrameSource : ((
        root.status && root.status.frameSource !== undefined
    ) ? root.status.frameSource : "")
    readonly property bool effectiveConnected: root.liveFrameSource.length > 0
        || Boolean(root.status && root.status.connected)
    readonly property string fpsText: root.liveFps >= 0
        ? (root.liveFps > 0 ? root.liveFps.toFixed(1) : "--")
        : ((root.status && root.status.fps) ? String(root.status.fps) : "--")
    readonly property real latencyValue: Number(
        root.status && root.status.latencyMs ? root.status.latencyMs : 0
    )
    readonly property bool hasVisibleVideo: root.showRealVideo
        && root.effectiveConnected
        && root.frameSource.length > 0

    title: root.centralMode ? "工业相机主画面" : "工业相机预览"
    readonly property string defaultImage: root.centralMode
        ? "qrc:/images/tactical_camera_main_default.png"
        : "qrc:/images/tactical_camera_preview_default.png"
    accent: "#c7f2ff"
    eyebrow: root.hasVisibleVideo ? "LIVE" : "WAIT"
    color: root.centralMode ? "transparent" : Qt.rgba(0.02, 0.05, 0.08, 0.52)

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: root.centralMode ? 8 : 12
        anchors.rightMargin: root.centralMode ? 8 : 12
        anchors.topMargin: root.centralMode ? 32 : 36
        anchors.bottomMargin: root.centralMode ? 8 : 12
        radius: 3
        color: root.hasVisibleVideo
            ? "transparent"
            : (root.effectiveConnected ? Qt.rgba(0, 0, 0, 0.02)
                : (root.centralMode ? Qt.rgba(0, 0, 0, 0.08) : Qt.rgba(0, 0, 0, 0.24)))
        border.color: root.hasVisibleVideo ? Qt.rgba(0.314, 1.0, 0.569, 0.56) : Qt.rgba(1.0, 0.667, 0.251, 0.48)
        border.width: 1
        clip: true

        Image {
            anchors.fill: parent
            visible: root.hasVisibleVideo
            source: visible ? root.frameSource : ""
            cache: false
            asynchronous: false
            fillMode: Image.PreserveAspectFit
            horizontalAlignment: Image.AlignHCenter
            verticalAlignment: Image.AlignVCenter
            smooth: true
        }

        // 默认图片 — 无图传画面时始终作为占位，不随比赛阶段隐藏
        Image {
            anchors.fill: parent
            visible: !root.hasVisibleVideo
            source: root.defaultImage
            fillMode: Image.PreserveAspectCrop
            cache: false
            asynchronous: true
            smooth: true
        }

        // 无图传暗化蒙层 — 比赛中断帧时也保持默认图可见
        Rectangle {
            visible: !root.hasVisibleVideo
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.65)
        }

        Canvas {
            anchors.fill: parent
            visible: !root.hasVisibleVideo
            opacity: 0.18
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.strokeStyle = "rgba(199, 242, 255, 0.12)"
                ctx.lineWidth = 1
                for (var y = 4; y < height; y += 10) {
                    ctx.beginPath()
                    ctx.moveTo(0, y)
                    ctx.lineTo(width, y)
                    ctx.stroke()
                }
                ctx.strokeStyle = "rgba(255, 177, 59, 0.16)"
                ctx.beginPath()
                ctx.moveTo(width * 0.18, height * 0.70)
                ctx.lineTo(width * 0.82, height * 0.22)
                ctx.stroke()
            }
        }

        Canvas {
            anchors.fill: parent
            opacity: root.hasVisibleVideo ? 0.42 : 0.35
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.strokeStyle = root.hasVisibleVideo ? "rgba(92, 255, 179, 0.24)" : "rgba(255, 181, 75, 0.22)"
                ctx.lineWidth = 2
                var l = Math.min(34, width * 0.08)
                ctx.beginPath()
                ctx.moveTo(0, l)
                ctx.lineTo(0, 0)
                ctx.lineTo(l, 0)
                ctx.moveTo(width - l, 0)
                ctx.lineTo(width, 0)
                ctx.lineTo(width, l)
                ctx.moveTo(width, height - l)
                ctx.lineTo(width, height)
                ctx.lineTo(width - l, height)
                ctx.moveTo(l, height)
                ctx.lineTo(0, height)
                ctx.lineTo(0, height - l)
                ctx.stroke()
            }
        }

        // 可选的 6×6 校准参考网格。它属于显示层、默认关闭，不再烙入
        // H.264 解码后的原始像素。
        Canvas {
            anchors.fill: parent
            visible: root.showGrid && root.hasVisibleVideo
            opacity: 0.62
            onVisibleChanged: if (visible) requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.strokeStyle = "rgba(144, 238, 144, 0.78)"
                ctx.lineWidth = 1
                for (var i = 1; i < 6; ++i) {
                    var x = Math.round(width * i / 6) + 0.5
                    var y = Math.round(height * i / 6) + 0.5
                    ctx.beginPath()
                    ctx.moveTo(x, 0)
                    ctx.lineTo(x, height)
                    ctx.moveTo(0, y)
                    ctx.lineTo(width, y)
                    ctx.stroke()
                }
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.leftMargin: root.centralMode ? 136 : 118
            anchors.top: parent.top
            anchors.topMargin: 9
            width: root.hasVisibleVideo ? 44 : 56
            height: 20
            radius: 2
            color: root.hasVisibleVideo ? Qt.rgba(0.85, 0, 0.05, 0.62)
                : Qt.rgba(0.12, 0.12, 0.12, 0.58)
            border.color: root.hasVisibleVideo ? "#ff1f34" : "#6f7d87"
            visible: root.hasVisibleVideo

            Text {
                anchors.centerIn: parent
                text: root.hasVisibleVideo ? "LIVE" : "WAIT"
                color: root.hasVisibleVideo ? "#ffffff" : "#b9c7d0"
                font.pixelSize: 11
                font.bold: true
            }
        }

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 10
            text: (root.hasVisibleVideo ? "CUSTOM CAM ONLINE" : "WAIT CUSTOM CAM")
                + "  FPS " + root.fpsText
                + (root.latencyValue > 0 ? "  LAT " + root.latencyValue + "ms" : "")
            color: root.hasVisibleVideo ? "#72ff9b" : "#ffbd69"
            font.pixelSize: 11
            font.bold: true
        }
    }
}
