// SPDX-License-Identifier: MIT
#ifndef CONFIGVALIDATOR_H
#define CONFIGVALIDATOR_H

#include <QJsonObject>
#include <QString>

/**
 * @file ConfigValidator.h
 * @brief 配置验证器
 * @details 负责验证配置数据的完整性和合法性。
 * @author Clear
 * @date 2025-11-29
 * @copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */
class ConfigValidator {
public:
  /**
   * @brief 验证配置
   * @param config 配置JSON对象
   * @param errorMsg 输出错误信息
   * @return 验证通过返回true
   */
  static bool validate(const QJsonObject &config, QString &errorMsg);
};

#endif // CONFIGVALIDATOR_H
