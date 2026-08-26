/**
 * @file ClientDevHooks.h
 * @brief 可选开发 Hook 层
 * @details 默认不参与 release 使用，仅在 RM26_ENABLE_DEVTOOLS 构建开关开启时编译。
 *          该层只暴露窄能力：
 *          - 健康检查
 *          - 状态快照
 *          - 截图
 *          - 窄动作转发
 *          - Trace 导出
 */

#ifndef CLIENTDEVHOOKS_H
#define CLIENTDEVHOOKS_H

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <Qt>

QT_BEGIN_NAMESPACE
class QHttpServer;
class QHttpServerRequest;
class QHttpServerResponse;
class QWidget;
QT_END_NAMESPACE

#include "../core/GameData.h"

namespace RM {

class NetworkManager;
class VideoReceiver;
class HevcDecoder;
class VideoBackgroundWidget;

class ClientDevHooks : public QObject {
  Q_OBJECT

public:
  explicit ClientDevHooks(GameData *gameData, NetworkManager *networkManager,
                          VideoReceiver *videoReceiver,
                          VideoBackgroundWidget *videoWidget,
                          HevcDecoder *hevcDecoder, QWidget *window,
                          QObject *parent = nullptr);
  ~ClientDevHooks() override;

  static bool isEnabled();

  bool start();
  void stop();
  quint16 port() const { return m_port; }

  void recordTraceEvent(const QString &event,
                        const QJsonObject &payload = QJsonObject());

private:
  void setupRoutes();
  QJsonObject collectState() const;
  QJsonObject collectGameState() const;
  QJsonObject collectTopBarState() const;
  QJsonObject collectNetworkState() const;
  QJsonObject collectVideoStats() const;
  QJsonObject collectUiState() const;
  QJsonObject collectRespawnState() const;
  QString detectCurrentPanel() const;

  QHttpServerResponse handleHealth() const;
  QHttpServerResponse handleState() const;
  QHttpServerResponse handleCommonCommand(const QHttpServerRequest &request);
  QHttpServerResponse handleKeyPress(const QHttpServerRequest &request);
  QHttpServerResponse handleKeyRelease(const QHttpServerRequest &request);
  QHttpServerResponse handleScreenshot(const QHttpServerRequest &request);
  QHttpServerResponse handleFlushTrace();

  bool dispatchKeyEvent(int key, bool pressed, Qt::KeyboardModifiers modifiers);

private:
  GameData *m_gameData = nullptr;
  NetworkManager *m_networkManager = nullptr;
  VideoReceiver *m_videoReceiver = nullptr;
  VideoBackgroundWidget *m_videoWidget = nullptr;
  HevcDecoder *m_hevcDecoder = nullptr;
  QWidget *m_window = nullptr;
  QHttpServer *m_httpServer = nullptr;
  quint16 m_port = 17711;
  QString m_runDir;
  bool m_running = false;
  QJsonArray m_traceBuffer;
};

} // namespace RM

#endif // CLIENTDEVHOOKS_H
