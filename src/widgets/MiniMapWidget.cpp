#include "MiniMapWidget.h"
#include "../ui/LayoutConstants.h"
#include <QFont>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QtMath>

namespace RM {

MiniMapWidget::MiniMapWidget(QWidget *parent)
    : QWidget(parent), m_centerActive(false), m_currentRobotId(-1),
      m_breathTimer(nullptr), m_breathPhase(0.0f) {
  setFixedSize(270, 160);
  setAttribute(Qt::WA_TranslucentBackground);

  // --- 初始化前哨战状态 ---
  m_isOutpostDead[true] = false;  // 红方
  m_isOutpostDead[false] = false; // 蓝方

  // --- 初始化基地攻击状态 ---
  m_isBaseAtacked[true] = false;  // 红方基地未被攻击
  m_isBaseAtacked[false] = false; // 蓝方基地未被攻击

  // --- 初始化呼吸动画定时器（信号与槽的绑定） ---
  m_breathTimer = new QTimer(this);
  connect(m_breathTimer, &QTimer::timeout, this, [this]() {
    // 每次增加 0.1 弧度
    m_breathPhase += 0.1f;
    // 防止溢出
    if (m_breathPhase >= 2 * M_PI) {
      m_breathPhase -= 2 * M_PI;
    }
    // 触发重绘
    update();
  });
  m_breathTimer->start(10); // 10ms 更新一次

  // --- 基地警报停止定时器（红/蓝阵营） ---
  m_redBaseTimer = new QTimer(this);
  m_redBaseTimer->setSingleShot(true);
  connect(m_redBaseTimer, &QTimer::timeout, this, [this]() {
    m_isBaseAtacked[true] = false;
    update();
  });

  m_blueBaseTimer = new QTimer(this);
  m_blueBaseTimer->setSingleShot(true);
  connect(m_blueBaseTimer, &QTimer::timeout, this, [this]() {
    m_isBaseAtacked[false] = false;
    update();
  });
}

void MiniMapWidget::updateRobotPosition(int id, bool isRedTeam,
                                        const QPointF &pos, float angle,
                                        int is_high_light) {
  RobotMarker m{isRedTeam, pos, angle, is_high_light};
  // 使用偏移量区分红蓝双方ID，防止覆盖
  int key = isRedTeam ? id : id + 7;
  m_markers[key] = m;
  update();
}

void MiniMapWidget::updateRobotState(int id, bool isRedTeam, float currentHP) {
  // 使用偏移量区分红蓝双方ID，防止覆盖
  int key = isRedTeam ? id : id + 7;
  m_markers[key].isDead = currentHP <= 0.0f;
  update();
}

void MiniMapWidget::updateBaseState(bool isRedTeam) {
  m_isBaseAtacked[isRedTeam] = true;
  update();

  // 重启对应阵营的 6 秒倒计时
  if (isRedTeam) {
    if (m_redBaseTimer)
      m_redBaseTimer->start(6000);
  } else {
    if (m_blueBaseTimer)
      m_blueBaseTimer->start(6000);
  }
}

void MiniMapWidget::updateOutpostState(bool isRedTeam, float currentHP) {
  m_isOutpostDead[isRedTeam] = currentHP <= 0.0f;
  update();
}

void MiniMapWidget::updateCenterPoint(bool active) {
  m_centerActive = active;
  update(); // 是否是攻击时触发的信号
}

void MiniMapWidget::setCurrentRobotId(int robotId) {
  m_currentRobotId = robotId;
  update();
}

void MiniMapWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event)
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  QRect r = rect();

  //---局部变量---
  float scale = 1.0f / 3.0f;   // 地图元素缩放比例
  float relativeRedX = 0.06f;  // 固定组件相对位置（redX）
  float relativeBlueX = 0.94f; // 固定组件相对位置（blueX）
  float breathFactor =
      1.0f + 0.2f * qSin(m_breathPhase); // 呼吸动画缩放比例(用于雷达锁定动画)

  // 1.背景设置
  // 绘制背景图
  QPixmap bg(":/images/minimap_bg.png");

  if (!bg.isNull()) {
    p.save();
    p.setOpacity(0.7);
    p.drawPixmap(r, bg);
    p.restore();
  } else {
    // 资源缺失时回退到基础绘制
    p.setPen(QPen(QColor(190, 198, 216), 1));
    p.setBrush(QColor(26, 31, 46, 100));
    p.drawRoundedRect(r.adjusted(0, 0, -1, -1), 8, 8);
  }

  // 网格线已移除
  /*
  int cols = 8;
  int rows = 6;
  p.setPen(QColor(80, 86, 102));
  for (int c = 1; c < cols; ++c) {
    int x = r.left() + c * r.width() / cols;
    p.drawLine(x, r.top(), x, r.bottom());
  }
  for (int rIdx = 1; rIdx < rows; ++rIdx) {
    int y = r.top() + rIdx * r.height() / rows;
    p.drawLine(r.left(), y, r.right(), y);
  }
  */

  // 2.固定组件设置
  // 绘制前哨站
  for (auto it = m_isOutpostDead.begin(); it != m_isOutpostDead.end(); ++it) {
    p.save();
    if (it.key() == true) {
      if (it.value()) {
        p.setOpacity(0.5);
      } else {
        p.setOpacity(1.0);
      }
      drawMapElement(p, r, ":/images/minimap/red_map_outpost.png", 0.37f, 0.81f,
                     0.5f);
    } else {
      if (it.value()) {
        p.setOpacity(0.5);
      } else {
        p.setOpacity(1.0);
      }
      drawMapElement(p, r, ":/images/minimap/blue_map_outpost.png", 0.63f,
                     0.19f, 0.5f);
    }
    p.restore();
  }

  // 绘制前哨站背景
  drawMapElement(p, r, ":/images/minimap/red_map_outpost_bg.png", 0.34f, 0.81f,
                 0.5f);
  drawMapElement(p, r, ":/images/minimap/blue_map_outpost_bg.png", 0.66f, 0.19f,
                 0.5f);

  // 绘制基地区域
  drawMapElement(p, r, ":/images/minimap/red_map_base_area.png", relativeRedX,
                 0.5f, scale);
  drawMapElement(p, r, ":/images/minimap/blue_map_base_area.png", relativeBlueX,
                 0.5f, scale);

  // 绘制基地图标
  drawMapElement(p, r, ":/images/minimap/red_map_base.png", relativeRedX, 0.5f,
                 scale);
  drawMapElement(p, r, ":/images/minimap/blue_map_base.png", relativeBlueX,
                 0.5f, scale);

  // 3.显示机器人位置
  // 绘制机器人标记
  for (auto it = m_markers.begin(); it != m_markers.end(); ++it) {
    const RobotMarker &mk = it.value();
    // 还原逻辑 ID (如果是蓝方 >7，则减去 7)
    int storageKey = it.key();
    int robotId = (storageKey > 7) ? (storageKey - 7) : storageKey;
    float relScale = scale;

    // 根据是否被锁定绘出机器人
    // 雷达锁定
    if (mk.isHighLight) {
      relScale *= breathFactor;
      drawRobotMarker(p, r, mk, robotId, relScale);
      drawMapElement(p, r, ":/images/minimap/map_robot_lockline.png", mk.pos.x(),
                     mk.pos.y(), relScale * 3.0f / 4.0f);
    }
    // 未被锁定
    else {
      drawRobotMarker(p, r, mk, robotId, scale);
    }
  }

  // 4.中心点设置
  // 绘制中心点
  QPointF cp(r.center());
  QColor centerC =
      m_centerActive ? QColor(255, 214, 10) : QColor(160, 160, 160);
  p.setPen(Qt::NoPen);
  p.setBrush(centerC);
  p.drawEllipse(cp, 6, 6);

  // 5.文本设置
  // 绘制文字
  p.setPen(QColor(229, 229, 234));
  QFont f("Roboto", 8, QFont::Bold);
  p.setFont(f);
  p.drawText(rect().adjusted(8, 6, -8, -6), Qt::AlignTop | Qt::AlignLeft,
             "MINI MAP");

  // 6.基地被攻击时警报提醒
  for (auto it = m_isBaseAtacked.begin(); it != m_isBaseAtacked.end(); ++it) {
    bool isRed = it.key();
    bool isAttacked = it.value();
    float relativeX = isRed ? relativeRedX : relativeBlueX;

    if (isAttacked) {
      float relScale = scale * breathFactor;
      drawMapElement(p, r,
                     isRed ? ":/images/minimap/red_map_base_animation_1.png"
                           : ":/images/minimap/blue_map_base_animation_1.png",
                     relativeX, 0.5f, relScale);
      drawMapElement(p, r,
                     isRed ? ":/images/minimap/red_map_base_animation_2.png"
                           : ":/images/minimap/blue_map_base_animation_2.png",
                     relativeX, 0.5f, 1.2 * relScale);
      drawMapElement(p, r,
                     isRed ? ":/images/minimap/red_map_base_animation_3.png"
                           : ":/images/minimap/blue_map_base_animation_3.png",
                     relativeX, 0.5f, 1.3 * relScale);
      drawMapElement(p, r,
                     isRed ? ":/images/minimap/red_map_base_animation_4.png"
                           : ":/images/minimap/blue_map_base_animation_4.png",
                     relativeX, 0.5f, 1.4 * relScale);
    }
  }
}

void MiniMapWidget::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    // 计算归一化坐标（0.0-1.0）
    float x = static_cast<float>(event->pos().x()) / width();
    float y = static_cast<float>(event->pos().y()) / height();

    // 将坐标限制在 [0, 1] 范围内
    x = std::max(0.0f, std::min(1.0f, x));
    y = std::max(0.0f, std::min(1.0f, y));

    emit mapClicked(x, y);
  }
  QWidget::mousePressEvent(event);
}

void MiniMapWidget::drawMapElement(QPainter &p, const QRect &r,
                                   const QString &imagePath, float relX,
                                   float relY, float scale) {
  QPixmap pix(imagePath);
  if (!pix.isNull()) {
    int w = pix.width() * scale * r.width() / 270.0f;
    int h = pix.height() * scale * r.height() / 160.0f;
    // 中心对齐绘制:
    int drawX = r.left() + (relX * r.width()) - (w / 2);
    int drawY = r.top() + (relY * r.height()) - (h / 2);
    p.drawPixmap(drawX, drawY, w, h, pix);
  }
}


void MiniMapWidget::drawRobotMarker(QPainter &p, const QRect &r, const RobotMarker &mk, const int robotId,float scale){
    //判断机器人是否死亡（死亡设置透明度）
    if(mk.isDead){
      p.setOpacity(0.5);
    }
    else {
      p.setOpacity(1.0);
    }

    // 绘制位置标记
    if(robotId!=6){
      float robotScale=mk.isHighLight?scale*3.0f/7.0f:scale*3.0f/3.5f;      //机器人缩放比例(图片像素不同)
      bool isSelfRobot=(robotId == m_currentRobotId);     //判断是否是当前操控机器人

      const QString &robotPath=isSelfRobot ? ":/images/minimap/self_map_robot.png"
                                          :(mk.isRed ? ":/images/minimap/red_map_robot.png"
                                          :":/images/minimap/blue_map_robot.png");
      const QString &highlightPath=mk.isRed ? ":/images/minimap/red_map_robot_lockbg.png"
                                          : ":/images/minimap/blue_map_robot_lockbg.png";
      drawMapElement(p,r,mk.isHighLight?highlightPath:robotPath,mk.pos.x(),mk.pos.y(),robotScale);
    }

    // 绘制朝向箭头
    QPixmap arrow(mk.isRed ? ":/images/minimap/red_map_arrow.png"
                                             : ":/images/minimap/blue_map_arrow.png");
    if(robotId!=6){
      if(robotId == m_currentRobotId)
        arrow.load(":/images/minimap/self_map_arrow.png");

      if (!arrow.isNull()) {
          p.save();
          //计算绝对位置，便于确定方向
          float realX=r.left() + mk.pos.x() * r.width();
          float realY=r.top() + mk.pos.y() * r.height();
          p.translate(realX,realY);                     //到达中心位置
          p.rotate(mk.angle);                           //旋转，显示方向

          // 引入相对于基准尺寸(270x160)的视图缩放系数
          float viewScaleW = r.width() / 270.0f;
          float viewScaleH = r.height() / 160.0f;
          int w = arrow.width() * scale * viewScaleW;
          int h = arrow.height() * scale * viewScaleH;

          int drawX = -(w / 2);
          int drawY = -(h / 2) - 1.4 * h;
          p.drawPixmap(drawX, drawY, w, h, arrow);
          p.restore();
      }
    }

    // 绘制 ID、雷达或哨兵标识
    QString imagePath;
    switch(robotId){
    case 1:
      imagePath=":/images/minimap/id_1.png";
      break;
    case 2:
      imagePath=":/images/minimap/id_2.png";
      break;
    case 3:
      imagePath=":/images/minimap/id_3.png";
      break;
    case 4:
      imagePath=":/images/minimap/id_4.png";
      break;
    case 5:
      imagePath=":/images/minimap/id_5.png";
      break;
    case 6:
      imagePath=mk.isRed?":/images/minimap/red_map_airplane.png":":/images/minimap/blue_map_airplane.png";
      break;
    case 7:
      imagePath=mk.isRed?":/images/minimap/red_map_guard.png":":/images/minimap/blue_map_guard.png";
      break;
  }

  // 绘制位置标记
  if (robotId != 6) {
    float robotScale =
        mk.isHighLight ? scale * 3.0f / 7.0f
                       : scale * 3.0f / 3.5f; // 机器人缩放比例(图片像素不同)
    bool isSelfRobot =
        (robotId == m_currentRobotId); // 判断是否是当前操控机器人

    const QString &robotPath =
        isSelfRobot ? ":/images/minimap/self_map_robot.png"
                    : (mk.isRed ? ":/images/minimap/red_map_robot.png"
                                : ":/images/minimap/blue_map_robot.png");
    const QString &highlightPath =
        mk.isRed ? ":/images/minimap/red_map_robot_lockbg.png"
                 : ":/images/minimap/blue_map_robot_lockbg.png";
    drawMapElement(p, r, mk.isHighLight ? highlightPath : robotPath, mk.pos.x(),
                   mk.pos.y(), robotScale);
  }

  const QString &idPath = imagePath;
  drawMapElement(p, r, imagePath, mk.pos.x(), mk.pos.y(), scale);
}
} // namespace RM
