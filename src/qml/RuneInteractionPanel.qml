/**
 * @file RuneInteractionPanel.qml
 * @brief 长按 F 的大能量机关交互与状态面板
 */

pragma ComponentBehavior: Bound

import QtQuick 2.15

Item {
    id: root

    anchors.fill: parent

    // 由 C++ 在运行时传入，确保位于顶部信息条下方。
    property int panelLeftMargin: 14
    property int panelTopMargin: 90

    property int holdDurationMs: 800
    property real holdProgress: 0.0
    property double holdStartMs: 0
    property bool holding: false
    property bool activatedThisHold: false
    property int consumedChanceCount: 0

    // gameData 由 MainWindow 注入，面板内统一从这个入口读取符文状态。
    // qmllint disable unqualified
    readonly property var gameDataContext: typeof gameData !== "undefined" ? gameData : null
    // qmllint enable unqualified
    readonly property int remainingTime: {
        if (!gameDataContext)
            return 420
        var value = Number(gameDataContext.remainingTime)
        return isFinite(value) ? Math.max(0, Math.floor(value)) : 420
    }
    readonly property int elapsedTime: Math.max(0, 420 - remainingTime)
    readonly property int earnedChanceCount: {
        var count = 0
        if (elapsedTime >= 180)
            count += 1
        if (elapsedTime >= 255)
            count += 1
        if (elapsedTime >= 330)
            count += 1
        return count
    }
    readonly property int remainingChanceCount: Math.max(0, earnedChanceCount - consumedChanceCount)

    readonly property int runeStatusValue: gameDataContext ? Number(gameDataContext.runeStatus) : 1
    readonly property int activatedArmsValue: gameDataContext ? Number(gameDataContext.runeActivatedArms) : 0
    readonly property real averageRingsValue: gameDataContext ? Number(gameDataContext.runeAverageRings) : 0
    readonly property int activationStartRemainingTime: gameDataContext ? Number(gameDataContext.runeActivationStartRemainingTime) : -1
    readonly property int panelWidth: 316
    readonly property int collapsedHeight: 76
    readonly property int expandedHeight: 560
    readonly property int titleFontSize: 20
    readonly property int bodyFontSize: 16
    readonly property int valueFontSize: 18
    readonly property int rowFontSize: 15

    // 每组数据从 GameData 暴露的属性读取，回退到本地占位仅用于未收到协议时
    readonly property var groupAverageRingData: gameDataContext ? gameDataContext.runeGroupAverageRings : [0,0,0,0,0]
    readonly property var groupDeltaData: gameDataContext ? gameDataContext.runeGroupDeltas : [0,0,0,0,0]

    signal activateRequested()

    function beginHold() {
        holding = true
        holdProgress = 0.0
        holdStartMs = Date.now()
        activatedThisHold = false
    }

    function endHold() {
        holding = false
        holdProgress = 0.0
    }

    function markActivated() {
        consumedChanceCount += 1
    }

    function mainTitleText() {
        var remaining = runeActivationRemainingSeconds()
        if (runeStatusValue === 2 || runeStatusValue === 3)
            return "已开启，" + remaining + "s后结束"
        return "长按开启大能量机关(" + remainingChanceCount + ")"
    }

    function runeActivationRemainingSeconds() {
        if (runeStatusValue !== 2 && runeStatusValue !== 3)
            return 0
        if (activationStartRemainingTime < 0)
            return 20
        return Math.max(0, 20 - Math.max(0, activationStartRemainingTime - remainingTime))
    }

    function groupLeftText(index) {
        if (index < activatedArmsValue && index < groupAverageRingData.length)
            return "+" + groupAverageRingData[index]
        return ""
    }

    function groupRightText(index) {
        if (index < activatedArmsValue && index < groupDeltaData.length)
            return "+" + groupDeltaData[index]
        return ""
    }

    Timer {
        id: holdTimer
        interval: 16
        repeat: true
        running: root.holding
        onTriggered: {
            var elapsed = Math.max(0, Date.now() - root.holdStartMs)
            root.holdProgress = Math.min(1.0, elapsed / Math.max(1, root.holdDurationMs))
            if (!root.activatedThisHold && root.holdProgress >= 1.0) {
                root.activatedThisHold = true
                root.activateRequested()
            }
        }
    }

    Rectangle {
        id: panel
        width: root.panelWidth
        height: root.runeStatusValue === 3 ? root.expandedHeight : root.collapsedHeight
        x: root.panelLeftMargin
        y: root.panelTopMargin

        radius: 12
        color: "#00000000"
        border.width: 0

        Rectangle {
            anchors.fill: parent
            radius: 12
            color: "#66595959"
            border.width: 1
            border.color: "#22000000"
        }

        Rectangle {
            id: topBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 76
            color: "#00000000"

            Image {
                anchors.fill: parent
                source: "qrc:/images/rune_tab/rune_tab_bg.png"
                sourceSize.width: 316
                sourceSize.height: 336
                fillMode: Image.PreserveAspectCrop
                opacity: 0.24
            }

            Rectangle {
                anchors.fill: parent
                color: "#C51A1A1A"
                border.width: 0
            }
        }

        Image {
            id: keyCap
            x: 12
            y: 0
            width: 76
            height: 76
            source: root.runeStatusValue === 3 || root.remainingChanceCount <= 0
                    ? "qrc:/images/rune_tab/key_unable.png"
                    : "qrc:/images/rune_tab/key_able.png"
            sourceSize.width: 76
            sourceSize.height: 76
            fillMode: Image.Stretch
            smooth: true
        }

        Text {
            x: 27
            y: 21
            text: "F"
            color: "#EAF6FF"
            font.pixelSize: 30
            font.weight: Font.DemiBold
            font.family: "Microsoft YaHei"
        }

        Text {
            x: 100
            y: 24
            text: root.mainTitleText()
            color: root.runeStatusValue === 3 ? "#F3F3F3" : "#F0F0F0"
            font.pixelSize: 18
            font.weight: Font.Medium
            font.family: "Microsoft YaHei"
        }

        Row {
            id: statsBlock
            visible: root.runeStatusValue === 3
            x: 26
            y: 86
            spacing: 12

            Item {
                width: 90
                height: 90

                Image {
                    anchors.centerIn: parent
                    width: 90
                    height: 90
                    source: root.gameDataContext && Number(root.gameDataContext.currentRobotId) >= 100
                            ? "qrc:/images/rune_tab/rune_tab_blue.png"
                            : "qrc:/images/rune_tab/rune_tab_red.png"
                    sourceSize.width: 191
                    sourceSize.height: 158
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                }
            }

            Column {
                width: 96
                spacing: 10

                Text {
                    text: "平均环数"
                    color: "#F4DEDE"
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    font.family: "Microsoft YaHei"
                }

                Text {
                    text: "总臂数"
                    color: "#F4DEDE"
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    font.family: "Microsoft YaHei"
                }
            }

            Column {
                width: 64
                spacing: 10

                Text {
                    text: root.averageRingsValue.toFixed(1)
                    color: "#FAF0F0"
                    font.pixelSize: 18
                    font.weight: Font.Bold
                    font.family: "Microsoft YaHei"
                }

                Text {
                    text: String(root.activatedArmsValue)
                    color: "#FAF0F0"
                    font.pixelSize: 18
                    font.weight: Font.Bold
                    font.family: "Microsoft YaHei"
                }
            }
        }

        Column {
            visible: root.runeStatusValue === 3
            x: 16
            y: 216
            spacing: 12

            Repeater {
                model: 5
                delegate: Item {
                    id: runeGroup
                    required property int index

                    width: 288
                    height: 48

                    Image {
                        x: 0
                        y: 0
                        width: runeGroup.index < root.activatedArmsValue ? (root.runeStatusValue === 2 && runeGroup.index === root.activatedArmsValue - 1 ? 64 : 52) : 48
                        height: runeGroup.index < root.activatedArmsValue ? (root.runeStatusValue === 2 && runeGroup.index === root.activatedArmsValue - 1 ? 64 : 52) : 48
                        source: runeGroup.index < root.activatedArmsValue
                                ? (root.runeStatusValue === 2 && runeGroup.index === root.activatedArmsValue - 1
                                   ? "qrc:/images/rune_tab/rune_tab_hitting.png"
                                   : "qrc:/images/rune_tab/rune_tab_hit.png")
                                : "qrc:/images/rune_tab/rune_tab_nothit.png"
                        sourceSize.width: runeGroup.index < root.activatedArmsValue ? (root.runeStatusValue === 2 && runeGroup.index === root.activatedArmsValue - 1 ? 64 : 52) : 48
                        sourceSize.height: runeGroup.index < root.activatedArmsValue ? (root.runeStatusValue === 2 && runeGroup.index === root.activatedArmsValue - 1 ? 64 : 52) : 48
                        fillMode: Image.Stretch
                        smooth: true
                    }

                    Text {
                        x: 52
                        y: 10
                        text: "第" + (runeGroup.index + 1) + "组"
                        color: "#F1F1F1"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        font.family: "Microsoft YaHei"
                    }

                    Text {
                        x: 132
                        y: 10
                        width: 48
                        horizontalAlignment: Text.AlignRight
                        text: root.groupLeftText(runeGroup.index)
                        color: "#F3F3F3"
                        font.pixelSize: 16
                        font.weight: Font.Bold
                        font.family: "Microsoft YaHei"
                    }

                    Image {
                        x: 188
                        y: 8
                        width: 2
                        height: 24
                        source: "qrc:/images/rune_tab/rune_tab_delimiter.png"
                        sourceSize.width: 2
                        sourceSize.height: 24
                        fillMode: Image.Stretch
                        smooth: true
                    }

                    Text {
                        x: 204
                        y: 10
                        width: 48
                        horizontalAlignment: Text.AlignLeft
                        text: root.groupRightText(runeGroup.index)
                        color: "#F3F3F3"
                        font.pixelSize: 16
                        font.weight: Font.Bold
                        font.family: "Microsoft YaHei"
                    }
                }
            }
        }
    }
}
