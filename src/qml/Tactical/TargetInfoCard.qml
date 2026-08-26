import QtQuick 2.15

// 目标信息卡片 — 中央图传区右上角的详细目标信息
Rectangle {
    id: root
    color: "#CC0A0D14"
    radius: 6
    border.color: "#33FFAA00"
    border.width: 1

    property var model: ({})

    width: 160; height: 80

    Column {
        anchors.centerIn: parent
        spacing: 3

        // 目标名称 + ID
        Text {
            text: root.model.targetLabel || "目标"
            color: "#FFAA00"; font.pixelSize: 13; font.bold: true
            anchors.horizontalCenter: parent.horizontalCenter
        }

        // HP 条
        Row {
            spacing: 6
            anchors.horizontalCenter: parent.horizontalCenter
            Rectangle {
                width: 80; height: 6; radius: 3; color: "#1A2A4A"
                anchors.verticalCenter: parent.verticalCenter
                Rectangle {
                    width: parent.width * Math.min(1, (root.model.targetHp || 0) / (root.model.targetMaxHp || 150))
                    height: 6; radius: 3
                    color: (root.model.targetHp / root.model.targetMaxHp) < 0.3 ? "#FF4444" : "#FFAA00"
                }
            }
            Text {
                text: (root.model.targetHp || 0) + "/" + (root.model.targetMaxHp || 150)
                color: "#FF6644"; font.pixelSize: 13; font.bold: true
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        // 距离 + 锁定品质
        Text {
            text: (root.model.distance !== undefined ? root.model.distance + "m" : "?m")
                  + " · 品质 " + ((root.model.lockQuality || 0) * 100).toFixed(0) + "%"
            color: "#6688AA"; font.pixelSize: 9
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }
}
