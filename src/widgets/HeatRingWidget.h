#ifndef HEATRINGWIDGET_H
#define HEATRINGWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QTimer>
#include <QPaintEvent>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsDropShadowEffect>
#include <QSequentialAnimationGroup>
#include <QAbstractAnimation>
#include <QEasingCurve>
#include <QParallelAnimationGroup>
#include <QSequentialAnimationGroup>

class HeatRingWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(float shakeOffset READ getShakeOffset WRITE setShakeOffset)
    Q_PROPERTY(float flashOpacity READ getFlashOpacity WRITE setFlashOpacity)
    Q_PROPERTY(float pulseScale READ getPulseScale WRITE setPulseScale)
    Q_PROPERTY(float warningIntensity READ getWarningIntensity WRITE setWarningIntensity)

public:
    enum HeatLevel {
        Normal,      // 空环 (Q1=0)
        LowWarning,  // 0<Q1<1/2Q0
        MidWarning,  // 1/2Q0<Q1<3/4Q0
        HighWarning, // 3/4Q0<Q1<Q0
        Overheat     // Q1≥Q0 (触发扣血)
    };

    explicit HeatRingWidget(QWidget *parent = nullptr);

    // 设置接口
    void setHeat(int current, int maximum);
    void setDualBarrel(bool dual);
    void setBarrel1Heat(int current, int maximum);
    void setBarrel2Heat(int current, int maximum);
    void setRingSize(int size);
    void setThickness(int thickness);

    // 新增：精细热量阈值控制
    void setHeatThresholds(float lowWarning = 0.5f, float midWarning = 0.75f, float highWarning = 0.9f);
    void setOverheatPenalty(bool enabled);
    void setHeatDecayRate(float rate); // 热量衰减速率

    // 视觉效果
    void setGlowEnabled(bool enabled);
    void setPulseEnabled(bool enabled);
    void setShakeEnabled(bool enabled);
    void setParticleEnabled(bool enabled);
    void setWarningFlashEnabled(bool enabled);
    void setBreathingEnabled(bool enabled); // 新增：呼吸效果

    bool isGlowEnabled() const { return m_glowEnabled; }
    bool isPulseEnabled() const { return m_pulseEnabled; }
    bool isShakeEnabled() const { return m_shakeEnabled; }
    bool isParticleEnabled() const { return m_particleEnabled; }
    bool isWarningFlashEnabled() const { return m_warningFlashEnabled; }
    bool isBreathingEnabled() const { return m_breathingEnabled; }

    // 动画属性接口
    float getShakeOffset() const { return m_shakeOffset; }
    void setShakeOffset(float offset);
    float getFlashOpacity() const { return m_flashOpacity; }
    void setFlashOpacity(float opacity);
    float getPulseScale() const { return m_pulseScale; }
    void setPulseScale(float scale);
    float getWarningIntensity() const { return m_warningIntensity; }
    void setWarningIntensity(float intensity);

    // 查询接口
    int getCurrentHeat() const { return m_currentHeat; }
    int getMaxHeat() const { return m_maxHeat; }
    bool isDualBarrel() const { return m_dualBarrel; }
    HeatLevel getHeatLevel() const;
    bool isOverheated() const;

protected:
    void paintEvent(QPaintEvent *event) override;

public slots:
    void onAnimationTimer();
    void triggerOverheatShake();
    void triggerWarningFlash();
    void triggerOverheatPenalty(); // 新增：超热量惩罚动画

private slots:
    void updateShake();
    void updateFlash();
    void updatePulse();
    void updateWarning();

private:
    int m_currentHeat;
    int m_maxHeat;
    bool m_dualBarrel;

    // 双枪管热量
    int m_barrel1Current;
    int m_barrel1Max;
    int m_barrel2Current;
    int m_barrel2Max;

    // 视觉属性
    int m_ringSize;
    int m_thickness;

    // 新增：热量阈值
    float m_lowWarningThreshold;
    float m_midWarningThreshold;
    float m_highWarningThreshold;
    bool m_overheatPenaltyEnabled;
    float m_heatDecayRate;

    // 动画状态
    QTimer* m_animationTimer;
    float m_animationPhase;

    // 视觉效果状态
    bool m_glowEnabled;
    bool m_pulseEnabled;
    bool m_shakeEnabled;
    bool m_particleEnabled;
    bool m_warningFlashEnabled;
    bool m_breathingEnabled;

    // 动画对象
    QPropertyAnimation* m_shakeAnimation;
    QPropertyAnimation* m_flashAnimation;
    QPropertyAnimation* m_pulseAnimation;
    QPropertyAnimation* m_warningAnimation;
    QSequentialAnimationGroup* m_overheatSequence;
    QGraphicsOpacityEffect* m_opacityEffect;
    QGraphicsDropShadowEffect* m_glowEffect;

    // 特效状态
    float m_shakeOffset;
    float m_flashOpacity;
    float m_pulseScale;
    float m_warningIntensity;
    QList<QPointF> m_particles;
    QList<float> m_particleLife;
    QList<QPointF> m_particleVelocity; // 新增：粒子速度

    void setupAnimation();
    // 绘制方法
    void drawSingleBarrelRing(QPainter& painter);
    void drawDualBarrelRings(QPainter& painter);
    void drawHeatRing(QPainter& painter, int current, int maximum,
                      const QPointF& center, int radius, int thickness);
    void drawParticles(QPainter& painter);
    void drawWarningIndicators(QPainter& painter, HeatLevel level);
    void drawOverheatPenalty(QPainter& painter);
    void drawHeatDecayIndicator(QPainter& painter);

    void drawOverheatWarning(QPainter& painter); // 新增：超热量警告
    void drawHeatText(QPainter& painter, int current, int maximum, const QPointF& center); // 新增：热量文本
    void setupAnimations();
    void applyGlowEffect();
    void applyShakeEffect();

    QColor getHeatColor(int current, int maximum) const;
    QColor getEnhancedHeatColor(int current, int maximum) const; // 新增：增强颜色
    HeatLevel calculateHeatLevel(int current, int maximum) const;
    float getHeatPercentage(int current, int maximum) const;
    void updateParticleSystem(); // 新增：粒子系统更新
};

#endif // HEATRINGWIDGET_H
