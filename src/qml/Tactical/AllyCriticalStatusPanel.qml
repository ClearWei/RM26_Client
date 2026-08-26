import QtQuick 2.15

Rectangle {
    id: root
    color: "#0D1522"
    border.color: "#1A2A4A"
    border.width: 1
    radius: 4

    property var model: ([])

    Text {
        anchors.left: parent.left; anchors.leftMargin: 8; anchors.top: parent.top; anchors.topMargin: 4
        text: "我方执行能力"; color: "#8899AA"; font.pixelSize: 10; font.bold: true
    }

    Row {
        anchors.top: parent.top; anchors.topMargin: 20; anchors.left: parent.left; anchors.leftMargin: 8; anchors.right: parent.right; anchors.rightMargin: 8
        spacing: 8

        Repeater {
            model: root.model
            delegate: Rectangle {
                id: statusCard
                required property var modelData

                width: (parent.width - 24) / 4
                height: 56; radius: 4; color: "#111622"; border.color: "#1A2A4A"; border.width: 1

                Column {
                    anchors.fill: parent; anchors.margins: 4; spacing: 2

                    Text { text: statusCard.modelData.label; color: "#8899AA"; font.pixelSize: 10; font.bold: true; anchors.horizontalCenter: parent.horizontalCenter }

                    Row { spacing: 4; anchors.horizontalCenter: parent.horizontalCenter
                        Column { spacing: 1
                            Text { text: "电"; color: "#667788"; font.pixelSize: 8 }
                            Rectangle { width: 28; height: 6; radius: 2; color: "#1A2A4A"
                                Rectangle { width: parent.width * (statusCard.modelData.capPct / 100); height: 6; radius: 2; color: statusCard.modelData.capPct > 50 ? "#44FF44" : (statusCard.modelData.capPct > 20 ? "#FFAA00" : "#FF4444") }
                            }
                        }
                        Column { spacing: 1
                            Text { text: "热"; color: "#667788"; font.pixelSize: 8 }
                            Rectangle { width: 28; height: 6; radius: 2; color: "#1A2A4A"
                                Rectangle { width: parent.width * (statusCard.modelData.heatPct / 100); height: 6; radius: 2; color: statusCard.modelData.heatPct < 60 ? "#44FF44" : (statusCard.modelData.heatPct < 85 ? "#FFAA00" : "#FF4444") }
                            }
                        }
                        Column { spacing: 1
                            Text { text: "弹"; color: "#667788"; font.pixelSize: 8 }
                            Rectangle { width: 28; height: 6; radius: 2; color: "#1A2A4A"
                                Rectangle { width: parent.width * (statusCard.modelData.ammoPct / 100); height: 6; radius: 2; color: statusCard.modelData.ammoPct > 30 ? "#44FF44" : "#FF4444" }
                            }
                        }
                    }

                    Row { spacing: 4; anchors.horizontalCenter: parent.horizontalCenter
                        Text { text: "锁:" + (statusCard.modelData.lockedTarget || "-"); color: statusCard.modelData.lockedTarget !== "-" ? "#FFAA00" : "#667788"; font.pixelSize: 8 }
                        Rectangle { width: 8; height: 8; radius: 4; color: statusCard.modelData.canFire ? "#44FF44" : "#444444"; anchors.verticalCenter: parent.verticalCenter }
                    }
                }
            }
        }
    }
}
