import QtQuick 2.15

Rectangle {
    id: root

    property string title: ""
    property color accent: "#36d6ff"
    property real panelOpacity: 0.72
    property string eyebrow: ""
    property bool showDiagonalTexture: true
    property real cutSizeOverride: -1
    readonly property real cutSize: root.cutSizeOverride >= 0
        ? root.cutSizeOverride
        : Math.min(18, Math.max(10, Math.min(width, height) * 0.075))

    radius: 0
    color: "transparent"
    clip: true

    Canvas {
        id: panelCanvas
        anchors.fill: parent
        antialiasing: true
        onPaint: {
            var ctx = getContext("2d")
            var cut = root.cutSize
            ctx.clearRect(0, 0, width, height)

            // 绘制带切角的背景
            ctx.beginPath()
            ctx.moveTo(cut, 0)
            ctx.lineTo(width, 0)
            ctx.lineTo(width, height - cut)
            ctx.lineTo(width - cut, height)
            ctx.lineTo(0, height)
            ctx.lineTo(0, cut)
            ctx.closePath()
            ctx.fillStyle = "rgba(3, 12, 20, " + root.panelOpacity + ")"
            ctx.fill()

            // 在面板轮廓内绘制横向纹理线
            ctx.save()
            ctx.clip()
            ctx.globalAlpha = 0.18
            ctx.strokeStyle = "rgba(125, 223, 255, 0.16)"
            ctx.lineWidth = 1
            for (var y = 10; y < height; y += 18) {
                ctx.beginPath()
                ctx.moveTo(0, y)
                ctx.lineTo(width, y)
                ctx.stroke()
            }
            if (root.showDiagonalTexture) {
                ctx.strokeStyle = "rgba(255, 255, 255, 0.05)"
                for (var x = -height; x < width; x += 84) {
                    ctx.beginPath()
                    ctx.moveTo(x, height)
                    ctx.lineTo(x + height, 0)
                    ctx.stroke()
                }
            }
            ctx.restore()

            // 一次绘制完整边框，避免线段重叠或重复描边
            ctx.beginPath()
            ctx.moveTo(cut, 0.5)
            ctx.lineTo(width - 0.5, 0.5)
            ctx.lineTo(width - 0.5, height - cut)
            ctx.lineTo(width - cut, height - 0.5)
            ctx.lineTo(0.5, height - 0.5)
            ctx.lineTo(0.5, cut)
            ctx.closePath()
            ctx.lineWidth = 1.2
            ctx.strokeStyle = "rgba(" + Math.round(root.accent.r * 255) + ", "
                + Math.round(root.accent.g * 255) + ", "
                + Math.round(root.accent.b * 255) + ", 0.92)"
            ctx.stroke()
        }
    }

    onAccentChanged: panelCanvas.requestPaint()
    onPanelOpacityChanged: panelCanvas.requestPaint()
    onShowDiagonalTextureChanged: panelCanvas.requestPaint()
    onCutSizeChanged: panelCanvas.requestPaint()

    Rectangle {
        anchors.left: parent.left
        anchors.leftMargin: root.cutSize
        anchors.top: parent.top
        width: parent.width - root.cutSize
        height: 2
        radius: 1
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.95) }
            GradientStop { position: 0.55; color: Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.18) }
            GradientStop { position: 1.0; color: Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.72) }
        }
    }

    Rectangle {
        visible: root.title.length > 0
        anchors.left: parent.left
        anchors.leftMargin: root.cutSize
        anchors.top: parent.top
        width: Math.min(parent.width * 0.48, 190)
        height: 27
        color: Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.10)
        border.color: Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.20)
    }

    Rectangle {
        visible: root.title.length > 0
        anchors.left: parent.left
        anchors.leftMargin: root.cutSize
        anchors.top: parent.top
        anchors.topMargin: 27
        width: Math.min(parent.width * 0.32, 110)
        height: 1
        color: root.accent
        opacity: 0.62
    }

    Text {
        visible: root.title.length > 0
        anchors.left: parent.left
        anchors.leftMargin: root.cutSize + 12
        anchors.top: parent.top
        anchors.topMargin: 6
        text: root.title
        color: "#edfaff"
        font.pixelSize: 15
        font.bold: true
    }

    Text {
        visible: root.eyebrow.length > 0
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.top: parent.top
        anchors.topMargin: 7
        text: root.eyebrow
        color: Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.90)
        font.pixelSize: 9
        font.bold: true
    }
}
