#include "H264StreamRecoveryPolicy.h"

#include <algorithm>

namespace RM {

H264StreamRecoveryPolicy::H264StreamRecoveryPolicy(qint64 stallTimeoutMs,
                                                    qint64 recoveryCooldownMs)
    : m_stallTimeoutMs(std::max<qint64>(1, stallTimeoutMs)),
      m_recoveryCooldownMs(std::max<qint64>(1, recoveryCooldownMs)) {}

bool H264StreamRecoveryPolicy::shouldRecoverBeforeFragment(qint64 nowMs) {
  if (m_firstFragmentMs < 0) {
    m_firstFragmentMs = nowMs;
  }

  const qint64 lastProgressMs =
      m_lastDecodedFrameMs >= 0 ? m_lastDecodedFrameMs : m_firstFragmentMs;
  if (nowMs - lastProgressMs < m_stallTimeoutMs) {
    return false;
  }

  if (m_lastRecoveryMs >= 0 &&
      nowMs - m_lastRecoveryMs < m_recoveryCooldownMs) {
    return false;
  }

  m_lastRecoveryMs = nowMs;
  return true;
}

void H264StreamRecoveryPolicy::observeDecodeResult(qint64 nowMs,
                                                   bool decodedFrame) {
  if (m_firstFragmentMs < 0) {
    m_firstFragmentMs = nowMs;
  }
  if (decodedFrame) {
    m_lastDecodedFrameMs = nowMs;
  }
}

void H264StreamRecoveryPolicy::reset() {
  m_firstFragmentMs = -1;
  m_lastDecodedFrameMs = -1;
  m_lastRecoveryMs = -1;
}

} // namespace RM
