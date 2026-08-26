// SPDX-License-Identifier: MIT
/**
 * @file PrepPhasePopup.qml
 * @brief 准备阶段/自检阶段弹窗
 * @details 显示倒计时和阶段说明，提醒设置机器人性能
 *          参考官方客户端设计，包含警告提示框
 * @author Clear
 * @date 2026-01-31
 * @copyright Copyright (c) 2026 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

import QtQuick 2.15
import QtQuick.Layouts 1.15

// 准备阶段弹窗 - 在比赛准备阶段居中显示
Item {
    id: root
    anchors.fill: parent
    property real resolutionScale: 1.0
    readonly property real uiScale: resolutionScale

    // === 数据属性 ===
    // overlay 驱动属性：允许外部强制控制可见性以及覆盖标题/说明
    property bool forceVisible: false
    property string overlayTitle: ""
    property string overlayDescription: ""
    // qmllint disable unqualified
    readonly property var gameDataContext: typeof gameData !== "undefined" ? gameData : null
    // qmllint enable unqualified

    property int remainingTime: root.gameDataContext ? root.gameDataContext.remainingTime : 89  // 剩余时间 (秒)
    property string gamePhase: root.gameDataContext ? root.gameDataContext.gamePhase : "未开始"  // 当前阶段名称 (默认"未开始"以避免启动时弹窗)

    // === 显示逻辑 ===
    property bool isPrepPhase: root.gamePhase === "准备阶段" || root.gamePhase === "自检阶段"
    // visible 由 overlay 的 forceVisible 或原有阶段逻辑决定
    visible: root.forceVisible || isPrepPhase
    z: 100  // 确保在最上层

    // === 半透明遮罩背景 ===
    Rectangle {
        anchors.fill: parent
        color: "#60000000"
        visible: root.visible

        MouseArea {
            anchors.fill: parent
            // 阻止点击穿透
        }
    }

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

        // 弹窗背景图片 (CheckRobotFrame)
        Image {
            id: frameImage
            anchors.fill: parent
            source: {
                if(root.gamePhase === "准备阶段" && root.remainingTime <= 15){
                            return "qrc:/images/gamephase/gamestatus_red.png";
                        }else{
                            return "qrc:/images/gamephase/gamestatus_cyan.png";
                        }
            }
            fillMode: Image.Stretch
        }

        // 备用背景 (图片加载失败时显示)
        Rectangle {
            anchors.fill: parent
            color: "#DD1A1A2E"
            radius: 8
            border.color: "#404060"
            border.width: 1
            visible: frameImage.status !== Image.Ready
            z: -1
        }

        // === 内容布局 ===
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            // 顶部标题栏
            RowLayout {
                Layout.fillWidth: true
                spacing: 20

                Item {
                    Layout.fillWidth: true
                }  // 左侧弹簧

                // 倒计时显示
                Text {
                    id: countdownText
                    text: {
                        var m = Math.floor(root.remainingTime / 60);
                        var s = root.remainingTime % 60;
                        return (m < 10 ? "0" + m : m) + ":" + (s < 10 ? "0" + s : s);
                    }
                    Layout.alignment: Qt.AlignHCenter
                    color: {
                        if(root.gamePhase === "准备阶段" && root.remainingTime <= 15){
                            return "#FF4444";
                        }else{
                            return "#00FFFF";
                        }
                    }
                    font.family: "Arial Black"
                    font.pixelSize: 32
                    font.bold: true
                }

                Item {
                    Layout.fillWidth: true
                }  // 右侧弹簧
            }
            // 阶段标题
            Text {
                transform: Translate { y: -5 }
                text: {
                    if (root.overlayTitle !== "") return root.overlayTitle;
                    if (root.gamePhase === "准备阶段" || root.gamePhase === "自检阶段") return root.gamePhase;
                    return "";
                }
                Layout.alignment: Qt.AlignHCenter
                color: "#FFFFFF"
                font.family: "Microsoft YaHei"
                font.pixelSize: 18
                font.bold: true
            }
                // 说明文字（可被 overlay 覆盖）
            Text {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter
                text: {
                    if (root.overlayDescription !== "") return root.overlayDescription;
                    if (root.gamePhase === "自检阶段") {
                        return "裁判系统自检，请勿抢跑";
                    } else if (root.gamePhase === "准备阶段" && root.remainingTime <= 15) {
                        return "技术暂停窗口已关闭\n";
                    } else {
                        return "请检查键鼠等官方设备，如有疑问及时提出\n00:15前可申请技术暂停，申请后不可撤销或修改";
                    }
                }
                color: "#AAAAAA"
                font.family: "Microsoft YaHei"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            // === 警告框 ===
            Item {
                transform: Translate { y: -5 }
                Layout.fillWidth: true
                Layout.preferredHeight: 50
                visible:root.gamePhase === "准备阶段"

                // 警告背景图
                Image {
                    id: warningBg
                    anchors.fill: parent
                    source: "qrc:/images/message/warning_box.png"
                    fillMode: Image.Stretch
                }

                // 备用警告背景
                Rectangle {
                    anchors.fill: parent
                    color: "transparent"
                    border.color: "#FF4444"
                    border.width: 2
                    radius: 4
                    visible: warningBg.status !== Image.Ready
                }

                // 警告文字
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
