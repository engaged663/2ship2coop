// [COOP] See MessageVars.h.
#include "MessageVars.h"

#include "TalkSync.h"
#include "2s2h/Coop/Actors/CoopEngine.h"

#include "common/Hex.h"
#include "common/Protocol.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <vector>

extern "C" {
#include "functions.h"
#include "variables.h"
}

namespace coop::client {

namespace {

// What Message_Decode reads from this player's save and text state (z_message.c, z_message_nes.c): the name, the
// minigame's points and clocks, the amounts of a bank or bet conversation, the records, the codes and the Deku
// playground's names and times.
struct Vars {
    char playerName[8];
    u16 minigameScore;
    u16 minigameHiddenScore;
    u64 timerCurTimes[TIMER_ID_MAX];
    s32 rupeesSelected;
    s32 rupeesTotal;
    u32 highScores[HS_MAX];
    u32 dekuPlaygroundHighScores[3];
    s8 lotteryCodes[3][3];
    s8 spiderHouseMaskOrder[6];
    s8 bomberCode[5];
    char dekuNames[3][8];
};
static_assert(sizeof(Vars) * 2 <= kMaxTalkVarsHex, "MessageVars travel in talk.vars");
static_assert(sizeof(((SaveInfo*)nullptr)->lotteryCodes) == sizeof(((Vars*)nullptr)->lotteryCodes) &&
                  sizeof(((SaveInfo*)nullptr)->spiderHouseMaskOrder) == sizeof(((Vars*)nullptr)->spiderHouseMaskOrder) &&
                  sizeof(((SaveInfo*)nullptr)->bomberCode) == sizeof(((Vars*)nullptr)->bomberCode) &&
                  sizeof(((SaveInfo*)nullptr)->dekuPlaygroundHighScores) ==
                      sizeof(((Vars*)nullptr)->dekuPlaygroundHighScores) &&
                  sizeof(((SaveInfo*)nullptr)->inventory.dekuPlaygroundPlayerName) == sizeof(((Vars*)nullptr)->dekuNames),
              "MessageVars mirrors the save's fields");

constexpr char kNameSpace = 0x3E; // the name's "blank" (its characters are 0x00..0x40)
constexpr char kNameLast = 0x40;

Vars sTheirs;
bool sHave = false;
Vars sOurs;
bool sSwapped = false;

Vars Read() {
    Vars v;
    std::memset(&v, 0, sizeof(v));
    SaveInfo& info = gSaveContext.save.saveInfo;
    std::memcpy(v.playerName, info.playerData.playerName, sizeof(v.playerName));
    v.minigameScore = gSaveContext.minigameScore;
    v.minigameHiddenScore = gSaveContext.minigameHiddenScore;
    std::memcpy(v.timerCurTimes, gSaveContext.timerCurTimes, sizeof(v.timerCurTimes));
    if (gPlayState != nullptr) {
        v.rupeesSelected = gPlayState->msgCtx.rupeesSelected;
        v.rupeesTotal = gPlayState->msgCtx.rupeesTotal;
    }
    std::memcpy(v.highScores, info.highScores, sizeof(v.highScores));
    std::memcpy(v.dekuPlaygroundHighScores, info.dekuPlaygroundHighScores, sizeof(v.dekuPlaygroundHighScores));
    std::memcpy(v.lotteryCodes, info.lotteryCodes, sizeof(v.lotteryCodes));
    std::memcpy(v.spiderHouseMaskOrder, info.spiderHouseMaskOrder, sizeof(v.spiderHouseMaskOrder));
    std::memcpy(v.bomberCode, info.bomberCode, sizeof(v.bomberCode));
    std::memcpy(v.dekuNames, info.inventory.dekuPlaygroundPlayerName, sizeof(v.dekuNames));
    return v;
}

void Write(const Vars& v) {
    SaveInfo& info = gSaveContext.save.saveInfo;
    std::memcpy(info.playerData.playerName, v.playerName, sizeof(v.playerName));
    gSaveContext.minigameScore = v.minigameScore;
    gSaveContext.minigameHiddenScore = v.minigameHiddenScore;
    std::memcpy(gSaveContext.timerCurTimes, v.timerCurTimes, sizeof(v.timerCurTimes));
    if (gPlayState != nullptr) {
        gPlayState->msgCtx.rupeesSelected = v.rupeesSelected;
        gPlayState->msgCtx.rupeesTotal = v.rupeesTotal;
    }
    std::memcpy(info.highScores, v.highScores, sizeof(v.highScores));
    std::memcpy(info.dekuPlaygroundHighScores, v.dekuPlaygroundHighScores, sizeof(v.dekuPlaygroundHighScores));
    std::memcpy(info.lotteryCodes, v.lotteryCodes, sizeof(v.lotteryCodes));
    std::memcpy(info.spiderHouseMaskOrder, v.spiderHouseMaskOrder, sizeof(v.spiderHouseMaskOrder));
    std::memcpy(info.bomberCode, v.bomberCode, sizeof(v.bomberCode));
    std::memcpy(info.inventory.dekuPlaygroundPlayerName, v.dekuNames, sizeof(v.dekuNames));
}

// The text code uses these as indices (a letter of the name, a digit, a mask): keep them in range whatever arrives.
void Sanitize(Vars& v) {
    auto letter = [](char& c) {
        if (c < 0 || c > kNameLast) {
            c = kNameSpace;
        }
    };
    for (char& c : v.playerName) {
        letter(c);
    }
    for (auto& name : v.dekuNames) {
        for (char& c : name) {
            letter(c);
        }
    }
    for (auto& code : v.lotteryCodes) {
        for (s8& d : code) {
            d = std::clamp<s8>(d, 0, 9);
        }
    }
    for (s8& m : v.spiderHouseMaskOrder) {
        m = std::clamp<s8>(m, 0, 3);
    }
    for (s8& d : v.bomberCode) {
        d = std::clamp<s8>(d, 0, 9);
    }
    v.rupeesSelected = std::clamp<s32>(v.rupeesSelected, 0, 9999);
    v.rupeesTotal = std::clamp<s32>(v.rupeesTotal, 0, 9999);
}

} // namespace

std::string MessageVars_Capture() {
    Vars v = Read();
    return ToHex((const uint8_t*)&v, sizeof(v));
}

bool MessageVars_Set(const std::string& hex) {
    std::vector<uint8_t> bytes;
    if (hex.empty() || !FromHex(hex, bytes) || bytes.size() != sizeof(Vars)) {
        sHave = false;
        return false;
    }
    std::memcpy(&sTheirs, bytes.data(), sizeof(Vars));
    Sanitize(sTheirs);
    sHave = true;
    return true;
}

void MessageVars_Clear() {
    sHave = false;
}

} // namespace coop::client

using namespace coop::client;

extern "C" void Coop_OnMessageDecode(PlayState* play, s32 begin) {
    if (begin) {
        if (!sHave || sSwapped || play == nullptr || !TalkSync_Mirroring() || play->msgCtx.talkActor != nullptr ||
            play->msgCtx.currentTextId != TalkSync_MirroredTextId()) {
            return;
        }
        sOurs = Read();
        Write(sTheirs);
        sSwapped = true;
    } else if (sSwapped) {
        Write(sOurs);
        sSwapped = false;
    }
}
