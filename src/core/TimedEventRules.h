/**
 * @file TimedEventRules.h
 * @brief 固定时间事件提示规则
 */
#ifndef TIMEDEVENTRULES_H
#define TIMEDEVENTRULES_H

#include <QVector>
#include <QString>
#include <QSet>
#include <optional>

namespace RM {

constexpr int kOutpostReviveReminderIntervalMs = 30000;

struct TimedEventRule {
  QString key;                  // 规则唯一键
  int triggerRemainingSeconds;   // 触发时剩余时间 (秒)
  int preTriggerSeconds;         // 触发前提示窗口 (秒)
  int postTriggerSeconds;        // 触发后停留窗口 (秒)
  QString triggerTimeText;       // 规则基准时间文本
  QString label;                 // 提示文案
  QString soundFileName;         // 可选提示音文件名，空则不播放
};

struct TimedEventHit {
  QString key;                  // 命中的规则键
  int countdownSeconds = -1;    // 距离触发剩余秒数
  QString triggerTimeText;      // 规则基准时间文本
  QString label;                // 提示文案
  QString soundFileName;        // 可选提示音文件名，空则不播放
};

const QVector<TimedEventRule> &defaultTimedEventRules();
bool tryMatchTimedEvent(int currentGameTime, TimedEventHit *hit = nullptr, int enemyOutpostHp = -1);
QVector<TimedEventHit> tryMatchTimedEvents(int currentGameTime, int enemyOutpostHp = -1);
bool isGunnerOperator(int robotId);
bool isDartGateVoiceOperator(int robotId);
bool shouldPlayDartCanOpenDropSound(int currentGameTime,
                                    int previousEnemyOutpostHp,
                                    int currentEnemyOutpostHp,
                                    bool isBattleStage = true);
bool shouldPlayDartCanOpenLateSound(int currentGameTime,
                                    int currentEnemyOutpostHp,
                                    bool isBattleStage = true);
bool shouldPlayDartCanOpenSound(int currentGameTime, int previousEnemyOutpostHp,
                                int currentEnemyOutpostHp,
                                bool isBattleStage = true);
bool shouldEnableOutpostReviveReminder(bool isAllyOutpost, int outpostStatus,
                                       int currentGameTime,
                                       bool isBattleStage = true);

enum class RuneVoiceType {
  Small = 0,
  Large = 1,
};

struct RuneVoicePrompt {
  RuneVoiceType type = RuneVoiceType::Small;
  int remainingChances = 0;
  QString soundFileName;
};

QString runeVoiceSoundFileName(RuneVoiceType type, int remainingChances);

class RuneVoicePromptTracker {
public:
  void reset(const QString &reason = QStringLiteral("explicit"));
  void startBattle(int remainingSeconds, bool observedFullStart);
  void updateRuneStatus(int runeStatus);
  std::optional<RuneVoicePrompt> updateTime(int remainingSeconds);

  int remainingSmallChances() const { return m_smallChances; }
  int remainingLargeChances() const { return m_largeChances; }

private:
  static int elapsedSeconds(int remainingSeconds);
  void markPastGrantsProcessed(int elapsed);
  void grantOpportunitiesThrough(int elapsed);

  bool m_battleActive = false;
  int m_elapsedSeconds = -1;
  int m_runeStatus = 0;
  int m_smallChances = 0;
  int m_largeChances = 0;
  QSet<int> m_processedGrantTimes;
  QSet<int> m_processedPromptTimes;
  int m_lastLoggedRollbackRemaining = -1;
  bool m_loggedInactiveTimeRejection = false;
  bool m_waitingForOpeningClock = false;
};

} // namespace RM

#endif // TIMEDEVENTRULES_H
