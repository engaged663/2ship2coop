// [COOP] A mate runs an activity here (spec §3.3): we take part. With its first "act_hud" in our scene:
//   Together  our Link stands next to it (the table's slots) with the same minigame equipment (the bow on B, its
//             bombs...), the HUD shows the director's points, time, countdown and "perfect", and our shots hit the
//             targets its game simulates (C1/D3 send them there): the points are the group's
//   Turns     a small window shows its time and points; the rest is the original game (our round comes after)
//   EachOwn   our game runs its own copy (a race we followed it into): nothing to show
// Everything we changed goes back when it ends ("act end", no "act_hud" for 3 s, another scene, out of the group).
// Activity rooms (spec 2026-10-04-coop-salas-actividades §5.4): the running round of our room's director counts us in
// wherever we are; a scene change no longer leaves it (what belongs to the scene is undone and comes back when we are
// in the director's scene again), and away from it the small window shows its time and points.
#include "Activities.h"

#include "2s2h/Coop/Actors/ActorRegistry.h"
#include "2s2h/Coop/Chat/ChatModel.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Group/Group.h"
#include "2s2h/Coop/Room/Room.h"
#include "2s2h/Coop/World/FieldTable.h"
#include "2s2h/Coop/World/WorldSession.h"

#include "common/Protocol.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <libultraship/bridge/consolevariablebridge.h>
#include <libultraship/libultraship.h>
#include <ship/window/gui/GuiWindow.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include "functions.h"
#include "sequence.h"
#include "variables.h"
}

namespace coop::client {

namespace {

constexpr int64_t kLostMs = 3000;          // no "act_hud" for this long: the director is gone or done
constexpr float kJoinDist = 3000.f;        // a mate running a minigame this close counts us in
constexpr float kPlaceMaxDist = 1500.f;    // farther than this from our slot: we stay where we are
constexpr int64_t kPendingJoinMs = 30000;  // a "follow" with a key counts us in this long before its HUD arrives
constexpr int64_t kRejoinQuietMs = 30000;  // joining the same activity again this soon (after a scene) says nothing
constexpr int64_t kTurnAgainMs = 600000;   // played it in the last 10 minutes: no "your turn"

struct Hud {
    int status = 0, score = 0, hidden = 0, mstate = 0, perfect = 0, ammo = 0, b = 0, bomb = 0, chu = 0, sub = -1;
    bool arrows = false, frozen = false;
    Vec3f pos = {};
    s16 rot = 0;
    bool hasTimer = false, down = true, run = false;
    int timerId = 0;
    int64_t limit = 0, elapsed = 0;
    int64_t atMs = 0;
};

uint8_t sDirector = 0;
const ActivityDef* sDef = nullptr;
bool sViaRoom = false; // counted in by our room's running round (wherever we are)
std::string sDirectorNick;
int16_t sScene = -1;
Hud sHud;
bool sHaveHud = false;
// what we changed, to undo it
bool sPlaced = false, sStatusSet = false, sFroze = false, sArrows = false, sExplosives = false, sBAmmo = false;
int sTimerShown = -1;
int sLastB = 0, sLastBomb = 0, sLastChu = 0, sLastAmmo = 0;
int sPerfectShown = 0;
int sPrevMState = 0;
int sSubPlayed = -1;
// counted in by a "follow" before the HUD arrives
uint8_t sPendingFrom = 0;
std::string sPendingKey;
int64_t sPendingUntilMs = 0;
// the last activity we took part in (a scene change in the middle rejoins it quietly)
std::string sLastKey;
int64_t sLastLeftMs = 0;
// Cada uno: the activity we followed a mate into (our own run), until we leave it
std::string sOwnRunKey;
bool sPrevStarting = false;
// Por turnos: (mate, activity) -> when its last round ended
std::map<std::pair<uint8_t, std::string>, int64_t> sRounds;

Player* Link(PlayState* play) {
    return (Player*)play->actorCtx.actorLists[ACTORCAT_PLAYER].first;
}

int64_t Clamp(const json& ev, const char* key, int64_t def, int64_t lo, int64_t hi) {
    return std::clamp<int64_t>(GetInt(ev, key, def), lo, hi);
}

Hud ReadHud(const json& ev) {
    Hud h;
    h.status = (int)Clamp(ev, "status", 0, 0, 3);
    h.score = (int)Clamp(ev, "score", 0, 0, 0xFFFF);
    h.hidden = (int)Clamp(ev, "hidden", 0, 0, 0xFFFF);
    h.mstate = (int)Clamp(ev, "mstate", 0, 0, 255);
    h.perfect = (int)Clamp(ev, "perfect", 0, 0, 16);
    h.ammo = (int)Clamp(ev, "ammo", 0, 0, 0xFFFF);
    h.b = (int)Clamp(ev, "b", 0, -128, 127);
    h.bomb = (int)Clamp(ev, "bomb", 0, -128, 127);
    h.chu = (int)Clamp(ev, "chu", 0, -128, 127);
    h.sub = (int)Clamp(ev, "sub", -1, -1, 0xFFFF);
    h.arrows = GetBool(ev, "arrows");
    h.frozen = GetBool(ev, "frozen");
    h.rot = (s16)GetInt(ev, "rot");
    auto pos = ev.find("pos");
    if (pos != ev.end() && pos->is_array() && pos->size() == 3 && (*pos)[0].is_number() && (*pos)[1].is_number() &&
        (*pos)[2].is_number()) {
        h.pos = { (f32)(*pos)[0].get<double>(), (f32)(*pos)[1].get<double>(), (f32)(*pos)[2].get<double>() };
        if (!std::isfinite(h.pos.x) || !std::isfinite(h.pos.y) || !std::isfinite(h.pos.z)) {
            h.pos = {};
        }
    }
    auto timer = ev.find("timer");
    if (timer != ev.end() && timer->is_object()) {
        int id = (int)GetInt(*timer, "id", -1);
        // only the minigames' timers: never the moon's or the one of hot rooms and water
        if (id == TIMER_ID_POSTMAN || id == TIMER_ID_MINIGAME_1 || id == TIMER_ID_MINIGAME_2) {
            h.hasTimer = true;
            h.timerId = id;
            h.down = GetBool(*timer, "down", true);
            h.run = GetBool(*timer, "run");
            h.limit = Clamp(*timer, "limit", 0, 0, 36000000);
            h.elapsed = Clamp(*timer, "elapsed", 0, 0, 36000000);
        }
    }
    h.atMs = Group_NowMs();
    return h;
}

// ---- Undoing ----

// play == nullptr: the scene is going away and takes its own state along; what lives in the save (the timer, the
// minigame's status) is still put back.
void Undo(PlayState* play) {
    if (sTimerShown >= 0) {
        gSaveContext.timerStates[sTimerShown] = TIMER_STATE_OFF;
    }
    if (sStatusSet && gSaveContext.minigameStatus == MINIGAME_STATUS_ACTIVE) {
        gSaveContext.minigameStatus = MINIGAME_STATUS_END; // as the NPCs leave it: the HUD puts things back
    }
    if (play != nullptr) {
        Player* link = Link(play);
        if (sBAmmo && play->bButtonAmmoPlusOne > 0) {
            play->bButtonAmmoPlusOne = -10; // the minigame's bow is put away, as the NPCs do at the end
        }
        if (sExplosives) {
            play->unk_1887D = 0;
            play->unk_1887E = 0;
        }
        if (sArrows) {
            link->stateFlags3 &= ~PLAYER_STATE3_400;
        }
        if (sFroze) {
            link->stateFlags1 &= ~PLAYER_STATE1_20;
        }
        if (sSubPlayed >= 0) {
            Audio_StopSubBgm();
        }
    }
    sPlaced = sStatusSet = sFroze = sArrows = sExplosives = sBAmmo = false;
    sTimerShown = -1;
    sLastB = sLastBomb = sLastChu = sLastAmmo = 0;
    sPerfectShown = 0;
    sPrevMState = 0;
    sSubPlayed = -1;
}

void Leave(const char* why) {
    if (sDef == nullptr) {
        return;
    }
    Undo(gPlayState);
    SPDLOG_INFO("[Coop] We leave {} ({})", sDef->key, why);
    sLastKey = sDef->key;
    sLastLeftMs = Group_NowMs();
    sDef = nullptr;
    sDirector = 0;
    sHaveHud = false;
    sViaRoom = false;
}

// The director plays in our scene (a room's round counts us in from anywhere; its place and equipment are only for
// the ones who are with it).
bool DirectorHere(PlayState* play) {
    if (!sViaRoom) {
        return true; // (a group's minigame: we leave it when we change scene)
    }
    const RemotePlayer* p = Session_FindPlayer(sDirector);
    return play != nullptr && p != nullptr && p->scene == play->sceneId;
}

void Join(uint8_t from, const ActivityDef* def, PlayState* play) {
    Undo(play);
    sDirector = from;
    sDef = def;
    sScene = play->sceneId;
    const RemotePlayer* p = Session_FindPlayer(from);
    sDirectorNick = p != nullptr ? p->nick : std::string("?");
    sPendingFrom = 0;
    SPDLOG_INFO("[Coop] We join {} of player {}", def->key, (int)from);
    if (sLastKey == def->key && Group_NowMs() - sLastLeftMs < kRejoinQuietMs) {
        return;
    }
    std::string name = def->name;
    switch (def->mode) {
        case ActivityMode::Together:
            Chat_Add(ChatKind::Ok, "Juegas " + name + " con " + sDirectorNick + ": los puntos son del grupo.");
            break;
        case ActivityMode::EachOwn:
            Chat_Add(ChatKind::Ok, "Juegas " + name + " a la vez que " + sDirectorNick + ".");
            break;
        case ActivityMode::Turns:
            Chat_Add(ChatKind::Ok, "Miras la ronda de " + sDirectorNick + " en " + name + ".");
            break;
        case ActivityMode::Shared:
            Chat_Add(ChatKind::Ok, "Jugáis " + name + " con " + sDirectorNick + ": el progreso es de todos.");
            break;
    }
}

// ---- Together ----

// Our place among the guests: the group's order without the director.
int GuestOrder() {
    int order = 0;
    for (const GroupMember& m : Group_Members()) {
        if (m.id == sDirector) {
            continue;
        }
        if (m.id == Session_LocalId()) {
            break;
        }
        order++;
    }
    return std::min(order, 2);
}

void Place(PlayState* play) {
    const GuestSlot& s = sDef->slots[GuestOrder()];
    if (s.right == 0.f && s.forward == 0.f) {
        return;
    }
    Player* link = Link(play);
    f32 sinY = Math_SinS(sHud.rot);
    f32 cosY = Math_CosS(sHud.rot);
    Vec3f pos = sHud.pos;
    pos.x += s.forward * sinY - s.right * cosY; // forward (sin, cos); right (-cos, sin)
    pos.z += s.forward * cosY + s.right * sinY;
    if (Math_Vec3f_DistXYZ(&link->actor.world.pos, &pos) > kPlaceMaxDist) {
        return;
    }
    link->actor.world.pos = pos;
    link->actor.prevPos = pos;
    link->actor.shape.rot.y = sHud.rot;
    link->actor.world.rot.y = sHud.rot;
    link->yaw = sHud.rot;
}

// The director's timer, shown stopped at its value (it never ends anything in our game).
void ShowTimer() {
    if (!sHud.hasTimer) {
        if (sTimerShown >= 0) {
            gSaveContext.timerStates[sTimerShown] = TIMER_STATE_OFF;
            sTimerShown = -1;
        }
        return;
    }
    int id = sHud.timerId;
    if (sTimerShown >= 0 && sTimerShown != id) {
        gSaveContext.timerStates[sTimerShown] = TIMER_STATE_OFF;
    }
    sTimerShown = id;
    int64_t elapsed = sHud.elapsed;
    if (sHud.run) {
        elapsed += std::min<int64_t>(Group_NowMs() - sHud.atMs, 1000) / 10; // centiseconds since it was sent
    }
    elapsed = std::clamp<int64_t>(elapsed, 0, sHud.down ? sHud.limit : 36000000);
    gSaveContext.timerDirections[id] = sHud.down ? TIMER_COUNT_DOWN : TIMER_COUNT_UP;
    gSaveContext.timerTimeLimits[id] = (u64)sHud.limit;
    gSaveContext.timerStopTimes[id] = (u64)elapsed;
    gSaveContext.timerCurTimes[id] = (u64)(sHud.down ? sHud.limit - elapsed : elapsed);
    gSaveContext.timerStates[id] = TIMER_STATE_7;
    gSaveContext.timerX[id] = 26;
    gSaveContext.timerY[id] = gSaveContext.save.saveInfo.playerData.healthCapacity > 0xA0 ? 54 : 46;
}

// A value of the director's equipment: ours follows it when it grows (a new round) or ends; we spend ours.
template <class T> void Mirror(T& ours, int theirs, int& last, bool& touched) {
    if (theirs == last) {
        return;
    }
    if (theirs <= 0 || theirs > last || ours == 0) {
        ours = (T)theirs;
        touched = true;
    }
    last = theirs;
}

void ApplyTogether(PlayState* play) {
    Player* link = Link(play);
    if (!sPlaced && sHud.status == MINIGAME_STATUS_ACTIVE) {
        Place(play);
        sPlaced = true;
    }
    if (sHud.status == MINIGAME_STATUS_ACTIVE) {
        if (gSaveContext.minigameStatus != MINIGAME_STATUS_ACTIVE) {
            Interface_InitMinigame(play);
        }
        sStatusSet = true;
        // our copies of the targets never add points: the HUD shows the director's
        gSaveContext.minigameScore = (u16)sHud.score;
        gSaveContext.minigameHiddenScore = (u16)sHud.hidden;
        play->interfaceCtx.minigamePoints = 0;
        play->interfaceCtx.minigameHiddenPoints = 0;
    }
    if (sDef->loadout) {
        Mirror(play->bButtonAmmoPlusOne, sHud.b, sLastB, sBAmmo);
        Mirror(play->unk_1887E, sHud.bomb, sLastBomb, sExplosives);
        Mirror(play->unk_1887D, sHud.chu, sLastChu, sExplosives);
        bool ammoTouched = false;
        Mirror(play->interfaceCtx.minigameAmmo, sHud.ammo, sLastAmmo, ammoTouched);
        if (sHud.arrows) {
            link->stateFlags3 |= PLAYER_STATE3_400;
            sArrows = true;
        } else if (sArrows) {
            link->stateFlags3 &= ~PLAYER_STATE3_400;
            sArrows = false;
        }
    }
    if (sHud.frozen) {
        link->stateFlags1 |= PLAYER_STATE1_20;
        sFroze = true;
    } else if (sFroze) {
        link->stateFlags1 &= ~PLAYER_STATE1_20;
        sFroze = false;
    }
    if (sHud.mstate == MINIGAME_STATE_COUNTDOWN_SETUP_3 && sPrevMState != MINIGAME_STATE_COUNTDOWN_SETUP_3 &&
        play->interfaceCtx.minigameState == MINIGAME_STATE_NONE) {
        play->interfaceCtx.minigameState = MINIGAME_STATE_COUNTDOWN_SETUP_3; // 3, 2, 1, go
    }
    sPrevMState = sHud.mstate;
    if (sHud.perfect != 0 && sPerfectShown == 0) {
        Interface_SetPerfectLetters(play, (s16)sHud.perfect);
    }
    sPerfectShown = sHud.perfect;
    ShowTimer();
    int sub = AudioSeq_GetActiveSeqId(SEQ_PLAYER_BGM_SUB);
    if (sHud.sub >= 0 && sHud.sub != sSubPlayed && (sub & 0x7FFF) != (sHud.sub & 0x7FFF)) {
        Audio_PlaySubBgm((u16)sHud.sub); // the minigame's music
        sSubPlayed = sHud.sub;
    } else if (sHud.sub < 0 && sSubPlayed >= 0) {
        Audio_StopSubBgm();
        sSubPlayed = -1;
    }
}

// ---- Turns: a small window with the director's time and points ----

class GuestHudWindow : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;
    void InitElement() override {
    }
    void DrawElement() override {
    }
    void UpdateElement() override {
    }
    void Draw() override;
};

std::shared_ptr<GuestHudWindow> sHudWindow;

void GuestHudWindow::Draw() {
    if (sDef == nullptr || !sHaveHud) {
        return;
    }
    // Por turnos: always; any other mode: only away from the director, if there is a time or points to show
    bool away = sViaRoom && !DirectorHere(gPlayState) && (sHud.hasTimer || sHud.score > 0);
    if (sDef->mode != ActivityMode::Turns && !away) {
        return;
    }
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x - 16.0f, vp->Pos.y + 16.0f), ImGuiCond_Always,
                            ImVec2(1.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.5f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoInputs |
                             ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize;
    if (ImGui::Begin("##CoopGuestHud", nullptr, flags)) {
        ImGui::SetWindowFontScale(CVarGetFloat("gCoop.Chat.Scale", 1.4f));
        ImGui::Text("%s - %s", sDef->name, sDirectorNick.c_str());
        if (sHud.hasTimer) {
            int64_t elapsed = sHud.elapsed + (sHud.run ? std::min<int64_t>(Group_NowMs() - sHud.atMs, 1000) / 10 : 0);
            int64_t shown = sHud.down ? std::max<int64_t>(0, sHud.limit - elapsed) : elapsed;
            ImGui::Text("Tiempo: %s", Activity_TimeText(shown).c_str());
        }
        if (sHud.score > 0) {
            ImGui::Text("Puntos: %d", sHud.score);
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

// ---- Events and frames ----

void OnActHud(const json& ev) {
    PlayState* play = gPlayState;
    uint8_t from = (uint8_t)GetInt(ev, "from");
    if (play == nullptr || !WorldSession_Active() || !Group_OptMinigames() || Director_Running() != nullptr) {
        return;
    }
    const ActivityDef* def = Activity_ByKey(GetString(ev, "key"));
    if (def == nullptr || def->kind != ActivityKind::Minigame) {
        return;
    }
    // The running round of our room, from its director: it counts us in wherever we are
    const RoomInfo& room = Room_Get();
    bool viaRoom = Room_IsMate(from) && room.phase == RoomPhase::Running && room.director == from &&
                   room.key == def->key;
    if (!viaRoom && !Group_IsMate(from)) {
        return;
    }
    if (sDef != nullptr && from != sDirector) {
        return; // one director at a time
    }
    if (sDef == nullptr) {
        bool pending = sPendingFrom == from && sPendingKey == def->key && Group_NowMs() < sPendingUntilMs;
        if (!viaRoom && !pending && !Group_MateNear(from, kJoinDist)) {
            return;
        }
        Join(from, def, play);
        sViaRoom = viaRoom;
    }
    sHud = ReadHud(ev);
    sHaveHud = true;
}

// Played that activity lately: us (Director.cpp knows) or a mate (we heard its round end).
bool PlayedLately(uint8_t id, const ActivityDef& def) {
    int64_t last = 0;
    if (id == Session_LocalId()) {
        last = Director_LastOwnRunMs(def.key);
    } else if (auto it = sRounds.find({ id, def.key }); it != sRounds.end()) {
        last = it->second;
    }
    return last != 0 && Group_NowMs() - last < kTurnAgainMs;
}

// Who plays after `from`: the first member after it (its room's order, or the group's, around) who has not played
// lately and is in that scene (scene -1: anywhere) (0: nobody left).
uint8_t NextInTurn(uint8_t from, const ActivityDef& def, int16_t scene) {
    std::vector<uint8_t> members;
    if (Room_Has() && Room_Get().key == def.key) {
        for (const RoomMemberInfo& m : Room_Get().members) {
            members.push_back(m.id);
        }
    } else {
        for (const GroupMember& m : Group_Members()) {
            members.push_back(m.id);
        }
    }
    size_t n = members.size();
    size_t start = 0;
    for (size_t i = 0; i < n; i++) {
        if (members[i] == from) {
            start = i;
            break;
        }
    }
    for (size_t k = 1; k < n; k++) {
        uint8_t id = members[(start + k) % n];
        bool here = id == Session_LocalId() || scene < 0;
        if (!here) {
            const RemotePlayer* p = Session_FindPlayer(id);
            here = p != nullptr && p->scene == scene;
        }
        if (here && !PlayedLately(id, def)) {
            return id;
        }
    }
    return 0;
}

// A mate finished its round of a Por turnos minigame: its result, and our turn if we are the next one there.
void OnTurnEnded(uint8_t from, const std::string& nick, const ActivityDef& def, int64_t score, int64_t cs) {
    std::string result;
    if (score > 0) {
        result = std::to_string(score) + " puntos";
    }
    if (cs > 0) {
        result += (result.empty() ? "" : ", ") + Activity_TimeText(cs);
    }
    Chat_Add(ChatKind::Info, nick + " termina " + def.name + (result.empty() ? "." : ": " + result + "."));
    sRounds[{ from, def.key }] = Group_NowMs();
    PlayState* play = gPlayState;
    const RemotePlayer* p = Session_FindPlayer(from);
    // A room's round: its members take turns wherever they are; a group's: the ones in its scene
    bool roomRound = Room_IsMate(from) && Room_Get().key == def.key;
    if (play == nullptr || p == nullptr || (!roomRound && p->scene != play->sceneId) || !Group_OptMinigames() ||
        CVarGetInteger("gCoop.Group.Turns", 1) == 0) {
        return;
    }
    if (NextInTurn(from, def, roomRound ? -1 : play->sceneId) == Session_LocalId()) {
        Chat_Add(ChatKind::Ok,
                 std::string("Te toca: ") + def.name + ". Habla con quien lo lleva para jugar tu ronda.");
    }
}

void OnAct(const json& ev) {
    uint8_t from = (uint8_t)GetInt(ev, "from");
    if (!Mates_Is(from)) {
        return;
    }
    std::string state = GetString(ev, "state");
    std::string nick = GetString(ev, "nick");
    const ActivityDef* def = Activity_ByKey(GetString(ev, "key"));
    std::string name = GetString(ev, "name");
    if (name.empty()) {
        name = def != nullptr ? def->name : GetString(ev, "key");
    }
    if (state == "end") {
        if (from == sDirector) {
            Leave("its end");
        }
        if (def != nullptr && def->kind == ActivityKind::Minigame && def->mode == ActivityMode::Turns) {
            OnTurnEnded(from, nick, *def, GetInt(ev, "score"), GetInt(ev, "cs"));
        }
    } else if (state == "start") {
        Chat_Add(ChatKind::Info, nick + " empieza " + name + ".");
    } else if (state == "result") {
        int64_t cs = GetInt(ev, "cs");
        Chat_Add(ChatKind::Info, nick + (GetBool(ev, "won") ? " gana " : " no gana ") + name +
                                     (cs > 0 ? " (" + Activity_TimeText(cs) + ")." : "."));
    }
}

// Cada uno: our own run of an activity we followed a mate into ends when we leave it (not into its entrance).
void OwnRunFrame(PlayState* play) {
    bool starting = play->transitionTrigger == TRANS_TRIGGER_START;
    if (starting && !sPrevStarting && !sOwnRunKey.empty()) {
        const ActivityDef* def = Activity_ByKey(sOwnRunKey);
        if (def == nullptr) {
            sOwnRunKey.clear();
        } else if (!Activity_IsSpecialEntrance(*def, play->nextEntrance)) {
            // only out of the race itself (a trip we never made says nothing)
            if (play->sceneId == def->scene && Activity_IsSpecialEntrance(*def, gSaveContext.save.entrance)) {
                Director_ReportResult(*def);
            }
            sOwnRunKey.clear();
        }
    }
    sPrevStarting = starting;
}

void FrameEnd() {
    PlayState* play = gPlayState;
    if (play == nullptr) {
        return; // between scenes: wait for the next one
    }
    OwnRunFrame(play);
    if (sDef == nullptr) {
        return;
    }
    bool lost = Group_NowMs() - sHud.atMs > kLostMs;
    if (sViaRoom) {
        // Our room's round: until it is over or we are out of the room, wherever we go
        const RoomInfo& room = Room_Get();
        bool still = room.director == sDirector && room.key == sDef->key && room.phase == RoomPhase::Running;
        if (!WorldSession_Active() || !still || !Group_OptMinigames() || lost) {
            Leave("gone");
            return;
        }
    } else if (!WorldSession_Active() || play->sceneId != sScene || !Group_IsMate(sDirector) ||
               !Group_OptMinigames() || lost) {
        Leave("gone");
        return;
    }
    if (!sHaveHud) {
        return;
    }
    if (sDef->mode == ActivityMode::Together && DirectorHere(play)) {
        ApplyTogether(play);
    }
}

void Reset() {
    sDef = nullptr;
    sDirector = 0;
    sHaveHud = false;
    sViaRoom = false;
    sPendingFrom = 0;
    sOwnRunKey.clear();
    sPrevStarting = false;
    sRounds.clear();
    Undo(nullptr);
}

void RegisterGuest() {
    COND_HOOK(OnGameStateMainFinish, true, FrameEnd);
    COND_HOOK(OnPlayDestroy, true, []() {
        if (sDef != nullptr) {
            Undo(nullptr); // the scene took its state along; a new one may rejoin
            if (!sViaRoom) {
                sDef = nullptr; // (our room's round goes on: it applies again in the director's scene)
                sDirector = 0;
                sHaveHud = false;
            }
        }
    });
    if (sHudWindow == nullptr) {
        auto gui = Ship::Context::GetRawInstance()->GetWindow()->GetGui();
        sHudWindow = std::make_shared<GuestHudWindow>("gCoop.Group.GuestHud", "Co-op Minijuego del grupo");
        gui->AddGuiWindow(sHudWindow);
        sHudWindow->Show();
    }
}

} // namespace

bool Guest_Joined() {
    return sDef != nullptr || (sPendingFrom != 0 && Group_NowMs() < sPendingUntilMs);
}

uint8_t Guest_Director() {
    return sDef != nullptr ? sDirector : 0;
}

const ActivityDef* Guest_Activity() {
    return sDef;
}

bool Guest_SkipsLease(int16_t actorId) {
    const ActivityDef* def = sDef;
    if (def == nullptr && sPendingFrom != 0 && Group_NowMs() < sPendingUntilMs) {
        def = Activity_ByKey(sPendingKey);
    }
    // (Cada uno: each game runs its own race; whoever finished talks to the NPC for its prize)
    return def != nullptr && def->mode != ActivityMode::EachOwn &&
           (def->npcs[0] == actorId || Activity_Prop(*def, actorId) != nullptr);
}

bool Guest_ContactTarget(const TrackedActor& t) {
    if (sDef == nullptr || sDef->mode != ActivityMode::Together || !sHaveHud || t.actor == nullptr) {
        return false;
    }
    const ActivityProp* prop = Activity_Prop(*sDef, t.actor->id);
    if (prop != nullptr && !prop->ours) {
        return true; // balloons, rupees, rings
    }
    // The NPC's children (Honey and Darling's targets and baskets); never the NPC itself
    const TrackedActor* root = t.rootKey == t.key ? &t : ActorRegistry_Find(t.rootKey);
    return root != nullptr && root != &t && root->actor != nullptr && root->actor->id == sDef->npcs[0];
}

void Guest_OnFollow(uint8_t from, const std::string& key) {
    const ActivityDef* def = key.empty() ? nullptr : Activity_ByKey(key);
    if (def == nullptr) {
        return;
    }
    sPendingFrom = from;
    sPendingKey = key;
    sPendingUntilMs = Group_NowMs() + kPendingJoinMs;
    if (def->mode == ActivityMode::EachOwn) {
        sOwnRunKey = key; // our own run of it starts with this trip
    }
}

} // namespace coop::client

COOP_ON_EVENT(guestHud, coop::ev::kActHud, coop::client::OnActHud);
COOP_ON_EVENT(guestAct, coop::ev::kAct, coop::client::OnAct);
COOP_ON_EVENT(guestWelcome, coop::ev::kWelcome, [](const coop::json&) { coop::client::Reset(); });
COOP_ON_LOST(guestLost, [](const std::string&) { coop::client::Reset(); });
static RegisterShipInitFunc sGuestInit(coop::client::RegisterGuest);
