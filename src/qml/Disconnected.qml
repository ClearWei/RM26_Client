/**
 * @file Disconnected.qml
 * @brief 机器人断联弹窗
 * @details 当收到RobotStaticStatus未连接信号时将机器人状态设置为Disconnected，检测到机器人状态为Disconnected时触发弹窗，状态改变时隐藏
 *          暂未接入显示
 */

import QtQuick 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    anchors.fill: parent
    z: 200

    // === 主弹窗容器 ===
    Item {
        id: dialogContainer
        width: 650
        height: 250
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 180  // 距离顶部 180px，位于顶部信息栏下方

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
                    text: "当前机器人已断联"
                    color: "#FFFFFF"
                    Layout.alignment: Qt.AlignHCenter
                    font.family: "Microsoft YaHei"
                    font.pixelSize: 20
                    font.bold: true
                }

                Item { Layout.fillWidth: true }
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 24
            }
        }
    }
}
