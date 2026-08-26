// SPDX-License-Identifier: MIT
/**
 * @file VideoReceiver.h
 * @brief 视频流接收器 - UDP 视频帧重组模块（官方协议）
 * @details 本文件定义了视频流接收和重组的核心类。
 *
 *          根据 **RoboMaster 2026 机甲大师高校系列赛通信协议 V1.0.0**：
 *          自定义客户端可通过 UDP 监听 3334 端口获取图传码流数据。
 *          编码格式为 HEVC (H.265)，每个 UDP 包的码流数据前 8 个字节固定为：
 *          - 帧编号（递增）：2 byte
 *          - 当前帧内分片序号：2 byte
 *          - 当前帧总字节数：4 byte
 *
 *          UDP 包格式（官方标准，无 SOF/CRC）：
 *          ┌────────────┬──────────────┬──────────────┬────────────────┐
 *          │ FrameID(2) │ SliceID(2)   │ TotalBytes(4)│ HEVC Data (N)  │
 *          │  大端序     │  大端序       │  大端序       │  裸码流数据     │
 *          └────────────┴──────────────┴──────────────┴────────────────┘
 *
 *          数据流架构：
 *          ┌─────────────┐   UDP Chunks   ┌─────────────────┐
 *          │  图传发送端  │ ─────────────► │  VideoReceiver  │
 *          │ (相机/编码器) │                └────────┬────────┘
 *          └─────────────┘                         │
 *                                         ┌────────▼────────┐
 *                                         │   帧重组缓冲区    │
 *                                         │ (m_pendingFrames)│
 *                                         └────────┬────────┘
 *                                                  │ 完整 HEVC NAL
 *                                         ┌────────▼────────┐
 *                                         │ hevcDataReady() │
 *                                         │     信号         │
 *                                         └─────────────────┘
 *
 * @author Clear
 * @date 2025-12-13
 * @copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

#ifndef VIDEORECEIVER_H
#define VIDEORECEIVER_H

// --- 头文件包含 ---
#include <QImage>     // Qt 图像类（保留用于可能的软件解码）
#include <QMap>       // Qt 映射容器，用于存储分片数据
#include <QMutex>     // Qt 互斥锁，保护多线程访问
#include <QObject>    // Qt 基础对象类
#include <QSet>       // Qt 集合容器，用于已组装帧ID去重
#include <QString>
#include <QUdpSocket> // Qt UDP 套接字类
#include "VideoDatagramLogger.h"
#include "H264StreamRecoveryPolicy.h"
#include <QVector>
#include <atomic>

#if defined(RM_ENABLE_FFMPEG)
extern "C" {
struct AVCodecParserContext;
struct AVCodecContext;
}
#endif

namespace RM {

// 前向声明
class HevcDecoder;
class H264Decoder;

// --- 常量定义 ---

/// 视频流默认监听端口（官方协议规定）
constexpr quint16 VIDEO_UDP_PORT = 3334;

/// 视频包头长度（FrameID + SliceID + TotalBytes）
constexpr int VIDEO_HEADER_SIZE = 8;

// --- 数据结构定义 ---

/**
 * @struct VideoFrame
 * @brief 视频帧结构体
 * @details 用于存储一帧视频的所有相关信息，包括多个分片。
 *          由于 UDP 包大小限制（MTU 约 1500 字节），一帧 HEVC 数据
 *          会被分割成多个 Chunk 发送。
 *
 *          分片重组示例：
 *          Frame[100] = Chunk[0] + Chunk[1] + Chunk[2] + ... + Chunk[N]
 *
 *          重组条件：receivedBytes >= totalBytes
 */
struct VideoFrame {
  quint16 frameId = 0;              ///< 帧 ID，用于标识和排序
  quint32 totalBytes = 0;           ///< 该帧的总字节数（来自包头，可能被截断调整）
  quint32 originalTotalBytes = 0;   ///< 未截断前的原始 totalBytes，0 表示无截断
  quint32 receivedBytes = 0;        ///< 当前已接收的字节数
  QMap<quint16, QByteArray> chunks; ///< 分片映射，Key = Slice ID，Value = 数据
  qint64 timestamp = 0;             ///< 本地接收时间戳（毫秒）
  qint64 firstPacketMs = 0;         ///< 该帧首个分片到达本机的时间戳（毫秒）
  qint64 lastPacketMs = 0;          ///< 该帧最后一个分片到达本机的时间戳（毫秒）
  quint32 duplicateSliceCount = 0;  ///< 重复分片数量
  quint32 frameGapCount = 0;        ///< 相对上一完成帧的缺帧数量
};

struct AssembledVideoFrame {
  bool valid = false;
  quint16 frameId = 0;
  QByteArray data;
  qint64 firstPacketMs = 0;
  qint64 lastPacketMs = 0;
  qint64 assembleStartMs = 0;
  qint64 assembleDoneMs = 0;
};

// --- 视频接收器类定义 ---

/**
 * @class VideoReceiver
 * @brief UDP 视频流接收器（官方协议版本）
 * @details 按照 RoboMaster 2026 官方协议接收和重组 HEVC 视频分片。
 *
 *          主要功能：
 *          1. 监听 UDP 端口 3334 接收视频分片
 *          2. 解析 8 字节包头（FrameID + SliceID + TotalBytes）
 *          3. 根据 FrameID 和 SliceID 重组分片
 *          4. 当一帧所有分片到齐后，发送 HEVC 裸码流
 *          5. 统计 FPS、码率等信息
 *
 *          使用示例：
 *          @code
 *          VideoReceiver receiver;
 *          connect(&receiver, &VideoReceiver::hevcDataReady,
 *                  this, &Widget::handleHevcFrame);
 *          receiver.startListening(3334);
 *          @endcode
 */
class VideoReceiver : public QObject {
  Q_OBJECT

public:
  // --- 构造与析构 ---

  /**
   * @brief 构造函数
   * @param parent Qt 父对象指针
   */
  explicit VideoReceiver(QObject *parent = nullptr);

  /**
   * @brief 析构函数
   */
  ~VideoReceiver();

  // --- 公共接口 ---

  /**
   * @brief 开始监听 UDP 端口
   * @param port 监听端口号，默认 3334（官方协议规定）
   * @return true 绑定成功，false 绑定失败
   */
  bool startListening(quint16 port = VIDEO_UDP_PORT);

  /**
   * @brief 停止监听
   */
  void stopListening();

  bool isListening() const { return m_listening.load(); }
  quint16 listeningPort() const { return m_listeningPort.load(); }
  quint64 totalPacketsReceived() const { return m_totalPacketsReceived.load(); }
  quint64 totalBytesReceived() const { return m_totalBytesReceived.load(); }
  quint32 totalFramesAssembled() const { return m_totalFramesDecoded.load(); }
  qint64 lastFrameMs() const { return m_lastFrameMs.load(); }
  qint64 lastPacketMs() const { return m_lastPacketMs.load(); }
  qint64 lastAssembledFrameMs() const { return m_lastAssembledFrameMs.load(); }
  double lastFps() const { return m_lastComputedFps.load(); }
  quint32 totalTimeoutFrames() const { return m_totalTimeoutFrames.load(); }
  quint32 totalGapEvents() const { return m_totalGapEvents.load(); }
  bool waitingForKeyframeRecovery() const { return m_waitingForKeyframeRecovery.load(); }
  HevcDecoder *decoder() const { return m_hevcDecoder; }

signals:
  // --- 信号定义 ---

  /**
   * @brief HEVC 帧数据就绪信号
   * @details 当一帧完整 HEVC 数据重组完成后发出。
   *          接收方可将数据传递给 FFmpeg/GStreamer 进行解码。
   *
   * @param frameId 帧 ID
   * @param data    完整的 HEVC NAL 单元数据
   */
  void hevcDataReady(quint16 frameId, const QByteArray &data);

  /**
   * @brief 图像接收信号（兼容旧接口）
   * @details 如果启用了软件解码，解码后的图像通过此信号发出。
   * @param image 解码后的 QImage 对象
   */
  void imageReceived(const QImage &image);
  void imageReceivedH264(const QImage &image);

  /**
   * @brief 带时序元数据的图像接收信号
   * @details 用于运行时链路分析：从 UDP 包到达、组帧、解码到 UI 渲染的
   *          时间戳会随帧传递给呈现层，正式业务逻辑不依赖这些字段。
   */
  void imageReceivedTimed(quint16 frameId, const QImage &image,
                          qint64 firstPacketMs, qint64 lastPacketMs,
                          qint64 assembleStartMs, qint64 assembleDoneMs,
                          qint64 decodeStartMs, qint64 decodeDoneMs);

  /**
   * @brief 统计信息更新信号
   * @param stats 格式化的统计信息字符串
   */
  void statsUpdated(const QString &stats);

public slots:
  /**
   * @brief 将MQTT CustomByteBlock提供的完整 H264 编码输入H264 解码链
   * @param data 完整的 NAL/码流数据
   */
  void feedH264Frame(const QByteArray &data);
  void resetHeroVideoStreamState();

private slots:
  // --- 私有槽函数 ---

  /**
   * @brief UDP 数据就绪处理槽
   */
  void onReadyRead();

private:
  // --- 私有成员变量 ---

  QUdpSocket *m_udpSocket;                   ///< UDP 套接字实例
  VideoDatagramLogger m_datagramLogger;      ///< 独立 UDP 图传包头日志
  QMap<quint16, VideoFrame> m_pendingFrames; ///< 待重组帧缓冲区，Key = FrameID
  QMutex m_mutex;                            ///< 互斥锁，保护帧缓冲区
  quint16 m_lastFrameId;                     ///< 上一帧 ID，用于乱序检测
  QSet<quint16> m_assembledFrameIds;         ///< 已组装帧ID集合，用于阻止迟到分片创建幻影条目

  // ---------------------------------------------------------------------------
  // 统计相关变量
  // ---------------------------------------------------------------------------
  std::atomic<quint64> m_totalBytesReceived; ///< 累计接收字节数
  std::atomic<quint64> m_totalPacketsReceived; ///< 累计接收包数
  std::atomic<quint32> m_totalFramesDecoded; ///< 累计完成帧数
  std::atomic<qint64> m_lastFrameMs;         ///< 最近一帧完成组装的时间戳（毫秒）
  std::atomic<qint64> m_lastPacketMs;        ///< 最近一个 UDP 包接收时间戳（毫秒）
  std::atomic<qint64> m_lastAssembledFrameMs; ///< 最近一帧完成组装时间戳（毫秒）
  std::atomic<qint64> m_lastDecodedFrameMs;  ///< 最近一帧成功解码时间戳（毫秒）
  std::atomic<qint64> m_lastDecodeAttemptMs; ///< 最近一次解码尝试时间戳（毫秒）
  std::atomic<qint64> m_lastStallLogMs;      ///< 最近一次卡住诊断日志时间戳（毫秒）
  std::atomic<qint64> m_lastStatsTime;       ///< 上次统计时间戳（毫秒）
  std::atomic<quint64> m_bytesInLastSecond;  ///< 最近一秒接收字节数
  std::atomic<quint32> m_framesInLastSecond; ///< 最近一秒完成帧数
  std::atomic<double> m_lastComputedFps;     ///< 最近一次统计得到的 FPS
  std::atomic<quint32> m_totalTimeoutFrames; ///< 累计超时清理帧数
  std::atomic<quint32> m_totalGapEvents;     ///< 累计检测到的帧缺口事件数
  std::atomic<quint32> m_transportDiscontinuityStreak; ///< 连续传输异常次数
  std::atomic<bool> m_waitingForKeyframeRecovery; ///< 当前是否正在等待关键帧恢复
  std::atomic<bool> m_listening; ///< UDP 接收器是否已绑定
  std::atomic<quint16> m_listeningPort; ///< 当前监听端口

  // ---------------------------------------------------------------------------
  // HEVC 解码器
  // ---------------------------------------------------------------------------
  HevcDecoder *m_hevcDecoder; ///< HEVC 解码器实例
  H264Decoder *m_h264Decoder = nullptr; ///< H.264 解码器实例
  QVector<AssembledVideoFrame> m_completedFrameQueue; ///< 已组装待解码帧队列

  // H.264 连续流恢复策略：空图表示等待更多分片，仅持续无画面才重置。
  H264StreamRecoveryPolicy m_h264RecoveryPolicy;

  // MQTT H.264 分片末尾附带 1 字节自增包序号，用于检测丢包。
  bool m_h264PacketIdSeen = false;
  quint8 m_lastH264PacketId = 0;
  quint64 m_h264FragmentCount = 0;
  quint64 m_h264PacketLossCount = 0;
  quint64 m_h264DecodedImageCount = 0;
  qint64 m_h264LastStatsLogMs = 0;
  quint64 m_h264LastStatsFragmentCount = 0;
  quint64 m_h264LastStatsLossCount = 0;
  quint64 m_h264LastStatsDecodedImageCount = 0;

  // H.264 最近一次收到分片的时间戳，用于卡住检测
  qint64 m_h264LastFragmentMs = 0;

  // H.264 未被解析器消费的尾部数据（用于跨包重组）
  QByteArray m_h264LeftoverBuffer;

  // 最近缓存的 Annex-B 参数集，用于在收到分片但缺少 SPS/PPS 时补齐。
  QByteArray m_h264Sps; ///< 含起始码的 SPS NAL
  QByteArray m_h264Pps; ///< 含起始码的 PPS NAL

  // 说明：解析器和解码器上下文已经移至 H264Decoder 管理。

  // --- 私有方法 ---

  /**
   * @brief 处理单个 UDP 数据包（官方协议格式）
   * @param packet 接收到的原始 UDP 数据报
   * @param packetReceivedMs 数据报进入本进程的本地时间戳（毫秒）
   */
  void processPacket(const QByteArray &packet, qint64 packetReceivedMs);

  /**
   * @brief 组装帧
   * @param frameId 要组装的帧 ID
   */
  AssembledVideoFrame assembleFrame(quint16 frameId);

  /**
   * @brief 解码并投递已组装帧
   */
  void decodeAssembledFrame(const AssembledVideoFrame &frame);

  /**
   * @brief 清理超时帧
   */
  void cleanupOldFrames();

  /**
   * @brief 更新统计信息
   */
  void updateStats();

  /**
   * @brief 标记上游图传不连续并触发解码器重同步
   */
  void markTransportDiscontinuity(const QString &reason);

  /**
   * @brief 按固定周期输出 CustomByte/H.264 传输统计
   */
  void logH264TransportStatsIfDue();
};

} // namespace RM

#endif // VIDEORECEIVER_H
