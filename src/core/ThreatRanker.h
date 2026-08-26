#pragma once

#include "TacticalConfig.h"
#include "DataFreshnessGuard.h"

#include <QMutex>
#include <QObject>
#include <QVector>

namespace RM {

/// 威胁/价值评分结果
struct ThreatScore {
    int robotId = 0;
    QString label;
    QString icon;
    double totalScore = 0.0;

    // 分项评分 (调试用)
    double unitValueScore = 0.0;
    double lowHpScore = 0.0;
    double radarMarkScore = 0.0;
    double vulnerabilityScore = 0.0;
    double threatToBaseScore = 0.0;
    double objectiveScore = 0.0;
    double distancePenalty = 0.0;
    double freshnessWeight = 1.0;

    // 原始数据 (生成 TargetRankItem 用)
    int hp = 0;
    int maxHp = 400;
};

/// 敌方威胁 / 价值评分排序器。
/// 对敌方机器人逐项打分 (兵种价值 / 低血量 / 雷达标记 / 易伤 /
/// 基地威胁 / 战略目标 / 距离惩罚), 按总分排序输出 TOP N。
class ThreatRanker : public QObject {
    Q_OBJECT

public:
    explicit ThreatRanker(QObject *parent = nullptr);

    /// 设置可调配置
    void setConfig(const TacticalConfig &cfg);
    TacticalConfig config() const { return m_config; }

    /// 执行一次完整评分。
    /// @param allyRobots    所有己方机器人 (key = robotId)
    /// @param enemyRobots   所有敌方机器人 (key = robotId)
    /// @param allyBase      己方基地
    /// @param freshness     数据新鲜度守卫
    /// @return 按 totalScore 降序排列的评分列表
    QVector<ThreatScore> rankTargets(
        const QMap<int, QVariantMap> &allyRobots,
        const QMap<int, QVariantMap> &enemyRobots,
        int allyBaseHp,
        const DataFreshnessGuard &freshness) const;

signals:
    void rankingUpdated();

private:
    /// 对单个敌方机器人评分
    ThreatScore scoreSingle(
        int robotId,
        const QVariantMap &enemy,
        const QVariantMap &myRobot,
        int allyBaseHp,
        const DataFreshnessGuard &freshness) const;

    /// 兵种基础价值 (英雄 > 哨兵 > 步兵 > 工程)
    double calcUnitValue(int robotType) const;
    /// 残血得分 (血量越低分越高)
    double calcLowHpScore(int hp, int maxHp) const;
    /// 我方距目标距离惩罚 (越远扣越多)
    double calcDistancePenalty(double myX, double myY,
                               double enemyX, double enemyY) const;
    /// 对基地威胁评分 (敌方越靠近我方基地分越高)
    double calcBaseThreatScore(double enemyX, double enemyY) const;

    TacticalConfig m_config = TacticalConfig::defaults();
    mutable QMutex m_mutex;
};

} // namespace RM
