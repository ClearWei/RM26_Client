#ifndef CROSSHAIRWIDGET_H
#define CROSSHAIRWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QGraphicsDropShadowEffect>
#include <QEasingCurve>

class CrosshairWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(float pulseScale READ getPulseScale WRITE setPulseScale)
    Q_PROPERTY(qreal opacity READ getOpacity WRITE setOpacity)

public:
    enum CrosshairType {
        Default,
        Sniper,
        Shotgun
    };

    explicit CrosshairWidget(QWidget *parent = nullptr);

    // 设置接口
    void setCrosshairType(CrosshairType type);
    void setVisible(bool visible) override;
    void setColor(const QColor& color);
    void setSize(int size);
    void setThickness(int thickness);
    void setGap(int gap);
    void setGlowEnabled(bool enabled);
    void setPulseEnabled(bool enabled);
    void setHitMarkerEnabled(bool enabled);
    void setDynamicSize(bool enabled);
    void setAccuracy(float accuracy); // 0.0-1.0，影响准星扩散范围
    void setPulseScale(float scale);
    void setOpacity(qreal opacity);

    // 查询接口
    CrosshairType getCrosshairType() const { return m_type; }
    QColor getColor() const { return m_color; }
    int getSize() const { return m_size; }
    int getThickness() const { return m_thickness; }
    int getGap() const { return m_gap; }
    bool isGlowEnabled() const { return m_glowEnabled; }
    bool isPulseEnabled() const { return m_pulseEnabled; }
    bool isHitMarkerEnabled() const { return m_hitMarkerEnabled; }
    bool isDynamicSizeEnabled() const { return m_dynamicSizeEnabled; }
    float getAccuracy() const { return m_accuracy; }
    float getPulseScale() const { return m_pulseScale; }
    qreal getOpacity() const { return m_opacity; }

public slots:
    void showHitMarker();
    void triggerPulse();

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void updatePulse();
    void hideHitMarker();

private:
    void drawDefaultCrosshair(QPainter& painter);
    void drawSniperCrosshair(QPainter& painter);
    void drawShotgunCrosshair(QPainter& painter);
    void setupAnimations();
    void applyGlowEffect(QPainter& painter);
    int calculateDynamicSize() const;

    CrosshairType m_type;
    QColor m_color;
    int m_size;
    int m_thickness;
    int m_gap;
    bool m_visible;

    // 视觉效果配置
    bool m_glowEnabled;
    bool m_pulseEnabled;
    bool m_hitMarkerEnabled;
    bool m_dynamicSizeEnabled;
    float m_accuracy;

    // 动画与特效对象
    QPropertyAnimation* m_pulseAnimation;
    QPropertyAnimation* m_hitMarkerAnimation;
    QGraphicsOpacityEffect* m_opacityEffect;
    QGraphicsDropShadowEffect* m_glowEffect;

    // 动画状态
    float m_pulseScale;
    bool m_hitMarkerVisible;
    int m_currentSize;
    qreal m_opacity;
};

#endif // CROSSHAIRWIDGET_H
