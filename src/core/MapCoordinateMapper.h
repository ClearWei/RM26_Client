#pragma once

#include <QMutex>
#include <QObject>
#include <QPointF>
#include <QRectF>

namespace RM {

/// 战场坐标 ↔ 地图坐标转换器。
/// 统一维护坐标映射, QML 只拿换算后的 mapX/mapY 画点。
class MapCoordinateMapper : public QObject {
    Q_OBJECT

public:
    explicit MapCoordinateMapper(QObject *parent = nullptr);

    /// 设置战场物理尺寸 (默认 28m × 15m)
    void setBattlefieldSize(float width, float height);
    float battlefieldWidth() const { return m_battlefieldW; }
    float battlefieldHeight() const { return m_battlefieldH; }

    /// 设置小地图显示区域 (像素)
    void setMapRect(const QRectF &rect);
    QRectF mapRect() const { return m_mapRect; }

    /// 战场坐标 (米) → 地图像素坐标
    QPointF battlefieldToMap(float fieldX, float fieldY) const;
    /// 地图像素坐标 → 战场坐标 (米)
    QPointF mapToBattlefield(float mapX, float mapY) const;

    /// 战场坐标 (米) → 归一化坐标 (0.0~1.0, QML Canvas 用)
    QPointF battlefieldToNormalized(float fieldX, float fieldY) const;
    /// 归一化坐标 → 战场坐标
    QPointF normalizedToBattlefield(float nx, float ny) const;

    /// 战场 yaw 角度 → Canvas 旋转角度
    float yawToCanvasAngle(float yawDeg) const;

    // 标准 RM 场地尺寸
    static constexpr float DEFAULT_BATTLEFIELD_W = 28.0f;
    static constexpr float DEFAULT_BATTLEFIELD_H = 15.0f;

private:
    float m_battlefieldW = DEFAULT_BATTLEFIELD_W;
    float m_battlefieldH = DEFAULT_BATTLEFIELD_H;
    QRectF m_mapRect;
    mutable QMutex m_mutex;
};

} // namespace RM
