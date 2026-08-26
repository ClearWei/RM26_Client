#pragma once

#include "TacticalConfig.h"
#include "TacticalSnapshot.h"
#include "DataFreshnessGuard.h"
#include "MapCoordinateMapper.h"

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

// 前向声明 (避免循环依赖)
class GameData;

namespace RM {

class ThreatRanker;
class ExecutionFusion;

/// 战术分析核心引擎。
/// 定时从 GameData 收集汇总数据 → 调用 ThreatRanker/DataFreshnessGuard/
/// MapCoordinateMapper 进行分析 → 生成 TacticalSnapshot → 通过 Q_PROPERTY
/// 暴露给 QML 战术指挥屏。
class TacticalAnalyzer : public QObject {
    Q_OBJECT

    Q_PROPERTY(QVariantMap headerData READ headerData NOTIFY snapshotUpdated)
    Q_PROPERTY(QVariantMap resourceData READ resourceData NOTIFY snapshotUpdated)
    Q_PROPERTY(QVariantMap topStatusData READ topStatusData NOTIFY snapshotUpdated)
    Q_PROPERTY(QVariantList keyEvents READ keyEvents NOTIFY snapshotUpdated)
    Q_PROPERTY(QVariantMap videoOverlay READ videoOverlay NOTIFY snapshotUpdated)
    Q_PROPERTY(QVariantMap mainDecision READ mainDecision NOTIFY snapshotUpdated)
    Q_PROPERTY(QVariantList topTargets READ topTargets NOTIFY snapshotUpdated)
    Q_PROPERTY(QVariantMap radarData READ radarData NOTIFY snapshotUpdated)
    Q_PROPERTY(QVariantMap mapData READ mapData NOTIFY snapshotUpdated)
    Q_PROPERTY(QVariantList allyRobotList READ allyRobotList NOTIFY snapshotUpdated)
    Q_PROPERTY(QVariantList enemyRobotList READ enemyRobotList NOTIFY snapshotUpdated)
    Q_PROPERTY(QVariantList analysisMetrics READ analysisMetrics NOTIFY snapshotUpdated)
    Q_PROPERTY(QVariantMap cameraPreviewData READ cameraPreviewData NOTIFY snapshotUpdated)
    Q_PROPERTY(QVariantMap linkHealth READ linkHealth NOTIFY snapshotUpdated)
    Q_PROPERTY(QString layoutMode READ layoutMode WRITE setLayoutMode NOTIFY layoutModeChanged)
    Q_PROPERTY(QVariantList allyExecution READ allyExecution NOTIFY snapshotUpdated)
    Q_PROPERTY(QVariantMap predictionData READ predictionData NOTIFY snapshotUpdated)
    Q_PROPERTY(bool useMockData READ useMockData WRITE setUseMockData NOTIFY useMockDataChanged)

public:
    explicit TacticalAnalyzer(GameData *gameData, QObject *parent = nullptr);

    // ── 配置 ──
    void setConfig(const TacticalConfig &cfg);
    TacticalConfig config() const { return m_config; }

    // ── 从属组件注入 ──
    void setThreatRanker(ThreatRanker *ranker) { m_ranker = ranker; }
    void setFreshnessGuard(DataFreshnessGuard *guard) { m_freshness = guard; }
    void setCoordMapper(MapCoordinateMapper *mapper) { m_mapper = mapper; }
    void setExecutionFusion(ExecutionFusion *fusion) { m_fusion = fusion; }

    // ── 启停 ──
    void start(int intervalMs = 100);
    void stop();
    bool isRunning() const;

    // ── 数据源切换 ──
    bool useMockData() const { return m_useMockData; }
    void setUseMockData(bool use);

    // ── QML 只读投影 ──
    QVariantMap headerData() const;
    QVariantMap resourceData() const;
    QVariantMap topStatusData() const;
    QVariantList keyEvents() const;
    QVariantMap videoOverlay() const;
    QVariantMap mainDecision() const;
    QVariantList topTargets() const;
    QVariantMap radarData() const;
    QVariantMap mapData() const;
    QVariantList allyRobotList() const;
    QVariantList enemyRobotList() const;
    QVariantList analysisMetrics() const;
    QVariantMap cameraPreviewData() const;
    QVariantMap linkHealth() const;
    QString layoutMode() const;
    void setLayoutMode(const QString &mode);
    QVariantList allyExecution() const;
    QVariantMap predictionData() const;

    TacticalSnapshot snapshot() const;

signals:
    void snapshotUpdated();
    void useMockDataChanged();
    void layoutModeChanged();
    void enemyPaidRespawnDetected(int robotId);

public slots:
    /// 执行一次完整分析（可由 QML 手动触发）
    void analyze();

private:
    void loadMockData();
    void analyzeMock();
    void analyzeReal();

    // 各面板数据更新
    void updateHeader();
    void updateResources();
    void updateTopStatus();
    void updateKeyEvents();
    void updateVideoOverlay();
    void updateMainDecision();
    void updateTopTargets();
    void updateRadarData();
    void updateRobotLists();
    void updateAnalysisMetrics();
    void updateCameraPreviewData();
    void updateLinkHealth();
    void updateAllyExecution();
    void updatePredictions();

    // 转换辅助
    static QVariantMap toVariantMap(const MatchOverview &d);
    static QVariantMap toVariantMap(const ResourceSummary &d);
    static QVariantMap toVariantMap(const TopStatusSummary &d);
    static QVariantMap toVariantMap(const KeyEventItem &d);
    static QVariantMap toVariantMap(const VideoOverlay &d);
    static QVariantMap toVariantMap(const MainDecision &d);
    static QVariantMap toVariantMap(const TargetRankItem &d);
    static QVariantMap toVariantMap(const MapRobot &d);
    static QVariantMap toVariantMap(const RadarData &d);
    static QVariantMap toVariantMap(const AllyExecutionCard &d);
    static QVariantMap toVariantMap(const PredictionData &d);

    // ── 成员 ──
    GameData *m_gameData;
    TacticalConfig m_config = TacticalConfig::defaults();
    ThreatRanker *m_ranker = nullptr;
    DataFreshnessGuard *m_freshness = nullptr;
    MapCoordinateMapper *m_mapper = nullptr;
    ExecutionFusion *m_fusion = nullptr;

    bool m_useMockData = true;        // true 使用模拟数据，false 使用 GameData 实时数据。
    TacticalSnapshot m_snapshot;
    QTimer m_timer;
    mutable QMutex m_mutex;

    // 决策防抖
    MainDecision m_lastDecision;
    int m_decisionFrameCount = 0;
    qint64 m_lastDecisionTime = 0;

    QHash<int, bool> m_robotHadPositiveHpById;
    QHash<int, bool> m_enemyWasZeroAfterPositiveHpById;
    QHash<int, qint64> m_enemyReviveFlashUntilMsById;
    int m_robotFlashStateRound = 0;
};

} // namespace RM
