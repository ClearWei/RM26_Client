import QtQuick 2.15

Rectangle {
    id: root
    color: "#0D1522"
    border.color: "#1A2A4A"
    border.width: 1
    property var model: ({})

    Row {
        anchors.centerIn: parent
        spacing: 16

        // 己方基地
        Row { spacing: 4
            Text { text: "己基"; color: "#4488FF"; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
            Text { text: (root.model.allyOutpostDestroyed ? "1" : "2") + "塔"; color: "#6688AA"; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
            Text { text: root.model.allyBaseHp + "/" + root.model.allyBaseMax; color: "#00FF88"; font.pixelSize: 12; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
        }

        Rectangle { width: 1; height: 16; color: "#1A2A4A" }

        // 己方前哨站
        Row { spacing: 4
            Text { text: "己哨"; color: "#4488FF"; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
            Text {
                text: root.model.allyOutpostDestroyed ? "[已摧毁]" : (root.model.allyOutpostHp + "/" + root.model.allyOutpostMax)
                color: root.model.allyOutpostDestroyed ? "#666666" : "#44AAFF"
                font.pixelSize: 12; font.bold: true
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Rectangle { width: 1; height: 16; color: "#1A2A4A" }

        // 敌方基地
        Row { spacing: 4
            Text { text: "敌基"; color: "#FF4444"; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
            Text { text: (root.model.enemyOutpostDestroyed ? "1" : "2") + "塔"; color: "#6688AA"; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
            Text { text: root.model.enemyBaseHp + "/" + root.model.enemyBaseMax; color: "#FF6644"; font.pixelSize: 12; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
        }

        Rectangle { width: 1; height: 16; color: "#1A2A4A" }

        // 敌方前哨站
        Row { spacing: 4
            Text { text: "敌哨"; color: "#FF6644"; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
            Text {
                text: root.model.enemyOutpostDestroyed ? "[已摧毁]" : (root.model.enemyOutpostHp + "/" + root.model.enemyOutpostMax)
                color: root.model.enemyOutpostDestroyed ? "#666666" : "#FFAA44"
                font.pixelSize: 12; font.bold: true
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Rectangle { width: 1; height: 16; color: "#1A2A4A" }

        // 差值信息
        Row { spacing: 6
            Text {
                text: "经济差:" + (root.model.economyDiff > 0 ? "+" : "") + root.model.economyDiff
                color: root.model.economyDiff > 0 ? "#44FF44" : "#FF4444"; font.pixelSize: 10
            }
            Text {
                text: "伤害差:" + (root.model.damageDiff > 0 ? "+" : "") + root.model.damageDiff
                color: root.model.damageDiff > 0 ? "#44FF44" : "#FF4444"; font.pixelSize: 10
            }
            Text {
                text: "血量差:" + (root.model.hpDiff > 0 ? "+" : "") + root.model.hpDiff
                color: root.model.hpDiff > 0 ? "#44FF44" : "#FF4444"; font.pixelSize: 10
            }
        }
    }
}
