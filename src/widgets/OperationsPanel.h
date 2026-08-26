#ifndef OPERATIONSPANEL_H
#define OPERATIONSPANEL_H

#include <QWidget>

class QLabel;

namespace RM {

/**
 * @brief ~ 键 - 快捷操作面板
 */
class OperationsPanel : public QWidget {
  Q_OBJECT

public:
  explicit OperationsPanel(QWidget *parent = nullptr);

protected:
  void paintEvent(QPaintEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private:
  void setupUI();

signals:
  void closed();
  void commandTriggered(int cmdType, int targetId);
};

} // namespace RM

#endif // OPERATIONSPANEL_H
