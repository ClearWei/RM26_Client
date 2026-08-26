#ifndef POPUPOVERLAYPOLICY_H
#define POPUPOVERLAYPOLICY_H

#include <QSize>

namespace RM::PopupOverlayPolicy {

double resolutionScale(const QSize &viewport,
                       const QSize &reference = QSize(1920, 1080));
QSize scaledSize(const QSize &baseSize, const QSize &viewport,
                 const QSize &reference = QSize(1920, 1080));
bool shouldActivateOverlay(bool hasActivePopups, bool hasRespawnPending);
bool canSendPaidRespawn(bool isPendingRespawn, bool canPayForRespawn,
                        bool robotOnline);
bool canSendFreeRespawn(bool isPendingRespawn, bool canFreeRespawn,
                        bool robotOnline);

} // namespace RM::PopupOverlayPolicy

#endif // POPUPOVERLAYPOLICY_H
