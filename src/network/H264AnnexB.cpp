#include "H264AnnexB.h"

namespace RM {
namespace {

struct StartCode {
  int offset = -1;
  int size = 0;
};

StartCode findStartCode(const uint8_t *data, int size, int from) {
  if (!data || size < 4) {
    return {};
  }

  for (int i = qMax(0, from); i + 3 < size; ++i) {
    if (data[i] != 0 || data[i + 1] != 0) {
      continue;
    }
    if (data[i + 2] == 1) {
      return {i, 3};
    }
    if (i + 3 < size && data[i + 2] == 0 && data[i + 3] == 1) {
      return {i, 4};
    }
  }
  return {};
}

} // namespace

bool h264AnnexBContainsNalType(const uint8_t *data, int size, int nalType) {
  return !h264AnnexBExtractNalUnit(data, size, nalType).isEmpty();
}

QByteArray h264AnnexBExtractNalUnit(const uint8_t *data, int size,
                                   int nalType) {
  if (!data || size <= 0 || nalType < 0 || nalType > 31) {
    return {};
  }

  StartCode current = findStartCode(data, size, 0);
  while (current.offset >= 0) {
    const int headerOffset = current.offset + current.size;
    if (headerOffset >= size) {
      return {};
    }

    const StartCode next = findStartCode(data, size, headerOffset + 1);
    const int endOffset = next.offset >= 0 ? next.offset : size;
    if ((data[headerOffset] & 0x1F) == nalType) {
      return QByteArray(reinterpret_cast<const char *>(data + current.offset),
                        endOffset - current.offset);
    }
    current = next;
  }

  return {};
}

} // namespace RM
