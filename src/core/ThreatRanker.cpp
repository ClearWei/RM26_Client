#include "ThreatRanker.h"

#include <QVariantMap>
#include <algorithm>
#include <cmath>

namespace RM {

ThreatRanker::ThreatRanker(QObject *parent) : QObject(parent) {}

void ThreatRanker::setConfig(const TacticalConfig &cfg) {
    QMutexLocker lock(&m_mutex);
    m_config = cfg;
}

QVector<ThreatScore> ThreatRanker::rankTargets(
    const QMap<int, QVariantMap> &allyRobots,
    const QMap<int, QVariantMap> &enemyRobots,
    int allyBaseHp,
    const DataFreshnessGuard &freshness) const {

    QMutexLocker lock(&m_mutex);

    // 找到"当前操控机器人"作为距离计算参考原点
    QVariantMap myRobot;
    for (auto it = allyRobots.cbegin(); it != allyRobots.cend(); ++it) {
        myRobot = it.value(); // 取第一个己方机器人
        break;
    }

    QVector<ThreatScore> scores;
    scores.reserve(enemyRobots.size());

    for (auto it = enemyRobots.cbegin(); it != enemyRobots.cend(); ++it) {
        scores.append(scoreSingle(it.key(), it.value(), myRobot,
                                  allyBaseHp, freshness));
    }

    // 按 totalScore 降序排序
    std::sort(scores.begin(), scores.end(),
              [](const ThreatScore &a, const ThreatScore &b) {
                  return a.totalScore > b.totalScore;
              });

    return scores;
}

ThreatScore ThreatRanker::scoreSingle(
    int robotId,
    const QVariantMap &enemy,
    const QVariantMap &myRobot,
    int allyBaseHp,
    const DataFreshnessGuard &freshness) const {

    ThreatScore s;
    s.robotId = robotId;
    s.label = enemy.value("label").toString();
    s.icon = enemy.value("icon").toString();

    int hp = enemy.value("hp", 400).toInt();
    int maxHp = enemy.value("maxHp", 400).toInt();
    int robotType = enemy.value("type", 0).toInt();
    double ex = enemy.value("x", 0.0).toDouble();
    double ey = enemy.value("y", 0.0).toDouble();
    s.hp = hp;
    s.maxHp = maxHp;

    double myX = myRobot.value("x", 0.0).toDouble();
    double myY = myRobot.value("y", 0.0).toDouble();

    // 数据新鲜度
    QString sourceId = QString("enemy_pos_%1").arg(robotId);
    FreshnessResult fr = freshness.check(sourceId);
    s.freshnessWeight = fr.weightFactor;

    // 分项评分
    s.unitValueScore = calcUnitValue(robotType);
    s.lowHpScore = calcLowHpScore(hp, maxHp);
    s.radarMarkScore = enemy.value("isHighLight", false).toBool() ? 1.0 : 0.0;
    s.vulnerabilityScore = enemy.value("hasVulnerability", false).toBool() ? 1.0 : 0.0;
    s.threatToBaseScore = calcBaseThreatScore(ex, ey);
    s.objectiveScore = 0.0; // 后续从 GameData 读取战略目标
    s.distancePenalty = calcDistancePenalty(myX, myY, ex, ey);

    const auto &w = m_config;
    s.totalScore = s.freshnessWeight * (
        s.unitValueScore * w.w_unitValue +
        s.lowHpScore * w.w_lowHp +
        s.radarMarkScore * w.w_radarMark +
        s.vulnerabilityScore * w.w_vulnerability +
        s.threatToBaseScore * w.w_threatToBase +
        s.objectiveScore * w.w_objective -
        s.distancePenalty * w.w_distancePenalty
    );

    return s;
}

double ThreatRanker::calcUnitValue(int robotType) const {
    // 兵种价值：英雄(1)=1.0，哨兵(7)=0.8，步兵(3,4,5)=0.6，工程(2)=0.4。
    switch (robotType) {
    case 1: return 1.0;  // 英雄
    case 7: return 0.8;  // 哨兵
    case 3: case 4: case 5: return 0.6; // 步兵
    case 2: return 0.4;  // 工程
    default: return 0.5;
    }
}

double ThreatRanker::calcLowHpScore(int hp, int maxHp) const {
    if (maxHp <= 0) return 0.0;
    double ratio = static_cast<double>(hp) / maxHp;
    if (ratio < 0.15) return 1.0;      // 极残
    if (ratio < 0.30) return 0.85;     // 残血
    if (ratio < 0.50) return 0.55;     // 半血
    if (ratio < 0.75) return 0.25;     // 轻伤
    return 0.0;                         // 满血/健康
}

double ThreatRanker::calcDistancePenalty(double myX, double myY,
                                         double enemyX, double enemyY) const {
    double dx = myX - enemyX;
    double dy = myY - enemyY;
    double dist = std::sqrt(dx * dx + dy * dy);
    // 距离归一化: 28m 场地对角线 ≈ 31.8m → 归一化到 0~1
    double normalized = std::min(1.0, dist / 31.8);
    return normalized;
}

double ThreatRanker::calcBaseThreatScore(double enemyX, double enemyY) const {
    // 己方基地位于 x≈0 区域, y 中心附近
    // 敌方越接近 x=0 区域威胁越大
    constexpr double allyBaseX = 0.0;
    constexpr double allyBaseY = 7.5; // 场地半高

    double dx = enemyX - allyBaseX;
    double dy = enemyY - allyBaseY;
    double dist = std::sqrt(dx * dx + dy * dy);
    double normalized = 1.0 - std::min(1.0, dist / 20.0);
    return normalized * normalized; // 平方强调近距离
}

} // namespace RM
