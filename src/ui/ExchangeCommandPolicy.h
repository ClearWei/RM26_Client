#ifndef RM_EXCHANGECOMMANDPOLICY_H
#define RM_EXCHANGECOMMANDPOLICY_H

namespace RM::ExchangeCommandPolicy {

constexpr int kExchange17mmCommand = 1;
constexpr int kExchange42mmCommand = 2;
constexpr int kRemoteHealCommand = 6;

bool canOpenRemoteExchange(bool canRemoteHeal, bool canRemoteAmmo);
bool isValidRequest(int commandType, int param);

} // namespace RM::ExchangeCommandPolicy

#endif // RM_EXCHANGECOMMANDPOLICY_H
