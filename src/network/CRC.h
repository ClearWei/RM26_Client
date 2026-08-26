// SPDX-License-Identifier: MIT
/**
 * @file CRC.h
 * @brief CRC 循环冗余校验算法 - RoboMaster 通信协议专用
 * @details 本文件实现了 RoboMaster 裁判系统通信协议所需的 CRC8 和 CRC16
 * 校验算法。 CRC（Cyclic Redundancy
 * Check）是一种数据完整性校验方法，用于检测数据 在传输过程中是否发生错误。
 *
 *          协议帧结构中的 CRC 使用：
 *          - CRC8: 用于校验帧头（frame_header），占 1 字节
 *          - CRC16: 用于校验整帧数据（包括帧头和数据域），占 2 字节
 *
 * @author Clear
 * @date 2025-12-03
 * @copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

#ifndef CRC_H
#define CRC_H

#include <cstddef> // 提供 size_t 类型
#include <cstdint> // 提供 uint8_t, uint16_t, uint32_t 类型

// --- CRC8 校验函数声明 ---

/**
 * @brief 计算 CRC8 校验和
 * @details 使用查表法计算数据块的 CRC8 校验值。
 *          多项式：x^8 + x^5 + x^4 + 1 (0x31)
 *          初始值：0xFF
 *
 * @param pchMessage 指向待校验数据的指针
 * @param dwLength   数据长度（字节数）
 * @param ucCRC8     CRC8 初始值，通常为 0xFF
 * @return unsigned char 计算得到的 CRC8 校验值
 *
 * @note 该函数不会修改原始数据
 *
 * @example
 * @code
 * unsigned char data[] = {0xA5, 0x04, 0x00, 0x01};
 * unsigned char crc = Get_CRC8_Check_Sum(data, 4, 0xFF);
 * @endcode
 */
unsigned char Get_CRC8_Check_Sum(unsigned char *pchMessage,
                                 unsigned int dwLength, unsigned char ucCRC8);

/**
 * @brief 验证 CRC8 校验和
 * @details 验证数据块末尾的 CRC8 校验值是否正确。
 *          函数假设 CRC8 值存储在数据块的最后一个字节。
 *
 * @param pchMessage 指向待验证数据的指针（包含末尾的 CRC8 字节）
 * @param dwLength   总数据长度（包括 CRC8 字节）
 * @return unsigned int 验证结果：1 = 通过，0 = 失败
 *
 * @warning 如果 pchMessage 为 nullptr 或 dwLength <= 2，返回 0
 */
unsigned int Verify_CRC8_Check_Sum(unsigned char *pchMessage,
                                   unsigned int dwLength);

/**
 * @brief 在数据块末尾追加 CRC8 校验值
 * @details 计算数据的 CRC8 值并写入数据块的最后一个字节位置。
 *          调用前需确保数据缓冲区有足够空间。
 *
 * @param pchMessage 指向数据缓冲区的指针
 * @param dwLength   缓冲区总长度（包括预留的 CRC8 字节位置）
 *
 * @note 该函数会修改 pchMessage[dwLength-1] 的值
 */
void Append_CRC8_Check_Sum(unsigned char *pchMessage, unsigned int dwLength);

// --- CRC16 校验函数声明 ---

/**
 * @brief 计算 CRC16 校验和
 * @details 使用查表法计算数据块的 CRC16 校验值。
 *          多项式：x^16 + x^12 + x^5 + 1 (0x1021)
 *          初始值：0xFFFF
 *
 * @param pchMessage 指向待校验数据的指针
 * @param dwLength   数据长度（字节数）
 * @param wCRC       CRC16 初始值，通常为 0xFFFF
 * @return uint16_t  计算得到的 CRC16 校验值（小端序）
 *
 * @note 返回值的低字节在前，高字节在后（小端序存储）
 */
uint16_t Get_CRC16_Check_Sum(uint8_t *pchMessage, uint32_t dwLength,
                             uint16_t wCRC);

/**
 * @brief 验证 CRC16 校验和
 * @details 验证数据块末尾的 CRC16 校验值是否正确。
 *          函数假设 CRC16 值存储在数据块的最后两个字节（小端序）。
 *
 * @param pchMessage 指向待验证数据的指针（包含末尾的 CRC16 字节）
 * @param dwLength   总数据长度（包括 2 个 CRC16 字节）
 * @return uint32_t  验证结果：1 = 通过，0 = 失败
 *
 * @warning 如果 pchMessage 为 nullptr 或 dwLength <= 2，返回 0
 */
uint32_t Verify_CRC16_Check_Sum(uint8_t *pchMessage, uint32_t dwLength);

/**
 * @brief 在数据块末尾追加 CRC16 校验值
 * @details 计算数据的 CRC16 值并以小端序写入数据块的最后两个字节。
 *          调用前需确保数据缓冲区有足够空间。
 *
 * @param pchMessage 指向数据缓冲区的指针
 * @param dwLength   缓冲区总长度（包括预留的 2 个 CRC16 字节位置）
 *
 * @note 该函数会修改 pchMessage[dwLength-2] 和 pchMessage[dwLength-1] 的值
 *       存储格式：[低字节][高字节]（小端序）
 */
void Append_CRC16_Check_Sum(uint8_t *pchMessage, uint32_t dwLength);

#endif // CRC_H
