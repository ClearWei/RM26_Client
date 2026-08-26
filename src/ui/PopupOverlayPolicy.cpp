#include "PopupOverlayPolicy.h"

#include <QtMath>

namespace RM::PopupOverlayPolicy {

double resolutionScale(const QSize &viewport, const QSize &reference) {
  if (viewport.width() <= 0 || viewport.height() <= 0 ||
      reference.width() <= 0 || reference.height() <= 0) {
    return 1.0;
  }
  return qMin(static_cast<double>(viewport.width()) / reference.width(),
              static_cast<double>(viewport.height()) / reference.height());
}

QSize scaledSize(const QSize &baseSize, const QSize &viewport,
                 const QSize &reference) {
  const double scale = resolutionScale(viewport, reference);
  return QSize(qMax(1, qRound(baseSize.width() * scale)),
               qMax(1, qRound(baseSize.height() * scale)));
}

bool shouldActivateOverlay(bool hasActivePopups, bool hasRespawnPending) {
  return hasActivePopups || hasRespawnPending;
}

bool canSendPaidRespawn(bool isPendingRespawn, bool canPayForRespawn,
                        bool robotOnline) {
  return isPendingRespawn && canPayForRespawn && robotOnline;
}

bool canSendFreeRespawn(bool isPendingRespawn, bool canFreeRespawn,
                        bool robotOnline) {
  return isPendingRespawn && canFreeRespawn && robotOnline;
}

} // namespace RM::PopupOverlayPolicy
