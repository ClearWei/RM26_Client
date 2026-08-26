#include "GameResultWidget.h"
#include "../ui/LayoutConstants.h"
#include "../ui/PopupOverlayPolicy.h"
#include <QtCore/QtGlobal>
#include <QApplication>
#include <QScreen>
#include <QEasingCurve>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QRegularExpression>
#include <algorithm>

// 战报界面的统一样式
class GameStyles {
public:
    static QColor redTeamColor() {
        return QColor(255, 69, 58); // #FF453A
    }

    static QColor blueTeamColor() {
        return QColor(10, 132, 255); // #0A84FF
    }

    static QColor neutralColor() {
        return QColor(142, 142, 147); // #8E8E93
    }

    static QColor victoryGoldColor() {
        return QColor(255, 215, 0); // 胜利金色
    }

    static QFont getHeaderFont() {
        QFont font("Microsoft YaHei", 48, QFont::Bold);
        return font;
    }

    static QFont getScoreFont() {
        QFont font("robot", 60, QFont::Bold);
        return font;
    }

    static QFont getTeamNameFont() {
        QFont font("Microsoft YaHei", 16, QFont::Bold);
        return font;
    }

    static QFont getSchoolNameFont() {
        QFont font("Microsoft YaHei", 12);
        return font;
    }

    static QFont getRoundInfoFont() {
        QFont font("Microsoft YaHei", 12, QFont::Bold);
        return font;
    }
};

GameResultWidget::GameResultWidget(QWidget *parent)
    : QWidget(parent)
    , m_result(Draw)
    , m_reason(TimeUp)
    , m_redScore(0)
    , m_blueScore(0)
    , m_round(1)
    , m_gameTime("00:00")
    , m_scaleAnimation(nullptr)
    , m_opacityEffect(nullptr)
    , m_redBaseHP(2000)
    , m_blueBaseHP(2000)
    , m_redOutpostHP(0)
    , m_blueOutpostHP(0)
    , m_redSentryHP(0)
    , m_blueSentryHP(0)
    , m_redEnergyActivations(0)
    , m_blueEnergyActivations(0)
    , m_redArmorBreaks(0)
    , m_blueArmorBreaks(0)
    , m_frameTimer(new QTimer(this))
    , m_currentFrameIndex(0)
    , m_showStats(false)
    , m_victorySound(new QSoundEffect(this))
{
    setupUI();
    setupAnimations();

    // 结算语音属于可选资源，优先读取 Qt 资源，再回退到应用目录附近。
    QString wavPath = ":/sounds/game_finish.mp3";

    if (!QFileInfo::exists(wavPath)) {
        wavPath = QCoreApplication::applicationDirPath() + "/resources/sounds/game_finish.mp3";
    }
    if (!QFileInfo::exists(wavPath)) {
        wavPath = QCoreApplication::applicationDirPath() + "/../../resources/sounds/game_finish.mp3";
    }

    if (QFileInfo::exists(wavPath) && QFileInfo(wavPath).size() > 1024) {
        if (wavPath.startsWith(":")) {
            m_victorySound->setSource(QUrl("qrc" + wavPath));
        } else {
            m_victorySound->setSource(QUrl::fromLocalFile(wavPath));
        }
        m_victorySound->setVolume(0.5);
    } else {
        qWarning() << "Game finish sound (WAV) not found at:" << wavPath;
    }

    connect(m_frameTimer, &QTimer::timeout, this, [this]() {
        if (m_victoryFramePaths.isEmpty()) {
            qDebug() << "GameResultWidget frame timer fired but no frames loaded";
            return;
        }

        m_currentFrameIndex++;
        if (m_currentFrameIndex >= m_victoryFramePaths.size()) {
             m_currentFrameIndex = m_victoryFramePaths.size() - 1;
             m_frameTimer->stop();

             m_showStats = true;
             m_currentFramePixmap = QPixmap();

             resizeToScaledContent();
             if (parentWidget()) {
                 int x = (parentWidget()->width() - width()) / 2;
                 int y = (parentWidget()->height() - height()) / 2;
                 move(x, y);
             }

             update();
             return;
        }

        const QString framePath = m_victoryFramePaths[m_currentFrameIndex];
        m_currentFramePixmap.load(framePath);
        update();
    });
}

void GameResultWidget::setupUI()
{
    // 窗口保持透明，由绘制逻辑负责背景和边框。
    setAttribute(Qt::WA_TranslucentBackground);
    resize(designSize()); // 初始使用设计稿尺寸，显示时再按视口缩放。
}

void GameResultWidget::setResolutionViewport(const QSize &viewport)
{
    const QSize normalizedViewport =
        (viewport.width() > 0 && viewport.height() > 0)
            ? viewport
            : QSize(1920, 1080);
    if (m_resolutionViewport == normalizedViewport) {
        return;
    }

    m_resolutionViewport = normalizedViewport;
    resizeToScaledContent();
    update();
}

QSize GameResultWidget::scaledContentSize() const
{
    const QSize baseSize =
        (!m_showStats && !m_currentFramePixmap.isNull())
            ? m_currentFramePixmap.size()
            : designSize();
    return RM::PopupOverlayPolicy::scaledSize(baseSize,
                                               m_resolutionViewport);
}

void GameResultWidget::resizeToScaledContent()
{
    resize(scaledContentSize());
}

void GameResultWidget::setupAnimations()
{
    // 只保留缩放动画，避免透明度动画让战报内容发灰。
    m_scaleAnimation = new QPropertyAnimation(this, "geometry", this);
    m_scaleAnimation->setDuration(800);
    m_scaleAnimation->setEasingCurve(QEasingCurve::OutBack);
}

void GameResultWidget::showResult(GameResult result, WinReason reason)
{
    m_result = result;
    m_reason = reason;

    // 重复显示结果时先停止上一轮动画。
    if (m_scaleAnimation->state() == QAbstractAnimation::Running) {
        m_scaleAnimation->stop();
    }

    m_frameTimer->stop();
    loadVictoryFrames(result);

    // 动画结束前不显示统计页。
    m_showStats = false;

    // 有动画时按首帧缩放，否则直接使用统计页尺寸。
    if (!m_victoryFramePaths.isEmpty()) {
        if (!m_currentFramePixmap.isNull()) {
            resizeToScaledContent();
        }
    } else {
        m_showStats = true;
        resizeToScaledContent();
    }

    // 尺寸变化后重新居中。
    if (parentWidget()) {
        int x = (parentWidget()->width() - width()) / 2;
        int y = (parentWidget()->height() - height()) / 2;
        move(x, y);
    }

    show();

    // 从中心位置放大到最终尺寸。
    QRect startGeometry = geometry();
    startGeometry.setSize(QSize(width() * 0.5, height() * 0.5));
    startGeometry.moveCenter(geometry().center());

    QRect endGeometry = geometry();

    m_scaleAnimation->setStartValue(startGeometry);
    m_scaleAnimation->setEndValue(endGeometry);
    m_scaleAnimation->start();

    if (!m_victoryFramePaths.isEmpty()) {
        // 结算动画每 10 毫秒推进一帧。
        m_frameTimer->start(10);
    }

    // 根据外部开关播放结算音效。
    if (m_playVictoryOnShow) {
        if (m_victorySound->source().isValid()) {
            m_victorySound->play();
        }
    }

    // 获取焦点后才能响应关闭和音效快捷键。
    setFocus();
}

void GameResultWidget::setGameData(int redScore, int blueScore, int round, const QString& gameTime)
{
    m_redScore = redScore;
    m_blueScore = blueScore;
    m_round = round;
    m_gameTime = gameTime;
    update();
}

void GameResultWidget::setTeamData(const QString& redTeam, const QString& blueTeam,
                                   const QString& redSchool, const QString& blueSchool)
{
    m_redTeam = redTeam;
    m_blueTeam = blueTeam;
    m_redSchool = redSchool;
    m_blueSchool = blueSchool;
    update();
}

void GameResultWidget::setDetailedStats(const QMap<QString, QVariant>& stats)
{
    // 协议汇总值用于单机器人明细缺失时的兜底。
    if (stats.contains("redTotalDamage")) {
        bool ok = false;
        int v = stats.value("redTotalDamage").toInt(&ok);
        if (ok) m_protocolRedTotalDamage = v;
    }
    if (stats.contains("blueTotalDamage")) {
        bool ok = false;
        int v = stats.value("blueTotalDamage").toInt(&ok);
        if (ok) m_protocolBlueTotalDamage = v;
    }
    // 其他扩展字段暂不参与当前战报绘制。
    update();
}

//设置音量函数
void GameResultWidget::setSoundVolume(qreal volume)
{
    if (!m_victorySound) {
        return;
    }
    const qreal clamped = qBound<qreal>(0.0, volume, 1.0);
    m_victorySound->setVolume(clamped);
}

void GameResultWidget::setRobotStats(const QMap<int, RobotStats>& redRobots, const QMap<int, RobotStats>& blueRobots)
{
    m_redRobots = redRobots;
    m_blueRobots = blueRobots;
    update();
}

void GameResultWidget::setEnhancedGameStats(int redBaseHP, int blueBaseHP, int redOutpostHP, int blueOutpostHP,
                                           int redSentryHP, int blueSentryHP, int redEnergyActivations,
                                           int blueEnergyActivations, const QString& mvpPlayer)
{
    m_redBaseHP = redBaseHP;
    m_blueBaseHP = blueBaseHP;
    m_redOutpostHP = redOutpostHP;
    m_blueOutpostHP = blueOutpostHP;
    m_redSentryHP = redSentryHP;
    m_blueSentryHP = blueSentryHP;
    m_redEnergyActivations = redEnergyActivations;
    m_blueEnergyActivations = blueEnergyActivations;
    m_mvpPlayer = mvpPlayer;
    update();
}

void GameResultWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    setFocus(); // 显示时接收快捷键。
}

void GameResultWidget::keyPressEvent(QKeyEvent *event)
{
    // Ctrl+R 用于现场快速切换结算音效。
    if (event->modifiers() & Qt::ControlModifier && event->key() == Qt::Key_R) {
        if (m_victorySound->isPlaying()) {
            m_victorySound->stop();
        } else {
            m_victorySound->play();
        }
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}

void GameResultWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (!m_showStats) {
        if (!m_currentFramePixmap.isNull()) {
            painter.save();

            QSize widgetSize = size();
            QSize frameSize = m_currentFramePixmap.size();

            if (frameSize.isValid()) {
                QSize scaledSize = frameSize.scaled(widgetSize, Qt::KeepAspectRatio);
                int x = (widgetSize.width() - scaledSize.width()) / 2;
                int y = (widgetSize.height() - scaledSize.height()) / 2;
                QRect targetRect(x, y, scaledSize.width(), scaledSize.height());

                painter.drawPixmap(targetRect, m_currentFramePixmap);
            }

            painter.restore();
        }
        return;
    }

    const QSize canvasSize = designSize();
    painter.scale(static_cast<qreal>(width()) / canvasSize.width(),
                  static_cast<qreal>(height()) / canvasSize.height());

    drawBackground(painter);

    drawHeader(painter);
    drawStatsComparison(painter);
    drawRobotLists(painter);
}

void GameResultWidget::drawStatsComparison(QPainter& painter)
{
    painter.save();

    // 从机器人明细计算两队汇总统计。
    int redTotalDamage = 0;
    int blueTotalDamage = 0;
    int redTotalKills = 0;
    int blueTotalKills = 0;
    int redTotalRobotHP = 0;
    int blueTotalRobotHP = 0;
    int redMaxSingleDamage = 0;
    int blueMaxSingleDamage = 0;

    for (const auto& robot : m_redRobots) {
        redTotalDamage += robot.damageDealt;
        redTotalKills += robot.kills;
        redTotalRobotHP += robot.currentHP;
        if (robot.damageDealt > redMaxSingleDamage) redMaxSingleDamage = robot.damageDealt;
    }

    for (const auto& robot : m_blueRobots) {
        blueTotalDamage += robot.damageDealt;
        blueTotalKills += robot.kills;
        blueTotalRobotHP += robot.currentHP;
        if (robot.damageDealt > blueMaxSingleDamage) blueMaxSingleDamage = robot.damageDealt;
    }

    int startY = 220;
    int rowHeight = 75;

    // 统计项图标使用 qrc 路径，缺失时由 drawStatRow 绘制占位符。
    QString defaultIcon = ":/images/message/validate_icon_hp.png";
    QString hpIcon = ":/images/message/validate_icon_hp.png";
    QString hurtIcon = ":/images/message/validate_icon_totalhurt.png";
    QString killIcon = ":/images/message/validate_icon_killcount.png";
    QString energyIcon = ":/images/message/validate_icon_rune.png";

    // 2. 基地血量
    drawStatRow(painter, startY , "基地血量", m_redBaseHP, m_blueBaseHP, 5000, hpIcon);

    // 3. 前哨站血量
    drawStatRow(painter, startY + rowHeight, "前哨站血量", m_redOutpostHP, m_blueOutpostHP, 1500, hpIcon);

    // 4. 哨兵血量
    drawStatRow(painter, startY + rowHeight * 2, "哨兵血量", m_redSentryHP, m_blueSentryHP, 600, hpIcon);

    // 5. 总伤害：机器人明细合计为 0 时回退到协议汇总值。
    int effectiveRedTotal = redTotalDamage;
    int effectiveBlueTotal = blueTotalDamage;
    if (effectiveRedTotal == 0 && m_protocolRedTotalDamage > 0) effectiveRedTotal = m_protocolRedTotalDamage;
    if (effectiveBlueTotal == 0 && m_protocolBlueTotalDamage > 0) effectiveBlueTotal = m_protocolBlueTotalDamage;
    drawStatRow(painter, startY + rowHeight * 3, "伤害总量", effectiveRedTotal, effectiveBlueTotal, qMax(effectiveRedTotal, effectiveBlueTotal), hurtIcon);

    // 6. 能量机关激活次数
    drawStatRow(painter, startY + rowHeight * 4, "能量机关", m_redEnergyActivations, m_blueEnergyActivations, 5, energyIcon);

    // 7. 击杀数
    drawStatRow(painter, startY + rowHeight * 5, "击杀数", redTotalKills, blueTotalKills, qMax(redTotalKills, blueTotalKills), killIcon);

    // 8. 机器人剩余总血量
    drawStatRow(painter, startY + rowHeight * 6, "机器人总血量", redTotalRobotHP, blueTotalRobotHP, qMax(redTotalRobotHP, blueTotalRobotHP), hpIcon);

    painter.restore();
}

void GameResultWidget::loadVictoryFrames(GameResult result)
{
    qDebug() << "GameResultWidget::loadVictoryFrames called, result =" << static_cast<int>(result);
    m_victoryFramePaths.clear();
    m_currentFrameIndex = 0;
    m_currentFramePixmap = QPixmap();

    QString subdir;
    if (result == BlueWin) {
        subdir = "blue_win_zh";
    } else if (result == RedWin) {
        subdir = "red_win_zh";
    } else if (result == Terminated) {
        // 异常结束使用单独的中文动画目录。
        subdir = "abnormal_termination_zh";
    } else {
        qDebug() << "GameResultWidget::loadVictoryFrames early return, no animation for this result";
        return;
    }

    QString appDir = QCoreApplication::applicationDirPath();
    QDir dir;

    QStringList candidateDirs;
    // 打包版本优先读取 qrc，开发环境再尝试可执行文件和源码目录附近的资源。
    candidateDirs << (QString(":/images/resultpanel/") + subdir);
    candidateDirs << appDir + "/resources/images/resultpanel/" + subdir;
    candidateDirs << appDir + "/../resources/images/resultpanel/" + subdir;
    candidateDirs << appDir + "/../../resources/images/resultpanel/" + subdir;
    candidateDirs << appDir + "/../../../resources/images/resultpanel/" + subdir;
    // IDE 从项目目录启动时使用工作目录兜底。
    candidateDirs << QDir::currentPath() + "/resources/images/resultpanel/" + subdir;

    for (const QString& candidate : candidateDirs) {
        dir.setPath(candidate);
        bool exists = dir.exists();
        // QDir::exists 同时支持文件系统目录和 qrc 路径。
        if (exists) {
            break;
        }
    }

    if (!dir.exists()) {
        return;
    }

    QStringList filters;
    filters << "*.png";
    dir.setNameFilters(filters);
    QFileInfoList fileList = dir.entryInfoList(QDir::Files);

    if (fileList.isEmpty()) {
        return;
    }

    // 按文件名中的帧序号排序，避免 10.png 排在 2.png 前面。
    std::sort(fileList.begin(), fileList.end(), [](const QFileInfo &a, const QFileInfo &b) {
        // 兼容 blue_win_123.png 一类命名。
        QString nameA = a.fileName();
        QString nameB = b.fileName();

        static QRegularExpression re("(\\d+)");
        QRegularExpressionMatch matchA = re.match(nameA);
        QRegularExpressionMatch matchB = re.match(nameB);

        int numA = matchA.hasMatch() ? matchA.captured(1).toInt() : 0;
        int numB = matchB.hasMatch() ? matchB.captured(1).toInt() : 0;

        return numA < numB;
    });

    for (const QFileInfo& fileInfo : fileList) {
        m_victoryFramePaths.append(fileInfo.absoluteFilePath());
    }
    // 预加载首帧，首次绘制时不再等待磁盘读取。
    if (!m_victoryFramePaths.isEmpty()) {
        m_currentFramePixmap.load(m_victoryFramePaths.first());
    }
}

void GameResultWidget::drawStatRow(QPainter& painter, int y, const QString& label, int redValue, int blueValue, int maxValue, const QString& iconPath)
{
    int centerX = designSize().width() / 2;
    int barMaxWidth = 180; // 为中央图标和标签预留空间。
    int barHeight = 8;
    int centerColumnWidth = 140;
    int gap = 20;

    // 最大值至少为 1，避免比例计算除零。
    if (maxValue <= 0) maxValue = 1;

    // 数值和标签使用不同字号。
    QFont labelFont("Microsoft YaHei", 9);
    QFont valueFont("Microsoft YaHei", 14, QFont::Bold);

    // 第一行：两侧数值和中央图标
    int iconSize = 30;
    QRect iconRect(centerX - iconSize/2, y, iconSize, iconSize);

    // 图标读取后放入缓存，后续帧直接复用。
    QPixmap icon;
    if (m_iconCache.contains(iconPath)) {
        icon = m_iconCache[iconPath];
    } else {
        if (icon.load(iconPath)) {
            m_iconCache.insert(iconPath, icon);
        }
    }

    if (!icon.isNull()) {
        painter.drawPixmap(iconRect, icon.scaled(iconRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        // 缺少图标时绘制中性占位符。
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QColor(200, 200, 200));
        painter.drawRect(iconRect);
    }

    // 红方数值靠中央列左侧右对齐。
    painter.setPen(Qt::white);
    painter.setFont(valueFont);
    int redValueRightX = centerX - centerColumnWidth/2 - gap;
    QRect redValueRect(redValueRightX - 100, y, 100, iconSize);
    painter.drawText(redValueRect, Qt::AlignRight | Qt::AlignVCenter, QString::number(redValue));

    // 蓝方数值靠中央列右侧左对齐。
    int blueValueLeftX = centerX + centerColumnWidth/2 + gap;
    QRect blueValueRect(blueValueLeftX, y, 100, iconSize);
    painter.drawText(blueValueRect, Qt::AlignLeft | Qt::AlignVCenter, QString::number(blueValue));

    // 第二行：比例条和标签

    int line2Y = y + iconSize + 5;
    int labelHeight = 20;

    // 中央标签
    painter.setPen(QColor(200, 200, 200));
    painter.setFont(labelFont);
    QRect labelRect(centerX - centerColumnWidth/2, line2Y, centerColumnWidth, labelHeight);
    painter.drawText(labelRect, Qt::AlignCenter, label);

    // 红方比例条从中央向左增长。
    int redBarWidth = (long long)redValue * barMaxWidth / maxValue;
    int blueBarWidth = (long long)blueValue * barMaxWidth / maxValue;

    // 比例限制在有效区间。
    redBarWidth = qMin(redBarWidth, barMaxWidth);
    blueBarWidth = qMin(blueBarWidth, barMaxWidth);

    int barY = line2Y + labelHeight/2 - barHeight/2;

    // 红方前景条
    int redBarRightX = centerX - centerColumnWidth/2 - gap;
    QRect redBarRect(redBarRightX - redBarWidth, barY, redBarWidth, barHeight);
    painter.fillRect(redBarRect, GameStyles::redTeamColor());

    // 蓝方前景条
    int blueBarLeftX = centerX + centerColumnWidth/2 + gap;
    QRect blueBarRect(blueBarLeftX, barY, blueBarWidth, barHeight);
    painter.fillRect(blueBarRect, GameStyles::blueTeamColor());

    // 淡色背景表示完整量程。
    painter.fillRect(QRect(redBarRightX - barMaxWidth, barY, barMaxWidth, barHeight), QColor(255, 255, 255, 20));
    painter.fillRect(QRect(blueBarLeftX, barY, barMaxWidth, barHeight), QColor(255, 255, 255, 20));
}

void GameResultWidget::drawRobotLists(QPainter& painter)
{
    painter.save();

    int listTopY = 220;
    int itemHeight = 85;
    int robotIconSize = 48;

    QFont idFont("Microsoft YaHei", 10, QFont::Bold);
    QFont damageTitleFont("Microsoft YaHei", 10, QFont::Bold);
    QFont damageValueFont("Microsoft YaHei", 14, QFont::Bold);

    // 两队标题
    painter.setPen(QColor(200, 200, 200));
    painter.setFont(damageTitleFont);

    // 以两队单机最高伤害作为比例条量程。
    int maxDamage = 1;
    for (const auto& robot : m_redRobots) {
        if (robot.damageDealt > maxDamage) maxDamage = robot.damageDealt;
    }
    for (const auto& robot : m_blueRobots) {
        if (robot.damageDealt > maxDamage) maxDamage = robot.damageDealt;
    }

    // 根据机器人 ID 选择图标。
    auto getIconPath = [](bool isRed, int id) -> QString {
        QString color = isRed ? "red" : "blue";
        QString type = "infantry";

        // 蓝方 ID 先归一化到 1—9。
        int normalizedId = id;
        if (id > 100) normalizedId -= 100;

        switch (normalizedId) {
            case 1: type = "hero"; break;
            case 2: type = "engineer"; break;
            case 3: case 4: case 5: type = "infantry"; break;
            case 6: type = "drone"; break;
            case 7: type = "sentry"; break;
            case 9: type = "sentry"; break; // 雷达暂用哨兵图标兜底。
        }
        return QString(":/images/robots/%1_%2.png").arg(color, type);
    };

    // 红方位于左侧。
    int leftX = 40;
    painter.drawText(QRect(leftX, listTopY - 35, 150, 30), Qt::AlignLeft | Qt::AlignVCenter, "单兵伤害量");

    int currentY = listTopY;

    for (auto it = m_redRobots.begin(); it != m_redRobots.end(); ++it) {
        int id = it.key();
        const RobotStats& robot = it.value();

        // 1. 机器人图标
        int iconY = currentY + 20;
        QRect iconRect(leftX, iconY, robotIconSize, robotIconSize);

        // 加载并绘制图标。
        QString iconPath = getIconPath(true, id);
        QPixmap icon;
        if (m_iconCache.contains(iconPath)) {
            icon = m_iconCache[iconPath];
        } else {
            if (icon.load(iconPath)) {
                m_iconCache.insert(iconPath, icon);
            }
        }

        if (!icon.isNull()) {
            painter.drawPixmap(iconRect, icon.scaled(iconRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            // 素材缺失时显示轮廓占位符。
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(GameStyles::redTeamColor(), 2));
            painter.drawEllipse(iconRect);
        }

        // 图标右下角的 ID 徽章
        QRect idRect(iconRect.right() - 15, iconRect.bottom() - 15, 18, 18);
        painter.setBrush(Qt::black);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(idRect);
        painter.setPen(Qt::white);
        painter.setFont(idFont);
        painter.drawText(idRect, Qt::AlignCenter, QString::number(id));

        // 2. 伤害数值和比例条
        int contentLeftX = iconRect.right() + 15;
        int barMaxWidth = 160;

        // 比例条上方显示伤害值。
        painter.setPen(Qt::white);
        painter.setFont(damageValueFont);
        QRect damageTextRect(contentLeftX, currentY + 10, barMaxWidth, 25);
        painter.drawText(damageTextRect, Qt::AlignLeft | Qt::AlignVCenter, QString::number(robot.damageDealt));

        // 伤害比例条
        int barHeight = 8;
        int barY = currentY + 45;

        // 背景条
        painter.fillRect(contentLeftX, barY, barMaxWidth, barHeight, QColor(255, 255, 255, 30));

        // 前景条
        if (maxDamage > 0) {
            int fillWidth = (long long)robot.damageDealt * barMaxWidth / maxDamage;
            // 渐变突出高伤害端。
            QLinearGradient gradient(contentLeftX, barY, contentLeftX + fillWidth, barY);
            gradient.setColorAt(0, QColor(255, 69, 58, 150));
            gradient.setColorAt(1, QColor(255, 69, 58));
            painter.fillRect(contentLeftX, barY, fillWidth, barHeight, QBrush(gradient));

            // 末端亮条增强辨识度。
            if (fillWidth > 0)
                painter.fillRect(contentLeftX + fillWidth - 2, barY, 2, barHeight, Qt::white);
        }

        currentY += itemHeight;
    }

    // 蓝方位于右侧。
    painter.setPen(QColor(200, 200, 200));
    painter.setFont(damageTitleFont);
    painter.drawText(QRect(designSize().width() - 40 - 150, listTopY - 35, 150, 30), Qt::AlignRight | Qt::AlignVCenter, "单兵伤害量");

    currentY = listTopY;

    for (auto it = m_blueRobots.begin(); it != m_blueRobots.end(); ++it) {
        int id = it.key();
        const RobotStats& robot = it.value();

        // 图标靠右排列。
        int iconX = designSize().width() - 40 - robotIconSize;
        int iconY = currentY + 20;
        QRect iconRect(iconX, iconY, robotIconSize, robotIconSize);

        // 加载并绘制图标。
        QString iconPath = getIconPath(false, id);
        QPixmap icon;
        if (m_iconCache.contains(iconPath)) {
            icon = m_iconCache[iconPath];
        } else {
            if (icon.load(iconPath)) {
                m_iconCache.insert(iconPath, icon);
            }
        }

        if (!icon.isNull()) {
            painter.drawPixmap(iconRect, icon.scaled(iconRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(GameStyles::blueTeamColor(), 2));
            painter.drawEllipse(iconRect);
        }

        // 蓝方徽章放在图标左下角，与红方镜像。
        QRect idRect(iconRect.left() - 3, iconRect.bottom() - 15, 18, 18);
        painter.setBrush(Qt::black);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(idRect);
        painter.setPen(Qt::white);
        painter.setFont(idFont);
        int displayId = (id > 100) ? (id - 100) : id;
        painter.drawText(idRect, Qt::AlignCenter, QString::number(displayId));

        // 2. 图标左侧的伤害数值和比例条
        int contentRightX = iconRect.left() - 15;
        int barMaxWidth = 160;

        // 数值在比例条上方右对齐。
        painter.setPen(Qt::white);
        painter.setFont(damageValueFont);
        QRect damageTextRect(contentRightX - barMaxWidth, currentY + 10, barMaxWidth, 25);
        painter.drawText(damageTextRect, Qt::AlignRight | Qt::AlignVCenter, QString::number(robot.damageDealt));

        // 比例条从右向左增长。
        int barHeight = 8;
        int barY = currentY + 45;

        // 背景条
        painter.fillRect(contentRightX - barMaxWidth, barY, barMaxWidth, barHeight, QColor(255, 255, 255, 30));

        // 前景条
        if (maxDamage > 0) {
            int fillWidth = (long long)robot.damageDealt * barMaxWidth / maxDamage;
            int barStartX = contentRightX - fillWidth;

            // 渐变前景
            QLinearGradient gradient(barStartX, barY, contentRightX, barY);
            gradient.setColorAt(0, QColor(10, 132, 255));
            gradient.setColorAt(1, QColor(10, 132, 255, 150));

            painter.fillRect(barStartX, barY, fillWidth, barHeight, QBrush(gradient));

            // 末端亮条
            if (fillWidth > 0)
                painter.fillRect(barStartX, barY, 2, barHeight, Qt::white);
        }

        currentY += itemHeight;
    }

    painter.restore();
}

void GameResultWidget::drawBackground(QPainter& painter)
{
    painter.save();

    // 圆角主背景
    QRect bgRect(QPoint(0, 0), designSize());
    bgRect.adjust(10, 10, -10, -10);
    int radius = 20;

    QPainterPath path;
    path.addRoundedRect(bgRect, radius, radius);

    // 由上方深灰色过渡到下方黑色。
    QLinearGradient bgGradient(bgRect.topLeft(), bgRect.bottomLeft());
    bgGradient.setColorAt(0.0, QColor(40, 44, 52, 255));
    bgGradient.setColorAt(1.0, QColor(0, 0, 0, 255));
    painter.fillPath(path, bgGradient);

    // 外边框
    QPen borderPen(QColor(58, 65, 81), 2);
    painter.setPen(borderPen);
    painter.drawPath(path);

    // 四角科技感装饰线
    int bracketSize = 40;
    int bracketOffset = 5;
    QPen accentPen(GameStyles::neutralColor(), 3);
    painter.setPen(accentPen);

    // 左上
    painter.drawLine(bgRect.topLeft() + QPoint(0, bracketSize), bgRect.topLeft());
    painter.drawLine(bgRect.topLeft(), bgRect.topLeft() + QPoint(bracketSize, 0));

    // 右上
    painter.drawLine(bgRect.topRight() + QPoint(0, bracketSize), bgRect.topRight());
    painter.drawLine(bgRect.topRight(), bgRect.topRight() - QPoint(bracketSize, 0));

    // 左下
    painter.drawLine(bgRect.bottomLeft() - QPoint(0, bracketSize), bgRect.bottomLeft());
    painter.drawLine(bgRect.bottomLeft(), bgRect.bottomLeft() + QPoint(bracketSize, 0));

    // 右下
    painter.drawLine(bgRect.bottomRight() - QPoint(0, bracketSize), bgRect.bottomRight());
    painter.drawLine(bgRect.bottomRight(), bgRect.bottomRight() - QPoint(bracketSize, 0));

    painter.restore();
}

void GameResultWidget::drawHeader(QPainter& painter)
{
    painter.save();

    int centerX = designSize().width() / 2;
    int topY = 40;

    // 1. 胜方标识跟随获胜队伍位置；平局时居中。

    QString resultText;
    QColor resultColor;
    QRect resultRect;

    if (m_result == RedWin) {
        resultText = "WINNER";
        resultColor = GameStyles::victoryGoldColor();
        // 红方胜利标识位于左侧队伍信息附近。
        resultRect = QRect(50, topY, 400, 80);

        // 垂直渐变降低大字对队伍信息的遮挡。
        QLinearGradient gradient(resultRect.topLeft(), resultRect.bottomLeft());
        gradient.setColorAt(0, QColor(255, 215, 0, 255));
        gradient.setColorAt(1, QColor(255, 215, 0, 0));
        QPen textPen(QBrush(gradient), 0);
        painter.setPen(textPen);
        painter.setFont(GameStyles::getHeaderFont());
        painter.drawText(resultRect, Qt::AlignLeft | Qt::AlignVCenter, resultText);

    } else if (m_result == BlueWin) {
        resultText = "WINNER";
        resultColor = GameStyles::victoryGoldColor();
        // 蓝方胜利标识位于右侧。
        resultRect = QRect(designSize().width() - 450, topY, 400, 80);

        QLinearGradient gradient(resultRect.topLeft(), resultRect.bottomLeft());
        gradient.setColorAt(0, QColor(255, 215, 0, 255));
        gradient.setColorAt(1, QColor(255, 215, 0, 0));
        QPen textPen(QBrush(gradient), 0);
        painter.setPen(textPen);
        painter.setFont(GameStyles::getScoreFont());
        painter.drawText(resultRect, Qt::AlignRight | Qt::AlignVCenter, resultText);
    } else {
        resultText = "DRAW";
        resultColor = GameStyles::neutralColor();
        // 平局标识居中。
        resultRect = QRect(centerX - 200, topY, 400, 80);
        painter.setPen(resultColor);
        painter.setFont(GameStyles::getHeaderFont());
        painter.drawText(resultRect, Qt::AlignCenter, resultText);
    }

    // 2. 中央比分
    QRect redScoreRect(centerX - 160, topY, 100, 80);
    painter.setPen(GameStyles::redTeamColor());
    painter.setFont(GameStyles::getScoreFont());
    painter.drawText(redScoreRect, Qt::AlignRight | Qt::AlignVCenter, QString::number(m_redScore));

    QRect blueScoreRect(centerX + 60, topY, 100, 80);
    painter.setPen(GameStyles::blueTeamColor());
    painter.drawText(blueScoreRect, Qt::AlignLeft | Qt::AlignVCenter, QString::number(m_blueScore));

    // 3. 比分上下方的局次与比赛时长
    QRect roundRect(centerX - 100, topY - 10, 200, 30);

    // 局次使用胶囊形背景。
    QPainterPath pillPath;
    pillPath.addRoundedRect(roundRect.adjusted(60, 0, -60, 0), 10, 10);
    painter.fillPath(pillPath, QColor(255, 255, 255, 200));

    painter.setPen(QColor(10, 14, 26));
    painter.setFont(QFont("Microsoft YaHei", 10, QFont::Bold));
    painter.drawText(roundRect, Qt::AlignCenter, QString("Round %1").arg(m_round));

    // 比分下方显示本局时长。
    QRect timeRect(centerX - 100, topY + 40, 200, 40);
    painter.setPen(QColor(200, 200, 200));
    painter.setFont(QFont("Microsoft YaHei", 10));
    painter.drawText(timeRect, Qt::AlignTop | Qt::AlignHCenter, "Round Duration");

    QRect timeValueRect(centerX - 100, topY + 60, 200, 30);
    painter.setFont(QFont("Microsoft YaHei", 14, QFont::Bold));
    painter.setPen(Qt::white);
    painter.drawText(timeValueRect, Qt::AlignTop | Qt::AlignHCenter, m_gameTime);

    // 4. 队名、校名和队徽，按“队名—队徽—比分—队徽—队名”排列。

    int nameY = topY + 20;
    int logoSize = 80;

    // 红方队徽靠近中央比分。
    QRect redLogoRect(centerX - 130 - logoSize, nameY - 10, logoSize, logoSize);

    // 红方队徽
    QPixmap redLogo(":/icons/red_team.svg");
    if (!redLogo.isNull()) {
        painter.drawPixmap(redLogoRect, redLogo.scaled(redLogoRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        // 队徽缺失时绘制字母占位符。
        painter.setPen(Qt::NoPen);
        painter.setBrush(GameStyles::redTeamColor());
        painter.drawEllipse(redLogoRect);
        painter.setPen(Qt::white);
        painter.drawText(redLogoRect, Qt::AlignCenter, "LOGO");
    }

    // 红方队名位于队徽左侧。
    QRect redNameRect(centerX - 450, nameY, 230, 60);
    painter.setPen(Qt::white);
    painter.setFont(GameStyles::getTeamNameFont());
    painter.drawText(redNameRect, Qt::AlignRight | Qt::AlignTop, m_redSchool);

    painter.setFont(GameStyles::getSchoolNameFont());
    painter.setPen(QColor(200, 200, 200));
    painter.drawText(redNameRect.adjusted(0, 30, 0, 0), Qt::AlignRight | Qt::AlignTop, m_redTeam);

    // 蓝方队徽位于比分右侧。
    QRect blueLogoRect(centerX + 130, nameY - 10, logoSize, logoSize);

    // 蓝方队徽
    QPixmap blueLogo(":/icons/blue_team.svg");
    if (!blueLogo.isNull()) {
        painter.drawPixmap(blueLogoRect, blueLogo.scaled(blueLogoRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        // 队徽缺失时绘制字母占位符。
        painter.setPen(Qt::NoPen);
        painter.setBrush(GameStyles::blueTeamColor());
        painter.drawEllipse(blueLogoRect);
        painter.setPen(Qt::white);
        painter.drawText(blueLogoRect, Qt::AlignCenter, "LOGO");
    }

    // 蓝方队名位于队徽右侧。
    QRect blueNameRect(centerX + 130 + logoSize + 20, nameY, 230, 60);
    painter.setPen(Qt::white);
    painter.setFont(GameStyles::getTeamNameFont());
    painter.drawText(blueNameRect, Qt::AlignLeft | Qt::AlignTop, m_blueSchool);

    painter.setFont(GameStyles::getSchoolNameFont());
    painter.setPen(QColor(200, 200, 200));
    painter.drawText(blueNameRect.adjusted(0, 30, 0, 0), Qt::AlignLeft | Qt::AlignTop, m_blueTeam);

    painter.restore();
}
