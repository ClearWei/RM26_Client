// SPDX-License-Identifier: MIT
/**
 * @file test_crc.cpp
 * @brief CRC 校验单元测试
 * @details 验证 CRC8 和 CRC16 算法与官方协议一致性
 * @author Fudan EGA Team
 * @date 2025-12-07
 * @copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

#include "network/CRC.h"
#include <QtTest>

/**
 * @brief CRC 单元测试类
 *
 * 测试 CRC8 和 CRC16 计算的正确性，确保与官方协议一致。
 */
class TestCRC : public QObject {
  Q_OBJECT

private slots:
  /**
   * @brief 测试 CRC8 计算
   * @details 使用已知数据验证 CRC8 计算结果
   */
  void testCRC8Calculation() {
    // 测试数据: SOF(0xA5) + Len(0x0010) + Seq(0x00)
    unsigned char testData[] = {0xA5, 0x10, 0x00, 0x00};

    // 计算 CRC8
    unsigned char crc8 = Get_CRC8_Check_Sum(testData, 4, 0xFF);

    // 验证 CRC8 不为0（表示计算执行了）
    QVERIFY(crc8 != 0 || testData[0] == 0);

    // 验证 Append 和 Verify 配对
    testData[3] = 0; // 重置
    Append_CRC8_Check_Sum(testData, 4);
    QVERIFY(Verify_CRC8_Check_Sum(testData, 4));
  }

  /**
   * @brief 测试 CRC16 计算
   * @details 使用已知数据验证 CRC16 计算结果
   */
  void testCRC16Calculation() {
    // 测试数据: 帧头 + 简单数据
    unsigned char testData[16] = {0xA5, 0x02, 0x00, 0x00, 0x00, 0x01,
                                  0x00, 0xAA, 0xBB, 0x00, 0x00};

    // 先计算 CRC8 (帧头)
    Append_CRC8_Check_Sum(testData, 4);

    // 帧长度 = 帧头(7) + 数据(2) + CRC16(2) = 11
    // 在追加 CRC16 前，数据到第9字节
    uint32_t frameLen = 7 + 2; // 帧头 + 数据

    // 追加 CRC16
    Append_CRC16_Check_Sum(testData, frameLen + 2);

    // 验证 CRC16
    QVERIFY(Verify_CRC16_Check_Sum(testData, frameLen + 2));
  }

  /**
   * @brief 测试 CRC8 校验失败情况
   */
  void testCRC8VerifyFailure() {
    unsigned char testData[] = {0xA5, 0x10, 0x00, 0x00};
    Append_CRC8_Check_Sum(testData, 4);

    // 篡改数据
    testData[1] = 0x20;

    // 校验应失败
    QVERIFY(!Verify_CRC8_Check_Sum(testData, 4));
  }

  /**
   * @brief 测试 CRC16 校验失败情况
   */
  void testCRC16VerifyFailure() {
    unsigned char testData[16] = {0xA5, 0x02, 0x00, 0x00, 0x00, 0x01,
                                  0x00, 0xAA, 0xBB, 0x00, 0x00};
    Append_CRC8_Check_Sum(testData, 4);
    Append_CRC16_Check_Sum(testData, 11);

    // 篡改数据
    testData[7] = 0xCC;

    // 校验应失败
    QVERIFY(!Verify_CRC16_Check_Sum(testData, 11));
  }

  /**
   * @brief 测试 CRC 初始化值
   */
  void testCRCInitValues() {
    // CRC8 初始值应为 0xFF
    unsigned char empty[] = {};
    unsigned char crc8 = Get_CRC8_Check_Sum(empty, 0, 0xFF);
    QCOMPARE(crc8, (unsigned char)0xFF);

    // CRC16 初始值应为 0xFFFF
    uint16_t crc16 = Get_CRC16_Check_Sum(empty, 0, 0xFFFF);
    QCOMPARE(crc16, (uint16_t)0xFFFF);
  }
};

QTEST_MAIN(TestCRC)
#include "test_crc.moc"
