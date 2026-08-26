#ifndef ROBOTSTATUSPANEL_H
#define ROBOTSTATUSPANEL_H

#include <QWidget>
#include "../ui/LayoutConstants.h"

namespace RM {

/**
 * @brief 机器人状态面板
 * 显示单个机器人的状态信息：ID、类型、等级、血量、状态
 */
class RobotStatusPanel : public QWidget {
    Q_OBJECT

public:
    enum Team {
        Red,
        Blue
    };

    explicit RobotStatusPanel(Team team, QWidget* parent = nullptr);

    // 设置数据
    void setRobotId(int id);
    void setRobotType(RobotType type);
    void setLevel(int level);
    void setHealth(int current, int max);
    void setStatus(RobotStatus status);
    void setBuffs(const QList<BuffType>& buffs);

    // 获取数据
    int robotId() const { return m_robotId; }
    RobotType robotType() const { return m_robotType; }
    int level() const { return m_level; }
    int currentHealth() const { return m_currentHealth; }
    int maxHealth() const { return m_maxHealth; }
    RobotStatus status() const { return m_status; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

signals:
    void robotSelected(int robotId);

private:
    void updateStyle();
    QString getRobotTypeIcon() const;
    QString getRobotTypeName() const;
    QColor getStatusColor() const;
    void drawBuffIcons(QPainter& painter);

    Team m_team;
    int m_robotId;
    RobotType m_robotType;
    int m_level;
    int m_currentHealth;
    int m_maxHealth;
    RobotStatus m_status;
    QList<BuffType> m_buffs;
    bool m_hovered;
    bool m_selected;

    // 样式
    QColor m_borderColor;
    QColor m_backgroundColor;
    QColor m_textColor;
};

} // namespace RM

#endif // ROBOTSTATUSPANEL_H