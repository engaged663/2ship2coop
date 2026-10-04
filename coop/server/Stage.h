#pragma once
// [COOP] Sincronización total §5.2: a scene and its layer (gSaveContext.sceneLayer). Two players in the same scene with
// different layers (a minigame's special entrance) have different actors: whatever replicates actors or flags is only
// shared within the same stage. Seeing each other (poses), chat, groups, dialogues and cutscenes stay per scene.
#include "server/PlayerRegistry.h"

#include <cstdint>

namespace coop::server {

// One number per stage: the scene itself for layer 0 (scene ids are below 0x100, layers at most 16).
inline int16_t StageOf(int16_t scene, uint8_t layer) {
    return scene < 0 ? (int16_t)-1 : (int16_t)(scene + (layer & 0x7F) * 0x100);
}

inline int16_t StageOf(const RemoteClient& c) {
    return StageOf(c.scene, c.layer);
}

inline bool SameStage(const RemoteClient& a, const RemoteClient& b) {
    return a.scene >= 0 && a.scene == b.scene && a.layer == b.layer;
}

} // namespace coop::server
