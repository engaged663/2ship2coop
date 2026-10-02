// [COOP] "mod": the orders of the server's mods for this game (coop/docs/mods/API.md, game.*; the server builds them
// in coop/server/Mods/Api/ApiGame.cpp). They run only while this game plays in the server's world (never on the
// player's own saves) and with gCoop.Mods on, and each one checks its fields again. An order waits in a queue until
// the game can run it (Needs), 10 s at most; a message that never found its moment goes to the chat.
// New order: a function below, one line in kOps, and its game.* function on the server.
#include "Mods.h"

#include "2s2h/Coop/Activities/Activities.h"
#include "2s2h/Coop/Actors/ActorRegistry.h"
#include "2s2h/Coop/Chat/ChatModel.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Features/Ending.h"
#include "2s2h/Coop/Features/Warp.h"
#include "2s2h/Coop/Host/HostMode.h"
#include "2s2h/Coop/Puppet/PoseCapture.h"
#include "2s2h/Coop/World/WorldSession.h"

#include "common/ModRules.h"
#include "common/PlayerState.h"
#include "common/Protocol.h"
#include "common/Text.h"

#include "2s2h/BenGui/Notification.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <libultraship/bridge/consolevariablebridge.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <deque>
#include <set>
#include <string>

extern "C" {
#include "functions.h"
#include "variables.h"
extern SceneEntranceTableEntry sSceneEntranceTable[]; // z_scene_table.c
}

namespace coop::client {

namespace {

constexpr size_t kMaxQueued = 64;     // orders waiting for their moment
constexpr int64_t kMaxWaitMs = 10000; // an order that cannot run this long is dropped
constexpr int kLinesPerBox = 4;       // lines of the game's text box (CustomMessage::AddLineBreaks)

// How many sounds each bank of the game has (sfx_params.c, gSfxParams in bank order): a sound id past them would
// read outside the game's tables.
#undef DEFINE_SFX
#define DEFINE_SFX(...) +1
constexpr int kSfxCount[] = {
    0
#include "tables/sfx/playerbank_table.h"
    ,
    0
#include "tables/sfx/itembank_table.h"
    ,
    0
#include "tables/sfx/environmentbank_table.h"
    ,
    0
#include "tables/sfx/enemybank_table.h"
    ,
    0
#include "tables/sfx/systembank_table.h"
    ,
    0
#include "tables/sfx/ocarinabank_table.h"
    ,
    0
#include "tables/sfx/voicebank_table.h"
};
#undef DEFINE_SFX

// When an order can run.
enum class Needs {
    Nothing,  // any time
    Gameplay, // a scene is running and is not changing
    FreeLink, // ...and Link can be moved (no cutscene, dialogue, pause, ride)
    FreeText, // ...and no text box is open
};

struct OpDef {
    const char* name;
    Needs needs;
    void (*run)(PlayState* play, const json& op);
};

struct Queued {
    const OpDef* def;
    json op;
    int64_t since;
};

std::deque<Queued> sQueue;
bool sGivingItem = false;
bool sSpawning = false;          // an order is creating an actor: the objects it asks for need not be loaded...
std::set<int16_t> sFreeObjects; // ...and stay so until the next scene (its update may ask for them again)

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

void Drop(const json& op, const char* why) {
    SPDLOG_WARN("[Coop] Mods: order '{}' dropped ({})", SanitizeChat(GetString(op, "op"), 32), why);
}

// op[key], a whole number in [min, max]: required...
bool Need(const json& op, const char* key, int64_t min, int64_t max, int64_t& out) {
    auto it = op.find(key);
    if (it == op.end() || !it->is_number_integer()) {
        return false;
    }
    out = it->get<int64_t>();
    return out >= min && out <= max;
}

// ...or optional (def when it is missing).
bool Field(const json& op, const char* key, int64_t min, int64_t max, int64_t def, int64_t& out) {
    if (!op.contains(key)) {
        out = def;
        return true;
    }
    return Need(op, key, min, max, out);
}

// A text of the order: valid UTF-8, line breaks and no other control character, kMaxModText characters at most.
std::string Text(const json& op) {
    return SanitizeText(GetString(op, "text"), kMaxModText);
}

bool LinkDying(PlayState* play) {
    return GET_PLAYER(play)->stateFlags1 & PLAYER_STATE1_DEAD;
}

// The mask Link wears, or the one of the form he has: taken away, he could not take it off.
bool Worn(PlayState* play, int item) {
    static const uint8_t kFormMasks[] = { ITEM_MASK_FIERCE_DEITY, ITEM_MASK_GORON, ITEM_MASK_ZORA, ITEM_MASK_DEKU };
    Player* link = GET_PLAYER(play);
    if (link->transformation < PLAYER_FORM_HUMAN && kFormMasks[link->transformation] == item) {
        return true;
    }
    return Player_GetCurMaskItemId(play) == item;
}

void OpNotify(PlayState*, const json& op) {
    std::string text = Text(op);
    int64_t seconds = 0;
    if (text.empty() || !Field(op, "seconds", 1, 30, 6, seconds)) {
        return Drop(op, "text, seconds");
    }
    Notification::Emit({ .message = text, .remainingTime = (float)seconds });
}

void OpMessage(PlayState*, const json& op) {
    std::string text = ToGameFontText(Text(op)); // the game's font has no accents
    if (text.empty()) {
        return Drop(op, "text");
    }
    // What autoFormat does, plus a new box after every 4 lines (it only starts one where it wraps a line itself).
    CustomMessage::Entry entry;
    entry.autoFormat = false;
    CustomMessage::ReplaceColorChars(&text);
    CustomMessage::Replace(&text, "\n", "\x11");
    CustomMessage::AddLineBreaks(&text);
    SplitTextBoxes(text, '\x11', '\x10', kLinesPerBox);
    CustomMessage::EnsureMessageEnd(&text);
    CustomMessage::StartTextbox(text, entry);
}

void OpSfx(PlayState*, const json& op) {
    int64_t id = 0;
    if (!Need(op, "id", 0, 0xFFFF, id)) {
        return Drop(op, "id");
    }
    // A sound of the game: its bank, the 0x800 every id has, and one of that bank's entries.
    uint32_t bank = SFX_BANK(id);
    if (bank >= std::size(kSfxCount) || (id & 0xC00) != 0x800 || SFX_INDEX(id) >= kSfxCount[bank]) {
        return Drop(op, "no such sound");
    }
    Audio_PlaySfx((u16)id);
}

void OpItem(PlayState* play, const json& op) {
    int64_t id = 0;
    if (!Need(op, "id", 0, 0xFF, id) || !mods::ItemGivable((int)id)) {
        return Drop(op, "id");
    }
    sGivingItem = true;
    Rewards_SuppressBegin(); // not a group prize: nobody else gets a copy
    Item_Give(play, (u8)id);
    Rewards_SuppressEnd();
    sGivingItem = false;
}

void OpTake(PlayState* play, const json& op) {
    int64_t id = 0;
    if (!Need(op, "id", 0, 0xFF, id) || !mods::ItemGivable((int)id)) {
        return Drop(op, "id");
    }
    if (id >= (int64_t)std::size(gItemSlots)) {
        return; // no slot of its own (equipment, songs, ammo...): nothing to take
    }
    if (Worn(play, (int)id)) {
        return Drop(op, "Link wears that mask");
    }
    // Its slot when it holds this very item (several share one), or the first bottle that holds it.
    Inventory& inventory = gSaveContext.save.saveInfo.inventory;
    int slot = SLOT(id);
    int last = slot == SLOT_BOTTLE_1 ? SLOT_BOTTLE_6 : slot;
    for (int s = slot; s <= last; s++) {
        if (inventory.items[s] == id) {
            Inventory_DeleteItem((s16)id, (s16)s);
            return;
        }
    }
}

void OpRupees(PlayState*, const json& op) {
    int64_t amount = 0;
    if (!Need(op, "amount", -kMaxModRupees, kMaxModRupees, amount) || amount == 0) {
        return Drop(op, "amount");
    }
    // What fits in the wallet, or what there is.
    int have = std::max(0, gSaveContext.save.saveInfo.playerData.rupees + gSaveContext.rupeeAccumulator);
    int change = amount > 0 ? (int)std::min<int64_t>(amount, std::max(0, CUR_CAPACITY(UPG_WALLET) - have))
                            : (int)std::max<int64_t>(amount, -have);
    if (change == 0) {
        return;
    }
    Rewards_SuppressBegin(); // not a group prize
    Rupees_ChangeBy((s16)change);
    Rewards_SuppressEnd();
}

void OpHeal(PlayState* play, const json& op) {
    int64_t amount = 0;
    if (!Field(op, "amount", 0, kMaxModHealth, 0, amount)) {
        return Drop(op, "amount");
    }
    if (!LinkDying(play)) { // a Link already dying is not brought back halfway
        Health_ChangeBy(play, (s16)(amount == 0 ? gSaveContext.save.saveInfo.playerData.healthCapacity : amount));
    }
}

void OpDamage(PlayState* play, const json& op) {
    int64_t amount = 0;
    if (!Need(op, "amount", 1, kMaxModHealth, amount)) {
        return Drop(op, "amount");
    }
    if (!LinkDying(play)) {
        Health_ChangeBy(play, (s16)-amount); // health 0: the game's own death (Player_UpdateCommon), a fairy saves him
    }
}

void OpKill(PlayState* play, const json&) {
    if (!LinkDying(play)) {
        gSaveContext.save.saveInfo.playerData.health = 0;
    }
}

void OpMagic(PlayState* play, const json& op) {
    int64_t amount = 0;
    if (!Field(op, "amount", -MAGIC_DOUBLE_METER, MAGIC_DOUBLE_METER, 0, amount)) {
        return Drop(op, "amount");
    }
    if (amount >= 0) {
        Magic_Add(play, amount > 0 ? (s16)amount : (s16)MAGIC_FILL_TO_CAPACITY); // nothing without the magic meter
        return;
    }
    s8& magic = gSaveContext.save.saveInfo.playerData.magic;
    magic = (s8)std::max<int64_t>(0, magic + amount);
}

// pos[3]: finite and inside the world.
bool Position(const json& op, Vec3f& out) {
    auto it = op.find("pos");
    if (it == op.end() || !it->is_array() || it->size() != 3) {
        return false;
    }
    float v[3];
    for (size_t i = 0; i < 3; i++) {
        const json& n = (*it)[i];
        if (!n.is_number() || !(std::fabs(n.get<double>()) <= pose_limits::kWorldLimit)) {
            return false;
        }
        v[i] = (float)n.get<double>();
    }
    out = { v[0], v[1], v[2] };
    return true;
}

void OpSpawn(PlayState* play, const json& op) {
    int64_t actor = 0;
    int64_t params = 0;
    int64_t rotY = 0;
    int64_t dist = 0;
    if (!Need(op, "actor", 1, ACTOR_ID_MAX - 1, actor) || actor == ACTOR_EN_COOP_PUPPET ||
        !Field(op, "params", -32768, 65535, 0, params) || !Field(op, "rotY", -32768, 65535, 0, rotY) ||
        !Field(op, "dist", 0, kMaxModSpawnDistance, 100, dist)) {
        return Drop(op, "actor, params, rotY, dist");
    }
    Vec3f pos;
    if (op.contains("pos")) {
        if (!Position(op, pos)) {
            return Drop(op, "pos");
        }
    } else { // dist units in front of Link
        Actor* link = &GET_PLAYER(play)->actor;
        s16 yaw = link->shape.rot.y;
        pos = { link->world.pos.x + Math_SinS(yaw) * dist, link->world.pos.y, link->world.pos.z + Math_CosS(yaw) * dist };
    }
    // Only this game's: nobody else simulates it (never in ActorRegistry). Its objects need not be in the scene.
    sSpawning = true;
    Actor* created = ActorRegistry_SpawnUntracked((int16_t)actor, pos, (s16)params, (s16)rotY);
    sSpawning = false;
    if (created == nullptr) {
        SPDLOG_INFO("[Coop] Mods: actor {:#x} not created (a cleared room takes no enemies; 255 actors at most)",
                    (int)actor);
    }
}

void OpWarp(PlayState* play, const json& op) {
    int64_t entrance = 0;
    if (!Need(op, "entrance", 0, 0xFFFF, entrance)) {
        return Drop(op, "entrance");
    }
    // An entrance of the game: a scene of its table, one of that scene's spawns and its first layer.
    uint32_t scene = (uint32_t)entrance >> 9;
    uint32_t spawn = ((uint32_t)entrance >> 4) & 0x1F;
    if (scene >= ENTR_SCENE_MAX || sSceneEntranceTable[scene].table == nullptr ||
        spawn >= sSceneEntranceTable[scene].tableCount || (entrance & 0xF) != 0) {
        return Drop(op, "no such entrance");
    }
    play->nextEntrance = (u16)entrance;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
}

// One line per order the server may give (coop/docs/mods/API.md: game.*).
const OpDef kOps[] = {
    { "notify", Needs::Nothing, OpNotify },    // text, seconds
    { "message", Needs::FreeText, OpMessage }, // text
    { "sfx", Needs::Gameplay, OpSfx },         // id
    { "item", Needs::Gameplay, OpItem },       // id
    { "take", Needs::Gameplay, OpTake },       // id
    { "rupees", Needs::Gameplay, OpRupees },   // amount
    { "heal", Needs::Gameplay, OpHeal },       // amount (0: all)
    { "damage", Needs::Gameplay, OpDamage },   // amount
    { "kill", Needs::Gameplay, OpKill },
    { "magic", Needs::Gameplay, OpMagic },     // amount (0: fill)
    { "spawn", Needs::Gameplay, OpSpawn },     // actor, params, pos[3] | dist, rotY
    { "warp", Needs::FreeLink, OpWarp },       // entrance
};

const OpDef* FindOp(const std::string& name) {
    for (const OpDef& def : kOps) {
        if (name == def.name) {
            return &def;
        }
    }
    return nullptr;
}

bool Ready(Needs needs, PlayState* play) {
    if (needs == Needs::Nothing) {
        return true;
    }
    // In the server's world, playing, not in the ending (the co-op stands aside there) and no scene change.
    if (!WorldSession_Active() || EndingMode_Active() || !PoseCapture_InGameplay() ||
        play->transitionTrigger != TRANS_TRIGGER_OFF || play->transitionMode != TRANS_MODE_OFF) {
        return false;
    }
    if (needs == Needs::Gameplay) {
        return true;
    }
    if (!Warp_BlockedReason().empty()) { // a cutscene, a dialogue, the pause menu, a ride, death
        return false;
    }
    return needs == Needs::FreeLink || play->msgCtx.msgMode == MSGMODE_NONE;
}

void Expire(const Queued& q) {
    SPDLOG_INFO("[Coop] Mods: order '{}' could not run in 10 s: dropped", q.def->name);
    if (q.def->run == OpMessage) {
        std::string text = Text(q.op);
        if (!text.empty()) {
            Chat_Add(ChatKind::Info, text); // the message still reaches the player
        }
    }
}

// Start of every frame: what can run now runs, in the order it came.
void RunQueue() {
    if (sQueue.empty()) {
        return;
    }
    if (!Mods_Enabled() || WorldSession_State() == WorldState::Outside) {
        sQueue.clear();
        return;
    }
    PlayState* play = gPlayState;
    int64_t now = NowMs();
    std::deque<Queued> queue;
    queue.swap(sQueue);
    for (Queued& q : queue) {
        if (Ready(q.def->needs, play)) {
            q.def->run(play, q.op);
        } else if (now - q.since >= kMaxWaitMs) {
            Expire(q);
        } else {
            sQueue.push_back(std::move(q));
        }
    }
}

void OnMod(const json& ev) {
    if (!Mods_Enabled() || WorldSession_State() == WorldState::Outside) {
        return; // never on the player's own saves
    }
    auto ops = ev.find("ops");
    if (ops == ev.end() || !ops->is_array()) {
        return;
    }
    int taken = 0;
    for (const json& op : *ops) {
        if (taken++ == kMaxModOpsPerEvent || sQueue.size() >= kMaxQueued) {
            SPDLOG_WARN("[Coop] Mods: too many orders at once, the rest are dropped");
            return;
        }
        const OpDef* def = FindOp(GetString(op, "op"));
        if (def == nullptr) {
            Drop(op, "unknown order");
            continue;
        }
        sQueue.push_back({ def, op, NowMs() });
    }
}

void OnLost(const std::string&) {
    sQueue.clear();
}

void RegisterGameOps() {
    // An order's actor is created even if the scene has not loaded its graphics (the port finds them by name):
    // every object asked for while it is created stays free of that rule until the next scene.
    COND_VB_SHOULD(VB_ENABLE_OBJECT_DEPENDENCY, true, {
        int16_t objectId = (int16_t)va_arg(args, int);
        if (sSpawning) {
            sFreeObjects.insert(objectId);
        }
        if (sFreeObjects.count(objectId) > 0) {
            *should = false;
        }
    });
    COND_HOOK(OnSceneInit, true, [](s8, s8) { sFreeObjects.clear(); });
    COND_HOOK(OnGameStateMainStart, true, RunQueue);
}

} // namespace

bool Mods_Enabled() {
    return CVarGetInteger("gCoop.Mods", 1) != 0 && !HostMode_Enabled();
}

bool Mods_GivingItem() {
    return sGivingItem;
}

COOP_ON_EVENT(gameOpsMod, ev::kMod, OnMod);
COOP_ON_LOST(gameOpsLost, OnLost);

} // namespace coop::client

static RegisterShipInitFunc sGameOpsInit(coop::client::RegisterGameOps);
