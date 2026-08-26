#include "OperationsPanel.h"
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

namespace RM {

OperationsPanel::OperationsPanel(QWidget *parent) : QWidget(parent) {
  setFixedSize(500, 350);
  setupUI();

  setStyleSheet("OperationsPanel { "
                "  background-color: rgba(30, 40, 30, 230); "
                "  border: 2px solid #00FF00; "
                "  border-radius: 10px; "
                "}"
                "QLabel { color: white; }"
                "QPushButton { "
                "  background-color: #00AA00; "
                "  color: white; "
                "  border: none; "
                "  padding: 10px; "
                "  border-radius: 5px; "
                "  font-size: 14px; "
                "}"
                "QPushButton:hover { background-color: #00DD00; }");
}

void OperationsPanel::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(20, 20, 20, 20);
  mainLayout->setSpacing(15);

  // 标题
  QLabel *titleLabel = new QLabel("快捷操作 (按 ~ 或 ESC 关闭)", this);
  QFont titleFont;
  titleFont.setPixelSize(22);
  titleFont.setBold(true);
  titleLabel->setFont(titleFont);
  titleLabel->setAlignment(Qt::AlignCenter);
  mainLayout->addWidget(titleLabel);

  // 操作按钮
  QVBoxLayout *buttonsLayout = new QVBoxLayout();
  buttonsLayout->setSpacing(10);

  QPushButton *btn1 = new QPushButton("🎯 请求补给");
  QPushButton *btn2 = new QPushButton("⚠ 发送警告");
  QPushButton *btn3 = new QPushButton("📍 标记位置");
  QPushButton *btn4 = new QPushButton("🔧 快速维修");

  connect(btn1, &QPushButton::clicked, this,
          [this]() { emit commandTriggered(0, 0); });
  connect(btn2, &QPushButton::clicked, this,
          [this]() { emit commandTriggered(1, 0); });
  connect(btn3, &QPushButton::clicked, this,
          [this]() { emit commandTriggered(2, 0); });
  connect(btn4, &QPushButton::clicked, this,
          [this]() { emit commandTriggered(3, 0); });

  buttonsLayout->addWidget(btn1);
  buttonsLayout->addWidget(btn2);
  buttonsLayout->addWidget(btn3);
  buttonsLayout->addWidget(btn4);

  mainLayout->addLayout(buttonsLayout);
  mainLayout->addStretch();

  // 提示
  QLabel *hintLabel = new QLabel("按 ~ 或 ESC 关闭此面板", this);
  hintLabel->setStyleSheet("color: #AAA; font-size: 14px;");
  hintLabel->setAlignment(Qt::AlignCenter);
  mainLayout->addWidget(hintLabel);
}

void OperationsPanel::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QWidget::paintEvent(event);
}

void OperationsPanel::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_AsciiTilde || event->key() == Qt::Key_Escape) {
    emit closed();
    event->accept();
  } else {
    QWidget::keyPressEvent(event);
  }
}

} // namespace RM
