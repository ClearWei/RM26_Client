pragma ComponentBehavior: Bound

import QtQuick 2.15

HudPanel {
    id: root

    property var metrics: []
    property var decision: ({})

    title: "战场数据分析"
    accent: "#178cff"
    eyebrow: ""

    readonly property real panelPadding: 14
    readonly property real headerOffset: 42
    readonly property real footerHeight: 34
    readonly property real footerGap: 8
    readonly property real cardSpacing: width >= 720 ? 10 : 8
    readonly property int metricColumns: width >= 640 ? 4 : 2
    readonly property real contentWidth: Math.max(0, width - panelPadding * 2)
    readonly property real contentTop: headerOffset
    readonly property real metricsHeight: Math.max(
        120,
        height - contentTop - footerHeight - footerGap - panelPadding
    )

    function metricAt(index) {
        var list = root.metrics || []
        if (index < list.length) {
            return list[index]
        }
        var fallback = [
            { title: "我方总经济", value: "--", status: "warn", trend: [0.18, 0.24, 0.28, 0.34, 0.39] },
            { title: "我方总伤害", value: "--", status: "good", trend: [0.12, 0.22, 0.30, 0.36, 0.44] },
            { title: "敌方总伤害", value: "--", status: "warn", trend: [0.10, 0.18, 0.21, 0.24, 0.29] },
            { title: "总血量差", value: "--", status: "good", trend: [0.20, 0.25, 0.29, 0.35, 0.42] }
        ]
        return fallback[index]
    }

    function metricColor(metric) {
        if (!metric) {
            return "#178cff"
        }
        if (metric.status === "bad") {
            return "#ff313b"
        }
        if (metric.status === "warn") {
            return "#ffad2e"
        }
        return "#178cff"
    }

    function metricVerdict(metric) {
        if (!metric || metric.value === "--") {
            return "等待数据"
        }
        if (metric.status === "bad") {
            return "敌方优势"
        }
        if (metric.status === "warn") {
            return "需要关注"
        }
        return "我方优势"
    }

    function metricValueText(metric) {
        if (!metric || metric.value === undefined || metric.value === null || metric.value === "") {
            return "--"
        }
        var value = String(metric.value)
        if (value.length > 8 && value.toUpperCase().indexOf("UNKNOWN") >= 0) {
            return "UNKNOWN"
        }
        return value
    }

    Grid {
        id: metricGrid
        x: root.panelPadding
        y: root.contentTop
        width: root.contentWidth
        height: root.metricsHeight
        columns: root.metricColumns
        rows: Math.ceil(4 / columns)
        spacing: root.cardSpacing

        Repeater {
            model: 4
            Rectangle {
                id: metricCard
                required property int index
                property var metric: root.metricAt(index)
                property color cardAccent: root.metricColor(metricCard.metric)
                property string valueText: root.metricValueText(metricCard.metric)

                readonly property real gridWidth: metricGrid.width
                readonly property real gridHeight: metricGrid.height
                readonly property int gridColumns: Math.max(1, metricGrid.columns)
                readonly property int gridRows: Math.max(1, metricGrid.rows)
                readonly property real innerSpacingX: Math.max(0, (gridColumns - 1) * metricGrid.spacing)
                readonly property real innerSpacingY: Math.max(0, (gridRows - 1) * metricGrid.spacing)

                width: Math.max(128, (gridWidth - innerSpacingX) / gridColumns)
                height: Math.max(112, (gridHeight - innerSpacingY) / gridRows)
                radius: 3
                color: metricCard.metric.status === "bad" ? Qt.rgba(0.26, 0.02, 0.04, 0.42)
                     : metricCard.metric.status === "warn" ? Qt.rgba(0.24, 0.15, 0.03, 0.38)
                     : Qt.rgba(0.018, 0.08, 0.13, 0.58)
                border.color: Qt.rgba(cardAccent.r, cardAccent.g, cardAccent.b, 0.24)

                Text {
                    x: 10
                    y: 14
                    width: parent.width - 20
                    text: metricCard.metric.title || "--"
                    color: "#dce9f3"
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }

                Text {
                    x: 10
                    y: 42
                    width: parent.width - 20
                    text: metricCard.valueText
                    color: metricCard.cardAccent
                    font.pixelSize: metricCard.valueText.length > 6 ? 23 : 28
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }

                Text {
                    x: 10
                    y: 79
                    width: parent.width - 20
                    text: root.metricVerdict(metricCard.metric)
                    color: metricCard.metric.status === "bad" ? "#ff565f"
                        : metricCard.metric.status === "warn" ? "#ffcf70" : "#34ff78"
                    font.pixelSize: 12
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }

                //对比说明文字
                Text {
                    x: 10
                    y: 108
                    width: parent.width - 20
                    text: metricCard.metric.compareText || metricCard.metric.subText || ""
                    color: "#c6d4df"
                    visible: parent.height > 142
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }

                Canvas {
                    id: chartCanvas
                    x: 10
                    y: Math.max(104, parent.height - 36)
                    width: parent.width - 20
                    height: 24
                    opacity: 0.94
                    onPaint: {
                        var ctx = getContext("2d")
                        var points = metricCard.metric.trend || [0.18, 0.26, 0.30, 0.38, 0.45]
                        ctx.clearRect(0, 0, width, height)
                        ctx.strokeStyle = "rgba(255, 49, 59, 0.85)"
                        ctx.lineWidth = 1
                        ctx.beginPath()
                        for (var i = 0; i < points.length; ++i) {
                            var x = i * width / Math.max(1, points.length - 1)
                            var y = height - Math.max(2, Math.min(height - 2, points[i] * height))
                            if (i === 0) ctx.moveTo(x, y + 8)
                            else ctx.lineTo(x, y + 8)
                        }
                        ctx.stroke()
                        ctx.strokeStyle = "rgba(" + Math.round(metricCard.cardAccent.r * 255) + ", "
                            + Math.round(metricCard.cardAccent.g * 255) + ", "
                            + Math.round(metricCard.cardAccent.b * 255) + ", 0.95)"
                        ctx.lineWidth = 1.5
                        ctx.beginPath()
                        for (var j = 0; j < points.length; ++j) {
                            var px = j * width / Math.max(1, points.length - 1)
                            var py = height - Math.max(2, Math.min(height - 2, (points[j] + 0.16) * height))
                            if (j === 0) ctx.moveTo(px, py)
                            else ctx.lineTo(px, py)
                        }
                        ctx.stroke()
                    }
                }
            }
        }
    }

    Text {
        x: root.panelPadding
        y: metricGrid.y + metricGrid.height + root.footerGap
        width: root.contentWidth
        text: root.decision.reasons && root.decision.reasons.length
            ? "分析结论：" + root.decision.reasons.join("，")
            : "分析结论：等待有效战术窗口，暂不下发真实指令。"
        color: "#c9d8e3"
        font.pixelSize: 12
        elide: Text.ElideRight
    }
}
