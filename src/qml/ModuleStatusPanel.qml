pragma ComponentBehavior: Bound

import QtQuick 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    color: "transparent"

    // qmllint disable unqualified
    readonly property var gameDataContext: typeof gameData !== "undefined" ? gameData : null
    // qmllint enable unqualified
    property var moduleList: root.gameDataContext ? root.gameDataContext.moduleList : []

    // 状态颜色映射
    function getStatusColor(status) {
        // 协议定义 0 正常、1 离线、2 故障；3 仅保留为界面兼容的警告色。
        switch (status) {
        case 0:
            return "#00FF00"; // 正常
        case 1:
            return "#FF0000"; // 离线
        case 2:
            return "#FF0000"; // 错误
        case 3:
            return "#FFFF00"; // 警告
        default:
            return "#888888"; // 未知
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 5

        // 标题
        Text {
            text: "11) 模块状态"
            color: "white"
            font.pixelSize: 16
            font.bold: true
            font.family: "Microsoft YaHei"
        }

        // 模块网格
        GridLayout {
            columns: 5
            columnSpacing: 15
            rowSpacing: 10

            Repeater {
                model: root.moduleList

                Item {
                    id: moduleRow
                    required property var modelData

                    // 预留斜线状态条和名称的宽度。
                    Layout.preferredWidth: 110
                    Layout.preferredHeight: 24

                    Row {
                        spacing: 8
                        anchors.verticalCenter: parent.verticalCenter

                        // 斜线状态条
                        Rectangle {
                            width: 6
                            height: 16
                            color: root.getStatusColor(moduleRow.modelData.status)
                            transform: Matrix4x4 {
                                property real skewX: -0.3 // 水平错切
                                matrix: Qt.matrix4x4(1, skewX, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)
                            }
                        }
                    }

                    // 斜线状态条
                    Rectangle {
                        id: bar
                        width: 6
                        height: 14
                        color: root.getStatusColor(moduleRow.modelData.status)
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter

                        transform: Rotation {
                            origin.x: bar.width / 2
                            origin.y: bar.height / 2
                            axis {
                                x: 1
                                y: 0
                                z: 0
                            }
                            angle: 0
                        }
                        rotation: 20
                        antialiasing: true
                    }

                    // 模块名称
                    Text {
                        anchors.left: bar.right
                        anchors.leftMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: moduleRow.modelData.name
                        color: "white"
                        font.pixelSize: 13
                        font.family: "Microsoft YaHei"
                    }
                }
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
