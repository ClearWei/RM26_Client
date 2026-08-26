#ifndef LAYOUTCONSTANTS_H
#define LAYOUTCONSTANTS_H

#include <QColor>
#include <QtGlobal>

namespace RM {

// 主界面规格
struct MainLayout {
  // 基础尺寸（作为参考基准）
  static constexpr int BASE_WIDTH = 1470;
  static constexpr int BASE_HEIGHT = 956;
  static constexpr int MARGIN = 20;

  // 动态尺寸计算函数
  static int getCanvasWidth(int screenWidth) {
    return qMax(static_cast<int>(screenWidth * 0.85), 1280);
  }

  static int getCanvasHeight(int screenHeight) {
    return qMax(static_cast<int>(screenHeight * 0.85), 720);
  }

  // 根据屏幕分辨率动态计算布局尺寸

  static int getBaseHealthWidth(int screenWidth) {
    double scaleFactor = static_cast<double>(screenWidth) / 1920.0;
    return static_cast<int>(840 * qBound(0.8, scaleFactor, 1.3));
  }

  static int getBaseHealthHeight(int screenHeight) {
    double scaleFactor = static_cast<double>(screenHeight) / 1080.0;
    return static_cast<int>(84 * qBound(0.8, scaleFactor, 1.3));
  }

  // 布局边距和间距常量
  static constexpr int LAYOUT_MARGIN = 15;
  static constexpr int LAYOUT_MARGIN_TOP = 10;
  static constexpr int LAYOUT_MARGIN_BOTTOM = 5;
  static constexpr int LAYOUT_SPACING_SMALL = 8;
  static constexpr int LAYOUT_SPACING_MEDIUM = 10;
  static constexpr int LAYOUT_SPACING_LARGE = 15;

  // 标签边距和间距
  static constexpr int LABEL_MARGIN_H = 5;
  static constexpr int LABEL_MARGIN_V = 2;
  static constexpr int LABEL_SPACING = 8;

  // 控制元素间距
  static constexpr int BASE_CONTROL_SPACING = 5;
  static constexpr int CONTROL_SPACING = 3;
  static constexpr int HEALTH_BAR_SPACING = 2;

  // 中央区域间距
  static constexpr int CENTER_SPACING = 4;
  static constexpr int TIME_SPACING = 1;

  // 机器人缩略图间距
  static constexpr int ROBOT_THUMB_SPACING = 4;

  // 战区状态区域
  static constexpr int WARZONE_WIDTH = 180;
  static constexpr int WARZONE_HEIGHT = 50;
  static constexpr int WARZONE_MARGIN_H = 8;
  static constexpr int WARZONE_MARGIN_V = 3;
  static constexpr int WARZONE_SPACING = 4;

  // 顶部区域 (两行布局：第一行校徽校名+血条+胜利点，第二行机器人图标+血条)
  static constexpr int TOP_AREA_Y = 10;
  static constexpr int TOP_AREA_HEIGHT =
      100; // 适配 -50 间距的紧凑布局

  // 根据屏幕分辨率动态计算布局尺寸
  static int getTopAreaHeight(int screenHeight) {
    double scaleFactor = static_cast<double>(screenHeight) / 1080.0;
    return static_cast<int>(
        170 *
        qBound(0.8, scaleFactor, 1.5)); // 保持顶部两行完整，同时减少顶部区域占高
  }
  static constexpr int BASE_HEALTH_WIDTH = 840;       // 官方宽度：840像素
  static constexpr int BASE_HEALTH_HEIGHT = 64;       // 官方高度：84像素
  static constexpr int BASE_HEALTH_VALUE_WIDTH = 280; // 血量数值显示宽度
  static constexpr int BASE_HEALTH_VALUE_HEIGHT = 64; // 血量数值显示高度

  // 校徽尺寸 - 放大到48x48像素
  static constexpr int EMBLEM_SIZE = 48;
  static constexpr int EMBLEM_RADIUS = 24;

  // 中央信息区域
  static constexpr int CENTER_INFO_WIDTH = 280; // 优化后的中央区域宽度
  static constexpr int TIME_FONT_SIZE = 32;     // 时间显示字体大小
  static constexpr int ROUND_FONT_SIZE = 11;    // 回合显示字体大小

  // 计分板
  static constexpr int SCORE_BOARD_WIDTH = 691;
  static constexpr int SCORE_BOARD_HEIGHT = 60;

  // 侧边栏
  static constexpr int SIDE_WIDTH = 300;
  static constexpr int SIDE_CARD_HEIGHT = 96;
  static constexpr int SIDE_CARD_MARGIN = 2;

  // 底部信息栏
  static constexpr int BOTTOM_BAR_HEIGHT = 40;

  // 中心区域
  static constexpr int CROSSHAIR_SIZE = 20;
  static constexpr int HEAT_RING_OUTER = 120;
  static constexpr int HEAT_RING_THICKNESS = 6;
  static constexpr int CENTER_BOOST_Y_OFFSET = 40;

  // 机器人缩略图规格
  static constexpr int ROBOT_THUMB_WIDTH = 75;  // 机器人缩略图宽度
  static constexpr int ROBOT_THUMB_HEIGHT = 60; // 机器人缩略图高度
  static constexpr int ROBOT_ICON_SIZE = 35;    // 机器人图标大小
  static constexpr int ROBOT_HEALTH_HEIGHT = 5; // 机器人血条高度
  static constexpr int STATUS_LIGHT_SIZE = 8;   // 状态指示灯大小
};

// 颜色规范
struct Colors {
  // 主色调
  static constexpr auto RED_TEAM = QColor(255, 69, 58);       // #FF453A
  static constexpr auto BLUE_TEAM = QColor(10, 132, 255);     // #0A84FF
  static constexpr auto NEUTRAL_GRAY = QColor(142, 142, 147); // #8E8E93

  // 状态颜色
  static constexpr auto WARNING_YELLOW = QColor(255, 214, 10); // #FFD60A
  static constexpr auto DANGER_RED = QColor(255, 69, 58);      // #FF453A
  static constexpr auto SUCCESS_GREEN = QColor(48, 209, 88);   // #30D158

  // 界面颜色
  static constexpr auto BACKGROUND_MAIN = QColor(10, 14, 26);  // #0A0E1A
  static constexpr auto PANEL_BACKGROUND = QColor(26, 31, 46); // #1A1F2E
  static constexpr auto TEXT_PRIMARY = QColor(229, 229, 234);  // #E5E5EA
  static constexpr auto BORDER_COLOR = QColor(58, 65, 81);     // #3A4151
};

// 样式规范
struct Styles {
  static constexpr int BORDER_RADIUS_SMALL = 8;
  static constexpr int BORDER_RADIUS_MEDIUM = 12;
  static constexpr int BORDER_RADIUS_LARGE = 15;
  static constexpr int BORDER_WIDTH = 1;
  static constexpr int PADDING_SMALL = 5;
  static constexpr int PADDING_MEDIUM = 10;
  static constexpr int MARGIN_SMALL = 2;
  static constexpr int MARGIN_MEDIUM = 5;
};

// 字体规范
struct Fonts {
  // 基础字体大小（基于1920x1080分辨率）
  static constexpr int SIZE_SMALL = 12;
  static constexpr int SIZE_MEDIUM = 14;
  static constexpr int SIZE_LARGE = 16;
  static constexpr int SIZE_XLARGE = 18;
  static constexpr int SIZE_SCORE = 20;
  static constexpr int SIZE_TIMER = 24;
  static constexpr int SIZE_HEALTH_VALUE = 36; // 血量数值
  static constexpr int SIZE_TIME_DISPLAY = 32; // 时间显示
  static constexpr int SIZE_ROUND_LABEL = 11;  // 回合标签
  static constexpr int SIZE_ROBOT_LABEL = 10;  // 机器人标签
  static constexpr int SIZE_STATUS_LABEL = 8;  // 状态标签

  static constexpr int WEIGHT_NORMAL = 400;
  static constexpr int WEIGHT_MEDIUM = 500;
  static constexpr int WEIGHT_BOLD = 700;

  // 字体族
  static constexpr const char *FONT_FAMILY = "Microsoft YaHei"; // 微软雅黑
  static constexpr const char *FONT_FAMILY_BACKUP = "SimHei";   // 黑体备选
};

// 机器人角色类型
enum class RobotType {
  Standard, // 步兵
  Hero,     // 英雄
  Engineer, // 工程
  Aerial,   // 空中
  Sentry,   // 哨兵
  Dart,     // 飞镖
  Gate,     // 闸门
  Radar,    // 雷达
  Outpost,  // 前哨站
  Base      // 基地
};

// 机器人状态
enum class RobotStatus {
  Alive,       // 存活
  Dead,        // 阵亡
  Respawning,  // 复活中
  Invincible,  // 无敌
  Penalty,     // 处罚
  Disconnected // 断线
};

// BUFF类型
enum class BuffType {
  AddHp,        // 回血
  Shield,       // 护盾
  DamageBoost,  // 伤害加成
  DefenseBoost, // 防御加成
  Cooling,      // 冷却加速
  Invincible,   // 无敌
  DebuffShield, // 伤害加深
  Rune          // 符文效果
};

} // namespace RM

#endif // LAYOUTCONSTANTS_H
