#include "BattleMessageWidget.h"
#include "../ui/LayoutConstants.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QStringList>
#include <QUrl>
#include <QtGlobal>

namespace RM {

BattleMessageWidget::BattleMessageWidget(GameData *gameData, QWidget *parent)
    : QWidget(parent), m_opacity(0.0), m_isVisible(false),
      m_type(MessageType::None), m_gameData(gameData) {
  // 设置窗口属性
  setAttribute(Qt::WA_TransparentForMouseEvents); // 穿透鼠标事件
  setAttribute(Qt::WA_TranslucentBackground);

  // 击杀语音来自可选资源包；文件缺失或只是占位时保持静默。
  auto tryLoadSound = [](QSoundEffect *s, const QString &aliasPath) {
    const QString fileName = QUrl(aliasPath).fileName();
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        aliasPath,
        appDir + "/resources/sounds/" + fileName,
        appDir + "/../resources/sounds/" + fileName,
        appDir + "/../Resources/sounds/" + fileName,
        QDir::currentPath() + "/resources/sounds/" + fileName,
    };

    for (const QString &candidate : candidates) {
      const bool isQrc = candidate.startsWith("qrc:");
      const QString probePath = isQrc ? candidate.mid(3) : candidate;
      QFile file(probePath);
      if (!file.open(QIODevice::ReadOnly) || file.size() <= 1024) {
        continue;
      }
      s->setSource(isQrc ? QUrl(candidate)
                         : QUrl::fromLocalFile(
                               QFileInfo(candidate).absoluteFilePath()));
      return;
    }
  };

  m_killSound = new QSoundEffect(this);
  m_killSound->setVolume(0.5);
  tryLoadSound(m_killSound, "qrc:/sounds/dead.wav");

  m_firstBloodSound = new QSoundEffect(this);
  m_firstBloodSound->setVolume(0.5);
  tryLoadSound(m_firstBloodSound, "qrc:/sounds/firstblood.wav");

  for (int i = 2; i <= 5; ++i) {
      m_killStreakSounds[i] = new QSoundEffect(this);
      m_killStreakSounds[i]->setVolume(0.5);
      tryLoadSound(m_killStreakSounds[i], QString("qrc:/sounds/%1kill.wav").arg(i));
  }

  // 创建淡出动画
  m_fadeAnimation = new QPropertyAnimation(this, "windowOpacity");
  m_fadeAnimation->setDuration(500); // 500ms淡出
  m_fadeAnimation->setStartValue(1.0);
  m_fadeAnimation->setEndValue(0.0);
  connect(m_fadeAnimation, &QPropertyAnimation::finished, this,
          &BattleMessageWidget::onFadeFinished);

  // 创建隐藏定时器
  m_hideTimer = new QTimer(this);
  m_hideTimer->setSingleShot(true);
  connect(m_hideTimer, &QTimer::timeout, this,
          [this]() { m_fadeAnimation->start(); });

  // 初始不显示（等待第一条消息）
  setWindowOpacity(0.0);
}

//设置音量函数
void BattleMessageWidget::setSoundVolume(qreal volume) {
  const qreal clamped = qBound<qreal>(0.0, volume, 1.0);
  if (m_killSound) {
    m_killSound->setVolume(clamped);
  }
  if (m_firstBloodSound) {
    m_firstBloodSound->setVolume(clamped);
  }
  for (int i = 2; i <= 5; ++i) {
    if (m_killStreakSounds[i]) {
      m_killStreakSounds[i]->setVolume(clamped);
    }
  }
}

void BattleMessageWidget::showMessage(const QString &message, int duration) {
  m_message = message;
  m_isVisible = true;
  m_opacity = 1.0;

  // 停止之前的动画
  m_fadeAnimation->stop();
  m_hideTimer->stop();

  // 显示组件
  setWindowOpacity(1.0);
  show();
  update();

  // 如果设置了持续时间，启动定时器
  if (duration > 0) {
    m_hideTimer->start(duration);
  }
}



void BattleMessageWidget::showRefereeWarning(const quint8 panaltytype,const quint8 robotid,
                                             const quint8 penalty_effect_sec,const quint8 penaltycardcount) {
    m_type = MessageType::Warning;

    // 颜色逻辑 (保持原样)
    uint32_t colorHex = 0xFF0000; // 默认红色

    // 如果是 MQTT 的 PenaltyInfo，类型可能不同，需要根据文档映射
    // 假设: 1=黄牌, 2=双方黄牌, 3=红牌, 4=超功率, 5=超热量, 6=超射速

    switch(panaltytype) {
        case 1: colorHex = 0xFFD700; break;
        case 2: colorHex = 0xFFD700; break;
        case 3: colorHex = 0xFFD700; break;
        default: break;
    }


    QColor color = QColor::fromRgb(colorHex & 0xFFFFFF);
    m_warningInfo = {panaltytype, robotid, penalty_effect_sec, penaltycardcount, color};

    m_isVisible = true;
    m_opacity = 1.0;

    m_fadeAnimation->stop();
    m_hideTimer->stop();

    setWindowOpacity(1.0);
    show();
    update();   // 触发 paintEvent → drawRefereeWarning

    m_hideTimer->start(3000);
}

void BattleMessageWidget::processKillEvent(const KillRecord &record) {
    if (!m_gameData) return;

    const RobotData *killer = m_gameData->getRobotById(record.killerId);
    const RobotData *victim = m_gameData->getRobotById(record.victimId);

    if (victim) {
        bool victimRed = (victim->team == TeamColor::RED);
        // 如果有明确击杀者，用击杀者的队伍；如果没有(killer=0)，则认为是敌对阵营造成的击杀
        bool killerRed = killer ? (killer->team == TeamColor::RED) : !victimRed;

        // 1. 显示中央大字提示 (调用现有逻辑)
        showKill(record.killerId, killerRed, record.victimId,
                 victimRed, record.isFirstBlood, record.killStreak);

        // 2. 生成击杀播报 (HTML Log) 并添加到 GameData
        QString killerName = killer ? killer->name : "未知";
        QString killerColor = killerRed ? "#FF5050" : "#50A0FF";

        QString victimName = victim->name;
        QString victimColor = victimRed ? "#FF5050" : "#50A0FF";

        QString message;
        if (record.killerId == 0 || !killer) {
            // 阵亡消息
            message = QString("<font color='%1'>%2</font> <font color='#FFFFFF'>阵亡</font>")
                          .arg(victimColor, victimName);
        } else {
            // 击杀消息
            message = QString("<font color='%1'>%2</font> <font color='#FFFFFF'>击杀</font> <font color='%3'>%4</font>")
                          .arg(killerColor, killerName, victimColor, victimName);
        }

        m_gameData->addRobotMessage(message);
    }
}


void BattleMessageWidget::showKill(int killerId, bool killerIsRed, int victimId,
                                   bool victimIsRed, bool isFirstBlood, int killStreak){
    m_type = MessageType::Kill;

    m_killInfo = {killerId, killerIsRed, victimId, victimIsRed, isFirstBlood, killStreak};
    m_isVisible = true;
    m_opacity = 1.0;

    // --- 音效播放逻辑 (优先级: 连杀 > 第一滴血 > 普通击杀) ---
    if (killStreak >= 2 && killStreak <= 5 && m_killStreakSounds[killStreak]->source().isValid()) {
        m_killStreakSounds[killStreak]->stop();
        m_killStreakSounds[killStreak]->play();
    } else if (isFirstBlood && m_firstBloodSound->source().isValid()) {
        m_firstBloodSound->stop();
        m_firstBloodSound->play();
    } else if (m_killSound->source().isValid()) {
        m_killSound->stop();
        m_killSound->play();
    }

    // 停止之前的动画
    m_fadeAnimation->stop();
    m_hideTimer->stop();

    // 显示组件
    setWindowOpacity(1.0);
    show();
    update();

    // 击杀消息显示 3 秒
    m_hideTimer->start(3000);
}

void BattleMessageWidget::showAirSupportStarted(bool isRedTeam) {
    m_type = MessageType::AirSupportStarted;
    m_airSupportInfo = {isRedTeam};
    m_isVisible = true;
    m_opacity = 1.0;

    m_fadeAnimation->stop();
    m_hideTimer->stop();

    setWindowOpacity(1.0);
    show();
    update();

    m_hideTimer->start(3000);
}

void BattleMessageWidget::showDartGateOpened(bool isRedTeam, bool isEnemyTeam) {
    m_type = MessageType::DartGateOpened;
    m_dartGateInfo = {isRedTeam, isEnemyTeam};
    m_isVisible = true;
    m_opacity = 1.0;

    m_fadeAnimation->stop();
    m_hideTimer->stop();

    setWindowOpacity(1.0);
    show();
    update();

    m_hideTimer->start(3000);
}

void BattleMessageWidget::showBaseUnderAttack(bool isRedTeam) {
    m_type = MessageType::BattlefieldNotice;
    m_battlefieldNoticeInfo = {isRedTeam, QStringLiteral("己方基地遭到攻击")};
    m_isVisible = true;
    m_opacity = 1.0;

    m_fadeAnimation->stop();
    m_hideTimer->stop();

    setWindowOpacity(1.0);
    show();
    update();

    m_hideTimer->start(3000);
}

void BattleMessageWidget::showEnemyOutpostStopped(bool isRedTeam) {
    m_type = MessageType::BattlefieldNotice;
    m_battlefieldNoticeInfo = {isRedTeam, QStringLiteral("对方前哨站停转")};
    m_isVisible = true;
    m_opacity = 1.0;

    m_fadeAnimation->stop();
    m_hideTimer->stop();

    setWindowOpacity(1.0);
    show();
    update();

    m_hideTimer->start(3000);
}

void BattleMessageWidget::showEnemyBaseShieldOpened(bool isRedTeam) {
    m_type = MessageType::BattlefieldNotice;
    m_battlefieldNoticeInfo = {isRedTeam, QStringLiteral("对方基地护甲展开")};
    m_isVisible = true;
    m_opacity = 1.0;

    m_fadeAnimation->stop();
    m_hideTimer->stop();

    setWindowOpacity(1.0);
    show();
    update();

    m_hideTimer->start(3000);
}

void BattleMessageWidget::showBaseDestroyed(bool isRedBase){
    m_type = MessageType::OutpostDestroyed;
    m_outpostDestroyedInfo = {isRedBase};
    m_isVisible = true;
    m_opacity = 1.0;

    m_fadeAnimation->stop();
    m_hideTimer->stop();

    setWindowOpacity(1.0);
    show();
    update();

    m_hideTimer->start(4000);
}

void BattleMessageWidget::showOutpostDestroyed(bool isRedOutpost){
    m_type = MessageType::OutpostDestroyed;
    m_outpostDestroyedInfo = {isRedOutpost};
    m_isVisible = true;
    m_opacity = 1.0;

    m_fadeAnimation->stop();
    m_hideTimer->stop();

    setWindowOpacity(1.0);
    show();
    update();

    m_hideTimer->start(4000); // 前哨战摧毁显示稍久一点
}

void BattleMessageWidget::showRuneActivable(int runeType) {
    // 如果是未激活状态，且之前的状态不是正在激活或已激活，则不显示（避免开局刷屏）
    // 但 GameData 保证了只有状态变化才触发信号，所以这里只要收到信号就应该显示

    m_type = MessageType::Rune;
    m_runeData.type = runeType;
    m_isVisible = true;
    m_opacity = 1.0;

    m_fadeAnimation->stop();
    m_hideTimer->stop();

    setWindowOpacity(1.0);
    show();
    update();

    // 显示时长统一为 3秒 (和 KillMessage 一致)
    m_hideTimer->start(3000);
}

void BattleMessageWidget::showRuneActived(int runeType) {
    m_type = MessageType::RuneActived;
    m_runeData.type = runeType;
    m_isVisible = true;
    m_opacity = 1.0;

    m_fadeAnimation->stop();
    m_hideTimer->stop();

    setWindowOpacity(1.0);
    show();
    update();

    m_hideTimer->start(3000);
}

void BattleMessageWidget::showBaseStatusChange(bool isRed, int status) {
    m_type = MessageType::BaseStatusChange;
    m_baseStatusChangeInfo = {isRed, status};
    m_isVisible = true;
    m_opacity = 1.0;

    m_fadeAnimation->stop();
    m_hideTimer->stop();

    setWindowOpacity(1.0);
    show();
    update();

    m_hideTimer->start(4000);
}

void BattleMessageWidget::showOutpostStatusChange(bool isRed, int status) {
    m_type = MessageType::OutpostStatusChange;
    m_outpostStatusChangeInfo = {isRed, status};
    m_isVisible = true;
    m_opacity = 1.0;

    m_fadeAnimation->stop();
    m_hideTimer->stop();

    setWindowOpacity(1.0);
    show();
    update();

    m_hideTimer->start(4000);
}

void BattleMessageWidget::onFadeFinished() {
  m_isVisible = false;
  hide();
}


void BattleMessageWidget::paintEvent(QPaintEvent*) {
    if (!m_isVisible) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    switch (m_type) {
    case MessageType::Normal:
        drawNormal(p);
        break;
    case MessageType::Kill:
        drawKill(p);
        break;
    case MessageType::Warning:
        drawRefereeWarning(p);
        break;
    case MessageType::BaseDestroyed:
        drawBaseDestroyed(p);
        break;
    case MessageType::OutpostDestroyed:
        drawOutpostDestroyed(p);
        break;
    case MessageType::Rune:
        drawRune(p);
        break;
    case MessageType::RuneActived:
        drawRuneActived(p);
        break;
    case MessageType::AirSupportStarted:
        drawAirSupportStarted(p);
        break;
    case MessageType::DartGateOpened:
        drawDartGateOpened(p);
        break;
    case MessageType::BattlefieldNotice:
        drawBattlefieldNotice(p);
        break;
    case MessageType::BaseStatusChange:
        drawBaseStatusChange(p);
        break;
    case MessageType::OutpostStatusChange:
        drawOutpostStatusChange(p);
        break;
    default:
        break;
    }
}

void BattleMessageWidget::drawNormal(QPainter &p){
    //设置字体
      QFont font("Roboto", 20, QFont::Bold);
      p.setFont(font);
      QFontMetrics fm(font);

      // 计算文本尺寸
      int textWidth = fm.horizontalAdvance(m_message);
      int textHeight = fm.height();
      int padding = 15;


      int minBoxWidth = 300;
      int boxWidth = std::max(minBoxWidth, textWidth + padding * 2);
      int boxHeight = textHeight + padding * 2;
      int boxX = (width() - boxWidth) / 2;
      int boxY = (height() - boxHeight) / 2;


      // 绘制半透明背景
      QColor bgColor(20, 20, 30, 220); // 深色半透明背景
      p.setBrush(bgColor);
      p.setPen(Qt::NoPen);
      p.drawRoundedRect(boxX, boxY, boxWidth, boxHeight, 8, 8);

      // 绘制红色边框
      QPen borderPen(Colors::DANGER_RED, 2);
      p.setPen(borderPen);
      p.setBrush(Qt::NoBrush);
      p.drawRoundedRect(boxX, boxY, boxWidth, boxHeight, 8, 8);

      // 绘制文本
      p.setPen(Colors::TEXT_PRIMARY);
      p.drawText(boxX + padding, boxY + padding + fm.ascent(), m_message);
}

void BattleMessageWidget::drawRune(QPainter &p) {
    // 加载背景图和图标
    // 假设保持原有逻辑，使用绿色背景表示正面信息
    QPixmap bg(":/images/message/message_green.png");
    QPixmap icon(":/images/message/rune_icon.png");

    if (bg.isNull() || icon.isNull()) {
        qDebug() << "Rune resources missing:"
                 << "bg:" << bg.isNull()
                 << "icon:" << icon.isNull();
        // 资源缺失时回退为纯文字
        p.setPen(Qt::white);
        p.drawText(rect(), Qt::AlignCenter, "Rune Status Changed");
        return;
    }

    // 设置字体
    QFont font("Microsoft YaHei", 12, QFont::Bold);
    p.setFont(font);
    QFontMetrics fm(font);

    // 准备内容
    const QString runePrefix = (m_runeData.type == 0)
        ? QString::fromUtf8("小能量机关")
        : QString::fromUtf8("大能量机关");
    QString text = runePrefix + QString::fromUtf8("可激活");

    int textWidth = fm.horizontalAdvance(text);
    int textHeight = fm.ascent();

    // 布局参数 (参考 drawKill)
    int iconSize = 40;     // 图标大小
    int contentSpacing = 10; // 图标和文字间距
    int hPadding = 30;     // 水平内边距

    // 计算总尺寸
    int contentWidth = iconSize + contentSpacing + textWidth;
    int boxWidth = contentWidth + hPadding * 2;
    int boxHeight = 50; // 固定高度

    // 计算位置 (顶部居中)
    int startX = (width() - boxWidth) / 2;
    int startY = 100; // 保持稍微靠下的位置

    // 绘制背景
    QRect rect(startX, startY, boxWidth, boxHeight);
    p.drawPixmap(rect, bg);

    // 计算内容起始位置 (确保整体居中)
    int contentStartX = startX + (boxWidth - contentWidth) / 2;
    int contentCenterY = startY + boxHeight / 2;

    // 绘制图标 (垂直居中)
    QRect iconRect(contentStartX, contentCenterY - iconSize / 2, iconSize, iconSize);
    p.drawPixmap(iconRect, icon);

    // 绘制文字 (垂直居中)
    int textX = contentStartX + iconSize + contentSpacing;
    int textY = contentCenterY + textHeight / 2 - fm.descent() + 2;

    p.setPen(Qt::white);

    // 绘制文字描边
    QPainterPath path;
    path.addText(textX, textY, font, text);

    p.setPen(QPen(QColor(0, 100, 0), 2)); // 深绿色描边
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    // 填充文字
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.drawPath(path);
}

void BattleMessageWidget::drawRuneActived(QPainter &p) {
    QPixmap bg(":/images/message/message_green.png");
    QPixmap icon(":/images/message/rune_icon.png");

    if (bg.isNull() || icon.isNull()) {
        qDebug() << "Rune resources missing:"
                 << "bg:" << bg.isNull()
                 << "icon:" << icon.isNull();
        p.setPen(Qt::white);
        const QString fallbackText = (m_runeData.type == 0)
            ? QString::fromUtf8("小能量机关已激活")
            : QString::fromUtf8("大能量机关已激活");
        p.drawText(rect(), Qt::AlignCenter, fallbackText);
        return;
    }

    QFont font("Microsoft YaHei", 12, QFont::Bold);
    p.setFont(font);
    QFontMetrics fm(font);

    const QString runePrefix = (m_runeData.type == 0)
        ? QString::fromUtf8("小能量机关")
        : QString::fromUtf8("大能量机关");
    QString text = runePrefix + QString::fromUtf8("已激活");

    int textWidth = fm.horizontalAdvance(text);
    int textHeight = fm.ascent();

    int iconSize = 40;
    int contentSpacing = 10;
    int hPadding = 30;

    int contentWidth = iconSize + contentSpacing + textWidth;
    int boxWidth = contentWidth + hPadding * 2;
    int boxHeight = 50;

    int startX = (width() - boxWidth) / 2;
    int startY = 100;

    QRect rect(startX, startY, boxWidth, boxHeight);
    p.drawPixmap(rect, bg);

    int contentStartX = startX + (boxWidth - contentWidth) / 2;
    int contentCenterY = startY + boxHeight / 2;

    QRect iconRect(contentStartX, contentCenterY - iconSize / 2, iconSize, iconSize);
    p.drawPixmap(iconRect, icon);

    int textX = contentStartX + iconSize + contentSpacing;
    int textY = contentCenterY + textHeight / 2 - fm.descent() + 2;

    p.setPen(Qt::white);

    QPainterPath path;
    path.addText(textX, textY, font, text);

    p.setPen(QPen(QColor(0, 100, 0), 2));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.drawPath(path);
}

void BattleMessageWidget::drawRefereeWarning(QPainter &p) {
    // 构造显示文本
    QString title;
    QString penaltyText;

    switch(m_warningInfo.panaltytype) {
    case 1:
        title = "黄牌警告";
        penaltyText = "黄牌判罚";
        break;
    case 2:
        title = "双方黄牌";
        penaltyText = "双方黄牌判罚";
        break;
    case 3:
        title = "红牌警告";
        penaltyText = "红牌判罚";
        break;
    default:
        break;
    }

    QString body;
    if(m_warningInfo.panaltytype==1||m_warningInfo.panaltytype==3){
        bool isBlue = (m_warningInfo.robotid > 100);
        int robotId = isBlue ? (m_warningInfo.robotid - 100) : m_warningInfo.robotid;
        QString teamName = isBlue ? "蓝方" : "红方";
        body = QString("%1%2号机器人违规，惩罚时间%5，累计%3次数：%4")
                   .arg(teamName)
                   .arg(robotId)
                   .arg(penaltyText)
                   .arg(m_warningInfo.penaltycardcount)
                   .arg(m_warningInfo.penalty_effect_sec);
    }else{
        body = QString("惩罚时间%5")
                   .arg(m_warningInfo.penalty_effect_sec);
    }


    // 绘制逻辑

    // 字体设置
    QFont titleFont("Roboto", 24, QFont::Bold); // 加大标题字号
    QFont bodyFont("Roboto", 16); // 加大正文字号

    // 计算尺寸
    QFontMetrics fmTitle(titleFont);
    QFontMetrics fmBody(bodyFont);

    int titleWidth = fmTitle.horizontalAdvance(title);
    int bodyWidth = fmBody.horizontalAdvance(body);

    int contentWidth = std::max(titleWidth, bodyWidth);
    int padding = 40;

    int boxWidth = std::max(600, contentWidth + padding * 2);

    // 确保不超出屏幕宽度
    if (boxWidth > width()) boxWidth = width();

    int boxHeight = 150; // 加高背景

    QRect rect(
        (width() - boxWidth) / 2,
        height() * 0.2, // 稍微上移
        boxWidth,
        boxHeight
        );

    // 背景
    // 尝试加载特定颜色的背景图
    QString bgPath = ":/images/message/warning_box.png";
    // 可以根据 m_warningInfo.color 选择不同背景，例如黄色/红色背景图
    // 目前使用统一背景或颜色回退

    QPixmap bgPixmap(bgPath);
    if (!bgPixmap.isNull()) {
        p.drawPixmap(rect, bgPixmap);
    } else {
        // 回退方案：绘制半透明背景
        p.setBrush(QColor(20, 20, 30, 220));
        p.setPen(QPen(m_warningInfo.color, 3)); // 加粗边框
        p.drawRoundedRect(rect, 10, 10);
    }

    // 绘制标题 (上半部分)
    p.setFont(titleFont);
    p.setPen(m_warningInfo.color); // 标题使用警告色，例如黄色

    QRect titleRect = rect;
    titleRect.setHeight(rect.height() / 2);
    titleRect.moveTop(rect.top() + 15); // 顶部留白
    p.drawText(titleRect, Qt::AlignCenter, title);

    // 绘制正文 (下半部分)
    p.setFont(bodyFont);
    p.setPen(Qt::white); // 正文使用白色

    QRect bodyRect = rect;
    bodyRect.setTop(rect.top() + rect.height() / 2); // 从中部开始绘制
    p.drawText(bodyRect, Qt::AlignCenter, body);
}


void BattleMessageWidget::drawKill(QPainter &p){
    int avatarSize = 40;
    int padding = 10;
    int textWidth = 60;

    // 设置字体
    QFont font("Roboto", 10, QFont::Bold);
    p.setFont(font);
    QFontMetrics fm(font);

    if (m_killInfo.killerId == 0) {
        // --- 阵亡模式 (未知击杀者/系统击杀) ---
        QString text = "阵亡";
        textWidth = fm.horizontalAdvance(text) + padding * 2;

        int totalWidth = avatarSize + textWidth + padding * 5;
        int totalHeight = avatarSize + padding;

        int startX = (width() - totalWidth) / 2;
        int startY = 0;

        // 绘制背景
        QRect rect(startX, startY, totalWidth, totalHeight);
        QPixmap bgPixmap;
        if (m_killInfo.victimIsRed) {
            bgPixmap.load(":/images/message/message_red.png");
        } else {
            bgPixmap.load(":/images/message/message_blue.png");
        }


        if (!bgPixmap.isNull()) {
            p.drawPixmap(rect, bgPixmap);
        } else {
            QColor bgColor(20, 20, 30, 220);
            p.setBrush(bgColor);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(rect, 10, 10);

            p.setPen(QPen(Colors::DANGER_RED, 2));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(rect, 10, 10);

            // 边框 (阵亡通常用红色)
            p.setPen(QPen(Colors::DANGER_RED, 2));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(rect, 10, 10);
        }

        // 绘制被击杀者头像 (左侧)
        QRect victimRect(startX + padding, startY + padding / 2, avatarSize, avatarSize);
        drawAvatar(p, victimRect, m_killInfo.victimId, m_killInfo.victimIsRed);

        // 绘制文本 (右侧)
        p.setPen(Qt::white);
        QRect textRect(victimRect.right(), startY, textWidth, totalHeight);
        p.drawText(textRect, Qt::AlignCenter, text);

    } else {
        // --- 正常击杀模式 (A 摧毁 B) ---
        QString text;
        if (m_killInfo.killStreak == 2) text = "双杀";
        else if (m_killInfo.killStreak == 3) text = "三杀";
        else if (m_killInfo.killStreak == 4) text = "四杀";
        else if (m_killInfo.killStreak == 5) text = "五杀";
        else if (m_killInfo.isFirstBlood) text = "第一滴血";
        else text = "摧毁";

        textWidth = fm.horizontalAdvance(text) + padding * 2;

        // 计算总宽度：头像*2 + 文本 + 间距*2 + 外边距*2
        int contentSpacing = 5; // 元素间距
        int outerMargin = 10;   // 左右外边距
        int totalWidth = avatarSize * 2 + textWidth + contentSpacing * 2 + outerMargin * 2;
        int totalHeight = avatarSize + padding;

        // 确保 boxWidth 不超过 widget 宽度
        if (totalWidth > width()) totalWidth = width();

        int startX = (width() - totalWidth) / 2;
        int startY = 0; // 从顶部开始绘制

        // 绘制背景
        QRect rect(startX, startY, totalWidth, totalHeight);

        QPixmap bgPixmap;
        if (!m_killInfo.victimIsRed) {
            bgPixmap.load(":/images/message/message_red.png");
        } else {
            bgPixmap.load(":/images/message/message_blue.png");
        }

        if (!bgPixmap.isNull()) {
            p.drawPixmap(rect, bgPixmap);
        } else {
            QColor bgColor(20, 20, 30, 220);
            p.setBrush(bgColor);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(rect, 10, 10);

            p.setPen(QPen(Colors::DANGER_RED, 2));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(rect, 10, 10);
        }

        // 绘制击杀者头像
        QRect killerRect(startX + outerMargin, startY + padding / 2, avatarSize,
                        avatarSize);
        drawAvatar(p, killerRect, m_killInfo.killerId,
                m_killInfo.killerIsRed);

        // 绘制文本
        p.setPen(Qt::white);
        QRect textRect(killerRect.right() + contentSpacing, startY, textWidth, totalHeight);
        p.drawText(textRect, Qt::AlignCenter, text);

        // 绘制被击杀者头像
        QRect victimRect(textRect.right() + contentSpacing, startY + padding / 2, avatarSize,
                        avatarSize);
        drawAvatar(p, victimRect, m_killInfo.victimId,
                m_killInfo.victimIsRed);
    }
}

void BattleMessageWidget::drawAirSupportStarted(QPainter &p) {
    const int padding = 10;
    const int verticalPadding = 6;

    QFont font("Roboto", 10, QFont::Bold);
    p.setFont(font);
    QFontMetrics fm(font);

    const QString text = m_airSupportInfo.isRedTeam
        ? QStringLiteral("红方启动空中支援")
        : QStringLiteral("蓝方启动空中支援");

    const int totalWidth = fm.horizontalAdvance(text) + padding * 4;
    const int totalHeight = fm.height() + verticalPadding * 2;
    const int startX = (width() - totalWidth) / 2;
    const int startY = 0;
    QRect rect(startX, startY, totalWidth, totalHeight);

    QPixmap bgPixmap;
    bgPixmap.load(m_airSupportInfo.isRedTeam
                      ? ":/images/message/message_red.png"
                      : ":/images/message/message_blue.png");

    if (!bgPixmap.isNull()) {
        p.drawPixmap(rect, bgPixmap);
    } else {
        QColor bgColor(20, 20, 30, 220);
        p.setBrush(bgColor);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(rect, 10, 10);

        const QColor borderColor = m_airSupportInfo.isRedTeam
            ? Colors::DANGER_RED
            : Colors::BLUE_TEAM;
        p.setPen(QPen(borderColor, 2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(rect, 10, 10);
    }

    p.setPen(Qt::white);
    p.drawText(rect, Qt::AlignCenter, text);
}

void BattleMessageWidget::drawDartGateOpened(QPainter &p) {
    const int padding = 10;
    const int verticalPadding = 6;

    QFont font("Roboto", 10, QFont::Bold);
    p.setFont(font);
    QFontMetrics fm(font);

    const QString text = m_dartGateInfo.isEnemyTeam
        ? QStringLiteral("对方飞镖闸门开启")
        : QStringLiteral("己方飞镖闸门开启");
    const int totalWidth = fm.horizontalAdvance(text) + padding * 4;
    const int totalHeight = fm.height() + verticalPadding * 2;
    const int startX = (width() - totalWidth) / 2;
    const int startY = 0;
    QRect rect(startX, startY, totalWidth, totalHeight);

    QPixmap bgPixmap;
    bgPixmap.load(m_dartGateInfo.isRedTeam
                      ? ":/images/message/message_red.png"
                      : ":/images/message/message_blue.png");

    if (!bgPixmap.isNull()) {
        p.drawPixmap(rect, bgPixmap);
    } else {
        QColor bgColor(20, 20, 30, 220);
        p.setBrush(bgColor);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(rect, 10, 10);

        const QColor borderColor = m_dartGateInfo.isRedTeam
            ? Colors::DANGER_RED
            : Colors::BLUE_TEAM;
        p.setPen(QPen(borderColor, 2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(rect, 10, 10);
    }

    p.setPen(Qt::white);
    p.drawText(rect, Qt::AlignCenter, text);
}

void BattleMessageWidget::drawBattlefieldNotice(QPainter &p) {
    const int padding = 10;
    const int verticalPadding = 6;

    QFont font("Roboto", 10, QFont::Bold);
    p.setFont(font);
    QFontMetrics fm(font);

    const QString text = m_battlefieldNoticeInfo.text;
    const int totalWidth = fm.horizontalAdvance(text) + padding * 4;
    const int totalHeight = fm.height() + verticalPadding * 2;
    const int startX = (width() - totalWidth) / 2;
    const int startY = 0;
    QRect rect(startX, startY, totalWidth, totalHeight);

    QPixmap bgPixmap;
    bgPixmap.load(m_battlefieldNoticeInfo.isRedTeam
                      ? ":/images/message/message_red.png"
                      : ":/images/message/message_blue.png");

    if (!bgPixmap.isNull()) {
        p.drawPixmap(rect, bgPixmap);
    } else {
        QColor bgColor(20, 20, 30, 220);
        p.setBrush(bgColor);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(rect, 10, 10);

        const QColor borderColor = m_battlefieldNoticeInfo.isRedTeam
            ? Colors::DANGER_RED
            : Colors::BLUE_TEAM;
        p.setPen(QPen(borderColor, 2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(rect, 10, 10);
    }

    p.setPen(Qt::white);
    p.drawText(rect, Qt::AlignCenter, text);
}

void BattleMessageWidget::drawAvatar(QPainter &p, const QRect &rect, int id,
                                     bool isRed){
    p.save();

    QColor teamColor = isRed ? Colors::DANGER_RED : QColor(0, 100, 255);
    p.setPen(QPen(teamColor, 2));
    p.setBrush(QColor(0, 0, 0, 180));
    p.drawEllipse(rect);

    //图片路径
    int typeId = (id > 100) ? (id - 100) : id;
    QString typeName;
    switch (typeId) {
    case 1:
        typeName = "hero";
        break;
    case 2:
        typeName = "engineer";
        break;
    case 3:
    case 4:
    case 5:
        typeName = "infantry";
        break;
    case 6:
        typeName = "drone";
        break;
    case 7:
        typeName = "sentry";
        break;
    default:
        typeName = "robot"; // 兜底类型
        break;
    }
    QString colorName = isRed ? "red" : "blue";
    QString avatarPath =
        QString(":/images/robots/%1_%2.png").arg(colorName, typeName);

    qDebug() << "Trying to load avatar from path:" << avatarPath;

    QRect imgRect = rect.adjusted(4, 4, -4, -4);
    QPixmap avatar(avatarPath);

    if (!avatar.isNull()) {
        qDebug() << "Avatar loaded successfully!";
        QPainterPath path;
        path.addEllipse(imgRect);
        p.setClipPath(path);
        p.drawPixmap(imgRect, avatar);
        p.setClipping(false);
    }else {
        qDebug() << "Failed to load avatar! QPixmap is null.";
    }

    // ID 徽章
    int badgeSize = rect.width() / 3;
    QRect badgeRect(rect.right() - badgeSize, rect.bottom() - badgeSize,
                    badgeSize, badgeSize);

    p.setPen(QPen(Qt::white, 1));
    p.setBrush(teamColor);
    p.drawEllipse(badgeRect);

    p.setPen(Qt::white);
    QFont f = p.font();
    f.setPixelSize(badgeSize * 0.6);
    f.setBold(true);
    p.setFont(f);

    p.drawText(badgeRect, Qt::AlignCenter, QString::number(typeId));

    p.restore();

}
void BattleMessageWidget::drawBaseDestroyed(QPainter &p) {
    int avatarSize = 40;
    int padding = 10;
    int contentSpacing = 5;
    int outerMargin = 10;
    int totalHeight = avatarSize + padding;

    QString text;
    text = "被摧毁";

    // 字体设置
    QFont font("Roboto", 10, QFont::Bold);
    p.setFont(font);
    QFontMetrics fm(font);
    int textWidth = fm.horizontalAdvance(text) + padding * 2;

    // 计算总宽度
    int totalWidth = 0;
    totalWidth =  textWidth + contentSpacing + outerMargin * 2;

    // 确保 boxWidth 不超过 widget 宽度
    if (totalWidth > width()) totalWidth = width();

    // 居中显示
    int startX = (width() - totalWidth) / 2;
    int startY = 0;

    // 绘制背景
    QRect rect(startX, startY, totalWidth, totalHeight);

    QPixmap bgPixmap;
    bool isRedBg = false;
    isRedBg = m_outpostDestroyedInfo.isRedOutpost;

    if (isRedBg) {
        bgPixmap.load(":/images/message/message_red.png");
    } else {
        bgPixmap.load(":/images/message/message_blue.png");
    }

    if (!bgPixmap.isNull()) {
        p.drawPixmap(rect, bgPixmap);
    } else {
        QColor bgColor(20, 20, 30, 220);
        p.setBrush(bgColor);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(rect, 10, 10);

        // 边框颜色
        QColor borderColor = isRedBg ? Colors::DANGER_RED : Colors::BLUE_TEAM;
        p.setPen(QPen(borderColor, 2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(rect, 10, 10);
    }

    // 绘制文本
    p.setPen(Qt::white);
    QRect textRect(contentSpacing, startY, textWidth, totalHeight);
    p.drawText(textRect, Qt::AlignCenter, text);
}

void BattleMessageWidget::drawOutpostDestroyed(QPainter &p) {
    int avatarSize = 40;
    int padding = 10;
    int contentSpacing = 5;
    int outerMargin = 10;
    int totalHeight = avatarSize + padding;

    QString text;
    text = "被摧毁";

    // 字体设置
    QFont font("Roboto", 10, QFont::Bold);
    p.setFont(font);
    QFontMetrics fm(font);
    int textWidth = fm.horizontalAdvance(text) + padding * 2;

    // 计算总宽度
    int totalWidth = 0;
    totalWidth = avatarSize + textWidth + contentSpacing + outerMargin * 2;

    // 确保 boxWidth 不超过 widget 宽度
    if (totalWidth > width()) totalWidth = width();

    // 居中显示
    int startX = (width() - totalWidth) / 2;
    int startY = 0;

    // 绘制背景
    QRect rect(startX, startY, totalWidth, totalHeight);

    QPixmap bgPixmap;
    bool isRedBg = false;
    isRedBg = m_outpostDestroyedInfo.isRedOutpost;

    if (isRedBg) {
        bgPixmap.load(":/images/message/message_red.png");
    } else {
        bgPixmap.load(":/images/message/message_blue.png");
    }

    if (!bgPixmap.isNull()) {
        p.drawPixmap(rect, bgPixmap);
    } else {
        QColor bgColor(20, 20, 30, 220);
        p.setBrush(bgColor);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(rect, 10, 10);

        // 边框颜色
        QColor borderColor = isRedBg ? Colors::DANGER_RED : Colors::BLUE_TEAM;
        p.setPen(QPen(borderColor, 2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(rect, 10, 10);
    }

    // 定义绘制前哨站的 Lambda (避免重复代码)
    auto drawOutpostAvatar = [&](QRect r, bool isRed) {
        p.save();
        QColor outpostTeamColor = isRed ? Colors::DANGER_RED : Colors::BLUE_TEAM;
        p.setPen(QPen(outpostTeamColor, 2));
        p.setBrush(QColor(0, 0, 0, 180));
        p.drawEllipse(r);

        QString outpostColorName = isRed ? "red" : "blue";
        QString outpostPath = QString(":/images/message/%1_message_outpost.png").arg(outpostColorName);
        QRect imgRect = r.adjusted(4, 4, -4, -4);
        QPixmap outpostImg(outpostPath);

        if (!outpostImg.isNull()) {
            p.drawPixmap(imgRect, outpostImg);
        } else {
            QString oldPath = QString(":/images/top_outpost/%1_avatar.png").arg(outpostColorName);
            outpostImg.load(oldPath);
            if (!outpostImg.isNull()) {
                QPainterPath path;
                path.addEllipse(imgRect);
                p.setClipPath(path);
                p.drawPixmap(imgRect, outpostImg);
                p.setClipping(false);
            } else {
                p.setPen(outpostTeamColor);
                p.setFont(QFont("Roboto", 8));
                p.drawText(imgRect, Qt::AlignCenter, "Outpost");
            }
        }
        p.restore();
        // 恢复字体
        p.setFont(font);
    };
    //1. 绘制前哨站图标 (左侧)
    QRect outpostRect(startX + outerMargin, startY + padding / 2, avatarSize, avatarSize);
    drawOutpostAvatar(outpostRect, m_outpostDestroyedInfo.isRedOutpost);

    // 2. 绘制文本 (右侧)
    p.setPen(Qt::white);
    QRect textRect(outpostRect.right() + contentSpacing, startY, textWidth, totalHeight);
    p.drawText(textRect, Qt::AlignCenter, text);
}

void BattleMessageWidget::drawBaseStatusChange(QPainter &p) {
    // 文本内容
    QString side = m_baseStatusChangeInfo.isRed ? "红方" : "蓝方";
    QString statusText;
    if (m_baseStatusChangeInfo.status == 1) {
        statusText = "基地护甲未展开";
    } else if (m_baseStatusChangeInfo.status == 2) {
        statusText = "基地护甲展开";
    } else {
        statusText = "基地无敌解除"; // 兜底文本
    }

    QString text = side + statusText;

    // 字体设置 (调小字号与 drawKill 一致)
    QFont font("Roboto", 10, QFont::Bold);
    p.setFont(font);
    QFontMetrics fm(font);

    int avatarSize = 40; // 与 drawKill 一致，用于计算高度
    int padding = 10;
    int outerMargin = 10;
    int textWidth = fm.horizontalAdvance(text) + padding * 4; // 稍微多一点 padding

    int totalWidth = textWidth + outerMargin * 4;
    int totalHeight = avatarSize + padding;

    // 限制宽度
    if (totalWidth > width()) totalWidth = width();

    // 位置调整：从顶部开始绘制，与 drawKill 一致
    int startX = (width() - totalWidth) / 2;
    int startY = 0;
    QRect rect(startX, startY, totalWidth, totalHeight);

    // 绘制背景
    QPixmap bgPixmap;
    if (m_baseStatusChangeInfo.isRed) {
        bgPixmap.load(":/images/message/message_red.png");
    } else {
        bgPixmap.load(":/images/message/message_blue.png");
    }

    if (!bgPixmap.isNull()) {
        p.drawPixmap(rect, bgPixmap);
    } else {
        // 回退方案：带渐变的圆角矩形
        QColor teamColor = m_baseStatusChangeInfo.isRed ? QColor(220, 0, 0, 180) : QColor(0, 0, 220, 180);
        QLinearGradient gradient(rect.left(), rect.top(), rect.right(), rect.top());
        gradient.setColorAt(0, QColor(teamColor.red(), teamColor.green(), teamColor.blue(), 0));
        gradient.setColorAt(0.5, teamColor);
        gradient.setColorAt(1, QColor(teamColor.red(), teamColor.green(), teamColor.blue(), 0));

        p.setBrush(gradient);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(rect, 5, 5);
    }

    // 绘制文本
    p.setPen(Qt::white);
    QRect textRect(startX + outerMargin * 2, startY + padding / 2, textWidth, totalHeight - padding);
    p.drawText(textRect, Qt::AlignCenter, text);
}

void BattleMessageWidget::drawOutpostStatusChange(QPainter &p) {
    // 文本内容
    QString side = m_outpostStatusChangeInfo.isRed ? "红方" : "蓝方";
    // 修正：基于 status 映射护甲（中部装甲）相关提示，而不是始终显示“无敌解除”
    QString statusText;
    if (m_outpostStatusChangeInfo.status == 1) {
        statusText = "前哨站无敌解除，中部装甲旋转"; // 中部装甲开始旋转 / 展开
    } else if (m_outpostStatusChangeInfo.status == 2) {
        statusText = "前哨站无敌解除，中部装甲停转"; // 中部装甲停止旋转 / 收起
    } else {
        statusText = "前哨站无敌解除"; // 兜底文本
    }

    QString text = side + statusText;

    // 字体设置
    QFont font("Roboto", 10, QFont::Bold);
    p.setFont(font);
    QFontMetrics fm(font);

    int avatarSize = 40;
    int padding = 10;
    int outerMargin = 10;
    int textWidth = fm.horizontalAdvance(text) + padding * 4;

    int totalWidth = textWidth + outerMargin * 4;
    int totalHeight = avatarSize + padding;

    // 限制宽度
    if (totalWidth > width()) totalWidth = width();

    // 位置调整：居中
    int startX = (width() - totalWidth) / 2;
    int startY = 0;
    QRect rect(startX, startY, totalWidth, totalHeight);

    // 绘制背景
    QPixmap bgPixmap;
    if (m_outpostStatusChangeInfo.isRed) {
        bgPixmap.load(":/images/message/message_red.png");
    } else {
        bgPixmap.load(":/images/message/message_blue.png");
    }

    if (!bgPixmap.isNull()) {
        p.drawPixmap(rect, bgPixmap);
    } else {
        // 回退方案
        QColor teamColor = m_outpostStatusChangeInfo.isRed ? QColor(220, 0, 0, 180) : QColor(0, 0, 220, 180);
        QLinearGradient gradient(rect.left(), rect.top(), rect.right(), rect.top());
        gradient.setColorAt(0, QColor(teamColor.red(), teamColor.green(), teamColor.blue(), 0));
        gradient.setColorAt(0.5, teamColor);
        gradient.setColorAt(1, QColor(teamColor.red(), teamColor.green(), teamColor.blue(), 0));

        p.setBrush(gradient);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(rect, 5, 5);
    }

    // 绘制文本
    p.setPen(Qt::white);
    QRect textRect(startX + outerMargin * 2, startY + padding / 2, textWidth, totalHeight - padding);
    p.drawText(textRect, Qt::AlignCenter, text);
}

} // namespace RM
