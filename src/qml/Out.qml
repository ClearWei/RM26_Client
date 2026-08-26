/**
 * @file Out.qml
 * @brief 机器人被裁判系统踢出弹窗
 * @details 标题“机器人被踢出比赛”，文字说明“主裁判将机器人踢出比赛”，弹窗背景图片使用"qrc:/images/prep_phase_frame.png"，弹窗布局以及代码格式均参考PrepPhasePopup.qml
 *          数据绑定暂不清楚，暂时留空
 */

import QtQuick 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    anchors.fill: parent
    property real resolutionScale: 1.0
    readonly property real uiScale: resolutionScale

    // === 数据属性 ===

    // qmllint disable unqualified
    readonly property var gameDataContext: typeof gameData !== "undefined" ? gameData : null
    // qmllint enable unqualified

    // === 显示逻辑与外部数据绑定：例如从gameData获取 ===
    property bool outVisible: root.gameDataContext ? root.gameDataContext.kickedAll : false
    visible: outVisible
    z: 200

    // === 主弹窗容器 ===
    Item {
        id: dialogContainer
        width: 650
        height: 250
        scale: root.uiScale
        transformOrigin: Item.Top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 180 * root.uiScale

        Image {
            id: frameImage
            anchors.fill: parent
            source: "qrc:/images/gamephase/gamestatus_yellow.png"
            fillMode: Image.Stretch
        }

        // 备用背景（图片未加载时显示）
        Rectangle {
            anchors.fill: parent
            color: "#DD1A1A2E"
            radius: 8
            border.color: "#404060"
            border.width: 1
            visible: frameImage.status !== Image.Ready
            z: -1
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Item {Layout.fillWidth: true}

                Text {
                    text: "机器人被踢出比赛"
                    color: "#FFFFFF"
                    Layout.alignment: Qt.AlignHCenter
                    font.family: "Microsoft YaHei"
                    font.pixelSize: 20
                    font.bold: true
                }

                Item { Layout.fillWidth: true }
            }

            Text {
                text: "主裁判将机器人踢出比赛"
                color: "#DDDDDD"
                font.family: "Microsoft YaHei"
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 24
            }
        }
    }
}
