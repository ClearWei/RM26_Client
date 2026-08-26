/**
 * @file FpsDisplay.qml
 * @brief FPS 显示组件
 * @details 使用 FrameAnimation 计算实时渲染帧率
 */
import QtQuick 2.15

Item {
    id: root
    width: 60
    height: 20

    property int fps: 60
    property int frameCounter: 0
    property color textColor: "#00FF00" // 绿色

    // 驱动帧计数 (兼容 Qt 5)
    property real timeSource: 0
    NumberAnimation {
        target: root
        property: "timeSource"
        from: 0
        to: 100
        duration: 1000
        loops: Animation.Infinite
    }
    onTimeSourceChanged: {
        root.frameCounter++;
    }

    // 每秒更新一次显示的 FPS 数值
    Timer {
        interval: 1000
        repeat: true
        running: root.visible
        onTriggered: {
            root.fps = root.frameCounter;
            root.frameCounter = 0;

            // 根据 FPS 改变颜色
            if (root.fps >= 55)
                root.textColor = "#00FF00"; // 绿色
            else
            if (root.fps >= 30)
                root.textColor = "#FFFF00"; // 黄色
            else
                root.textColor = "#FF0000"; // 红色
        }
    }

    Text {
        anchors.centerIn: parent
        text: "FPS: " + root.fps
        color: root.textColor
        font.family: "Inter"
        font.pixelSize: 12
        font.bold: true
        style: Text.Outline
        styleColor: "#000000"
    }
}
