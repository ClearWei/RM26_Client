import QtQuick 2.15

Item {
    id: root
    anchors.fill: parent
    focus: true
    property real resolutionScale: 1.0

    // === 数据属性 ===
    // gameData 由 MainWindow 注入 QML 上下文，统一在弹窗编排入口适配。
    // qmllint disable unqualified
    readonly property var gameDataContext: typeof gameData !== "undefined" ? gameData : null
    // qmllint enable unqualified
    property var activePopups: (root.gameDataContext && root.gameDataContext.activePopups) ? root.gameDataContext.activePopups : []

    // Loader.item 的具体类型由 source 决定，统一在这里做运行时接口检查。
    function invokeLoaderMethod(loader, methodName, argument) {
        if (!loader || !loader.item)
            return false

        var method = loader.item[methodName]
        if (typeof method !== "function")
            return false

        if (arguments.length >= 3)
            method.call(loader.item, argument)
        else
            method.call(loader.item)
        return true
    }

    function focusRespawnIfVisible() {
        if (respawnLoader.visible)
            root.invokeLoaderMethod(respawnLoader, "forceActiveFocus")
    }

    function findPayloadFor(type) {
        if (!root.activePopups) return undefined;
        for (var i = 0; i < root.activePopups.length; ++i) {
            var p = root.activePopups[i];
            if (p && p.type === type) return p.payload ? p.payload : {};
        }
        return undefined;
    }

    // === 弹窗实例 ===
    Loader {
        id: prepLoader
        anchors.fill: parent
        asynchronous: false
        source: "qrc:/qml/PrepPhasePopup.qml"
        visible: (function() {
            for (var i=0;i<root.activePopups.length;++i) if (root.activePopups[i].type === "PrepPhase") return true;
            return false;
        })()
        onLoaded: root.invokeLoaderMethod(prepLoader, "applyPayload", root.findPayloadFor("PrepPhase"))
    }
    Binding {
        target: prepLoader.item
        property: "resolutionScale"
        value: root.resolutionScale
        when: prepLoader.item !== null
    }

    Loader {
        id: countdownLoader
        anchors.fill: parent
        asynchronous: false
        source: "qrc:/qml/Countdown.qml"
        visible: (function() {
            for (var i=0;i<root.activePopups.length;++i) if (root.activePopups[i].type === "Countdown") return true;
            return false;
        })()
        onLoaded: root.invokeLoaderMethod(countdownLoader, "applyPayload", root.findPayloadFor("Countdown"))
    }
    Binding {
        target: countdownLoader.item
        property: "resolutionScale"
        value: root.resolutionScale
        when: countdownLoader.item !== null
    }

    Loader {
        id: respawnLoader
        anchors.fill: parent
        asynchronous: false
        source: "qrc:/qml/RobotRespawn.qml"
        visible: (root.gameDataContext && root.gameDataContext.robotRespawnStatus && root.gameDataContext.robotRespawnStatus.is_pending_respawn) ? true : false
        onLoaded: {
            if (root.gameDataContext && root.gameDataContext.robotRespawnStatus)
                root.invokeLoaderMethod(respawnLoader, "updateFromStatus", root.gameDataContext.robotRespawnStatus)
            Qt.callLater(root.focusRespawnIfVisible)
        }
        onVisibleChanged: {
            if (visible) Qt.callLater(root.focusRespawnIfVisible)
        }
    }
    Binding {
        target: respawnLoader.item
        property: "resolutionScale"
        value: root.resolutionScale
        when: respawnLoader.item !== null
    }

    Loader {
        id: outLoader
        anchors.fill: parent
        asynchronous: true
        source: "qrc:/qml/Out.qml"
        visible: (function() {
            for (var i=0;i<root.activePopups.length;++i) if (root.activePopups[i].type === "Out") return true;
            return false;
        })()
        onLoaded: root.invokeLoaderMethod(outLoader, "applyPayload", root.findPayloadFor("Out"))
    }
    Binding {
        target: outLoader.item
        property: "resolutionScale"
        value: root.resolutionScale
        when: outLoader.item !== null
    }

    Loader {
        id: pauseLoader
        anchors.fill: parent
        asynchronous: false
        source: "qrc:/qml/BattlePausePopup.qml"
        visible: (function() {
            for (var i=0;i<root.activePopups.length;++i) if (root.activePopups[i].type === "BattlePause") return true;
            return false;
        })()
        onLoaded: root.invokeLoaderMethod(pauseLoader, "applyPayload", root.findPayloadFor("BattlePause"))
    }
    Binding {
        target: pauseLoader.item
        property: "resolutionScale"
        value: root.resolutionScale
        when: pauseLoader.item !== null
    }

    // 接收来自 GameData 的 payload 增量更新，直接应用到对应 Loader 实例，避免重建
    Connections {
        target: root.gameDataContext
        function onPopupPayloadUpdated(type, payload) {
            try {
                switch (type){
                case "PrepPhase": root.invokeLoaderMethod(prepLoader, "applyPayload", payload); break;
                case "Countdown": root.invokeLoaderMethod(countdownLoader, "applyPayload", payload); break;
                case "Out": root.invokeLoaderMethod(outLoader, "applyPayload", payload); break;
                case "BattlePause": root.invokeLoaderMethod(pauseLoader, "applyPayload", payload); break;
                default: break;
                }
            } catch (e) { console.warn("PopupOverlay.onPopupPayloadUpdated error", e); }
        }
        function onRobotRespawnStatusUpdated(status) {
            try {
                root.invokeLoaderMethod(respawnLoader, "updateFromStatus", status)
                if (status && status.is_pending_respawn) Qt.callLater(root.focusRespawnIfVisible)
            } catch (e) { console.warn(e); }
        }
    }

}
