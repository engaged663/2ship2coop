// Game side of the admin commands: "unlock_all" fills the inventory with every item, mask, song, upgrade and
// heart there is (the pieces of a fresh save stay: this is a test/admin gift, not a world change); "give" adds
// rupees to the wallet and puts whatever does not fit in the bank.
#include "2s2h/Coop/Activities/Activities.h"
#include "2s2h/Coop/Chat/ChatModel.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Puppet/PoseCapture.h"

#include "common/Protocol.h"

#include <algorithm>

extern "C" {
#include "functions.h"
#include "variables.h"
}

using coop::client::Chat_Add;
using coop::client::ChatKind;

namespace {

constexpr int kMaxWalletUpgrade = 3;   // giant wallet: 500 rupees
constexpr s16 kMaxMagic = 2;           // double magic

void GiveHeartContainers(int count) {
    s16 cap = (s16)(count * 16);
    gSaveContext.save.saveInfo.playerData.healthCapacity = cap;
    gSaveContext.save.saveInfo.playerData.health = cap;
}

void UnlockAll() {
    Inventory* inv = &gSaveContext.save.saveInfo.inventory;

    // The three transformation masks are items too (the forms follow from them).
    static const u8 kItems[] = {
        ITEM_OCARINA_OF_TIME, ITEM_BOW,      ITEM_BOW_FIRE,   ITEM_BOW_ICE,        ITEM_BOW_LIGHT,
        ITEM_OCARINA_FAIRY,   ITEM_BOMB,     ITEM_BOMBCHU,    ITEM_PICTOGRAPH_BOX, ITEM_LENS_OF_TRUTH,
        ITEM_HOOKSHOT,        ITEM_BOTTLE,   ITEM_BOTTLE,     ITEM_BOTTLE,          ITEM_BOTTLE,
        ITEM_SWORD_GILDED,    ITEM_MOONS_TEAR,
        ITEM_MASK_DEKU,       ITEM_MASK_GORON, ITEM_MASK_ZORA, ITEM_MASK_FIERCE_DEITY,
        ITEM_MASK_TRUTH,      ITEM_MASK_KAFEIS_MASK, ITEM_MASK_ALL_NIGHT, ITEM_MASK_BUNNY,
        ITEM_MASK_KEATON,     ITEM_MASK_GARO,  ITEM_MASK_ROMANI, ITEM_MASK_CIRCUS_LEADER,
        ITEM_MASK_POSTMAN,    ITEM_MASK_COUPLE, ITEM_MASK_GREAT_FAIRY, ITEM_MASK_GIBDO,
        ITEM_MASK_DON_GERO,   ITEM_MASK_KAMARO, ITEM_MASK_CAPTAIN, ITEM_MASK_STONE,
        ITEM_MASK_BREMEN,     ITEM_MASK_BLAST, ITEM_MASK_SCENTS,  ITEM_MASK_GIANT,
        ITEM_BOMBERS_NOTEBOOK,
    };
    std::fill(std::begin(inv->items), std::end(inv->items), ITEM_NONE);
    size_t at = 0;
    for (u8 item : kItems) {
        if (at < ARRAY_COUNT(inv->items)) {
            inv->items[at++] = item;
        }
    }

    // Every song (the quest items are what the ocarina checks; the item slot makes them appear in the menus).
    inv->questItems |= (0xF << QUEST_REMAINS_ODOLWA);        // the four boss remains
    inv->questItems |= gBitFlags[QUEST_SONG_SONATA] | gBitFlags[QUEST_SONG_LULLABY] |
                       gBitFlags[QUEST_SONG_BOSSA_NOVA] | gBitFlags[QUEST_SONG_ELEGY] | gBitFlags[QUEST_SONG_OATH] |
                       gBitFlags[QUEST_SONG_SARIA] | gBitFlags[QUEST_SONG_TIME] | gBitFlags[QUEST_SONG_HEALING] |
                       gBitFlags[QUEST_SONG_EPONA] | gBitFlags[QUEST_SONG_SOARING] | gBitFlags[QUEST_SONG_STORMS] |
                       gBitFlags[QUEST_SONG_SUN] | gBitFlags[QUEST_BOMBERS_NOTEBOOK];
    // 44 heart pieces = 11 containers, plus the 4 the forms give: full hearts.
    inv->questItems = (inv->questItems & ~0xF0000000) | (44 << QUEST_HEART_PIECE_COUNT);

    // Upgrades: biggest quiver, bomb bag, wallet, bullet bag, sticks and nuts.
    inv->upgrades |= (3 << (UPG_QUIVER * 3)) | (3 << (UPG_BOMB_BAG * 3)) | (3 << (UPG_WALLET * 3)) |
                     (2 << (UPG_BULLET_BAG * 3)) | (3 << (UPG_DEKU_STICKS * 3)) | (3 << (UPG_DEKU_NUTS * 3));

    // Ammo full, magic full, double defense, every heart.
    inv->ammo[SLOT(ITEM_BOW)] = 50;
    inv->ammo[SLOT(ITEM_BOMB)] = 40;
    inv->ammo[SLOT(ITEM_BOMBCHU)] = 40;
    inv->ammo[SLOT(ITEM_DEKU_STICK)] = 30;
    inv->ammo[SLOT(ITEM_DEKU_NUT)] = 40;
    gSaveContext.save.saveInfo.playerData.isDoubleMagicAcquired = true;
    gSaveContext.save.saveInfo.playerData.magicLevel = kMaxMagic;
    gSaveContext.save.saveInfo.playerData.magic = MAGIC_DOUBLE_METER;
    gSaveContext.save.saveInfo.playerData.doubleDefense = true;
    GiveHeartContainers(20);

    // Equipped: the best sword, the mirror shield (a great fairy gives it without the flag otherwise).
    SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_GILDED);
    SET_EQUIP_VALUE(EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_MIRROR);

    // Epona and the scarecrow's spawn song: the songs already work, these two need their flags.
    gSaveContext.save.saveInfo.scarecrowSpawnSongSet = true;
    gSaveContext.save.saveInfo.weekEventReg[91] |= 0x01; // hasEpona
}

void OnUnlockAll(const coop::json&) {
    if (!PoseCapture_InGameplay()) {
        Chat_Add(ChatKind::Error, "No puedes recibir los objetos fuera de una partida.");
        return;
    }
    UnlockAll();
    Chat_Add(ChatKind::Ok, "Un administrador te ha desbloqueado todos los objetos, canciones y corazones.");
}

// Rupees to the wallet; whatever does not fit goes to the bank (retirable at the Clock Town bank).
void OnGive(const coop::json& ev) {
    int amount = (int)coop::GetInt(ev, "amount");
    if (!PoseCapture_InGameplay() || amount <= 0) {
        return;
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

} // namespace

COOP_ON_EVENT(adminUnlockAll, coop::ev::kUnlockAll, OnUnlockAll);
COOP_ON_EVENT(adminGive, coop::ev::kGive, OnGive);
