// [COOP] See Sync.h: a sound id of this game. The banks' sizes come from the game's own tables (sfx_params.c, in bank
// order): an id past them would read outside them.
#include "Sync.h"

#include <iterator>

namespace coop::client {

namespace {

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
static_assert(std::size(kSfxCount) == (size_t)sound_limits::kBanks, "SoundEntry.h: one count per bank");

} // namespace

bool Sfx_Valid(uint16_t sfxId) {
    uint32_t bank = SFX_BANK(sfxId);
    return bank < std::size(kSfxCount) && (sfxId & 0xC00) == SFX_FLAG && (int)SFX_INDEX(sfxId) < kSfxCount[bank];
}

} // namespace coop::client
