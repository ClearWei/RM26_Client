// SPDX-License-Identifier: MIT
/**
 * @file ConfigManager.h
 * @brief 配置管理器
 * @details 单例模式，提供全局配置访问接口。
 * @author Clear
 * @date 2025-11-29
 * @copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include "ConfigLoader.h"
#include "ConfigValidator.h"
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QObject>

class ConfigManager : public QObject {
  Q_OBJECT

public:
  /**
   * @brief 获取单例实例
   * @return ConfigManager& 实例引用
   */
  static ConfigManager &instance();

  /**
   * @brief 加载配置文件
   * @param configPath 配置文件路径，默认为 "config.json"
   * @return bool 加载是否成功
   */
  bool loadConfig(const QString &configPath = "config.json");

  // 应用配置访问器
  QString getAppName() const;
  QString getAppVersion() const;
  QString getUIText(const QString &key) const;

  // 网络配置访问器
  QString getServerIP() const;
  quint16 getServerPort() const;
  quint16 getClientPort() const;
  quint16 getVideoPort() const;
  QString getMqttBroker() const;
  quint16 getMqttPort() const;
  int getClientRobotId() const;

  // 窗口配置访问器
  int getWindowMinWidth() const;      // 窗口最小宽度 (默认1280)
  int getWindowMinHeight() const;     // 窗口最小高度 (默认720)
  int getWindowDefaultWidth() const;  // 窗口默认宽度 (默认1920)
  int getWindowDefaultHeight() const; // 窗口默认高度 (默认1080)
  bool getWindowFullscreen() const;   // 是否全屏 (默认true)

  // 视频配置访问器
  QString getDefaultVideoPath() const;
  QString getVideoStreamUrl() const; // 图传视频流URL (udp://ip:port)
  QString getVideoResolution() const;
  bool getIndustrialCameraGridEnabled() const; // 工业相机校准网格（默认关闭）

  // 机器人配置访问器
  QJsonObject getRobotConfig(const QString &robotType) const;
  QString getRobotLayout(const QString &robotType) const;

  // --- AR 叠加配置访问器 ---
  bool getAROverlayEnabled() const;       // AR 叠加是否启用 (默认false)
  QString getARModelPath() const;         // YOLO 模型路径
  float getARConfidenceThreshold() const; // 置信度阈值 (默认0.5)
  float getARNMSThreshold() const;        // NMS 阈值 (默认0.4)
  float getARSmoothingFactor() const;     // 平滑因子 (默认0.3)
  int getARMaxMissedFrames() const;       // 最大丢失帧数 (默认10)
  int getARDetectionInterval() const;     // 检测间隔毫秒 (默认33)

  // --- AR 配置写入接口 ---
  void setAROverlayEnabled(bool enabled);

signals:
  /**
   * @brief 配置重载信号
   * @details 当配置文件发生变化并重新加载后发射此信号。
   */
  void configReloaded();

private slots:
  /**
   * @brief 处理文件变化
   * @param path 发生变化的文件路径
   */
  void onFileChanged(const QString &path);

private:
  explicit ConfigManager(QObject *parent = nullptr);
  ~ConfigManager();
  ConfigManager(const ConfigManager &) = delete;
  ConfigManager &operator=(const ConfigManager &) = delete;

  QString getEnvOrConfigString(const char *envName, const QString &section,
                               const QString &key,
                               const QString &defaultValue) const;
  quint16 getEnvOrConfigPort(const char *envName, const QString &section,
                             const QString &key,
                             quint16 defaultValue) const;

  QJsonObject m_config;
  static QMutex m_mutex;
  class QFileSystemWatcher *m_watcher;
  QString m_currentConfigPath;
};

#endif // CONFIGMANAGER_H
