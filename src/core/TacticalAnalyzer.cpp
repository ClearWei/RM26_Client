#include "TacticalAnalyzer.h"
#include "ExecutionFusion.h"
#include "GameData.h"
#include "ThreatRanker.h"

#include <QDateTime>
#include <QDebug>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QtGlobal>

namespace RM {
namespace {

constexpr int kTacticalDefaultRobotMaxHp = 150;

QString tacticalRobotPrefix(bool isAlly) {
    return isAlly ? QStringLiteral("A") : QStringLiteral("E");
}

QString tacticalRobotIdLabel(int robotId, bool isAlly) {
    return QStringLiteral("%1%2")
        .arg(tacticalRobotPrefix(isAlly))
        .arg(robotId % 100);
}

QString tacticalRobotTypeIcon(RobotType type) {
    switch (type) {
    case RobotType::HERO:
        return QStringLiteral("hero");
    case RobotType::ENGINEER:
        return QStringLiteral("engineer");
    case RobotType::AERIAL:
        return QStringLiteral("aerial");
    case RobotType::SENTRY:
        return QStringLiteral("sentry");
    case RobotType::INFANTRY_3:
    case RobotType::INFANTRY_4:
    case RobotType::INFANTRY_5:
    default:
        return QStringLiteral("infantry");
    }
}

RobotType tacticalDefaultTypeForSlot(int slot) {
    switch (slot) {
    case 1:
        return RobotType::HERO;
    case 2:
        return RobotType::ENGINEER;
    case 6:
        return RobotType::AERIAL;
    case 7:
        return RobotType::SENTRY;
    case 3:
    case 4:
    case 5:
    default:
        return RobotType::INFANTRY_3;
    }
}

QString tacticalRobotTypeLabel(RobotType type, int slot) {
    switch (type) {
    case RobotType::HERO:
        return QStringLiteral("英雄");
    case RobotType::ENGINEER:
        return QStringLiteral("工程");
    case RobotType::AERIAL:
        return QStringLiteral("空中");
    case RobotType::SENTRY:
        return QStringLiteral("哨兵");
    case RobotType::INFANTRY_3:
    case RobotType::INFANTRY_4:
    case RobotType::INFANTRY_5:
    default:
        return QStringLiteral("步兵%1").arg(slot);
    }
}

//默认最大血量
int tacticalDefaultMaxHpForSlot(int slot) {
    Q_UNUSED(slot)
    return kTacticalDefaultRobotMaxHp;
}

qint64 robotLastUpdateMs(const RobotData &robot) {
    return robot.lastUpdateTime.isValid()
               ? robot.lastUpdateTime.toMSecsSinceEpoch()
               : qint64(0);
}

bool robotHasUsablePosition(const RobotData &robot) {
    return robot.posX != 0.0f || robot.posY != 0.0f;
}

int pct(int value, int maxValue) {
    if (maxValue <= 0) {
        return 0;
    }
    return qBound(0, qRound(value * 100.0 / maxValue), 100);
}

QString tacticalSignedNumber(int value) {
    return QStringLiteral("%1%2")
        .arg(value >= 0 ? QStringLiteral("+") : QString())
        .arg(value);
}

QString tacticalLeadText(int diff) {
    if (diff > 0) {
        return QStringLiteral("我方领先 %1").arg(diff);
    }
    if (diff < 0) {
        return QStringLiteral("敌方领先 %1").arg(qAbs(diff));
    }
    return QStringLiteral("双方持平");
}

QVariantMap tacticalMetric(const QString &key,
                           const QString &title,
                           const QVariant &value,
                           const QString &status,
                           const QString &source,
                           int confidence,
                           const QString &subText = QString()) {
    QVariantMap metric;
    metric["key"] = key;
    metric["title"] = title;
    metric["value"] = value;
    metric["status"] = status;
    metric["source"] = source;
    metric["confidence"] = qBound(0, confidence, 100);
    metric["compareText"] = subText;
    metric["subText"] = subText;
    return metric;
}

} // namespace

// ═══════════════════════════════════════════════════════════
// 构造 / 配置
// ═══════════════════════════════════════════════════════════

TacticalAnalyzer::TacticalAnalyzer(GameData *gameData, QObject *parent)
    : QObject(parent), m_gameData(gameData) {
    QObject::connect(&m_timer, &QTimer::timeout, this, &TacticalAnalyzer::analyze);
}

void TacticalAnalyzer::setConfig(const TacticalConfig &cfg) {
    QMutexLocker lock(&m_mutex);
    m_config = cfg;
    if (m_ranker) {
        m_ranker->setConfig(cfg);
    }
    if (m_freshness) {
        m_freshness->setThresholds(cfg.freshnessFreshMs,
                                   cfg.freshnessDegradedMs,
                                   cfg.freshnessStaleMs);
    }
    if (m_fusion) {
        m_fusion->setConfig(cfg);
    }
}

void TacticalAnalyzer::start(int intervalMs) {
    const int boundedIntervalMs = qBound(50, intervalMs, 1000);
    if (m_timer.isActive()) {
        m_timer.setInterval(boundedIntervalMs);
        return;
    }

    if (m_useMockData) {
        loadMockData();
        emit snapshotUpdated();
    } else {
        analyze();
    }
    m_timer.start(boundedIntervalMs);
}

void TacticalAnalyzer::stop() {
    m_timer.stop();
}

bool TacticalAnalyzer::isRunning() const {
    return m_timer.isActive();
}

void TacticalAnalyzer::setUseMockData(bool use) {
    {
        QMutexLocker lock(&m_mutex);
        if (m_useMockData == use) {
            return;
        }
        m_useMockData = use;
    }
    emit useMockDataChanged();
    analyze();
}

QString TacticalAnalyzer::layoutMode() const {
    QMutexLocker lock(&m_mutex);
    return m_snapshot.layoutMode;
}

void TacticalAnalyzer::setLayoutMode(const QString &mode) {
    const QString normalized =
        (mode == QStringLiteral("video_primary")) ? QStringLiteral("video_primary")
                                                  : QStringLiteral("map_primary");
    {
        QMutexLocker lock(&m_mutex);
        if (m_snapshot.layoutMode == normalized) {
            return;
        }
        m_snapshot.layoutMode = normalized;
    }
    emit layoutModeChanged();
    emit snapshotUpdated();
}

// ═══════════════════════════════════════════════════════════
// 主分析入口
// ═══════════════════════════════════════════════════════════

void TacticalAnalyzer::analyze() {
    if (m_useMockData) {
        analyzeMock();
    } else {
        analyzeReal();
    }
}

// ═══════════════════════════════════════════════════════════
// 模拟数据，与 TacticalMock.qml 的展示字段保持一致
// ═══════════════════════════════════════════════════════════

void TacticalAnalyzer::loadMockData() {
    QMutexLocker lock(&m_mutex);

    // 头部
    m_snapshot.header.matchMode = "正赛";
    m_snapshot.header.redScore = 3;
    m_snapshot.header.blueScore = 1;
    m_snapshot.header.currentRound = 4;
    m_snapshot.header.totalRounds = 5;
    m_snapshot.header.timeRemaining = "3:42";
    m_snapshot.header.stage = "BATTLE";
    m_snapshot.header.linkStatus = "链路 OK";
    m_snapshot.header.linkLatency = 23;

    // 资源
    m_snapshot.resources.allyBaseHp = 3200;
    m_snapshot.resources.allyBaseMax = 5000;
    m_snapshot.resources.enemyBaseHp = 4100;
    m_snapshot.resources.enemyBaseMax = 5000;
    m_snapshot.resources.allyOutpostHp = 420;
    m_snapshot.resources.allyOutpostMax = 1500;
    m_snapshot.resources.enemyOutpostHp = 780;
    m_snapshot.resources.enemyOutpostMax = 1500;
    m_snapshot.resources.allyOutpostDestroyed = false;
    m_snapshot.resources.enemyOutpostDestroyed = false;
    m_snapshot.resources.economyDiff = -350;
    m_snapshot.resources.damageDiff = 1420;
    m_snapshot.resources.hpDiff = 800;

    m_snapshot.topStatus.allyRemainingEconomy = 5000;
    m_snapshot.topStatus.allyTotalEconomyObtained = 5000;
    m_snapshot.topStatus.allyTechLevel = 0;
    m_snapshot.topStatus.allyEncryptionLevel = 0;
    m_snapshot.topStatus.allyFortressOccupationSec = 0;
    m_snapshot.topStatus.enemyFortressOccupationSec = 0;
    m_snapshot.topStatus.respawnGoldCost = 340;
    m_snapshot.topStatus.affordableRespawnCount = 14;
    m_snapshot.topStatus.respawnEconomyVisible = true;

    // 关键事件
    QVariantList events;
    {
        KeyEventItem ev;
        ev.time = "3:45"; ev.icon = "!";
        ev.text = "敌方前哨站被摧毁"; ev.color = "#17dd56"; ev.priority = "P1";
        events.append(toVariantMap(ev));
    }
    {
        KeyEventItem ev;
        ev.time = "3:45"; ev.icon = "★";
        ev.text = "大能量机关被激活"; ev.color = "#FFAA00"; ev.priority = "P0";
        events.append(toVariantMap(ev));
    }
    {
        KeyEventItem ev;
        ev.time = "3:15"; ev.icon = "⚠";
        ev.text = "敌方英雄狙击伤害 450"; ev.color = "#d52424"; ev.priority = "P2";
        events.append(toVariantMap(ev));
    }
    m_snapshot.keyEvents = events;

    // 图传覆盖
    m_snapshot.video.targetId = "E3";
    m_snapshot.video.targetLabel = "敌 3号步兵";
    m_snapshot.video.targetHp = 84;
    m_snapshot.video.targetMaxHp = 400;
    m_snapshot.video.distance = 15.2;
    m_snapshot.video.lockQuality = 0.92;
    m_snapshot.video.hasHologram = true;
    m_snapshot.video.compensationText = "瞄准: ↓2.3";

    // 主决策
    m_snapshot.decision.title = "集火击杀敌方3号步兵";
    m_snapshot.decision.priority = "P1";
    m_snapshot.decision.confidence = 87;
    m_snapshot.decision.windowText = "窗口 ~8秒";
    m_snapshot.decision.reasons = QStringList{
        "残血 84/400", "雷达易伤激活", "A3可支援", "基地安全"
    };
    m_snapshot.decision.fallbackActions = QStringList{
        "转火敌方哨兵", "控图保热"
    };

    // 目标排序
    QVariantList targets;
    {
        TargetRankItem t;
        t.rank = 1; t.id = "E3"; t.label = "敌步兵3";
        t.hp = 84; t.maxHp = 400; t.threat = 0.92; t.icon = "infantry";
        targets.append(toVariantMap(t));
    }
    {
        TargetRankItem t;
        t.rank = 2; t.id = "E2"; t.label = "敌工程2";
        t.hp = 310; t.maxHp = 600; t.threat = 0.65; t.icon = "engineer";
        targets.append(toVariantMap(t));
    }
    {
        TargetRankItem t;
        t.rank = 3; t.id = "E4"; t.label = "敌步兵4";
        t.hp = 220; t.maxHp = 400; t.threat = 0.58; t.icon = "infantry";
        targets.append(toVariantMap(t));
    }
    {
        TargetRankItem t;
        t.rank = 4; t.id = "E1"; t.label = "敌英雄1";
        t.hp = 680; t.maxHp = 800; t.threat = 0.45; t.icon = "hero";
        targets.append(toVariantMap(t));
    }
    m_snapshot.topTargets = targets;

    // 雷达地图
    m_snapshot.radar.radarAgeMs = 118;
    m_snapshot.radar.focusTargetId = "E3";
    m_snapshot.radar.mapImageSource = "qrc:/images/minimap_bg.png";

    {
        QVariantList ally;
        ally.append(toVariantMap(MapRobot{"A1", "A1", 0.25, 0.55, 45}));
        ally.append(toVariantMap(MapRobot{"A2", "A2", 0.32, 0.62, 90}));
        ally.append(toVariantMap(MapRobot{"A3", "A3", 0.28, 0.48, 30}));
        ally.append(toVariantMap(MapRobot{"A4", "A4", 0.22, 0.42, 0}));
        ally.append(toVariantMap(MapRobot{"A6", "A6", 0.38, 0.55, 135}));
        ally.append(toVariantMap(MapRobot{"A7", "A7", 0.42, 0.70, 180}));
        m_snapshot.radar.allyRobots = ally;
    }
    {
        QVariantList enemy;
        enemy.append(toVariantMap(MapRobot{"E1", "E1", 0.60, 0.55, 225}));
        enemy.append(toVariantMap(MapRobot{"E2", "E2", 0.68, 0.42, 270}));
        enemy.append(toVariantMap(MapRobot{"E3", "E3", 0.55, 0.38, 200}));
        enemy.append(toVariantMap(MapRobot{"E4", "E4", 0.72, 0.60, 240}));
        m_snapshot.radar.enemyRobots = enemy;
    }

    {
        QVariantList routes;
        QVariantMap rt;
        rt["fromX"] = 0.28; rt["fromY"] = 0.48;
        rt["toX"] = 0.55; rt["toY"] = 0.38;
        routes.append(rt);
        m_snapshot.radar.routes = routes;
    }
    {
        QVariantList dzs;
        QVariantMap dz;
        dz["centerX"] = 0.65; dz["centerY"] = 0.50; dz["radius"] = 0.08;
        dzs.append(dz);
        m_snapshot.radar.dangerZones = dzs;
    }
    {
        QVariantList buffs;
        QVariantMap bz;
        bz["x"] = 0.45; bz["y"] = 0.35;
        buffs.append(bz);
        m_snapshot.radar.buffZones = buffs;
    }

    // 我方执行能力
    QVariantList execs;
    {
        AllyExecutionCard c;
        c.id = "A1"; c.label = "A1 H"; c.capPct = 85; c.heatPct = 30;
        c.ammoPct = 70; c.lockedTarget = "E3"; c.canFire = true;
        execs.append(toVariantMap(c));
    }
    {
        AllyExecutionCard c;
        c.id = "A2"; c.label = "A2 E"; c.capPct = 60; c.heatPct = 70;
        c.ammoPct = 40; c.lockedTarget = "-"; c.canFire = false;
        execs.append(toVariantMap(c));
    }
    {
        AllyExecutionCard c;
        c.id = "A3"; c.label = "A3 I3"; c.capPct = 90; c.heatPct = 20;
        c.ammoPct = 55; c.lockedTarget = "E3"; c.canFire = true;
        execs.append(toVariantMap(c));
    }
    {
        AllyExecutionCard c;
        c.id = "A7"; c.label = "A7 S"; c.capPct = 100; c.heatPct = 0;
        c.ammoPct = 80; c.lockedTarget = "-"; c.canFire = false;
        execs.append(toVariantMap(c));
    }
    m_snapshot.allyExecution = execs;

    // 2026 阵容没有 5 号步兵，列表保留 1、2、3、4、6、7 号位。
    m_snapshot.allyRobotList = QVariantList{
        QVariantMap{{"id", "A1"}, {"slot", 1}, {"team", "ally"}, {"type", "hero"}, {"label", "英雄"}, {"hp", 2580}, {"maxHp", 4000}, {"hpPct", 65}, {"heatPct", 30}, {"capPct", 85}, {"ammoPct", 70}, {"online", true}, {"alive", true}, {"stale", false}, {"ageMs", 120}},
        QVariantMap{{"id", "A2"}, {"slot", 2}, {"team", "ally"}, {"type", "engineer"}, {"label", "工程"}, {"hp", 420}, {"maxHp", 500}, {"hpPct", 84}, {"heatPct", -1}, {"capPct", 60}, {"ammoPct", 40}, {"online", true}, {"alive", true}, {"stale", false}, {"ageMs", 160}},
        QVariantMap{{"id", "A3"}, {"slot", 3}, {"team", "ally"}, {"type", "infantry"}, {"label", "步兵3"}, {"hp", 320}, {"maxHp", 400}, {"hpPct", 80}, {"heatPct", 20}, {"capPct", 90}, {"ammoPct", 55}, {"online", true}, {"alive", true}, {"stale", false}, {"ageMs", 140}},
        QVariantMap{{"id", "A4"}, {"slot", 4}, {"team", "ally"}, {"type", "infantry"}, {"label", "步兵4"}, {"hp", 0}, {"maxHp", 400}, {"hpPct", 0}, {"heatPct", -1}, {"capPct", -1}, {"ammoPct", -1}, {"online", false}, {"alive", false}, {"stale", true}, {"ageMs", 999999}},
        QVariantMap{{"id", "A6"}, {"slot", 6}, {"team", "ally"}, {"type", "aerial"}, {"label", "空中"}, {"hp", 210}, {"maxHp", 300}, {"hpPct", 70}, {"heatPct", -1}, {"capPct", -1}, {"ammoPct", 80}, {"online", true}, {"alive", true}, {"stale", false}, {"ageMs", 210}},
        QVariantMap{{"id", "A7"}, {"slot", 7}, {"team", "ally"}, {"type", "sentry"}, {"label", "哨兵"}, {"hp", 560}, {"maxHp", 600}, {"hpPct", 93}, {"heatPct", 0}, {"capPct", 100}, {"ammoPct", 80}, {"online", true}, {"alive", true}, {"stale", false}, {"ageMs", 170}}
    };
    m_snapshot.enemyRobotList = QVariantList{
        QVariantMap{{"id", "E1"}, {"slot", 1}, {"team", "enemy"}, {"type", "hero"}, {"label", "敌英雄"}, {"hp", 680}, {"maxHp", 800}, {"hpPct", 85}, {"heatPct", -1}, {"capPct", -1}, {"ammoPct", -1}, {"online", true}, {"alive", true}, {"stale", false}, {"ageMs", 180}},
        QVariantMap{{"id", "E2"}, {"slot", 2}, {"team", "enemy"}, {"type", "engineer"}, {"label", "敌工程"}, {"hp", 310}, {"maxHp", 600}, {"hpPct", 52}, {"heatPct", -1}, {"capPct", -1}, {"ammoPct", -1}, {"online", true}, {"alive", true}, {"stale", false}, {"ageMs", 180}},
        QVariantMap{{"id", "E3"}, {"slot", 3}, {"team", "enemy"}, {"type", "infantry"}, {"label", "敌步兵3"}, {"hp", 84}, {"maxHp", 400}, {"hpPct", 21}, {"heatPct", -1}, {"capPct", -1}, {"ammoPct", -1}, {"online", true}, {"alive", true}, {"stale", false}, {"ageMs", 120}},
        QVariantMap{{"id", "E4"}, {"slot", 4}, {"team", "enemy"}, {"type", "infantry"}, {"label", "敌步兵4"}, {"hp", 220}, {"maxHp", 400}, {"hpPct", 55}, {"heatPct", -1}, {"capPct", -1}, {"ammoPct", -1}, {"online", true}, {"alive", true}, {"stale", false}, {"ageMs", 250}},
        QVariantMap{{"id", "E6"}, {"slot", 6}, {"team", "enemy"}, {"type", "aerial"}, {"label", "敌空中"}, {"hp", 0}, {"maxHp", 300}, {"hpPct", 0}, {"heatPct", -1}, {"capPct", -1}, {"ammoPct", -1}, {"online", false}, {"alive", false}, {"stale", true}, {"ageMs", 999999}},
        QVariantMap{{"id", "E7"}, {"slot", 7}, {"team", "enemy"}, {"type", "sentry"}, {"label", "敌哨兵"}, {"hp", 520}, {"maxHp", 600}, {"hpPct", 87}, {"heatPct", -1}, {"capPct", -1}, {"ammoPct", -1}, {"online", true}, {"alive", true}, {"stale", false}, {"ageMs", 240}}
    };
    const int allyEconomy = 1280;
    const int enemyEconomy = 1630;
    const int allyDamage = 2560;
    const int enemyDamage = 1140;

    m_snapshot.analysisMetrics = QVariantList{
        tacticalMetric("ally_economy", "我方总经济", allyEconomy,
                       allyEconomy >= enemyEconomy ? "good" : "warn",
                       "GlobalLogisticsStatus", 65,
                       QStringLiteral("敌方 %1").arg(enemyEconomy)),
        tacticalMetric("enemy_economy", "敌方总经济", enemyEconomy,
                       enemyEconomy > allyEconomy ? "warn" : "good",
                       "GlobalLogisticsStatus", 65,
                       QStringLiteral("我方 %1").arg(allyEconomy)),
        tacticalMetric("ally_damage", "我方总伤害", allyDamage,
                       allyDamage >= enemyDamage ? "good" : "warn",
                       "RobotInjuryStat", 70,
                       QStringLiteral("敌方 %1").arg(enemyDamage)),
        tacticalMetric("enemy_damage", "敌方总伤害", enemyDamage,
                       enemyDamage > allyDamage ? "warn" : "good",
                       "RobotInjuryStat", 70,
                       QStringLiteral("我方 %1").arg(allyDamage)),
        tacticalMetric("hp_diff", "总血量差", "+800", "good", "GlobalUnitStatus", 80,
                       QStringLiteral("我方领先 800"))
    };
    m_snapshot.cameraPreviewData = QVariantMap{
        {"connected", false},
        {"fps", "--"},
        {"latencyMs", 0},
        {"decodeMs", 0},
        {"renderMs", 0},
        {"grayFrameRate", 0},
        {"stall", false},
        {"sourceName", "0x0310 H264"}
    };
    m_snapshot.linkHealth = QVariantMap{
        {"mqttStatus", "ok"},
        {"mqttLatencyMs", 23},
        {"videoStatus", "unknown"},
        {"videoLatencyMs", 0},
        {"radarStatus", "ok"},
        {"radarAgeMs", 118},
        {"commandStatus", "disabled"}
    };

    // 趋势预测
    m_snapshot.predictions.threatHistory = {0.30, 0.45, 0.55, 0.62, 0.70};
    m_snapshot.predictions.economyHistory = {320, 350, 380, 410, 440};
    m_snapshot.predictions.threatTrend = "↗";
    m_snapshot.predictions.threatDesc = "持续上升";
    m_snapshot.predictions.economyProjection = "+120";
    {
        QVariantList predEvents;
        QVariantMap pe1;
        pe1["time"] = "T+8s"; pe1["text"] = "前哨站将被摧毁"; pe1["color"] = "#FF4444";
        predEvents.append(pe1);
        QVariantMap pe2;
        pe2["time"] = "T+15s"; pe2["text"] = "大能量机关刷新"; pe2["color"] = "#FFAA00";
        predEvents.append(pe2);
        m_snapshot.predictions.predictedEvents = predEvents;
    }

    m_snapshot.snapshotTimestamp = QDateTime::currentMSecsSinceEpoch();
}

void TacticalAnalyzer::analyzeMock() {
    bool needsInitialSnapshot = false;
    {
        QMutexLocker lock(&m_mutex);
        needsInitialSnapshot = m_snapshot.snapshotTimestamp <= 0;
    }

    // 手动调用 analyze() 时也应得到完整快照，不能依赖 start() 预先加载。
    if (needsInitialSnapshot) {
        loadMockData();
    } else {
        QMutexLocker lock(&m_mutex);
        m_snapshot.snapshotTimestamp = QDateTime::currentMSecsSinceEpoch();
    }
    // 信号必须在解锁后发出，避免 QML 同步回调死锁。
    emit snapshotUpdated();
}

void TacticalAnalyzer::analyzeReal() {
    // 从 GameData 读取真实数据，并交给 ThreatRanker 计算威胁排序。
    if (!m_gameData) {
        emit snapshotUpdated();
        return;
    }

    {
        QMutexLocker lock(&m_mutex);
        updateHeader();
        updateResources();
        updateTopStatus();
        updateKeyEvents();
        updateRobotLists();
        updateAllyExecution();
        updateTopTargets();
        updateVideoOverlay();
        updateRadarData();
        updateAnalysisMetrics();
        updateCameraPreviewData();
        updateLinkHealth();
        updateMainDecision();
        updatePredictions();
        m_snapshot.snapshotTimestamp = QDateTime::currentMSecsSinceEpoch();
    }
    // 信号在解锁后发出，避免同步回调再次取锁。
    emit snapshotUpdated();
}

// ═══════════════════════════════════════════════════════════
// 各面板 Real 数据更新
// ═══════════════════════════════════════════════════════════

void TacticalAnalyzer::updateHeader() {
    const auto &gs = m_gameData->getGameState();
    auto &h = m_snapshot.header;

    h.matchMode = "正赛";
    h.redScore = gs.redScore;
    h.blueScore = gs.blueScore;
    h.currentRound = gs.currentRound;
    h.totalRounds = 5;
    h.stage = (gs.gameProgress == GameStage::PREPARATION) ? "准备"
            : (gs.gameProgress == GameStage::BATTLE) ? "BATTLE"
            : "其他";

    int secs = gs.gameTime;
    h.timeRemaining = QString("%1:%2")
        .arg(secs / 60).arg(secs % 60, 2, 10, QChar('0'));

    h.linkStatus = m_useMockData ? "Mock" : "链路 OK";
    h.linkLatency = 0;
}

void TacticalAnalyzer::updateResources() {
    auto &r = m_snapshot.resources;
    const bool redPerspective = m_gameData->currentRobotId() < 100;

    r.allyBaseHp = redPerspective ? m_gameData->redBaseHealth()
                                  : m_gameData->blueBaseHealth();
    r.allyBaseMax = redPerspective ? m_gameData->redBaseMaxHealth()
                                   : m_gameData->blueBaseMaxHealth();
    r.enemyBaseHp = redPerspective ? m_gameData->blueBaseHealth()
                                   : m_gameData->redBaseHealth();
    r.enemyBaseMax = redPerspective ? m_gameData->blueBaseMaxHealth()
                                    : m_gameData->redBaseMaxHealth();

    r.allyOutpostHp = redPerspective ? m_gameData->redOutpostHealth()
                                     : m_gameData->blueOutpostHealth();
    r.allyOutpostMax = redPerspective ? m_gameData->redOutpostMaxHealth()
                                      : m_gameData->blueOutpostMaxHealth();
    r.enemyOutpostHp = redPerspective ? m_gameData->blueOutpostHealth()
                                      : m_gameData->redOutpostHealth();
    r.enemyOutpostMax = redPerspective ? m_gameData->blueOutpostMaxHealth()
                                       : m_gameData->redOutpostMaxHealth();

    r.allyOutpostDestroyed = redPerspective ? m_gameData->redOutpostDestroyed()
                                            : m_gameData->blueOutpostDestroyed();
    r.enemyOutpostDestroyed = redPerspective ? m_gameData->blueOutpostDestroyed()
                                             : m_gameData->redOutpostDestroyed();
    r.allyBaseInvincible = redPerspective ? m_gameData->redBaseInvincible()
                                          : m_gameData->blueBaseInvincible();
    r.enemyBaseInvincible = redPerspective ? m_gameData->blueBaseInvincible()
                                           : m_gameData->redBaseInvincible();
    r.allyDefenseBonus = redPerspective ? m_gameData->redDefenseBonus()
                                        : m_gameData->blueDefenseBonus();
    r.enemyDefenseBonus = redPerspective ? m_gameData->blueDefenseBonus()
                                         : m_gameData->redDefenseBonus();

    int redEco = m_gameData->redEconomy();
    int blueEco = m_gameData->blueEconomy();
    r.economyDiff = redPerspective ? redEco - blueEco : blueEco - redEco;

    r.damageDiff = redPerspective
                       ? m_gameData->redTotalDamage() - m_gameData->blueTotalDamage()
                       : m_gameData->blueTotalDamage() - m_gameData->redTotalDamage();
    r.hpDiff = redPerspective
                   ? m_gameData->redRobotTotalHP() - m_gameData->blueRobotTotalHP()
                   : m_gameData->blueRobotTotalHP() - m_gameData->redRobotTotalHP();
}

void TacticalAnalyzer::updateTopStatus() {
    auto &t = m_snapshot.topStatus;
    const bool redPerspective = m_gameData->currentRobotId() < 100;

    t.allyRemainingEconomy = redPerspective
        ? static_cast<int>(m_gameData->redRemainingEconomy())
        : static_cast<int>(m_gameData->blueRemainingEconomy());
    t.allyTotalEconomyObtained = redPerspective
        ? static_cast<qulonglong>(m_gameData->redTotalEconomyObtained())
        : static_cast<qulonglong>(m_gameData->blueTotalEconomyObtained());
    t.allyTechLevel = redPerspective
        ? static_cast<int>(m_gameData->redTechLevel())
        : static_cast<int>(m_gameData->blueTechLevel());
    t.allyEncryptionLevel = redPerspective
        ? static_cast<int>(m_gameData->redEncryptionLevel())
        : static_cast<int>(m_gameData->blueEncryptionLevel());
    t.allyFortressOccupationSec = m_gameData->allyFortressOccupationSec();
    t.enemyFortressOccupationSec = m_gameData->enemyFortressOccupationSec();

    const GameStage stage = m_gameData->getCurrentStage();
    const bool battleActive = stage == GameStage::BATTLE &&
                              !m_gameData->is_paused();
    if (battleActive) {
        const int remainingTime =
            qBound(0, static_cast<int>(m_gameData->getGameTime()), 420);
        const int elapsedMinutes = (420 - remainingTime + 59) / 60;
        const int robotLevel = qMax(1, m_gameData->robotLevel());
        t.respawnGoldCost = elapsedMinutes * 80 + robotLevel * 20;
        t.affordableRespawnCount =
            qMax(0, m_gameData->currentTeamEconomy()) / t.respawnGoldCost;
    } else {
        t.respawnGoldCost = 0;
        t.affordableRespawnCount = 0;
    }
    t.respawnEconomyVisible =
        battleActive || stage == GameStage::NOT_STARTED ||
        stage == GameStage::PREPARATION || stage == GameStage::SELF_CHECK ||
        stage == GameStage::COUNTDOWN;
}

void TacticalAnalyzer::updateKeyEvents() {
    // 直接复用左侧系统消息链路中的结构化消息项（text/color），
    QVariantList events;
    QVariantList rawMsgs = m_gameData->getSystemMessages();
    static const QRegularExpression kTimestampPrefix(
        QStringLiteral("^(\\d+:\\d{2})\\s+(.*)$"));
    auto shouldHideFromTactical = [](const QString &message) {
        return message.contains(QStringLiteral("模块离线")) ||
               message.contains(QStringLiteral("黄牌已累计")) ||
               message.contains(QStringLiteral("未选择底盘性能")) ||
               message.contains(QStringLiteral("未选择发射机构类型"));
    };

    //从最后一条消息开始取
    for (int i = rawMsgs.size() - 1; i >= 0; --i) {
        const QVariantMap msgItem = rawMsgs[i].toMap();
        QString fullText = msgItem.value(QStringLiteral("text")).toString();
        QString color = msgItem.value(QStringLiteral("color")).toString();
        QString timeText;
        QString bodyText = fullText;

        //提取时间文字和文本文字
        const QRegularExpressionMatch match = kTimestampPrefix.match(fullText);
        if (match.hasMatch()) {
            timeText = match.captured(1);
            bodyText = match.captured(2);
        }

        if (shouldHideFromTactical(bodyText)) {
            continue;
        }

        //如果没有自带时间，则用当前游戏时间补上
        if (timeText.isEmpty()) {
            const auto &gs = m_gameData->getGameState();
            const int secs = gs.gameTime;
            timeText = QStringLiteral("%1:%2")
                           .arg(secs / 60)
                           .arg(secs % 60, 2, 10, QChar('0'));
        }

        KeyEventItem ev;
        ev.text = bodyText;
        ev.color = color.isEmpty() ? QStringLiteral("#AABBCC") : color;
        ev.time = timeText;
        events.append(toVariantMap(ev));
        if (events.size() >= m_config.maxKeyEvents) {
            break;
        }
    }
    m_snapshot.keyEvents = events;
}

void TacticalAnalyzer::updateVideoOverlay() {
    // 使用当前操控机器人信息填充图传覆盖
    int myId = m_gameData->currentRobotId();
    QVariantMap myRobot = m_gameData->getMyRobot();
    auto &v = m_snapshot.video;

    v.targetId = QString("R%1").arg(myId);
    v.targetLabel = "当前操控";
    v.targetHp = myRobot.value("hp", 0).toInt();
    v.targetMaxHp = myRobot.value("maxHp", kTacticalDefaultRobotMaxHp).toInt();
    v.distance = 0.0;
    v.lockQuality = 0.5;
    v.hasHologram = false;
    v.compensationText = "";

    // 如果有敌方主目标，优先显示
    if (m_ranker) {
        // 取 TOP1 目标
        if (!m_snapshot.topTargets.isEmpty()) {
            QVariantMap top = m_snapshot.topTargets.first().toMap();
            v.targetId = top.value("id", "?").toString();
            v.targetLabel = top.value("label", "目标").toString();
            v.targetHp = top.value("hp", 0).toInt();
            v.targetMaxHp = top.value("maxHp", kTacticalDefaultRobotMaxHp).toInt();
            v.hasHologram = true;
        }
    }
}

void TacticalAnalyzer::updateMainDecision() {
    auto &d = m_snapshot.decision;
    const auto &gs = m_gameData->getGameState();

    // 默认背景观察
    d.title = "态势观察";
    d.priority = "P3";
    d.confidence = 50;
    d.windowText = "";
    d.reasons = QStringList{"等待比赛数据..."};
    d.fallbackActions = QStringList{};

    if (!m_ranker || gs.gameProgress != GameStage::BATTLE) return;

    // 收集环境数据用于决策
    const bool redPerspective = m_gameData->currentRobotId() < 100;
    int allyBaseHp = redPerspective ? m_gameData->redBaseHealth()
                                    : m_gameData->blueBaseHealth();
    int allyBaseMax = redPerspective ? m_gameData->redBaseMaxHealth()
                                     : m_gameData->blueBaseMaxHealth();
    double allyBaseRatio = (allyBaseMax > 0)
        ? static_cast<double>(allyBaseHp) / allyBaseMax : 1.0;

    int enemyOutpostHp = redPerspective ? m_gameData->blueOutpostHealth()
                                        : m_gameData->redOutpostHealth();
    int enemyOutpostMax = redPerspective ? m_gameData->blueOutpostMaxHealth()
                                         : m_gameData->redOutpostMaxHealth();
    double enemyOutpostRatio = (enemyOutpostMax > 0)
        ? static_cast<double>(enemyOutpostHp) / enemyOutpostMax : 1.0;

    // 统计可射击的己方数量
    int fireReady = 0;
    int ammoLow = 0;
    for (const auto &card : m_snapshot.allyExecution) {
        QVariantMap c = card.toMap();
        if (c.value("canFire", false).toBool()) fireReady++;
        if (c.value("ammoPct", 100).toInt() < m_config.ammoLowRatio * 100) ammoLow++;
    }

    bool decisionSelected = false;

    // P0: 基地紧急防守
    if (allyBaseRatio < m_config.baseDefendHpRatio) {
        d.title = "回防基地";
        d.priority = "P0";
        d.confidence = 85;
        d.windowText = "立即";
        d.reasons = QStringList{
            QString("己方基地血量 %1%").arg(qRound(allyBaseRatio * 100)),
            "紧急防守最高优先级"
        };
        d.fallbackActions = QStringList{"撤退", "呼叫支援"};
        decisionSelected = true;
    }
    // P1: 推进前哨
    else if (enemyOutpostRatio < m_config.outpostPushHpRatio
             && fireReady >= m_config.fireReadyMin) {
        d.title = "推进敌方前哨站";
        d.priority = "P1";
        d.confidence = 70;
        d.windowText = QString("敌哨残血 %1%").arg(qRound(enemyOutpostRatio * 100));
        d.reasons = QStringList{
            QString("敌方前哨站残血 %1%").arg(qRound(enemyOutpostRatio * 100)),
            QString("己方火力 %1 台就绪").arg(fireReady)
        };
        d.fallbackActions = QStringList{"集火目标", "控图保热"};
        decisionSelected = true;
    }
    // P1: 击杀窗口 — 由 ThreatRanker 评分驱动
    else if (!m_snapshot.topTargets.isEmpty()) {
        QVariantMap top = m_snapshot.topTargets.first().toMap();
        int hp = top.value("hp", 0).toInt();
        int maxHp = top.value("maxHp", kTacticalDefaultRobotMaxHp).toInt();
        double hpRatio = maxHp > 0 ? static_cast<double>(hp) / maxHp : 1.0;

        if (hpRatio < m_config.killWindowHpRatio && fireReady >= m_config.fireReadyMin) {
            d.title = QString("集火击杀 %1").arg(top.value("label").toString());
            d.priority = "P1";
            d.confidence = qMin(90, qRound(top.value("threat", 0.5).toDouble() * 100));
            d.windowText = QString("残血 %1/%2").arg(hp).arg(maxHp);
            d.reasons = QStringList{
                QString("残血 %1/%2").arg(hp).arg(maxHp),
                QString("威胁评分 %1").arg(top.value("threat", 0).toDouble(), 0, 'f', 2),
                QString("火力就绪 %1 台").arg(fireReady)
            };
            d.fallbackActions = QStringList{"转火哨兵", "经济补给"};
            decisionSelected = true;
        }
    }

    // P2: 经济补给
    if (!decisionSelected && ammoLow > 0) {
        d.title = "控图补给";
        d.priority = "P2";
        d.confidence = 60;
        d.windowText = QString("%1 台弹量低").arg(ammoLow);
        d.reasons = QStringList{
            QString("%1 台机器人弹量不足 %2%").arg(ammoLow).arg(qRound(m_config.ammoLowRatio * 100)),
            "建议回撤补给"
        };
        d.fallbackActions = QStringList{"控图保热", "等待窗口"};
        decisionSelected = true;
    }

    // 防抖: 首次决策直接采用, 后续切换需确认帧数 + 时间 + 分数差
    bool firstDecision = m_lastDecision.title.isEmpty();
    if (!firstDecision && m_lastDecision.title != d.title) {
        m_decisionFrameCount++;
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (m_decisionFrameCount < m_config.decisionConfirmFrames
            && (now - m_lastDecisionTime) < m_config.decisionHoldMs) {
            d = m_lastDecision; // 保持旧决策
            return;
        }
    }
    m_lastDecision = d;
    m_decisionFrameCount = 0;
    m_lastDecisionTime = QDateTime::currentMSecsSinceEpoch();
}

void TacticalAnalyzer::updateTopTargets() {
    if (!m_ranker || !m_freshness) return;

    const auto &allRobots = m_gameData->getAllRobots();
    int myId = m_gameData->currentRobotId();
    bool isRed = (myId < 100);

    // 分离己方/敌方机器人
    QMap<int, QVariantMap> allyRobots, enemyRobots;
    for (const auto &robot : allRobots) {
        QVariantMap info = m_gameData->getRobotInfo(robot.robotId);
        if (info.isEmpty()) continue;
        const bool isAlly = (robot.robotId < 100) == isRed;
        info["label"] = tacticalRobotIdLabel(robot.robotId, isAlly);
        info["icon"] = tacticalRobotTypeIcon(robot.type);
        info["x"] = robot.posX;
        info["y"] = robot.posY;
        info["angle"] = robot.angle;
        info["type"] = static_cast<int>(robot.type);
        info["isHighLight"] = robot.isHighLight > 0;
        info["hasVulnerability"] = robot.buffs.defenseOrVulnerability.level < 0;

        if (m_freshness) {
            const qint64 lastUpdateMs = robotLastUpdateMs(robot);
            if (lastUpdateMs > 0) {
                m_freshness->touch(QStringLiteral("enemy_pos_%1").arg(robot.robotId),
                                   lastUpdateMs);
            }
        }

        if (isAlly) {
            allyRobots[robot.robotId] = info;
        } else {
            enemyRobots[robot.robotId] = info;
        }
    }

    const bool redPerspective = m_gameData->currentRobotId() < 100;
    int allyBaseHp = redPerspective ? m_gameData->redBaseHealth()
                                    : m_gameData->blueBaseHealth();

    // 调用 ThreatRanker
    auto scores = m_ranker->rankTargets(allyRobots, enemyRobots,
                                        allyBaseHp, *m_freshness);

    // 转换为 TargetRankItem
    QVariantList items;
    int maxCount = qMin(scores.size(), m_config.maxTopTargets);
    for (int i = 0; i < maxCount; ++i) {
        const auto &s = scores[i];
        TargetRankItem item;
        item.rank = i + 1;
        item.id = tacticalRobotIdLabel(s.robotId, false);
        item.label = s.label.isEmpty() ? item.id : s.label;
        item.icon = s.icon;
        item.hp = s.hp;
        item.maxHp = s.maxHp;
        item.threat = s.totalScore;
        items.append(toVariantMap(item));
    }
    m_snapshot.topTargets = items;
}

void TacticalAnalyzer::updateRadarData() {
    auto &rd = m_snapshot.radar;
    rd.mapImageSource = "qrc:/images/minimap_bg.png";
    const auto &allRobots = m_gameData->getAllRobots();
    int myId = m_gameData->currentRobotId();
    bool isRed = (myId < 100);

    QVariantList ally, enemy;
    qint64 newestRadarUpdateMs = 0;
    for (const auto &robot : allRobots) {
        // 只接入有位置数据的机器人
        if (!robotHasUsablePosition(robot)) continue;

        MapRobot mr;
        bool isAlly = (robot.robotId < 100) == isRed;
        mr.id = tacticalRobotIdLabel(robot.robotId, isAlly);
        mr.label = mr.id;

        //坐标归一化
        QPointF normalized = m_mapper
            ? m_mapper->battlefieldToNormalized(robot.posX, robot.posY)
            : QPointF(static_cast<double>(robot.posX) / m_config.battlefieldWidth,
                      static_cast<double>(robot.posY) / m_config.battlefieldHeight);
        mr.x = qBound(0.0, normalized.x(), 1.0);
        mr.y = qBound(0.0, normalized.y(), 1.0);
        mr.angle = m_mapper ? m_mapper->yawToCanvasAngle(robot.angle) : robot.angle;

        const qint64 updateMs = robotLastUpdateMs(robot);
        newestRadarUpdateMs = qMax(newestRadarUpdateMs, updateMs);

        QVariantMap map = toVariantMap(mr);
        map["ageMs"] = updateMs > 0
                           ? QDateTime::currentMSecsSinceEpoch() - updateMs
                           : 999999;
        map["number"] = robot.robotId % 100;
        map["team"] = isAlly ? QStringLiteral("ally") : QStringLiteral("enemy");
        map["visible"] = true;
        map["predicted"] = false;
        map["stale"] = map.value("ageMs").toInt() > m_config.freshnessStaleMs;
        map["hpPct"] = pct(robot.currentHP, robot.maxHP);
        map["focus"] = false;
        if (isAlly) ally.append(map);
        else enemy.append(map);
    }

    rd.allyRobots = ally;
    rd.enemyRobots = enemy;

    // 主目标：取当前威胁排名第一的敌方
    if (!m_snapshot.topTargets.isEmpty()) {
        rd.focusTargetId = m_snapshot.topTargets.first().toMap()
            .value("id", "").toString();
    }

    rd.radarAgeMs = newestRadarUpdateMs > 0
        ? static_cast<int>(QDateTime::currentMSecsSinceEpoch() - newestRadarUpdateMs)
        : 999999;

    if (!rd.focusTargetId.isEmpty()) {
        auto markFocus = [&](QVariantList &robots) {
            for (QVariant &entry : robots) {
                QVariantMap map = entry.toMap();
                map["focus"] = (map.value("id").toString() == rd.focusTargetId);
                entry = map;
            }
        };
        markFocus(rd.enemyRobots);
    }
}

void TacticalAnalyzer::updateRobotLists() {
    const bool redPerspective = m_gameData->currentRobotId() < 100;
    const int currentRound = m_gameData->currentRound();
    if (currentRound > 0 && m_robotFlashStateRound != currentRound) {
        m_robotHadPositiveHpById.clear();
        m_enemyWasZeroAfterPositiveHpById.clear();
        m_enemyReviveFlashUntilMsById.clear();
        m_robotFlashStateRound = currentRound;
    }
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const QList<int> visibleSlots{1, 2, 3, 4, 6, 7};

    auto buildList = [&](bool redTeam, bool ally) {
        QVariantList list;
        for (int slot : visibleSlots) {
            const int robotId = redTeam ? slot : 100 + slot;
            const RobotData *robot = m_gameData->getRobotById(static_cast<quint8>(robotId));
            const RobotType type = robot ? robot->type : tacticalDefaultTypeForSlot(slot);
            const qint64 lastMs = robot ? robotLastUpdateMs(*robot) : 0;
            const int ageMs = lastMs > 0 ? static_cast<int>(nowMs - lastMs) : 999999;
            const bool onlineByStatic = robot && robot->tabStaticConnected;
            const bool onlineByGlobalHp =
                robot && robot->hasTabGlobalSnapshot && robot->tabGlobalCurrentHP > 0;
            const bool online = robot && (onlineByStatic || onlineByGlobalHp);
            // 战术血量以 GlobalUnitStatus 为准。0 表示已被击毁，即使机器人仍在线，
            // 也不能回退到可能过期的 RobotStaticStatus/currentHP。
            const bool useGlobalHp = robot && robot->hasTabGlobalSnapshot;
            const int hp = online && robot
                               ? static_cast<int>(useGlobalHp
                                                      ? robot->tabGlobalCurrentHP
                                                      : robot->currentHP)
                               : 0;
            const bool hasHealthSnapshot =
                robot && (robot->hasTabGlobalSnapshot || onlineByStatic);
            const int maxHp = qMax(
                online && robot
                    ? qMax(static_cast<int>(robot->currentRoundMaxHP),
                           static_cast<int>(robot->maxHP))
                    : 0,
                tacticalDefaultMaxHpForSlot(slot));
            const bool alive = online && hp > 0 &&
                               (!robot || robot->status != RobotStatus::DESTROYED ||
                                useGlobalHp);
            if (!ally && hasHealthSnapshot) {
                const bool wasZeroAfterPositive =
                    m_enemyWasZeroAfterPositiveHpById.value(robotId, false);
                if (wasZeroAfterPositive && hp > 150) {
                    m_enemyReviveFlashUntilMsById.insert(robotId, nowMs + 3900);
                    emit enemyPaidRespawnDetected(robotId);
                }
                if (hp > 0) {
                    m_robotHadPositiveHpById.insert(robotId, true);
                    m_enemyWasZeroAfterPositiveHpById.insert(robotId, false);
                } else if (m_robotHadPositiveHpById.value(robotId, false)) {
                    m_enemyWasZeroAfterPositiveHpById.insert(robotId, true);
                }
            }
            const bool reviveFlash =
                !ally && nowMs < m_enemyReviveFlashUntilMsById.value(robotId, 0);
            const int heatPct = onlineByStatic && robot && robot->heatLimit > 0
                                    ? pct(robot->currentHeat, robot->heatLimit)
                                    : -1;
            const int capPct = onlineByStatic && robot && robot->maxBufferEnergy > 0
                                   ? pct(robot->bufferEnergy, robot->maxBufferEnergy)
                                   : -1;

            QVariantMap item;
            item["id"] = tacticalRobotIdLabel(robotId, ally);
            item["robotId"] = robotId;
            item["slot"] = slot;
            item["type"] = tacticalRobotTypeIcon(type);
            item["label"] = tacticalRobotTypeLabel(type, slot);
            item["hp"] = hp;
            item["maxHp"] = maxHp;
            item["heatPct"] = heatPct;
            item["capPct"] = capPct;
            item["online"] = online;
            item["alive"] = alive;
            item["stale"] = ageMs > m_config.freshnessStaleMs;
            item["reviveFlash"] = reviveFlash;
            item["level"] = onlineByStatic && robot ? static_cast<int>(robot->level) : 0;
            list.append(item);
        }
        return list;
    };

    m_snapshot.allyRobotList = buildList(redPerspective, true);
    m_snapshot.enemyRobotList = buildList(!redPerspective, false);
}

void TacticalAnalyzer::updateAnalysisMetrics() {
    const auto &r = m_snapshot.resources;
    const bool redPerspective = m_gameData->currentRobotId() < 100;
    const int allyEconomy = redPerspective ? m_gameData->redEconomy()
                                           : m_gameData->blueEconomy();
    const int enemyEconomy = redPerspective ? m_gameData->blueEconomy()
                                            : m_gameData->redEconomy();
    const int allyDamage = redPerspective ? m_gameData->redTotalDamage()
                                          : m_gameData->blueTotalDamage();
    const int enemyDamage = redPerspective ? m_gameData->blueTotalDamage()
                                           : m_gameData->redTotalDamage();
    const QString hpStatus = r.hpDiff >= 0 ? QStringLiteral("good") : QStringLiteral("bad");
    const QString allyEconomyStatus =
        allyEconomy >= enemyEconomy ? QStringLiteral("good") : QStringLiteral("warn");
    const QString enemyEconomyStatus =
        enemyEconomy > allyEconomy ? QStringLiteral("warn") : QStringLiteral("good");
    const QString allyDamageStatus =
        allyDamage >= enemyDamage ? QStringLiteral("good") : QStringLiteral("warn");
    const QString enemyDamageStatus =
        enemyDamage > allyDamage ? QStringLiteral("warn") : QStringLiteral("good");

    m_snapshot.analysisMetrics = QVariantList{
        tacticalMetric("ally_economy", "我方总经济", allyEconomy,
                       allyEconomyStatus, "GlobalLogisticsStatus", 60,
                       QStringLiteral("敌方 %1").arg(enemyEconomy)),
        tacticalMetric("enemy_economy", "敌方总经济", enemyEconomy,
                       enemyEconomyStatus, "GlobalLogisticsStatus", 60,
                       QStringLiteral("我方 %1").arg(allyEconomy)),
        tacticalMetric("ally_damage", "我方总伤害", allyDamage,
                       allyDamageStatus, "RobotInjuryStat", 65,
                       QStringLiteral("敌方 %1").arg(enemyDamage)),
        tacticalMetric("enemy_damage", "敌方总伤害", enemyDamage,
                       enemyDamageStatus, "RobotInjuryStat", 65,
                       QStringLiteral("我方 %1").arg(allyDamage)),
        tacticalMetric("hp_diff", "总血量差", tacticalSignedNumber(r.hpDiff),
                       hpStatus, "GlobalUnitStatus", 80, tacticalLeadText(r.hpDiff))
    };
}

void TacticalAnalyzer::updateCameraPreviewData() {
    const bool hasFrame = m_gameData->hasHeroFrame();
    m_snapshot.cameraPreviewData = QVariantMap{
        {"connected", hasFrame},
        {"fps", hasFrame ? QStringLiteral("live") : QStringLiteral("--")},
        {"latencyMs", 0},
        {"decodeMs", 0},
        {"renderMs", 0},
        {"grayFrameRate", 0},
        {"stall", false},
        {"sourceName", QStringLiteral("CUSTOM CAM")},
        {"frameRevision", QVariant::fromValue<qulonglong>(m_gameData->heroFrameRevision())},
        {"frameSource", hasFrame ? m_gameData->heroFrameSource() : QString()}
    };
}

void TacticalAnalyzer::updateLinkHealth() {
    const int radarAge = m_snapshot.radar.radarAgeMs;
    const QString radarStatus = radarAge <= m_config.freshnessDegradedMs
                                    ? QStringLiteral("ok")
                                    : (radarAge <= m_config.freshnessStaleMs
                                           ? QStringLiteral("stale")
                                           : QStringLiteral("lost"));
    const bool hasHeroVideo = m_snapshot.cameraPreviewData.value("connected").toBool();
    m_snapshot.linkHealth = QVariantMap{
        {"mqttStatus", QStringLiteral("ok")},
        {"mqttLatencyMs", 0},
        {"videoStatus", hasHeroVideo ? QStringLiteral("ok") : QStringLiteral("unknown")},
        {"videoLatencyMs", m_snapshot.cameraPreviewData.value("latencyMs", 0)},
        {"radarStatus", radarStatus},
        {"radarAgeMs", radarAge},
        {"commandStatus", QStringLiteral("disabled")}
    };
    m_snapshot.header.linkStatus =
        QStringLiteral("MQTT %1 / VIDEO %2 / RADAR %3")
            .arg(m_snapshot.linkHealth.value("mqttStatus").toString().toUpper(),
                 m_snapshot.linkHealth.value("videoStatus").toString().toUpper(),
                 radarStatus.toUpper());
}

void TacticalAnalyzer::updateAllyExecution() {
    // 优先使用 ExecutionFusion 融合结果
    QVector<ExecutionAbility> abilities;
    if (m_fusion) {
        abilities = m_fusion->fuse();
    } else {
        // 融合器不可用时直接从 GameData 生成执行能力。
        const auto &allRobots = m_gameData->getAllRobots();
        int myId = m_gameData->currentRobotId();
        bool isRed = (myId < 100);

        for (const auto &robot : allRobots) {
            bool isAlly = (robot.robotId < 100) == isRed;
            if (!isAlly) continue;
            if (robot.status != RobotStatus::NORMAL) continue;

            ExecutionAbility ea;
            ea.robotId = robot.robotId;
            ea.label = QString("R%1").arg(robot.robotId % 100);
            ea.capPct = (robot.maxBufferEnergy > 0)
                ? static_cast<int>(100 * robot.bufferEnergy / robot.maxBufferEnergy) : 0;
            ea.heatPct = (robot.heatLimit > 0)
                ? static_cast<int>(100 * robot.currentHeat / robot.heatLimit) : 0;
            int ammo = robot.allowedAmmo17mm + robot.allowedAmmo42mm;
            ea.ammoPct = (ammo >= 100) ? 100 : qBound(0, ammo, 100);
            ea.lockedTarget = "-";
            ea.canFire = (robot.currentHeat < robot.heatLimit * 0.8
                          && ea.ammoPct > m_config.ammoLowRatio * 100);
            abilities.append(ea);
        }
    }

    QVariantList cards;
    for (const auto &a : abilities) {
        AllyExecutionCard c;
        c.id = QString::number(a.robotId);
        c.label = a.label;
        c.capPct = a.capPct;
        c.heatPct = a.heatPct;
        c.ammoPct = a.ammoPct;
        c.lockedTarget = a.lockedTarget;
        c.canFire = a.canFire;
        cards.append(toVariantMap(c));
    }
    m_snapshot.allyExecution = cards;
}

void TacticalAnalyzer::updatePredictions() {
    // 根据历史快照推算简单趋势。
    auto &p = m_snapshot.predictions;

    // 威胁趋势: 基于敌方 TOP 目标血量变化
    QVector<double> threatTrend;
    if (!m_snapshot.topTargets.isEmpty()) {
        double avgThreat = 0;
        for (auto &t : m_snapshot.topTargets) {
            avgThreat += t.toMap().value("threat", 0.0).toDouble();
        }
        avgThreat /= m_snapshot.topTargets.size();
        p.threatHistory.append(avgThreat);
        if (p.threatHistory.size() > m_config.trendHistorySize)
            p.threatHistory.removeFirst();
    }

    // 经济趋势
    double ecoDiff = m_gameData->redEconomy() - m_gameData->blueEconomy();
    p.economyHistory.append(ecoDiff);
    if (p.economyHistory.size() > m_config.trendHistorySize)
        p.economyHistory.removeFirst();

    // 判读趋势方向
    if (p.threatHistory.size() >= 2) {
        double last = p.threatHistory.last();
        double prev = p.threatHistory[p.threatHistory.size() - 2];
        p.threatTrend = (last > prev + 0.05) ? "↗" : (last < prev - 0.05) ? "↘" : "→";
        p.threatDesc = (last > 0.5) ? "高威胁" : "威胁可控";
    }

    if (p.economyHistory.size() >= 2) {
        double last = p.economyHistory.last();
        double prev = p.economyHistory[p.economyHistory.size() - 2];
        p.economyProjection = QString("%1%2")
            .arg(last >= prev ? "+" : "")
            .arg(qRound(last - prev));
    }

    p.predictedEvents.clear(); // 事件预测尚未接入。
}

// ═══════════════════════════════════════════════════════════
// QML 只读投影
// ═══════════════════════════════════════════════════════════

QVariantMap TacticalAnalyzer::headerData() const {
    QMutexLocker lock(&m_mutex);
    return toVariantMap(m_snapshot.header);
}

QVariantMap TacticalAnalyzer::resourceData() const {
    QMutexLocker lock(&m_mutex);
    return toVariantMap(m_snapshot.resources);
}

QVariantMap TacticalAnalyzer::topStatusData() const {
    QMutexLocker lock(&m_mutex);
    return toVariantMap(m_snapshot.topStatus);
}

QVariantList TacticalAnalyzer::keyEvents() const {
    QMutexLocker lock(&m_mutex);
    return m_snapshot.keyEvents;
}

QVariantMap TacticalAnalyzer::videoOverlay() const {
    QMutexLocker lock(&m_mutex);
    return toVariantMap(m_snapshot.video);
}

QVariantMap TacticalAnalyzer::mainDecision() const {
    QMutexLocker lock(&m_mutex);
    return toVariantMap(m_snapshot.decision);
}

QVariantList TacticalAnalyzer::topTargets() const {
    QMutexLocker lock(&m_mutex);
    return m_snapshot.topTargets;
}

QVariantMap TacticalAnalyzer::radarData() const {
    QMutexLocker lock(&m_mutex);
    return toVariantMap(m_snapshot.radar);
}

QVariantMap TacticalAnalyzer::mapData() const {
    QMutexLocker lock(&m_mutex);
    QVariantMap map = toVariantMap(m_snapshot.radar);
    map["estimated"] = true;
    map["calibrationProfile"] = QStringLiteral("estimated_official_ratio");
    return map;
}

QVariantList TacticalAnalyzer::allyRobotList() const {
    QMutexLocker lock(&m_mutex);
    return m_snapshot.allyRobotList;
}

QVariantList TacticalAnalyzer::enemyRobotList() const {
    QMutexLocker lock(&m_mutex);
    return m_snapshot.enemyRobotList;
}

QVariantList TacticalAnalyzer::analysisMetrics() const {
    QMutexLocker lock(&m_mutex);
    return m_snapshot.analysisMetrics;
}

QVariantMap TacticalAnalyzer::cameraPreviewData() const {
    QMutexLocker lock(&m_mutex);
    return m_snapshot.cameraPreviewData;
}

QVariantMap TacticalAnalyzer::linkHealth() const {
    QMutexLocker lock(&m_mutex);
    return m_snapshot.linkHealth;
}

QVariantList TacticalAnalyzer::allyExecution() const {
    QMutexLocker lock(&m_mutex);
    return m_snapshot.allyExecution;
}

QVariantMap TacticalAnalyzer::predictionData() const {
    QMutexLocker lock(&m_mutex);
    return toVariantMap(m_snapshot.predictions);
}

TacticalSnapshot TacticalAnalyzer::snapshot() const {
    QMutexLocker lock(&m_mutex);
    return m_snapshot;
}

// ═══════════════════════════════════════════════════════════
// toVariantMap 转换
// ═══════════════════════════════════════════════════════════

QVariantMap TacticalAnalyzer::toVariantMap(const MatchOverview &d) {
    return {{"matchMode", d.matchMode},
            {"redScore", d.redScore},
            {"blueScore", d.blueScore},
            {"currentRound", d.currentRound},
            {"totalRounds", d.totalRounds},
            {"timeRemaining", d.timeRemaining},
            {"stage", d.stage},
            {"linkStatus", d.linkStatus},
            {"linkLatency", d.linkLatency}};
}

QVariantMap TacticalAnalyzer::toVariantMap(const KeyEventItem &d) {
    return {{"time", d.time},
            {"icon", d.icon},
            {"text", d.text},
            {"color", d.color},
            {"priority", d.priority}};
}

QVariantMap TacticalAnalyzer::toVariantMap(const ResourceSummary &d) {
    return {{"allyBaseHp", d.allyBaseHp},
            {"allyBaseMax", d.allyBaseMax},
            {"enemyBaseHp", d.enemyBaseHp},
            {"enemyBaseMax", d.enemyBaseMax},
            {"allyOutpostHp", d.allyOutpostHp},
            {"allyOutpostMax", d.allyOutpostMax},
            {"enemyOutpostHp", d.enemyOutpostHp},
            {"enemyOutpostMax", d.enemyOutpostMax},
            {"allyOutpostDestroyed", d.allyOutpostDestroyed},
            {"enemyOutpostDestroyed", d.enemyOutpostDestroyed},
            {"allyBaseInvincible", d.allyBaseInvincible},
            {"enemyBaseInvincible", d.enemyBaseInvincible},
            {"allyDefenseBonus", d.allyDefenseBonus},
            {"enemyDefenseBonus", d.enemyDefenseBonus},
            {"economyDiff", d.economyDiff},
            {"damageDiff", d.damageDiff},
            {"hpDiff", d.hpDiff}};
}

QVariantMap TacticalAnalyzer::toVariantMap(const TopStatusSummary &d) {
    return {{"allyRemainingEconomy", d.allyRemainingEconomy},
            {"allyTotalEconomyObtained", d.allyTotalEconomyObtained},
            {"allyTechLevel", d.allyTechLevel},
            {"allyEncryptionLevel", d.allyEncryptionLevel},
            {"allyFortressOccupationSec", d.allyFortressOccupationSec},
            {"enemyFortressOccupationSec", d.enemyFortressOccupationSec},
            {"respawnGoldCost", d.respawnGoldCost},
            {"affordableRespawnCount", d.affordableRespawnCount},
            {"respawnEconomyVisible", d.respawnEconomyVisible}};
}

QVariantMap TacticalAnalyzer::toVariantMap(const VideoOverlay &d) {
    return {{"targetId", d.targetId},
            {"targetLabel", d.targetLabel},
            {"targetHp", d.targetHp},
            {"targetMaxHp", d.targetMaxHp},
            {"distance", d.distance},
            {"lockQuality", d.lockQuality},
            {"hasHologram", d.hasHologram},
            {"compensationText", d.compensationText}};
}

QVariantMap TacticalAnalyzer::toVariantMap(const MainDecision &d) {
    return {{"title", d.title},
            {"priority", d.priority},
            {"confidence", d.confidence},
            {"windowText", d.windowText},
            {"reasons", QVariant(d.reasons)},
            {"fallbackActions", QVariant(d.fallbackActions)}};
}

QVariantMap TacticalAnalyzer::toVariantMap(const TargetRankItem &d) {
    return {{"rank", d.rank},
            {"id", d.id},
            {"label", d.label},
            {"icon", d.icon},
            {"hp", d.hp},
            {"maxHp", d.maxHp},
            {"threat", d.threat}};
}

QVariantMap TacticalAnalyzer::toVariantMap(const MapRobot &d) {
    return {{"id", d.id},
            {"label", d.label},
            {"x", d.x},
            {"y", d.y},
            {"angle", d.angle}};
}

QVariantMap TacticalAnalyzer::toVariantMap(const RadarData &d) {
    return {{"radarAgeMs", d.radarAgeMs},
            {"allyRobots", d.allyRobots},
            {"enemyRobots", d.enemyRobots},
            {"focusTargetId", d.focusTargetId},
            {"routes", d.routes},
            {"dangerZones", d.dangerZones},
            {"buffZones", d.buffZones},
            {"mapImageSource", d.mapImageSource}};
}

QVariantMap TacticalAnalyzer::toVariantMap(const AllyExecutionCard &d) {
    return {{"id", d.id},
            {"label", d.label},
            {"capPct", d.capPct},
            {"heatPct", d.heatPct},
            {"ammoPct", d.ammoPct},
            {"lockedTarget", d.lockedTarget},
            {"canFire", d.canFire}};
}

QVariantMap TacticalAnalyzer::toVariantMap(const PredictionData &d) {
    QVariantList threatHistory;
    for (double value : d.threatHistory) {
        threatHistory.append(value);
    }
    QVariantList economyHistory;
    for (double value : d.economyHistory) {
        economyHistory.append(value);
    }
    return {{"threatHistory", threatHistory},
            {"economyHistory", economyHistory},
            {"threatTrend", d.threatTrend},
            {"threatDesc", d.threatDesc},
            {"economyProjection", d.economyProjection},
            {"predictedEvents", d.predictedEvents}};
}

} // namespace RM
