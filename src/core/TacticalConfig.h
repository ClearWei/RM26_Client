#pragma once

#include <QObject>

namespace RM {

/// 战术分析系统所有可调参数的集中定义。
/// 后续可从 config.json 加载或 QML 设置面板运行时修改。
struct TacticalConfig {
    Q_GADGET
public:

    // ── 威胁评分权重 (总和应 ≈ 1.0) ──
    Q_PROPERTY(double w_unitValue MEMBER w_unitValue)
    Q_PROPERTY(double w_lowHp MEMBER w_lowHp)
    Q_PROPERTY(double w_radarMark MEMBER w_radarMark)
    Q_PROPERTY(double w_vulnerability MEMBER w_vulnerability)
    Q_PROPERTY(double w_threatToBase MEMBER w_threatToBase)
    Q_PROPERTY(double w_objective MEMBER w_objective)
    Q_PROPERTY(double w_distancePenalty MEMBER w_distancePenalty)

    double w_unitValue = 0.15;
    double w_lowHp = 0.25;
    double w_radarMark = 0.20;
    double w_vulnerability = 0.15;
    double w_threatToBase = 0.15;
    double w_objective = 0.10;
    double w_distancePenalty = 0.05;

    // ── 决策规则阈值 ──
    Q_PROPERTY(double baseDefendHpRatio MEMBER baseDefendHpRatio)
    Q_PROPERTY(double killWindowHpRatio MEMBER killWindowHpRatio)
    Q_PROPERTY(double outpostPushHpRatio MEMBER outpostPushHpRatio)
    Q_PROPERTY(int fireReadyMin MEMBER fireReadyMin)
    Q_PROPERTY(double ammoLowRatio MEMBER ammoLowRatio)
    Q_PROPERTY(double economySupplyThreshold MEMBER economySupplyThreshold)

    double baseDefendHpRatio = 0.35;
    double killWindowHpRatio = 0.30;
    double outpostPushHpRatio = 0.35;
    int fireReadyMin = 2;
    double ammoLowRatio = 0.30;
    double economySupplyThreshold = 200;

    // ── 数据时效阈值 (毫秒) ──
    Q_PROPERTY(qint64 freshnessFreshMs MEMBER freshnessFreshMs)
    Q_PROPERTY(qint64 freshnessDegradedMs MEMBER freshnessDegradedMs)
    Q_PROPERTY(qint64 freshnessStaleMs MEMBER freshnessStaleMs)

    qint64 freshnessFreshMs = 300;
    qint64 freshnessDegradedMs = 800;
    qint64 freshnessStaleMs = 1500;

    // ── 决策防抖 ──
    Q_PROPERTY(int decisionHoldMs MEMBER decisionHoldMs)
    Q_PROPERTY(double decisionSwitchThreshold MEMBER decisionSwitchThreshold)
    Q_PROPERTY(int decisionConfirmFrames MEMBER decisionConfirmFrames)

    int decisionHoldMs = 2000;
    double decisionSwitchThreshold = 15.0;
    int decisionConfirmFrames = 3;

    // ── 地图 ──
    Q_PROPERTY(float battlefieldWidth MEMBER battlefieldWidth)
    Q_PROPERTY(float battlefieldHeight MEMBER battlefieldHeight)

    float battlefieldWidth = 28.0f;
    float battlefieldHeight = 15.0f;

    // ── 面板限制 ──
    Q_PROPERTY(int maxKeyEvents MEMBER maxKeyEvents)
    Q_PROPERTY(int maxTopTargets MEMBER maxTopTargets)

    int maxKeyEvents = 7;
    int maxTopTargets = 4;

    // ── 趋势预测 ──
    Q_PROPERTY(int trendHistorySize MEMBER trendHistorySize)
    Q_PROPERTY(int predictionWindowSec MEMBER predictionWindowSec)

    int trendHistorySize = 5;
    int predictionWindowSec = 20;

    /// 返回一套默认配置
    static TacticalConfig defaults() { return TacticalConfig(); }
};

} // namespace RM
