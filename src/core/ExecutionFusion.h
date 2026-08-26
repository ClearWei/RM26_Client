#pragma once

#include "TacticalConfig.h"

#include <QMutex>
#include <QObject>
#include <QVector>
#include <QVariantMap>

// 前向声明
class GameData;

namespace RM {

/// 单机器人执行能力摘要
struct ExecutionAbility {
    int robotId = 0;
    QString label;
    int capPct = 0;          // 超级电容百分比 (0-100)
    int heatPct = 0;         // 当前热量百分比 (0-100)
    int ammoPct = 0;         // 弹药百分比 (0-100)
    QString lockedTarget;    // 锁定目标ID ("-" 表示无)
    bool canFire = false;    // 是否可射击 (热量OK + 弹量OK)
    bool isAlive = true;     // 是否存活
    bool hasVulnerability = false;  // 是否有易伤Buff
    double posX = 0.0;
    double posY = 0.0;

    QVariantMap toMap() const;
};

/// 执行能力融合器。
///
/// 职责:
///   - 从 GameData 读取所有己方机器人状态
///   - 融合热量、弹药、电容、Buff、位置等信息
///   - 判断哪些机器人可以射击、需要保热、弹量不足
///   - 为 TacticalAnalyzer 提供 ExecutionAbility 列表
///
/// 当前使用 GameData 已有数据 (Phase 3).
/// 未来可接入 0x0310 TeamTelemetry 汇聚数据 (Phase 4-5).
class ExecutionFusion : public QObject {
    Q_OBJECT

public:
    explicit ExecutionFusion(GameData *gameData, QObject *parent = nullptr);

    /// 设置可调配置
    void setConfig(const TacticalConfig &cfg);

    /// 执行一次融合: 读取所有己方机器人状态, 返回执行能力列表
    QVector<ExecutionAbility> fuse() const;

    /// 获取最近一次融合结果
    QVector<ExecutionAbility> lastResult() const { return m_lastResult; }

signals:
    void fusionUpdated(const QVector<ExecutionAbility> &results);

private:
    ExecutionAbility fuseSingle(int robotId,
                                const QVariantMap &robotInfo,
                                const QVariantMap &customData) const;

    GameData *m_gameData;
    TacticalConfig m_config;
    QVector<ExecutionAbility> m_lastResult;
    mutable QMutex m_mutex;
};

} // namespace RM
