/**
 * @file BattlePausePopup.qml
 * @brief 暂停弹窗
 * @details 技术暂停和战斗阶段暂停界面
 */

import QtQuick 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    anchors.fill: parent
    property real resolutionScale: 1.0
    readonly property real uiScale: resolutionScale

    // 数据绑定
    property bool forceVisible: false
    property string overlayTitle: ""
    property string overlayDescription: ""
    // gameData 由 MainWindow 注入 QML 上下文，集中适配后再供组件内部使用。
    // qmllint disable unqualified
    readonly property var gameDataContext: typeof gameData !== "undefined" ? gameData : null
    // qmllint enable unqualified
    property string gamePhase: root.gameDataContext ? root.gameDataContext.gamePhase : "未开始"  // 当前阶段名称 (默认"未开始"以避免启动时弹窗)
    property int remainingTime: root.gameDataContext ? root.gameDataContext.remainingTime : 5
    property bool isPaused: root.gameDataContext ? root.gameDataContext.is_paused : false

    visible: root.forceVisible || root.isPaused
    z: 100

    Rectangle {
        anchors.fill: parent
        color: "#60000000"
        visible: root.visible

        MouseArea {
            anchors.fill: parent
        }
    }

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
                spacing: 20

                Item {
                    Layout.fillWidth: true
                }  // 左侧弹簧

                Text {
                    id: countdownText
                    text:{
                        var m = Math.floor(root.remainingTime / 60);
                        var s = root.remainingTime % 60;
                        return (m < 10 ? "0" + m : m) + ":" + (s < 10 ? "0" + s : s);
                    }
                    Layout.alignment: Qt.AlignHCenter
                    color: "#00FFFF"
                    font.family: "Arial Black"
                    font.pixelSize: 32
                    font.bold: true
                }

                Item {
                    Layout.fillWidth: true
                }  // 右侧弹簧
            }
            //标题
            Text {
                transform: Translate { y: -5 }
                    text: {
                        if (root.overlayTitle !== "") return root.overlayTitle;
                        else if(root.gamePhase==="战斗阶段") return "战斗阶段暂停";
                        return "技术暂停";
                    }
                    color: "#FFFFFF"
                    Layout.alignment: Qt.AlignHCenter
                    font.family: "Microsoft YaHei"
                    font.pixelSize: 18
                    font.bold: true
                }
            //文字说明
            Text {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter
                text: {
                    if (root.overlayDescription !== "") return root.overlayDescription;
                    else if(root.gamePhase==="战斗阶段") return "请等待裁判恢复比赛";
                    return "请耐心等待\n";
                }
                color: "#AAAAAA"
                font.family: "Microsoft YaHei"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            //警告框
            Item {
                transform: Translate { y: -5 }
                Layout.fillWidth: true
                Layout.preferredHeight: 50
                visible:root.gamePhase === "准备阶段"

                Image {
                    id: warningBg
                    anchors.fill: parent
                    source: "qrc:/images/message/warning_box.png"
                    fillMode: Image.Stretch
                }

                Rectangle {
                    anchors.fill: parent
                    color: "transparent"
                    border.color: "#FF4444"
                    border.width: 2
                    radius: 4
                    visible: warningBg.status !== Image.Ready
                }

                Text {
                    anchors.centerIn: parent
                    text: "请尽快设置机器人性能"
                    color: "#FF6666"
                    font.family: "Microsoft YaHei"
                    font.pixelSize: 16
                    font.bold: true
                }
            }
            // 底部提示
            Text {
                transform: Translate { y: -10 }
                visible:root.gamePhase === "准备阶段"
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter
                text: "未设置机器人性能，可按P键设置"
                color: "#888888"
                font.family: "Microsoft YaHei"
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

}
