import QtQuick 2.15

// 中央图传区 — 透明背景露出底层 VideoBackgroundWidget + 全息 HUD + 目标卡片
Rectangle {
    id: root
    // 透明背景: 让 MainWindow 中的 VideoBackgroundWidget 透过 QML 显示
    color: "transparent"
    border.color: "#1A2A4A"
    border.width: 1
    radius: 4

    property var model: ({})
    // tacticalAnalyzer 由 MainWindow 的 QML 上下文注入，统一在组件入口适配，
    // 避免内部节点继续依赖隐式作用域。
    // qmllint disable unqualified
    readonly property var analyzer: typeof tacticalAnalyzer !== "undefined"
        ? tacticalAnalyzer : null
    // qmllint enable unqualified

    // 图传占据整个区域 (设为透明让底层 VideoBackgroundWidget 可见)
    Rectangle {
        id: videoPlaceholder
        anchors.fill: parent
        anchors.margins: 2
        color: "transparent"

        // 无视频时显示提示 (半透明)
        Text {
            anchors.centerIn: parent
            text: root.analyzer && !root.analyzer.useMockData ? "" : "图传区域"
            color: "#33224466"
            font.pixelSize: 24
            visible: text !== ""
        }
    }

    // === 全息锁定覆盖层 ===
    HologramLockOverlay {
        id: hologramOverlay
        anchors.fill: parent
        anchors.margins: 4
        model: root.model
    }

    // === 目标信息卡片 (右上角) ===
    TargetInfoCard {
        id: targetCard
        anchors.right: parent.right; anchors.rightMargin: 12
        anchors.top: parent.top; anchors.topMargin: 12
        visible: root.model.targetId !== undefined
        model: root.model
    }
}
