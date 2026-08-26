#ifndef H264STREAMRECOVERYPOLICY_H
#define H264STREAMRECOVERYPOLICY_H

#include <QtGlobal>

namespace RM {

/**
 * @brief H.264 连续分片流的无画面恢复策略。
 *
 * MQTT CustomByteBlock 每次只携带约 300 字节码流。单个分片没有产出完整
 * 图像是正常状态，不能按“连续空图次数”重置解码器。该策略仅在持续收到
 * 分片、但一段时间没有成功解码帧时触发恢复，并对恢复动作限频。
 */
class H264StreamRecoveryPolicy {
public:
  explicit H264StreamRecoveryPolicy(qint64 stallTimeoutMs = 2000,
                                    qint64 recoveryCooldownMs = 2000);

  /**
   * 在当前分片送入解码器之前判断是否需要恢复。触发恢复的分片会进入
   * 新的解析器，不会在解码完成后随重置操作一并清空。
   *
   * @return true 表示先重置解析器和解码上下文，再送入当前分片。
   */
  bool shouldRecoverBeforeFragment(qint64 nowMs);

  /** 记录当前分片处理结果；成功帧会重新开始无画面计时窗口。 */
  void observeDecodeResult(qint64 nowMs, bool decodedFrame);

  void reset();

  qint64 lastDecodedFrameMs() const { return m_lastDecodedFrameMs; }
  qint64 lastRecoveryMs() const { return m_lastRecoveryMs; }

private:
  qint64 m_stallTimeoutMs;
  qint64 m_recoveryCooldownMs;
  qint64 m_firstFragmentMs = -1;
  qint64 m_lastDecodedFrameMs = -1;
  qint64 m_lastRecoveryMs = -1;
};

} // namespace RM

#endif // H264STREAMRECOVERYPOLICY_H
