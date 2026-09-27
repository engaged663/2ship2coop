#include "SaveBuilder.h"

#include "FieldTable.h"

#include "common/Clock.h"

#include "2s2h/GameInteractor/GameInteractor.h"

#include <algorithm>
#include <cstring>

extern "C" {
#include "functions.h"
#include "variables.h"
#include "z64horse.h"
}

namespace coop::client {

namespace {

void SetWeekEvent(uint16_t flag) {
    gSaveContext.save.saveInfo.weekEventReg[flag >> 8] |= (uint8_t)(flag & 0xFF);
}

// Sram_InitNewSave plus what Sram_OpenSave clears when a save is loaded (nothing of another game stays).
void FreshSave() {
    Sram_InitNewSave();
    std::memcpy(gSaveContext.save.saveInfo.playerData.newf, "ZELDA3", 6);
    std::memset(gSaveContext.eventInf, 0, sizeof(gSaveContext.eventInf));
    std::memset(gSaveContext.cycleSceneFlags, 0, sizeof(gSaveContext.cycleSceneFlags));
    std::memset(gSaveContext.masksGivenOnMoon, 0, sizeof(gSaveContext.masksGivenOnMoon));
    for (int i = 0; i < TIMER_ID_MAX; i++) {
        gSaveContext.timerStates[i] = TIMER_STATE_OFF;
        gSaveContext.timerCurTimes[i] = SECONDS_TO_TIMER(0);
        gSaveContext.timerTimeLimits[i] = SECONDS_TO_TIMER(0);
        gSaveContext.timerStartOsTimes[i] = 0;
        gSaveContext.timerStopTimes[i] = SECONDS_TO_TIMER(0);
        gSaveContext.timerPausedOsTimes[i] = 0;
    }
    gSaveContext.powderKegTimer = 0;
    gSaveContext.unk_1014 = 0;
    gSaveContext.jinxTimer = 0;
    gSaveContext.rupeeAccumulator = 0;
    gSaveContext.healthAccumulator = 0;
    gHorseIsMounted = false; // as Sram_ResetSaveFromMoonCrash: nobody arrives riding Epona
    D_801BDAA0 = true;
}

void EquipOnC(int button, u8 item) {
    if (INV_CONTENT(item) != item) {
        return;
    }
    BUTTON_ITEM_EQUIP(0, button) = item;
    C_SLOT_EQUIP(0, button) = SLOT(item);
}

// The game's own alphabet (as DeveloperTools/SaveEditor.cpp); anything else becomes a space.
void SetPlayerName(const std::string& nick) {
    char* name = gSaveContext.save.saveInfo.playerData.playerName;
    for (int i = 0; i < 8; i++) {
        char c = i < (int)nick.size() ? nick[i] : ' ';
        u8 glyph = 0x3E;
        if (c >= '0' && c <= '9') {
            glyph = (u8)(c - '0');
        } else if (c >= 'A' && c <= 'Z') {
            glyph = (u8)(0x0A + c - 'A');
        } else if (c >= 'a' && c <= 'z') {
            glyph = (u8)(0x24 + c - 'a');
        }
        name[i] = (char)glyph;
    }
}

int MaxMagic() {
    const SavePlayerData& player = gSaveContext.save.saveInfo.playerData;
    return !player.isMagicAcquired ? 0 : (player.isDoubleMagicAcquired ? MAGIC_DOUBLE_METER : MAGIC_NORMAL_METER);
}

} // namespace

void SaveBuilder_NewWorld() {
    Rand_Seed((u32)osGetTime()); // lottery, Bombers and Spider House codes: different in every world
    FreshSave();
    // What SkipIntroSequence + SkipFirstCycle leave (Enhancements/Cutscenes/SkipIntroSequence.cpp): human Link in
    // Clock Town with Tatl, the Ocarina, the Deku Mask, the Songs of Time and Healing, and magic.
    gSaveContext.save.isFirstCycle = true;
    gSaveContext.save.hasTatl = true;
    gSaveContext.cycleSceneFlags[SCENE_INSIDETOWER].switch0 |= (1 << 0);
    gSaveContext.cycleSceneFlags[SCENE_OPENINGDAN].switch0 |= (1 << 2) | (1 << 0);
    gSaveContext.cycleSceneFlags[SCENE_OPENINGDAN].chest |= (1 << 0);
    gSaveContext.cycleSceneFlags[SCENE_YOUSEI_IZUMI].switch0 |= (1 << 10);
    INV_CONTENT(ITEM_DEKU_NUT) = ITEM_DEKU_NUT; // with no nuts left, as after the first cycle
    INV_CONTENT(ITEM_OCARINA_OF_TIME) = ITEM_OCARINA_OF_TIME;
    INV_CONTENT(ITEM_MASK_DEKU) = ITEM_MASK_DEKU;
    SavePlayerData& player = gSaveContext.save.saveInfo.playerData;
    player.isMagicAcquired = true;
    player.threeDayResetCount = 1;
    gSaveContext.save.saveInfo.inventory.questItems |= (1 << QUEST_SONG_TIME) | (1 << QUEST_SONG_HEALING);
    SetWeekEvent(WEEKEVENTREG_59_04);
    SetWeekEvent(WEEKEVENTREG_31_04);
    SetWeekEvent(WEEKEVENTREG_ENTERED_EAST_CLOCK_TOWN);
    SetWeekEvent(WEEKEVENTREG_ENTERED_WEST_CLOCK_TOWN);
    SetWeekEvent(WEEKEVENTREG_ENTERED_NORTH_CLOCK_TOWN);
}

void SaveBuilder_LoadWorld(const json& fields) {
    FreshSave();
    fields::WriteAllJson(fields, nullptr);
}

void SaveBuilder_LoadPlayer(const json& inv) {
    SavePlayerData& player = gSaveContext.save.saveInfo.playerData;
    int bottles = fields::BottleCount(); // the world's: this player's data may predate a bottle someone found
    const json* own = nullptr;
    if (inv.is_object() && GetInt(inv, "v") == 1) {
        auto it = inv.find("fields");
        own = (it != inv.end() && it->is_object()) ? &*it : nullptr;
    }
    if (own != nullptr) {
        fields::WritePlayer(*own);
    } else {
        // A new player: human, full health and magic, the sword on B, the Ocarina and the Deku Mask on C
        gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
        gSaveContext.save.equippedMask = PLAYER_MASK_NONE;
        player.health = player.healthCapacity;
        player.magic = (s8)MaxMagic();
        EquipOnC(EQUIP_SLOT_C_DOWN, ITEM_OCARINA_OF_TIME);
        EquipOnC(EQUIP_SLOT_C_LEFT, ITEM_MASK_DEKU);
    }
    fields::SetBottleCount(bottles);
    if (player.health <= 0) {
        player.health = 0x30;
    }
    player.health = std::min(player.health, player.healthCapacity);
    player.magic = (s8)std::clamp<int>(player.magic, 0, MaxMagic());
    player.magicLevel = 0; // the game rebuilds the magic meter, as when a file is loaded
    fields::SyncSwordButton();
    if (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_RAZOR && player.swordHealth == 0) {
        player.swordHealth = 100;
    }
}

void SaveBuilder_SetClock(const json& serverClock) {
    int64_t limit = clock::kMoonAbs - clock::kClientMoonMargin;
    uint32_t abs = (uint32_t)std::clamp<int64_t>(GetInt(serverClock, "abs", 0), 0, limit);
    gSaveContext.save.day = clock::DayOfAbs(abs);
    gSaveContext.save.eventDayCount = gSaveContext.save.day;
    gSaveContext.save.time = clock::TimeOfAbs(abs);
    gSaveContext.save.isNight = clock::IsNight(gSaveContext.save.time);
    gSaveContext.save.timeSpeedOffset = GetBool(serverClock, "inv") ? -2 : 0;
}

void SaveBuilder_PrepareStart(const WarpTarget* spot, const std::string& nick) {
    SetPlayerName(nick);
    // What FileSelect_LoadGame sets before Play_Init
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.fileNum = 0xFF; // no file behind this save: nothing reaches the player's saves
    gSaveContext.save.cutsceneIndex = 0;
    gSaveContext.nextCutsceneIndex = 0;
    gSaveContext.seqId = NA_BGM_DISABLED;
    gSaveContext.ambienceId = AMBIENCE_ID_DISABLED;
    gSaveContext.showTitleCard = true;
    gSaveContext.dogParams = 0;
    gSaveContext.prevHudVisibility = HUD_VISIBILITY_ALL;
    gSaveContext.nayrusLoveTimer = 0;
    gSaveContext.healthAccumulator = 0;
    gSaveContext.magicFlag = 0;
    gSaveContext.forcedSeqId = 0;
    gSaveContext.skyboxTime = CLOCK_TIME(0, 0);
    gSaveContext.nextTransitionType = TRANS_NEXT_TYPE_DEFAULT;
    gSaveContext.cutsceneTrigger = 0;
    gSaveContext.chamberCutsceneNum = 0;
    gSaveContext.nextDayTime = NEXT_TIME_NONE;
    gSaveContext.retainWeatherMode = false;
    for (int btn = EQUIP_SLOT_B; btn <= EQUIP_SLOT_A; btn++) {
        gSaveContext.buttonStatus[btn] = BTN_ENABLED;
    }
    for (int btn = EQUIP_SLOT_D_RIGHT; btn <= EQUIP_SLOT_D_UP; btn++) {
        gSaveContext.shipSaveContext.dpad.status[btn] = BTN_ENABLED;
    }
    gSaveContext.hudVisibilityForceButtonAlphasByStatus = false;
    gSaveContext.nextHudVisibility = HUD_VISIBILITY_IDLE;
    gSaveContext.hudVisibility = HUD_VISIBILITY_IDLE;
    gSaveContext.hudVisibilityTimer = 0;
    gSaveContext.save.saveInfo.playerData.tatlTimer = 0;
    gWeatherMode = WEATHER_MODE_CLEAR;

    // 2Ship's per-save setup (playtime, randomizer off...). Before the respawn data: the "remember save location"
    // hook of SavingEnhancements copies the save's (empty) respawn data over it.
    GameInteractor_ExecuteOnSaveLoad(gSaveContext.fileNum);
    gSaveContext.respawn[RESPAWN_MODE_GORON].entrance = 0xFF;
    gSaveContext.respawn[RESPAWN_MODE_ZORA].entrance = 0xFF;
    gSaveContext.respawn[RESPAWN_MODE_DEKU].entrance = 0xFF;
    gSaveContext.respawn[RESPAWN_MODE_HUMAN].entrance = 0xFF;

    if (spot != nullptr) {
        gSaveContext.save.entrance = Entrance_Create(spot->entrance >> 9, 0, 0);
        Warp_SetRespawn(*spot);
    } else {
        gSaveContext.save.entrance = ENTRANCE(SOUTH_CLOCK_TOWN, 0);
        gSaveContext.respawnFlag = 0;
        gSaveContext.respawn[RESPAWN_MODE_DOWN].entrance = ENTR_LOAD_OPENING;
    }
}

void SaveBuilder_EndOfCycleOnCopy(const std::function<void()>& readResult) {
    static SaveContext sBackup; // too big for the stack
    PlayState* play = gPlayState;
    ActorContextSceneFlags sceneFlags = play->actorCtx.sceneFlags;
    sBackup = gSaveContext;
    // The Song of Time "yes" (z_message.c) and the Dawn of the First Day screen (z_daytelop.c)
    Sram_SaveEndOfCycle(play);
    Sram_ClearFlagsAtDawnOfTheFirstDay();
    Sram_IncrementDay();
    readResult();
    gSaveContext = sBackup;
    play->actorCtx.sceneFlags = sceneFlags;
    SaveBuilder_RefreshButtons(); // the rules reloaded the icons of the buttons they changed
}

void SaveBuilder_RefreshButtons() {
    if (fields::PlayLive()) {
        fields::LoadButtonIcons();
    }
}

void SaveBuilder_ResetForFileSelect() {
    // As SkipToFileSelect.cpp: an empty save, then the file select takes over
    Sram_InitNewSave();
    gSaveContext.save.time = CLOCK_TIME(8, 0);
    gSaveContext.save.day = 1;
    gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
    gSaveContext.gameMode = GAMEMODE_FILE_SELECT;
    gSaveContext.seqId = NA_BGM_DISABLED;
    gSaveContext.ambienceId = AMBIENCE_ID_DISABLED;
    gSaveContext.respawnFlag = 0;
}

} // namespace coop::client
