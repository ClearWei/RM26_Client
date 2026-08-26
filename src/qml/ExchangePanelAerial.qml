// SPDX-License-Identifier: MIT
/**
 * ExchangePanelAerial.qml
 * @brief H键触发的兑换/交流面板
 * @details 按下H键显示，再次按下或ESC隐藏。显示资源兑换和交流信息。
 * @author Clear
 * @date 2025-12-13
 * @copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    focus: true

    //  ---------- 面板尺寸与背景样式 ----------
    width: 300
    height: 190

    // 半透明深色背景
    radius: 0.053 * root.height
    color: "transparent"

    // 边缘发光
    Rectangle {
        id: glowBorder
        anchors.fill: parent
        color: "transparent"
        radius: root.radius
        border.color: "#4A4F57"
        border.width: 1
        z: 1000
        visible: root.isVisible
    }

    // ---------------- 1.  对外信号与属性（----------------
    signal closed                     // 面板关闭信号（供外部监听）

    //------------------------ 2.变量（数据）------------------------------
    // gameData 与 network 由 MainWindow 注入 QML 上下文，统一在面板入口适配。
    // qmllint disable unqualified
    readonly property var gameDataContext: typeof gameData !== "undefined" ? gameData : null
    readonly property var networkContext: typeof network !== "undefined" ? network : null
    // qmllint enable unqualified
    // 剩余免费时间（秒），外部可随时更新此值以刷新显示
    readonly property real consumedSeconds: root.gameDataContext
        ? root.gameDataContext.airSupportCostCoins : 0.0 //空中支援消费金币
    readonly property real totalSeconds: root.gameDataContext
        ? root.gameDataContext.airSupportLeftTime : 30.0 //总剩余时间
    property bool isVisible: true       //是否可见


    //常量
    readonly property int airSupportCmdFree: 1
    readonly property int airSupportCmdPaid: 2
    readonly property int airSupportCmdInterrupt: 0




    // ---------------- 3.键盘与鼠标交互 ----------------

    Shortcut {
        sequence: "N"
        context: Qt.ApplicationShortcut
        enabled: root.visible
        onActivated: root.actionSelected(0)
    }

    Shortcut {
        sequence: "Ctrl+1"
        context: Qt.ApplicationShortcut
        enabled: root.visible && root.totalSeconds > 0
        onActivated: root.actionSelected(1)
    }
    Shortcut {
        sequence: "Ctrl+2"
        context: Qt.ApplicationShortcut
        enabled: root.visible
        onActivated: root.actionSelected(2)
    }

    Keys.onPressed: (event) => {
        if (!root.visible || !(event.modifiers & Qt.ControlModifier)) {
            return
        }

        if (event.key === Qt.Key_1) {
            root.actionSelected(1)
            event.accepted = true
        } else if (event.key === Qt.Key_2) {
            root.actionSelected(2)
            event.accepted = true
        }
    }



    // ------------------ 4.触发函数 ----------------------
    //使用network发送
    function sendAirSupportCommandToServer(commandId) {
        if (root.networkContext && root.networkContext.sendAirSupportCommand) {
            return root.networkContext.sendAirSupportCommand(commandId)
        }
        console.warn("[ExchangePanelAerial] network.sendAirSupportCommand not available, drop cmd", commandId)
        return false
    }

    function endPannel() {
        root.actionSelected(0)
    }

    //Ctrl + 1/2 /按下n按键  发送消息
    function actionSelected(action) {
        console.log("[ExchangePanelAerial] actionSelected", action,
                    "totalSeconds=", root.totalSeconds,
                    "networkReady=", root.networkContext
                        && root.networkContext.sendAirSupportCommand)
        if (action === 1) {
           if (root.sendAirSupportCommandToServer(root.airSupportCmdFree))
               freeExchange1.open()
        }
        else if (action === 2 && root.totalSeconds > 0.0) {
           if (root.sendAirSupportCommandToServer(root.airSupportCmdPaid))
               freeExchange2.open()
        }
        else if (action === 2 && root.totalSeconds <= 0.0) {
            if (root.sendAirSupportCommandToServer(root.airSupportCmdPaid))
                exchange.open()
        }
        else if (action === 0) {
            if (!root.sendAirSupportCommandToServer(root.airSupportCmdInterrupt))
                return
             if(freeExchange1.opened) {
                freeExchange1.close()
             }
             if(freeExchange2.opened) {
                freeExchange2.close()
             }
             if(exchange.opened) {
                exchange.close()
             }
             endSupport.open()
        }
        root.isVisible = false
    }


    Timer {
        id: exchangeTimer
        interval: 1000
        repeat: true
        running: false
        onTriggered: {
            if(!exchange.opened){
                if(root.totalSeconds <= 0.0) {
                    exchangeTimer.stop()
                    if(freeExchange1.opened) {
                        freeExchange1.close()
                        endSupport.open()
                    }
                    else if (freeExchange2.opened) {
                        freeExchange2.close()
                        exchange.open()
                    }
                }
            }
        }
    }


    // ---------------------  5.主布局 ---------------------
    Rectangle {
        id: mainPanel
        anchors.fill: parent
        color: Qt.rgba(75/255, 77/255, 80/255, 200/255) // 与 H 键提示背景相同的低饱和度灰色
        border.width: 0
        radius: 0.042 * root.height
        visible: root.isVisible

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 0.04 * root.width
            spacing: 0.053 * root.height

            // 顶部两列：左（自动结束），右（消耗金币）
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0.056 * root.width

                // --- 左：免费时间耗尽后自动结束 ---
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spacing: 0.047 * root.height
                    opacity: root.totalSeconds > 0 ? 1.0 : 0.3

                    Item{
                        Layout.preferredHeight: 0.033 * root.height
                    }
                    // 标题（两行）
                    Text {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 0.133 * root.height
                        horizontalAlignment: Text.AlignHCenter
                        text: "免费时间耗尽后\n自动结束"
                        color: "#DDDDDD"
                        font.pixelSize: 0.053 * root.height
                        font.bold: true
                    }

                    // 快捷键展示标签（CTRL + 1）
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 0.24 * root.width
                        Layout.preferredHeight: 0.12 * root.height
                        color: "transparent"
                        radius: 0.027 * root.height

                        Text {
                            anchors.centerIn: parent
                            text: "CTRL + 1"
                            color: "#1FC48C"
                            font.pixelSize: 0.047 * root.height
                            font.bold: true
                        }

                    }

                    // 剩余时间显示：示例“剩余0.0秒”
                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: "剩余" + root.totalSeconds.toFixed(1) + "秒"
                        color: "#DDDDDD"
                        font.pixelSize: 0.053 * root.height
                        font.bold: true
                    }
                }

                // --- 右：消耗金币（优先免费时间） ---
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spacing: 0.047 * root.height

                    Item {
                        Layout.preferredHeight: 0.033 * root.height
                    }

                    // 标题（两行，含括号）
                    Text {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 0.133 * root.height
                        horizontalAlignment: Text.AlignHCenter
                        text: "消耗金币\n(优先免费时间)"
                        color: "#DDDDDD"
                        font.pixelSize: 0.053 * root.height
                        font.bold: true
                    }

                    // 快捷键展示标签（CTRL + 2，绿色）
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 0.24 * root.width
                        Layout.preferredHeight: 0.12 * root.height
                        color: "transparent"
                        radius: 0.027 * root.height

                        Text {
                            anchors.centerIn: parent
                            text: "CTRL + 2"
                            color: "#1FC48C"
                            font.pixelSize: 0.047 * root.height
                            font.bold: true
                        }
                    }
                }
            }

            Item{
                    Layout.fillHeight: true
            }
        }
    }

     // ---------- 按键组件 ------------
     // ----------freeExchange1 免费时间------------

    Popup {
        id: freeExchange1
        parent: root.Window.window ? root.Window.window.contentItem : root
        modal: true
        Overlay.modal: Rectangle { color: "transparent" }
        Overlay.modeless: Rectangle { color: "transparent"}
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        anchors.centerIn: parent
        width: root.width
        height: root.height

        onVisibleChanged: {
            if (visible) {
                exchangeTimer.start()
            } else {
                exchangeTimer.stop()
            }
        }

       Shortcut {
            sequence: "N"
            enabled: freeExchange1.visible
            onActivated: root.actionSelected(0)
        }


        background: Rectangle {
            color: Qt.rgba(75/255, 77/255, 80/255, 200/255)
            radius: 0.033 * root.height
            border.color: "#4A4F57"
            border.width: 1
        }

        contentItem: ColumnLayout {
            id: aerialContent
            focus: true
            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_N) {
                    root.endPannel();
                    event.accepted = true;
                }
            }
            anchors.margins: 0.04 * root.width
            spacing: 0.027 * root.height

            Item{
                Layout.fillHeight: true
            }

            // 1.空中支援中
            Text {
                id: aerialOnlineTitle1
                Layout.fillWidth: true
                textFormat: Text.PlainText
                horizontalAlignment: Text.AlignHCenter
                text:"空中支援中"
                color: "#DDDDDD"
                font.pixelSize: 0.1 * root.height
                font.bold: true
            }
            // 2.本次呼叫剩余时间
            Text {
                id: aerialOnlineDetail1
                Layout.fillWidth: true
                textFormat: Text.PlainText
                horizontalAlignment: Text.AlignHCenter
                text:"本次呼叫剩余"+root.totalSeconds.toFixed(1)+"秒\n免费空中支援时间剩"+root.totalSeconds.toFixed(1)+"秒"
                color: "#DDDDDD"
                font.pixelSize: 0.053 * root.height
                font.bold: true
            }

            Item{
                Layout.fillHeight: true
            }

            // 底部提示：“按 N 结束空中支援”
            Text {
                Layout.fillWidth: true
                Layout.preferredHeight: 0.2 * root.height
                Layout.bottomMargin: 0.035 * root.height
                horizontalAlignment: Text.AlignHCenter|| Text.AlignVCenter
                text: "按N结束空中支援"
                color: "#DDDDDD"
                font.pixelSize: 0.06 * root.height
                font.bold: true
            }
        }
    }

   // ----------freeExchange2 空中支援------------
    Popup {
        id: freeExchange2
        parent: root.Window.window ? root.Window.window.contentItem : root
        modal: true
        Overlay.modal: Rectangle { color: "transparent" }
        Overlay.modeless: Rectangle { color: "transparent"}
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        anchors.centerIn: parent
        width: root.width
        height: root.height

        onVisibleChanged: {
            if (visible) {
                exchangeTimer.start()
            } else {
                exchangeTimer.stop()
            }
        }

        Shortcut {
            sequence: "N"
            enabled: freeExchange2.visible
            onActivated: root.actionSelected(0)
        }

        background: Rectangle {
            color: Qt.rgba(75/255, 77/255, 80/255, 200/255)
            radius: 0.033 * root.height
            border.color: "#4A4F57"
            border.width: 1
        }

        contentItem: ColumnLayout {
            focus: true
            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_N) {
                    root.endPannel();
                    event.accepted = true;
                } else if (event.key === Qt.Key_F) {
                    freeExchange2.close();
                    exchange.open();
                    event.accepted = true;
                } else if (event.key === Qt.Key_G) {
                    freeExchange2.close();
                    event.accepted = true;
                }
            }
            anchors.margins: 0.04 * root.width
            spacing: 0.027 * root.height

            Item{
                Layout.fillHeight: true
            }

            // 1.空中支援中
            Text {
                id: aerialOnlineTitle2
                Layout.fillWidth: true
                textFormat: Text.PlainText
                horizontalAlignment: Text.AlignHCenter
                text:"空中支援中"
                color: "#DDDDDD"
                font.pixelSize: 0.1 * root.height
                font.bold: true
            }
            // 2.本次呼叫剩余时间
            Text {
                id: aerialOnlineDetail2
                Layout.fillWidth: true
                textFormat: Text.PlainText
                horizontalAlignment: Text.AlignHCenter
                text:"本次呼叫剩余"+root.totalSeconds.toFixed(1)+"秒\n"+root.totalSeconds.toFixed(1)+"秒后将消耗金币兑换时间"
                color: "#DDDDDD"
                font.pixelSize: 0.053 * root.height
                font.bold: true
            }

            Item{
                Layout.fillHeight: true
            }
            // 底部提示：“按 N 结束空中支援”
            Text {
                Layout.fillWidth: true
                Layout.preferredHeight: 0.2 * root.height
                Layout.bottomMargin: 0.035 * root.height
                horizontalAlignment: Text.AlignHCenter|| Text.AlignVCenter
                text: "按N结束空中支援"
                color: "#DDDDDD"
                font.pixelSize: 0.06 * root.height
                font.bold: true
            }
        }
    }

    //----------exchange 金币兑换------------
    Popup {
        id: exchange
        parent: root.Window.window ? root.Window.window.contentItem : root
        modal: true
        Overlay.modal: Rectangle { color: "transparent" }
        Overlay.modeless: Rectangle { color: "transparent"}
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        anchors.centerIn: parent
        width: root.width
        height: root.height

        onVisibleChanged: {
            if (visible) {
                exchangeTimer.start()
            } else {
                exchangeTimer.stop()
            }
        }
        Shortcut {
            sequence: "N"
            enabled: exchange.visible
            onActivated: root.actionSelected(0)
        }

        background: Rectangle {
            color: Qt.rgba(75/255, 77/255, 80/255, 200/255)
            radius: 0.033 * root.height
            border.color: "#4A4F57"
            border.width: 1
        }

        contentItem: ColumnLayout {
            focus: true
            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_N) {
                    root.endPannel();
                    event.accepted = true;
                }
            }
            anchors.margins: 0.04 * root.width
            spacing: 0.027 * root.height

            Item{
                Layout.fillHeight: true
            }

            // 1.空中支援中
            Text {
                id: aerialOnlineTitle3
                Layout.fillWidth: true
                textFormat: Text.PlainText
                horizontalAlignment: Text.AlignHCenter
                text:"空中支援中"
                color: "#DDDDDD"
                font.pixelSize: 0.1 * root.height
                font.bold: true
            }
            // 2.本次呼叫剩余时间
            Text {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                horizontalAlignment: Text.AlignHCenter
                text:"本次已消耗"+root.consumedSeconds.toFixed(1)+"金币兑换时间\n（1金币每秒）"
                color: "#FFC107"
                font.pixelSize: 0.053 * root.height
                font.bold: true
            }
            Item{
                Layout.fillHeight:true
            }
            // 底部提示：“按 N 结束空中支援”
            Text {
                Layout.fillWidth: true
                Layout.preferredHeight: 0.2 * root.height
                Layout.bottomMargin: 0.035 * root.height
                horizontalAlignment: Text.AlignHCenter|| Text.AlignVCenter
                text: "按N结束空中支援"
                color: "#DDDDDD"
                font.pixelSize: 0.06 * root.height
                font.bold: true
            }
        }
    }
    //---------- 结束空中支援 ------------
    Popup {
        id: endSupport
        parent: root.Window.window ? root.Window.window.contentItem : root
        modal: true
        Overlay.modal: Rectangle { color: "transparent" }
        Overlay.modeless: Rectangle { color: "transparent"}
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        anchors.centerIn: parent
        width: root.width*0.8
        height: root.height*0.8



        background: Rectangle {
            color: Qt.rgba(75/255, 77/255, 80/255, 200/255)
            radius: 0.033 * root.height
            border.color: "#4A4F57"
            border.width: 1
        }

        contentItem: ColumnLayout {
            anchors.margins: 0.04 * root.width
            focus: true
            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_N) {
                    root.endPannel();
                    event.accepted = true;
                }
            }
            spacing: 0.027 * root.height

            Item{
                Layout.fillHeight:true
            }

            // 1.空中支援中
            Text {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                horizontalAlignment: Text.AlignHCenter
                text:"已结束空中支援"
                color: "#DDDDDD"
                font.pixelSize: 0.1 * root.height
                font.bold: true
            }

            Item{
                Layout.fillHeight:true
            }

            // 底部提示：“按 N 结束空中支援”
            Text {
                Layout.fillWidth: true
                Layout.preferredHeight: 0.2 * root.height
                Layout.bottomMargin: 0.035 * root.height
                horizontalAlignment: Text.AlignHCenter|| Text.AlignVCenter
                text: "按N结束空中支援"
                color: "#DDDDDD"
                font.pixelSize: 0.06 * root.height
                font.bold: true
                opacity: 0.5
            }
        }
        // 自动关闭计时器（默认 6s）
        Timer {
            id: endTimer
            interval: 6000
            running: endSupport.visible
            repeat: false
            onTriggered: {
                endSupport.close()
                root.isVisible = true
            }
        }
    }
}
