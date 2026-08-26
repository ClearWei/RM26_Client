// SPDX-License-Identifier: MIT
#ifndef CONFIGLOADER_H
#define CONFIGLOADER_H

#include <QJsonObject>
#include <QString>

/**
 * @file ConfigLoader.h
 * @brief 配置加载器
 * @details 负责从文件加载配置数据。
 * @author Clear
 * @date 2025-11-29
 * @copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */
class ConfigLoader {
public:
  /**
   * @brief 加载配置文件
   * @param path 配置文件路径
   * @return 加载的JSON对象，失败返回空对象
   */
  static QJsonObject load(const QString &path, QString *foundPath = nullptr);
};

#endif // CONFIGLOADER_H
