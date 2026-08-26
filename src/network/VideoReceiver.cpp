// SPDX-License-Identifier: MIT
/**
 * @file VideoReceiver.cpp
 * @brief 视频流接收器实现文件（官方协议版本）
 * @details 本文件实现了 VideoReceiver 类，按照 RoboMaster 2026 官方协议
 *          解析 UDP 视频分片并重组为完整帧。
 *
 *          官方协议规定的 UDP 包格式（无 SOF/CRC）：
 *          ┌────────────┬──────────────┬──────────────┬────────────────┐
 *          │ FrameID(2) │ SliceID(2)   │ TotalBytes(4)│ HEVC Data (N)  │
 *          │  大端序     │  大端序       │  大端序       │  裸码流数据     │
 *          └────────────┴──────────────┴──────────────┴────────────────┘
 *          │◄────────── 8 字节 ───────►│
 *
 * @author Clear
 * @date 2025-12-13
 * @copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

// --- 头文件包含 ---
#include "VideoReceiver.h"
#include "HevcDecoder.h" // HEVC 解码器
#include "H264Decoder.h"
#if defined(RM_ENABLE_FFMPEG)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
#endif
#include <QDataStream>   // Qt 数据流，用于二进制解析
#include <QDateTime>     // Qt 日期时间类
#include <QDebug>        // Qt 调试输出
#include <QNetworkProxy>
#include <QtGlobal>
#include <QtEndian>      // Qt 字节序转换函数
#include <algorithm>
#include <chrono>
#include <cmath>

namespace RM {
namespace {

constexpr qint64 STALL_LOG_INTERVAL_MS = 1000;
constexpr qint64 STALL_THRESHOLD_MS = 1000;

QString videoLogTime() {
  return QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
}

qint64 runtimeMonoMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

QString runtimeLogFields() {
  const QDateTime now = QDateTime::currentDateTimeUtc();
  return QStringLiteral("wall_ms=%1 wall_time=%2 mono_ms=%3")
      .arg(now.toMSecsSinceEpoch())
      .arg(now.toString(Qt::ISODateWithMs))
      .arg(runtimeMonoMs());
}

bool verboseVideoLogEnabled() {
  static const bool enabled = qEnvironmentVariableIsSet("RM_VERBOSE_VIDEO_LOG");
  return enabled;
}

struct FrameLossStats {
  int uniqueSliceCount = 0;
  int duplicateSliceCount = 0;
  int missingSliceCount = 0;
  int missingBytes = 0;
};

FrameLossStats frameLossStats(const VideoFrame &frame) {
  FrameLossStats stats;
  stats.uniqueSliceCount = frame.chunks.size();
  stats.duplicateSliceCount = static_cast<int>(frame.duplicateSliceCount);
  stats.missingBytes =
      std::max(0, static_cast<int>(frame.totalBytes) -
                      static_cast<int>(frame.receivedBytes));
  if (frame.chunks.isEmpty()) {
    stats.missingSliceCount = stats.missingBytes > 0 ? 1 : 0;
    return stats;
  }

  const int observedMissingInRange =
      std::max(0, static_cast<int>(frame.chunks.lastKey()) + 1 -
                      stats.uniqueSliceCount);
  int estimatedMissingByBytes = 0;
  if (stats.missingBytes > 0 && frame.receivedBytes > 0) {
    const double avgPayloadBytes =
        static_cast<double>(frame.receivedBytes) / stats.uniqueSliceCount;
    estimatedMissingByBytes =
        static_cast<int>(std::ceil(stats.missingBytes / avgPayloadBytes));
  }
  stats.missingSliceCount =
      std::max(observedMissingInRange, estimatedMissingByBytes);
  return stats;
}

void logFrameSummary(const VideoFrame &frame, bool complete, bool timeout,
                     qint64 summaryMs) {
  Q_UNUSED(summaryMs);
  const FrameLossStats loss = frameLossStats(frame);
  qInfo().noquote()
      << QStringLiteral(
             "RM26_VIDEO_FRAME_SUMMARY %1 frame_id=%2 first_packet_ms=%3 "
             "last_packet_ms=%4 total_bytes=%5 received_bytes=%6 "
             "unique_slice_count=%7 duplicate_slice_count=%8 "
             "missing_slice_count=%9 missing_bytes=%10 frame_gap_count=%11 "
             "complete=%12 timeout=%13 receive_span_ms=%14")
             .arg(runtimeLogFields())
             .arg(frame.frameId)
             .arg(frame.firstPacketMs)
             .arg(frame.lastPacketMs)
             .arg(frame.totalBytes)
             .arg(frame.receivedBytes)
             .arg(loss.uniqueSliceCount)
             .arg(loss.duplicateSliceCount)
             .arg(loss.missingSliceCount)
             .arg(loss.missingBytes)
             .arg(frame.frameGapCount)
             .arg(complete ? 1 : 0)
             .arg(timeout ? 1 : 0)
             .arg(frame.firstPacketMs > 0 && frame.lastPacketMs > 0
                      ? frame.lastPacketMs - frame.firstPacketMs
                      : -1);
}

} // namespace

// --- 构造与析构 ---

/**
 * @brief 构造函数实现
 * @details 初始化 UDP 套接字、HEVC 解码器和成员变量，建立信号槽连接。
 */
VideoReceiver::VideoReceiver(QObject *parent)
    : QObject(parent), m_udpSocket(new QUdpSocket(this)), m_lastFrameId(0),
      m_totalBytesReceived(0), m_totalPacketsReceived(0),
      m_totalFramesDecoded(0), m_lastFrameMs(0), m_lastPacketMs(0),
      m_lastAssembledFrameMs(0), m_lastDecodedFrameMs(0),
      m_lastDecodeAttemptMs(0), m_lastStallLogMs(0), m_lastStatsTime(0),
      m_bytesInLastSecond(0),
      m_framesInLastSecond(0), m_lastComputedFps(0.0),
      m_totalTimeoutFrames(0), m_totalGapEvents(0),
      m_transportDiscontinuityStreak(0),
      m_waitingForKeyframeRecovery(false), m_listening(false),
      m_listeningPort(0), m_hevcDecoder(new HevcDecoder(this)),
      m_h264Decoder(new H264Decoder(this)) {
  // 避免继承环境里的 HTTP/SOCKS 代理，UDP socket 绑定时会直接失败。
  m_udpSocket->setProxy(QNetworkProxy::NoProxy);
  const int receiveBufferBytes =
      qEnvironmentVariableIntValue("RM_VIDEO_UDP_RCVBUF_BYTES") > 0
          ? qEnvironmentVariableIntValue("RM_VIDEO_UDP_RCVBUF_BYTES")
          : 8 * 1024 * 1024;
  m_udpSocket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption,
                               receiveBufferBytes);
  // 连接信号槽：有数据可读时触发 onReadyRead
  connect(m_udpSocket, &QUdpSocket::readyRead, this,
          &VideoReceiver::onReadyRead);

  // 初始化 HEVC 解码器
  if (!m_hevcDecoder->initialize()) {
    qWarning() << "VideoReceiver: HEVC 解码器初始化失败";
  }

  // 初始化 H.264 解码器
  if (m_h264Decoder) {
    if (!m_h264Decoder->initialize()) {
      qWarning() << "[custombyte] VideoReceiver: H264 decoder init failed";
    }
    // H264 英雄画面只走独立信号，避免误投递到主图传通道导致全屏显示
    connect(m_h264Decoder, &H264Decoder::frameDecoded, this,
            &VideoReceiver::imageReceivedH264);
    connect(m_h264Decoder, &H264Decoder::decodingError, this,
            [](const QString &error) { qWarning() << error; });
  }

  // 初始化统计起始时间
  m_lastStatsTime = QDateTime::currentMSecsSinceEpoch();
}

/**
 * @brief 析构函数实现
 */
VideoReceiver::~VideoReceiver() {
  stopListening();
}

// --- 连接控制 ---

/**
 * @brief 启动 UDP 监听
 * @details 绑定官方协议指定的端口（默认 3334）。
 */
bool VideoReceiver::startListening(quint16 port) {
  if (!m_udpSocket) {
    qWarning() << "VideoReceiver: UDP socket is null";
    return false;
  }

  if (m_udpSocket->state() != QAbstractSocket::UnconnectedState ||
      m_udpSocket->localPort() != 0) {
    qInfo() << "VideoReceiver: closing stale UDP socket before rebinding"
            << "state=" << m_udpSocket->state()
            << "localPort=" << m_udpSocket->localPort();
    m_udpSocket->close();
  }

  // 使用 ShareAddress/ReUseAddressHint 模式绑定端口，允许重复启动或快速重绑
  if (m_udpSocket->bind(QHostAddress::Any, port,
                        QUdpSocket::ShareAddress |
                            QUdpSocket::ReuseAddressHint)) {
    const int receiveBufferBytes =
        qEnvironmentVariableIntValue("RM_VIDEO_UDP_RCVBUF_BYTES") > 0
            ? qEnvironmentVariableIntValue("RM_VIDEO_UDP_RCVBUF_BYTES")
            : 8 * 1024 * 1024;
    m_udpSocket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption,
                                 receiveBufferBytes);
    m_listening.store(true, std::memory_order_relaxed);
    m_listeningPort.store(m_udpSocket->localPort(), std::memory_order_relaxed);
    m_datagramLogger.startSession(m_udpSocket->localPort());
    qDebug() << "VideoReceiver: 正在监听端口" << port << "(官方协议格式)";
    return true;
  }

  qWarning() << "VideoReceiver: 绑定端口失败" << port
             << "error=" << m_udpSocket->errorString();
  m_listening.store(false, std::memory_order_relaxed);
  m_listeningPort.store(0, std::memory_order_relaxed);
  qWarning() << "VideoReceiver: 绑定端口失败" << port;
  return false;
}

/**
 * @brief 停止监听
 */
void VideoReceiver::stopListening() {
  m_datagramLogger.stopSession();
  if (m_udpSocket->isOpen()) {
    m_udpSocket->close();
    qDebug() << "VideoReceiver: 已停止监听";
  }
  m_listening.store(false, std::memory_order_relaxed);
  m_listeningPort.store(0, std::memory_order_relaxed);
}

// --- 数据接收处理 ---

/**
 * @brief UDP 数据就绪处理
 */
void VideoReceiver::onReadyRead() {
  while (m_udpSocket->hasPendingDatagrams()) {
    // 读取数据报
    QByteArray datagram;
    datagram.resize(m_udpSocket->pendingDatagramSize());
    m_udpSocket->readDatagram(datagram.data(), datagram.size());
    m_datagramLogger.logDatagram(datagram, m_udpSocket->localPort());

    // 更新字节统计
    m_totalPacketsReceived.fetch_add(1, std::memory_order_relaxed);
    m_totalBytesReceived.fetch_add(static_cast<quint64>(datagram.size()),
                                   std::memory_order_relaxed);
    m_bytesInLastSecond.fetch_add(static_cast<quint64>(datagram.size()),
                                  std::memory_order_relaxed);
    const qint64 packetReceivedMs = QDateTime::currentMSecsSinceEpoch();
    m_lastPacketMs.store(packetReceivedMs, std::memory_order_relaxed);

    // 处理数据包
    processPacket(datagram, packetReceivedMs);
  }

  // 清理超时未完成的帧
  cleanupOldFrames();

  QVector<AssembledVideoFrame> completedFrames;
  completedFrames.swap(m_completedFrameQueue);
  for (const AssembledVideoFrame &frame : completedFrames) {
    decodeAssembledFrame(frame);
  }

  // 更新统计信息（每秒一次）
  updateStats();

  // 卡住检测：若数据到达但长时间未组装/解码/渲染，输出诊断日志
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  const qint64 lastStallLogMs =
      m_lastStallLogMs.load(std::memory_order_relaxed);
  if (now - lastStallLogMs >= STALL_LOG_INTERVAL_MS) {
    const qint64 lastAssembledMs =
        m_lastAssembledFrameMs.load(std::memory_order_relaxed);
    const qint64 lastDecodeAttemptMs =
        m_lastDecodeAttemptMs.load(std::memory_order_relaxed);
    const qint64 lastDecodedMs =
        m_lastDecodedFrameMs.load(std::memory_order_relaxed);
    const qint64 lastPktMs =
        m_lastPacketMs.load(std::memory_order_relaxed);

    QString stage;
    qint64 durationMs = 0;
    const bool hasRecentPackets =
        lastPktMs > 0 && (now - lastPktMs) < STALL_THRESHOLD_MS;

    if (hasRecentPackets &&
        (lastAssembledMs == 0 ||
         now - lastAssembledMs > STALL_THRESHOLD_MS)) {
      // 阶段 1: UDP 包持续到达但一直不组帧
      stage = QStringLiteral("receive_to_assemble");
      durationMs = lastAssembledMs > 0 ? now - lastAssembledMs
                                       : now - lastPktMs;
    } else if (!hasRecentPackets && lastAssembledMs > 0 &&
               now - lastAssembledMs < STALL_THRESHOLD_MS) {
      // 阶段 2: 组帧完成但一直不解码
      if (lastDecodeAttemptMs == 0 ||
          now - lastDecodeAttemptMs > STALL_THRESHOLD_MS) {
        stage = QStringLiteral("assemble_to_decode");
        durationMs = lastDecodeAttemptMs > 0 ? now - lastDecodeAttemptMs
                                             : now - lastAssembledMs;
      }
    } else if (lastDecodedMs > 0 &&
               now - lastDecodedMs > STALL_THRESHOLD_MS) {
      // 阶段 3: 解码成功但长期没有新帧输出
      stage = QStringLiteral("decode_to_render");
      durationMs = now - lastDecodedMs;
    }

    if (!stage.isEmpty()) {
      qWarning().noquote()
          << QStringLiteral(
                 "RM26_VIDEO_STALL %1 stage=%2 duration_ms=%3 last_frame_id=%4 "
                 "last_packet_ms=%5 last_assembled_ms=%6 "
                 "last_decode_attempt_ms=%7 last_decoded_ms=%8")
                 .arg(runtimeLogFields())
                 .arg(stage)
                 .arg(durationMs)
                 .arg(m_lastFrameId)
                 .arg(lastPktMs)
                 .arg(lastAssembledMs)
                 .arg(lastDecodeAttemptMs)
                 .arg(lastDecodedMs);
      m_lastStallLogMs.store(now, std::memory_order_relaxed);
    }
  }
}

/**
 * @brief 解析 UDP 视频数据包（官方协议格式）
 * @details 根据 RoboMaster 2026 官方协议解析：
 *
 *          包格式（无 SOF/CRC）：
 *          ┌────────────┬──────────────┬──────────────┬────────────────┐
 *          │ FrameID(2) │ SliceID(2)   │ TotalBytes(4)│ HEVC Data (N)  │
 *          └────────────┴──────────────┴──────────────┴────────────────┘
 *          偏移: 0        2              4              8
 *
 *          解析流程：
 *          1. 长度校验（至少 8 字节 + 1 字节数据）
 *          2. 解析 8 字节头部（大端序）
 *          3. 提取 HEVC 数据
 *          4. 存入帧缓冲区
 *          5. 检查是否可组装
 *
 * @param packet 原始 UDP 数据包
 */
void VideoReceiver::processPacket(const QByteArray &packet,
                                  qint64 packetReceivedMs) {
  // 详细日志开启时，每 100 个包记录一次包头摘要。
  static int packetCount = 0;
  if (verboseVideoLogEnabled() && packetCount++ % 100 == 0) {
    qDebug() << "VideoReceiver: Packet #" << packetCount
             << "size:" << packet.size() << "header:" << packet.left(8).toHex();
  }
  // 步骤 1: 最小长度校验
  // -------------------------------------------------------------------------
  // 官方协议：8 字节头部 + 至少 1 字节数据
  if (packet.size() < VIDEO_HEADER_SIZE + 1) {
    return;
  }

  // -------------------------------------------------------------------------
  // 步骤 2: 解析 8 字节头部（大端序）
  // -------------------------------------------------------------------------
  // 使用 Qt 的字节序转换函数确保跨平台兼容
  const uchar *data = reinterpret_cast<const uchar *>(packet.constData());

  // 帧编号（递增）：2 byte，大端序
  quint16 frameId = qFromBigEndian<quint16>(data);

  // 当前帧内分片序号：2 byte，大端序
  quint16 sliceId = qFromBigEndian<quint16>(data + 2);

  // 当前帧总字节数：4 byte，大端序
  quint32 totalBytes = qFromBigEndian<quint32>(data + 4);

  // -------------------------------------------------------------------------
  // 步骤 3: 提取 HEVC 数据（去掉 8 字节头部）
  // -------------------------------------------------------------------------
  QByteArray hevcData = packet.mid(VIDEO_HEADER_SIZE);

  // -------------------------------------------------------------------------
  // 步骤 4: 帧重组逻辑
  // -------------------------------------------------------------------------
  QMutexLocker locker(&m_mutex);
  VideoFrame &frame = m_pendingFrames[frameId];

  // 如果是新帧的第一个分片，初始化帧信息
  if (frame.chunks.isEmpty()) {
    frame.frameId = frameId;
    frame.totalBytes = totalBytes;
    frame.receivedBytes = 0;
    frame.timestamp = packetReceivedMs;
    frame.firstPacketMs = packetReceivedMs;
    if (verboseVideoLogEnabled()) {
      qDebug().noquote()
          << QStringLiteral("[%1] VideoReceiver: 新帧 ID= %2 总字节= %3")
                 .arg(videoLogTime())
                 .arg(frameId)
                 .arg(totalBytes);
    }
  }

  // 校验 totalBytes 是否一致
  if (frame.totalBytes != totalBytes) {
    qWarning() << "VideoReceiver: 帧" << frameId
               << "总字节数不匹配! 旧:" << frame.totalBytes
               << "新:" << totalBytes << "- 重置帧";
    frame.totalBytes = totalBytes;
    frame.receivedBytes = 0;
    frame.chunks.clear();
    frame.timestamp = packetReceivedMs;
    frame.firstPacketMs = packetReceivedMs;
    frame.lastPacketMs = 0;
  }

  // 存储分片数据（避免重复）
  if (!frame.chunks.contains(sliceId)) {
    frame.chunks[sliceId] = hevcData;
    frame.receivedBytes += hevcData.size();
    frame.lastPacketMs = packetReceivedMs;

    // 检查是否接收完成。这里只做组帧和入队，真正解码放到本轮 UDP 批量读取之后，
    // 避免解码耗时阻塞后续 UDP datagram 读取。
    if (frame.receivedBytes >= frame.totalBytes) {
      if (verboseVideoLogEnabled()) {
        qDebug() << "VideoReceiver: 帧" << frameId << "接收完成，开始组装";
      }
      AssembledVideoFrame assembled = assembleFrame(frameId);
      m_lastFrameId = frameId;
      m_pendingFrames.remove(frameId);
      if (assembled.valid) {
        m_completedFrameQueue.append(std::move(assembled));
      }
    }
  } else {
    ++frame.duplicateSliceCount;
  }
}

// --- 帧组装 ---

/**
 * @brief 组装完整帧
 * @details 将所有分片按 SliceID 顺序拼接为完整的 HEVC NAL 数据。
 */
AssembledVideoFrame VideoReceiver::assembleFrame(quint16 frameId) {
  AssembledVideoFrame assembled;
  if (!m_pendingFrames.contains(frameId))
    return assembled;

  const qint64 assembleStartMs = QDateTime::currentMSecsSinceEpoch();

  const quint16 expectedFrameId = static_cast<quint16>(m_lastFrameId + 1);
  VideoFrame &frame = m_pendingFrames[frameId];
  if (m_lastFrameId != 0 && frameId != expectedFrameId) {
    const quint16 gap = static_cast<quint16>(frameId - expectedFrameId);
    frame.frameGapCount = gap == 0 ? 1 : gap;
    m_totalGapEvents.fetch_add(1, std::memory_order_relaxed);
    markTransportDiscontinuity(
        QStringLiteral("检测到图传帧缺口，期望 %1，实际收到 %2")
            .arg(expectedFrameId)
            .arg(frameId));
  }

  QByteArray completeData;

  // 按 SliceID 顺序拼接所有分片
  // QMap 自动按 Key 排序
  for (auto it = frame.chunks.begin(); it != frame.chunks.end(); ++it) {
    completeData.append(it.value());
  }

  // 校验拼接后的总长度
  if (completeData.size() != (int)frame.totalBytes) {
    qWarning() << "VideoReceiver: 帧" << frameId << "组装长度不匹配: 预期"
               << frame.totalBytes << "实际" << completeData.size();
    // 依然发送，可能只是丢失了部分数据
  }

  // 更新统计
  m_totalFramesDecoded.fetch_add(1, std::memory_order_relaxed);
  m_framesInLastSecond.fetch_add(1, std::memory_order_relaxed);
  const qint64 assembledMs = QDateTime::currentMSecsSinceEpoch();
  m_lastFrameMs.store(assembledMs, std::memory_order_relaxed);
  m_lastAssembledFrameMs.store(assembledMs, std::memory_order_relaxed);

  if (verboseVideoLogEnabled()) {
    qDebug() << "VideoReceiver: 帧" << frameId
             << "组装完成，大小:" << completeData.size() << "字节";
  }
  logFrameSummary(frame, true, false, assembledMs);

  assembled.valid = true;
  assembled.frameId = frameId;
  assembled.data = std::move(completeData);
  assembled.firstPacketMs = frame.firstPacketMs;
  assembled.lastPacketMs = frame.lastPacketMs;
  assembled.assembleStartMs = assembleStartMs;
  assembled.assembleDoneMs = assembledMs;
  return assembled;
}

void VideoReceiver::decodeAssembledFrame(const AssembledVideoFrame &frame) {
  if (!frame.valid || frame.data.isEmpty()) {
    return;
  }

  // ---------------------------------------------------------------------------
  // 使用 FFmpeg HEVC 解码器解码。该阶段不持有 m_mutex，避免阻塞 UDP 入队。
  // ---------------------------------------------------------------------------
  emit hevcDataReady(frame.frameId, frame.data);
  if (m_hevcDecoder && m_hevcDecoder->isInitialized()) {
    const qint64 decodeStartMs = QDateTime::currentMSecsSinceEpoch();
    m_lastDecodeAttemptMs.store(decodeStartMs, std::memory_order_relaxed);
    QImage image = m_hevcDecoder->decode(frame.data);
    const qint64 decodeDoneMs = QDateTime::currentMSecsSinceEpoch();
    qInfo().noquote()
        << QStringLiteral(
               "RM26_VIDEO_FRAME_TIMING %1 event_version=2 stage=decode "
               "frame_id=%2 first_packet_ms=%3 last_packet_ms=%4 "
               "assemble_start_ms=%5 assemble_done_ms=%6 decode_start_ms=%7 "
               "decode_done_ms=%8 receive_span_ms=%9 assemble_ms=%10 "
               "first_packet_to_assemble_ms=%11 last_packet_to_assemble_ms=%12 "
               "assemble_to_decode_start_ms=%13 decode_ms=%14 decoded=%15 bytes=%16")
               .arg(runtimeLogFields())
               .arg(frame.frameId)
               .arg(frame.firstPacketMs)
               .arg(frame.lastPacketMs)
               .arg(frame.assembleStartMs)
               .arg(frame.assembleDoneMs)
               .arg(decodeStartMs)
               .arg(decodeDoneMs)
               .arg(frame.firstPacketMs > 0 && frame.lastPacketMs > 0
                        ? frame.lastPacketMs - frame.firstPacketMs
                        : -1)
               .arg(frame.assembleDoneMs - frame.assembleStartMs)
               .arg(frame.firstPacketMs > 0 ? frame.assembleDoneMs - frame.firstPacketMs
                                            : -1)
               .arg(frame.lastPacketMs > 0 ? frame.assembleDoneMs - frame.lastPacketMs
                                           : -1)
               .arg(decodeStartMs - frame.assembleDoneMs)
               .arg(decodeDoneMs - decodeStartMs)
               .arg(image.isNull() ? 0 : 1)
               .arg(frame.data.size());
    if (!image.isNull()) {
      m_waitingForKeyframeRecovery.store(false, std::memory_order_relaxed);
      m_transportDiscontinuityStreak.store(0, std::memory_order_relaxed);
      m_lastDecodedFrameMs.store(decodeDoneMs, std::memory_order_relaxed);
      emit imageReceivedTimed(frame.frameId, image, frame.firstPacketMs,
                              frame.lastPacketMs, frame.assembleStartMs,
                              frame.assembleDoneMs, decodeStartMs, decodeDoneMs);
      emit imageReceived(image);
      if (verboseVideoLogEnabled()) {
        qDebug() << "VideoReceiver: 帧" << frame.frameId << "HEVC 解码成功";
      }
    } else {
      if (m_waitingForKeyframeRecovery.load(std::memory_order_relaxed)) {
        qWarning() << "VideoReceiver: 帧" << frame.frameId
                   << "上游图传已出现丢帧/断流，当前等待关键帧恢复...";
      } else {
        // 解码失败，可能是不完整的 NAL 单元
        if (verboseVideoLogEnabled()) {
          qDebug() << "VideoReceiver: 帧" << frame.frameId << "等待更多数据...";
        }
      }
    }
  }
}

// 处理 MQTT CustomByteBlock 中的 H.264 分片。
void VideoReceiver::feedH264Frame(const QByteArray &data) {
  // H264Decoder 负责跨分片解析并完成解码。
  if (m_h264Decoder && m_h264Decoder->isInitialized()) {
    if (data.isEmpty()) {
      qWarning() << "[custombyte] VideoReceiver: empty H264 fragment received";
      return;
    }

    const qint64 fragmentReceivedMs = QDateTime::currentMSecsSinceEpoch();
    m_h264LastFragmentMs = fragmentReceivedMs;

    ++m_h264FragmentCount;
    const quint8 packetId = static_cast<quint8>(data.back());
    const quint8 expectedId = m_h264PacketIdSeen
                                  ? static_cast<quint8>(m_lastH264PacketId + 1)
                                  : packetId;
    if (verboseVideoLogEnabled()) {
      qDebug() << "[custombyte] VideoReceiver: fragment received bytes="
               << data.size() << "packetId=" << static_cast<unsigned>(packetId);
    }
    quint8 gap = 0;
    if (m_h264PacketIdSeen) {
      gap = static_cast<quint8>(packetId - expectedId);
      if (gap != 0) {
        m_h264PacketLossCount += gap;
        qWarning() << "[custombyte] VideoReceiver: detected fragment loss"
                   << "expectedId=" << static_cast<unsigned>(expectedId)
                   << "actualId=" << static_cast<unsigned>(packetId)
                   << "lostPackets=" << static_cast<unsigned>(gap)
                   << "totalLost=" << m_h264PacketLossCount;
      }
    }
    m_h264PacketIdSeen = true;
    m_lastH264PacketId = packetId;

    const QByteArray payload = data.left(data.size() - 1);
    if (payload.isEmpty()) {
      qWarning() << "[custombyte] VideoReceiver: fragment only contains packet id";
      return;
    }

    const qint64 fragmentMonoMs = runtimeMonoMs();
    const bool shouldRecover =
        m_h264RecoveryPolicy.shouldRecoverBeforeFragment(fragmentMonoMs);
    if (shouldRecover) {
      qWarning() << "[custombyte] VideoReceiver: H264 stream produced no "
                    "frame for 2000 ms; resetting parser/codec";
      m_h264Decoder->resetParserAndCodec();
    }
    // 恢复判断必须发生在解码前；当前分片可能正是下一组 SPS/IDR
    // 的开头，不能先送入旧解析器再重置。
    QImage img = m_h264Decoder->decode(payload);
    const bool decoded = !img.isNull();
    m_h264RecoveryPolicy.observeDecodeResult(fragmentMonoMs, decoded);
    if (verboseVideoLogEnabled()) {
      qInfo().noquote()
          << QStringLiteral(
                 "RM26_H264_FRAGMENT_SUMMARY %1 fragment_index=%2 "
                 "expected_packet_id=%3 actual_packet_id=%4 "
                 "lost_fragments=%5 total_lost_fragments=%6 "
                 "payload_bytes=%7 decoded=%8")
                 .arg(runtimeLogFields())
                 .arg(m_h264FragmentCount)
                 .arg(expectedId)
                 .arg(packetId)
                 .arg(static_cast<unsigned>(gap))
                 .arg(m_h264PacketLossCount)
                 .arg(payload.size())
                 .arg(decoded ? 1 : 0);
    }

    if (decoded) {
      // frameDecoded 已连接到 imageReceivedH264，此处不再发出同一帧。
      ++m_h264DecodedImageCount;
      if (verboseVideoLogEnabled()) {
        qDebug() << "[custombyte] VideoReceiver: decode produced image"
                 << "size=" << img.size();
      }
      logH264TransportStatsIfDue();
      return;
    } else {
      if (verboseVideoLogEnabled()) {
        qDebug() << "[custombyte] VideoReceiver: waiting for more H264 fragments";
      }
    }
    logH264TransportStatsIfDue();
  } else {
    qWarning() << "[custombyte] VideoReceiver: H264 decoder not initialized";
  }
}

void VideoReceiver::resetHeroVideoStreamState() {
  qWarning() << "[custombyte] VideoReceiver: manual hero video refresh requested";

  if (m_h264Decoder) {
    m_h264Decoder->resetParserAndCodec();
  }

  m_h264RecoveryPolicy.reset();
  m_h264PacketIdSeen = false;
  m_lastH264PacketId = 0;
  m_h264PacketLossCount = 0;
  m_h264FragmentCount = 0;
  m_h264DecodedImageCount = 0;
  m_h264LastStatsLogMs = 0;
  m_h264LastStatsFragmentCount = 0;
  m_h264LastStatsLossCount = 0;
  m_h264LastStatsDecodedImageCount = 0;
  m_h264LeftoverBuffer.clear();
  m_h264Sps.clear();
  m_h264Pps.clear();
}

// --- 维护与统计 ---

/**
 * @brief 清理超时帧
 */
void VideoReceiver::cleanupOldFrames() {
  QString discontinuityReason;
  QMutexLocker locker(&m_mutex);
  qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

  auto it = m_pendingFrames.begin();
  while (it != m_pendingFrames.end()) {
    if (currentTime - it.value().timestamp > 500) { // 500ms 超时
      qDebug().noquote() << QStringLiteral("[%1] VideoReceiver: 清理超时帧 %2")
                                .arg(videoLogTime())
                                .arg(it.key());
      logFrameSummary(it.value(), false, true, currentTime);
      m_totalTimeoutFrames.fetch_add(1, std::memory_order_relaxed);
      discontinuityReason =
          QStringLiteral("帧 %1 在重组阶段超时，判定上游图传不连续")
              .arg(it.key());
      it = m_pendingFrames.erase(it);
    } else {
      ++it;
    }
  }

  locker.unlock();
  if (!discontinuityReason.isEmpty()) {
    markTransportDiscontinuity(discontinuityReason);
  }
}

/**
 * @brief 更新统计信息
 */
void VideoReceiver::updateStats() {
  qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
  const qint64 lastStatsTime = m_lastStatsTime.load(std::memory_order_relaxed);

  if (currentTime - lastStatsTime >= 1000) {
    const quint32 framesInLastSecond =
        m_framesInLastSecond.exchange(0, std::memory_order_relaxed);
    const quint64 bytesInLastSecond =
        m_bytesInLastSecond.exchange(0, std::memory_order_relaxed);
    double fps =
        framesInLastSecond * 1000.0 / (currentTime - lastStatsTime);
    m_lastComputedFps.store(fps, std::memory_order_relaxed);
    double bitrate = bytesInLastSecond * 8.0 / 1024.0 / 1024.0; // Mbps

    QString stats =
        QString("[视频-官方协议] FPS: %1, 码率: %2 Mbps, 总帧数: %3")
            .arg(fps, 0, 'f', 1)
            .arg(bitrate, 0, 'f', 2)
            .arg(m_totalFramesDecoded.load(std::memory_order_relaxed));

    emit statsUpdated(stats);

    m_lastStatsTime.store(currentTime, std::memory_order_relaxed);
  }
}

void VideoReceiver::markTransportDiscontinuity(const QString &reason) {
  m_waitingForKeyframeRecovery.store(true, std::memory_order_relaxed);
  const quint32 streak =
      m_transportDiscontinuityStreak.fetch_add(1, std::memory_order_relaxed) + 1;
  const int resetThreshold =
      qEnvironmentVariableIntValue("RM_VIDEO_RESET_ON_LOSS_THRESHOLD") > 0
          ? qEnvironmentVariableIntValue("RM_VIDEO_RESET_ON_LOSS_THRESHOLD")
          : 3;
  qWarning() << "VideoReceiver: [DISCONTINUITY]" << reason
             << "streak=" << streak << "resetThreshold=" << resetThreshold;

  if (streak >= static_cast<quint32>(resetThreshold) && m_hevcDecoder &&
      m_hevcDecoder->isInitialized()) {
    m_hevcDecoder->resetStreamState();
    m_transportDiscontinuityStreak.store(0, std::memory_order_relaxed);
  }
}

void VideoReceiver::logH264TransportStatsIfDue() {
  const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
  if (m_h264LastStatsLogMs == 0) {
    m_h264LastStatsLogMs = nowMs;
    return;
  }

  const qint64 elapsedMs = nowMs - m_h264LastStatsLogMs;
  if (elapsedMs < 1000) {
    return;
  }

  const quint64 deltaFragments =
      m_h264FragmentCount - m_h264LastStatsFragmentCount;
  const quint64 deltaLoss = m_h264PacketLossCount - m_h264LastStatsLossCount;
  const quint64 deltaDecoded =
      m_h264DecodedImageCount - m_h264LastStatsDecodedImageCount;
  const quint64 expectedFragments = deltaFragments + deltaLoss;
  const double windowLossRate =
      expectedFragments > 0
          ? (static_cast<double>(deltaLoss) * 100.0 /
             static_cast<double>(expectedFragments))
          : 0.0;

  const quint64 totalExpectedFragments =
      m_h264FragmentCount + m_h264PacketLossCount;
  const double totalLossRate =
      totalExpectedFragments > 0
          ? (static_cast<double>(m_h264PacketLossCount) * 100.0 /
             static_cast<double>(totalExpectedFragments))
          : 0.0;

  const double decodedFps =
      elapsedMs > 0 ? static_cast<double>(deltaDecoded) * 1000.0 / elapsedMs
                    : 0.0;
  qInfo().noquote()
      << QStringLiteral("[custombyte] H264 transport stats: windowFragments=%1 "
                        "windowLost=%2 windowLossRate=%3% decodedImages=%4 "
                        "decodedFps=%5 totalFragments=%6 totalLost=%7 "
                        "totalLossRate=%8%")
             .arg(deltaFragments)
             .arg(deltaLoss)
             .arg(windowLossRate, 0, 'f', 2)
             .arg(deltaDecoded)
             .arg(decodedFps, 0, 'f', 1)
             .arg(m_h264FragmentCount)
             .arg(m_h264PacketLossCount)
             .arg(totalLossRate, 0, 'f', 2);

  m_h264LastStatsLogMs = nowMs;
  m_h264LastStatsFragmentCount = m_h264FragmentCount;
  m_h264LastStatsLossCount = m_h264PacketLossCount;
  m_h264LastStatsDecodedImageCount = m_h264DecodedImageCount;
}

} // namespace RM
