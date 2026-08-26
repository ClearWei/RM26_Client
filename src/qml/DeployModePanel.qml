/**
 * @file DeployModePanel.qml
 * @brief 常驻部署状态提示组件（K/L）
 * @details
 *  1) 组件常驻显示于 H 提示上方；
 *  2) 按键逻辑全部在 QML 内实现：
 *     - 长按 K：进入部署模式
 *     - 长按 L：退出部署模式
 */

import QtQuick

Item {
    id: root
    width: 200
    height: 96
    readonly property real uiScale: Math.min(root.width / 200.0, root.height / 96.0)
    property bool shortcutsEnabled: true
    // gameData 与 network 均由 MainWindow 注入 QML 上下文，统一在入口适配。
    // qmllint disable unqualified
    readonly property var gameDataContext: typeof gameData !== "undefined" ? gameData : null
    readonly property var networkContext: typeof network !== "undefined" ? network : null
    // qmllint enable unqualified
    // 来自 DeployModeStatusSync.status (0:未部署, 1:已部署)
    property int deployModeStatusSync: root.gameDataContext
        && root.gameDataContext.deployModeStatus !== undefined
        ? root.gameDataContext.deployModeStatus : 0
    // mode 状态常量，避免直接使用魔法数字
    readonly property int modeIdle: 0
    readonly property int modeEnterHolding: 1
    readonly property int modeEnterWaitingAck: 2
    readonly property int modeDeployedIdle: 3
    readonly property int modeExitHolding: 4
    readonly property int modeExitWaitingAck: 5

    /*
        mode:
            modeIdle = 未部署待机
            modeEnterHolding = K 长按读条中
            modeEnterWaitingAck = 进入请求已发送，等待 DeployModeStatusSync=1
            modeDeployedIdle = 已部署待机
            modeExitHolding = L 长按读条中
            modeExitWaitingAck = 退出请求已发送，等待 DeployModeStatusSync=0
    */
    property int mode: root.modeIdle


    // 当前读条（0.0 ~ 1.0）
    property real progress: 0.0

    // 长按判定参数(可调)
    property int holdMs: 700            // 从按下到读条完成的时间
    property int releaseGapMs: 100      // 松开后多长时间内再次按下仍视为“近似按住”
    property int initialGraceMs: 500    // 刚按下时的宽松时间（允许误差较大，提升体验）
    property int decayMs: 180          // 松开后读条回退速度（ms 从 1.0 回退到 0.0）

    // 记录快捷键激活时间（用于“近似按住”判断）
    property double lastKTs: 0      // K 最近一次被按下的时间
    property double lastLTs: 0      // L 最近一次被按下的时间
    property double kGraceUntil: 0      // K 按下后宽松时间截止点（允许误差较大，提升体验）
    property double lGraceUntil: 0      // L 按下后宽松时间截止点（允许误差较大，提升体验）
    property real sheenProgress: -0.2      // 光效进度（-0.2 ~ 1.0）

    NumberAnimation {
        target: root
        property: "sheenProgress"
        from: -0.2
        to: 1.0
        duration: 1200
        easing.type: Easing.InOutSine
        running: root.isWaitingAck()
        loops: Animation.Infinite
    }


    function nowMs() {
        return Date.now();
    }

    //K 处于“正在按下”的状态
    function isKHolding() {
        var now = nowMs();
        return now <= kGraceUntil || (now - lastKTs <= releaseGapMs);
    }

    //L 处于“正在按下”的状态
    function isLHolding() {
        var now = nowMs();
        return now <= lGraceUntil || (now - lastLTs <= releaseGapMs);
    }

    // 当前是否处于已部署状态（以协议同步的状态为准）
    function isDeployed() {
        return root.deployModeStatusSync === 1;
    }

    // 是否处于长按读条阶段（部署或退出）
    function isHoldingProgress() {
        return root.mode === root.modeEnterHolding || root.mode === root.modeExitHolding;
    }

    // 是否处于等待服务器确认阶段（部署或退出）
    function isWaitingAck() {
        return root.mode === root.modeEnterWaitingAck || root.mode === root.modeExitWaitingAck;
    }

    // 从协议状态同步界面显示状态
    function syncFromProtocol() {
        if (root.isDeployed()) {
            root.mode = root.modeDeployedIdle;
            root.progress = 1.0;
        } else {
            root.mode = root.modeIdle;
            root.progress = 0.0;
        }
    }

    //显示的按键
    function keyLabelText() {
        return (root.isDeployed() ||
                root.mode === root.modeExitHolding ||
                root.mode === root.modeExitWaitingAck) ? "L" : "K";
    }

    //按键状态
    function keySubText() {
        if (mode === root.modeEnterHolding)
            return "长按部署";
        if (mode === root.modeEnterWaitingAck)
            return "等待部署";
        if (mode === root.modeExitHolding)
            return "长按退出";
        if (mode === root.modeExitWaitingAck)
            return "等待退出";
        return root.isDeployed() ? "长按退出" : "长按部署";
    }

    //---------------------------------- 按键响应 -------------------------------

    // K：长按进入部署
    Shortcut {
        sequence: "K"
        autoRepeat: true
        enabled: root.visible && root.shortcutsEnabled
        onActivated: {
            var now = root.nowMs();
            root.lastKTs = now;
            root.kGraceUntil = Math.max(root.kGraceUntil, now + root.initialGraceMs);

            // 已部署态时忽略 K
            if (root.isDeployed() ||
                    root.mode === root.modeEnterWaitingAck ||
                    root.mode === root.modeExitHolding ||
                    root.mode === root.modeExitWaitingAck)
                return;

            // 开始进入部署读条
            if (root.mode !== root.modeEnterHolding) {
                root.mode = root.modeEnterHolding;
                root.progress = 0;
            }
        }
    }

    // L：长按退出部署
    Shortcut {
        sequence: "L"
        autoRepeat: true
        enabled: root.visible && root.shortcutsEnabled
        onActivated: {
            var now = root.nowMs();
            root.lastLTs = now;
            root.lGraceUntil = Math.max(root.lGraceUntil, now + root.initialGraceMs);

            // 只有已部署/退出读条中才处理 L
            if ((!root.isDeployed() &&
                    root.mode !== root.modeExitHolding &&
                    root.mode !== root.modeExitWaitingAck) ||
                    root.mode === root.modeEnterHolding ||
                    root.mode === root.modeEnterWaitingAck ||
                    root.mode === root.modeExitWaitingAck)
                return;

            if (root.mode !== root.modeExitHolding) {
                root.mode = root.modeExitHolding;
                root.progress = 0;
            }
        }
    }

    //---------------------------------- 时间函数 -------------------------------
    Timer {
        interval: 16
        running: root.visible && root.shortcutsEnabled
        repeat: true
        onTriggered: {
            var dtEnter = interval / root.holdMs;
            var dtExit = interval / root.decayMs;

            if (root.mode === root.modeEnterHolding) {
                if (root.isKHolding()) {
                    root.progress = Math.min(1.0, root.progress + dtEnter);
                } else {
                    root.progress = Math.max(0.0, root.progress - dtExit);
                    if (root.progress <= 0.0)
                        root.mode = root.modeIdle;
                }

                if (root.progress >= 1.0) {
                    root.progress = 1.0;
                    root.mode = root.modeEnterWaitingAck;
                    if (root.networkContext && root.networkContext.sendHeroDeployMode)
                        root.networkContext.sendHeroDeployMode(1);
                }
            } else if (root.mode === root.modeExitHolding) {
                if (root.isLHolding()) {
                    root.progress = Math.min(1.0, root.progress + dtEnter);
                } else {
                    root.progress = Math.max(0.0, root.progress - dtExit);
                    if (root.progress <= 0.0)
                        root.mode = root.modeDeployedIdle;
                }

                if (root.progress >= 1.0) {
                    root.progress = 1.0;
                    root.mode = root.modeExitWaitingAck;
                    if (root.networkContext && root.networkContext.sendHeroDeployMode)
                        root.networkContext.sendHeroDeployMode(0);
                }
            }
        }
    }

    Connections {
        target: root.gameDataContext

        function onDeployModeStatusChanged() {
            // 协议状态变化时同步界面显示状态
            root.syncFromProtocol();
        }
    }

    Component.onCompleted: syncFromProtocol()

    //主显示界面
    Column {
        anchors.fill: parent
        spacing: 4 * root.uiScale

        //显示K/L的按键提示
        Rectangle {
            width: 38 * root.uiScale
            height: 38 * root.uiScale
            radius: 5 * root.uiScale
            anchors.horizontalCenter: parent.horizontalCenter
            color: "transparent"
            border.color: "#8DD9D0"
            border.width: 1

            gradient: Gradient {
                GradientStop { position: 0.0; color: "#8FA4A8" }
                GradientStop { position: 1.0; color: "#606E71" }
            }

            Column {
                anchors.centerIn: parent
                spacing: 1 * root.uiScale
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.keyLabelText()
                    color: "#EAF8FA"
                    font.pixelSize: 18 * root.uiScale
                    font.bold: true
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.keySubText()
                    color: "#D0E8EA"
                    font.pixelSize: 9 * root.uiScale
                }
            }
        }

        //进度条
        Item {
            width: parent.width
            height: 16 * root.uiScale

            Rectangle {
                width: 128 * root.uiScale
                height: 5 * root.uiScale
                radius: 2.5 * root.uiScale
                anchors.centerIn: parent
                visible: root.isHoldingProgress() || root.isWaitingAck()
                color: "#283137"
                border.color: "#3A474F"
                border.width: 1

                Rectangle {
                    width: parent.width * root.progress
                    height: parent.height
                    radius: parent.radius
                    opacity: root.isWaitingAck() ? 0.92 : 1.0
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: root.isWaitingAck() ? "#C2FFE7" : "#A9FFD7" }
                        GradientStop { position: 1.0; color: root.isWaitingAck() ? "#7BF2BE" : "#68DFA8" }
                    }

                    //进度条动画过渡
                    Behavior on width {
                        NumberAnimation {
                            duration: root.isWaitingAck() ? 90 : 60
                            easing.type: Easing.OutCubic
                        }
                    }

                }

            }
        }
    }
}
