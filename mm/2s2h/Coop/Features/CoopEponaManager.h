#pragma once

#include "z64.h"
#include "common/EponaState.h"

#include <cstdint>

struct Actor;
struct EnHorse;
struct PlayState;

namespace coop::client {

struct HorseProxyState {
    EponaKey key;
    EnHorse* actor = nullptr;
    EponaState state;
    bool remote = true;
    bool pendingDestroy = false;
    bool localPassenger = false;
};

void CoopEpona_FrameStart(PlayState* play);
void CoopEpona_FrameEnd(PlayState* play);
void CoopEpona_OnPlayDestroy();
void CoopEpona_OnActorDestroy(Actor* actor);
void CoopEpona_OnSongPlayed(PlayState* play);

HorseProxyState* CoopEpona_ProxyState(Actor* actor);
bool CoopEpona_IsProxy(const Actor* actor);
bool CoopEpona_IsProxyRideable(const Actor* actor);
bool CoopEpona_ProxyHasLocalPassenger(const Actor* actor);
void CoopEpona_ApplyProxy(EnHorse* horse, PlayState* play);
void CoopEpona_BeforeProxyUpdate(EnHorse* horse, PlayState* play);
void CoopEpona_AfterProxyUpdate(EnHorse* horse, PlayState* play);

} // namespace coop::client

#ifdef __cplusplus
extern "C" {
#endif

void Coop_EponaSongPlayed(PlayState* play);
void Coop_EponaFrameStart(PlayState* play);
void Coop_EponaFrameEnd(PlayState* play);
void Coop_EponaOnPlayDestroy();
void Coop_EponaOnActorDestroy(Actor* actor);
s32 Coop_EponaIsProxy(const Actor* actor);
s32 Coop_EponaIsProxyRideable(const Actor* actor);
s32 Coop_EponaProxyHasLocalPassenger(const Actor* actor);
void Coop_EponaBeforeProxyUpdate(struct EnHorse* horse, PlayState* play);
void Coop_EponaAfterProxyUpdate(struct EnHorse* horse, PlayState* play);

#ifdef __cplusplus
}
#endif
