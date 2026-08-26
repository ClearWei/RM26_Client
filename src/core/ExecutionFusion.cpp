#include "ExecutionFusion.h"
#include "GameData.h"
#include "TacticalConfig.h"

#include <QDebug>
#include <QtGlobal>

namespace RM {

ExecutionFusion::ExecutionFusion(GameData *gameData, QObject *parent)
    : QObject(parent), m_gameData(gameData) {}

void ExecutionFusion::setConfig(const TacticalConfig &cfg) {
    QMutexLocker lock(&m_mutex);
    m_config = cfg;
}

QVariantMap ExecutionAbility::toMap() const {
    return {{"id", QString::number(robotId)},
            {"label", label},
            {"capPct", capPct},
            {"heatPct", heatPct},
            {"ammoPct", ammoPct},
            {"lockedTarget", lockedTarget},
            {"canFire", canFire}};
}

QVector<ExecutionAbility> ExecutionFusion::fuse() const {
    if (!m_gameData) return {};

    QMutexLocker lock(&m_mutex);

    int myId = m_gameData->currentRobotId();
    bool isRed = (myId < 100);

    QVector<ExecutionAbility> results;
    const auto &allRobots = m_gameData->getAllRobots();

    for (const auto &robot : allRobots) {
        bool isAlly = (robot.robotId < 100) == isRed;
        if (!isAlly) continue;
        if (robot.status != RobotStatus::NORMAL) continue;

        QVariantMap info = m_gameData->getRobotInfo(robot.robotId);
        QVariantMap custom = m_gameData->getMyRobotCustomData();

        ExecutionAbility ea = fuseSingle(robot.robotId, info, custom);
        results.append(ea);
    }

    const_cast<ExecutionFusion *>(this)->m_lastResult = results;
    return results;
}

ExecutionAbility ExecutionFusion::fuseSingle(
    int robotId,
    const QVariantMap &info,
    const QVariantMap &customData) const {

    ExecutionAbility ea;
    ea.robotId = robotId;
    ea.label = QString("R%1").arg(robotId % 100);

    // 超级电容
    int cap = info.value("bufferEnergy", 0).toInt();
    int maxCap = info.value("maxBufferEnergy", 1000).toInt();
    ea.capPct = (maxCap > 0) ? qBound(0, cap * 100 / maxCap, 100) : 0;

    // 热量
    int heat = info.value("currentHeat", 0).toInt();
    int maxHeat = info.value("heatLimit", 240).toInt();
    ea.heatPct = (maxHeat > 0) ? qBound(0, heat * 100 / maxHeat, 100) : 0;

    // 弹药
    int ammo17 = info.value("allowedAmmo17mm", 0).toInt();
    int ammo42 = info.value("allowedAmmo42mm", 0).toInt();
    int totalAmmo = ammo17 + ammo42;
    // 步兵默认弹药上限约 100-150, 英雄更高
    int maxAmmo = qMax(100, totalAmmo);
    ea.ammoPct = (maxAmmo > 0) ? qBound(0, totalAmmo * 100 / maxAmmo, 100) : 100;

    // 锁定目标
    ea.lockedTarget = "-";
    if (!customData.isEmpty()) {
        int targetId = customData.value("targetId", 0).toInt();
        if (targetId > 0) {
            ea.lockedTarget = QString("E%1").arg(targetId % 100);
        }
    }

    // 可射击判断: 热量 < 80% 且弹量高于战术阈值
    bool heatOk = (ea.heatPct < 80);
    bool ammoOk = (ea.ammoPct > qRound(m_config.ammoLowRatio * 100.0));
    ea.canFire = heatOk && ammoOk;

    // 易伤Buff
    ea.hasVulnerability = info.value("hasVulnerability", false).toBool();

    // 位置
    ea.posX = info.value("posX", 0.0).toDouble();
    ea.posY = info.value("posY", 0.0).toDouble();
    ea.isAlive = info.value("currentHealth", 0).toInt() > 0;

    return ea;
}

} // namespace RM
