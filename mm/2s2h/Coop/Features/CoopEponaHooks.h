#pragma once

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

s32 Coop_EponaIsProxy(const Actor* actor);
s32 Coop_EponaIsProxyRideable(const Actor* actor);
s32 Coop_EponaProxyHasLocalPassenger(const Actor* actor);
// 1 = this horse may put our Link on it: he rides (or is getting on) no other one (common/EponaRules.h)
s32 Coop_EponaMayMount(PlayState* play, const Actor* horse);
void Coop_EponaBeforeProxyUpdate(struct EnHorse* horse, PlayState* play);
void Coop_EponaAfterProxyUpdate(struct EnHorse* horse, PlayState* play);
void Coop_EponaSongPlayed(PlayState* play);

#ifdef __cplusplus
}
#endif
