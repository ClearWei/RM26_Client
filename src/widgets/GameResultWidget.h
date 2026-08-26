#ifndef GAMERESULTWIDGET_H
#define GAMERESULTWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QPixmap>
#include <QStringList>
#include <QMap>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QTimer>
#include <QSoundEffect>
#include <QKeyEvent>

class GameResultWidget : public QWidget
{
    Q_OBJECT

public:
    enum GameResult {
        RedWin,
        BlueWin,
        Draw,
        Terminated
    };

    enum WinReason {
        BaseDestroyed,
        TimeUp,
        Surrender,
        RefereeTerminated
    };

    // 单机器人统计
    struct RobotStats {
        QString name;
        int currentHP;
        int maxHP;
        int damageDealt;
        int damageTaken;
        int kills;
        int deaths;
        bool isAlive;

        RobotStats() : currentHP(0), maxHP(0), damageDealt(0), damageTaken(0), kills(0), deaths(0), isAlive(true) {}
    };

    explicit GameResultWidget(QWidget *parent = nullptr);

    void showResult(GameResult result, WinReason reason = TimeUp);
    void setResolutionViewport(const QSize &viewport);
    QSize resolutionViewport() const { return m_resolutionViewport; }
    static QSize designSize() { return QSize(1200, 800); }
    void setPlayVictoryOnShow(bool enabled) { m_playVictoryOnShow = enabled; }
    bool playVictoryOnShow() const { return m_playVictoryOnShow; }
    void setGameData(int redScore, int blueScore, int round, const QString& gameTime);
    void setTeamData(const QString& redTeam, const QString& blueTeam,
                     const QString& redSchool, const QString& blueSchool);
    void setDetailedStats(const QMap<QString, QVariant>& stats);
    void setRobotStats(const QMap<int, RobotStats>& redRobots, const QMap<int, RobotStats>& blueRobots);
    void setSoundVolume(qreal volume); //设置音量
    void setEnhancedGameStats(int redBaseHP, int blueBaseHP, int redOutpostHP, int blueOutpostHP,
                             int redSentryHP, int blueSentryHP, int redEnergyActivations,
                             int blueEnergyActivations, const QString& mvpPlayer);

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void setupUI();
    void setupAnimations();
    void drawBackground(QPainter& painter);
    void drawHeader(QPainter& painter);
    void drawStatsComparison(QPainter& painter);
    void drawRobotLists(QPainter& painter);
    QSize scaledContentSize() const;
    void resizeToScaledContent();

    // 绘制单行两队对比统计
    void drawStatRow(QPainter& painter, int y, const QString& label, int redValue, int blueValue, int maxValue, const QString& iconPath);

    // 结算动画资源
    void loadVictoryFrames(GameResult result);
    QStringList m_victoryFramePaths;
    QPixmap m_currentFramePixmap;
    QTimer* m_frameTimer;
    int m_currentFrameIndex;

    // 战报数据
    QMap<QString, QPixmap> m_iconCache;
    bool m_showStats;
    QSize m_resolutionViewport = QSize(1920, 1080);
    GameResult m_result;
    WinReason m_reason;
    int m_redScore;
    int m_blueScore;
    int m_round;
    QString m_gameTime;
    QString m_redTeam;
    QString m_blueTeam;
    QString m_redSchool;
    QString m_blueSchool;

    // 动画对象
    QPropertyAnimation *m_scaleAnimation;
    QGraphicsOpacityEffect *m_opacityEffect;

    // 音效
    QSoundEffect* m_victorySound;
    bool m_playVictoryOnShow = true; // 默认播放，可由外部显式关闭。

    // 两队机器人统计
    QMap<int, RobotStats> m_redRobots;
    QMap<int, RobotStats> m_blueRobots;

    // 其他比赛统计
    int m_redBaseHP;
    int m_blueBaseHP;
    int m_redOutpostHP;
    int m_blueOutpostHP;
    int m_redSentryHP;
    int m_blueSentryHP;
    int m_redEnergyActivations;
    int m_blueEnergyActivations;
    // 破甲次数暂未在界面展示，字段保留用于兼容旧调用。
    int m_redArmorBreaks;
    int m_blueArmorBreaks;
    QString m_mvpPlayer;
    // 协议提供的队伍汇总值，在机器人明细缺失时兜底。
    int m_protocolRedTotalDamage = 0;
    int m_protocolBlueTotalDamage = 0;

signals:
    void closeRequested();
};

#endif // GAMERESULTWIDGET_H
