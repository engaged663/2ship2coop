// [COOP] Prizes with the group (spec §5). The game that gets a prize sends it ("act_reward") to the mates who were
// there: a GetItem an NPC (not a shop) gives, or any while a minigame runs here or just ended; and the rupees a
// minigame gives outside a GetItem (sent every half second). The mates' games give them what is each player's own
// (rupees, bottle contents, ammo, hearts) and show its text; what the world shares (masks, heart pieces, upgrades...)
// is already theirs through the world (B): only its text. Nothing is copied twice (a heart piece would raise
// everyone's life) and nothing bought.
#include "Activities.h"

#include "2s2h/Coop/Actors/ActorRegistry.h"
#include "2s2h/Coop/Actors/CoopEngine.h"
#include "2s2h/Coop/Chat/ChatModel.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Group/Group.h"
#include "2s2h/Coop/World/FieldTable.h"
#include "2s2h/Coop/World/WorldSession.h"

#include "common/Protocol.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <deque>
#include <initializer_list>

extern "C" {
#include "functions.h"
#include "sequence.h"
#include "variables.h"
}

// z_player.c's table (the same struct as Rando/ConvertItem.cpp)
typedef struct GetItemEntry {
    /* 0x0 */ u8 itemId;
    /* 0x1 */ u8 field;
    /* 0x2 */ s8 gid;
    /* 0x3 */ u8 textId;
    /* 0x4 */ u16 objectId;
} GetItemEntry;
extern "C" GetItemEntry sGetItemTable[GI_MAX - 1];
extern "C" void Interface_StartBottleTimer(s16 seconds, s16 timerId); // z_parameter.c (no header has it)

namespace coop::client {

namespace {

constexpr float kShareDist = 2000.f;    // mates this close to us get a copy
constexpr float kReceiveDist = 2500.f;  // (checked again on their side)
constexpr int64_t kRupeeBatchMs = 500;
constexpr int64_t kPaidMemoryMs = 60000; // a GetItem from the actor we paid in the last minute was bought
constexpr int64_t kWaitMs = 10000;       // a copy waits this long for an empty bottle / a free text box

enum class GiClass { Rupees, Own, Bottle, Shared, Never };

GiClass Classify(int gi) {
    switch (gi) {
        case GI_RUPEE_GREEN: case GI_RUPEE_BLUE: case GI_RUPEE_10: case GI_RUPEE_RED: case GI_RUPEE_PURPLE:
        case GI_RUPEE_SILVER: case GI_RUPEE_HUGE:
            return GiClass::Rupees;
        case GI_RECOVERY_HEART: case GI_MAGIC_JAR_SMALL: case GI_MAGIC_JAR_BIG:
        case GI_BOMBS_1: case GI_BOMBS_5: case GI_BOMBS_10: case GI_BOMBS_20: case GI_BOMBS_30:
        case GI_DEKU_STICKS_1: case GI_BOMBCHUS_10: case GI_BOMBCHUS_20: case GI_BOMBCHUS_1: case GI_BOMBCHUS_5:
        case GI_ARROWS_10: case GI_ARROWS_30: case GI_ARROWS_40: case GI_ARROWS_50:
        case GI_DEKU_NUTS_1: case GI_DEKU_NUTS_5: case GI_DEKU_NUTS_10: case GI_POWDER_KEG: case GI_MAGIC_BEANS:
            return GiClass::Own;
        case GI_POTION_RED_BOTTLE: case GI_POTION_RED: case GI_POTION_GREEN: case GI_POTION_BLUE: case GI_FAIRY:
        case GI_MILK_BOTTLE: case GI_MILK_HALF: case GI_FISH: case GI_BUG: case GI_BLUE_FIRE: case GI_POE:
        case GI_BIG_POE: case GI_SPRING_WATER: case GI_HOT_SPRING_WATER: case GI_GOLD_DUST: case GI_MUSHROOM:
        case GI_CHATEAU_BOTTLE: case GI_HYLIAN_LOACH: case GI_CHATEAU: case GI_MILK: case GI_GOLD_DUST_2:
        case GI_HYLIAN_LOACH_2:
            return GiClass::Bottle;
        case GI_NONE: case GI_ZORA_EGG: case GI_SEAHORSE: case GI_SEAHORSE_CAUGHT: case GI_DEKU_PRINCESS:
        case GI_STRAY_FAIRY: case GI_SKULL_TOKEN: case GI_ICE_TRAP: case GI_BOTTLE_STOLEN:
            return GiClass::Never;
        default:
            return (gi > GI_NONE && gi < GI_MAX) ? GiClass::Shared : GiClass::Never;
    }
}

bool In(int16_t id, std::initializer_list<int16_t> list) {
    return std::find(list.begin(), list.end(), id) != list.end();
}

// They sell: what they hand over is bought, never a prize.
bool IsShop(int16_t id) {
    return In(id, { ACTOR_EN_GINKO_MAN, ACTOR_EN_SOB1, ACTOR_EN_TRT, ACTOR_EN_OSSAN, ACTOR_EN_FSN, ACTOR_EN_BAL,
                    ACTOR_EN_KUJIYA, ACTOR_EN_TAB });
}

// ---- The game that gets the prize ----

int sSuppress = 0;
bool sInGetItem = false;
int sPendingRupees = 0;
int64_t sRupeesSinceMs = 0;
int64_t sPaidMs = -kPaidMemoryMs;
Actor* sPaidTo = nullptr;

Actor* TalkingTo(PlayState* play) {
    Player* link = (Player*)play->actorCtx.actorLists[ACTORCAT_PLAYER].first;
    return link != nullptr ? link->talkActor : nullptr;
}

bool ShouldShare(PlayState* play, Actor* giver) {
    if (sSuppress > 0 || !WorldSession_Active() || !Group_Has() || !Group_AnyMateNear(kShareDist)) {
        return false;
    }
    if (Director_Recent() != nullptr) {
        return true; // a minigame here: its prizes are the group's
    }
    bool npc = giver != nullptr && giver->category == ACTORCAT_NPC && ActorRegistry_Get(giver) != nullptr &&
               !IsShop(giver->id);
    bool bought = giver != nullptr && giver == sPaidTo && Group_NowMs() - sPaidMs < kPaidMemoryMs;
    return npc && !bought;
}

void SendRupees() {
    json ev = MakeEvent(ev::kActReward);
    ev["rupees"] = std::min(sPendingRupees, kMaxRewardRupees);
    NetClient::Get().SendEvent(ev);
    sPendingRupees = 0;
    sRupeesSinceMs = 0;
}

// ---- The mates that get a copy ----

struct Copy {
    int gi = 0;
    std::string nick;
    bool applied = false;
    int64_t sinceMs = 0;
};

std::deque<Copy> sCopies;

// The bottle content a GetItem puts in a bottle (the "new bottle" items give their content: the bottle itself comes
// through the world).
u8 BottleContent(u8 item) {
    switch (item) {
        case ITEM_LONGSHOT: // the red potion in a new bottle (an OoT leftover id)
            return ITEM_POTION_RED;
        case ITEM_CHATEAU_2:
            return ITEM_CHATEAU;
        case ITEM_MILK:
            return ITEM_MILK_BOTTLE;
        case ITEM_GOLD_DUST_2:
            return ITEM_GOLD_DUST;
        case ITEM_HYLIAN_LOACH_2:
            return ITEM_HYLIAN_LOACH;
        default:
            return item;
    }
}

// An empty bottle of ours gets the content (false: none yet).
bool FillBottle(PlayState* play, u8 content) {
    u8* items = gSaveContext.save.saveInfo.inventory.items;
    for (int slot = SLOT_BOTTLE_1; slot <= SLOT_BOTTLE_6; slot++) {
        if (items[slot] != ITEM_BOTTLE) {
            continue;
        }
        items[slot] = content;
        if (content == ITEM_HOT_SPRING_WATER) {
            Interface_StartBottleTimer(60, slot - SLOT_BOTTLE_1);
        }
        fields::RefreshButtonsForSlot(slot);
        return true;
    }
    return false;
}

bool TextBoxFree(PlayState* play) {
    return play->msgCtx.msgMode == MSGMODE_NONE && play->transitionTrigger == TRANS_TRIGGER_OFF &&
           !IS_PAUSED(&play->pauseCtx) && !Play_InCsMode(play);
}

void OnReward(const json& ev) {
    PlayState* play = gPlayState;
    uint8_t from = (uint8_t)GetInt(ev, "from");
    if (play == nullptr || !WorldSession_Active() || !Group_OptRewards() || !Group_IsMate(from) ||
        !(Guest_Director() == from || Group_MateNear(from, kReceiveDist))) {
        return;
    }
    std::string nick = GetString(ev, "nick");
    int64_t rupees = GetInt(ev, "rupees");
    if (rupees > 0) {
        rupees = std::min<int64_t>(rupees, kMaxRewardRupees);
        Rewards_SuppressBegin();
        Rupees_ChangeBy((s16)rupees);
        Rewards_SuppressEnd();
        Audio_PlaySfx(NA_SE_SY_GET_ITEM);
        Chat_Add(ChatKind::Ok, "+" + std::to_string(rupees) + " rupias (premio de " + nick + ").");
        return;
    }
    int gi = (int)GetInt(ev, "gi");
    if (Classify(gi) == GiClass::Never || sCopies.size() > 16) {
        return;
    }
    sCopies.push_back({ gi, nick, false, Group_NowMs() });
}

// One copy at a time: give it (a bottle content waits for an empty bottle), then show its text.
void ProcessCopies(PlayState* play) {
    if (sCopies.empty()) {
        return;
    }
    Copy& c = sCopies.front();
    const GetItemEntry& entry = sGetItemTable[c.gi - 1];
    bool late = Group_NowMs() - c.sinceMs > kWaitMs;
    if (!c.applied) {
        switch (Classify(c.gi)) {
            case GiClass::Rupees:
            case GiClass::Own:
                Rewards_SuppressBegin();
                Item_Give(play, entry.itemId);
                Rewards_SuppressEnd();
                break;
            case GiClass::Bottle:
                if (!FillBottle(play, BottleContent(entry.itemId))) {
                    if (!late) {
                        return; // the world may still bring the new bottle
                    }
                    Chat_Add(ChatKind::Warn, "No tenías una botella vacía para el premio de " + c.nick + ".");
                    sCopies.pop_front();
                    return;
                }
                break;
            default:
                break; // shared: the world already gives it
        }
        c.applied = true;
        c.sinceMs = Group_NowMs();
        SPDLOG_INFO("[Coop] Group prize GI {:#x} from {}", c.gi, c.nick);
    }
    if (TextBoxFree(play)) {
        Message_StartTextbox(play, entry.textId, NULL);
        Audio_PlayFanfare(Classify(c.gi) == GiClass::Shared ? NA_BGM_GET_ITEM : NA_BGM_GET_SMALL_ITEM);
        Chat_Add(ChatKind::Ok, "Premio del grupo (de " + c.nick + ").");
        sCopies.pop_front();
    } else if (Group_NowMs() - c.sinceMs > kWaitMs) {
        Chat_Add(ChatKind::Ok, "Premio del grupo (de " + c.nick + ").");
        sCopies.pop_front();
    }
}

void FrameEnd() {
    PlayState* play = gPlayState;
    if (play == nullptr || !WorldSession_Active()) {
        return;
    }
    if (sPendingRupees > 0 && Group_NowMs() - sRupeesSinceMs >= kRupeeBatchMs) {
        SendRupees();
    }
    ProcessCopies(play);
}

void Reset() {
    sCopies.clear();
    sPendingRupees = 0;
    sRupeesSinceMs = 0;
    sPaidTo = nullptr;
}

void RegisterRewards() {
    COND_HOOK(OnGameStateMainFinish, true, FrameEnd);
    COND_HOOK(OnPlayDestroy, true, []() { sPaidTo = nullptr; });
}

} // namespace

void Rewards_SuppressBegin() {
    sSuppress++;
}

void Rewards_SuppressEnd() {
    sSuppress = std::max(0, sSuppress - 1);
}

} // namespace coop::client

using namespace coop;
using namespace coop::client;

extern "C" void Coop_OnGetItem(PlayState* play, s32 getItemId, Actor* giver) {
    sInGetItem = true;
    if (play == nullptr || getItemId <= GI_NONE || getItemId >= GI_MAX || Classify(getItemId) == GiClass::Never ||
        !ShouldShare(play, giver)) {
        return;
    }
    json ev = MakeEvent(ev::kActReward);
    ev["gi"] = getItemId;
    NetClient::Get().SendEvent(ev);
}

extern "C" void Coop_OnGetItemEnd(void) {
    sInGetItem = false;
}

extern "C" void Coop_OnRupeesChanged(s16 rupeeChange) {
    PlayState* play = gPlayState;
    if (play == nullptr || !WorldSession_Active() || rupeeChange == 0) {
        return;
    }
    if (rupeeChange < 0) {
        sPaidMs = Group_NowMs(); // paid: what this actor hands over next is bought
        sPaidTo = TalkingTo(play);
        return;
    }
    if (sInGetItem || sSuppress > 0 || Director_Recent() == nullptr || !Group_Has() ||
        !Group_AnyMateNear(kShareDist)) {
        return;
    }
    if (sPendingRupees == 0) {
        sRupeesSinceMs = Group_NowMs();
    }
    sPendingRupees = std::min(sPendingRupees + rupeeChange, 9999);
}

COOP_ON_EVENT(rewardsEvent, coop::ev::kActReward, OnReward);
COOP_ON_EVENT(rewardsWelcome, coop::ev::kWelcome, [](const coop::json&) { Reset(); });
COOP_ON_LOST(rewardsLost, [](const std::string&) { Reset(); });
static RegisterShipInitFunc sRewardsInit(RegisterRewards);
