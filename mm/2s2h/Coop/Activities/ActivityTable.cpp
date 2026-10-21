// [COOP] The minigames and side quests the group shares (spec §3.1, §13; limits fix §4). ONE LINE EACH: change a line
// to change how that minigame is played together.
//   mode       Together: every guest plays the same game (what they do counts); EachOwn: each game runs its own copy
//              after following the director in (results shared); Turns: one player at a time, the others watch its
//              time and points and play their round next
//   entrances  the scene reloaded for the minigame (the director's game going there takes the group along, if follow)
//   loadout    Together: the guests get the director's minigame equipment (the bow on B, its bombs...)
//   slots      Together: where the guests stand when it starts {right, forward} of the director, in units
//   props      room actors the minigame uses { ACTOR_..., ours }: the director's game simulates them while it runs
//              (ours = machinery acting with the director's Link; false = targets any Link may touch)
//   hooks      EachOwn: how a guest starts its own run and how a run ended (won, time); a minigame played in several
//              scenes (scene -1): when it runs (active), how this game sets it off (startedHere) and how it goes
//              (progress, shown in its room's window)
// Every minigame a game starts opens an activity room (Room/Room.h): where it is played comes from its line (special
// entrances: "entrance"; scene -1: "anywhere"; otherwise "here", where its NPC is).
// Quests only name the invitations ("Ana invites you to Anju and Kafei") by the NPC next to the inviter; their dialogues,
// cutscenes and prizes are shared by the group anyway (TalkSync.cpp, Cinema.cpp, Rewards.cpp).
#include "Activities.h"

#include "common/Protocol.h"

#include <algorithm>
#include <cstdio>
#include <iterator>

extern "C" {
#include "z64.h"
#include "functions.h"
#include "variables.h"
}

namespace coop::client {

namespace {

using K = ActivityKind;
using M = ActivityMode;

// Gorman brothers' race (Cada uno): the referee (En_Horse_Game_Check) only exists while this game's race state is
// START (a local bit: FieldTable.cpp), so a guest following the director in sets it. 2 = won (white fade), 3/4 = lost
// (z_en_horse_game_check.c); its clock is TIMER_ID_MINIGAME_2.
void GormanGuestStart() {
    SET_WEEKEVENTREG_HORSE_RACE_STATE(WEEKEVENTREG_HORSE_RACE_STATE_START);
}

bool GormanResult(bool* won, int32_t* cs) {
    u8 state = GET_WEEKEVENTREG_HORSE_RACE_STATE;
    *won = state == WEEKEVENTREG_HORSE_RACE_STATE_2;
    *cs = (int32_t)std::min<u64>(gSaveContext.timerCurTimes[TIMER_ID_MINIGAME_2], 36000000);
    return state == WEEKEVENTREG_HORSE_RACE_STATE_2 || state == WEEKEVENTREG_HORSE_RACE_STATE_3 ||
           state == WEEKEVENTREG_HORSE_RACE_STATE_4;
}

// Goron race (Cada uno): EVENTINF_11 = this game's Link crossed first, EVENTINF_12 = a Goron did, or we gave up
// (z_en_mt_tag.c); neither (a false start: the race begins again) is no result. Its clock is TIMER_ID_MINIGAME_2,
// stopped (state 6) when someone crosses the line.
bool GoronResult(bool* won, int32_t* cs) {
    *won = CHECK_EVENTINF(EVENTINF_11) != 0;
    bool timed = gSaveContext.timerStates[TIMER_ID_MINIGAME_2] == TIMER_STATE_6;
    *cs = timed ? (int32_t)std::min<u64>(gSaveContext.timerCurTimes[TIMER_ID_MINIGAME_2], 36000000) : 0;
    return *won || CHECK_EVENTINF(EVENTINF_12) != 0;
}

constexpr ActivityHooks kGormanHooks = { GormanGuestStart, GormanResult };
constexpr ActivityHooks kGoronHooks = { nullptr, GoronResult };
// The Bombers' hide-and-seek (Bombers.cpp): Jim sets it off in North Clock Town and the five hide all over the town.
constexpr ActivityHooks kBombersHooks = { nullptr, nullptr, Bombers_Active, Bombers_StartedHere, Bombers_Progress };

#define NO_SLOTS { { 0.f, 0.f }, { 0.f, 0.f }, { 0.f, 0.f } }
// Side by side with the director, facing the same way (a shooting gallery's counter)
#define ROW(d) { { -(d), 0.f }, { (d), 0.f }, { -2.f * (d), 0.f } }
#define E(scene, spawn) ((uint16_t)ENTRANCE(scene, spawn))

constexpr ActivityDef kActivities[] = {
    // ---- Minigames ----
    // Octoroks (city) and Deku Scrubs, Guays and Wolfos (swamp) are enemies (replicated) that check AC_HIT: the
    // guests' arrows count.
    { "galeria_ciudad", Txt::ActGaleriaCiudad, K::Minigame, SCENE_SYATEKI_MIZU, { ACTOR_EN_SYATEKI_MAN },
      M::Together, {}, false, true, ROW(60.f) },
    { "galeria_pantano", Txt::ActGaleriaPantano, K::Minigame, SCENE_SYATEKI_MORI, { ACTOR_EN_SYATEKI_MAN },
      M::Together, {}, false, true, ROW(60.f) },
    // En_Fu spawns its targets (En_Fu_Mato, En_Fu_Kago: replicated). Day 3 (arrows) checks AC_HIT: the guests' shots
    // count; days 1-2 (bombchus, bombs in the baskets) check contact (OC): the guests' contacts are sent to the
    // director (HitSync.cpp). The spinning platform (Bg_Fu_Kaiten) is the room's, moved by the real En_Fu: lent to the
    // director, it spins for everyone.
    { "honey_darling", Txt::ActHoneyDarling, K::Minigame, SCENE_BOWLING, { ACTOR_EN_FU }, M::Together, {}, false, true,
      { { -40.f, 0.f }, { 40.f, 0.f }, { 0.f, -40.f } }, { { ACTOR_BG_FU_KAITEN, true } } },
    // GHOST_HUT 1 starts it (z_en_gb2.c, the Poe sisters are its children: enemies, AC_HIT); 2 is the way out.
    { "casa_espiritus", Txt::ActCasaEspiritus, K::Minigame, SCENE_TOUGITES, { ACTOR_EN_GB2 }, M::Together,
      { E(GHOST_HUT, 1) }, true, false, NO_SLOTS },
    // En_Gk sends to GORON_RACETRACK 1; the race's referee (En_Mt_tag, BG: local) only exists there and judges each
    // game's own Link; the runners (En_Rg) are NPCs (replicated). 2 is the way out.
    { "carrera_goron", Txt::ActCarreraGoron, K::Minigame, SCENE_GORONRACE, { ACTOR_EN_GK }, M::EachOwn,
      { E(GORON_RACETRACK, 1) }, true, false, NO_SLOTS, {}, &kGoronHooks },
    // En_In: GORMAN_TRACK 5 when the race starts (text 0x3475). The referee (En_Horse_Game_Check, BG: local) judges
    // each game's own Link: a guest's exists once it sets its race state (kGormanHooks).
    { "carrera_gorman", Txt::ActCarreraGorman, K::Minigame, SCENE_KOEPONARACE, { ACTOR_EN_IN }, M::EachOwn,
      { E(GORMAN_TRACK, 5) }, true, false, NO_SLOTS, {}, &kGormanHooks },
    // En_Az: WATERFALL_RAPIDS 1 is the race (the HUD shows its rings there); 2 is the arrival after it. The rings
    // (En_Twig) look at the nearest Link: the guests' count.
    { "carrera_castores", Txt::ActCarreraCastores, K::Minigame, SCENE_35TAKI, { ACTOR_EN_AZ }, M::Together,
      { E(WATERFALL_RAPIDS, 1) }, true, false, NO_SLOTS, { { ACTOR_EN_TWIG, false } } },
    // En_Aob_01: DOGGY_RACETRACK 1 starts the race (and reloads the track when it is over). One player by design: the
    // bet is each one's own; the dogs (En_Racedog) are children of the NPC: moved by the game of who bets.
    { "carrera_perros", Txt::ActCarreraPerros, K::Minigame, SCENE_F01_B, { ACTOR_EN_AOB_01 }, M::Turns,
      { E(DOGGY_RACETRACK, 1) }, true, false, NO_SLOTS },
    // Its game reloads the ranch in a scene layer of its own (ROMANI_RANCH 0 with cutscene index 0xFFF0: Link on
    // Epona, the balloons) where the others are not, so it is played one at a time; ROMANI_RANCH 1 reloads the ranch
    // at its end. (The night of the aliens needs no line: they are replicated enemies, everyone in the ranch fights
    // the same ones.)
    { "globos_romani", Txt::ActGlobosRomani, K::Minigame, SCENE_F01, { ACTOR_EN_MA4 }, M::Turns, {}, false, false,
      NO_SLOTS },
    // The maze is played where it is; TREASURE_CHEST_SHOP 1 only reloads the shop at its end (the result). One player
    // by design; its maze (Obj_Takaraya_Wall, created by the NPC) travels with its tables: who watches sees it.
    { "cofres", Txt::ActCofres, K::Minigame, SCENE_TAKARAYA, { ACTOR_EN_TAKARAYA }, M::Turns, {}, false, false,
      NO_SLOTS },
    // The rupees are taken by touching them (contacts: HitSync.cpp); the lifts are moved by the director's game.
    { "patio_deku", Txt::ActPatioDeku, K::Minigame, SCENE_DEKUTES, { ACTOR_EN_LIFT_NUTS }, M::Together, {}, false,
      false, NO_SLOTS, { { ACTOR_EN_GAMELUPY, false }, { ACTOR_OBJ_LUPYGAMELIFT, true } } },
    // The logs (En_Maruta) are children of the NPC: already replicated.
    { "espadachin", Txt::ActEspadachin, K::Minigame, SCENE_DOUJOU, { ACTOR_EN_KENDO_JS }, M::Together, {}, false,
      false, ROW(60.f) },
    // One player by design.
    { "cartero", Txt::ActCartero, K::Minigame, SCENE_POSTHOUSE, { ACTOR_EN_MM3 }, M::Turns, {}, false, false,
      NO_SLOTS },
    // The ride starts when its game loads the swamp on the boat (spawn 6); TOURIST_INFORMATION 1/2 take the player
    // back at its end. The boat (Bg_Ingate) is each game's own, NOT a prop: its cruise belongs to the player that
    // loaded the scene on it and its code moves that player (camera, texts, the trip back), so a copy of another
    // game's boat would leave the others without theirs. One at a time.
    { "barca_koume", Txt::ActBarcaKoume, K::Minigame, SCENE_20SICHITAI, { ACTOR_EN_TRU_MT }, M::Turns, {},
      false, false, NO_SLOTS },
    // Played on the pillars of the coast; GREAT_BAY_COAST 13 only reloads the coast at its end (the result). One
    // player by design.
    { "saltos_pescador", Txt::ActSaltosPescador, K::Minigame, SCENE_30GYOSON, { ACTOR_EN_JGAME_TSN }, M::Turns, {},
      false, false, NO_SLOTS, { { ACTOR_OBJ_JGAME_LIGHT, true } } },
    // One player by design. Its doors (Bg_Crace_Movebg) are each game's own, NOT props: the race starts no timer and
    // shows no score, so this game never knows it runs (nothing would keep them lent), and they judge the local Link
    // with the local switch flags.
    { "mayordomo_deku", Txt::ActMayordomoDeku, K::Minigame, SCENE_DANPEI, { ACTOR_EN_DNO }, M::Turns, {}, false,
      false, NO_SLOTS },
    // Played all over Clock Town (scene -1): every game catches the Bombers it simulates and the count is the world's
    // (Bombers.cpp); whoever catches the last one takes the room to Jim for the code. Its NPCs name the invitations.
    { "bombers", Txt::ActBombers, K::Minigame, -1,
      { ACTOR_EN_BOMJIMA, ACTOR_EN_BOMBERS, ACTOR_EN_BOMBERS2 }, M::Shared, {}, false, false, NO_SLOTS, {},
      &kBombersHooks },
    // ---- Side quests (they name the invitations; their mode is not used) ----
    { "anju_kafei", Txt::ActAnjuKafei, K::Quest, -1,
      { ACTOR_EN_AN, ACTOR_EN_TEST3, ACTOR_EN_AL, ACTOR_EN_SUTTARI, ACTOR_EN_AH, ACTOR_EN_PM }, M::Turns, {}, false,
      false, NO_SLOTS },
    { "pollitos_grog", Txt::ActPollitosGrog, K::Quest, -1, { ACTOR_EN_HS }, M::Turns, {}, false, false, NO_SLOTS },
    { "romani_cremia", Txt::ActRomaniCremia, K::Quest, -1, { ACTOR_EN_MA4, ACTOR_EN_MA_YTS, ACTOR_EN_MA_YTO }, M::Turns,
      {}, false, false, NO_SLOTS },
    { "anciana_bombas", Txt::ActAncianaBombas, K::Quest, -1, { ACTOR_EN_BABA }, M::Turns, {}, false,
      false, NO_SLOTS },
    { "kamaro", Txt::ActKamaro, K::Quest, -1, { ACTOR_EN_YB, ACTOR_EN_RZ }, M::Turns, {}, false, false,
      NO_SLOTS },
    { "abuela_anju", Txt::ActAbuelaAnju, K::Quest, -1, { ACTOR_EN_NB }, M::Turns, {}, false, false, NO_SLOTS },
    { "keaton", Txt::ActKeaton, K::Quest, -1, { ACTOR_EN_KITAN }, M::Turns, {}, false, false, NO_SLOTS },
    { "mascara_piedra", Txt::ActMascaraPiedra, K::Quest, -1, { ACTOR_EN_STONE_HEISHI }, M::Turns, {}, false, false,
      NO_SLOTS },
    { "mascara_gibdo", Txt::ActMascaraGibdo, K::Quest, -1, { ACTOR_EN_PAMERA, ACTOR_EN_HG, ACTOR_EN_HGO }, M::Turns,
      {}, false, false, NO_SLOTS },
    { "mano_retrete", Txt::ActManoRetrete, K::Quest, -1, { ACTOR_EN_BJT }, M::Turns, {}, false, false, NO_SLOTS },
    { "compania_gorman", Txt::ActCompaniaGorman, K::Quest, -1, { ACTOR_EN_GM, ACTOR_EN_TOTO }, M::Turns, {}, false,
      false, NO_SLOTS },
    { "fantasmas_dampe", Txt::ActFantasmasDampe, K::Quest, -1, { ACTOR_EN_TK }, M::Turns, {}, false, false,
      NO_SLOTS },
    { "huevos_zora", Txt::ActHuevosZora, K::Quest, -1, { ACTOR_EN_MK, ACTOR_EN_ZOV }, M::Turns, {}, false, false,
      NO_SLOTS },
    { "casa_skulltulas", Txt::ActCasaSkulltulas, K::Quest, -1, { ACTOR_EN_SSH }, M::Turns, {}, false, false,
      NO_SLOTS },
    { "comercio_deku", Txt::ActComercioDeku, K::Quest, -1, { ACTOR_EN_AKINDONUTS, ACTOR_EN_SCOPENUTS }, M::Turns,
      {}, false, false, NO_SLOTS },
};

#undef NO_SLOTS
#undef ROW
#undef E

constexpr bool KeysAreValid() {
    for (const ActivityDef& d : kActivities) {
        size_t n = 0;
        for (const char* p = d.key; *p != '\0'; p++, n++) {
            char c = *p;
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) {
                return false;
            }
        }
        if (n == 0 || n > (size_t)kMaxActivityKey) {
            return false;
        }
    }
    return true;
}
static_assert(KeysAreValid(), "activity keys: 1..32 characters of a-z, 0-9 and _ (the server checks the same)");

bool SameEntrance(uint16_t a, uint16_t b) {
    return (a & ~0xF) == (b & ~0xF); // the low bits are the scene layer
}

} // namespace

const ActivityDef* Activity_All(size_t* count) {
    *count = std::size(kActivities);
    return kActivities;
}

const ActivityDef* Activity_ByKey(const std::string& key) {
    for (const ActivityDef& d : kActivities) {
        if (key == d.key) {
            return &d;
        }
    }
    return nullptr;
}

const ActivityDef* Activity_MinigameOfScene(int16_t scene) {
    for (const ActivityDef& d : kActivities) {
        if (d.kind == ActivityKind::Minigame && d.scene == scene) {
            return &d;
        }
    }
    return nullptr;
}

const ActivityDef* Activity_QuestOfNpc(int16_t actorId) {
    for (const ActivityDef& d : kActivities) {
        if (d.kind != ActivityKind::Quest && !Activity_IsGlobal(d)) {
            continue; // (a minigame played in several scenes names the invitations near its NPCs too)
        }
        for (int16_t npc : d.npcs) {
            if (npc != 0 && npc == actorId) {
                return &d;
            }
        }
    }
    return nullptr;
}

bool Activity_IsSpecialEntrance(const ActivityDef& def, uint16_t entrance) {
    for (uint16_t e : def.entrances) {
        if (e != 0 && SameEntrance(e, entrance)) {
            return true;
        }
    }
    return false;
}

bool Activity_IsPropId(int16_t actorId) {
    for (const ActivityDef& d : kActivities) {
        if (Activity_Prop(d, actorId) != nullptr) {
            return true;
        }
    }
    return false;
}

const ActivityProp* Activity_Prop(const ActivityDef& def, int16_t actorId) {
    for (const ActivityProp& p : def.props) {
        if (p.id != 0 && p.id == actorId) {
            return &p;
        }
    }
    return nullptr;
}

bool Activity_IsGlobal(const ActivityDef& def) {
    return def.kind == ActivityKind::Minigame && def.scene < 0 && def.hooks != nullptr && def.hooks->active != nullptr;
}

const char* Activity_PlaceName(const ActivityDef& def) {
    if (Activity_IsGlobal(def)) {
        return "anywhere";
    }
    for (uint16_t e : def.entrances) {
        if (e != 0) {
            return "entrance";
        }
    }
    return "here";
}

const char* Activity_ModeName(ActivityMode mode) {
    switch (mode) {
        case ActivityMode::Together:
            return "together";
        case ActivityMode::EachOwn:
            return "each";
        case ActivityMode::Turns:
            return "turns";
        case ActivityMode::Shared:
            return "shared";
    }
    return "together";
}

const char* Activity_ModeText(const std::string& modeName) {
    if (modeName == "together") {
        return Tc(Txt::ModeTogether);
    }
    if (modeName == "each") {
        return Tc(Txt::ModeEach);
    }
    if (modeName == "turns") {
        return Tc(Txt::ModeTurns);
    }
    if (modeName == "shared") {
        return Tc(Txt::ModeShared);
    }
    return "";
}

const char* Activity_Name(const ActivityDef& def) {
    return Tc(def.name);
}

const char* Activity_NameIn(const ActivityDef& def, Lang lang) {
    return TcIn(lang, def.name);
}

std::string Activity_ShownName(const std::string& key, const std::string& sent) {
    const ActivityDef* def = key.empty() ? nullptr : Activity_ByKey(key);
    return def != nullptr ? std::string(Activity_Name(*def)) : sent;
}

std::string Activity_TimeText(int64_t cs) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d.%02d", (int)(cs / 6000), (int)(cs / 100 % 60), (int)(cs % 100));
    return buf;
}

} // namespace coop::client
