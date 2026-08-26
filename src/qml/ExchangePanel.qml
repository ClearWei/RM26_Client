// SPDX-License-Identifier: MIT
/**
 * ExchangePanel.qml
 * @brief H键触发的兑换/交流面板
 * @details 按下H键显示，再次按下或ESC隐藏。显示资源兑换和交流信息。
            按下N按键模拟无法进行远程兑换
 * @author Clear
 * @date 2025-12-13
 * @copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    // 面板尺寸
    width: 300
    height: 190

    // 半透明深色背景
    color: "transparent"
    radius: 0.027 * root.height


    // ----------------------------1.信号----------------------
    signal closed
    signal commonCommandRequested(int commandType, int param)

    //---------------------------2.变量（数据）-------------------------
    // 机器人类型，由 C++ 传入，例如 "R1 - Hero"、"R3 - Standard"
    property string selectedRobotType: ""
    // gameData 由 MainWindow 注入 QML 上下文，统一在面板入口适配。
    // qmllint disable unqualified
    readonly property var gameDataContext: typeof gameData !== "undefined" ? gameData : null
    // qmllint enable unqualified


    // 当前操作机器人所属队伍剩余经济（由 gameData 链路驱动）
    readonly property int currentTeamEconomy: {
        if (!root.gameDataContext)
            return 0
        if (root.gameDataContext.currentTeamEconomy !== undefined)
            return root.gameDataContext.currentTeamEconomy
        var myRobot = root.gameDataContext.myRobot
        var robotId = myRobot && myRobot.robotId !== undefined ? Number(myRobot.robotId) : 1
        return robotId >= 100 ? root.gameDataContext.blueEconomy
                              : root.gameDataContext.redEconomy
    }
    readonly property bool canRemoteHeal: root.gameDataContext
        && root.gameDataContext.canRemoteHeal !== undefined
        ? root.gameDataContext.canRemoteHeal : false  //可远程兑换血量
    readonly property bool canRemoteAmmo: root.gameDataContext
        && root.gameDataContext.canRemoteAmmo !== undefined
        ? root.gameDataContext.canRemoteAmmo : false    //可远程兑换弹丸


    // 逻辑：仅 Standard(R3/R4/R5) 可以远程购买 17mm
    readonly property bool is17mmEnabled: {
        if (selectedRobotType === "") return true; // 默认开启
        return selectedRobotType.indexOf("Standard") >= 0
                || selectedRobotType.indexOf("Sentry") >= 0;
    }

    // 逻辑：Hero(R1) 可以买 42mm
    readonly property bool is42mmEnabled: {
        if (selectedRobotType === "") return true;
        return selectedRobotType.indexOf("Hero") >= 0;
    }

    // 确认弹窗要展示的提示文本
    property string confirmText: ""

    // 当前交易类型：1=HP, 2=17mm, 3=42mm
    property int currentTransactionType: 0
    readonly property int txHealth: 1
    readonly property int tx17mm: 2
    readonly property int tx42mm: 3

    // 成功弹窗提示文本（“购买成功，6s 后生效” 等）
    property string successText: ""

    // CommonCommand 协议绑定
    readonly property int cmdRemoteExchangeAmmo: 5
    readonly property int cmdRemoteExchangeHealth: 6

    // 比赛倒计时：始终以协议权威时间为准，避免本地测试计时漂移
    readonly property int remainingTime: root.gameDataContext
                                         && root.gameDataContext.remainingTime !== undefined
                                         ? Number(root.gameDataContext.remainingTime)
                                         : 420

    // 动态计算血量兑换消耗：50 + ceil((420 - remainingTime)/60) * 20
    readonly property int healthCost: {
        var safeRemaining = Math.max(0, Math.min(420, Number(root.remainingTime)))
        var elapsed = 420 - safeRemaining
        return 50 + Math.ceil(elapsed / 60.0) * 20
    }

    //兑换上限
    readonly property int ammo17mmLimit: 1000
    readonly property int ammo42mmLimit: 100
    readonly property int ammo17mmCost: 150
    readonly property int ammo42mmCost: 150

    // 经济/规则统一开关
    readonly property bool enableHealthExchange: root.canRemoteHeal
                                          && root.currentTeamEconomy >= root.healthCost
    readonly property bool enable17mmExchange: root.is17mmEnabled
                                          && root.canRemoteAmmo
                                          && root.currentTeamEconomy >= root.ammo17mmCost
                                          && (!root.gameDataContext
                                              || root.gameDataContext.ammo17mmExchangeCount < root.ammo17mmLimit)
    readonly property bool enable42mmExchange: root.is42mmEnabled
                                          && root.canRemoteAmmo
                                          && root.currentTeamEconomy >= root.ammo42mmCost
                                          && (!root.gameDataContext
                                              || root.gameDataContext.ammo42mmExchangeCount < root.ammo42mmLimit)

    property bool isVisible:true

    //----------------------3.函数------------------------------
    // 统一打开确认弹窗的方法
    function showConfirm(message) {
        confirmText = message
        confirmPopup.open()
        isVisible = false
    }

    // 下一步成功提示弹窗封装：先关再开，避免多个弹窗同时存在
    function showSuccess(message, durationMs) {
        successText = message
        successPopup.open()
        successTimer.interval = durationMs > 0 ? durationMs : 6000
        successTimer.restart()
    }

    //使用network
    function sendCommonCommandToServer(cmdType, param) {
        root.commonCommandRequested(cmdType, param)
        return true
    }

    //构建发送参数
    function buildCommonCommandByTransaction() {
        switch (root.currentTransactionType) {
        case root.txHealth:
            // cmd_type 6：远程兑换血量（面板当前固定 60%）
            return { cmdType: root.cmdRemoteExchangeHealth, param: 60 }
        case root.tx17mm:
            // cmd_type 5：远程兑换允许发弹量；17mm 的远程最小兑换单位为 100 发
            return { cmdType: root.cmdRemoteExchangeAmmo, param: 100 }
        case root.tx42mm:
            // cmd_type 5：远程兑换允许发弹量；42mm 的远程最小兑换单位为 10 发
            return { cmdType: root.cmdRemoteExchangeAmmo, param: 10 }
        default:
            return null
        }
    }

    //发送消息
    function commitCurrentTransaction() {
        var cmd = buildCommonCommandByTransaction()
        if (!cmd) {
            console.warn("[ExchangePanel] invalid transaction type:", root.currentTransactionType)
            return false
        }
        return sendCommonCommandToServer(cmd.cmdType, cmd.param)
    }

    //----------------------4.按键触发--------------------------------
    Shortcut {
        sequence: "Ctrl+1";
        enabled: root.enableHealthExchange
        onActivated: {
            root.currentTransactionType = root.txHealth
            root.showConfirm("是否消耗 " + root.healthCost + " 金币\n兑换 60% 血量恢复？")
        }
    }
    Shortcut {
        sequence: "Ctrl+2";
        enabled: root.enable17mmExchange
        onActivated: {
            root.currentTransactionType = root.tx17mm
            root.showConfirm("是否消耗 150 金币\n兑换 100 发 17mm 弹丸？")
        }
    }
    Shortcut {
        sequence: "Ctrl+3";
        enabled: root.enable42mmExchange
        onActivated: {
            root.currentTransactionType = root.tx42mm
            root.showConfirm("是否消耗 150 金币\n兑换 10 发 42mm 弹丸？")
        }
    }



    // ----------------------5.主布局------------------------------
    Rectangle {
        id: mainPanel
        anchors.fill: parent
        color: Qt.rgba(75/255, 77/255, 80/255, 200/255) // 与 H 键提示背景相同的低饱和度灰色
        border.color: "#4A4F57"
        border.width: 1
        radius: root.radius
        visible: root.isVisible

        ColumnLayout {
            id: columnLayout
            anchors.fill: parent
            anchors.leftMargin: 0.04 * root.width
            anchors.rightMargin: 0.04 * root.width
            anchors.topMargin: 0.015 * root.height
            anchors.bottomMargin: 0.03 * root.height
            spacing: 0.02 * root.height
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0.04 * root.height

                // ---60%血量恢复 ---
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 0.054 * root.height
                    opacity: root.enableHealthExchange ? 1.0 : 0.3

                    Image{
                        Layout.preferredWidth:  0.1 * root.width
                        Layout.preferredHeight: 0.14 * root.height
                        Layout.alignment: Qt.AlignHCenter
                        source: "qrc:/images/panel/blood.png"
                        fillMode: Image.PreserveAspectFit
                    }

                    Text{
                        Layout.fillWidth: true
                        Layout.preferredHeight: 0.14 * root.height
                        horizontalAlignment: Text.AlignHCenter
                        text: "60%血量"
                        color: "#DDDDDD"
                        font.pixelSize: 0.043 * root.height
                        font.bold: true
                    }



                    Rectangle{
                        Layout.preferredWidth: 0.2 * root.width
                        Layout.preferredHeight: 0.12 * root.height
                        Layout.alignment: Qt.AlignHCenter
                        color:Qt.rgba(0.16, 0.16, 0.2, 0.6)
                        radius: 0.053 * root.height

                        Text{
                            anchors.centerIn: parent
                            text: "CTRL + 1"
                            color: "#1FC48C"
                            font.pixelSize: 0.04 * root.height
                            font.bold: true
                        }
                    }
                }

                // ---17mm弹丸购买 ---
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 0.054 * root.height
                    // 经济足够且满足规则时不透明，否则半透明
                    opacity: root.enable17mmExchange ? 1.0 : 0.3

                    Image{
                        Layout.preferredWidth:  0.1 * root.width
                        Layout.preferredHeight: 0.14 * root.height
                        Layout.alignment: Qt.AlignHCenter
                        source: "qrc:/images/panel/17.png"
                        fillMode: Image.PreserveAspectFit
                    }

                    Text{
                        Layout.fillWidth: true
                        Layout.preferredHeight: 0.14 * root.height
                        horizontalAlignment: Text.AlignHCenter
                        text: "17mm弹丸"
                        color: "#DDDDDD"
                        font.pixelSize: 0.043 * root.height
                        font.bold: true
                    }

                    Rectangle{
                        Layout.preferredWidth: 0.2 * root.width
                        Layout.preferredHeight: 0.12 * root.height
                        Layout.alignment: Qt.AlignHCenter
                        color:Qt.rgba(0.16, 0.16, 0.2, 0.6)
                        radius: 0.053 * root.height

                        Text{
                            anchors.centerIn: parent
                            text: "CTRL + 2"
                            color: "#1FC48C"
                            font.pixelSize: 0.04 * root.height
                            font.bold: true
                        }

                    }
                }

                // ---42mm弹丸购买 ---
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 0.054 * root.height
                    // 经济足够且满足规则时不透明，否则半透明
                    opacity: root.enable42mmExchange ? 1.0 : 0.3

                    Image{
                        Layout.preferredWidth:  0.1 * root.width
                        Layout.preferredHeight: 0.14 * root.height
                        Layout.alignment: Qt.AlignHCenter
                        source: "qrc:/images/panel/42.png"
                        fillMode: Image.PreserveAspectFit
                    }

                    Text{
                        Layout.fillWidth: true
                        Layout.preferredHeight: 0.14 * root.height
                        horizontalAlignment: Text.AlignHCenter
                        text: "42mm弹丸"
                        color: "#DDDDDD"
                        font.pixelSize: 0.043 * root.height
                        font.bold: true
                    }

                    Rectangle{
                        Layout.preferredWidth: 0.2 * root.width
                        Layout.preferredHeight: 0.12 * root.height
                        Layout.alignment: Qt.AlignHCenter
                        color:Qt.rgba(0.16, 0.16, 0.2, 0.6)
                        radius: 0.053 * root.height

                        Text{
                            anchors.centerIn: parent
                            text: "CTRL + 3"
                            color: "#1FC48C"
                            font.pixelSize: 0.04 * root.height
                            font.bold: true
                        }
                    }
                }
            }
        }
    }

    // 确认弹窗：显示从快捷键或点击触发的提示文字
    Popup {
        id: confirmPopup
        // 将弹窗的视觉父对象设置为窗口，使其不受 root.visible 影响
        parent: root.Window.window ? root.Window.window.contentItem : root
        modal: true
        Overlay.modal: Rectangle { color: "transparent" }
        Overlay.modeless: Rectangle { color: "transparent"}
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        anchors.centerIn: parent
        width: 0.8 * root.width


        // 弹窗内快捷键：Y 进入成功提示；N 仅关闭确认弹窗
        Shortcut {
            sequence: "Y";
            enabled: confirmPopup.visible
            onActivated: {
                confirmPopup.close()
                if (root.commitCurrentTransaction())
                    root.showSuccess("购买成功，6s 后生效", 6000)
            }
        }
        Shortcut { sequence: "N"; enabled: confirmPopup.visible; onActivated: { confirmPopup.close(); root.isVisible = true; } }

        background: Rectangle {
            color: Qt.rgba(0.08, 0.08, 0.12, 0.9)
            radius: 0.027 * root.height
            border.color: "#4A4F57"
            border.width: 1
        }
        contentItem: ColumnLayout {
            anchors.margins: 0.04 * root.width
            spacing: 0.014 * root.height
            Text {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                horizontalAlignment: Text.AlignHCenter
                text: root.confirmText
                color: "#DDDDDD"
                font.pixelSize: 0.049 * root.height
                font.bold: true
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 0.014 * root.height

                Item{
                    Layout.fillWidth: true
                }
                //--- 确定 ---
                Rectangle{
                    Layout.preferredWidth: 0.08 * root.width
                    Layout.preferredHeight: 0.108 * root.height
                    color:Qt.rgba(0.16, 0.16, 0.2, 0.6)
                    radius: 0.014 * root.height
                    border.color: "#1FC48C"
                    border.width: 1

                    Text{
                        anchors.centerIn: parent
                        text: "Y"
                        color: "#1FC48C"
                        font.pixelSize: 0.04 * root.height
                        font.bold: true
                    }

                }

                Text{
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: 0.12 * root.width
                    horizontalAlignment: Text.AlignHCenter
                    text: "确定"
                    color: "#1FC48C"
                    font.pixelSize: 0.054 * root.height
                    font.bold: true
                }

                Item{
                    Layout.fillWidth: true
                }
                // --- 取消 ---
                Rectangle{
                        Layout.preferredWidth: 0.08 * root.width
                        Layout.preferredHeight: 0.108 * root.height
                        color:Qt.rgba(0.16, 0.16, 0.2, 0.6)
                        radius: 0.014 * root.height
                        border.color: "red"
                        border.width: 1


                        Text{
                            anchors.centerIn: parent
                            text: "N"
                            color: "red"
                            font.pixelSize: 0.04 * root.height
                            font.bold: true
                        }
                    }

                Text{
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: 0.12 * root.width
                    horizontalAlignment: Text.AlignHCenter
                    text: "取消"
                    color: "red"
                    font.pixelSize: 0.054 * root.height
                    font.bold: true
                }

                Item{
                    Layout.fillWidth: true
                }
            }
        }
    }
    // 成功提示弹窗：显示“购买成功，6s 后生效”，并自动关闭
    Popup {
        id: successPopup
        // 将弹窗的视觉父对象设置为窗口，使其不受 root.visible 影响
        parent: root.Window.window ? root.Window.window.contentItem : root
        modal: true
        Overlay.modal: Rectangle { color: "transparent" }
        Overlay.modeless: Rectangle { color: "transparent"}
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        anchors.centerIn: parent
        width: 0.8 * root.width
        background: Rectangle {
            color: Qt.rgba(0.08, 0.08, 0.12, 0.9)
            radius: 0.027 * root.height
            border.color: "#4A4F57"
            border.width: 1
        }
        contentItem: Item {
            width: parent.width
            height: 0.324 * root.height
            Text {
                anchors.centerIn: parent
                text: root.successText
                color: "#1FC48C"
                font.pixelSize: 0.054 * root.height
                font.bold: true
            }
        }
        // 自动关闭计时器（默认 6s）
        Timer {
            id: successTimer
            interval: 6000
            running: false
            repeat: false
            onTriggered: {
                successPopup.close()
                root.isVisible = true;
            }
        }
    }
}
