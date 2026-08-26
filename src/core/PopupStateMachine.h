/**
 * @file PopupStateMachine.h
 * @brief 比赛流程弹窗状态机头文件
 */
#ifndef POPUPSTATEMACHINE_H
#define POPUPSTATEMACHINE_H

#include "PopupTypes.h"
#include <QList>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QVariant>

class PopupStateMachine : public QObject {
  Q_OBJECT
public:
  explicit PopupStateMachine(QObject *parent = nullptr);
  ~PopupStateMachine() override = default;

  void submitIntent(Popup::PopupType type, Popup::PopupPriority prio,
                    Popup::PopupIntent intent,
                    const QVariantMap &payload = QVariantMap());

  QVariantList activePopups() const;
  void clearAll();

signals:
  void activePopupsChanged();
  void popupPayloadChanged(const QString &type, const QVariantMap &payload);

private:
  struct Record {
    Popup::PopupEntry entry;
  };

  struct LastSubmittedIntent {
    Popup::PopupIntent intent = Popup::PopupIntent::Dismiss;
    Popup::PopupPriority prio = Popup::PopupPriority::Low;
    QVariantMap payload;
    qint64 tsMs = 0;
    bool initialized = false;
  };

  bool recomputeActiveLocked();

  QList<Record> m_queue;
  QVariantList m_activeCache;
  QVariantList m_activeTypeCache;
  mutable QMutex m_mutex;
  qint64 m_seqCounter = 1;
  int m_batchDepth = 0;
  bool m_pendingActiveChanged = false;
  QMap<QString, QVariantMap> m_pendingPayloads;
  bool m_pendingPayloadsChanged = false;
  QMap<Popup::PopupType, LastSubmittedIntent> m_lastSubmittedIntents;
  int m_repeatIntentCooldownMs = 200;

public:
  void beginBatchUpdate();
  void endBatchUpdate();
};

#endif // POPUPSTATEMACHINE_H
