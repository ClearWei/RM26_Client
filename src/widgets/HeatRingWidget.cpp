#include "HeatRingWidget.h"
#include <QPainter>
#include <QtMath>

HeatRingWidget::HeatRingWidget(QWidget *parent)
    : QWidget(parent), m_currentHeat(0), m_maxHeat(240), m_dualBarrel(false),
      m_barrel1Current(0), m_barrel1Max(240), m_barrel2Current(0),
      m_barrel2Max(240), m_ringSize(120) // 增大环尺寸
      ,
      m_thickness(10) // 稍微增厚
      ,
      m_lowWarningThreshold(0.5f), m_midWarningThreshold(0.75f),
      m_highWarningThreshold(0.9f), m_overheatPenaltyEnabled(true),
      m_heatDecayRate(0.02f), m_animationTimer(new QTimer(this)),
      m_animationPhase(0.0f), m_glowEnabled(true), m_pulseEnabled(true),
      m_shakeEnabled(true), m_particleEnabled(false),
      m_warningFlashEnabled(true), m_breathingEnabled(true),
      m_shakeAnimation(nullptr), m_flashAnimation(nullptr),
      m_pulseAnimation(nullptr), m_warningAnimation(nullptr),
      m_overheatSequence(nullptr), m_opacityEffect(nullptr),
      m_glowEffect(nullptr), m_shakeOffset(0.0f), m_flashOpacity(1.0f),
      m_pulseScale(1.0f), m_warningIntensity(0.0f) {
  setupAnimation();
  setupAnimations();
  setMinimumSize(200, 200); // 增大最小尺寸
  setAttribute(Qt::WA_TransparentForMouseEvents);
}

void HeatRingWidget::setupAnimation() {
  m_animationTimer->setInterval(50); // 20 FPS
  connect(m_animationTimer, &QTimer::timeout, this,
          &HeatRingWidget::onAnimationTimer);
  m_animationTimer->start();
}

void HeatRingWidget::setHeat(int current, int maximum) {
  m_currentHeat = current;
  m_maxHeat = maximum;
  update();
}

void HeatRingWidget::setDualBarrel(bool dual) {
  m_dualBarrel = dual;
  update();
}

void HeatRingWidget::setBarrel1Heat(int current, int maximum) {
  m_barrel1Current = current;
  m_barrel1Max = maximum;
  update();
}

void HeatRingWidget::setBarrel2Heat(int current, int maximum) {
  m_barrel2Current = current;
  m_barrel2Max = maximum;
  update();
}

void HeatRingWidget::setRingSize(int size) {
  m_ringSize = size;
  update();
}

void HeatRingWidget::setThickness(int thickness) {
  m_thickness = thickness;
  update();
}

void HeatRingWidget::setGlowEnabled(bool enabled) {
  m_glowEnabled = enabled;
  if (enabled) {
    applyGlowEffect();
  } else if (m_glowEffect) {
    setGraphicsEffect(nullptr);
    m_glowEffect = nullptr;
  }
}

void HeatRingWidget::setPulseEnabled(bool enabled) {
  m_pulseEnabled = enabled;
  update();
}

void HeatRingWidget::setShakeEnabled(bool enabled) {
  m_shakeEnabled = enabled;
  if (!enabled && m_shakeAnimation) {
    m_shakeAnimation->stop();
    m_shakeOffset = 0.0f;
    update();
  }
}

void HeatRingWidget::setParticleEnabled(bool enabled) {
  m_particleEnabled = enabled;
  if (!enabled) {
    m_particles.clear();
    m_particleLife.clear();
  }
  update();
}

void HeatRingWidget::setWarningFlashEnabled(bool enabled) {
  m_warningFlashEnabled = enabled;
  if (!enabled && m_flashAnimation) {
    m_flashAnimation->stop();
    m_flashOpacity = 1.0f;
    update();
  }
}

void HeatRingWidget::setShakeOffset(float offset) {
  m_shakeOffset = offset;
  update();
}

void HeatRingWidget::setFlashOpacity(float opacity) {
  m_flashOpacity = opacity;
  update();
}

HeatRingWidget::HeatLevel HeatRingWidget::getHeatLevel() const {
  if (m_dualBarrel) {
    HeatLevel level1 = calculateHeatLevel(m_barrel1Current, m_barrel1Max);
    HeatLevel level2 = calculateHeatLevel(m_barrel2Current, m_barrel2Max);
    return (level1 > level2) ? level1 : level2;
  } else {
    return calculateHeatLevel(m_currentHeat, m_maxHeat);
  }
}

void HeatRingWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event)

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // 应用抖动效果
  if (m_shakeOffset != 0.0f) {
    painter.translate(m_shakeOffset, 0);
  }

  // 应用脉冲缩放效果
  if (m_pulseScale != 1.0f) {
    QPointF center = rect().center();
    painter.translate(center);
    painter.scale(m_pulseScale, m_pulseScale);
    painter.translate(-center);
  }

  // 应用闪烁透明度
  if (m_flashOpacity != 1.0f) {
    painter.setOpacity(m_flashOpacity);
  }

  // 绘制热量环
  if (m_dualBarrel) {
    drawDualBarrelRings(painter);
  } else {
    drawSingleBarrelRing(painter);
  }

  // 绘制粒子效果
  if (m_particleEnabled && !m_particles.isEmpty()) {
    drawParticles(painter);
  }

  // 绘制警告指示器
  HeatLevel level = getHeatLevel();
  if (level >= MidWarning) {
    drawWarningIndicators(painter, level);
  }

  // 绘制超热量惩罚提示
  if (level == Overheat && m_overheatPenaltyEnabled) {
    drawOverheatPenalty(painter);
  }

  // 绘制热量衰减指示器
  if (m_heatDecayRate > 0 && level > Normal) {
    drawHeatDecayIndicator(painter);
  }
}

void HeatRingWidget::onAnimationTimer() {
  m_animationPhase += 0.1f;
  if (m_animationPhase > 2 * M_PI) {
    m_animationPhase = 0.0f;
  }

  // 更新粒子系统
  updateParticleSystem();

  // 根据热量级别控制动画
  HeatLevel level = getHeatLevel();
  static HeatLevel lastLevel = Normal;

  // 启动/停止呼吸动画
  if (m_breathingEnabled) {
    if (level >= MidWarning &&
        m_pulseAnimation->state() != QAbstractAnimation::Running) {
      m_pulseAnimation->start();
    } else if (level < MidWarning &&
               m_pulseAnimation->state() == QAbstractAnimation::Running) {
      m_pulseAnimation->stop();
      m_pulseScale = 1.0f;
    }
  }

  // 启动/停止警告动画
  if (level >= HighWarning &&
      m_warningAnimation->state() != QAbstractAnimation::Running) {
    m_warningAnimation->start();
  } else if (level < HighWarning &&
             m_warningAnimation->state() == QAbstractAnimation::Running) {
    m_warningAnimation->stop();
    m_warningIntensity = 0.0f;
  }

  // 触发特殊效果
  if (level == Overheat && lastLevel != Overheat) {
    triggerOverheatShake();
    triggerOverheatPenalty();
  }

  if (level >= MidWarning && lastLevel < MidWarning) {
    triggerWarningFlash();
  }

  lastLevel = level;

  // 只有在需要时才更新显示
  if (level > Normal || !m_particles.isEmpty() ||
      m_pulseAnimation->state() == QAbstractAnimation::Running ||
      m_warningAnimation->state() == QAbstractAnimation::Running) {
    update();
  }
}

void HeatRingWidget::triggerOverheatShake() {
  if (m_shakeEnabled && m_shakeAnimation) {
    m_shakeAnimation->start();
  }
}

void HeatRingWidget::triggerWarningFlash() {
  if (m_warningFlashEnabled && m_flashAnimation) {
    m_flashAnimation->start();
  }
}

void HeatRingWidget::triggerOverheatPenalty() {
  if (m_overheatPenaltyEnabled && m_overheatSequence) {
    m_overheatSequence->start();
  }
}

void HeatRingWidget::updatePulse() { update(); }

void HeatRingWidget::updateWarning() { update(); }

void HeatRingWidget::updateParticleSystem() {
  if (!m_particleEnabled)
    return;

  HeatLevel level = getHeatLevel();

  // 根据热量级别生成不同数量的粒子
  int maxParticles = 0;
  switch (level) {
  case LowWarning:
    maxParticles = 5;
    break;
  case MidWarning:
    maxParticles = 10;
    break;
  case HighWarning:
    maxParticles = 15;
    break;
  case Overheat:
    maxParticles = 25;
    break;
  default:
    maxParticles = 0;
    break;
  }

  // 添加新粒子
  if (m_particles.size() < maxParticles && level > Normal) {
    QPointF center = rect().center();
    float angle = (float)rand() / RAND_MAX * 2 * M_PI;
    float radius = 20 + (float)rand() / RAND_MAX * 30;
    QPointF particle(center.x() + cos(angle) * radius,
                     center.y() + sin(angle) * radius);

    // 粒子速度
    float speed = 0.5f + (float)rand() / RAND_MAX * 1.5f;
    QPointF velocity(cos(angle) * speed, sin(angle) * speed);

    m_particles.append(particle);
    m_particleLife.append(1.0f);
    m_particleVelocity.append(velocity);
  }

  // 更新现有粒子
  for (int i = m_particles.size() - 1; i >= 0; --i) {
    m_particleLife[i] -= m_heatDecayRate;

    // 更新粒子位置
    m_particles[i] += m_particleVelocity[i];

    // 移除死亡粒子
    if (m_particleLife[i] <= 0) {
      m_particles.removeAt(i);
      m_particleLife.removeAt(i);
      m_particleVelocity.removeAt(i);
    }
  }
}

void HeatRingWidget::updateShake() { update(); }

void HeatRingWidget::updateFlash() { update(); }

void HeatRingWidget::drawSingleBarrelRing(QPainter &painter) {
  QPointF center = rect().center();
  int radius = m_ringSize / 2;

  drawHeatRing(painter, m_currentHeat, m_maxHeat, center, radius, m_thickness);
}

void HeatRingWidget::drawDualBarrelRings(QPainter &painter) {
  QPointF center = rect().center();
  int radius = m_ringSize / 2;
  int offset = m_thickness + 4;

  // 内环（枪管 1）
  drawHeatRing(painter, m_barrel1Current, m_barrel1Max, center, radius - offset,
               m_thickness);

  // 外环（枪管 2）
  drawHeatRing(painter, m_barrel2Current, m_barrel2Max, center, radius,
               m_thickness);
}

void HeatRingWidget::drawHeatRing(QPainter &painter, int current, int maximum,
                                  const QPointF &center, int radius,
                                  int thickness) {
  if (maximum <= 0)
    return;

  float percentage = getHeatPercentage(current, maximum);
  QColor heatColor = getHeatColor(current, maximum);

  // 背景环
  QPen bgPen(QColor(255, 255, 255, 30), thickness);
  bgPen.setCapStyle(Qt::RoundCap);
  painter.setPen(bgPen);
  painter.setBrush(Qt::NoBrush);
  painter.drawEllipse(center, radius, radius);

  // 热量环
  if (percentage > 0) {
    QPen heatPen(heatColor, thickness);
    heatPen.setCapStyle(Qt::RoundCap);
    painter.setPen(heatPen);

    // 计算圆弧参数
    int startAngle = -90 * 16; // 从顶部开始
    int spanAngle = percentage * 360 * 16;

    // 过热时叠加脉冲效果
    if (calculateHeatLevel(current, maximum) == Overheat) {
      float pulse = (sin(m_animationPhase) + 1.0f) * 0.5f;
      QColor pulseColor = heatColor;
      pulseColor.setAlpha(100 + pulse * 155);
      heatPen.setColor(pulseColor);
      painter.setPen(heatPen);
    }

    QRectF rect(center.x() - radius, center.y() - radius, radius * 2,
                radius * 2);
    painter.drawArc(rect, startAngle, spanAngle);
  }

  // 热量数值
  painter.setPen(QColor(255, 255, 255));
  QFont font("Arial", 8, QFont::Bold);
  painter.setFont(font);

  QString heatText = QString("%1/%2").arg(current).arg(maximum);
  QRectF textRect(center.x() - 20, center.y() - 6, 40, 12);
  painter.drawText(textRect, Qt::AlignCenter, heatText);
}

QColor HeatRingWidget::getHeatColor(int current, int maximum) const {
  HeatLevel level = calculateHeatLevel(current, maximum);

  switch (level) {
  case Normal:
    return QColor(200, 200, 200, 80); // 浅灰色半透明 - 空环更明显
  case LowWarning:
    return QColor(0, 255, 128); // 亮绿色 - 正常
  case MidWarning:
    return QColor(255, 200, 0); // 亮橙色 - 黄色警示
  case HighWarning:
    return QColor(255, 50, 50); // 亮红色 - 红色警示
  case Overheat:
    return QColor(255, 0, 0); // 纯红色 - 超热量惩罚
  default:
    return QColor(0, 255, 128);
  }
}

QColor HeatRingWidget::getEnhancedHeatColor(int current, int maximum) const {
  QColor baseColor = getHeatColor(current, maximum);
  HeatLevel level = calculateHeatLevel(current, maximum);

  // 为不同级别添加特殊效果
  if (level == Overheat) {
    // 超热量时添加闪烁效果
    float pulse = (sin(m_animationPhase * 3) + 1.0f) * 0.5f;
    baseColor.setAlpha(150 + pulse * 105);
  } else if (level >= MidWarning) {
    // 警告级别时添加呼吸效果
    float breath = (sin(m_animationPhase * 2) + 1.0f) * 0.5f;
    baseColor.setAlpha(200 + breath * 55);
  }

  return baseColor;
}

HeatRingWidget::HeatLevel
HeatRingWidget::calculateHeatLevel(int current, int maximum) const {
  float percentage = getHeatPercentage(current, maximum);

  if (percentage >= 1.0f) {
    return Overheat; // Q1≥Q0 (触发扣血)
  } else if (percentage >= m_highWarningThreshold) {
    return HighWarning; // 3/4Q0<Q1<Q0
  } else if (percentage >= m_midWarningThreshold) {
    return MidWarning; // 1/2Q0<Q1<3/4Q0
  } else if (percentage > 0.0f) {
    return LowWarning; // 0<Q1<1/2Q0
  } else {
    return Normal; // 空环 (Q1=0)
  }
}

float HeatRingWidget::getHeatPercentage(int current, int maximum) const {
  if (maximum <= 0)
    return 0.0f;
  return qMin(1.0f, (float)current / maximum);
}

void HeatRingWidget::setupAnimations() {
  // 过热抖动动画
  m_shakeAnimation = new QPropertyAnimation(this, "shakeOffset");
  m_shakeAnimation->setDuration(200);
  m_shakeAnimation->setStartValue(0.0f);
  m_shakeAnimation->setKeyValueAt(0.25, 3.0f);
  m_shakeAnimation->setKeyValueAt(0.5, -3.0f);
  m_shakeAnimation->setKeyValueAt(0.75, 2.0f);
  m_shakeAnimation->setEndValue(0.0f);
  m_shakeAnimation->setEasingCurve(QEasingCurve::OutBounce);
  connect(m_shakeAnimation, &QPropertyAnimation::valueChanged, this,
          &HeatRingWidget::updateShake);

  // 警告闪烁动画
  m_flashAnimation = new QPropertyAnimation(this, "flashOpacity");
  m_flashAnimation->setDuration(500);
  m_flashAnimation->setStartValue(1.0f);
  m_flashAnimation->setKeyValueAt(0.5, 0.3f);
  m_flashAnimation->setEndValue(1.0f);
  m_flashAnimation->setLoopCount(3);
  connect(m_flashAnimation, &QPropertyAnimation::valueChanged, this,
          &HeatRingWidget::updateFlash);

  // 呼吸脉冲动画
  m_pulseAnimation = new QPropertyAnimation(this, "pulseScale");
  m_pulseAnimation->setDuration(1500);
  m_pulseAnimation->setStartValue(1.0f);
  m_pulseAnimation->setKeyValueAt(0.5, 1.1f);
  m_pulseAnimation->setEndValue(1.0f);
  m_pulseAnimation->setLoopCount(-1); // 无限循环
  m_pulseAnimation->setEasingCurve(QEasingCurve::InOutSine);
  connect(m_pulseAnimation, &QPropertyAnimation::valueChanged, this,
          &HeatRingWidget::updatePulse);

  // 警告强度动画
  m_warningAnimation = new QPropertyAnimation(this, "warningIntensity");
  m_warningAnimation->setDuration(800);
  m_warningAnimation->setStartValue(0.0f);
  m_warningAnimation->setKeyValueAt(0.5, 1.0f);
  m_warningAnimation->setEndValue(0.0f);
  m_warningAnimation->setLoopCount(-1);
  m_warningAnimation->setEasingCurve(QEasingCurve::InOutQuad);
  connect(m_warningAnimation, &QPropertyAnimation::valueChanged, this,
          &HeatRingWidget::updateWarning);

  // 过热惩罚动画序列
  m_overheatSequence = new QSequentialAnimationGroup(this);

  // 第一阶段：强烈抖动
  QPropertyAnimation *intenseShake =
      new QPropertyAnimation(this, "shakeOffset");
  intenseShake->setDuration(100);
  intenseShake->setStartValue(0.0f);
  intenseShake->setKeyValueAt(0.2, 5.0f);
  intenseShake->setKeyValueAt(0.4, -5.0f);
  intenseShake->setKeyValueAt(0.6, 4.0f);
  intenseShake->setKeyValueAt(0.8, -4.0f);
  intenseShake->setEndValue(0.0f);
  intenseShake->setLoopCount(3);

  // 第二阶段：红色闪烁
  QPropertyAnimation *redFlash = new QPropertyAnimation(this, "flashOpacity");
  redFlash->setDuration(200);
  redFlash->setStartValue(1.0f);
  redFlash->setKeyValueAt(0.5, 0.1f);
  redFlash->setEndValue(1.0f);
  redFlash->setLoopCount(5);

  m_overheatSequence->addAnimation(intenseShake);
  m_overheatSequence->addAnimation(redFlash);
}

void HeatRingWidget::applyGlowEffect() {
  if (!m_glowEffect) {
    m_glowEffect = new QGraphicsDropShadowEffect(this);
    m_glowEffect->setBlurRadius(15);
    m_glowEffect->setColor(QColor(255, 100, 100, 180));
    m_glowEffect->setOffset(0, 0);
    setGraphicsEffect(m_glowEffect);
  }
}

void HeatRingWidget::applyShakeEffect() {
  // paintEvent 通过平移画笔应用抖动效果
}

void HeatRingWidget::drawParticles(QPainter &painter) {
  if (!m_particleEnabled || m_particles.isEmpty())
    return;

  painter.save();
  for (int i = 0; i < m_particles.size(); ++i) {
    float life = m_particleLife[i];
    QColor particleColor = getHeatColor(m_currentHeat, m_maxHeat);
    particleColor.setAlphaF(life * 0.8f);

    painter.setPen(QPen(particleColor, 2));
    painter.setBrush(particleColor);

    float size = life * 4;
    QRectF particleRect(m_particles[i].x() - size / 2,
                        m_particles[i].y() - size / 2, size, size);
    painter.drawEllipse(particleRect);
  }
  painter.restore();
}

void HeatRingWidget::setHeatThresholds(float lowWarning, float midWarning,
                                       float highWarning) {
  m_lowWarningThreshold = qBound(0.0f, lowWarning, 1.0f);
  m_midWarningThreshold = qBound(m_lowWarningThreshold, midWarning, 1.0f);
  m_highWarningThreshold = qBound(m_midWarningThreshold, highWarning, 1.0f);
  update();
}

void HeatRingWidget::setOverheatPenalty(bool enabled) {
  m_overheatPenaltyEnabled = enabled;
  if (enabled && isOverheated()) {
    triggerOverheatPenalty();
  }
}

void HeatRingWidget::setHeatDecayRate(float rate) {
  m_heatDecayRate = qBound(0.0f, rate, 1.0f);
}

void HeatRingWidget::setBreathingEnabled(bool enabled) {
  m_breathingEnabled = enabled;
  if (!enabled && m_pulseAnimation) {
    m_pulseAnimation->stop();
    m_pulseScale = 1.0f;
    update();
  }
}

void HeatRingWidget::setPulseScale(float scale) {
  m_pulseScale = scale;
  update();
}

void HeatRingWidget::setWarningIntensity(float intensity) {
  m_warningIntensity = intensity;
  update();
}

bool HeatRingWidget::isOverheated() const {
  if (m_dualBarrel) {
    return (m_barrel1Current >= m_barrel1Max) ||
           (m_barrel2Current >= m_barrel2Max);
  } else {
    return m_currentHeat >= m_maxHeat;
  }
}

void HeatRingWidget::drawWarningIndicators(QPainter &painter, HeatLevel level) {
  if (level < MidWarning)
    return;

  QPointF center = rect().center();
  int radius = m_ringSize + 15;

  // 设置警告颜色
  QColor warningColor;
  switch (level) {
  case MidWarning:
    warningColor = QColor(255, 165, 0, 100 + 50 * m_warningIntensity); // 橙色
    break;
  case HighWarning:
    warningColor = QColor(255, 69, 0, 120 + 80 * m_warningIntensity); // 红橙色
    break;
  case Overheat:
    warningColor = QColor(255, 0, 0, 150 + 100 * m_warningIntensity); // 红色
    break;
  default:
    return;
  }

  painter.setPen(QPen(warningColor, 2));
  painter.setBrush(Qt::NoBrush);

  // 绘制警告三角形
  int triangleCount = (level == MidWarning)    ? 3
                      : (level == HighWarning) ? 6
                                               : 12;
  for (int i = 0; i < triangleCount; ++i) {
    float angle = (2.0f * M_PI * i) / triangleCount + m_animationPhase;
    float x = center.x() + cos(angle) * radius;
    float y = center.y() + sin(angle) * radius;

    QPolygonF triangle;
    triangle << QPointF(x, y - 5) << QPointF(x - 4, y + 3)
             << QPointF(x + 4, y + 3);

    painter.drawPolygon(triangle);
  }
}

void HeatRingWidget::drawOverheatPenalty(QPainter &painter) {
  QPointF center = rect().center();

  // 绘制超热量惩罚文字
  painter.setPen(QPen(QColor(255, 0, 0), 2));
  painter.setFont(QFont("Arial", 12, QFont::Bold));

  QString penaltyText = "OVERHEAT PENALTY";
  QFontMetrics fm(painter.font());
  QRect textRect = fm.boundingRect(penaltyText);

  QPointF textPos(center.x() - textRect.width() / 2,
                  center.y() + m_ringSize + 30);

  // 添加闪烁效果
  int alpha = 255 * (0.5 + 0.5 * sin(m_animationPhase * 3));
  painter.setPen(QPen(QColor(255, 0, 0, alpha), 2));
  painter.drawText(textPos, penaltyText);

  // 绘制惩罚图标（感叹号）
  painter.setPen(QPen(QColor(255, 255, 0, alpha), 3));
  painter.setBrush(QBrush(QColor(255, 0, 0, alpha / 2)));

  QRectF iconRect(center.x() - 8, center.y() - m_ringSize - 25, 16, 20);
  painter.drawEllipse(iconRect);

  painter.setPen(QPen(QColor(255, 255, 255), 2));
  painter.drawText(iconRect, Qt::AlignCenter, "!");
}

void HeatRingWidget::drawHeatDecayIndicator(QPainter &painter) {
  QPointF center = rect().center();
  int radius = m_ringSize - 20;

  // 绘制热量衰减箭头
  painter.setPen(QPen(QColor(0, 255, 0, 150), 2));

  for (int i = 0; i < 4; ++i) {
    float angle = (M_PI / 2 * i) + m_animationPhase * 0.5f;
    float x1 = center.x() + cos(angle) * radius;
    float y1 = center.y() + sin(angle) * radius;
    float x2 = center.x() + cos(angle) * (radius - 10);
    float y2 = center.y() + sin(angle) * (radius - 10);

    painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));

    // 绘制箭头头部
    float arrowAngle1 = angle + M_PI / 6;
    float arrowAngle2 = angle - M_PI / 6;
    float arrowLength = 5;

    QPointF arrow1(x2 + cos(arrowAngle1) * arrowLength,
                   y2 + sin(arrowAngle1) * arrowLength);
    QPointF arrow2(x2 + cos(arrowAngle2) * arrowLength,
                   y2 + sin(arrowAngle2) * arrowLength);

    painter.drawLine(QPointF(x2, y2), arrow1);
    painter.drawLine(QPointF(x2, y2), arrow2);
  }

  // 绘制衰减率文字
  painter.setPen(QPen(QColor(0, 255, 0), 1));
  painter.setFont(QFont("Arial", 8));

  QString decayText = QString("-%1/s").arg(m_heatDecayRate, 0, 'f', 2);
  QFontMetrics fm(painter.font());
  QRect textRect = fm.boundingRect(decayText);

  QPointF textPos(center.x() - textRect.width() / 2,
                  center.y() + textRect.height() / 2);

  painter.drawText(textPos, decayText);
}
