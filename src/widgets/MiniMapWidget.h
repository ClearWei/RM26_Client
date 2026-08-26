#ifndef MINIMAPWIDGET_H
#define MINIMAPWIDGET_H

#include <QMap>
#include <QPointF>
#include <QWidget>
#include <QTimer>

namespace RM {

class MiniMapWidget : public QWidget {
  Q_OBJECT

public:
  explicit MiniMapWidget(QWidget *parent = nullptr);
  void updateRobotPosition(int id, bool isRedTeam, const QPointF &pos, float angle, int isHighLight);
  void updateRobotState(int id, bool isRedTeam, float currentHP);
  void updateBaseState(bool isRedTeam);
  void updateOutpostState(bool isRedTeam, float currentHP);
  void updateCenterPoint(bool active);
  /**
   *@brief 获取当前操控机器人的id
   *param robotId 机器人id
   */
   void setCurrentRobotId(int robotId);

signals:
  void mapClicked(float x, float y);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;

private:
  struct RobotMarker {
    bool isRed;
    QPointF pos;
    float angle;            // 角度
    int isHighLight;        // 是否被锁定
    bool isDead=false;            //是否死亡
  };

  //--- 基础属性 ---
  QMap<int, RobotMarker> m_markers;
  bool m_centerActive;
  int m_currentRobotId;    //当前操作的机器人id
  QMap<bool, bool>  m_isBaseAtacked;    //是否是红色基地:是否被攻击
  QMap<bool, bool>  m_isOutpostDead;     //是否是红方前哨战：是否死亡

  //--- 呼吸动画属性 ---
  QTimer *m_breathTimer;   //呼吸动画定时器
  float m_breathPhase;     //动画相位 (0 ~ 2*PI)

  //--- 基地警报停止定时器（分阵营） ---
  QTimer *m_redBaseTimer = nullptr;
  QTimer *m_blueBaseTimer = nullptr;

  //--- 辅助绘图函数 ---
  /**
   *@brief 绘制地图元素
   *param p 绘图上下文
   *param rect 绘制区域
   *param imagePath 图片路径
   *param relX 相对x坐标 (0 ~ 1)
   *param relY 相对y坐标 (0 ~ 1)
   *param scale 缩放比例 (默认0.5)
   */
  void drawMapElement(QPainter &p, const QRect &r, const QString &imagePath,
                        float relX, float relY, float scale = 0.5f);

  /**
   *@brief 绘制机器人标记
   *param p 绘图上下文
   *param rect 绘制区域
   *param mk 机器人标记
   *param robotId 机器人id
   *param scale 缩放比例 (默认0.5)
   */
   void drawRobotMarker(QPainter &p, const QRect &r, const RobotMarker &mk,
                          const int robotId, float scale );
};

} // namespace RM

#endif // MINIMAPWIDGET_H
