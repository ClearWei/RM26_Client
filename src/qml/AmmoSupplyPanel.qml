pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    width: 520
    height: 360
    radius: 0.1
    color: "#000000"  //改成黑色不透明背景
    border.color: "#7b92ad"  //添加边框
    border.width: 0.5

    // 外部可配置
    property string panelTitle: "17mm弹丸补给面板"
    property string ammoName: "17mm"
    property int commandType: 1
    property int batchSize: 100
    property int batchPrice: 150
    property int fallbackCurrentCount: 0
    property int maxCount: 1000
    // gameData 由 MainWindow 注入 QML 上下文，统一在组件入口适配。
    // qmllint disable unqualified
    readonly property var gameDataContext: typeof gameData !== "undefined" ? gameData : null
    // qmllint enable unqualified
    readonly property bool canRemoteAmmo: root.gameDataContext
        && root.gameDataContext.canRemoteAmmo !== undefined
        ? root.gameDataContext.canRemoteAmmo : false
    readonly property string statusText: canRemoteAmmo ? "可远程兑换" : "当前不可远程兑换"

    // 交互信号（先留骨架，后面你再接业务）
    signal amountSelected(int delta)     // 当前选中兑换量
    signal commonCommandRequested(int commandType, int param)

    property int selectedDelta: 0
    property int lastStep: 0
    readonly property var negativeOptions: [-10 * batchSize, -4 * batchSize,
                                             -2 * batchSize, -batchSize]
    readonly property var positiveOptions: [batchSize, 2 * batchSize,
                                             4 * batchSize, 10 * batchSize]
    readonly property var myRobotData: root.gameDataContext ? root.gameDataContext.myRobot : null
    readonly property int currentCount: {
        if (!myRobotData)
            return root.fallbackCurrentCount;
        var raw = root.commandType === 2 ? myRobotData.allowedAmmo42mm
                                         : myRobotData.allowedAmmo17mm;
        var v = Number(raw);
        return isFinite(v) ? Math.max(0, Math.floor(v)) : root.fallbackCurrentCount;
    }
    readonly property int currentRobotId: {
        if (!myRobotData)
            return 1;
        var v = Number(myRobotData.robotId);
        return isFinite(v) ? Math.floor(v) : 1;
    }
    readonly property bool isBlueRobot: currentRobotId >= 100
    readonly property int currentEconomy: {
        if (!root.gameDataContext)
            return 0;
        return root.isBlueRobot ? root.gameDataContext.blueEconomy
                                : root.gameDataContext.redEconomy;
    }
    readonly property int ammoCapacity: {
        return root.maxCount;
    }
    readonly property int maxByAmmo: Math.max(0, root.ammoCapacity - root.currentCount)
    readonly property int maxByEconomy: root.batchPrice > 0
                                        ? Math.floor(root.currentEconomy / root.batchPrice) * root.batchSize : 0
    readonly property int maxSelectable: Math.max(0, Math.min(root.maxByAmmo, root.maxByEconomy))
    readonly property int totalPrice: root.selectedDelta > 0
                                      ? Math.ceil(root.selectedDelta / root.batchSize) * root.batchPrice : 0

    function clampSelection(value) {
        var bounded = Math.max(0, Math.min(value, root.maxSelectable));
        return Math.floor(bounded / root.batchSize) * root.batchSize;
    }

    function adjustSelection(step) {
        root.lastStep = step;
        root.selectedDelta = root.clampSelection(root.selectedDelta + step);
        root.amountSelected(root.selectedDelta);
    }

    function openConfirmPopup() {
        if (root.selectedDelta <= 0)
            return;
        confirmPopup.open();
    }

    onCurrentCountChanged: {
        root.selectedDelta = root.clampSelection(root.selectedDelta);
    }
    onCurrentEconomyChanged: {
        root.selectedDelta = root.clampSelection(root.selectedDelta);
    }
    onAmmoCapacityChanged: {
        root.selectedDelta = root.clampSelection(root.selectedDelta);
    }
    onMaxCountChanged: {
        root.selectedDelta = root.clampSelection(root.selectedDelta);
    }

    Column {
        anchors.top: parent.top
        anchors.topMargin: 14
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 8

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: root.panelTitle
            color: "#E6EAF0"
            font.pixelSize: 20
            font.bold: true
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: "已得/可兑换" + root.ammoName + "弹药量"
            color: "#9CA8B3"
            font.pixelSize: 14
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: root.currentCount + "/" + (root.currentCount + root.maxSelectable)
            color: "white"
            font.pixelSize: 24
            font.bold: true
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: root.statusText
            color: "#FF4D4F"
            font.pixelSize: 13
        }
    }

    Row {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.bottom: bottomBar.top
        anchors.bottomMargin: 14
        height: 40
        spacing: 4

        Repeater {
            model: root.negativeOptions
            delegate: Button {
                id: negBtn
                required property var modelData
                width: 46
                height: 40
                text: String(modelData)
                enabled: root.selectedDelta > 0
                hoverEnabled: true
                highlighted: root.lastStep === modelData
                scale: negBtn.hovered && negBtn.enabled ? 1.06 : 1.0
                Behavior on scale {
                    NumberAnimation {
                        duration: 120
                        easing.type: Easing.OutCubic
                    }
                }
                contentItem: Text {
                    text: negBtn.text
                    color: negBtn.enabled ? "#DCE7F5" : "#687281"
                    font.pixelSize: 15
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 0
                    color: !negBtn.enabled ? "#0D121B" : (negBtn.hovered ? "#1F2E40" : (negBtn.highlighted ? "#1D3449" : "#111825"))
                    border.color: !negBtn.enabled ? "#2A323E" : (negBtn.hovered ? "#6DD6FF" : (negBtn.highlighted ? "#48B8E8" : "#3A4654"))
                    border.width: 1
                    Behavior on color {
                        ColorAnimation {
                            duration: 140
                        }
                    }
                    Behavior on border.color {
                        ColorAnimation {
                            duration: 140
                        }
                    }
                }
                onClicked: {
                    root.adjustSelection(modelData);
                }
            }
        }

        Rectangle {
            width: 80
            height: 40
            radius: 0
            color: "#14181F"
            border.color: "#3A4654"
            border.width: 1

            Label {
                anchors.centerIn: parent
                text: String(root.selectedDelta)
                color: "#E6EAF0"
                font.pixelSize: 16
                font.bold: true
            }
        }

        Repeater {
            model: root.positiveOptions
            delegate: Button {
                id: posBtn
                required property var modelData
                width: 46
                height: 40
                text: "+" + modelData
                enabled: root.selectedDelta < root.maxSelectable
                hoverEnabled: true
                highlighted: root.lastStep === modelData
                scale: posBtn.hovered && posBtn.enabled ? 1.06 : 1.0
                Behavior on scale {
                    NumberAnimation {
                        duration: 120
                        easing.type: Easing.OutCubic
                    }
                }
                contentItem: Text {
                    text: posBtn.text
                    color: posBtn.enabled ? "#DCE7F5" : "#687281"
                    font.pixelSize: 15
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 0
                    color: !posBtn.enabled ? "#0D121B" : (posBtn.hovered ? "#1F2E40" : (posBtn.highlighted ? "#1D3449" : "#111825"))
                    border.color: !posBtn.enabled ? "#2A323E" : (posBtn.hovered ? "#6DD6FF" : (posBtn.highlighted ? "#48B8E8" : "#3A4654"))
                    border.width: 1
                    Behavior on color {
                        ColorAnimation {
                            duration: 140
                        }
                    }
                    Behavior on border.color {
                        ColorAnimation {
                            duration: 140
                        }
                    }
                }
                onClicked: {
                    root.adjustSelection(modelData);
                }
            }
        }
    }

    Item {
        id: bottomBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.bottomMargin: 14
        height: 44

        Button {
            id: confirmBtn
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            width: 180
            height: 38
            enabled: root.canRemoteAmmo && root.selectedDelta > 0
            hoverEnabled: true
            scale: confirmBtn.hovered && confirmBtn.enabled ? 1.03 : 1.0
            Behavior on scale {
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.OutCubic
                }
            }
            text: "确认"
            contentItem: Text {
                text: confirmBtn.text
                color: confirmBtn.enabled ? "#DCE7F5" : "#687281"
                font.pixelSize: 16
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: 0
                color: !confirmBtn.enabled ? "#0D121B" : (confirmBtn.hovered ? "#143042" : "#0F2332")
                border.color: !confirmBtn.enabled ? "#2A323E" : (confirmBtn.hovered ? "#6DD6FF" : "#3FB7E9")
                border.width: 1
                Behavior on color {
                    ColorAnimation {
                        duration: 140
                    }
                }
                Behavior on border.color {
                    ColorAnimation {
                        duration: 140
                    }
                }
            }
            onClicked: root.openConfirmPopup()
        }
    }

    Rectangle {
        id: pricePreview
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        width: 190
        height: 86
        radius: 0
        color: "#111825"
        border.color: "#3A4654"
        border.width: 1

        Column {
            anchors.fill: parent
            anchors.margins: 6
            spacing: 3

            Text {
                text: "每 " + root.batchSize + " 发: " + root.batchPrice + " 金币"
                color: "#9CA8B3"
                font.pixelSize: 12
            }
            Text {
                text: "金币: " + root.currentEconomy
                color: "#9CA8B3"
                font.pixelSize: 12
            }
            Text {
                text: "总价: " + root.totalPrice + " 金币"
                color: "#DCE7F5"
                font.pixelSize: 14
                font.bold: true
            }
        }
    }

    Popup {
        id: confirmPopup
        modal: true
        focus: true
        width: 330
        height: 170
        anchors.centerIn: parent
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            radius: 0
            color: "#18212D"
            border.color: "#4B5B6E"
            border.width: 1
        }

        Column {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 14

            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                text: "共消费 " + root.totalPrice + " 金币购买 " + root.selectedDelta + " 颗子弹，是否购买？"
                color: "#E6EDF7"
                font.pixelSize: 15
                font.bold: true
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 12

                Button {
                    id: popupConfirmBtn
                    text: "确认"
                    width: 110
                    hoverEnabled: true
                    contentItem: Text {
                        text: popupConfirmBtn.text
                        color: "#DCE7F5"
                        font.pixelSize: 15
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 0
                        color: popupConfirmBtn.hovered ? "#143042" : "#0F2332"
                        border.color: popupConfirmBtn.hovered ? "#6DD6FF" : "#3FB7E9"
                        border.width: 1
                        Behavior on color {
                            ColorAnimation {
                                duration: 120
                            }
                        }
                        Behavior on border.color {
                            ColorAnimation {
                                duration: 120
                            }
                        }
                    }
                    onClicked: {
                        confirmPopup.close();
                        root.commonCommandRequested(root.commandType,
                                                    root.selectedDelta);
                    }
                }

                Button {
                    id: popupCancelBtn
                    text: "取消"
                    width: 110
                    hoverEnabled: true
                    contentItem: Text {
                        text: popupCancelBtn.text
                        color: "#DCE7F5"
                        font.pixelSize: 15
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 0
                        color: popupCancelBtn.hovered ? "#242B36" : "#1A222E"
                        border.color: popupCancelBtn.hovered ? "#8AA2BE" : "#5A6E86"
                        border.width: 1
                        Behavior on color {
                            ColorAnimation {
                                duration: 120
                            }
                        }
                        Behavior on border.color {
                            ColorAnimation {
                                duration: 120
                            }
                        }
                    }
                    onClicked: confirmPopup.close()
                }
            }
        }
    }
}
