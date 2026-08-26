import QtQuick 2.15

// 第一阶段模拟数据，后续替换为 TacticalSnapshot
QtObject {
    property string layoutMode: "map_primary"

    property var headerData: ({
        matchMode: "正赛",
        redScore: 3,
        blueScore: 1,
        currentRound: 4,
        totalRounds: 5,
        timeRemaining: "3:42",
        stage: "BATTLE",
        linkStatus: "链路 OK",
        linkLatency: 23
    })

    property var resourceData: ({
        allyBaseHp: 3200, allyBaseMax: 5000,
        enemyBaseHp: 4100, enemyBaseMax: 5000,
        allyOutpostHp: 420, allyOutpostMax: 1500,
        enemyOutpostHp: 780, enemyOutpostMax: 1500,
        allyOutpostDestroyed: false, enemyOutpostDestroyed: false,
        allyBaseInvincible: true, enemyBaseInvincible: true,
        allyDefenseBonus: 0, enemyDefenseBonus: 0,
        economyDiff: -350,
        damageDiff: 1420,
        hpDiff: 800
    })

    property var topStatusData: ({
        allyRemainingEconomy: 5000,
        allyTotalEconomyObtained: 5000,
        allyTechLevel: 0,
        allyEncryptionLevel: 0,
        allyFortressOccupationSec: 0,
        enemyFortressOccupationSec: 0,
        respawnGoldCost: 340,
        affordableRespawnCount: 14,
        respawnEconomyVisible: true
    })

    property var keyEvents: ([
        { time: "3:45", icon: "!", text: "敌方前哨站被摧毁", color: "#17dd56", priority: "P1" },
        { time: "3:40", icon: "★", text: "大能量机关被激活", color: "#FFAA00", priority: "P0" },
        { time: "3:15", icon: "⚠", text: "敌方英雄狙击伤害 450", color: "#d52424", priority: "P2" }
    ])

    property var mainDecision: ({
        title: "集火击杀敌方3号步兵",
        priority: "P1",
        confidence: 87,
        windowText: "窗口 ~8秒",
        reasons: ["残血 84/400", "雷达易伤激活", "A3可支援", "基地安全"],
        fallbackActions: ["转火敌方哨兵", "控图保热"]
    })

    property var videoOverlay: ({
        targetId: "E3",
        targetLabel: "敌 3号步兵",
        targetHp: 84,
        targetMaxHp: 400,
        distance: 15.2,
        lockQuality: 0.92,
        hasHologram: true,
        compensationText: "瞄准: ↓2.3"
    })

    property var topTargets: ([
        { rank: 1, id: "E3", label: "敌步兵3", hp: 84, maxHp: 400, threat: 0.92, icon: "infantry" },
        { rank: 2, id: "E2", label: "敌工程2", hp: 310, maxHp: 600, threat: 0.65, icon: "engineer" },
        { rank: 3, id: "E4", label: "敌步兵4", hp: 220, maxHp: 400, threat: 0.58, icon: "infantry" },
        { rank: 4, id: "E1", label: "敌英雄1", hp: 680, maxHp: 800, threat: 0.45, icon: "hero" }
    ])

    property var radarData: ({
        radarAgeMs: 118,
        allyRobots: [
            { id: "A1", label: "A1", x: 0.25, y: 0.55, angle: 45 },
            { id: "A2", label: "A2", x: 0.32, y: 0.62, angle: 90 },
            { id: "A3", label: "A3", x: 0.28, y: 0.48, angle: 30 },
            { id: "A4", label: "A4", x: 0.22, y: 0.42, angle: 0 },
            { id: "A6", label: "A6", x: 0.38, y: 0.55, angle: 135 },
            { id: "A7", label: "A7", x: 0.42, y: 0.70, angle: 180 }
        ],
        enemyRobots: [
            { id: "E1", label: "E1", x: 0.60, y: 0.55, angle: 225 },
            { id: "E2", label: "E2", x: 0.68, y: 0.42, angle: 270 },
            { id: "E3", label: "E3", x: 0.55, y: 0.38, angle: 200 },
            { id: "E4", label: "E4", x: 0.72, y: 0.60, angle: 240 }
        ],
        focusTargetId: "E3",
        mapImageSource: "qrc:/images/minimap_bg_red_left.png",
        routes: [{ fromX: 0.28, fromY: 0.48, toX: 0.55, toY: 0.38 }],
        dangerZones: [{ centerX: 0.65, centerY: 0.50, radius: 0.08 }],
        buffZones: [{ x: 0.45, y: 0.35 }]
    })

    property var mapData: radarData

    property var allyRobotList: ([
        { id: "A1", slot: 1, team: "ally", type: "hero", label: "英雄", hp: 2580, maxHp: 4000, hpPct: 65, heatPct: 30, capPct: 85, ammoPct: 70, online: true, alive: true, stale: false, ageMs: 120 },
        { id: "A2", slot: 2, team: "ally", type: "engineer", label: "工程", hp: 420, maxHp: 500, hpPct: 84, heatPct: -1, capPct: 60, ammoPct: 40, online: true, alive: true, stale: false, ageMs: 160 },
        { id: "A3", slot: 3, team: "ally", type: "infantry", label: "步兵3", hp: 320, maxHp: 400, hpPct: 80, heatPct: 20, capPct: 90, ammoPct: 55, online: true, alive: true, stale: false, ageMs: 140 },
        { id: "A4", slot: 4, team: "ally", type: "infantry", label: "步兵4", hp: 0, maxHp: 400, hpPct: 0, heatPct: -1, capPct: -1, ammoPct: -1, online: false, alive: false, stale: true, ageMs: 999999 },
        { id: "A6", slot: 6, team: "ally", type: "aerial", label: "空中", hp: 210, maxHp: 300, hpPct: 70, heatPct: -1, capPct: -1, ammoPct: 80, online: true, alive: true, stale: false, ageMs: 210 },
        { id: "A7", slot: 7, team: "ally", type: "sentry", label: "哨兵", hp: 560, maxHp: 600, hpPct: 93, heatPct: 0, capPct: 100, ammoPct: 80, online: true, alive: true, stale: false, ageMs: 170 }
    ])

    property var enemyRobotList: ([
        { id: "E1", slot: 1, team: "enemy", type: "hero", label: "敌英雄", hp: 680, maxHp: 800, hpPct: 85, heatPct: -1, capPct: -1, ammoPct: -1, online: true, alive: true, stale: false, ageMs: 180 },
        { id: "E2", slot: 2, team: "enemy", type: "engineer", label: "敌工程", hp: 310, maxHp: 600, hpPct: 52, heatPct: -1, capPct: -1, ammoPct: -1, online: true, alive: true, stale: false, ageMs: 180 },
        { id: "E3", slot: 3, team: "enemy", type: "infantry", label: "敌步兵3", hp: 84, maxHp: 400, hpPct: 21, heatPct: -1, capPct: -1, ammoPct: -1, online: true, alive: true, stale: false, ageMs: 120 },
        { id: "E4", slot: 4, team: "enemy", type: "infantry", label: "敌步兵4", hp: 220, maxHp: 400, hpPct: 55, heatPct: -1, capPct: -1, ammoPct: -1, online: true, alive: true, stale: false, ageMs: 250 },
        { id: "E6", slot: 6, team: "enemy", type: "aerial", label: "敌空中", hp: 0, maxHp: 300, hpPct: 0, heatPct: -1, capPct: -1, ammoPct: -1, online: false, alive: false, stale: true, ageMs: 999999 },
        { id: "E7", slot: 7, team: "enemy", type: "sentry", label: "敌哨兵", hp: 520, maxHp: 600, hpPct: 87, heatPct: -1, capPct: -1, ammoPct: -1, online: true, alive: true, stale: false, ageMs: 240 }
    ])

    property var analysisMetrics: ([
        { key: "ally_economy", title: "我方总经济", value: 1280, status: "warn", source: "GlobalLogisticsStatus", confidence: 65, compareText: "敌方 1630" },
        { key: "enemy_economy", title: "敌方总经济", value: 1630, status: "warn", source: "GlobalLogisticsStatus", confidence: 65, compareText: "我方 1280" },
        { key: "ally_damage", title: "我方总伤害", value: 2560, status: "good", source: "RobotInjuryStat", confidence: 70, compareText: "敌方 1140" },
        { key: "enemy_damage", title: "敌方总伤害", value: 1140, status: "good", source: "RobotInjuryStat", confidence: 70, compareText: "我方 2560" },
        { key: "hp_diff", title: "总血量差", value: "+800", status: "good", source: "GlobalUnitStatus", confidence: 80, compareText: "我方领先 800" }
    ])

    property var cameraPreviewData: ({
        connected: false,
        fps: "--",
        latencyMs: 0,
        decodeMs: 0,
        renderMs: 0,
        grayFrameRate: 0,
        stall: false,
        sourceName: "CUSTOM CAM"
    })

    property var linkHealth: ({
        mqttStatus: "ok",
        mqttLatencyMs: 23,
        videoStatus: "unknown",
        videoLatencyMs: 0,
        radarStatus: "ok",
        radarAgeMs: 118,
        commandStatus: "disabled"
    })


    property var allyExecution: ([
        { id: "A1", label: "A1 H", capPct: 85, heatPct: 30, ammoPct: 70, lockedTarget: "E3", canFire: true },
        { id: "A2", label: "A2 E", capPct: 60, heatPct: 70, ammoPct: 40, lockedTarget: "-", canFire: false },
        { id: "A3", label: "A3 I3", capPct: 90, heatPct: 20, ammoPct: 55, lockedTarget: "E3", canFire: true },
        { id: "A7", label: "A7 S", capPct: 100, heatPct: 0, ammoPct: 80, lockedTarget: "-", canFire: false }
    ])

    property var predictionData: ({
        threatHistory: [0.30, 0.45, 0.55, 0.62, 0.70],
        threatTrend: "↗",
        threatDesc: "持续上升",
        economyHistory: [320, 350, 380, 410, 440],
        economyProjection: "+120",
        predictedEvents: [
            { time: "T+8s", text: "前哨站将被摧毁", color: "#FF4444" },
            { time: "T+15s", text: "大能量机关刷新", color: "#FFAA00" }
        ]
    })
}
