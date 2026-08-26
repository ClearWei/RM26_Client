#include "InfoPanel.h"
#include <QDateTime>
#include <QDebug>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QProgressBar>
#include <QTableWidget>
#include <QVBoxLayout>

namespace RM {

InfoPanel::InfoPanel(QWidget *parent) : QWidget(parent) {
  setFixedSize(1200, 700); // 为详细统计预留显示空间

  // 初始化演示数据
  auto initData = [](int id, QString type, int hp, int power, int heat,
                     int cool, int fire) {
    return RobotInfo{id, type, hp, hp, power, heat, cool, fire, true};
  };

  m_redTeamData = {
      initData(1, "英雄", 250, 110, 200, 40, 16),
      initData(2, "工程", 250, 0, 0, 0, 0),
      initData(3, "步兵", 200, 45, 50, 40, 25),
      initData(4, "步兵", 200, 45, 50, 40, 25), // 4 号沿用步兵类型
      initData(7, "哨兵", 400, 100, 400, 80, 25)};

  m_blueTeamData = {initData(1, "英雄", 300, 65, 260, 56, 16),
                    initData(2, "工程", 250, 0, 0, 0, 0),
                    initData(3, "步兵", 250, 55, 120, 50, 25),
                    initData(4, "步兵", 250, 55, 120, 50, 25),
                    initData(7, "哨兵", 400, 100, 400, 80, 25)};

  setupUI();

  m_updateTimer = new QTimer(this);
  connect(m_updateTimer, &QTimer::timeout, this, &InfoPanel::updateData);
}

void InfoPanel::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(20, 20, 20, 20);
  mainLayout->setSpacing(10);

  // 1. 顶部统计区
  mainLayout->addWidget(createHeaderStats());

  // 2. 表格区
  QHBoxLayout *tablesLayout = new QHBoxLayout();
  tablesLayout->setSpacing(20);

  QWidget *redTableWidget = createTeamTable(true);
  m_redTable = redTableWidget->findChild<QTableWidget *>();

  QWidget *blueTableWidget = createTeamTable(false);
  m_blueTable = blueTableWidget->findChild<QTableWidget *>();

  tablesLayout->addWidget(redTableWidget);
  tablesLayout->addWidget(blueTableWidget);

  mainLayout->addLayout(tablesLayout);
}

QWidget *InfoPanel::createHeaderStats() {
  QWidget *widget = new QWidget(this);
  widget->setFixedHeight(120);
  QVBoxLayout *layout = new QVBoxLayout(widget);
  layout->setContentsMargins(0, 0, 0, 0);

  // 顶栏：回合信息
  QHBoxLayout *topLayout = new QHBoxLayout();
  m_roundLabel = new QLabel("Round 1/1", widget);
  m_roundLabel->setStyleSheet("color: #AAA; font-size: 14px;");

  m_timeLabel = new QLabel("0:02", widget);
  m_timeLabel->setStyleSheet(
      "color: #FFD700; font-size: 32px; font-weight: bold;");

  m_redScoreLabel = new QLabel("0", widget);
  m_redScoreLabel->setStyleSheet(
      "color: #FF3232; font-size: 32px; font-weight: bold;");

  m_blueScoreLabel = new QLabel("0", widget);
  m_blueScoreLabel->setStyleSheet(
      "color: #3264FF; font-size: 32px; font-weight: bold;");

  topLayout->addStretch();
  topLayout->addWidget(m_redScoreLabel);
  topLayout->addSpacing(40);
  topLayout->addWidget(m_timeLabel);
  topLayout->addSpacing(40);
  topLayout->addWidget(m_blueScoreLabel);
  topLayout->addStretch();

  layout->addLayout(topLayout);

  // 底栏：详细统计
  QHBoxLayout *statsLayout = new QHBoxLayout();

  auto createStat = [](QString label, QString value, QString color) {
    QLabel *l = new QLabel(QString("%1 %2").arg(label).arg(value));
    l->setStyleSheet(
        QString("color: %1; font-size: 14px; font-weight: bold;").arg(color));
    return l;
  };

  // 红方统计
  m_redDartsLabel = createStat("飞镖命中数 | 总数", "0 | 4", "white");
  m_redTotalHpLabel = createStat("机器人总剩余血量", "526", "#FF3232");

  // 中央伤害对比
  QWidget *damageWidget = new QWidget(widget);
  damageWidget->setFixedSize(300, 50);
  damageWidget->setStyleSheet(
      "border: 1px solid #FFD700; background: rgba(0,0,0,100);");
  QHBoxLayout *dmgLayout = new QHBoxLayout(damageWidget);
  m_redDamageLabel = new QLabel("0", damageWidget);
  m_redDamageLabel->setStyleSheet(
      "color: #FF3232; font-size: 24px; font-weight: bold;");
  QLabel *vsIcon =
      new QLabel("⚔", damageWidget); // 交叉剑图标占位
  vsIcon->setStyleSheet("color: white; font-size: 20px;");
  m_blueDamageLabel = new QLabel("915", damageWidget);
  m_blueDamageLabel->setStyleSheet(
      "color: #3264FF; font-size: 24px; font-weight: bold;");

  dmgLayout->addWidget(m_redDamageLabel, 0, Qt::AlignCenter);
  dmgLayout->addWidget(vsIcon, 0, Qt::AlignCenter);
  dmgLayout->addWidget(m_blueDamageLabel, 0, Qt::AlignCenter);

  // 蓝方统计
  m_blueTotalHpLabel = createStat("机器人总剩余血量", "1450", "#3264FF");
  m_blueDartsLabel = createStat("飞镖命中数 | 总数", "0 | 4", "white");

  statsLayout->addStretch();
  statsLayout->addWidget(m_redDartsLabel);
  statsLayout->addSpacing(20);
  statsLayout->addWidget(m_redTotalHpLabel);
  statsLayout->addSpacing(40);
  statsLayout->addWidget(damageWidget);
  statsLayout->addSpacing(40);
  statsLayout->addWidget(m_blueTotalHpLabel);
  statsLayout->addSpacing(20);
  statsLayout->addWidget(m_blueDartsLabel);
  statsLayout->addStretch();

  layout->addLayout(statsLayout);

  return widget;
}

QWidget *InfoPanel::createTeamTable(bool isRed) {
  QWidget *widget = new QWidget(this);
  QVBoxLayout *layout = new QVBoxLayout(widget);
  layout->setContentsMargins(0, 0, 0, 0);

  // 带队伍色标的表头
  QWidget *headerLine = new QWidget(widget);
  headerLine->setFixedHeight(2);
  headerLine->setStyleSheet(
      QString("background-color: %1;").arg(isRed ? "#FF3232" : "#3264FF"));
  layout->addWidget(headerLine);

  QTableWidget *table = new QTableWidget(5, 6, widget);
  QStringList headers = {"#",        "机器人信息", "底盘功率上限",
                         "热量上限", "热量冷却",   "射速上限"};
  table->setHorizontalHeaderLabels(headers);

  // 表格样式
  table->verticalHeader()->setVisible(false);
  table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
  table->setColumnWidth(0, 40); // ID 列宽度
  table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
  table->setColumnWidth(1, 180); // 机器人信息列宽度

  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table->setSelectionMode(QAbstractItemView::NoSelection);
  table->setShowGrid(false);
  table->setStyleSheet(
      "QTableWidget { background-color: rgba(20, 20, 30, 200); border: none; "
      "color: white; font-size: 12px; }"
      "QHeaderView::section { background-color: rgba(40, 40, 50, 200); color: "
      "#AAA; border: none; padding: 8px; font-weight: bold; }"
      "QTableWidget::item { border-bottom: 1px solid #333; padding: 5px; }");

  layout->addWidget(table);
  return widget;
}

void InfoPanel::updateTableData(QTableWidget *table,
                                const QList<RobotInfo> &data, bool isRed) {
  table->setRowCount(data.size());

  for (int i = 0; i < data.size(); ++i) {
    const RobotInfo &info = data[i];

    // 1. ID
    QTableWidgetItem *idItem = new QTableWidgetItem(QString::number(info.id));
    idItem->setTextAlignment(Qt::AlignCenter);
    table->setItem(i, 0, idItem);

    // 2. 机器人信息（自定义控件）
    QWidget *infoWidget = new QWidget();
    QHBoxLayout *infoLayout = new QHBoxLayout(infoWidget);
    infoLayout->setContentsMargins(5, 2, 5, 2);
    infoLayout->setSpacing(10);

    // 图标
    QLabel *iconLabel = new QLabel(infoWidget);
    iconLabel->setFixedSize(32, 32);
    const QString iconPath = ":/images/robot_avatar.png";
    QPixmap iconPixmap(iconPath);
    if (iconPixmap.isNull()) {
      const QString fallbackPath =
          isRed ? QStringLiteral(":/images/red_robot_icon.png")
                : QStringLiteral(":/images/blue_robot_icon.png");
      iconPixmap.load(fallbackPath);
      if (iconPixmap.isNull()) {
        qDebug() << "Failed to load icon from resource path:" << iconPath
                 << "fallback:" << fallbackPath;
      }
    }

    if (!iconPixmap.isNull()) {
      iconLabel->setPixmap(iconPixmap.scaled(32, 32, Qt::KeepAspectRatio,
                                             Qt::SmoothTransformation));
    } else {
      // 如果都失败了，显示一个占位色块或文字
      iconLabel->setText("Icon");
      iconLabel->setStyleSheet("background-color: red; color: white;");
    }

    // 文字和状态条
    QWidget *textWidget = new QWidget(infoWidget);
    QVBoxLayout *textLayout = new QVBoxLayout(textWidget);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(2);

    QLabel *typeLabel = new QLabel(info.type, textWidget);
    typeLabel->setStyleSheet(
        QString("color: %1; font-weight: bold; font-size: 12px;")
            .arg(isRed ? "#FF8888" : "#8888FF"));

    QProgressBar *hpBar = new QProgressBar(textWidget);
    hpBar->setRange(0, info.maxHp);
    hpBar->setValue(info.currentHp);
    hpBar->setTextVisible(true);
    hpBar->setFormat(QString("%1/%2").arg(info.currentHp).arg(info.maxHp));
    hpBar->setFixedHeight(12);
    hpBar->setStyleSheet(
        QString("QProgressBar { border: none; background: #333; border-radius: "
                "2px; color: white; font-size: 10px; }"
                "QProgressBar::chunk { background: %1; border-radius: 2px; }")
            .arg(isRed ? "#FF3232" : "#3264FF"));

    textLayout->addWidget(typeLabel);
    textLayout->addWidget(hpBar);

    infoLayout->addWidget(iconLabel);
    infoLayout->addWidget(textWidget);

    table->setCellWidget(i, 1, infoWidget);

    // 3. 其它统计
    auto setItem = [&](int col, QString text) {
      QTableWidgetItem *item = new QTableWidgetItem(text);
      item->setTextAlignment(Qt::AlignCenter);
      table->setItem(i, col, item);
    };

    setItem(2, info.powerLimit > 0 ? QString::number(info.powerLimit) : "-");
    setItem(3, info.heatLimit > 0 ? QString::number(info.heatLimit) : "-");
    setItem(4,
            info.heatCooldown > 0 ? QString::number(info.heatCooldown) : "-");
    setItem(5,
            info.fireRateLimit > 0 ? QString::number(info.fireRateLimit) : "-");
  }

  // 避免表格抢占焦点，影响 Tab 快捷键。
  table->setFocusPolicy(Qt::NoFocus);
}

void InfoPanel::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  // 深色半透明背景
  p.fillRect(rect(), QColor(10, 10, 15, 245));

  // 金色边框
  p.setPen(QPen(QColor(255, 215, 0), 2));
  p.drawRect(rect().adjusted(1, 1, -1, -1));
}

void InfoPanel::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Tab || event->key() == Qt::Key_Escape) {
    emit closed();
    event->accept();
  } else {
    QWidget::keyPressEvent(event);
  }
}

void InfoPanel::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);
  m_updateTimer->start(1000);
  updateData();
}

void InfoPanel::updateData() {
  // 更新演示数据
  static int time = 420;
  time--;
  if (time < 0)
    time = 420;

  int min = time / 60;
  int sec = time % 60;
  m_timeLabel->setText(QString("%1:%2").arg(min).arg(sec, 2, 10, QChar('0')));

  // 刷新表格
  updateTableData(m_redTable, m_redTeamData, true);
  updateTableData(m_blueTable, m_blueTeamData, false);

  // 刷新顶部演示统计
  int redHp = 0;
  for (const auto &r : m_redTeamData)
    redHp += r.currentHp;
  m_redTotalHpLabel->setText(QString("机器人总剩余血量 %1").arg(redHp));

  int blueHp = 0;
  for (const auto &b : m_blueTeamData)
    blueHp += b.currentHp;
  m_blueTotalHpLabel->setText(QString("机器人总剩余血量 %1").arg(blueHp));
}

} // namespace RM
