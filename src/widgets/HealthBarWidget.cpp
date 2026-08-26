#include "HealthBarWidget.h"
#include <QDebug> // 调试输出
#include <QFont>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QPaintEvent>
#include <QPainterPath>
#include <QPixmap>
#include <QRadialGradient>

HealthBarWidget::HealthBarWidget(TeamColor team, QWidget *parent)
    : QWidget(parent), m_team(team), m_currentHealth(100), m_maxHealth(100),
      m_virtualShield(0), m_isInvincible(false), m_lowHealthThreshold(0.2f),
      m_blinkState(false), m_animatedHealth(100), m_glowIntensity(0.0f),
      m_shieldOpacity(1.0f), m_damageFlash(0.0f), m_previousHealth(100) {
  setMinimumSize(100, 45); // 允许宽度扩展，固定高度由布局控制
  setupColors();
  setupAnimations();
  loadHealthBarImages();
}

void HealthBarWidget::setHealth(int current, int maximum) {
  if (m_currentHealth != current || m_maxHealth != maximum) {
    m_maxHealth = maximum;

    // 检查伤害 (血量减少)
    if (current < m_previousHealth) {
      startDamageAnimation();
    }

    // 动画显示血量变化
    m_healthAnimation->setStartValue(m_animatedHealth);
    m_healthAnimation->setEndValue(current);
    m_healthAnimation->start();

    m_previousHealth = m_currentHealth;
    m_currentHealth = current;

    // 如果低血量则开始闪烁
    updateBlinkState();

    // 无敌状态下开始发光脉冲
    if (m_isInvincible) {
      startGlowPulse();
    }

    update();
  }
}

void HealthBarWidget::setVirtualShield(int shield) {
  if (m_virtualShield != shield) {
    m_virtualShield = shield;

    // 当添加护盾时开始护盾动画
    if (shield > 0) {
      startShieldAnimation();
    }

    update();
  }
}

void HealthBarWidget::setAnimatedHealth(int health) {
  if (m_animatedHealth != health) {
    m_animatedHealth = health;
    update(); // 触发重绘以显示新的动画血量值
  }
}

void HealthBarWidget::setGlowIntensity(float intensity) {
  if (m_glowIntensity != intensity) {
    m_glowIntensity = intensity;
    update();
  }
}

void HealthBarWidget::setShieldOpacity(float opacity) {
  if (m_shieldOpacity != opacity) {
    m_shieldOpacity = opacity;
    update();
  }
}

void HealthBarWidget::setDamageFlash(float flash) {
  if (m_damageFlash != flash) {
    m_damageFlash = flash;
    update();
  }
}

void HealthBarWidget::setInvincible(bool invincible) {
  if (m_isInvincible != invincible) {
    m_isInvincible = invincible;

    // 根据无敌状态开始或停止发光动画
    startGlowPulse();

    update();
  }
}

void HealthBarWidget::setTeamInfo(const QString &teamName,
                                  const QString &schoolName) {
  m_teamName = teamName;
  m_schoolName = schoolName;
  update();
}

void HealthBarWidget::setTeamLogo(const QString &logoPath) {
  m_logoPath = logoPath;
  if (!logoPath.isEmpty()) {
    m_logoPixmap = QPixmap(logoPath);
    if (!m_logoPixmap.isNull()) {
      // 将 Logo 缩放到合适大小
      m_logoPixmap = m_logoPixmap.scaled(40, 40, Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation);
    }
  } else {
    m_logoPixmap = QPixmap();
  }
  update();
}

void HealthBarWidget::setLowHealthThreshold(float threshold) {
  m_lowHealthThreshold = threshold;
  updateBlinkState();
}

void HealthBarWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::SmoothPixmapTransform);

  QRect r = rect();

  // 1. 绘制底图（失去血量的背景，始终保持全宽）
  // 由用户反馈移除复杂的背景填充，直接使用图片
  p.drawPixmap(r, m_bottomPixmap);

  // 3. 绘制中层图（当前血量填充，按遮罩裁剪）
  if (m_maxHealth > 0 && m_currentHealth > 0) {
    float ratio = static_cast<float>(m_currentHealth) / m_maxHealth;
    ratio = qBound(0.0f, ratio, 1.0f);

    // 计算裁剪区域 (从锚点向外扩展)
    int sourceWidth = m_middlePixmap.width();
    int sourceHeight = m_middlePixmap.height();
    int clipWidth = static_cast<int>(sourceWidth * ratio);

    QRect sourceRect, targetRect;

    if (m_team == Red) {
      // 红方左对齐，从左向右增长
      // 裁剪源图片的左边部分
      sourceRect = QRect(0, 0, clipWidth, sourceHeight);
      // 目标区域也是左侧部分
      int targetClipWidth = static_cast<int>(r.width() * ratio);
      targetRect = QRect(r.left(), r.top(), targetClipWidth, r.height());
    } else {

      // 计算裁剪区域 (从锚点向外扩展)
      int sourceWidth = m_middlePixmap.width();
      int sourceHeight = m_middlePixmap.height();
      int clipWidth = static_cast<int>(sourceWidth * ratio);

      QRect sourceRect, targetRect;

      if (m_team == Red) {
        // 红方左对齐，从左向右增长
        // 裁剪源图片的左边部分
        sourceRect = QRect(0, 0, clipWidth, sourceHeight);
        // 目标区域也是左侧部分
        int targetClipWidth = static_cast<int>(r.width() * ratio);
        targetRect = QRect(r.left(), r.top(), targetClipWidth, r.height());
      } else {
        // 蓝方右对齐，从右向左增长
        // 裁剪源图片的右边部分
        sourceRect = QRect(sourceWidth - clipWidth, 0, clipWidth, sourceHeight);
        // 目标区域也是右侧部分
        int targetClipWidth = static_cast<int>(r.width() * ratio);
        targetRect = QRect(r.right() - targetClipWidth, r.top(),
                           targetClipWidth, r.height());
      }

      // 需要斜角时可用 CompositionMode 做精确遮罩
      // 当前简单增长效果使用 drawPixmap 的源、目标裁剪即可。
      // 中层图自带斜角 Alpha 通道时，源图裁剪同样有效。
      p.drawPixmap(targetRect, m_middlePixmap, sourceRect);
    }

    // 3. 绘制顶图（边框、高光和玻璃效果，始终保持全宽）
    if (!m_topPixmap.isNull()) {
      p.drawPixmap(r, m_topPixmap);
    }
  }

  // 绘制血量文字
  p.setPen(Qt::white);
  QFont f = p.font();
  f.setPixelSize(20);
  f.setBold(true);
  p.setFont(f);

  QRect textRect =
      r.adjusted(m_team == Red ? 20 : 20, 0, m_team == Red ? -20 : -20, 0);
  QString text = QString("%1").arg(m_currentHealth);

  if (m_team == Red) {
    p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);
  } else {
    p.drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, text);
  }
}

void HealthBarWidget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  update();
}

void HealthBarWidget::onBlinkTimer() {
  if (isLowHealth()) {
    m_blinkState = !m_blinkState;
    update();
  }
}

void HealthBarWidget::onAnimationFinished() {
  m_animatedHealth = m_currentHealth;
}

void HealthBarWidget::setupColors() {
  if (m_team == Red) {
    m_teamColor = QColor(220, 50, 50);
    m_backgroundColor = QColor(80, 20, 20);
    m_borderColor = QColor(255, 100, 100);
  } else {
    m_teamColor = QColor(50, 120, 220);
    m_backgroundColor = QColor(20, 40, 80);
    m_borderColor = QColor(100, 150, 255);
  }
  m_textColor = QColor(255, 255, 255);
}

void HealthBarWidget::loadHealthBarImages() {
  // 加载血条图片资源
  // qrc 别名: :/images/red_base_blood/bottom.png,
  // :/images/red_base_blood/middle.png
  QString prefix;
  if (m_team == Red) {
    prefix = ":/images/red_base_blood/";
  } else {
    prefix = ":/images/blue_base_blood/";
  }

  // bottom：失去血量的暗色背景
  m_bottomPixmap = QPixmap(prefix + "bottom.png");
  // middle：当前血量的亮色填充
  m_middlePixmap = QPixmap(prefix + "middle.png");
  // top：顶层遮罩（边框和高光）
  m_topPixmap = QPixmap(prefix + "top.png");

  // 调试输出
  if (m_bottomPixmap.isNull()) {
    qWarning() << "Failed to load bottom pixmap:" << prefix + "bottom.png";
  }
  if (m_middlePixmap.isNull()) {
    qWarning() << "Failed to load middle pixmap:" << prefix + "middle.png";
  }
  if (m_topPixmap.isNull()) {
    qWarning() << "Failed to load top pixmap:" << prefix + "top.png";
  }
}

void HealthBarWidget::setupAnimations() {
  m_blinkTimer = new QTimer(this);
  connect(m_blinkTimer, &QTimer::timeout, this, &HealthBarWidget::onBlinkTimer);
  m_blinkTimer->setInterval(500); // 每 500ms 闪烁一次

  m_healthAnimation = new QPropertyAnimation(this, "animatedHealth");
  m_healthAnimation->setDuration(300);
  connect(m_healthAnimation, &QPropertyAnimation::finished, this,
          &HealthBarWidget::onAnimationFinished);

  // 无敌效果的发光动画
  m_glowAnimation = new QPropertyAnimation(this, "glowIntensity");
  m_glowAnimation->setDuration(1000);
  m_glowAnimation->setLoopCount(-1); // 无限循环
  m_glowAnimation->setKeyValueAt(0, 0.3f);
  m_glowAnimation->setKeyValueAt(0.5f, 1.0f);
  m_glowAnimation->setKeyValueAt(1, 0.3f);

  // 护盾不透明度动画
  m_shieldAnimation = new QPropertyAnimation(this, "shieldOpacity");
  m_shieldAnimation->setDuration(800);
  m_shieldAnimation->setStartValue(0.0f);
  m_shieldAnimation->setEndValue(1.0f);
  m_shieldAnimation->setEasingCurve(QEasingCurve::OutBounce);

  // 伤害闪光动画
  m_damageAnimation = new QPropertyAnimation(this, "damageFlash");
  m_damageAnimation->setDuration(200);
  m_damageAnimation->setStartValue(0.0f);
  m_damageAnimation->setKeyValueAt(0.5f, 1.0f);
  m_damageAnimation->setEndValue(0.0f);

  // 复杂效果的动画组
  m_animationGroup = new QParallelAnimationGroup(this);
}

void HealthBarWidget::drawBackground(QPainter &painter) {
  QRect bgRect = rect().adjusted(2, 2, -2, -2);

  // 绘制背景渐变
  QLinearGradient bgGradient(0, 0, 0, height());
  bgGradient.setColorAt(0, m_backgroundColor.lighter(120));
  bgGradient.setColorAt(1, m_backgroundColor.darker(120));

  painter.setBrush(bgGradient);
  painter.setPen(QPen(m_borderColor, 2));
  painter.drawRoundedRect(bgRect, 8, 8);
}

void HealthBarWidget::drawHealthBar(QPainter &painter) {
  QRect healthRect = getHealthBarRect();

  if (m_maxHealth <= 0)
    return;

  // 计算血量百分比
  float healthPercent = static_cast<float>(m_animatedHealth) / m_maxHealth;
  int healthWidth = static_cast<int>(healthRect.width() * healthPercent);

  QRect filledRect = healthRect;
  filledRect.setWidth(healthWidth);

  // 血条渐变
  QLinearGradient healthGradient(0, filledRect.top(), 0, filledRect.bottom());

  if (isLowHealth() && m_blinkState) {
    // 低血量闪烁效果
    healthGradient.setColorAt(0, m_teamColor.lighter(150));
    healthGradient.setColorAt(1, m_teamColor.darker(150));
  } else {
    healthGradient.setColorAt(0, m_teamColor.lighter(130));
    healthGradient.setColorAt(1, m_teamColor.darker(130));
  }

  painter.setBrush(healthGradient);
  painter.setPen(Qt::NoPen);
  painter.drawRoundedRect(filledRect, 4, 4);

  // 绘制血条边框
  painter.setBrush(Qt::NoBrush);
  painter.setPen(QPen(m_borderColor, 1));
  painter.drawRoundedRect(healthRect, 4, 4);
}

void HealthBarWidget::drawVirtualShield(QPainter &painter) {
  QRect shieldRect = getShieldBarRect();

  // 护盾渐变
  QLinearGradient shieldGradient(0, shieldRect.top(), 0, shieldRect.bottom());
  shieldGradient.setColorAt(0, QColor(100, 200, 255, 180));
  shieldGradient.setColorAt(1, QColor(50, 150, 255, 180));

  painter.setBrush(shieldGradient);
  painter.setPen(QPen(QColor(150, 220, 255), 1));
  painter.drawRoundedRect(shieldRect, 4, 4);
}

void HealthBarWidget::drawInvincibleEffect(QPainter &painter) {
  QRect effectRect = rect().adjusted(1, 1, -1, -1);

  // 金色发光效果
  QPen glowPen(QColor(255, 215, 0), 3);
  glowPen.setStyle(Qt::DashLine);

  QFont font = painter.font();
  font.setPointSize(12);
  font.setBold(true);
  painter.setFont(font);

  QString healthText = QString("%1/%2").arg(m_currentHealth).arg(m_maxHealth);
  if (m_virtualShield > 0) {
    healthText += QString(" (+%1)").arg(m_virtualShield);
  }

  QRect textRect = rect().adjusted(10, -25, -10, -5);
  painter.drawText(textRect, Qt::AlignRight | Qt::AlignBottom, healthText);
}

void HealthBarWidget::updateBlinkState() {
  if (isLowHealth()) {
    if (!m_blinkTimer->isActive()) {
      m_blinkTimer->start();
    }
  } else {
    m_blinkTimer->stop();
    m_blinkState = false;
  }
}

QRect HealthBarWidget::getHealthBarRect() const {
  return rect().adjusted(10, height() - 35, -10, -15);
}

QRect HealthBarWidget::getShieldBarRect() const {
  QRect healthRect = getHealthBarRect();
  return healthRect.adjusted(0, -8, 0, -healthRect.height() - 2);
}

bool HealthBarWidget::isLowHealth() const {
  return m_maxHealth > 0 && (float)m_currentHealth / m_maxHealth < 0.2f;
}

void HealthBarWidget::drawEnhancedGlow(QPainter &painter) {
  if (m_glowIntensity <= 0.0f)
    return;

  painter.save();

  // 创建金色发光效果
  QColor glowColor = QColor(255, 215, 0, (int)(255 * m_glowIntensity * 0.6f));
  QPen glowPen(glowColor, 3);
  painter.setPen(glowPen);

  // 绘制多层发光
  for (int i = 1; i <= 3; ++i) {
    QRect glowRect = rect().adjusted(-i * 2, -i * 2, i * 2, i * 2);
    painter.drawRoundedRect(glowRect, 8, 8);
  }

  painter.restore();
}

void HealthBarWidget::drawDamageEffect(QPainter &painter) {
  if (m_damageFlash <= 0.0f)
    return;

  // 禁用闪烁效果 (根据用户反馈)
  return;

  /*
  painter.save();

  // 红色闪光覆盖
  QColor flashColor = QColor(255, 0, 0, (int)(255 * m_damageFlash * 0.4f));
  painter.fillRect(rect(), flashColor);

  painter.restore();
  */
}

void HealthBarWidget::drawBreathingEffect(QPainter &painter) {
  painter.save();

  // 根据时间计算呼吸强度
  static qint64 startTime = QDateTime::currentMSecsSinceEpoch();
  qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
  float breathingPhase = sin((currentTime - startTime) * 0.003f) * 0.5f + 0.5f;

  // 绘制低血量时的红色脉冲边框
  QColor breathingColor = QColor(255, 0, 0, (int)(255 * breathingPhase * 0.5f));
  QPen breathingPen(breathingColor, 2);
  painter.setPen(breathingPen);
  painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 6, 6);

  painter.restore();
}

void HealthBarWidget::startDamageAnimation() {
  if (m_damageAnimation->state() == QAbstractAnimation::Running) {
    m_damageAnimation->stop();
  }

  m_damageAnimation->start();
}

void HealthBarWidget::startShieldAnimation() {
  if (m_shieldAnimation->state() == QAbstractAnimation::Running) {
    m_shieldAnimation->stop();
  }

  m_shieldAnimation->start();
}

void HealthBarWidget::startGlowPulse() {
  if (m_isInvincible &&
      m_glowAnimation->state() != QAbstractAnimation::Running) {
    m_glowAnimation->start();
  } else if (!m_isInvincible &&
             m_glowAnimation->state() == QAbstractAnimation::Running) {
    m_glowAnimation->stop();
    setGlowIntensity(0.0f);
  }
}
