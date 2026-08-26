// SPDX-License-Identifier: MIT
/**
 * @file ConfigValidator.cpp
 * @brief 配置验证器实现
 * @author Clear
 * @date 2025-11-29
 * @copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

#include "ConfigValidator.h"
#include <QDebug>

bool ConfigValidator::validate(const QJsonObject &config, QString &errorMsg) {
  if (config.isEmpty()) {
    errorMsg = "配置为空";
    return false;
  }

  // 验证必要的根节点
  QStringList requiredKeys = {"app_settings", "network", "video", "robots"};
  for (const QString &key : requiredKeys) {
    if (!config.contains(key)) {
      errorMsg = QString("缺少必要的配置项: %1").arg(key);
      return false;
    }
  }

  // 验证网络配置
  QJsonObject network = config["network"].toObject();
  if (!network.contains("server_ip") || !network.contains("server_port")) {
    errorMsg = "网络配置不完整";
    return false;
  }

  return true;
}
