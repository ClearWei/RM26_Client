#include "RobotStatusPanel.h"
#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace RM {

RobotStatusPanel::RobotStatusPanel(Team team, QWidget *parent)
    : QWidget(parent), m_team(team), m_robotId(1),
      m_robotType(RobotType::Standard), m_level(1), m_currentHealth(600),
      m_maxHealth(600), m_status(RobotStatus::Alive), m_hovered(false),
      m_selected(false) {
  setFixedHeight(MainLayout::SIDE_CARD_HEIGHT);
  setMinimumWidth(MainLayout::SIDE_WIDTH - 20);

  updateStyle();
}

void RobotStatusPanel::setRobotId(int id) {
  if (m_robotId != id) {
    m_robotId = id;
    update();
  }
}

void RobotStatusPanel::setRobotType(RobotType type) {
  if (m_robotType != type) {
    m_robotType = type;
    update();
  }
}

void RobotStatusPanel::setLevel(int level) {
  if (m_level != level) {
    m_level = level;
    update();
  }
}

void RobotStatusPanel::setHealth(int current, int max) {
  if (m_currentHealth != current || m_maxHealth != max) {
    m_currentHealth = current;
    m_maxHealth = max;
    update();
  }
}

void RobotStatusPanel::setStatus(RobotStatus status) {
  if (m_status != status) {
    m_status = status;
    updateStyle();
    update();
  }
}

void RobotStatusPanel::setBuffs(const QList<BuffType> &buffs) {
  if (m_buffs != buffs) {
    m_buffs = buffs;
    update();
  }
}

void RobotStatusPanel::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  QRect r = rect();

  // 斜切背景
  QPolygon bgPoly;
  int slant = 10;
  bgPoly << r.topLeft() << r.topRight() + QPoint(-slant, 0) << r.bottomRight()
         << r.bottomLeft() + QPoint(slant, 0);

  QColor bgColor =
      (m_team == Red) ? QColor(50, 0, 0, 150) : QColor(0, 0, 50, 150);
  if (m_currentHealth <= 0)
    bgColor = QColor(20, 20, 20, 150); // 阵亡状态颜色

  painter.setPen(QPen(Qt::white, 1));
  painter.setBrush(bgColor);
  painter.drawPolygon(bgPoly);

  // 机器人 ID
  painter.setPen(Qt::white);
  QFont f = painter.font();
  f.setBold(true);
  f.setPixelSize(16);
  painter.setFont(f);
  painter.drawText(r.adjusted(0, 5, 0, -20), Qt::AlignCenter,
                   QString::number(m_robotId));

  // 机器人类型图标
  static QPixmap redIcon(":/images/red_message_guard.png");
  static QPixmap blueIcon(":/images/blue_teammate_avatar_hero_adbuff.png");

  QPixmap *icon = (m_team == Red) ? &redIcon : &blueIcon;
  if (!icon->isNull()) { // 缩小图标，避免与 ID 重叠
    QSize iconSize(35, 20);
    QPixmap scaled =
        icon->scaled(iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // 图标在下半区域居中
    int x = r.center().x() - scaled.width() / 2;
    int y = r.center().y() - scaled.height() / 2 + 8; // 下移以避开 ID

    painter.drawPixmap(x, y, scaled);
  } else {
    // 图标缺失时回退为文字
    f.setPixelSize(10);
    painter.setFont(f);
    QString typeStr = "STD";
    switch (m_robotType) {
    case RM::RobotType::Hero:
      typeStr = "HERO";
      break;
    case RM::RobotType::Engineer:
      typeStr = "ENG";
      break;
    case RM::RobotType::Sentry:
      typeStr = "SEN";
      break;
    case RM::RobotType::Aerial:
      typeStr = "AIR";
      break;
    default:
      break;
    }
    painter.drawText(r.adjusted(0, 20, 0, -5), Qt::AlignCenter, typeStr);
  }

  // 底部血量条
  if (m_maxHealth > 0) {
    int barHeight = 4;
    QRect barRect(r.left() + slant + 2, r.bottom() - barHeight - 2,
                  r.width() - slant * 2 - 4, barHeight);

    // 背景
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(100, 100, 100));
    painter.drawRect(barRect);

    // 填充
    float ratio = (float)m_currentHealth / m_maxHealth;
    QRect fillRect = barRect;
    fillRect.setWidth(barRect.width() * ratio);

    QColor hpColor = (m_team == Red) ? Qt::red : Qt::blue;
    if (ratio < 0.3)
      hpColor = Qt::yellow;

    painter.setBrush(hpColor);
    painter.drawRect(fillRect);
  }
}

void RobotStatusPanel::mousePressEvent(QMouseEvent *event) {
  Q_UNUSED(event)
  m_selected = !m_selected;
  update();
  emit robotSelected(m_robotId);
}

void RobotStatusPanel::enterEvent(QEnterEvent *event) {
  Q_UNUSED(event)
  m_hovered = true;
  update();
}

void RobotStatusPanel::leaveEvent(QEvent *event) {
  Q_UNUSED(event)
  m_hovered = false;
  update();
}

void RobotStatusPanel::updateStyle() {
  // 根据队伍设置基础颜色
  if (m_team == Red) {
    m_textColor = Colors::RED_TEAM;
  } else {
    m_textColor = Colors::BLUE_TEAM;
  }

  // 根据状态设置边框颜色
  switch (m_status) {
  case RobotStatus::Alive:
    m_borderColor = Colors::SUCCESS_GREEN;
    m_backgroundColor = Colors::PANEL_BACKGROUND;
    break;
  case RobotStatus::Dead:
    m_borderColor = Colors::DANGER_RED;
    m_backgroundColor = Colors::PANEL_BACKGROUND.darker(150);
    break;
  case RobotStatus::Respawning:
    m_borderColor = Colors::WARNING_YELLOW;
    m_backgroundColor = Colors::PANEL_BACKGROUND;
    break;
  case RobotStatus::Invincible:
    m_borderColor = Colors::WARNING_YELLOW;
    m_backgroundColor = Colors::PANEL_BACKGROUND;
    break;
  case RobotStatus::Penalty:
    m_borderColor = Colors::DANGER_RED;
    m_backgroundColor = Colors::PANEL_BACKGROUND;
    break;
  case RobotStatus::Disconnected:
    m_borderColor = Colors::NEUTRAL_GRAY;
    m_backgroundColor = Colors::PANEL_BACKGROUND.darker(200);
    break;
  }
}

QString RobotStatusPanel::getRobotTypeIcon() const {
  switch (m_robotType) {
  case RobotType::Standard:
    return "步";
  case RobotType::Hero:
    return "英";
  case RobotType::Engineer:
    return "工";
  case RobotType::Aerial:
    return "空";
  case RobotType::Sentry:
    return "哨";
  case RobotType::Dart:
    return "镖";
  case RobotType::Gate:
    return "门";
  case RobotType::Radar:
    return "雷";
  case RobotType::Outpost:
    return "哨";
  case RobotType::Base:
    return "基";
  default:
    return "?";
  }
}

QString RobotStatusPanel::getRobotTypeName() const {
  switch (m_robotType) {
  case RobotType::Standard:
    return "步兵";
  case RobotType::Hero:
    return "英雄";
  case RobotType::Engineer:
    return "工程";
  case RobotType::Aerial:
    return "空中";
  case RobotType::Sentry:
    return "哨兵";
  case RobotType::Dart:
    return "飞镖";
  case RobotType::Gate:
    return "闸门";
  case RobotType::Radar:
    return "雷达";
  case RobotType::Outpost:
    return "前哨";
  case RobotType::Base:
    return "基地";
  default:
    return "未知";
  }
}

QColor RobotStatusPanel::getStatusColor() const {
  switch (m_status) {
  case RobotStatus::Alive:
    return Colors::SUCCESS_GREEN;
  case RobotStatus::Dead:
    return Colors::DANGER_RED;
  case RobotStatus::Respawning:
    return Colors::WARNING_YELLOW;
  case RobotStatus::Invincible:
    return Colors::WARNING_YELLOW;
  case RobotStatus::Penalty:
    return Colors::DANGER_RED;
  case RobotStatus::Disconnected:
    return Colors::NEUTRAL_GRAY;
  default:
    return Colors::TEXT_PRIMARY;
  }
}

void RobotStatusPanel::drawBuffIcons(QPainter &painter) {
  int iconSize = 16;
  int margin = 2;
  int startX = width() - (m_buffs.size() * (iconSize + margin)) - margin;
  int startY = margin;

  for (int i = 0; i < m_buffs.size(); ++i) {
    QRect buffRect(startX + i * (iconSize + margin), startY, iconSize,
                   iconSize);

    // BUFF图标背景
    painter.setBrush(Colors::WARNING_YELLOW);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(buffRect, 3, 3);

    // BUFF图标文字
    painter.setPen(Colors::BACKGROUND_MAIN);
    QFont buffFont("Roboto", Fonts::SIZE_SMALL - 2, Fonts::WEIGHT_BOLD);
    painter.setFont(buffFont);
    painter.drawText(buffRect, Qt::AlignCenter, "B");
  }
}

} // namespace RM
