// SPDX-License-Identifier: MIT
/**
 * @file MqttLogger.cpp
 * @brief MQTT 消息日志记录器实现
 * @author Clear
 * @date 2026-05-30
 * @copyright Copyright (c) 2026 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

#include "MqttLogger.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

// ============================================================================
// 单例
// ============================================================================

MqttLogger *MqttLogger::instance() {
  static MqttLogger s_instance;
  return &s_instance;
}

// ============================================================================
// 构造 / 析构
// ============================================================================

MqttLogger::MqttLogger(QObject *parent)
    : QObject(parent) {
  m_flushTimer.setInterval(FLUSH_INTERVAL_MS);
  connect(&m_flushTimer, &QTimer::timeout, this, &MqttLogger::flushBuffer);
}

MqttLogger::~MqttLogger() {
  stopSession();
}

// ============================================================================
// 会话管理
// ============================================================================

void MqttLogger::startSession() {
  if (m_isActive) {
    // 先停止已有会话（正常不应发生，防御性处理）
    stopSession();
  }

  // 日志目录：优先环境变量 RM_LOG_DIR，否则自动查找项目根目录
  const QString envLogDir =
      qEnvironmentVariable("RM_LOG_DIR").trimmed();
  QString logRoot;
  if (!envLogDir.isEmpty()) {
    logRoot = envLogDir;
  } else {
    // 从可执行文件所在目录向上查找项目根目录（兼容 macOS .app bundle 和 Ubuntu）
    QDir dir(QCoreApplication::applicationDirPath());
    bool found = false;
    for (int i = 0; i < 8; ++i) {
      if (QFileInfo::exists(dir.filePath(QStringLiteral("config.json"))) &&
          QFileInfo(dir.filePath(QStringLiteral("src"))).isDir()) {
        found = true;
        break;
      }
      if (!dir.cdUp()) break;
    }
    logRoot = found ? dir.absolutePath() : QCoreApplication::applicationDirPath();
    logRoot += QStringLiteral("/tmp/log");
  }
  QDir().mkpath(logRoot);

  const QString timestamp =
      QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
  m_logFilePath = logRoot + QStringLiteral("/mqtt_full_") + timestamp + QStringLiteral(".jsonl");

  m_file.setFileName(m_logFilePath);
  if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
    qWarning() << "MqttLogger: Failed to open log file:" << m_logFilePath;
    return;
  }

  m_isActive = true;
  m_flushTimer.start();
  qInfo() << "MqttLogger: Session started, log:" << m_logFilePath;
}

void MqttLogger::stopSession() {
  m_flushTimer.stop();
  flushBuffer();

  if (m_file.isOpen()) {
    m_file.close();
  }

  if (m_isActive) {
    qInfo() << "MqttLogger: Session stopped, logged to:" << m_logFilePath;
    m_isActive = false;
  }
}

bool MqttLogger::isActive() const {
  return m_isActive;
}

// ============================================================================
// 日志方法（线程安全）
// ============================================================================

void MqttLogger::logRx(const QString &topic, const QByteArray &payload) {
  if (!m_isActive) return;
  if (shouldSkip(topic, payload)) return;

  QJsonObject obj;
  obj.insert(QStringLiteral("ts"),
             QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
  obj.insert(QStringLiteral("dir"), QStringLiteral("rx"));
  obj.insert(QStringLiteral("topic"), topic);
  obj.insert(QStringLiteral("bytes"), payload.size());
  obj.insert(QStringLiteral("hex"),
             QString::fromLatin1(payload.toHex()));

  appendLine(QString::fromUtf8(
      QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void MqttLogger::logTx(const QString &topic, const QByteArray &payload) {
  if (!m_isActive) return;

  QJsonObject obj;
  obj.insert(QStringLiteral("ts"),
             QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
  obj.insert(QStringLiteral("dir"), QStringLiteral("tx"));
  obj.insert(QStringLiteral("topic"), topic);
  obj.insert(QStringLiteral("bytes"), payload.size());
  obj.insert(QStringLiteral("hex"),
             QString::fromLatin1(payload.toHex()));

  appendLine(QString::fromUtf8(
      QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

// ============================================================================
// 私有方法
// ============================================================================

bool MqttLogger::shouldSkip(const QString &topic, const QByteArray &payload) {
  Q_UNUSED(payload)
  // 跳过 CustomByteBlock 中的 H.264 视频帧（>100 字节视为视频数据）
  // 28~100 字节的 CustomByteBlock 是机器人自定义状态数据，需要保留
  if (topic == QLatin1String("CustomByteBlock") && payload.size() > 100) {
    return true;
  }
  return false;
}

void MqttLogger::appendLine(const QString &line) {
  QMutexLocker locker(&m_mutex);
  m_buffer.append(line);

  // 缓冲区满时立即刷盘（timer 已运行的间隔内不会重复触发）
  if (m_buffer.size() >= MAX_BUFFER_SIZE) {
    // 不在此处直接 flush（避免在 logRx/logTx 线程中做文件 I/O），
    // 由 timer 在下次 tick 时执行，最坏延迟 = FLUSH_INTERVAL_MS
  }
}

void MqttLogger::flushBuffer() {
  QStringList batch;
  {
    QMutexLocker locker(&m_mutex);
    if (m_buffer.isEmpty()) return;
    batch.swap(m_buffer);
  }

  if (!m_file.isOpen()) {
    qWarning() << "MqttLogger: flushBuffer called but file is not open";
    return;
  }

  for (const QString &line : std::as_const(batch)) {
    m_file.write(line.toUtf8());
    m_file.write("\n");
  }
  m_file.flush();
}
