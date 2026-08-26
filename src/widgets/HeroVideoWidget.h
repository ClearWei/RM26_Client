#ifndef HEROVIDEOWIDGET_H
#define HEROVIDEOWIDGET_H

#include <QImage>
#include <QWidget>

namespace RM {

class HeroVideoWidget : public QWidget {
  Q_OBJECT

public:
  explicit HeroVideoWidget(QWidget *parent = nullptr);

  void setFrame(const QImage &frame);
  void setCurrentRobotId(int robotId);
  void setForceVisible(bool forceVisible);

signals:
  void visibilityChanged(bool visible);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  void updateEffectiveVisibility();

  QImage m_frame;
  int m_currentRobotId = -1;
  bool m_forceVisible = false;
  bool m_lastEffectiveVisible = false;
};

} // namespace RM

#endif // HEROVIDEOWIDGET_H
