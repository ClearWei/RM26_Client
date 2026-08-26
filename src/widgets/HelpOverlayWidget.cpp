#include "HelpOverlayWidget.h"

#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QResizeEvent>
#include <QVBoxLayout>

namespace RM {

HelpOverlayWidget::HelpOverlayWidget(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_StyledBackground, false);
  setFocusPolicy(Qt::NoFocus);

  m_panel = new QWidget(this);
  m_panel->setObjectName("helpOverlayPanel");
  m_panel->setFixedSize(760, 340);
  m_panel->setAttribute(Qt::WA_StyledBackground, true);
  m_panel->setStyleSheet(
      "#helpOverlayPanel {"
      " background: rgba(18, 20, 24, 238);"
      " border: 1px solid rgba(80, 90, 98, 220);"
      " border-radius: 14px;"
      "}");

  auto *shadow = new QGraphicsDropShadowEffect(m_panel);
  shadow->setBlurRadius(28);
  shadow->setOffset(0, 10);
  shadow->setColor(QColor(0, 0, 0, 120));
  m_panel->setGraphicsEffect(shadow);

  auto *mainLayout = new QVBoxLayout(m_panel);
  mainLayout->setContentsMargins(36, 28, 36, 28);
  mainLayout->setSpacing(18);

  auto *title = new QLabel(QStringLiteral("帮助提示"), m_panel);
  title->setStyleSheet(
      "color: #F3F6F7; font-family: 'Microsoft YaHei';"
      " font-size: 26px; font-weight: 700; border: none; background: transparent;");
  mainLayout->addWidget(title);

  auto *subtitle = new QLabel(QStringLiteral("按住 F12 查看，松开即关闭"), m_panel);
  subtitle->setStyleSheet(
      "color: rgba(220, 228, 232, 180); font-family: 'Microsoft YaHei';"
      " font-size: 13px; border: none; background: transparent;");
  mainLayout->addWidget(subtitle);

  auto *grid = new QGridLayout();
  grid->setHorizontalSpacing(18);
  grid->setVerticalSpacing(14);

  grid->addWidget(createHelpItem(QStringLiteral("P"),
                                 QStringLiteral("设置面板，可设置客户端 ID、鼠标灵敏度和自定义数据"),
                                 m_panel),
                  0, 0);
  grid->addWidget(createHelpItem(QStringLiteral("~"),
                                 QStringLiteral("显示伤害、模块状态、扣血与占比等信息"),
                                 m_panel),
                  0, 1);
  grid->addWidget(createHelpItem(QStringLiteral("Tab"),
                                 QStringLiteral("显示双方机器人状态、性能体系、经验与伤害数据"),
                                 m_panel),
                  1, 0);
  grid->addWidget(createHelpItem(QStringLiteral("O / I"),
                                 QStringLiteral("弹药补给面板，按规则补给 17mm / 42mm 弹"),
                                 m_panel),
                  1, 1);
  grid->addWidget(createHelpItem(QStringLiteral("M"),
                                 QStringLiteral("打开大地图，配合 A / B / I 进行战术标记"),
                                 m_panel),
                  2, 0);
  grid->addWidget(createHelpItem(QStringLiteral("H"),
                                 QStringLiteral("打开兑换或支援面板，执行远程兑换与相关操作"),
                                 m_panel),
                  2, 1);

  mainLayout->addLayout(grid);
}

QWidget *HelpOverlayWidget::createHelpItem(const QString &keyText,
                                           const QString &description,
                                           QWidget *parent) {
  auto *item = new QWidget(parent);
  item->setAttribute(Qt::WA_StyledBackground, true);
  item->setStyleSheet(
      "background: rgba(255, 255, 255, 0.02);"
      "border: 1px solid rgba(255, 255, 255, 0.04);"
      "border-radius: 10px;");

  auto *layout = new QHBoxLayout(item);
  layout->setContentsMargins(14, 14, 14, 14);
  layout->setSpacing(14);

  auto *keyLabel = new QLabel(keyText, item);
  keyLabel->setAlignment(Qt::AlignCenter);
  keyLabel->setFixedSize(keyText == QStringLiteral("Tab") ? 64 : 54, 38);
  keyLabel->setStyleSheet(
      "background: rgba(12, 14, 18, 0.95);"
      "border: 1px solid rgba(92, 96, 108, 0.85);"
      "border-radius: 8px;"
      "color: #F7FAFC;"
      "font-family: 'Microsoft YaHei';"
      "font-size: 18px;"
      "font-weight: 700;");

  auto *descLabel = new QLabel(description, item);
  descLabel->setWordWrap(true);
  descLabel->setStyleSheet(
      "background: transparent; border: none;"
      "color: rgba(231, 236, 239, 0.92);"
      "font-family: 'Microsoft YaHei';"
      "font-size: 14px;"
      "line-height: 1.45;");

  layout->addWidget(keyLabel, 0, Qt::AlignTop);
  layout->addWidget(descLabel, 1);

  return item;
}

void HelpOverlayWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), QColor(8, 10, 14, 132));
}

void HelpOverlayWidget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);

  if (!m_panel) {
    return;
  }

  m_panel->move((width() - m_panel->width()) / 2,
                (height() - m_panel->height()) / 2);
}

} // namespace RM
