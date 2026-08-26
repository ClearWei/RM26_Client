/**
 * @file TimedEventRules.cpp
 * @brief 固定时间事件提示规则实现
 */

#include "TimedEventRules.h"

#include <QDebug>
#include <algorithm>

namespace RM {

namespace {
constexpr int kDartCanOpenStartRemainingSeconds = 390; // 6:30
constexpr int kDartCanOpenDropEndRemainingSeconds = 181; // 3:01
constexpr int kDartCanOpenLateStartRemainingSeconds = 180; // 3:00
constexpr int kDartCanOpenEndRemainingSeconds = 0; // 0:00
constexpr int kOutpostReviveReminderEndRemainingSeconds = 120; // 2:00

const char *runeVoiceTypeName(RuneVoiceType type) {
  return type == RuneVoiceType::Small ? "small" : "large";
}

QString runeStatusText(int status) {
  return status == 0 ? QStringLiteral("unknown") : QString::number(status);
}
} // namespace

// 参数：事件ID，比赛剩余时间，触发前提示时间，触发后停留时间，TIMER显示文本，弹窗文案，可选提示音文件名
const QVector<TimedEventRule> &defaultTimedEventRules() {
  static const QVector<TimedEventRule> rules = {
      {QStringLiteral("small_mechanism_659"), 419, 1, 3, QStringLiteral("6:59"),
       QStringLiteral("可开启小能量机关")},
      {QStringLiteral("small_mechanism_530"), 330, 5, 3, QStringLiteral("5:30"),
       QStringLiteral("可开启小能量机关")},
      {QStringLiteral("dart_gate_open_630"), 390, 5, 3, QStringLiteral("6:30"),
       QStringLiteral("可开启飞镖闸门")},
      {QStringLiteral("dart_gate_open_300"), 180, 5, 3, QStringLiteral("3:00"),
       QStringLiteral("可开启飞镖闸门")},
      {QStringLiteral("big_mechanism_400"), 240, 5, 3, QStringLiteral("4:00"),
       QStringLiteral("可开启大能量机关")},
      {QStringLiteral("big_mechanism_245"), 165, 5, 3, QStringLiteral("2:45"),
       QStringLiteral("可开启大能量机关")},
      {QStringLiteral("big_mechanism_130"), 90, 5, 3, QStringLiteral("1:30"),
       QStringLiteral("可开启大能量机关")},
      {QStringLiteral("air_support_659"), 419, 1, 3, QStringLiteral("6:59"),
       QStringLiteral("空中机器人获得30秒空中支援时间")},
      {QStringLiteral("air_support_559"), 359, 3, 3, QStringLiteral("5:59"),
       QStringLiteral("空中机器人获得20秒空中支援时间")},
      {QStringLiteral("air_support_459"), 299, 3, 3, QStringLiteral("4:59"),
       QStringLiteral("空中机器人获得20秒空中支援时间")},
      {QStringLiteral("air_support_359"), 239, 3, 3, QStringLiteral("3:59"),
       QStringLiteral("空中机器人获得20秒空中支援时间")},
      {QStringLiteral("air_support_259"), 179, 3, 3, QStringLiteral("2:59"),
       QStringLiteral("空中机器人获得20秒空中支援时间")},
      {QStringLiteral("air_support_159"), 119, 3, 3, QStringLiteral("1:59"),
       QStringLiteral("空中机器人获得20秒空中支援时间")},
      {QStringLiteral("air_support_059"), 59, 3, 3, QStringLiteral("0:59"),
       QStringLiteral("空中机器人获得20秒空中支援时间")},
  };
  return rules;
}

bool tryMatchTimedEvent(int currentGameTime, TimedEventHit *hit, int enemyOutpostHp) {
  Q_UNUSED(enemyOutpostHp);
  for (const auto &rule : defaultTimedEventRules()) {
    const int countdown = currentGameTime - rule.triggerRemainingSeconds;
    if (countdown < -rule.preTriggerSeconds ||
        countdown > rule.postTriggerSeconds) {
      continue;
    }

    if (hit) {
      hit->key = rule.key;
      hit->countdownSeconds = std::max(0, countdown);
      hit->triggerTimeText = rule.triggerTimeText;
      hit->label = rule.label;
      hit->soundFileName = rule.soundFileName;
    }
    return true;
  }

  return false;
}

QVector<TimedEventHit> tryMatchTimedEvents(int currentGameTime, int enemyOutpostHp) {
  Q_UNUSED(enemyOutpostHp);
  QVector<TimedEventHit> hits;
  for (const auto &rule : defaultTimedEventRules()) {
    const int countdown = currentGameTime - rule.triggerRemainingSeconds;
    if (countdown < -rule.preTriggerSeconds ||
        countdown > rule.postTriggerSeconds) {
      continue;
    }
    TimedEventHit hit;
    hit.key = rule.key;
    hit.countdownSeconds = std::max(0, countdown);
    hit.triggerTimeText = rule.triggerTimeText;
    hit.label = rule.label;
    hit.soundFileName = rule.soundFileName;
    hits.append(hit);
  }
  return hits;
}

bool isGunnerOperator(int robotId) {
  // 本项目用 R6/B6 作为云台手客户端身份
  // （蓝方协议 ID 为 106）。
  const int normalizedRobotId = robotId >= 100 ? robotId - 100 : robotId;
  return normalizedRobotId == 6;
}

bool isDartGateVoiceOperator(int robotId) {
  // 官方选手端的飞镖操控交互仅属于云台手。
  return isGunnerOperator(robotId);
}

bool shouldPlayDartCanOpenDropSound(int currentGameTime,
                                    int previousEnemyOutpostHp,
                                    int currentEnemyOutpostHp,
                                    bool isBattleStage) {
  Q_UNUSED(previousEnemyOutpostHp);
  return isBattleStage &&
         currentGameTime <= kDartCanOpenStartRemainingSeconds &&
         currentGameTime >= kDartCanOpenDropEndRemainingSeconds &&
         currentEnemyOutpostHp == 0;
}

bool shouldPlayDartCanOpenLateSound(int currentGameTime,
                                    int currentEnemyOutpostHp,
                                    bool isBattleStage) {
  return isBattleStage &&
         currentGameTime <= kDartCanOpenLateStartRemainingSeconds &&
         currentGameTime >= kDartCanOpenEndRemainingSeconds &&
         currentEnemyOutpostHp == 0;
}

bool shouldPlayDartCanOpenSound(int currentGameTime, int previousEnemyOutpostHp,
                                int currentEnemyOutpostHp,
                                bool isBattleStage) {
  return shouldPlayDartCanOpenDropSound(currentGameTime, previousEnemyOutpostHp,
                                        currentEnemyOutpostHp, isBattleStage) ||
         shouldPlayDartCanOpenLateSound(currentGameTime,
                                        currentEnemyOutpostHp, isBattleStage);
}

bool shouldEnableOutpostReviveReminder(bool isAllyOutpost, int outpostStatus,
                                       int currentGameTime,
                                       bool isBattleStage) {
  return isAllyOutpost && isBattleStage && outpostStatus == 4 &&
         currentGameTime > kOutpostReviveReminderEndRemainingSeconds;
}

QString runeVoiceSoundFileName(RuneVoiceType type, int remainingChances) {
  if (type == RuneVoiceType::Small) {
    switch (remainingChances) {
    case 1:
      return QStringLiteral("rune_small_remaining_1.mp3");
    case 2:
      return QStringLiteral("rune_small_remaining_2.mp3");
    default:
      return {};
    }
  }

  switch (remainingChances) {
  case 1:
    return QStringLiteral("rune_large_remaining_1.mp3");
  case 2:
    return QStringLiteral("rune_large_remaining_2.mp3");
  case 3:
    return QStringLiteral("rune_large_remaining_3.mp3");
  default:
    return {};
  }
}

int RuneVoicePromptTracker::elapsedSeconds(int remainingSeconds) {
  return std::clamp(420 - remainingSeconds, 0, 420);
}

void RuneVoicePromptTracker::reset(const QString &reason) {
  const bool hadState = m_battleActive || m_elapsedSeconds >= 0 ||
                        m_runeStatus != 0 || m_smallChances != 0 ||
                        m_largeChances != 0 ||
                        !m_processedGrantTimes.isEmpty() ||
                        !m_processedPromptTimes.isEmpty();
  if (hadState) {
    qInfo().noquote()
        << QStringLiteral(
               "[RuneVoice] tracker reset reason=%1 battleActive=%2 "
               "elapsed=%3 status=%4 small=%5 large=%6 grants=%7 prompts=%8")
               .arg(reason)
               .arg(m_battleActive)
               .arg(m_elapsedSeconds)
               .arg(runeStatusText(m_runeStatus))
               .arg(m_smallChances)
               .arg(m_largeChances)
               .arg(m_processedGrantTimes.size())
               .arg(m_processedPromptTimes.size());
  }
  m_battleActive = false;
  m_elapsedSeconds = -1;
  m_runeStatus = 0;
  m_smallChances = 0;
  m_largeChances = 0;
  m_processedGrantTimes.clear();
  m_processedPromptTimes.clear();
  m_lastLoggedRollbackRemaining = -1;
  m_loggedInactiveTimeRejection = false;
  m_waitingForOpeningClock = false;
}

void RuneVoicePromptTracker::markPastGrantsProcessed(int elapsed) {
  static constexpr int grantTimes[] = {0, 90, 180, 255, 330};
  for (const int grantTime : grantTimes) {
    if (grantTime <= elapsed) {
      m_processedGrantTimes.insert(grantTime);
    }
  }
}

void RuneVoicePromptTracker::startBattle(int remainingSeconds,
                                         bool observedFullStart) {
  reset(QStringLiteral("start_battle"));
  m_battleActive = true;
  const bool fullStartPlaceholder =
      observedFullStart && remainingSeconds >= 0 && remainingSeconds <= 1;
  m_waitingForOpeningClock = fullStartPlaceholder || remainingSeconds == 0;
  if (m_waitingForOpeningClock) {
    // 服务端可能先用 0 或 1 秒占位，再发布 420/419… 的正式倒计时。
    // 完整开局从 elapsed=0 计算；重连只看到占位值时，不推测此前的机关机会。
    m_elapsedSeconds = observedFullStart ? 0 : -1;
  } else {
    m_elapsedSeconds = elapsedSeconds(remainingSeconds);
    markPastGrantsProcessed(m_elapsedSeconds);
  }

  // 只有观察到倒计时切入战斗，或在 6:59—7:00 接入，才能确认初始小能量机关机会。
  if (observedFullStart) {
    m_smallChances = 1;
    m_processedGrantTimes.insert(0);
  }

  qInfo().noquote()
      << QStringLiteral(
             "[RuneVoice] tracker startBattle remaining=%1 elapsed=%2 "
             "observedFullStart=%3 clockMode=%4 status=%5 small=%6 large=%7 "
             "grants=%8")
             .arg(remainingSeconds)
             .arg(m_elapsedSeconds)
             .arg(observedFullStart)
             .arg(m_waitingForOpeningClock
                      ? QStringLiteral("opening_placeholder_%1")
                            .arg(remainingSeconds)
                      : QStringLiteral("direct"))
             .arg(runeStatusText(m_runeStatus))
             .arg(m_smallChances)
             .arg(m_largeChances)
             .arg(m_processedGrantTimes.size());
}

void RuneVoicePromptTracker::grantOpportunitiesThrough(int elapsed) {
  struct Grant {
    int elapsed;
    RuneVoiceType type;
  };
  static constexpr Grant grants[] = {
      {90, RuneVoiceType::Small},
      {180, RuneVoiceType::Large},
      {255, RuneVoiceType::Large},
      {330, RuneVoiceType::Large},
  };

  for (const Grant &grant : grants) {
    if (grant.elapsed > elapsed ||
        m_processedGrantTimes.contains(grant.elapsed)) {
      continue;
    }
    m_processedGrantTimes.insert(grant.elapsed);
    if (grant.type == RuneVoiceType::Small) {
      m_smallChances = std::min(2, m_smallChances + 1);
    } else {
      m_largeChances = std::min(3, m_largeChances + 1);
    }
    qInfo().noquote()
        << QStringLiteral(
               "[RuneVoice] opportunity granted type=%1 grantElapsed=%2 "
               "currentElapsed=%3 small=%4 large=%5")
               .arg(QString::fromLatin1(runeVoiceTypeName(grant.type)))
               .arg(grant.elapsed)
               .arg(elapsed)
               .arg(m_smallChances)
               .arg(m_largeChances);
  }
}

void RuneVoicePromptTracker::updateRuneStatus(int runeStatus) {
  if (runeStatus < 1 || runeStatus > 3) {
    if (m_battleActive) {
      qWarning().noquote()
          << QStringLiteral(
                 "[RuneVoice] tracker status ignored reason=invalid_status "
                 "incoming=%1 elapsed=%2")
                 .arg(runeStatus)
                 .arg(m_elapsedSeconds);
    }
    return;
  }
  if (!m_battleActive) {
    return;
  }

  const int previousStatus = m_runeStatus;
  const int previousSmallChances = m_smallChances;
  const int previousLargeChances = m_largeChances;
  const bool enteredActivating = runeStatus == 2 && previousStatus != 2;
  const bool directlyActivated =
      runeStatus == 3 && previousStatus != 2 && previousStatus != 3;
  if (enteredActivating || directlyActivated) {
    // 现场服务端可能跳过状态 2，直接上报 1→3。首次观察到激活边沿时扣减机会，
    // 同时保证正常的 1→2→3 序列不会重复扣减。
    if (m_elapsedSeconds >= 0 && m_elapsedSeconds < 180) {
      m_smallChances = std::max(0, m_smallChances - 1);
    } else if (m_elapsedSeconds >= 180) {
      m_largeChances = std::max(0, m_largeChances - 1);
    }
  }

  m_runeStatus = runeStatus;
  if (previousStatus != runeStatus) {
    const bool consumed = previousSmallChances != m_smallChances ||
                          previousLargeChances != m_largeChances;
    qInfo().noquote()
        << QStringLiteral(
               "[RuneVoice] tracker status applied old=%1 new=%2 elapsed=%3 "
               "consumeTrigger=%4 consumed=%5 small=%6->%7 large=%8->%9")
               .arg(runeStatusText(previousStatus))
               .arg(runeStatus)
               .arg(m_elapsedSeconds)
               .arg(enteredActivating
                        ? QStringLiteral("entered_activating")
                        : directlyActivated
                            ? QStringLiteral("direct_activated")
                            : QStringLiteral("none"))
               .arg(consumed)
               .arg(previousSmallChances)
               .arg(m_smallChances)
               .arg(previousLargeChances)
               .arg(m_largeChances);
  }
}

std::optional<RuneVoicePrompt>
RuneVoicePromptTracker::updateTime(int remainingSeconds) {
  if (!m_battleActive) {
    if (!m_loggedInactiveTimeRejection) {
      qWarning().noquote()
          << QStringLiteral(
                 "[RuneVoice] time update rejected reason=battle_inactive "
                 "remaining=%1")
                 .arg(remainingSeconds);
      m_loggedInactiveTimeRejection = true;
    }
    return std::nullopt;
  }
  if (m_waitingForOpeningClock && remainingSeconds >= 0 &&
      remainingSeconds <= 1) {
    return std::nullopt;
  }
  if (remainingSeconds <= 0 || remainingSeconds > 420) {
    qWarning().noquote()
        << QStringLiteral(
               "[RuneVoice] time update rejected reason=invalid_remaining "
               "remaining=%1 elapsed=%2")
               .arg(remainingSeconds)
               .arg(m_elapsedSeconds);
    return std::nullopt;
  }

  const int elapsed = elapsedSeconds(remainingSeconds);
  if (m_waitingForOpeningClock && m_elapsedSeconds < 0) {
    // 首次只看到零占位值时，无法可靠还原第一个有效时钟之前的机会。
    m_waitingForOpeningClock = false;
    m_elapsedSeconds = elapsed;
    markPastGrantsProcessed(elapsed);
    qInfo().noquote()
        << QStringLiteral(
               "[RuneVoice] opening clock synchronized remaining=%1 "
               "elapsed=%2 mode=conservative_reconnect")
               .arg(remainingSeconds)
               .arg(elapsed);
    return std::nullopt;
  }

  if (elapsed < m_elapsedSeconds) {
    if (m_lastLoggedRollbackRemaining != remainingSeconds) {
      qWarning().noquote()
          << QStringLiteral(
                 "[RuneVoice] time update rejected reason=time_rollback "
                 "remaining=%1 incomingElapsed=%2 trackerElapsed=%3 "
                 "status=%4 small=%5 large=%6")
                 .arg(remainingSeconds)
                 .arg(elapsed)
                 .arg(m_elapsedSeconds)
                 .arg(runeStatusText(m_runeStatus))
                 .arg(m_smallChances)
                 .arg(m_largeChances);
      m_lastLoggedRollbackRemaining = remainingSeconds;
    }
    return std::nullopt;
  }
  if (m_waitingForOpeningClock) {
    m_waitingForOpeningClock = false;
    qInfo().noquote()
        << QStringLiteral(
               "[RuneVoice] opening clock synchronized remaining=%1 "
               "elapsed=%2 mode=full_start")
               .arg(remainingSeconds)
               .arg(elapsed);
  }
  m_lastLoggedRollbackRemaining = -1;
  grantOpportunitiesThrough(elapsed);
  m_elapsedSeconds = elapsed;

  const bool isSmallPrompt =
      elapsed >= 30 && elapsed <= 150 && elapsed % 30 == 0;
  const bool isLargePrompt =
      (elapsed >= 180 && elapsed <= 390 && elapsed % 30 == 0) ||
      elapsed == 255;
  if ((!isSmallPrompt && !isLargePrompt) ||
      m_processedPromptTimes.contains(elapsed)) {
    return std::nullopt;
  }

  // 即使当前不可提示，也要消费检查点，避免重复时间包或暂停造成延迟补播。
  m_processedPromptTimes.insert(elapsed);
  const RuneVoiceType type =
      isSmallPrompt ? RuneVoiceType::Small : RuneVoiceType::Large;
  const int remainingChances =
      type == RuneVoiceType::Small ? m_smallChances : m_largeChances;

  // 必须先收到有效 RuneStatusSync。状态 2 表示正在激活；状态 1 和 3 可以提示。
  if (m_runeStatus == 0) {
    qInfo().noquote()
        << QStringLiteral(
               "[RuneVoice] checkpoint elapsed=%1 remaining=%2 type=%3 "
               "status=unknown small=%4 large=%5 chances=%6 "
               "decision=suppress reason=no_valid_rune_status")
               .arg(elapsed)
               .arg(remainingSeconds)
               .arg(QString::fromLatin1(runeVoiceTypeName(type)))
               .arg(m_smallChances)
               .arg(m_largeChances)
               .arg(remainingChances);
    return std::nullopt;
  }
  if (m_runeStatus == 2) {
    qInfo().noquote()
        << QStringLiteral(
               "[RuneVoice] checkpoint elapsed=%1 remaining=%2 type=%3 "
               "status=2 small=%4 large=%5 chances=%6 decision=suppress "
               "reason=activating")
               .arg(elapsed)
               .arg(remainingSeconds)
               .arg(QString::fromLatin1(runeVoiceTypeName(type)))
               .arg(m_smallChances)
               .arg(m_largeChances)
               .arg(remainingChances);
    return std::nullopt;
  }

  const QString fileName = runeVoiceSoundFileName(type, remainingChances);
  if (remainingChances <= 0 || fileName.isEmpty()) {
    qInfo().noquote()
        << QStringLiteral(
               "[RuneVoice] checkpoint elapsed=%1 remaining=%2 type=%3 "
               "status=%4 small=%5 large=%6 chances=%7 decision=suppress "
               "reason=no_remaining_chance")
               .arg(elapsed)
               .arg(remainingSeconds)
               .arg(QString::fromLatin1(runeVoiceTypeName(type)))
               .arg(m_runeStatus)
               .arg(m_smallChances)
               .arg(m_largeChances)
               .arg(remainingChances);
    return std::nullopt;
  }

  qInfo().noquote()
      << QStringLiteral(
             "[RuneVoice] checkpoint elapsed=%1 remaining=%2 type=%3 "
             "status=%4 small=%5 large=%6 chances=%7 decision=emit file=%8")
             .arg(elapsed)
             .arg(remainingSeconds)
             .arg(QString::fromLatin1(runeVoiceTypeName(type)))
             .arg(m_runeStatus)
             .arg(m_smallChances)
             .arg(m_largeChances)
             .arg(remainingChances)
             .arg(fileName);
  return RuneVoicePrompt{type, remainingChances, fileName};
}

} // namespace RM
