import QtQuick 2.15

/**
 * @file AuxiliaryShootingPanel.qml
 * @brief 辅助射击信息面板 - 显示射速和发弹量
 * @details 位于热量环右侧，紧贴准星区域
 */
Item {
    id: root
    // 与 aimingContainer 尺寸匹配 (400x400)
    width: 400
    height: 400

    // 绑定 C++ 属性
    // qmllint disable unqualified
    readonly property var gameDataContext: typeof gameData !== "undefined" ? gameData : null
    // qmllint enable unqualified
    property var myRobot: root.gameDataContext ? root.gameDataContext.myRobot : null

    // --- 射击信息面板 (紧贴热量环右侧) ---
    Column {
        id: infoPanel
        // 定位：热量环半径约70，向右偏移约90-100像素
        anchors.left: parent.horizontalCenter
        anchors.leftMargin: 85  // 紧贴热量环右边缘
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: -10  // 略微上移
        spacing: 8

        // 射击初速度
        Column {
            spacing: 2
            Text {
                text: "射击初速度"
                color: "#888888"
                font.pixelSize: 11
            }
            Text {
                text: {
                    if (!root.myRobot)
                        return "0.0"
                    if (root.myRobot.firerate !== undefined)
                        return Number(root.myRobot.firerate).toFixed(1)
                    if (root.myRobot.muzzleVelocity !== undefined)
                        return Number(root.myRobot.muzzleVelocity).toFixed(1)
                    return "0.0"
                }
                color: "white"
                font.pixelSize: 20
                font.bold: true
            }
        }

        // 允许发弹量
        Column {
            spacing: 2
            Text {
                text: "允许发弹量"
                color: "#888888"
                font.pixelSize: 11
            }
            Text {
                text: (root.myRobot && root.myRobot.allowedAmmo !== undefined ? root.myRobot.allowedAmmo : "0")
                color: "white"
                font.pixelSize: 20
                font.bold: true
            }
        }
    }
}
