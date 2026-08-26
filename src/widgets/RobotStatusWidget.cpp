#include "RobotStatusWidget.h"
#include <QFont>
#include <QPaintEvent>
#include <QRandomGenerator>

RobotStatusWidget::RobotStatusWidget(TeamColor team, int robotId,
                                     QWidget *parent)
    : QWidget(parent), m_team(team), m_robotId(robotId), m_robotType(Infantry),
      m_status(Normal), m_currentHealth(100), m_maxHealth(100),
      m_previousHealth(100), m_level(1), m_currentExp(0), m_maxExp(100),
      m_currentPower(0.0f), m_maxPower(80.0f), m_currentHeat(0), m_maxHeat(240),
      m_blinkTimer(nullptr), m_blinkState(false), m_effectTimer(nullptr),
      m_damageTimer(nullptr), m_healthAnimation(nullptr),
      m_expAnimation(nullptr), m_powerAnimation(nullptr),
      m_heatAnimation(nullptr), m_glowAnimation(nullptr),
      m_shakeAnimation(nullptr), m_animationGroup(nullptr),
      m_healthProgress(1.0), m_expProgress(0.0), m_powerProgress(0.0),
      m_heatProgress(0.0), m_glowIntensity(0.0), m_shakeOffset(0.0),
      m_damageFlash(0.0), m_isInvulnerable(false), m_isReviving(false),
      m_isWarned(false), m_isOffline(false), m_showDamageEffect(false),
      m_buffMask(0) {
  setupColors();
  setupEnhancedAnimations();
}

void RobotStatusWidget::setRobotType(RobotType type) {
  m_robotType = type;
  update();
}

void RobotStatusWidget::drawEnhancedHealthBar(QPainter &painter) {
  QRect healthRect = QRect(10, 25, width() - 20, 8);

  // 背景
  painter.fillRect(healthRect, QColor(40, 40, 40));

  // 血量条
  if (m_maxHealth > 0) {
    QRect healthFill = healthRect;
    healthFill.setWidth(
        static_cast<int>(healthRect.width() * m_healthProgress));

    // 按剩余血量选择颜色
    QColor healthColor;
    if (m_healthProgress > 0.6) {
      healthColor = QColor(0, 200, 0); // 绿色
    } else if (m_healthProgress > 0.3) {
      healthColor = QColor(255, 165, 0); // 橙色
    } else {
      healthColor = QColor(200, 0, 0); // 红色
    }

    // 低血量时增加发光边框
    if (isCriticalHealth() && m_glowIntensity > 0) {
      painter.setPen(QPen(healthColor, 2));
      painter.drawRect(healthFill.adjusted(-1, -1, 1, 1));
    }

    painter.fillRect(healthFill, healthColor);
  }

  // 血量文字
  painter.setPen(m_textColor);
  painter.setFont(QFont("Arial", 7));
  QString healthText = QString("%1/%2").arg(m_currentHealth).arg(m_maxHealth);
  painter.drawText(healthRect, Qt::AlignCenter, healthText);
}

void RobotStatusWidget::drawEnhancedExpBar(QPainter &painter) {
  QRect expRect = QRect(10, 35, width() - 20, 4);

  // 背景
  painter.fillRect(expRect, QColor(30, 30, 30));

  // 经验条
  if (m_maxExp > 0) {
    QRect expFill = expRect;
    expFill.setWidth(static_cast<int>(expRect.width() * m_expProgress));

    QColor expColor = QColor(0, 150, 255); // 蓝色
    painter.fillRect(expFill, expColor);
  }
}

void RobotStatusWidget::drawEnhancedPowerBar(QPainter &painter) {
  QRect powerRect = QRect(10, 42, width() - 20, 4);

  // 背景
  painter.fillRect(powerRect, QColor(30, 30, 30));

  // 功率条
  if (m_maxPower > 0) {
    QRect powerFill = powerRect;
    powerFill.setWidth(static_cast<int>(powerRect.width() * m_powerProgress));

    QColor powerColor =
        isPowerCritical() ? QColor(255, 0, 0) : QColor(255, 255, 0);
    painter.fillRect(powerFill, powerColor);
  }
}

void RobotStatusWidget::drawEnhancedHeatBar(QPainter &painter) {
  QRect heatRect = QRect(10, 49, width() - 20, 4);

  // 背景
  painter.fillRect(heatRect, QColor(30, 30, 30));

  // 热量条
  if (m_maxHeat > 0) {
    QRect heatFill = heatRect;
    heatFill.setWidth(static_cast<int>(heatRect.width() * m_heatProgress));

    QColor heatColor;
    if (m_heatProgress > 0.8) {
      heatColor = QColor(255, 0, 0); // 红色：过热
    } else if (m_heatProgress > 0.6) {
      heatColor = QColor(255, 165, 0); // 橙色
    } else {
      heatColor = QColor(255, 255, 0); // 黄色
    }

    painter.fillRect(heatFill, heatColor);
  }
}

void RobotStatusWidget::drawStatusIndicators(QPainter &painter) {
  int iconSize = 12;
  int x = width() - 15;
  int y = 5;

  // 机器人 ID
  painter.setPen(m_textColor);
  painter.setFont(QFont("Arial", 10, QFont::Bold));
  QString robotText = QString("R%1").arg(m_robotId);
  painter.drawText(QRect(5, 5, 30, 15), Qt::AlignLeft | Qt::AlignVCenter,
                   robotText);

  // 等级
  QString levelText = QString("Lv.%1").arg(m_level);
  painter.drawText(QRect(5, height() - 15, 40, 10),
                   Qt::AlignLeft | Qt::AlignVCenter, levelText);

  // 状态图标
  if (m_status == Dead) {
    painter.fillRect(x - iconSize, y, iconSize, iconSize,
                     QColor(100, 100, 100));
    painter.setPen(QColor(255, 0, 0));
    painter.drawText(QRect(x - iconSize, y, iconSize, iconSize),
                     Qt::AlignCenter, "X");
  }
}

void RobotStatusWidget::drawGlowEffect(QPainter &painter) {
  if (m_glowIntensity <= 0)
    return;

  QRect glowRect = rect().adjusted(-2, -2, 2, 2);
  QColor glowColor = m_teamColor;
  glowColor.setAlpha(static_cast<int>(100 * m_glowIntensity));

  painter.setPen(QPen(glowColor, 3));
  painter.drawRect(glowRect);
}

void RobotStatusWidget::drawDamageEffect(QPainter &painter) {
  if (m_damageFlash <= 0)
    return;

  QColor flashColor = QColor(255, 0, 0);
  flashColor.setAlpha(static_cast<int>(100 * m_damageFlash));

  painter.fillRect(rect(), flashColor);
}

void RobotStatusWidget::drawWarningEffects(QPainter &painter) {
  if (!m_isWarned && !isCriticalHealth() && !isOverheating() &&
      !isPowerCritical())
    return;

  QColor warningColor = QColor(255, 255, 0);
  warningColor.setAlpha(static_cast<int>(50 * m_glowIntensity));

  painter.setPen(QPen(warningColor, 2));
  painter.drawRect(rect().adjusted(1, 1, -1, -1));
}

void RobotStatusWidget::drawInvulnerabilityEffect(QPainter &painter) {
  QColor invulColor = QColor(255, 215, 0); // 金色
  invulColor.setAlpha(static_cast<int>(80 * m_glowIntensity));

  painter.setPen(QPen(invulColor, 3));
  painter.drawRect(rect().adjusted(-1, -1, 1, 1));
}

void RobotStatusWidget::drawReviveEffect(QPainter &painter) {
  QColor reviveColor = QColor(0, 255, 255); // 青色
  reviveColor.setAlpha(static_cast<int>(60 * m_glowIntensity));

  painter.fillRect(rect(), reviveColor);
}

void RobotStatusWidget::drawOfflineEffect(QPainter &painter) {
  QColor offlineColor = QColor(100, 100, 100);
  offlineColor.setAlpha(150);

  painter.fillRect(rect(), offlineColor);

  painter.setPen(QColor(255, 0, 0));
  painter.setFont(QFont("Arial", 8, QFont::Bold));
  painter.drawText(rect(), Qt::AlignCenter, "OFFLINE");
}

void RobotStatusWidget::drawBuffIcons(QPainter &painter) {
  if (m_buffMask == 0 && m_buffEffects.isEmpty())
    return;

  int iconSize = 10;
  int x = 5;
  int y = height() - 25;

  painter.setFont(QFont("Arial", 6));

  // 按位掩码绘制增益图标
  int offset = 0;
  if (m_buffMask & 0x01) { // 血量恢复
    QRect buffRect(x + offset * (iconSize + 2), y, iconSize, iconSize);
    painter.fillRect(buffRect, QColor(0, 255, 0));
    painter.setPen(Qt::black);
    painter.drawText(buffRect, Qt::AlignCenter, "+");
    offset++;
  }
  if (m_buffMask & 0x02) { // 冷却
    QRect buffRect(x + offset * (iconSize + 2), y, iconSize, iconSize);
    painter.fillRect(buffRect, QColor(0, 200, 255));
    painter.setPen(Qt::black);
    painter.drawText(buffRect, Qt::AlignCenter, "C");
    offset++;
  }
  if (m_buffMask & 0x04) { // 防御
    QRect buffRect(x + offset * (iconSize + 2), y, iconSize, iconSize);
    painter.fillRect(buffRect, QColor(255, 255, 0));
    painter.setPen(Qt::black);
    painter.drawText(buffRect, Qt::AlignCenter, "D");
    offset++;
  }
  if (m_buffMask & 0x08) { // 攻击
    QRect buffRect(x + offset * (iconSize + 2), y, iconSize, iconSize);
    painter.fillRect(buffRect, QColor(255, 100, 100));
    painter.setPen(Qt::black);
    painter.drawText(buffRect, Qt::AlignCenter, "A");
    offset++;
  }
  if (m_buffMask & 0x10) { // 无 RFID（惩罚）
    QRect buffRect(x + offset * (iconSize + 2), y, iconSize, iconSize);
    painter.fillRect(buffRect, QColor(100, 100, 100));
    painter.setPen(Qt::red);
    painter.drawText(buffRect, Qt::AlignCenter, "R");
    offset++;
  }

  // 兼容旧版字符串列表
  for (int i = 0; i < m_buffEffects.size() && i < 3; ++i) {
    QRect buffRect(x + (offset + i) * (iconSize + 2), y, iconSize, iconSize);
    painter.fillRect(buffRect, QColor(0, 255, 0, 100));
    painter.setPen(QColor(255, 255, 255));
    painter.drawText(buffRect, Qt::AlignCenter, QString::number(i + 1));
  }
}

void RobotStatusWidget::drawModuleIndicators(QPainter &painter) {
  if (m_moduleStatus.isEmpty())
    return;

  int indicatorSize = 4;
  int x = width() - 20;
  int y = height() - 15;
  int count = 0;

  for (auto it = m_moduleStatus.begin();
       it != m_moduleStatus.end() && count < 4; ++it, ++count) {
    QRect moduleRect(x + (count % 2) * 6, y + (count / 2) * 6, indicatorSize,
                     indicatorSize);
    QColor moduleColor = it.value() ? QColor(0, 255, 0) : QColor(255, 0, 0);
    painter.fillRect(moduleRect, moduleColor);
  }
}

void RobotStatusWidget::setHealth(int current, int maximum) {
  // 检测本次是否受到伤害
  if (current < m_currentHealth) {
    m_showDamageEffect = true;
    m_lastDamageTime = QDateTime::currentDateTime();
    startDamageEffect();
  }

  m_previousHealth = m_currentHealth;
  m_currentHealth = current;
  m_maxHealth = maximum;

  // 启动血量变化动画
  startHealthAnimation();

  // 低血量时启动警告效果
  if (isCriticalHealth()) {
    startWarningEffect();
  }

  update();
}

void RobotStatusWidget::setLevel(int level) {
  m_level = level;
  update();
}

void RobotStatusWidget::setExperience(int current, int maximum) {
  m_currentExp = current;
  m_maxExp = maximum;
  update();
}

void RobotStatusWidget::setPower(float current, float maximum) {
  m_currentPower = current;
  m_maxPower = maximum;
  update();
}

void RobotStatusWidget::setHeat(int current, int maximum) {
  m_currentHeat = current;
  m_maxHeat = maximum;
  update();
}

void RobotStatusWidget::setStatus(RobotStatus status) {
  m_status = status;
  update();
}

void RobotStatusWidget::setBuffEffects(const QStringList &buffs) {
  m_buffEffects = buffs;
  update();
}

void RobotStatusWidget::setBuffMask(uint32_t mask) {
  m_buffMask = mask;
  update();
}

void RobotStatusWidget::setModuleStatus(const QMap<QString, bool> &modules) {
  m_moduleStatus = modules;
  update();
}

void RobotStatusWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);

  // 受击状态下应用抖动效果
  if (m_shakeOffset > 0) {
    painter.translate(
        m_shakeOffset * (QRandomGenerator::global()->bounded(3) - 1),
        m_shakeOffset * (QRandomGenerator::global()->bounded(3) - 1));
  }

  // 绘制背景
  drawBackground(painter);

  // 绘制当前状态对应的特效
  if (m_isInvulnerable) {
    drawInvulnerabilityEffect(painter);
  }

  if (m_isReviving) {
    drawReviveEffect(painter);
  }

  if (m_isOffline) {
    drawOfflineEffect(painter);
  }

  // 绘制发光效果
  if (m_glowIntensity > 0) {
    drawGlowEffect(painter);
  }

  // 绘制机器人图标
  drawRobotIcon(painter);

  // 绘制各项状态条
  drawEnhancedHealthBar(painter);
  drawEnhancedExpBar(painter);
  drawEnhancedPowerBar(painter);
  drawEnhancedHeatBar(painter);

  // 绘制状态指示
  drawStatusIndicators(painter);

  // 绘制增益图标
  drawBuffIcons(painter);

  // 绘制模块状态
  drawModuleIndicators(painter);

  // 绘制警告效果
  if (m_isWarned || isCriticalHealth() || isOverheating() ||
      isPowerCritical()) {
    drawWarningEffects(painter);
  }

  // 绘制受击效果
  if (m_showDamageEffect && m_damageFlash > 0) {
    drawDamageEffect(painter);
  }
}

void RobotStatusWidget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
}

void RobotStatusWidget::onBlinkTimer() {
  m_blinkState = !m_blinkState;
  update();
}

void RobotStatusWidget::setupColors() {
  switch (m_team) {
  case Red:
    m_teamColor = QColor(220, 20, 20);
    m_backgroundColor = QColor(40, 20, 20);
    m_borderColor = QColor(220, 20, 20);
    m_textColor = QColor(255, 255, 255);
    break;
  case Blue:
    m_teamColor = QColor(20, 20, 220);
    m_backgroundColor = QColor(20, 20, 40);
    m_borderColor = QColor(20, 20, 220);
    m_textColor = QColor(255, 255, 255);
    break;
  case Current:
    m_teamColor = QColor(20, 220, 20);
    m_backgroundColor = QColor(20, 40, 20);
    m_borderColor = QColor(20, 220, 20);
    m_textColor = QColor(255, 255, 255);
    break;
  }

  // 增强状态配色
  m_healthColor = QColor(0, 200, 0);
  m_expColor = QColor(0, 150, 255);
  m_powerColor = QColor(255, 255, 0);
  m_heatColor = QColor(255, 165, 0);
  m_warningColor = QColor(255, 255, 0);
  m_criticalColor = QColor(255, 0, 0);
  m_invulnerableColor = QColor(255, 215, 0);
  m_offlineColor = QColor(100, 100, 100);
}

void RobotStatusWidget::drawBackground(QPainter &painter) {
  QRect rect = this->rect();

  // 绘制半透明背景
  QColor bgColor =
      (m_team == Red) ? QColor(80, 20, 20, 100) : QColor(20, 40, 80, 100);
  if (m_isOffline) {
    bgColor = QColor(40, 40, 40, 100);
  }

  painter.fillRect(rect, bgColor);

  // 绘制边框
  QColor borderColor =
      (m_team == Red) ? QColor(255, 100, 100) : QColor(100, 150, 255);
  if (m_isOffline) {
    borderColor = QColor(100, 100, 100);
  }

  painter.setPen(QPen(borderColor, 2));
  painter.drawRect(rect.adjusted(1, 1, -1, -1));
}

void RobotStatusWidget::drawRobotIcon(QPainter &painter) {
  QRect iconRect(5, 5, 40, 40);

  // 绘制机器人类型图标背景
  QColor iconBg =
      (m_team == Red) ? QColor(255, 100, 100, 100) : QColor(100, 150, 255, 100);
  painter.fillRect(iconRect, iconBg);

  // 绘制机器人类型文字
  painter.setPen(Qt::white);
  painter.setFont(QFont("Arial", 8, QFont::Bold));

  QString typeText;
  switch (m_robotType) {
  case Infantry:
    typeText = "步兵";
    break;
  case Hero:
    typeText = "英雄";
    break;
  case Engineer:
    typeText = "工程";
    break;
  case Drone:
    typeText = "无人机";
    break;
  case Sentry:
    typeText = "哨兵";
    break;
  }

  painter.drawText(iconRect, Qt::AlignCenter, typeText);

  // 绘制机器人ID
  painter.setFont(QFont("Arial", 6));
  painter.drawText(iconRect.adjusted(0, 25, 0, 0), Qt::AlignCenter,
                   QString::number(m_robotId));
}

void RobotStatusWidget::drawHealthBar(QPainter &painter) {
  QRect healthRect(50, 8, width() - 60, 12);

  // 绘制血条背景
  painter.fillRect(healthRect, QColor(40, 40, 40));

  // 绘制血条
  float healthRatio = (float)m_currentHealth / m_maxHealth;
  QRect fillRect = healthRect;
  fillRect.setWidth(healthRect.width() * healthRatio);

  QColor healthColor;
  if (healthRatio > 0.6f) {
    healthColor = QColor(100, 255, 100);
  } else if (healthRatio > 0.3f) {
    healthColor = QColor(255, 255, 100);
  } else {
    healthColor = QColor(255, 100, 100);
  }

  painter.fillRect(fillRect, healthColor);

  // 绘制血量文字
  painter.setPen(Qt::white);
  painter.setFont(QFont("Arial", 8));
  QString healthText = QString("%1/%2").arg(m_currentHealth).arg(m_maxHealth);
  painter.drawText(healthRect, Qt::AlignCenter, healthText);
}

void RobotStatusWidget::drawLevelAndExp(QPainter &painter) {
  QRect levelRect(50, 22, 30, 12);
  QRect expRect(85, 22, width() - 95, 12);

  // 绘制等级
  painter.setPen(Qt::white);
  painter.setFont(QFont("Arial", 8, QFont::Bold));
  painter.drawText(levelRect, Qt::AlignCenter, QString("Lv.%1").arg(m_level));

  // 绘制经验条背景
  painter.fillRect(expRect, QColor(40, 40, 40));

  // 绘制经验条
  float expRatio = (float)m_currentExp / m_maxExp;
  QRect expFillRect = expRect;
  expFillRect.setWidth(expRect.width() * expRatio);
  painter.fillRect(expFillRect, QColor(100, 200, 255));

  // 绘制经验文字
  painter.setFont(QFont("Arial", 7));
  QString expText = QString("%1/%2").arg(m_currentExp).arg(m_maxExp);
  painter.drawText(expRect, Qt::AlignCenter, expText);
}

void RobotStatusWidget::drawPowerInfo(QPainter &painter) {
  QRect powerRect(50, 36, width() - 60, 8);

  // 绘制能量条背景
  painter.fillRect(powerRect, QColor(40, 40, 40));

  // 绘制能量条
  float powerRatio = m_currentPower / m_maxPower;
  QRect powerFillRect = powerRect;
  powerFillRect.setWidth(powerRect.width() * powerRatio);

  QColor powerColor =
      isPowerCritical() ? QColor(255, 100, 100) : QColor(100, 255, 200);
  painter.fillRect(powerFillRect, powerColor);

  // 绘制能量文字
  painter.setPen(Qt::white);
  painter.setFont(QFont("Arial", 6));
  QString powerText =
      QString("%.1fJ/%.1fJ").arg(m_currentPower).arg(m_maxPower);
  painter.drawText(powerRect, Qt::AlignCenter, powerText);
}

void RobotStatusWidget::drawHeatInfo(QPainter &painter) {
  QRect heatRect(50, 46, width() - 60, 8);

  // 绘制热量条背景
  painter.fillRect(heatRect, QColor(40, 40, 40));

  // 绘制热量条
  float heatRatio = (float)m_currentHeat / m_maxHeat;
  QRect heatFillRect = heatRect;
  heatFillRect.setWidth(heatRect.width() * heatRatio);

  QColor heatColor;
  if (isOverheating()) {
    heatColor = QColor(255, 50, 50);
  } else if (heatRatio > 0.8f) {
    heatColor = QColor(255, 150, 50);
  } else {
    heatColor = QColor(255, 200, 100);
  }

  painter.fillRect(heatFillRect, heatColor);

  // 绘制热量文字
  painter.setPen(Qt::white);
  painter.setFont(QFont("Arial", 6));
  QString heatText = QString("%1°/%2°").arg(m_currentHeat).arg(m_maxHeat);
  painter.drawText(heatRect, Qt::AlignCenter, heatText);
}

void RobotStatusWidget::drawStatusEffects(QPainter &painter) {
  int x = 5;
  int y = height() - 15;

  // 绘制状态图标
  if (m_isInvulnerable) {
    painter.fillRect(x, y, 10, 10, QColor(255, 255, 100));
    painter.setPen(Qt::black);
    painter.setFont(QFont("Arial", 6));
    painter.drawText(x, y, 10, 10, Qt::AlignCenter, "无敌");
    x += 12;
  }

  if (m_isReviving) {
    painter.fillRect(x, y, 10, 10, QColor(100, 255, 100));
    painter.setPen(Qt::black);
    painter.setFont(QFont("Arial", 6));
    painter.drawText(x, y, 10, 10, Qt::AlignCenter, "复活");
    x += 12;
  }

  if (m_status == Dead) {
    painter.fillRect(x, y, 10, 10, QColor(255, 100, 100));
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 6));
    painter.drawText(x, y, 10, 10, Qt::AlignCenter, "阵亡");
    x += 12;
  }
}

void RobotStatusWidget::drawModuleStatus(QPainter &painter) {
  // 在右下角绘制模块状态指示器
  int moduleSize = 8;
  int spacing = 2;
  int startX = width() - 50;
  int startY = height() - 15;

  QStringList modules = {"云台", "底盘", "发射", "供弹"};

  for (int i = 0; i < modules.size(); ++i) {
    QRect moduleRect(startX + i * (moduleSize + spacing), startY, moduleSize,
                     moduleSize);

    // 假设模块都正常工作
    bool moduleOk = true;
    QColor moduleColor =
        moduleOk ? QColor(100, 255, 100) : QColor(255, 100, 100);

    painter.fillRect(moduleRect, moduleColor);
    painter.setPen(Qt::black);
    painter.setFont(QFont("Arial", 5));
    painter.drawText(moduleRect, Qt::AlignCenter, QString::number(i + 1));
  }
}

QString RobotStatusWidget::getRobotTypeName() const {
  switch (m_robotType) {
  case Hero:
    return "Hero";
  case Engineer:
    return "Engineer";
  case Infantry:
    return "Infantry";
  case Drone:
    return "Drone";
  case Sentry:
    return "Sentry";
  default:
    return "Unknown";
  }
}

bool RobotStatusWidget::isLowHealth() const {
  return m_maxHealth > 0 &&
         (static_cast<float>(m_currentHealth) / m_maxHealth) < 0.2f;
}

bool RobotStatusWidget::needsBlinking() const {
  return isLowHealth() || m_status == Dead || m_status == Reviving;
}

void RobotStatusWidget::setupEnhancedAnimations() {
  // 血量动画
  m_healthAnimation = new QPropertyAnimation(this, "healthProgress");
  m_healthAnimation->setDuration(500);
  m_healthAnimation->setEasingCurve(QEasingCurve::OutCubic);

  // 经验动画
  m_expAnimation = new QPropertyAnimation(this, "expProgress");
  m_expAnimation->setDuration(800);
  m_expAnimation->setEasingCurve(QEasingCurve::OutQuart);

  // 功率动画
  m_powerAnimation = new QPropertyAnimation(this, "powerProgress");
  m_powerAnimation->setDuration(300);
  m_powerAnimation->setEasingCurve(QEasingCurve::OutQuad);

  // 热量动画
  m_heatAnimation = new QPropertyAnimation(this, "heatProgress");
  m_heatAnimation->setDuration(400);
  m_heatAnimation->setEasingCurve(QEasingCurve::OutQuad);

  // 发光动画
  m_glowAnimation = new QPropertyAnimation(this, "glowIntensity");
  m_glowAnimation->setDuration(1000);
  m_glowAnimation->setEasingCurve(QEasingCurve::InOutSine);
  m_glowAnimation->setLoopCount(-1);
  m_glowAnimation->setKeyValueAt(0.0, 0.0);
  m_glowAnimation->setKeyValueAt(0.5, 1.0);
  m_glowAnimation->setKeyValueAt(1.0, 0.0);

  // 抖动动画
  m_shakeAnimation = new QPropertyAnimation(this, "shakeOffset");
  m_shakeAnimation->setDuration(100);
  m_shakeAnimation->setEasingCurve(QEasingCurve::OutBounce);

  // 特效刷新定时器
  m_effectTimer = new QTimer(this);
  m_effectTimer->setInterval(50);
  connect(m_effectTimer, &QTimer::timeout, this,
          &RobotStatusWidget::updateAnimationProgress);
  m_effectTimer->start();

  // 受击效果定时器
  m_damageTimer = new QTimer(this);
  m_damageTimer->setSingleShot(true);
  connect(m_damageTimer, &QTimer::timeout, [this]() {
    m_showDamageEffect = false;
    update();
  });
}

bool RobotStatusWidget::isCriticalHealth() const {
  return m_maxHealth > 0 &&
         (static_cast<float>(m_currentHealth) / m_maxHealth) < 0.2f;
}

bool RobotStatusWidget::isOverheating() const {
  return m_maxHeat > 0 &&
         (static_cast<float>(m_currentHeat) / m_maxHeat) > 0.8f;
}

bool RobotStatusWidget::isPowerCritical() const {
  return m_maxPower > 0 && (m_currentPower / m_maxPower) > 0.9f;
}

void RobotStatusWidget::startHealthAnimation() {
  if (m_healthAnimation && m_maxHealth > 0) {
    qreal targetProgress = static_cast<qreal>(m_currentHealth) / m_maxHealth;
    m_healthAnimation->setStartValue(m_healthProgress);
    m_healthAnimation->setEndValue(targetProgress);
    m_healthAnimation->start();
  }
}

void RobotStatusWidget::startExpAnimation() {
  if (m_expAnimation && m_maxExp > 0) {
    qreal targetProgress = static_cast<qreal>(m_currentExp) / m_maxExp;
    m_expAnimation->setStartValue(m_expProgress);
    m_expAnimation->setEndValue(targetProgress);
    m_expAnimation->start();
  }
}

void RobotStatusWidget::startPowerAnimation() {
  if (m_powerAnimation && m_maxPower > 0) {
    qreal targetProgress = static_cast<qreal>(m_currentPower) / m_maxPower;
    m_powerAnimation->setStartValue(m_powerProgress);
    m_powerAnimation->setEndValue(targetProgress);
    m_powerAnimation->start();
  }
}

void RobotStatusWidget::startHeatAnimation() {
  if (m_heatAnimation && m_maxHeat > 0) {
    qreal targetProgress = static_cast<qreal>(m_currentHeat) / m_maxHeat;
    m_heatAnimation->setStartValue(m_heatProgress);
    m_heatAnimation->setEndValue(targetProgress);
    m_heatAnimation->start();
  }
}

void RobotStatusWidget::startDamageEffect() {
  m_damageFlash = 1.0;
  if (m_shakeAnimation) {
    m_shakeAnimation->setStartValue(0.0);
    m_shakeAnimation->setEndValue(5.0);
    m_shakeAnimation->start();
  }

  if (m_damageTimer) {
    m_damageTimer->start(1000); // 受击效果持续 1 秒
  }
}

void RobotStatusWidget::startWarningEffect() {
  if (m_glowAnimation &&
      m_glowAnimation->state() != QAbstractAnimation::Running) {
    m_glowAnimation->start();
  }
}

void RobotStatusWidget::updateAnimationProgress() {
  // 更新抖动偏移
  if (m_shakeOffset > 0) {
    m_shakeOffset *= 0.9; // 逐步衰减
    if (m_shakeOffset < 0.1) {
      m_shakeOffset = 0;
    }
  }

  update();
}

// 动画属性设置接口
void RobotStatusWidget::setHealthProgress(float value) {
  if (m_healthProgress != value) {
    m_healthProgress = value;
    update();
  }
}

void RobotStatusWidget::setExpProgress(float value) {
  if (m_expProgress != value) {
    m_expProgress = value;
    update();
  }
}

void RobotStatusWidget::setPowerProgress(float value) {
  if (m_powerProgress != value) {
    m_powerProgress = value;
    update();
  }
}

void RobotStatusWidget::setHeatProgress(float value) {
  if (m_heatProgress != value) {
    m_heatProgress = value;
    update();
  }
}

void RobotStatusWidget::setGlowIntensity(float value) {
  if (m_glowIntensity != value) {
    m_glowIntensity = value;
    update();
  }
}

void RobotStatusWidget::setShakeOffset(float value) {
  if (m_shakeOffset != value) {
    m_shakeOffset = value;
    update();
  }
}
