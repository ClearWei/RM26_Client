/**
 * @file Countdown.qml
 * @brief 自检结束后的 5 秒倒计时
 */

import QtQuick 2.15

Item {
	id: root
	anchors.fill: parent
	property real resolutionScale: 1.0
		readonly property real uiScale: resolutionScale

		// === 数据绑定 ===
		// qmllint disable unqualified
		readonly property var gameDataContext: typeof gameData !== "undefined" ? gameData : null
		// qmllint enable unqualified
		property string gamePhase: root.gameDataContext ? root.gameDataContext.gamePhase : "未开始"
		property bool isCountdownPhase: gamePhase === "倒计时"
		property int remainingTime: root.gameDataContext ? root.gameDataContext.remainingTime : 5
	property int displayCountdown: (remainingTime >= 0 && remainingTime <= 5) ? remainingTime : -1

	// 仅在五秒倒计时有效区间内展示图片，避免阶段切换瞬间拼出不存在的资源名
	visible: isCountdownPhase && displayCountdown >= 0
	z: 150

	// 屏幕中心倒计时图片
	Image {
		id: numberWrapper
		anchors.horizontalCenter: root.horizontalCenter
		anchors.verticalCenter: root.verticalCenter
			source: root.displayCountdown > 0 ? "qrc:/images/gamephase/" + "freeze_" + root.displayCountdown + ".png" : (root.displayCountdown === 0 ? "qrc:/images/gamephase/freeze_start.png" : "")
			fillMode: Image.PreserveAspectFit
			width: root.displayCountdown > 0 ? 288 : 509
			height: root.displayCountdown > 0 ? 310 : 112
		scale: root.uiScale
	}

}
