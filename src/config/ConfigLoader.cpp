// SPDX-License-Identifier: MIT
/**
 * @file ConfigLoader.cpp
 * @brief 配置加载器实现
 * @author Clear
 * @date 2025-11-29
 * @copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

#include "ConfigLoader.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>

QJsonObject ConfigLoader::load(const QString &path, QString *foundPath) {
  // 尝试在多个位置查找配置文件
  QStringList searchPaths = {
      path, QCoreApplication::applicationDirPath() + "/" + path,
      QCoreApplication::applicationDirPath() + "/../Resources/" +
          path,    // macOS 应用包
      "../" + path // 开发环境
  };

  QFile file;
  bool found = false;
  QString foundFilePath;

  for (const QString &p : searchPaths) {
    if (QFile::exists(p)) {
      file.setFileName(p);
      found = true;
      foundFilePath = p;
      if (foundPath)
        *foundPath = p;
      qDebug() << "找到配置文件:" << p;
      break;
    }
  }

  if (!found) {
    qWarning() << "未找到配置文件:" << path;
    return QJsonObject();
  }

  if (!file.open(QIODevice::ReadOnly)) {
    qWarning() << "无法打开配置文件:" << file.fileName();
    return QJsonObject();
  }

  QByteArray data = file.readAll();
  file.close();

  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(data, &error);

  if (error.error != QJsonParseError::NoError) {
    qWarning() << "配置文件解析错误:" << error.errorString();
    return QJsonObject();
  }

  if (!doc.isObject()) {
    qWarning() << "配置文件格式错误: 根节点必须是对象";
    return QJsonObject();
  }

  return doc.object();
}
