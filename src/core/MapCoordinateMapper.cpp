#include "MapCoordinateMapper.h"

namespace RM {

MapCoordinateMapper::MapCoordinateMapper(QObject *parent) : QObject(parent) {}

void MapCoordinateMapper::setBattlefieldSize(float width, float height) {
    QMutexLocker lock(&m_mutex);
    m_battlefieldW = (width > 0.0f) ? width : DEFAULT_BATTLEFIELD_W;
    m_battlefieldH = (height > 0.0f) ? height : DEFAULT_BATTLEFIELD_H;
}

void MapCoordinateMapper::setMapRect(const QRectF &rect) {
    QMutexLocker lock(&m_mutex);
    m_mapRect = rect;
}

QPointF MapCoordinateMapper::battlefieldToMap(float fieldX, float fieldY) const {
    QMutexLocker lock(&m_mutex);
    if (m_mapRect.isEmpty() || m_battlefieldW <= 0.0f || m_battlefieldH <= 0.0f) {
        return QPointF(fieldX, fieldY);
    }
    // 归一化 → 像素
    float nx = fieldX / m_battlefieldW;
    float ny = fieldY / m_battlefieldH;
    return QPointF(m_mapRect.x() + nx * m_mapRect.width(),
                   m_mapRect.y() + ny * m_mapRect.height());
}

QPointF MapCoordinateMapper::mapToBattlefield(float mapX, float mapY) const {
    QMutexLocker lock(&m_mutex);
    if (m_mapRect.isEmpty()) {
        return QPointF(mapX, mapY);
    }
    float nx = (mapX - m_mapRect.x()) / m_mapRect.width();
    float ny = (mapY - m_mapRect.y()) / m_mapRect.height();
    return QPointF(nx * m_battlefieldW, ny * m_battlefieldH);
}

QPointF MapCoordinateMapper::battlefieldToNormalized(float fieldX, float fieldY) const {
    QMutexLocker lock(&m_mutex);
    if (m_battlefieldW <= 0.0f || m_battlefieldH <= 0.0f) {
        return QPointF(0.5, 0.5);
    }
    return QPointF(fieldX / m_battlefieldW, fieldY / m_battlefieldH);
}

QPointF MapCoordinateMapper::normalizedToBattlefield(float nx, float ny) const {
    QMutexLocker lock(&m_mutex);
    return QPointF(nx * m_battlefieldW, ny * m_battlefieldH);
}

float MapCoordinateMapper::yawToCanvasAngle(float yawDeg) const {
    Q_UNUSED(m_mutex); // 此换算不读取可变状态。
    // 战场 yaw (北=0, 顺时针+) → Canvas 角度 (东=0, 顺时针+)
    return 90.0f - yawDeg;
}

} // namespace RM
