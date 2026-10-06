// Game side of the admin commands, only while playing in the server's world (never on the player's own saves):
// "unlock_all" gives every item, mask, song and heart with the engine's own rules (each item in its own slot, the
// upgrades through Inventory_ChangeUpgrade...), so the shared part reaches the server's world through WorldSync like any
// other change and this player's part (ammo, health, magic) through an upload the server saves at once; "give" adds
// rupees to the wallet and puts whatever does not fit in the bank.
#include "2s2h/Coop/Activities/Activities.h"
#include "2s2h/Coop/Chat/ChatModel.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Puppet/PoseCapture.h"
#include "2s2h/Coop/World/FieldTable.h"
#include "2s2h/Coop/World/SaveBuilder.h"
#include "2s2h/Coop/World/WorldSession.h"

#include "common/Protocol.h"
#include "common/SaveLayout.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <algorithm>
#include <chrono>

extern "C" {
#include "functions.h"
#include "variables.h"
void Inventory_ChangeUpgrade(s16 upgrade, u32 value);
}

using coop::client::Chat_Add;
using coop::client::ChatKind;

namespace {

constexpr int64_t kUnlockWaitMs = 10000; // an unlock that arrives in a scene change or a cutscene waits this long

// Every item slot's own item (INV_CONTENT puts it in its slot), but not the trade slots (their item changes with the
// quests) nor the bottles (their contents belong to each player: only empty bottles are added).
constexpr u8 kItems[] = {
    ITEM_OCARINA_OF_TIME, ITEM_BOW,         ITEM_ARROW_FIRE,     ITEM_ARROW_ICE,     ITEM_ARROW_LIGHT,
    ITEM_BOMB,            ITEM_BOMBCHU,     ITEM_DEKU_STICK,     ITEM_DEKU_NUT,      ITEM_MAGIC_BEANS,
    ITEM_POWDER_KEG,      ITEM_PICTOGRAPH_BOX, ITEM_LENS_OF_TRUTH, ITEM_HOOKSHOT,    ITEM_SWORD_GREAT_FAIRY,
};

// What the 100% save of 2 Ship's developer tools has (DeveloperTools.cpp): the four remains, the songs of this game
// (not the unused Saria's and Sun's) and the Bombers' Notebook.
constexpr int kQuestItems[] = {
    QUEST_REMAINS_ODOLWA, QUEST_REMAINS_GOHT, QUEST_REMAINS_GYORG,  QUEST_REMAINS_TWINMOLD, QUEST_SONG_SONATA,
    QUEST_SONG_LULLABY,   QUEST_SONG_BOSSA_NOVA, QUEST_SONG_ELEGY,  QUEST_SONG_OATH,        QUEST_SONG_TIME,
    QUEST_SONG_HEALING,   QUEST_SONG_EPONA,   QUEST_SONG_SOARING,   QUEST_SONG_STORMS,      QUEST_BOMBERS_NOTEBOOK,
};

int64_t sUnlockSinceMs = -1; // an unlock waiting for its moment (-1: none)

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

bool CanUnlockNow() {
    return coop::client::WorldSession_Active() && PoseCapture_InGameplay() && coop::client::fields::PlayLive() &&
           gPlayState->transitionTrigger == TRANS_TRIGGER_OFF;
}

void UnlockAll() {
    Inventory& inv = gSaveContext.save.saveInfo.inventory;
    SavePlayerData& player = gSaveContext.save.saveInfo.playerData;

    for (u8 item : kItems) {
        INV_CONTENT(item) = item;
    }
    for (int item = ITEM_MASK_DEKU; item <= ITEM_MASK_GIANT; item++) { // the 24 masks
        INV_CONTENT(item) = (u8)item;
    }
    for (int slot = SLOT_BOTTLE_1; slot <= SLOT_BOTTLE_6; slot++) {
        if (inv.items[slot] == ITEM_NONE) {
            inv.items[slot] = ITEM_BOTTLE;
        }
    }
    // What the first version of this command left in a world: Deku sticks and nuts past their capacity table, the
    // unused songs of Saria and the Sun (coop/common/SaveLayout.h)
    inv.upgrades = coop::save::RepairUpgrades(inv.upgrades);
    inv.questItems = coop::save::RepairQuestItems(inv.questItems);
    // The biggest quiver, bomb bag and wallet (Majora's Mask has no stick or nut upgrades)
    Inventory_ChangeUpgrade(UPG_QUIVER, 3);
    Inventory_ChangeUpgrade(UPG_BOMB_BAG, 3);
    Inventory_ChangeUpgrade(UPG_WALLET, 2);
    for (int quest : kQuestItems) {
        SET_QUEST_ITEM(quest);
    }
    // 20 hearts and no loose pieces (the game only ever has 0 to 3 of them)
    inv.questItems &= ~0xF0000000u;
    player.healthCapacity = 20 * 0x10;
    player.health = player.healthCapacity;
    // Double magic, full: the game rebuilds the meter when magicLevel is 0 (as FieldTable's WriteMagicFlags does)
    player.isMagicAcquired = true;
    player.isDoubleMagicAcquired = true;
    player.magic = MAGIC_DOUBLE_METER;
    player.magicLevel = 0;
    // Double defense, with its white-bordered hearts
    player.doubleDefense = true;
    inv.defenseHearts = 20;
    // The best sword and shield; B shows the sword (unless a minigame put its item there)
    SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_GILDED);
    SET_EQUIP_VALUE(EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_MIRROR);
    coop::client::fields::SyncSwordButton();
    // Ammo full: each player's own
    AMMO(ITEM_BOW) = CUR_CAPACITY(UPG_QUIVER);
    AMMO(ITEM_BOMB) = AMMO(ITEM_BOMBCHU) = CUR_CAPACITY(UPG_BOMB_BAG);
    AMMO(ITEM_DEKU_STICK) = CUR_CAPACITY(UPG_DEKU_STICKS);
    AMMO(ITEM_DEKU_NUT) = CUR_CAPACITY(UPG_DEKU_NUTS);
    AMMO(ITEM_MAGIC_BEANS) = 20;
    AMMO(ITEM_POWDER_KEG) = 1;
    coop::client::SaveBuilder_RefreshButtons(); // the icons of the buttons whose items changed
}

void RunUnlock() {
    sUnlockSinceMs = -1;
    UnlockAll();
    // Everything above must survive the save: read, written back and read again, nothing may change
    std::string err = coop::client::fields::SelfTest();
    if (!err.empty()) {
        Chat_Add(ChatKind::Warn, "Aviso del desbloqueo: " + err);
    }
    coop::client::WorldSession_UploadNow(true); // ammo, health and magic: on the server's disk now
    Chat_Add(ChatKind::Ok, "Tienes todos los objetos, máscaras, canciones y corazones (lo compartido es para toda la "
                           "partida y ya está guardado en el servidor).");
}

void OnUnlockAll(const coop::json&) {
    if (!coop::client::WorldSession_InWorld()) {
        Chat_Add(ChatKind::Error, "El desbloqueo solo funciona dentro de la partida del servidor.");
        return;
    }
    if (CanUnlockNow()) {
        RunUnlock();
        return;
    }
    sUnlockSinceMs = NowMs(); // in a scene change, a cutscene, still entering: as soon as it can (FrameEnd below)
}

void UnlockWhenReady() {
    if (sUnlockSinceMs < 0) {
        return;
    }
    if (!coop::client::WorldSession_InWorld()) {
        sUnlockSinceMs = -1; // left the world meanwhile
        return;
    }
    if (CanUnlockNow()) {
        RunUnlock();
    } else if (NowMs() - sUnlockSinceMs > kUnlockWaitMs) {
        sUnlockSinceMs = -1;
        Chat_Add(ChatKind::Error, "No se pudo aplicar el desbloqueo (la partida no estaba lista): pídelo otra vez.");
    }
}

// Rupees to the wallet; whatever does not fit goes to the bank (retirable at the Clock Town bank).
void OnGive(const coop::json& ev) {
    int amount = (int)coop::GetInt(ev, "amount");
    if (!coop::client::WorldSession_Active() || !PoseCapture_InGameplay() || amount <= 0) {
        return; // the server only sends it to players in its world; a game that just left keeps its own save intact
    }
    int capacity = CUR_CAPACITY(UPG_WALLET);
    int current = gSaveContext.save.saveInfo.playerData.rupees + gSaveContext.rupeeAccumulator;
    int toWallet = std::clamp(capacity - current, 0, amount);
    int toBank = amount - toWallet;
    if (toWallet > 0) {
        coop::client::Rewards_SuppressBegin(); // /give is not a group prize
        Rupees_ChangeBy((s16)toWallet);
        coop::client::Rewards_SuppressEnd();
    }
    if (toBank > 0) {
        gSaveContext.save.saveInfo.highScores[HS_BANK_RUPEES] += toBank;
    }
    Chat_Add(ChatKind::Ok, "Has recibido " + std::to_string(amount) + " rupias" +
                               (toBank > 0 ? (" (" + std::to_string(toBank) + " al banco).") : "."));
}

void RegisterAdminFeatures() {
    COND_HOOK(OnGameStateMainFinish, true, UnlockWhenReady);
}

} // namespace

COOP_ON_EVENT(adminUnlockAll, coop::ev::kUnlockAll, OnUnlockAll);
COOP_ON_EVENT(adminGive, coop::ev::kGive, OnGive);

static RegisterShipInitFunc sAdminFeaturesInit(RegisterAdminFeatures);
