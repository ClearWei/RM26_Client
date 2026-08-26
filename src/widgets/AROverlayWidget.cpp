// SPDX-License-Identifier: MIT
/**
 * @file AROverlayWidget.cpp
 * @brief AR 叠加渲染控件实现
 * @author Clear
 * @date 2026-01-09
 * @copyright Copyright (c) 2026 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

#include "AROverlayWidget.h"
#include "../core/GameData.h"

#include <QDebug>
#include <QPainter>
#include <QPainterPath>

namespace RM {

// ============================================================================
// 构造函数和析构函数
// ============================================================================

AROverlayWidget::AROverlayWidget(QWidget *parent) : QWidget(parent) {
  // 设置透明背景
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_TranslucentBackground);
  setStyleSheet("background: transparent;");
}

AROverlayWidget::~AROverlayWidget() = default;

// ============================================================================
// 公共方法
// ============================================================================

void AROverlayWidget::setGameData(GameData *gameData) { m_gameData = gameData; }

void AROverlayWidget::updateTargets(const QList<TrackedTarget> &targets) {
  m_overlayInfos.clear();

  for (const auto &target : targets) {
    OverlayInfo info = createOverlayInfo(target);
    m_overlayInfos.append(info);
  }

  update(); // 触发重绘
}

void AROverlayWidget::setDisplayOptions(bool showHealthBar, bool showLevel,
                                        bool showBuff) {
  m_showHealthBar = showHealthBar;
  m_showLevel = showLevel;
  m_showBuff = showBuff;
  update();
}

void AROverlayWidget::setInfoOffset(int offset) {
  m_infoOffset = offset;
  update();
}

void AROverlayWidget::setInfoScale(float scale) {
  m_infoScale = std::clamp(scale, 0.5f, 2.0f);
  update();
}

// ============================================================================
// 绘制方法
// ============================================================================

void AROverlayWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::TextAntialiasing);

  // 绘制所有叠加信息
  for (const auto &info : m_overlayInfos) {
    drawOverlayInfo(painter, info);
  }
}

void AROverlayWidget::drawOverlayInfo(QPainter &painter,
                                      const OverlayInfo &info) {
  if (info.opacity < 0.1f) {
    return; // 太透明，跳过
  }

  painter.setOpacity(info.opacity);

  // 计算信息板位置 (边界框顶部中心上方)
  QPointF center = info.topCenter;
  float scaledWidth = INFO_WIDTH * m_infoScale;
  float offsetY = m_infoOffset * m_infoScale;

  QPointF infoPos(center.x() - scaledWidth / 2, center.y() - offsetY);

  // 1. 绘制名称标签
  if (!info.name.isEmpty()) {
    drawNameTag(painter, info, QPointF(center.x(), infoPos.y() - 5));
    infoPos.setY(infoPos.y() + 20);
  }

  // 2. 绘制血条
  if (m_showHealthBar) {
    float barHeight = HEALTH_BAR_HEIGHT * m_infoScale;
    QRectF barRect(infoPos.x(), infoPos.y(), scaledWidth, barHeight);
    drawHealthBar(painter, info, barRect);
    infoPos.setY(infoPos.y() + barHeight + 5);
  }

  // 3. 绘制等级徽章
  if (m_showLevel) {
    drawLevelBadge(painter, info, QPointF(infoPos.x(), infoPos.y()));
  }

  // 4. 绘制增益图标
  if (m_showBuff && info.buffMask > 0) {
    float badgeWidth = LEVEL_BADGE_SIZE * m_infoScale + 5;
    drawBuffIcons(painter, info,
                  QPointF(infoPos.x() + badgeWidth, infoPos.y()));
  }

  painter.setOpacity(1.0f);
}

void AROverlayWidget::drawHealthBar(QPainter &painter, const OverlayInfo &info,
                                    const QRectF &barRect) {
  // 背景
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(0, 0, 0, 180));
  painter.drawRoundedRect(barRect, 2, 2);

  // 血量条
  float healthPercent =
      info.maxHP > 0 ? static_cast<float>(info.currentHP) / info.maxHP : 0.0f;
  healthPercent = std::clamp(healthPercent, 0.0f, 1.0f);

  if (healthPercent > 0) {
    QRectF fillRect = barRect;
    fillRect.setWidth(barRect.width() * healthPercent);
    fillRect.adjust(1, 1, -1, -1);

    QColor fillColor = getHealthColor(healthPercent);

    // 渐变效果
    QLinearGradient gradient(fillRect.topLeft(), fillRect.bottomLeft());
    gradient.setColorAt(0, fillColor.lighter(120));
    gradient.setColorAt(0.5, fillColor);
    gradient.setColorAt(1, fillColor.darker(110));

    painter.setBrush(gradient);
    painter.drawRoundedRect(fillRect, 2, 2);
  }

  // 边框
  painter.setPen(QPen(getTeamColor(info.isRed), 1));
  painter.setBrush(Qt::NoBrush);
  painter.drawRoundedRect(barRect, 2, 2);

  // 血量文字
  QString hpText = QString("%1/%2").arg(info.currentHP).arg(info.maxHP);
  QFont font = painter.font();
  font.setPixelSize(static_cast<int>(8 * m_infoScale));
  font.setBold(true);
  painter.setFont(font);
  painter.setPen(Qt::white);
  painter.drawText(barRect, Qt::AlignCenter, hpText);
}

void AROverlayWidget::drawLevelBadge(QPainter &painter, const OverlayInfo &info,
                                     const QPointF &pos) {
  float size = LEVEL_BADGE_SIZE * m_infoScale;
  QRectF badgeRect(pos.x(), pos.y(), size, size);

  // 绘制六边形或圆形徽章
  QColor badgeColor = getTeamColor(info.isRed);

  // 圆形背景
  painter.setPen(QPen(badgeColor.darker(120), 2));
  painter.setBrush(QColor(0, 0, 0, 200));
  painter.drawEllipse(badgeRect);

  // 等级文字
  QFont font = painter.font();
  font.setPixelSize(static_cast<int>(10 * m_infoScale));
  font.setBold(true);
  painter.setFont(font);
  painter.setPen(badgeColor);
  painter.drawText(badgeRect, Qt::AlignCenter, QString::number(info.level));
}

void AROverlayWidget::drawBuffIcons(QPainter &painter, const OverlayInfo &info,
                                    const QPointF &pos) {
  float iconSize = BUFF_ICON_SIZE * m_infoScale;
  float spacing = 3;
  float x = pos.x();

  // 解析增益掩码并绘制图标
  // 简化版本：使用颜色方块表示不同增益

  QStringList buffSymbols;
  if (info.buffMask & 0x01)
    buffSymbols << "⚡"; // 加速
  if (info.buffMask & 0x02)
    buffSymbols << "🛡️"; // 防御
  if (info.buffMask & 0x04)
    buffSymbols << "🔥"; // 攻击
  if (info.buffMask & 0x08)
    buffSymbols << "❤️"; // 恢复

  // 无敌状态
  if (info.isInvincible) {
    buffSymbols << "🌟";
  }

  QFont font = painter.font();
  font.setPixelSize(static_cast<int>(iconSize));
  painter.setFont(font);
  painter.setPen(Qt::white);

  for (const QString &symbol : buffSymbols) {
    painter.drawText(QPointF(x, pos.y() + iconSize), symbol);
    x += iconSize + spacing;
  }
}

void AROverlayWidget::drawNameTag(QPainter &painter, const OverlayInfo &info,
                                  const QPointF &pos) {
  QFont font = painter.font();
  font.setPixelSize(static_cast<int>(12 * m_infoScale));
  font.setBold(true);
  painter.setFont(font);

  QFontMetrics fm(font);
  int textWidth = fm.horizontalAdvance(info.name);

  // 背景
  QRectF bgRect(pos.x() - textWidth / 2 - 5, pos.y() - 15, textWidth + 10, 18);
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(0, 0, 0, 160));
  painter.drawRoundedRect(bgRect, 3, 3);

  // 文字
  painter.setPen(getTeamColor(info.isRed));
  painter.drawText(bgRect, Qt::AlignCenter, info.name);
}

// ============================================================================
// 工具方法
// ============================================================================

QRectF AROverlayWidget::normalizedToScreen(const QRectF &normalized) const {
  return QRectF(normalized.x() * width(), normalized.y() * height(),
                normalized.width() * width(), normalized.height() * height());
}

OverlayInfo
AROverlayWidget::createOverlayInfo(const TrackedTarget &target) const {
  OverlayInfo info;

  info.robotId = target.robotId;
  info.name = getRobotName(target.robotId);
  info.isRed = (target.robotId < 100);

  // 转换边界框到屏幕坐标
  info.screenBox = normalizedToScreen(target.smoothedBox);
  info.topCenter = QPointF(info.screenBox.center().x(), info.screenBox.top());

  // 从 GameData 获取机器人数据
  if (m_gameData) {
    const RobotData *robotData = m_gameData->getRobotById(target.robotId);
    if (robotData) {
      info.currentHP = robotData->currentHP;
      info.maxHP = robotData->maxHP;
      info.level = robotData->level;
      info.buffMask = robotData->buffMask;
      info.isDead = (robotData->status == RobotStatus::DESTROYED);
      info.isInvincible = (robotData->status == RobotStatus::INVINCIBLE);
    }
  }

  // 根据可见状态设置透明度
  info.opacity =
      target.isVisible ? 1.0f : (1.0f - target.framesSinceSeen / 10.0f);
  info.opacity = std::clamp(info.opacity, 0.3f, 1.0f);

  return info;
}

QString AROverlayWidget::getRobotName(int robotId) const {
  // 根据机器人 ID 生成名称
  static const QMap<int, QString> names = {
      {1, "英雄"},   {2, "工程"},    {3, "步兵3"},   {4, "步兵4"},
      {5, "步兵5"},  {6, "空中"},    {7, "哨兵"},    {101, "英雄"},
      {102, "工程"}, {103, "步兵3"}, {104, "步兵4"}, {105, "步兵5"},
      {106, "空中"}, {107, "哨兵"}};

  bool isRed = (robotId < 100);
  QString prefix = isRed ? "红" : "蓝";
  QString typeName = names.value(robotId, QString("机器人%1").arg(robotId));

  return prefix + typeName;
}

QColor AROverlayWidget::getTeamColor(bool isRed) const {
  return isRed ? QColor("#FB2C36") : QColor("#51A2FF");
}

QColor AROverlayWidget::getHealthColor(float healthPercent) const {
  if (healthPercent > 0.6f) {
    return QColor("#00E676"); // 绿色 - 健康
  } else if (healthPercent > 0.3f) {
    return QColor("#FFAB00"); // 橙色 - 中等
  } else {
    return QColor("#FF1744"); // 红色 - 危险
  }
}

} // namespace RM
