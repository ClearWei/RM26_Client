
#include "CrosshairWidget.h"
#include <QPainter>

CrosshairWidget::CrosshairWidget(QWidget *parent)
    : QWidget(parent), m_type(Default), m_color(Qt::green), m_size(40),
      m_thickness(2), m_gap(5), m_visible(true), m_glowEnabled(false),
      m_pulseEnabled(false), m_hitMarkerEnabled(true),
      m_dynamicSizeEnabled(false), m_accuracy(1.0f), m_pulseAnimation(nullptr),
      m_hitMarkerAnimation(nullptr), m_opacityEffect(nullptr),
      m_glowEffect(nullptr), m_pulseScale(1.0f), m_hitMarkerVisible(false),
      m_currentSize(40), m_opacity(1.0) {
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setFixedSize(200, 200); // 增大组件尺寸
  setupAnimations();
}

void CrosshairWidget::setCrosshairType(CrosshairType type) {
  m_type = type;
  update();
}

void CrosshairWidget::setVisible(bool visible) {
  m_visible = visible;
  QWidget::setVisible(visible);
}

void CrosshairWidget::setColor(const QColor &color) {
  m_color = color;
  update();
}

void CrosshairWidget::setSize(int size) {
  m_size = size;
  update();
}

void CrosshairWidget::setThickness(int thickness) {
  m_thickness = thickness;
  update();
}

void CrosshairWidget::setGap(int gap) {
  m_gap = gap;
  update();
}

void CrosshairWidget::setGlowEnabled(bool enabled) {
  m_glowEnabled = enabled;
  if (m_glowEffect) {
    m_glowEffect->setEnabled(enabled);
  }
  update();
}

void CrosshairWidget::setPulseEnabled(bool enabled) {
  m_pulseEnabled = enabled;
  if (m_pulseAnimation) {
    if (enabled) {
      m_pulseAnimation->start();
    } else {
      m_pulseAnimation->stop();
      m_pulseScale = 1.0f;
    }
  }
  update();
}

void CrosshairWidget::setHitMarkerEnabled(bool enabled) {
  m_hitMarkerEnabled = enabled;
  update();
}

void CrosshairWidget::setDynamicSize(bool enabled) {
  m_dynamicSizeEnabled = enabled;
  update();
}

void CrosshairWidget::setAccuracy(float accuracy) {
  m_accuracy = qBound(0.0f, accuracy, 1.0f);
  update();
}

void CrosshairWidget::setPulseScale(float scale) {
  m_pulseScale = scale;
  update();
}

void CrosshairWidget::setOpacity(qreal opacity) {
  m_opacity = opacity;
  update();
}

// 公共槽函数
void CrosshairWidget::showHitMarker() {
  if (!m_hitMarkerEnabled)
    return;

  m_hitMarkerVisible = true;
  if (m_hitMarkerAnimation) {
    m_hitMarkerAnimation->start();
  }
  update();
}

void CrosshairWidget::triggerPulse() {
  if (!m_pulseEnabled || !m_pulseAnimation)
    return;

  m_pulseAnimation->stop();
  m_pulseScale = 1.0f;
  m_pulseAnimation->start();
}

// 私有槽函数
void CrosshairWidget::updatePulse() { update(); }

void CrosshairWidget::hideHitMarker() {
  m_hitMarkerVisible = false;
  update();
}

void CrosshairWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  if (!m_visible)
    return;

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // 按配置叠加发光效果
  if (m_glowEnabled) {
    applyGlowEffect(painter);
  }

  // 根据动态尺寸和精度计算当前大小
  m_currentSize = calculateDynamicSize();

  switch (m_type) {
  case Default:
    drawDefaultCrosshair(painter);
    break;
  case Sniper:
    drawSniperCrosshair(painter);
    break;
  case Shotgun:
    drawShotgunCrosshair(painter);
    break;
  }

  // 命中标记启用时绘制提示
  if (m_hitMarkerVisible && m_hitMarkerEnabled) {
    painter.setPen(QPen(Qt::red, m_thickness + 1));
    int markerSize = m_currentSize / 3;
    QPoint center = rect().center();

    // 绘制 X 形命中标记
    painter.drawLine(center.x() - markerSize, center.y() - markerSize,
                     center.x() + markerSize, center.y() + markerSize);
    painter.drawLine(center.x() + markerSize, center.y() - markerSize,
                     center.x() - markerSize, center.y() + markerSize);
  }
}

void CrosshairWidget::setupAnimations() {
  // 呼吸动画
  m_pulseAnimation = new QPropertyAnimation(this, "pulseScale");
  m_pulseAnimation->setDuration(500);
  m_pulseAnimation->setStartValue(1.0f);
  m_pulseAnimation->setEndValue(1.3f);
  m_pulseAnimation->setEasingCurve(QEasingCurve::OutQuad);
  m_pulseAnimation->setLoopCount(-1);
  m_pulseAnimation->setDirection(QPropertyAnimation::Forward);

  connect(m_pulseAnimation, &QPropertyAnimation::valueChanged, this,
          &CrosshairWidget::updatePulse);

  // 命中标记淡出动画
  m_hitMarkerAnimation = new QPropertyAnimation(this, "opacity");
  m_hitMarkerAnimation->setDuration(300);
  m_hitMarkerAnimation->setStartValue(1.0);
  m_hitMarkerAnimation->setEndValue(0.0);
  m_hitMarkerAnimation->setEasingCurve(QEasingCurve::OutQuad);

  connect(m_hitMarkerAnimation, &QPropertyAnimation::finished, this,
          &CrosshairWidget::hideHitMarker);

  // 发光效果
  m_glowEffect = new QGraphicsDropShadowEffect(this);
  m_glowEffect->setBlurRadius(10);
  m_glowEffect->setColor(m_color);
  m_glowEffect->setOffset(0, 0);
  m_glowEffect->setEnabled(false);
  setGraphicsEffect(m_glowEffect);
}

void CrosshairWidget::applyGlowEffect(QPainter &painter) {
  if (m_glowEffect) {
    m_glowEffect->setColor(m_color);
  }
}

int CrosshairWidget::calculateDynamicSize() const {
  if (!m_dynamicSizeEnabled) {
    return static_cast<int>(m_size * m_pulseScale);
  }

  // 精度越低，准星扩散范围越大
  float accuracyFactor = 1.0f + (1.0f - m_accuracy) * 0.5f;
  return static_cast<int>(m_size * m_pulseScale * accuracyFactor);
}

void CrosshairWidget::drawDefaultCrosshair(QPainter &painter) {
  painter.setPen(QPen(m_color, 2));

  int cx = width() / 2;
  int cy = height() / 2;
  int s = m_size;

  // 中心十字保持原始尺寸
  painter.drawLine(cx - s / 2, cy, cx + s / 2, cy);
  painter.drawLine(cx, cy - s / 2, cx, cy + s / 2);

  // 外弧使用远大于中心十字的半径，形成左右分离的弧线。
  int arcRadius =
      s * 75; // 保留原始初始化，实际绘制半径在下方统一设置。
  arcRadius = s * 100;

  QRect arcRect(cx - arcRadius, cy - arcRadius, arcRadius * 2, arcRadius * 2);
  painter.setPen(QPen(QColor(255, 255, 255, 100), 5)); // 加粗弧线

  // 左侧弧以 180° 为中心，跨度 120°；Qt 角度单位为 1/16°。
  painter.drawArc(arcRect, 120 * 16, 120 * 16);

  // 右侧弧以 0° 为中心，同样绘制 120°。
  painter.drawArc(arcRect, -60 * 16, 120 * 16);

  // 右侧文字随外弧半径调整位置
  painter.setPen(Qt::white);
  QFont f = painter.font();
  f.setPixelSize(12);
  painter.setFont(f);

  int textX = cx + arcRadius + 20;
  painter.drawText(textX, cy - 20, "射击初速度: 28m/s");
  painter.drawText(textX, cy + 20, "允许发弹量: 500");
}

void CrosshairWidget::drawSniperCrosshair(QPainter &painter) {
  painter.setPen(QPen(m_color, m_thickness));

  QPoint center = rect().center();
  int halfSize = m_currentSize / 2;

  // 绘制带发光效果的瞄准圆环
  if (m_glowEnabled) {
    QPen glowPen(m_color, m_thickness + 2);
    glowPen.setColor(m_color.lighter(150));
    painter.setPen(glowPen);
    painter.drawEllipse(center.x() - halfSize - 1, center.y() - halfSize - 1,
                        m_currentSize + 2, m_currentSize + 2);
    painter.setPen(QPen(m_color, m_thickness));
  }

  painter.drawEllipse(center.x() - halfSize, center.y() - halfSize,
                      m_currentSize, m_currentSize);

  // 绘制随呼吸动画缩放的中心点
  int dotSize = m_pulseEnabled ? static_cast<int>(2 * m_pulseScale) : 2;
  painter.fillRect(center.x() - dotSize / 2, center.y() - dotSize / 2, dotSize,
                   dotSize, m_color);
}

void CrosshairWidget::drawShotgunCrosshair(QPainter &painter) {
  painter.setPen(QPen(m_color, m_thickness));

  QPoint center = rect().center();
  int halfSize = m_currentSize / 2;

  // 按精度绘制放射状散布线
  int numLines = 8;
  float spreadFactor = m_dynamicSizeEnabled ? (2.0f - m_accuracy) : 1.0f;

  for (int i = 0; i < numLines; ++i) {
    double angle = i * M_PI / 4;
    int innerRadius = static_cast<int>((halfSize - m_gap) * spreadFactor);
    int outerRadius = static_cast<int>(halfSize * spreadFactor);

    int x1 = center.x() + static_cast<int>(innerRadius * cos(angle));
    int y1 = center.y() + static_cast<int>(innerRadius * sin(angle));
    int x2 = center.x() + static_cast<int>(outerRadius * cos(angle));
    int y2 = center.y() + static_cast<int>(outerRadius * sin(angle));

    // 为每条散布线叠加发光效果
    if (m_glowEnabled) {
      QPen glowPen(m_color.lighter(150), m_thickness + 1);
      painter.setPen(glowPen);
      painter.drawLine(x1, y1, x2, y2);
      painter.setPen(QPen(m_color, m_thickness));
    }

    painter.drawLine(x1, y1, x2, y2);
  }
}
