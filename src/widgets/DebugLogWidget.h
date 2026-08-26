#ifndef DEBUGLOGWIDGET_H
#define DEBUGLOGWIDGET_H

#include "../network/Protocol.h"
#include "robomaster.pb.h"
#include <QCheckBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

namespace RM {

class DebugLogWidget : public QWidget {
  Q_OBJECT
public:
  explicit DebugLogWidget(QWidget *parent = nullptr) : QWidget(parent) {
    setWindowFlags(Qt::Window); // 独立窗口
    setWindowTitle("调试日志 (Debug Log)");
    resize(600, 400);
    setupUI();
  }

  void logSent(const QByteArray &data) {
    if (m_pauseLog->isChecked())
      return;
    if (!m_showSent->isChecked())
      return;
    appendLog("TX", data, "#00FF00"); // 发送数据使用绿色
  }

  void logReceived(const QByteArray &data) {
    if (m_pauseLog->isChecked())
      return;
    if (!m_showReceived->isChecked())
      return;
    appendLog("RX", data, "#00BFFF"); // 接收数据使用蓝色
  }

  void logVideoStats(const QString &stats) {
    if (m_pauseLog->isChecked() || !m_showVideo->isChecked())
      return;

    QString timeStr = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    QString html =
        QString(
            "<span style='color: #888;'>[%1]</span> "
            "<span style='color: #FF00FF; font-weight: bold;'>VIDEO</span>: "
            "<span style='color: #DDD;'>%2</span>")
            .arg(timeStr, stats);
    m_logArea->append(html);
  }

  void logError(const QString &error) {
    if (m_pauseLog->isChecked() || !m_showError->isChecked())
      return;

    QString timeStr = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    QString html =
        QString(
            "<span style='color: #888;'>[%1]</span> "
            "<span style='color: #FF0000; font-weight: bold;'>ERROR</span>: "
            "<span style='color: #FF5555;'>%2</span>")
            .arg(timeStr, error);
    m_logArea->append(html);
  }

private:
  QTextEdit *m_logArea;
  QCheckBox *m_showSent;
  QCheckBox *m_showReceived;
  QCheckBox *m_showVideo;
  QCheckBox *m_showError;
  QCheckBox *m_showRaw;
  QCheckBox *m_pauseLog;

  void setupUI() {
    QVBoxLayout *layout = new QVBoxLayout(this);

    // 控制区
    QHBoxLayout *controls = new QHBoxLayout();
    m_showSent = new QCheckBox("发送(TX)", this);
    m_showSent->setChecked(true);
    m_showReceived = new QCheckBox("接收(RX)", this);
    m_showReceived->setChecked(true);
    m_showVideo = new QCheckBox("视频", this);
    m_showVideo->setChecked(true);
    m_showError = new QCheckBox("错误", this);
    m_showError->setChecked(true);
    m_showRaw = new QCheckBox("原始数据(Hex)", this);
    m_showRaw->setChecked(false);
    m_pauseLog = new QCheckBox("暂停", this);

    QPushButton *clearBtn = new QPushButton("清空", this);
    connect(clearBtn, &QPushButton::clicked, [this]() { m_logArea->clear(); });

    controls->addWidget(m_showSent);
    controls->addWidget(m_showReceived);
    controls->addWidget(m_showVideo);
    controls->addWidget(m_showError);
    controls->addWidget(m_showRaw);
    controls->addWidget(m_pauseLog);
    controls->addStretch();
    controls->addWidget(clearBtn);
    layout->addLayout(controls);

    // 日志区
    m_logArea = new QTextEdit(this);
    m_logArea->setReadOnly(true);
    m_logArea->setStyleSheet("background-color: #1e1e1e; color: #d4d4d4; "
                             "font-family: Consolas, monospace;");
    layout->addWidget(m_logArea);
  }

  void appendLog(const QString &direction, const QByteArray &data,
                 const QString &color) {
    QString timeStr = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    QString content;

    if (m_showRaw->isChecked()) {
      content = data.toHex(' ').toUpper();
    } else if (data.startsWith("MQTT ")) {
      content = QString::fromUtf8(data).toHtmlEscaped();
    } else {
      // 尝试解析数据包
      robomaster::RoboMasterMessage msg;
      if (Protocol::parsePacket(data, msg)) {
        if (msg.has_game_info()) {
          content = QString("GameInfo: Stage=%1 Time=%2 Score=%3:%4")
                        .arg(msg.game_info().stage())
                        .arg(msg.game_info().time_remaining())
                        .arg(msg.game_info().red_score())
                        .arg(msg.game_info().blue_score());
        } else if (msg.has_robot_status()) {
          content = QString("RobotStatus: ID=%1 HP=%2/%3")
                        .arg(msg.robot_status().id())
                        .arg(msg.robot_status().hp())
                        .arg(msg.robot_status().max_hp());
        } else if (msg.has_battle_msg()) {
          content =
              QString("BattleMsg: %1")
                  .arg(QString::fromStdString(msg.battle_msg().content()));
        } else if (msg.has_global_unit_status()) {
          content = QString("GlobalStatus: BaseHP=%1 OutpostHP=%2")
                        .arg(msg.global_unit_status().base_health())
                        .arg(msg.global_unit_status().outpost_health());
        } else {
          content = "Other Message Type";
        }
      } else {
        content = "Parse Error (Invalid Protocol)";
      }
    }

    QString html =
        QString("<span style='color: #888;'>[%1]</span> "
                "<span style='color: %2; font-weight: bold;'>%3</span>: "
                "<span style='color: #DDD;'>%4</span>")
            .arg(timeStr, color, direction, content);
    m_logArea->append(html);
  }
};

} // namespace RM

#endif // DEBUGLOGWIDGET_H
