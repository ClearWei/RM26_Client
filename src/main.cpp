// SPDX-License-Identifier: MIT
/**
 * @file main.cpp
 * @brief 应用程序入口文件
 * @details 初始化Qt应用程序，设置全局样式，创建并显示主窗口。
 * @author Clear
 * @date 2025-11-09
 * @copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

#include "ui/MainWindow.h"
#include "core/LogFileNaming.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QStyleFactory>
#include <QSurfaceFormat>
#include <QtGlobal>
#include <cstdlib>
#include <cstdio>

namespace {

QMutex g_runtimeLogMutex;
QFile *g_runtimeLogFile = nullptr;
QFile *g_timelineFile = nullptr;
QtMessageHandler g_previousMessageHandler = nullptr;
QString g_runtimeLogPath;
QString g_timelinePath;
int g_runtimeLogWriteCount = 0;
QString g_runTimestamp; // 本次运行的时间戳，用于统一文件命名

QString messageTypeName(QtMsgType type) {
  switch (type) {
  case QtDebugMsg:
    return QStringLiteral("DEBUG");
  case QtInfoMsg:
    return QStringLiteral("INFO");
  case QtWarningMsg:
    return QStringLiteral("WARN");
  case QtCriticalMsg:
    return QStringLiteral("CRITICAL");
  case QtFatalMsg:
    return QStringLiteral("FATAL");
  }
  return QStringLiteral("UNKNOWN");
}

// 以 mono_ms 为排序键，只保留 MQTT_RX 和 VIDEO_* 事件的精简行
static bool isTimelineEvent(const QString &msg) {
  return msg.contains(QStringLiteral("RM26_MQTT_RX_TIMING")) ||
         msg.contains(QStringLiteral("RM26_VIDEO_FRAME_TIMING")) ||
         msg.contains(QStringLiteral("RM26_VIDEO_FRAME_SUMMARY")) ||
         msg.contains(QStringLiteral("RM26_H264_FRAGMENT_SUMMARY")) ||
         msg.contains(QStringLiteral("RM26_VIDEO_STALL"));
}

QString defaultRuntimeLogPath() {
  const QString fromEnv = qEnvironmentVariable("RM_RUNTIME_LOG_PATH");
  if (!fromEnv.trimmed().isEmpty()) {
    return RM::timestampedFilePath(
        fromEnv, QDateTime::fromString(g_runTimestamp,
                                       QStringLiteral("yyyyMMdd-HHmmss")));
  }
  return QDir::current().absoluteFilePath(
      QStringLiteral("tmp/runtime_%1/runtime.log").arg(g_runTimestamp));
}

QString defaultTimelinePath() {
  const QString fromEnv = qEnvironmentVariable("RM_RUNTIME_LOG_PATH");
  if (!fromEnv.trimmed().isEmpty()) {
    // 如果有自定义日志路径，时间线文件放在同目录下
    QFileInfo fi(fromEnv);
    return RM::timestampedFilePath(
        fi.absolutePath() + QStringLiteral("/timeline.log"),
        QDateTime::fromString(g_runTimestamp,
                              QStringLiteral("yyyyMMdd-HHmmss")));
  }
  return QDir::current().absoluteFilePath(
      QStringLiteral("tmp/runtime_%1/timeline.log").arg(g_runTimestamp));
}

void runtimeMessageHandler(QtMsgType type, const QMessageLogContext &context,
                           const QString &msg) {
  const QString timestamp =
      QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
  QString line = QStringLiteral("%1 %2 %3")
                     .arg(timestamp, messageTypeName(type), msg);
  if (context.file && context.line > 0) {
    line += QStringLiteral(" [%1:%2]").arg(context.file).arg(context.line);
  }

  bool wroteToFile = false;
  {
    QMutexLocker locker(&g_runtimeLogMutex);

    // 写入完整日志
    if (g_runtimeLogFile && g_runtimeLogFile->isOpen()) {
      g_runtimeLogFile->write(line.toUtf8());
      g_runtimeLogFile->write("\n");
      wroteToFile = true;
      ++g_runtimeLogWriteCount;
      if (g_runtimeLogWriteCount % 64 == 0 || type == QtWarningMsg ||
          type == QtCriticalMsg || type == QtFatalMsg) {
        g_runtimeLogFile->flush();
      }
    }

    // 事件时间线：只写入 MQTT 和图传关键事件
    if (g_timelineFile && g_timelineFile->isOpen() && isTimelineEvent(msg)) {
      // 时间线格式: mono_ms|event_prefix|payload
      // 提取 mono_ms 或 wall_ms 作为排序键
      QString tlLine;
      int monoIdx = msg.indexOf(QStringLiteral("mono_ms="));
      if (monoIdx >= 0) {
        int endIdx = msg.indexOf(QStringLiteral(" "), monoIdx);
        if (endIdx < 0) endIdx = msg.length();
        tlLine = msg.mid(monoIdx, endIdx - monoIdx) + QStringLiteral("|") + msg;
      } else {
        // 无 mono_ms 时用 wall_ms 或置 0
        int wallIdx = msg.indexOf(QStringLiteral("wall_ms="));
        if (wallIdx >= 0) {
          int endIdx = msg.indexOf(QStringLiteral(" "), wallIdx);
          if (endIdx < 0) endIdx = msg.length();
          tlLine = msg.mid(wallIdx, endIdx - wallIdx) + QStringLiteral("|") + msg;
        } else {
          tlLine = QStringLiteral("mono_ms=0|") + msg;
        }
      }
      g_timelineFile->write(tlLine.toUtf8());
      g_timelineFile->write("\n");
    }
  }

  static const bool mirrorAll = qEnvironmentVariableIsSet("RM_RUNTIME_LOG_STDERR");
  const bool shouldMirrorToStderr =
      mirrorAll || !wroteToFile || type == QtWarningMsg ||
      type == QtCriticalMsg || type == QtFatalMsg;
  if (shouldMirrorToStderr) {
    const QByteArray localLine = line.toLocal8Bit();
    std::fprintf(stderr, "%s\n", localLine.constData());
    if (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) {
      std::fflush(stderr);
    }
  }

  if (type == QtFatalMsg) {
    std::abort();
  }
}

void installRuntimeLogHandler() {
  g_runTimestamp =
      QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
  g_runtimeLogPath = defaultRuntimeLogPath();
  g_timelinePath = defaultTimelinePath();

  QFileInfo logInfo(g_runtimeLogPath);
  QDir().mkpath(logInfo.absolutePath());

  auto *file = new QFile(g_runtimeLogPath);
  if (file->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
    QMutexLocker locker(&g_runtimeLogMutex);
    g_runtimeLogFile = file;
  } else {
    delete file;
    g_runtimeLogFile = nullptr;
  }

  auto *tlFile = new QFile(g_timelinePath);
  if (tlFile->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
    QMutexLocker locker(&g_runtimeLogMutex);
    g_timelineFile = tlFile;
  } else {
    delete tlFile;
    g_timelineFile = nullptr;
  }

  g_previousMessageHandler = qInstallMessageHandler(runtimeMessageHandler);
}

void shutdownRuntimeLogHandler() {
  qInstallMessageHandler(g_previousMessageHandler);
  QMutexLocker locker(&g_runtimeLogMutex);
  if (g_runtimeLogFile) {
    g_runtimeLogFile->flush();
    g_runtimeLogFile->close();
    delete g_runtimeLogFile;
    g_runtimeLogFile = nullptr;
  }
  if (g_timelineFile) {
    g_timelineFile->flush();
    g_timelineFile->close();
    delete g_timelineFile;
    g_timelineFile = nullptr;
  }
}

} // namespace

/**
 * @brief 主函数
 * @details 程序的执行入口。负责初始化Qt应用程序对象，配置应用程序元数据，
 *          设置UI主题样式，并启动主事件循环。
 *
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return int 应用程序退出码
 */
int main(int argc, char *argv[]) {
  installRuntimeLogHandler();

  // Qt Quick 渲染后端可配置。默认 software 兼容 Docker/X11；
  // 可通过 RM_QT_QUICK_BACKEND=opengl 切换到硬件加速。
  const QByteArray quickBackend = qgetenv("RM_QT_QUICK_BACKEND").toLower();
  if (quickBackend == "opengl") {
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
  } else {
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
  }

  // [关键] 设置默认 SurfaceFormat 以支持 alpha 通道
  // 必须在 QApplication 创建之前调用，否则 OpenGL 上下文已初始化，设置无效！
  QSurfaceFormat format;
  format.setAlphaBufferSize(8);
  QSurfaceFormat::setDefaultFormat(format);

  // 初始化QML资源（从静态库链接时需要）
  Q_INIT_RESOURCE(qml);
  Q_INIT_RESOURCE(resources);

  // 在创建 QApplication 之前设置 Qt Quick Controls 2 风格（修复QML Controls不支持的报错）
  QQuickStyle::setStyle("Fusion");

  // 创建Qt应用程序对象，管理GUI程序的控制流和主要设置
  QApplication app(argc, argv);

  // 设置应用程序元数据，用于QSettings等组件的默认路径
  app.setApplicationName("复旦大学 EGA战队客户端");
  app.setApplicationVersion("1.0.0");
  app.setOrganizationName("复旦大学 EGA战队");
  app.setWindowIcon(QIcon(":/images/app_icon.png"));

  qInfo().noquote() << QStringLiteral("RM26_RUNTIME_LOG_START path=%1 timeline=%2 pid=%3")
                           .arg(g_runtimeLogPath, g_timelinePath)
                           .arg(QCoreApplication::applicationPid());

  // 设置全局样式为 "Fusion"
  app.setStyle(QStyleFactory::create("Fusion"));

  // 创建主窗口对象
  // RM::MainWindow 是2025赛季及以后版本的主界面类
  RM::MainWindow window;

  // 显示主窗口
  window.showFullScreen();

  // 进入应用程序主事件循环，等待事件（如鼠标点击、键盘输入）
  const int exitCode = app.exec();
  qInfo().noquote() << QStringLiteral("RM26_RUNTIME_LOG_END exit_code=%1")
                           .arg(exitCode);
  shutdownRuntimeLogHandler();
  return exitCode;
}
