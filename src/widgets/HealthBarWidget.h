#ifndef HEALTHBARWIDGET_H
#define HEALTHBARWIDGET_H

#include <QDateTime>
#include <QEasingCurve>
#include <QPainter>
#include <QParallelAnimationGroup>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QTimer>
#include <QWidget>

class HealthBarWidget : public QWidget {
  Q_OBJECT
  Q_PROPERTY(int animatedHealth READ getAnimatedHealth WRITE setAnimatedHealth)
  Q_PROPERTY(float glowIntensity READ getGlowIntensity WRITE setGlowIntensity)
  Q_PROPERTY(float shieldOpacity READ getShieldOpacity WRITE setShieldOpacity)
  Q_PROPERTY(float damageFlash READ getDamageFlash WRITE setDamageFlash)

public:
  enum TeamColor { Red, Blue };

  explicit HealthBarWidget(TeamColor team, QWidget *parent = nullptr);

  // 设置接口
  void setHealth(int current, int maximum);
  void setVirtualShield(int shield);
  void setInvincible(bool invincible);
  void setTeamInfo(const QString &teamName, const QString &schoolName);
  void setTeamLogo(const QString &logoPath);
  void setLowHealthThreshold(float threshold = 0.2f);

  // 查询接口
  int getCurrentHealth() const { return m_currentHealth; }
  int getMaxHealth() const { return m_maxHealth; }
  bool isInvincible() const { return m_isInvincible; }
  TeamColor getTeam() const { return m_team; }

  // 动画属性
  int getAnimatedHealth() const { return m_animatedHealth; }
  void setAnimatedHealth(int health);
  float getGlowIntensity() const { return m_glowIntensity; }
  void setGlowIntensity(float intensity);
  float getShieldOpacity() const { return m_shieldOpacity; }
  void setShieldOpacity(float opacity);
  float getDamageFlash() const { return m_damageFlash; }
  void setDamageFlash(float flash);

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private slots:
  void onBlinkTimer();
  void onAnimationFinished();

private:
  // --- 基础数据 ---
  TeamColor m_team;           // 队伍颜色 (红/蓝)
  int m_currentHealth;        // 当前血量
  int m_maxHealth;            // 最大血量
  int m_virtualShield;        // 虚拟护盾值
  bool m_isInvincible;        // 是否处于无敌状态
  QString m_teamName;         // 队伍名称
  QString m_schoolName;       // 学校名称
  QString m_logoPath;         // 队标图片路径
  QPixmap m_logoPixmap;       // 队标图片缓存
  float m_lowHealthThreshold; // 低血量阈值 (0.0-1.0)
  QPixmap m_bottomPixmap;     // 失去血量背景图 (暗色)
  QPixmap m_middlePixmap;     // 当前血量填充图 (亮色)
  QPixmap m_topPixmap;        // 顶层遮罩图 (边框/高光)

  // --- 动画组件 ---
  QTimer *m_blinkTimer;                      // 闪烁定时器 (低血量时触发)
  QPropertyAnimation *m_healthAnimation;     // 血量变化动画
  QPropertyAnimation *m_glowAnimation;       // 发光效果动画
  QPropertyAnimation *m_shieldAnimation;     // 护盾动画
  QPropertyAnimation *m_damageAnimation;     // 受伤闪烁动画
  QParallelAnimationGroup *m_animationGroup; // 并行动画组
  bool m_blinkState;                         // 当前闪烁状态
  int m_animatedHealth;                      // 动画插值血量
  float m_glowIntensity;                     // 发光强度 (0.0-1.0)
  float m_shieldOpacity;                     // 护盾透明度
  float m_damageFlash;                       // 伤害闪烁强度
  int m_previousHealth;                      // 上一次血量 (用于检测伤害)

  // --- 视觉属性 ---
  QColor m_teamColor;       // 队伍主题色
  QColor m_backgroundColor; // 背景颜色
  QColor m_borderColor;     // 边框颜色
  QColor m_textColor;       // 文字颜色

  void setupColors();
  void setupAnimations();
  void loadHealthBarImages();
  void drawBackground(QPainter &painter);
  void drawHealthBar(QPainter &painter);
  void drawVirtualShield(QPainter &painter);
  void drawInvincibleEffect(QPainter &painter);
  void drawTeamInfo(QPainter &painter);
  void drawHealthText(QPainter &painter);
  void drawEnhancedGlow(QPainter &painter);
  void drawDamageEffect(QPainter &painter);
  void drawBreathingEffect(QPainter &painter);
  void updateBlinkState();
  void startDamageAnimation();
  void startShieldAnimation();
  void startGlowPulse();

  QRect getHealthBarRect() const;
  QRect getShieldBarRect() const;
  bool isLowHealth() const;
};

#endif // HEALTHBARWIDGET_H
