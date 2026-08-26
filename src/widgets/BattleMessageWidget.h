#ifndef BATTLEMESSAGEWIDGET_H
#define BATTLEMESSAGEWIDGET_H

#include <QLabel>
#include <QPropertyAnimation>
#include <QTimer>
#include <QWidget>
#include <QGraphicsOpacityEffect>
#include <QSoundEffect>
#include "../core/GameData.h"

namespace RM {

/**
 * @brief 战场信息提示组件
 * 显示临时战场消息，带淡出动画效果
 */
class BattleMessageWidget : public QWidget {
    Q_OBJECT

public:
    explicit BattleMessageWidget(GameData *gameData, QWidget *parent = nullptr);

    /**
   * @brief 显示消息
   * @param message 要显示的消息文本
   * @param duration 显示持续时间(毫秒)，0表示不自动消失
   */
    void showMessage(const QString &message, int duration = 3000);
    void setSoundVolume(qreal volume);

    /**
   * @brief 立即隐藏消息
   */
    void hideMessage();

    // 新版接口
    /**
   * @brief 显示击杀信息
   * @param killerId 击杀者ID
   * @param killerIsRed 击杀者是否为红方
   * @param victimId 被击杀者ID
   * @param victimIsRed 被击杀者是否为红方
   * @param isFirstBlood 是否为第一滴血
   */
    void showKill(int killerId, bool killerIsRed, int victimId, bool victimIsRed, bool isFirstBlood = false, int killStreak = 0);
    void showOutpostDestroyed(bool isRedOutpost);
    void showBaseDestroyed(bool isRed);

    /**
     * @brief 显示能量机关状态变化
     * @param data 能量机关数据
     */
    void showRuneActivable(int runeType);
    void showRuneActived(int runeType);
    void showAirSupportStarted(bool isRedTeam);
    void showDartGateOpened(bool isRedTeam, bool isEnemyTeam);
    void showBaseUnderAttack(bool isRedTeam);
    void showEnemyOutpostStopped(bool isRedTeam);
    void showEnemyBaseShieldOpened(bool isRedTeam);

    /**
     * @brief 显示基地状态变化消息
     * @param isRed 是否为红方基地
     * @param status 基地状态 (1:解除无敌护甲未展开, 2:解除无敌护甲展开)
     */
    void showBaseStatusChange(bool isRed, int status);

    /**
     * @brief 显示前哨站状态变化消息 (无敌解除)
     * @param isRed 是否为红方前哨站
     * @param status 前哨站状态
     */
    void showOutpostStatusChange(bool isRed, int status);

    /**
     * @brief 显示结构化的裁判警告信息
     * @param data 警告数据
     */
    void showRefereeWarning(const quint8 panaltytype,const quint8 robotid,
                                               quint8 penalty_effect_sec,const quint8 penaltycardcount);

    /**
     * @brief 处理击杀事件
     * @details 包含中央大字提示和侧边日志记录
     * @param record 击杀记录
     */
    void processKillEvent(const KillRecord &record);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onFadeFinished();


private:
    void drawAvatar(QPainter &p, const QRect &rect, int id, bool isRed);
    void drawKill(QPainter &p);
    void drawRefereeWarning(QPainter &p);
    void drawNormal(QPainter &p);
    void drawOutpostDestroyed(QPainter &p);
    void drawBaseDestroyed(QPainter &p);
    void drawRune(QPainter &p);
    void drawRuneActived(QPainter &p);
    void drawAirSupportStarted(QPainter &p);
    void drawDartGateOpened(QPainter &p);
    void drawBattlefieldNotice(QPainter &p);
    void drawBaseStatusChange(QPainter &p);
    void drawOutpostStatusChange(QPainter &p);

private:
    QString m_message; //普通信息
    QTimer *m_hideTimer;
    QPropertyAnimation *m_fadeAnimation;
    qreal m_opacity;
    bool m_isVisible;

    enum class MessageType {
        None,
        Normal,
        Kill,
        Warning,
        OutpostDestroyed,
        BaseDestroyed,
        Rune,
        RuneActived,
        AirSupportStarted,
        DartGateOpened,
        BattlefieldNotice,
        BaseStatusChange,
        OutpostStatusChange
    }m_type;

    RuneData m_runeData;

    struct WarningInfo {
        quint8 panaltytype = 0;
        quint8 robotid = 0;
        quint8 penalty_effect_sec = 0;
        quint8 penaltycardcount = 0;
        QColor color;
    }m_warningInfo;

    struct KillInfo {
        int killerId;
        bool killerIsRed;
        int victimId;
        bool victimIsRed;
        bool isFirstBlood;
        int killStreak;
    } m_killInfo;

    struct OutpostDestroyedInfo {
        bool isRedOutpost;
        int killerId;
        bool killerIsRed;
    } m_outpostDestroyedInfo;

    struct BaseStatusChangeInfo {
        bool isRed;
        int status;
    } m_baseStatusChangeInfo;

    struct OutpostStatusChangeInfo {
        bool isRed;
        int status;
    } m_outpostStatusChangeInfo;

    struct AirSupportInfo {
        bool isRedTeam = true;
    } m_airSupportInfo;

    struct DartGateInfo {
        bool isRedTeam = true;
        bool isEnemyTeam = true;
    } m_dartGateInfo;

    struct BattlefieldNoticeInfo {
        bool isRedTeam = true;
        QString text;
    } m_battlefieldNoticeInfo;

    // 预加载音效，确保战场触发无延迟
    QSoundEffect *m_killSound;
    QSoundEffect *m_firstBloodSound;
    QSoundEffect *m_killStreakSounds[6]; // 索引 2-5 对应双杀到五杀

    GameData *m_gameData; // 数据中心指针
};

} // namespace RM

#endif // BATTLEMESSAGEWIDGET_H
