#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QObject>
#include <QMutex>
#include <QString>
#include <QVector>
#include <QVariantList>
#include <QVariantMap>

namespace RM {

// ═══════════════════════════════════════════════════════════
// TacticalSnapshot — QML 数据契约 (Phase 2 QML 绑定)
// 所有结构体使用 Q_GADGET 支持 QVariant 序列化
// ═══════════════════════════════════════════════════════════

/// 比赛概览 (→ TacticalHeader)
struct MatchOverview {
    Q_GADGET
public:
    Q_PROPERTY(QString matchMode MEMBER matchMode)
    Q_PROPERTY(int redScore MEMBER redScore)
    Q_PROPERTY(int blueScore MEMBER blueScore)
    Q_PROPERTY(int currentRound MEMBER currentRound)
    Q_PROPERTY(int totalRounds MEMBER totalRounds)
    Q_PROPERTY(QString timeRemaining MEMBER timeRemaining)
    Q_PROPERTY(QString stage MEMBER stage)
    Q_PROPERTY(QString linkStatus MEMBER linkStatus)
    Q_PROPERTY(int linkLatency MEMBER linkLatency)

    QString matchMode;
    int redScore = 0;
    int blueScore = 0;
    int currentRound = 1;
    int totalRounds = 5;
    QString timeRemaining;
    QString stage;
    QString linkStatus;
    int linkLatency = 0;
};

/// 资源概要 (→ ResourceSummaryBar)
struct ResourceSummary {
    Q_GADGET
public:
    Q_PROPERTY(int allyBaseHp MEMBER allyBaseHp)
    Q_PROPERTY(int allyBaseMax MEMBER allyBaseMax)
    Q_PROPERTY(int enemyBaseHp MEMBER enemyBaseHp)
    Q_PROPERTY(int enemyBaseMax MEMBER enemyBaseMax)
    Q_PROPERTY(int allyOutpostHp MEMBER allyOutpostHp)
    Q_PROPERTY(int allyOutpostMax MEMBER allyOutpostMax)
    Q_PROPERTY(int enemyOutpostHp MEMBER enemyOutpostHp)
    Q_PROPERTY(int enemyOutpostMax MEMBER enemyOutpostMax)
    Q_PROPERTY(bool allyOutpostDestroyed MEMBER allyOutpostDestroyed)
    Q_PROPERTY(bool enemyOutpostDestroyed MEMBER enemyOutpostDestroyed)
    Q_PROPERTY(bool allyBaseInvincible MEMBER allyBaseInvincible)
    Q_PROPERTY(bool enemyBaseInvincible MEMBER enemyBaseInvincible)
    Q_PROPERTY(int allyDefenseBonus MEMBER allyDefenseBonus)
    Q_PROPERTY(int enemyDefenseBonus MEMBER enemyDefenseBonus)
    Q_PROPERTY(int economyDiff MEMBER economyDiff)
    Q_PROPERTY(int damageDiff MEMBER damageDiff)
    Q_PROPERTY(int hpDiff MEMBER hpDiff)

    int allyBaseHp = 0;
    int allyBaseMax = 5000;
    int enemyBaseHp = 0;
    int enemyBaseMax = 5000;
    int allyOutpostHp = 0;
    int allyOutpostMax = 1500;
    int enemyOutpostHp = 0;
    int enemyOutpostMax = 1500;
    bool allyOutpostDestroyed = false;
    bool enemyOutpostDestroyed = false;
    bool allyBaseInvincible = false;
    bool enemyBaseInvincible = false;
    int allyDefenseBonus = 0;
    int enemyDefenseBonus = 0;
    int economyDiff = 0;
    int damageDiff = 0;
    int hpDiff = 0;
};

/// 顶部后勤/机制条 (→ TopLogisticsStrip)
struct TopStatusSummary {
    Q_GADGET
public:
    Q_PROPERTY(int allyRemainingEconomy MEMBER allyRemainingEconomy)
    Q_PROPERTY(qulonglong allyTotalEconomyObtained MEMBER allyTotalEconomyObtained)
    Q_PROPERTY(int allyTechLevel MEMBER allyTechLevel)
    Q_PROPERTY(int allyEncryptionLevel MEMBER allyEncryptionLevel)
    Q_PROPERTY(int allyFortressOccupationSec MEMBER allyFortressOccupationSec)
    Q_PROPERTY(int enemyFortressOccupationSec MEMBER enemyFortressOccupationSec)
    Q_PROPERTY(int respawnGoldCost MEMBER respawnGoldCost)
    Q_PROPERTY(int affordableRespawnCount MEMBER affordableRespawnCount)
    Q_PROPERTY(bool respawnEconomyVisible MEMBER respawnEconomyVisible)

    int allyRemainingEconomy = 0;
    qulonglong allyTotalEconomyObtained = 0;
    int allyTechLevel = 0;
    int allyEncryptionLevel = 0;
    int allyFortressOccupationSec = 0;
    int enemyFortressOccupationSec = 0;
    int respawnGoldCost = 0;
    int affordableRespawnCount = 0;
    bool respawnEconomyVisible = false;
};

/// 关键事件项 (→ KeyEventChainPanel)
struct KeyEventItem {
    Q_GADGET
public:
    Q_PROPERTY(QString time MEMBER time)
    Q_PROPERTY(QString icon MEMBER icon)
    Q_PROPERTY(QString text MEMBER text)
    Q_PROPERTY(QString color MEMBER color)
    Q_PROPERTY(QString priority MEMBER priority)

    QString time;
    QString icon;
    QString text;
    QString color;
    QString priority;
};

/// 图传覆盖数据 (→ TacticalVideoStage)
struct VideoOverlay {
    Q_GADGET
public:
    Q_PROPERTY(QString targetId MEMBER targetId)
    Q_PROPERTY(QString targetLabel MEMBER targetLabel)
    Q_PROPERTY(int targetHp MEMBER targetHp)
    Q_PROPERTY(int targetMaxHp MEMBER targetMaxHp)
    Q_PROPERTY(double distance MEMBER distance)
    Q_PROPERTY(double lockQuality MEMBER lockQuality)
    Q_PROPERTY(bool hasHologram MEMBER hasHologram)
    Q_PROPERTY(QString compensationText MEMBER compensationText)

    QString targetId;
    QString targetLabel;
    int targetHp = 0;
    int targetMaxHp = 400;
    double distance = 0.0;
    double lockQuality = 0.0;
    bool hasHologram = false;
    QString compensationText;
};

/// 主决策 (→ MainDecisionConsole)
struct MainDecision {
    Q_GADGET
public:
    Q_PROPERTY(QString title MEMBER title)
    Q_PROPERTY(QString priority MEMBER priority)
    Q_PROPERTY(int confidence MEMBER confidence)
    Q_PROPERTY(QString windowText MEMBER windowText)
    Q_PROPERTY(QStringList reasons MEMBER reasons)
    Q_PROPERTY(QStringList fallbackActions MEMBER fallbackActions)

    QString title;
    QString priority;
    int confidence = 0;
    QString windowText;
    QStringList reasons;
    QStringList fallbackActions;
};

/// 目标排名项 (→ TargetRankPanel)
struct TargetRankItem {
    Q_GADGET
public:
    Q_PROPERTY(int rank MEMBER rank)
    Q_PROPERTY(QString id MEMBER id)
    Q_PROPERTY(QString label MEMBER label)
    Q_PROPERTY(QString icon MEMBER icon)
    Q_PROPERTY(int hp MEMBER hp)
    Q_PROPERTY(int maxHp MEMBER maxHp)
    Q_PROPERTY(double threat MEMBER threat)

    int rank = 0;
    QString id;
    QString label;
    QString icon;
    int hp = 0;
    int maxHp = 400;
    double threat = 0.0;
};

/// 地图上机器人 (→ TacticalRadarMap)
struct MapRobot {
    Q_GADGET
public:
    Q_PROPERTY(QString id MEMBER id)
    Q_PROPERTY(QString label MEMBER label)
    Q_PROPERTY(double x MEMBER x)
    Q_PROPERTY(double y MEMBER y)
    Q_PROPERTY(double angle MEMBER angle)

    QString id;
    QString label;
    double x = 0.0;
    double y = 0.0;
    double angle = 0.0;
};

/// 雷达地图数据 (→ TacticalRadarMap)
/// 不使用 Q_GADGET: 含 QVariantList/QVector 复杂成员, 由 TacticalAnalyzer 手动转为 QVariantMap
struct RadarData {
    int radarAgeMs = 0;
    QVariantList allyRobots;
    QVariantList enemyRobots;
    QString focusTargetId;
    QVariantList routes;
    QVariantList dangerZones;
    QVariantList buffZones;
    QString mapImageSource;
};

/// 我方执行能力卡片 (→ AllyCriticalStatusPanel)
struct AllyExecutionCard {
    Q_GADGET
public:
    Q_PROPERTY(QString id MEMBER id)
    Q_PROPERTY(QString label MEMBER label)
    Q_PROPERTY(QString lockedTarget MEMBER lockedTarget)
    Q_PROPERTY(int capPct MEMBER capPct)
    Q_PROPERTY(int heatPct MEMBER heatPct)
    Q_PROPERTY(int ammoPct MEMBER ammoPct)
    Q_PROPERTY(bool canFire MEMBER canFire)

    QString id;
    QString label;
    QString lockedTarget;
    int capPct = 0;
    int heatPct = 0;
    int ammoPct = 0;
    bool canFire = false;
};

/// 趋势预测 (→ PredictionPanel)
/// 不使用 Q_GADGET: 含 QVector/QVariantList 复杂成员, 由 TacticalAnalyzer 手动转为 QVariantMap
struct PredictionData {
    QVector<double> threatHistory;
    QVector<double> economyHistory;
    QString threatTrend;
    QString threatDesc;
    QString economyProjection;
    QVariantList predictedEvents;
};

/// ─── 总快照 ───────────────────────────────────────
/// 不使用 Q_GADGET: 含 RadarData/PredictionData 等复杂成员,
/// 由 TacticalAnalyzer 的 Q_PROPERTY getters 分项暴露给 QML
struct TacticalSnapshot {
    MatchOverview header;
    ResourceSummary resources;
    TopStatusSummary topStatus;
    QVariantList keyEvents;
    VideoOverlay video;
    MainDecision decision;
    QVariantList topTargets;
    RadarData radar;
    QVariantList allyRobotList;
    QVariantList enemyRobotList;
    QVariantList analysisMetrics;
    QVariantMap cameraPreviewData;
    QVariantMap linkHealth;
    QString layoutMode = QStringLiteral("map_primary");
    QVariantList allyExecution;
    PredictionData predictions;
    qint64 snapshotTimestamp = 0;
};

} // namespace RM
