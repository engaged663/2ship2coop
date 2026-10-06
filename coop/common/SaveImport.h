#pragma once
// Base-game saves into the co-op: 2 Ship's saves/fileN.json ("type": "2S2H_SAVE", any version) becomes the shared
// world's fields, one player's own data and the clock, exactly as mm/2s2h/Coop/World/FieldTable.cpp reads that save
// in the game (the layout lives in SaveLayout.h; gCoop.Debug.FieldSelfTest compares both on the live save).
// Used by tools/SaveConvert.cpp (2ship-coop-convert) and the server's /importar. Plain JSON: no game code.
#include "Events.h"
#include "SaveLayout.h"

#include <cstdint>
#include <string>
#include <vector>

namespace coop::save {

// Which part of the file: the owl save (the middle of a cycle) or the save of the cycle's start.
enum class ImportSource { Auto, Owl, Cycle }; // Auto: the owl save when there is one
bool ParseSource(const std::string& text, ImportSource& out); // auto, buho/owl, ciclo/cycle

struct ImportOptions {
    ImportSource source = ImportSource::Auto;
    bool baseline = true;    // add what a co-op world needs to work (ApplyCoopBaseline)
    bool loaderRules = true; // what loading the save in the game changes: a cycle-start save starts as human Link
};

struct Imported {
    FieldBytes fields;       // the world now
    FieldBytes start;        // the world at the start of the cycle (the moon brings it back)
    uint32_t clockAbs = 0;   // coop::clock units: 0 = Day 1, 6:00
    bool inverted = false;   // the Inverted Song of Time was playing
    json inv;                // the player's own data {"v": 1, "fields": {...}} (+ "entrance": where an owl save starts)
    json invStart;           // the same at the start of the cycle
    bool fromOwl = false;    // the owl save was used
    int version = 0;         // the file's save version
    std::string playerName;  // the save's name in ASCII ("Link")
    uint32_t baseline = 0;   // BaselinePart bits the co-op minimum added
    std::vector<std::string> notes; // translated warnings (a newer save version...)
};

// False with err (translated) when the file is not a save the co-op can use.
bool ImportSave(const json& file, const ImportOptions& opts, Imported& out, std::string* err);

std::vector<std::string> DescribeImport(const Imported& imported); // translated summary lines for people
std::string DecodePlayerName(const json& glyphs); // the game's alphabet -> ASCII (unknown glyphs = spaces), trimmed
json FieldsToJson(const FieldBytes& fields);      // {"name": "hex"} in coop::world::kFields order

} // namespace coop::save
