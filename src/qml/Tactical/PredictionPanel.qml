import QtQuick 2.15

// 底部趋势预测 — 未来 20 秒关键指标演变
Rectangle {
    id: root
    color: "#0D1522"
    border.color: "#1A2A4A"
    border.width: 1
    radius: 4

    property var model: ({})

    Text {
        anchors.left: parent.left; anchors.leftMargin: 6
        anchors.top: parent.top; anchors.topMargin: 2
        text: "趋势预测 20s"; color: "#8899AA"; font.pixelSize: 10; font.bold: true
    }

    // 趋势内容区域
    Item {
        anchors.top: parent.top; anchors.topMargin: 18
        anchors.left: parent.left; anchors.leftMargin: 6
        anchors.right: parent.right; anchors.rightMargin: 6
        anchors.bottom: parent.bottom; anchors.bottomMargin: 4

        Row {
            anchors.fill: parent
            spacing: 8

            // --- 威胁趋势 ---
            Rectangle {
                width: parent.width * 0.3; height: parent.height
                color: "transparent"
                Column {
                    anchors.fill: parent
                    spacing: 3
                    Text { text: "威胁"; color: "#667788"; font.pixelSize: 8 }
                    // 迷你趋势条
                    Row {
                        spacing: 2
                        Repeater {
                            model: root.model.threatHistory || [0.3, 0.45, 0.55, 0.62, 0.70]
                            delegate: Rectangle {
                                id: threatBar
                                required property var modelData
                                width: 8; height: threatBar.modelData * 24
                                color: threatBar.modelData > 0.6 ? "#FF4444" : "#FFAA00"
                                radius: 1
                                anchors.bottom: parent.bottom
                            }
                        }
                    }
                    Text {
                        text: (root.model.threatTrend || "↗") + " " + (root.model.threatDesc || "持续上升")
                        color: "#FF6644"; font.pixelSize: 8
                    }
                }
            }

            // --- 经济预测 ---
            Rectangle {
                width: parent.width * 0.3; height: parent.height
                color: "transparent"
                Column {
                    anchors.fill: parent
                    spacing: 3
                    Text { text: "经济"; color: "#667788"; font.pixelSize: 8 }
                    Row {
                        spacing: 2
                        Repeater {
                            model: root.model.economyHistory || [320, 350, 380, 410, 440]
                            delegate: Rectangle {
                                id: economyBar
                                required property var modelData
                                width: 8; height: Math.max(4, economyBar.modelData / 30)
                                color: "#44FF44"
                                radius: 1
                                anchors.bottom: parent.bottom
                            }
                        }
                    }
                    Text {
                        text: "差额: " + (root.model.economyProjection || "+120")
                        color: "#44FF44"; font.pixelSize: 8
                    }
                }
            }

            // --- 关键事件预测 ---
            Rectangle {
                width: parent.width * 0.38; height: parent.height
                color: "transparent"
                Column {
                    anchors.fill: parent
                    spacing: 3
                    Text { text: "预测事件"; color: "#667788"; font.pixelSize: 8 }
                    Repeater {
                        model: root.model.predictedEvents || [
                            { time: "T+8s", text: "前哨站将被摧毁", color: "#FF4444" },
                            { time: "T+15s", text: "大能量机关刷新", color: "#FFAA00" }
                        ]
                        delegate: Row {
                            id: predictedEventRow
                            required property var modelData
                            spacing: 4
                            Text { text: predictedEventRow.modelData.time; color: "#556677"; font.pixelSize: 8 }
                            Text { text: predictedEventRow.modelData.text; color: predictedEventRow.modelData.color; font.pixelSize: 8 }
                        }
                    }
                }
            }
        }
    }
}
