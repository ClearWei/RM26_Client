#ifndef INFOPANEL_H
#define INFOPANEL_H

#include <QMap>
#include <QTimer>
#include <QWidget>

class QLabel;
class QTableWidget;

namespace RM {

struct RobotInfo {
  int id;
  QString type;
  int currentHp;
  int maxHp;
  int powerLimit;
  int heatLimit;
  int heatCooldown;
  int fireRateLimit;
  bool isAlive;
};

/**
 * @brief Tab 键 - 信息面板
 * 显示详细比赛信息和机器人状态
 */
class InfoPanel : public QWidget {
  Q_OBJECT

public:
  explicit InfoPanel(QWidget *parent = nullptr);

protected:
  void paintEvent(QPaintEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void showEvent(QShowEvent *event) override;

private:
  void setupUI();
  void updateData();

  // 界面创建辅助方法
  QWidget *createHeaderStats();
  QWidget *createTeamTable(bool isRed);
  void updateTableData(QTableWidget *table, const QList<RobotInfo> &data,
                       bool isRed);

  // 界面元素
  QLabel *m_roundLabel;
  QLabel *m_timeLabel;
  QLabel *m_redScoreLabel;
  QLabel *m_blueScoreLabel;

  QLabel *m_redDartsLabel;
  QLabel *m_blueDartsLabel;
  QLabel *m_redTotalHpLabel;
  QLabel *m_blueTotalHpLabel;
  QLabel *m_redDamageLabel;
  QLabel *m_blueDamageLabel;

  QTableWidget *m_redTable;
  QTableWidget *m_blueTable;

  QTimer *m_updateTimer;

  // 演示数据
  QList<RobotInfo> m_redTeamData;
  QList<RobotInfo> m_blueTeamData;

signals:
  void closed();
};

} // namespace RM

#endif // INFOPANEL_H
