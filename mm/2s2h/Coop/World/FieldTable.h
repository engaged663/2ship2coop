#pragma once
// Where every field of the shared world (coop/common/WorldFields.h) and of each player's own data lives in the
// game's save (gSaveContext), and what else must follow when one is written (buttons, magic meter, the loaded
// scene...). New world field: add it to kFields and to kAccess in FieldTable.cpp (static_asserts check both agree).
#include "common/Events.h"

#include <cstdint>
#include <string>

namespace coop::client::fields {

// World fields, by index in coop::world::kFields.
void Read(int field, uint8_t* out);       // kFields[field].size bytes
void Write(int field, const uint8_t* in); // into the live save (and the loaded scene / the buttons)
json ReadAllJson();                       // {"name": "hex"...}
bool CheckJson(const json& fields, std::string* err);    // every field there, valid hex, the right size
bool WriteAllJson(const json& fields, std::string* err); // all or nothing

// The player's own data: the opaque "inv" the server keeps for each player.
json ReadPlayer();                     // {"name": "hex"...}
void WritePlayer(const json& fields);  // missing or malformed entries are skipped; values are made safe

bool PlayLive();                      // a scene is loaded and its actors exist (between Actor_InitContext and its end)
int BottleCount();                    // bottles owned: bottle slots in use + bottles Takkuri stole
void SetBottleCount(int count);       // adds empty bottles, or removes bottles (empty ones first)
void RefreshButtonsForSlot(int slot); // C / D-pad buttons that show that slot follow its item
void SyncSwordButton();               // B shows the sword of equips.equipment (unless a minigame put its item there)

// Test aid (gCoop.Debug.FieldSelfTest): reading, writing back and reading again must not change anything.
// "" = OK, otherwise what differed.
std::string SelfTest();

} // namespace coop::client::fields
