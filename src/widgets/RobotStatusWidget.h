#ifndef ROBOTSTATUSWIDGET_H
#define ROBOTSTATUSWIDGET_H

#include <QColor>
#include <QDateTime>
#include <QEasingCurve>
#include <QFont>
#include <QFontMetrics>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QMap>
#include <QPainter>
#include <QParallelAnimationGroup>
#include <QPixmap>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QStringList>
#include <QTimer>
#include <QWidget>

class RobotStatusWidget : public QWidget {
  Q_OBJECT
  Q_PROPERTY(
      float healthProgress READ getHealthProgress WRITE setHealthProgress)
  Q_PROPERTY(float expProgress READ getExpProgress WRITE setExpProgress)
  Q_PROPERTY(float powerProgress READ getPowerProgress WRITE setPowerProgress)
  Q_PROPERTY(float heatProgress READ getHeatProgress WRITE setHeatProgress)
  Q_PROPERTY(float glowIntensity READ getGlowIntensity WRITE setGlowIntensity)
  Q_PROPERTY(float shakeOffset READ getShakeOffset WRITE setShakeOffset)

public:
  enum TeamColor {
    Red,
    Blue,
    Current // 当前操控机器人
  };

  enum RobotType {
    Hero = 1,
    Engineer = 2,
    Infantry = 3,
    Drone = 4,
    Sentry = 7
  };

  enum RobotStatus {
    Normal,
    Dead,
    Reviving,
    Invincible,
    Punished,
    YellowCard,
    RedCard,
    Disconnected
  };

  explicit RobotStatusWidget(TeamColor team, int robotId,
                             QWidget *parent = nullptr);

  // 设置接口
  void setRobotType(RobotType type);
  void setHealth(int current, int maximum);
  void setLevel(int level);
  void setExperience(int current, int maximum);
  void setPower(float current, float maximum);
  void setHeat(int current, int maximum);
  void setStatus(RobotStatus status);
  void setBuffEffects(const QStringList &buffs);
  void setBuffMask(uint32_t mask);
  void setModuleStatus(const QMap<QString, bool> &modules);

  // 查询接口
  int getRobotId() const { return m_robotId; }
  TeamColor getTeam() const { return m_team; }
  RobotType getRobotType() const { return m_robotType; }
  RobotStatus getStatus() const { return m_status; }

  // 动画属性接口
  float getHealthProgress() const { return m_healthProgress; }
  void setHealthProgress(float value);

  float getExpProgress() const { return m_expProgress; }
  void setExpProgress(float value);

  float getPowerProgress() const { return m_powerProgress; }
  void setPowerProgress(float value);

  float getHeatProgress() const { return m_heatProgress; }
  void setHeatProgress(float value);

  float getGlowIntensity() const { return m_glowIntensity; }
  void setGlowIntensity(float value);

  float getShakeOffset() const { return m_shakeOffset; }
  void setShakeOffset(float value);

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private slots:
  void onBlinkTimer();

private:
  // --- 基础标识 ---
  TeamColor m_team;      // 队伍颜色 (红/蓝)
  int m_robotId;         // 机器人ID
  RobotType m_robotType; // 机器人类型
  RobotStatus m_status;  // 当前状态

  // --- 血量数据 ---
  int m_currentHealth;  // 当前血量
  int m_maxHealth;      // 最大血量
  int m_previousHealth; // 上一次血量 (检测伤害用)

  // --- 等级与经验 ---
  int m_level;      // 当前等级 (1-3)
  int m_currentExp; // 当前经验值
  int m_maxExp;     // 升级所需经验值

  // --- 功率数据 ---
  float m_currentPower; // 当前功率 (W)
  float m_maxPower;     // 最大功率 (W)

  // --- 热量数据 ---
  int m_currentHeat; // 当前热量
  int m_maxHeat;     // 热量上限

  // --- 增益效果 ---
  QStringList m_buffEffects; // 增益效果名称列表
  uint32_t m_buffMask;       // 增益效果位掩码

  // --- 模块状态 ---
  QMap<QString, bool> m_moduleStatus; // 模块在线状态映射

  // --- 定时器 ---
  QTimer *m_blinkTimer;  // 闪烁定时器
  bool m_blinkState;     // 当前闪烁状态
  QTimer *m_effectTimer; // 特效定时器
  QTimer *m_damageTimer; // 伤害动画定时器

  // --- 增强动画对象 ---
  QPropertyAnimation *m_healthAnimation;     // 血量动画
  QPropertyAnimation *m_expAnimation;        // 经验动画
  QPropertyAnimation *m_powerAnimation;      // 功率动画
  QPropertyAnimation *m_heatAnimation;       // 热量动画
  QPropertyAnimation *m_glowAnimation;       // 发光动画
  QPropertyAnimation *m_shakeAnimation;      // 震动动画
  QParallelAnimationGroup *m_animationGroup; // 并行动画组

  // --- 视觉效果属性 ---
  qreal m_healthProgress; // 血量进度 (0.0-1.0)
  qreal m_expProgress;    // 经验进度 (0.0-1.0)
  qreal m_powerProgress;  // 功率进度 (0.0-1.0)
  qreal m_heatProgress;   // 热量进度 (0.0-1.0)
  qreal m_glowIntensity;  // 发光强度 (0.0-1.0)
  qreal m_shakeOffset;    // 震动偏移量
  qreal m_damageFlash;    // 伤害闪烁强度

  // --- 状态标记 ---
  bool m_isInvulnerable;      // 是否无敌
  bool m_isReviving;          // 是否复活中
  bool m_isWarned;            // 是否被警告
  bool m_isOffline;           // 是否离线
  bool m_showDamageEffect;    // 是否显示伤害特效
  QDateTime m_lastDamageTime; // 最后受伤时间

  // --- 颜色配置 ---
  QColor m_teamColor;         // 队伍主题色
  QColor m_backgroundColor;   // 背景色
  QColor m_borderColor;       // 边框色
  QColor m_textColor;         // 文字色
  QColor m_healthColor;       // 血量条颜色
  QColor m_expColor;          // 经验条颜色
  QColor m_powerColor;        // 功率条颜色
  QColor m_heatColor;         // 热量条颜色
  QColor m_warningColor;      // 警告颜色
  QColor m_criticalColor;     // 危急颜色
  QColor m_invulnerableColor; // 无敌状态颜色
  QColor m_offlineColor;      // 离线状态颜色

  void setupColors();
  void setupEnhancedAnimations();

  void drawBackground(QPainter &painter);
  void drawRobotIcon(QPainter &painter);
  void drawHealthBar(QPainter &painter);
  void drawLevelAndExp(QPainter &painter);
  void drawPowerInfo(QPainter &painter);
  void drawHeatInfo(QPainter &painter);
  void drawStatusEffects(QPainter &painter);
  void drawModuleStatus(QPainter &painter);

  // 增强绘制方法
  void drawEnhancedHealthBar(QPainter &painter);
  void drawEnhancedExpBar(QPainter &painter);
  void drawEnhancedPowerBar(QPainter &painter);
  void drawEnhancedHeatBar(QPainter &painter);
  void drawStatusIndicators(QPainter &painter);
  void drawGlowEffect(QPainter &painter);
  void drawDamageEffect(QPainter &painter);
  void drawWarningEffects(QPainter &painter);
  void drawInvulnerabilityEffect(QPainter &painter);
  void drawReviveEffect(QPainter &painter);
  void drawOfflineEffect(QPainter &painter);
  void drawBuffIcons(QPainter &painter);
  void drawModuleIndicators(QPainter &painter);

  QString getRobotTypeName() const;
  bool isLowHealth() const;
  bool isCriticalHealth() const;
  bool needsBlinking() const;
  bool isOverheating() const;
  bool isPowerCritical() const;

  // 动画辅助方法
  void startHealthAnimation();
  void startExpAnimation();
  void startPowerAnimation();
  void startHeatAnimation();
  void startDamageEffect();
  void startWarningEffect();
  void updateAnimationProgress();
};

#endif // ROBOTSTATUSWIDGET_H
