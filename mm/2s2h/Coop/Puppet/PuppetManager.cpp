#include "PuppetManager.h"

#include "PuppetActor.h"

#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/Session.h"

#include "common/Protocol.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <deque>
#include <map>

extern "C" {
#include "functions.h"
#include "variables.h"
}

namespace {

using Clock = std::chrono::steady_clock;

constexpr size_t kTargetBuffered = 2; // play a pose once two are queued (absorbs network jitter)
constexpr size_t kMaxBuffered = 5;    // beyond this we are late: skip ahead
constexpr size_t kReceiveCap = 8;
constexpr auto kStaleAfter = std::chrono::milliseconds(2000);

struct RemoteBody {
    std::deque<coop::PlayerState> buffer;
    coop::PlayerState current;
    bool hasCurrent = false;
    Actor* actor = nullptr;
    uint8_t actorForm = 0xFF;
    Clock::time_point lastReceived;
    const char* lastAbsentReason = nullptr; // diagnostics
};

std::map<uint8_t, RemoteBody> sBodies;

void KillActor(RemoteBody& body) {
    if (body.actor != nullptr) {
        SPDLOG_INFO("[Coop] Removing puppet of player {}", (int)COOP_PUPPET_GET_PLAYER_ID(body.actor));
        Actor_Kill(body.actor);
        body.actor = nullptr;
    }
}

void OnPose(const uint8_t* data, size_t size) {
    coop::PlayerState state;
    if (!coop::DecodePlayerState(data, size, state) || state.playerId == 0 ||
        state.playerId == coop::client::Session_LocalId()) {
        return;
    }
    RemoteBody& body = sBodies[state.playerId];
    if (!body.hasCurrent && body.buffer.empty()) {
        SPDLOG_INFO("[Coop] Receiving pose of player {} (scene {})", (int)state.playerId, (int)state.sceneId);
    }
    body.buffer.push_back(state);
    while (body.buffer.size() > kReceiveCap) {
        body.buffer.pop_front();
    }
    body.lastReceived = Clock::now();
}

void OnLeave(const coop::json& ev) {
    PuppetManager_RemovePlayer((uint8_t)coop::GetInt(ev, "id"));
}

void OnLost(const std::string& reason) {
    PuppetManager_Clear();
}

} // namespace

void PuppetManager_Update(PlayState* play) {
    if (gSaveContext.gameMode != GAMEMODE_NORMAL) {
        return;
    }
    Clock::time_point now = Clock::now();
    for (auto& [id, body] : sBodies) {
        if (body.buffer.size() > kMaxBuffered) {
            while (body.buffer.size() > kTargetBuffered + 1) {
                body.buffer.pop_front();
            }
        }
        if (!body.buffer.empty() && (body.buffer.size() >= kTargetBuffered || !body.hasCurrent)) {
            body.current = body.buffer.front();
            body.buffer.pop_front();
            body.hasCurrent = true;
        }

        const char* absent = !body.hasCurrent                                    ? "no pose yet"
                             : now - body.lastReceived >= kStaleAfter             ? "no recent pose"
                             : body.current.sceneId != play->sceneId              ? "other scene"
                             : coop::client::Session_FindPlayer(id) == nullptr ? "unknown player"
                                                                                  : nullptr;
        if (absent != body.lastAbsentReason) {
            body.lastAbsentReason = absent;
            if (absent != nullptr) {
                SPDLOG_INFO("[Coop] Player {} has no puppet here: {}", (int)id, absent);
            }
        }
        if (absent != nullptr) {
            KillActor(body);
            continue;
        }
        if (body.actor != nullptr && body.actorForm != body.current.form) {
            KillActor(body); // a new form needs a new skeleton: respawn below next frame
            continue;
        }
        if (body.actor == nullptr) {
            const coop::PlayerState& s = body.current;
            body.actor = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_COOP_PUPPET, s.pos[0], s.pos[1], s.pos[2], 0,
                                     s.rot.y, 0, COOP_PUPPET_PARAMS(id, s.form));
            body.actorForm = s.form;
            SPDLOG_INFO("[Coop] Spawned puppet of player {} (form {}) at ({:.0f}, {:.0f}, {:.0f}): {}", (int)id,
                        (int)s.form, s.pos[0], s.pos[1], s.pos[2], body.actor != nullptr ? "ok" : "FAILED");
        }
    }
}

bool PuppetManager_GetCurrent(uint8_t playerId, coop::PlayerState& out) {
    auto it = sBodies.find(playerId);
    if (it == sBodies.end() || !it->second.hasCurrent) {
        return false;
    }
    out = it->second.current;
    return true;
}

void PuppetManager_OnPuppetDestroyed(uint8_t playerId, Actor* actor) {
    auto it = sBodies.find(playerId);
    if (it != sBodies.end() && it->second.actor == actor) {
        it->second.actor = nullptr;
    }
}

void PuppetManager_OnSceneEnd() {
    for (auto& [id, body] : sBodies) {
        body.actor = nullptr;
    }
}

void PuppetManager_RemovePlayer(uint8_t playerId) {
    auto it = sBodies.find(playerId);
    if (it != sBodies.end()) {
        KillActor(it->second);
        sBodies.erase(it);
    }
}

void PuppetManager_Clear() {
    for (auto& [id, body] : sBodies) {
        KillActor(body);
    }
    sBodies.clear();
}

COOP_ON_STREAM(puppetPose, coop::kStreamPlayerState, OnPose);
COOP_ON_EVENT(puppetLeave, coop::ev::kLeave, OnLeave);
COOP_ON_LOST(puppetLost, OnLost);

static void RegisterPuppetManager() {
    // Runs right after the local Link updates: a safe point to spawn/kill actors every frame.
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnActorUpdate>(
        ACTOR_PLAYER, [](Actor* actor) { PuppetManager_Update(gPlayState); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDestroy>(PuppetManager_OnSceneEnd);
}

static RegisterShipInitFunc sPuppetManagerInit(RegisterPuppetManager);
