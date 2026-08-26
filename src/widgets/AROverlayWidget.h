// SPDX-License-Identifier: MIT
/**
 * @file AROverlayWidget.h
 * @brief AR 叠加渲染控件
 * @details 在视频画面上渲染机器人信息叠加层，包括血条、等级、增益效果等。
 *          采用透明背景，覆盖在 VideoBackgroundWidget 上方。
 * @author Clear
 * @date 2026-01-09
 * @copyright Copyright (c) 2026 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

#ifndef AROVERLAYWIDGET_H
#define AROVERLAYWIDGET_H

#include "../core/TargetTracker.h"

#include <QList>
#include <QMap>
#include <QPropertyAnimation>
#include <QWidget>

#include "../core/GameData.h"
struct RobotData;

namespace RM {

/**
 * @struct OverlayInfo
 * @brief 单个机器人的叠加信息
 * @details 结合追踪位置和比赛数据
 */
struct OverlayInfo {
  int robotId;       // 机器人ID
  QString name;      // 名称 (如 "步兵3号")
  QRectF screenBox;  // 屏幕坐标边界框
  QPointF topCenter; // 信息板位置 (边界框顶部中心)

  // 比赛数据
  int currentHP;     // 当前血量
  int maxHP;         // 最大血量
  int level;         // 等级
  quint32 buffMask;  // 增益效果掩码
  bool isRed;        // 是否红方
  bool isInvincible; // 是否无敌
  bool isDead;       // 是否阵亡

  // 动画状态
  float opacity;         // 透明度 (淡入淡出)
  float healthAnimValue; // 血条动画值

  OverlayInfo()
      : robotId(0), currentHP(0), maxHP(600), level(1),
        buffMask(0), isRed(true), isInvincible(false), isDead(false),
        opacity(1.0f), healthAnimValue(1.0f) {}
};

/**
 * @class AROverlayWidget
 * @brief AR 叠加渲染控件
 * @details 透明控件，渲染机器人信息叠加层
 */
class AROverlayWidget : public QWidget {
  Q_OBJECT

public:
  explicit AROverlayWidget(QWidget *parent = nullptr);
  ~AROverlayWidget();

  /**
   * @brief 设置比赛数据源
   * @param gameData 比赛数据对象
   */
  void setGameData(GameData *gameData);

  /**
   * @brief 更新追踪目标
   * @param targets 追踪目标列表
   */
  void updateTargets(const QList<TrackedTarget> &targets);

  /**
   * @brief 设置显示模式
   * @param showHealthBar 显示血条
   * @param showLevel 显示等级
   * @param showBuff 显示增益
   */
  void setDisplayOptions(bool showHealthBar, bool showLevel, bool showBuff);

  /**
   * @brief 设置信息板偏移
   * @param offset 相对于边界框顶部的偏移 (像素)
   */
  void setInfoOffset(int offset);

  /**
   * @brief 设置信息板缩放
   * @param scale 缩放比例 (1.0 = 100%)
   */
  void setInfoScale(float scale);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  // 渲染方法
  void drawOverlayInfo(QPainter &painter, const OverlayInfo &info);
  void drawHealthBar(QPainter &painter, const OverlayInfo &info,
                     const QRectF &barRect);
  void drawLevelBadge(QPainter &painter, const OverlayInfo &info,
                      const QPointF &pos);
  void drawBuffIcons(QPainter &painter, const OverlayInfo &info,
                     const QPointF &pos);
  void drawNameTag(QPainter &painter, const OverlayInfo &info,
                   const QPointF &pos);

  // 计算屏幕坐标
  QRectF normalizedToScreen(const QRectF &normalized) const;

  // 获取机器人数据
  OverlayInfo createOverlayInfo(const TrackedTarget &target) const;

  // 获取机器人名称
  QString getRobotName(int robotId) const;

  // 颜色工具
  QColor getTeamColor(bool isRed) const;
  QColor getHealthColor(float healthPercent) const;

private:
  // --- 数据源 ---
  GameData *m_gameData = nullptr;

  // --- 叠加信息 ---
  QList<OverlayInfo> m_overlayInfos;

  // --- 显示选项 ---
  bool m_showHealthBar = true;
  bool m_showLevel = true;
  bool m_showBuff = true;
  int m_infoOffset = 20;    // 信息板偏移 (像素)
  float m_infoScale = 1.0f; // 信息板缩放

  // --- 样式常量 ---
  static constexpr int INFO_WIDTH = 120;      // 信息板宽度
  static constexpr int HEALTH_BAR_HEIGHT = 8; // 血条高度
  static constexpr int LEVEL_BADGE_SIZE = 20; // 等级徽章大小
  static constexpr int BUFF_ICON_SIZE = 16;   // 增益图标大小
};

} // namespace RM

#endif // AROVERLAYWIDGET_H
