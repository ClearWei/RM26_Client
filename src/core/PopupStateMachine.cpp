/**
 * @file PopupStateMachine.cpp
 * @brief 比赛流程弹窗状态机实现
 */

#include "PopupStateMachine.h"
#include <QDebug>
#include <QSet>
#include <algorithm>
#include <QDateTime>

using namespace Popup;

namespace {
constexpr bool kPopupStateMachineVerboseLog = false;
}

static bool isNoisyFrameLevelPopupType(PopupType type) {
  return type == PopupType::PrepPhase || type == PopupType::Countdown ||
         type == PopupType::BattlePause;
}

static QString popupTypeToString(PopupType t) {
  switch (t) {
  case PopupType::PrepPhase:
    return QStringLiteral("PrepPhase");
  case PopupType::Countdown:
    return QStringLiteral("Countdown");
  case PopupType::RobotRespawn:
    return QStringLiteral("RobotRespawn");
  case PopupType::Out:
    return QStringLiteral("Out");
  case PopupType::BattlePause:
    return QStringLiteral("BattlePause");
  default:
    return QStringLiteral("Unknown");
  }
}

PopupStateMachine::PopupStateMachine(QObject *parent) : QObject(parent) {}

void PopupStateMachine::submitIntent(PopupType type, PopupPriority prio,
                                     PopupIntent intent,
                                     const QVariantMap &payload) {
  const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
  bool changed = false;
  bool payloadChangedForType = false;
  {
    QMutexLocker lk(&m_mutex);
        if (kPopupStateMachineVerboseLog) {
          qInfo() << "[RespawnDebug] PopupStateMachine: submitIntent ts=" << nowMs
            << "type=" << popupTypeToString(type)
            << "intent=" << static_cast<int>(intent)
            << "prio=" << static_cast<int>(prio)
            << "queue_len=" << m_queue.size();
        }

    const LastSubmittedIntent last = m_lastSubmittedIntents.value(type);
    const bool sameAsLast =
        last.initialized && last.intent == intent && last.prio == prio &&
        last.payload == payload;
    const bool inCooldown =
        sameAsLast && (nowMs - last.tsMs) < m_repeatIntentCooldownMs;

    if (intent == PopupIntent::Show && sameAsLast &&
      (isNoisyFrameLevelPopupType(type) || inCooldown)) {
            if (kPopupStateMachineVerboseLog) {
        qInfo() << "[RespawnDebug] PopupStateMachine: ignoreDuplicateIntent ts="
          << nowMs << "type=" << popupTypeToString(type)
          << "intent=" << static_cast<int>(intent)
          << "cooldown_ms=" << m_repeatIntentCooldownMs
          << "reason="
          << (isNoisyFrameLevelPopupType(type) ? "noisy_frame_level"
                       : "in_cooldown");
            }
      return;
    }

    LastSubmittedIntent current;
    current.intent = intent;
    current.prio = prio;
    current.payload = payload;
    current.tsMs = nowMs;
    current.initialized = true;
    m_lastSubmittedIntents.insert(type, current);

    if (intent == PopupIntent::Show) {
      bool found = false;
      for (auto &rec : m_queue) {
        if (rec.entry.type == type) {
          if (rec.entry.payload != payload)
            payloadChangedForType = true;
          rec.entry.prio = prio;
          rec.entry.payload = payload;
          rec.entry.intent = intent;
          found = true;
          break;
        }
      }

      if (!found) {
        PopupEntry e;
        e.type = type;
        e.prio = prio;
        e.intent = intent;
        e.payload = payload;
        e.seq = m_seqCounter++;
        Record r;
        r.entry = e;
        m_queue.append(r);
      }

      QSet<PopupType> seen;
      for (int i = m_queue.size() - 1; i >= 0; --i) {
        PopupType t = m_queue[i].entry.type;
        if (seen.contains(t)) {
          m_queue.removeAt(i);
        } else {
          seen.insert(t);
        }
      }

      changed = recomputeActiveLocked();
    } else {
      for (int i = m_queue.size() - 1; i >= 0; --i) {
        if (m_queue[i].entry.type == type) {
          m_queue.removeAt(i);
        }
      }
      changed = recomputeActiveLocked();
    }
  }

  if (changed) {
    QMutexLocker lk(&m_mutex);
    if (m_batchDepth > 0) {
      m_pendingActiveChanged = true;
    } else {
      lk.unlock();
      if (kPopupStateMachineVerboseLog) {
        qInfo() << "[RespawnDebug] PopupStateMachine: emitting activePopupsChanged ts=" << QDateTime::currentMSecsSinceEpoch();
      }
      emit activePopupsChanged();
    }
  } else if (payloadChangedForType) {
    QMutexLocker lk(&m_mutex);
    if (m_batchDepth > 0) {
      m_pendingPayloads.insert(popupTypeToString(type), payload);
      m_pendingPayloadsChanged = true;
    } else {
      lk.unlock();
      if (kPopupStateMachineVerboseLog) {
        qInfo() << "[RespawnDebug] PopupStateMachine: emitting popupPayloadChanged ts=" << QDateTime::currentMSecsSinceEpoch()
                << "type=" << popupTypeToString(type);
      }
      emit popupPayloadChanged(popupTypeToString(type), payload);
    }
  }
}

void PopupStateMachine::beginBatchUpdate() {
  QMutexLocker lk(&m_mutex);
  ++m_batchDepth;
}

void PopupStateMachine::endBatchUpdate() {
  bool shouldEmit = false;
  {
    QMutexLocker lk(&m_mutex);
    if (m_batchDepth <= 0)
      return;
    --m_batchDepth;
    if (m_batchDepth == 0 && m_pendingActiveChanged) {
      shouldEmit = true;
      m_pendingActiveChanged = false;
    }
  }
  if (shouldEmit)
    emit activePopupsChanged();

  QMap<QString, QVariantMap> pending;
  {
    QMutexLocker lk(&m_mutex);
    if (m_pendingPayloadsChanged) {
      pending = m_pendingPayloads;
      m_pendingPayloads.clear();
      m_pendingPayloadsChanged = false;
    }
  }
  if (!pending.isEmpty()) {
    for (auto it = pending.constBegin(); it != pending.constEnd(); ++it) {
      emit popupPayloadChanged(it.key(), it.value());
    }
  }
}

QVariantList PopupStateMachine::activePopups() const {
  QMutexLocker lk(&m_mutex);
  return m_activeCache;
}

void PopupStateMachine::clearAll() {
  bool changed = false;
  {
    QMutexLocker lk(&m_mutex);
    m_queue.clear();
    // 清空去重与批处理缓存，避免 clear 后首个同型意图被误判为重复而丢弃。
    m_lastSubmittedIntents.clear();
    m_pendingPayloads.clear();
    m_pendingPayloadsChanged = false;
    m_pendingActiveChanged = false;
    changed = recomputeActiveLocked();
  }
  if (changed)
    emit activePopupsChanged();
}

bool PopupStateMachine::recomputeActiveLocked() {
  QVariantList newActiveFull;
  QVariantList newActiveTypes;
  if (!m_queue.isEmpty()) {
    PopupPriority maxP = m_queue.first().entry.prio;
    for (const auto &rec : m_queue) {
      if (static_cast<int>(rec.entry.prio) > static_cast<int>(maxP))
        maxP = rec.entry.prio;
    }

    QList<const Record *> sorted;
    for (const auto &rec : m_queue) {
      if (rec.entry.prio == maxP)
        sorted.append(&rec);
    }
    std::sort(sorted.begin(), sorted.end(),
              [](const Record *a, const Record *b) {
                return a->entry.seq < b->entry.seq;
              });

    for (const auto *prec : sorted) {
      const auto &rec = *prec;
      QVariantMap mfull;
      mfull["type"] = popupTypeToString(rec.entry.type);
      mfull["priority"] = static_cast<int>(rec.entry.prio);
      mfull["intent"] = static_cast<int>(rec.entry.intent);
      mfull["seq"] = static_cast<qint64>(rec.entry.seq);
      mfull["payload"] = rec.entry.payload;
      newActiveFull.append(mfull);

      QVariantMap mtype;
      mtype["type"] = mfull.value("type");
      mtype["priority"] = mfull.value("priority");
      mtype["intent"] = mfull.value("intent");
      mtype["seq"] = mfull.value("seq");
      newActiveTypes.append(mtype);
    }
  }

  if (m_activeTypeCache != newActiveTypes) {
    m_activeTypeCache = newActiveTypes;
    m_activeCache = newActiveFull;
    if (kPopupStateMachineVerboseLog) {
      qInfo() << "[RespawnDebug] PopupStateMachine: recomputeActiveLocked changed ts=" << QDateTime::currentMSecsSinceEpoch()
              << "queue_len=" << m_queue.size() << "active_count=" << m_activeCache.size();
    }
    return true;
  }
  if (kPopupStateMachineVerboseLog) {
    qInfo() << "[RespawnDebug] PopupStateMachine: recomputeActiveLocked no-change ts=" << QDateTime::currentMSecsSinceEpoch()
            << "queue_len=" << m_queue.size() << "active_count=" << m_activeCache.size();
  }
  m_activeCache = newActiveFull;
  return false;
}
