#ifndef HELPOVERLAYWIDGET_H
#define HELPOVERLAYWIDGET_H

#include <QWidget>

namespace RM {

class HelpOverlayWidget : public QWidget {
  Q_OBJECT

public:
  explicit HelpOverlayWidget(QWidget *parent = nullptr);

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  QWidget *createHelpItem(const QString &keyText, const QString &description,
                          QWidget *parent);

  QWidget *m_panel = nullptr;
};

} // namespace RM

#endif // HELPOVERLAYWIDGET_H
