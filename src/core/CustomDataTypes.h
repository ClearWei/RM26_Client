/**
 * @file CustomDataTypes.h
 * @brief 自定义数据结构定义 (用于 CustomByteBlock 协议)
 * @details 机器人端和客户端共享的数据结构定义
 */

#ifndef CUSTOM_DATA_TYPES_H
#define CUSTOM_DATA_TYPES_H

#include <cstdint>

#pragma pack(push, 1)

/**
 * @struct RobotCustomStatus
 * @brief 机器人自定义状态数据结构
 * @details 总大小 28 字节，可通过 CustomByteBlock 发送
 *
 * 数据布局:
 * - Byte 0-1:  摩擦轮/拨弹轮/底盘状态
 * - Byte 2-5:  超级电容数据
 * - Byte 6-9:  角度数据
 * - Byte 10-27: 保留扩展
 */
struct RobotCustomStatus {
    // ========== Byte 0: 摩擦轮/拨弹轮状态 ==========
    union {
        struct {
            uint8_t fric_enabled : 1;      // 摩擦轮开启 (bit 0)
            uint8_t rammer_enabled : 1;    // 拨弹轮开启 (bit 1)
            uint8_t reserve1 : 6;          // 保留
        } bits;
        uint8_t byte;
    } friction_status;

    // ========== Byte 1: 底盘状态 ==========
    union {
        struct {
            uint8_t chassis_mode : 3;      // 底盘模式: 0=正常 1=跟随 2=小陀螺 3=保护
            uint8_t spin_mode : 1;         // 小陀螺模式 (冗余，便于显示)
            uint8_t follow_mode : 1;       // 跟随模式 (冗余)
            uint8_t chassis_protect : 1;   // 底盘保护
            uint8_t chassis_warning : 1;   // 底盘警告
            uint8_t reserve2 : 1;          // 保留
        } bits;
        uint8_t byte;
    } chassis_status;

    // ========== Byte 2-3: 超级电容当前能量 ==========
    // 精度: 0.1%, 范围 0-1000 (代表 0-100.0%)
    uint16_t super_cap_energy;

    // ========== Byte 4-5: 超级电容最大能量 ==========
    // 通常是 1000 (100.0%)
    uint16_t super_cap_max;

    // ========== Byte 6-7: 云台-底盘角度差 ==========
    // 精度: 0.1度, 范围 -1800~1800 (代表 -180.0~180.0度)
    int16_t gimbal_chassis_angle;

    // ========== Byte 8: 目标距离 ==========
    // 单位: 分米 (0.1米)，范围 0-255 (0-25.5米)
    uint8_t target_distance_dm;

    // ========== Byte 9: 弹道补偿 ==========
    // 单位: 像素或 0.1度，范围 -128~127
    int8_t ballistic_compensation;

    // ========== Byte 10-27: 保留扩展 ==========
    uint8_t reserved[18];

    // 默认构造函数
    RobotCustomStatus() {
        friction_status.byte = 0;
        chassis_status.byte = 0;
        super_cap_energy = 0;
        super_cap_max = 1000;
        gimbal_chassis_angle = 0;
        target_distance_dm = 0;
        ballistic_compensation = 0;
        memset(reserved, 0, sizeof(reserved));
    }
};

#pragma pack(pop)

// 确保结构体大小正确
static_assert(sizeof(RobotCustomStatus) == 28,
              "RobotCustomStatus size must be 28 bytes");

/**
 * @struct RobotMiniStateV1
 * @brief 0x0301 机器人最小状态摘要 (20B, 用于 TeamTelemetry 汇聚)
 */
struct RobotMiniStateV1 {
    uint8_t magic = 0xAA;
    uint8_t version = 1;
    uint8_t robotId = 0;
    uint8_t seq = 0;
    uint16_t timestampLow = 0;
    uint8_t flags = 0;
    uint8_t targetId = 0;
    uint8_t visionState = 0;
    uint8_t confidence = 0;
    uint8_t heatRatio = 0;
    uint8_t ammoRatio = 0;
    uint8_t capRatio = 0;
    uint8_t underFireLevel = 0;
    uint16_t faultFlags = 0;
    uint8_t crc8 = 0;
};

/// 0x0310 TeamTelemetry 汇聚协议头
struct TeamTelemetryHeader {
    uint8_t magic = 0xBB;
    uint8_t version = 1;
    uint8_t robotCount = 0;
    uint8_t seq = 0;
    uint16_t timestampLow = 0;
    int8_t rssi = 0;
    uint8_t crc8 = 0;
};

// 底盘模式枚举
enum class ChassisMode : uint8_t {
    NORMAL = 0,   // 正常模式
    FOLLOW = 1,   // 跟随模式
    SPIN = 2,     // 小陀螺模式
    PROTECT = 3   // 保护模式
};

#endif // CUSTOM_DATA_TYPES_H
