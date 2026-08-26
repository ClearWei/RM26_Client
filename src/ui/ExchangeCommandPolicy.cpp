#include "ExchangeCommandPolicy.h"

namespace RM::ExchangeCommandPolicy {

bool canOpenRemoteExchange(bool canRemoteHeal, bool canRemoteAmmo) {
  return canRemoteHeal || canRemoteAmmo;
}

bool isValidRequest(int commandType, int param) {
  switch (commandType) {
  case kExchange17mmCommand:
    return param > 0 && param % 100 == 0;
  case kExchange42mmCommand:
    return param > 0 && param % 10 == 0;
  case kRemoteHealCommand:
    return param > 0 && param <= 100;
  default:
    return false;
  }
}

} // namespace RM::ExchangeCommandPolicy
