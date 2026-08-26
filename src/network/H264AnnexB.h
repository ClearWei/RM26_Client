#ifndef H264ANNEXB_H
#define H264ANNEXB_H

#include <QByteArray>

#include <cstdint>

namespace RM {

/** 查找 Annex-B access unit 中是否包含指定 H.264 NAL 类型。 */
bool h264AnnexBContainsNalType(const uint8_t *data, int size, int nalType);

/** 提取首个指定类型的完整 Annex-B NAL（含 start code）。 */
QByteArray h264AnnexBExtractNalUnit(const uint8_t *data, int size, int nalType);

} // namespace RM

#endif // H264ANNEXB_H
