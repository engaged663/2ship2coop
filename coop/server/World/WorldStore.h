#pragma once
// The shared world's bytes (schema: common/WorldFields.h), the copy taken when the cycle started (the moon
// restores it) and their world.json form. The clock lives in WorldClock; SharedWorld ties them together.
#include "common/Events.h"
#include "common/SaveLayout.h"
#include "common/WorldOps.h"

#include <cstdint>
#include <string>
#include <vector>

namespace coop::server {

using FieldSet = save::FieldBytes; // one entry per kFields, with its exact size

class WorldStore {
  public:
    bool Exists() const {
        return mExists;
    }
    int Cycle() const {
        return mCycle;
    }
    const FieldSet& Fields() const {
        return mFields;
    }
    const FieldSet& Start() const {
        return mStart;
    }

    // A cycle starts with these fields (world created, Song of Time): they are also the cycle-start copy.
    void StartCycle(const FieldSet& fields, int cycle);
    // The moon: back to the cycle-start copy, now numbered newCycle.
    void RestoreCycleStart(int newCycle);
    // A whole other world (an import, a restored backup): its fields, its cycle-start copy and its number.
    void Replace(const FieldSet& fields, const FieldSet& start, int cycle);
    // Applies a game's ops. relay gets the ops that changed something (adds with the delta really added),
    // corrections the adds the sender must undo locally (applied - requested, after clamping).
    // *invalid = ops that do not fit the schema (skipped). True if the world changed.
    bool Apply(const world::Ops& ops, world::Ops& relay, world::Ops& corrections, int* invalid);

    json FieldsJson() const; // {"weekEventReg": "hex", ...}
    // Every field of the schema, as hex of its exact size; err names the first bad field (what games send).
    static bool ParseFields(const json& fields, FieldSet& out, std::string* err);
    // What a file holds: a world saved by another version of the server still loads. A missing field starts at
    // zeros, a field of another size is cut or padded with zeros, an unknown one is dropped (each case adds a
    // translated warning); only a value that is not hex fails.
    static bool ParseFieldsLenient(const json& fields, FieldSet& out, std::vector<std::string>* warnings,
                                   std::string* err);
    static FieldSet EmptyFields();

    json ToJson() const; // world.json without the clock (WorldImage.h adds it)
    bool FromJson(const json& saved, std::string* err, std::vector<std::string>* warnings = nullptr); // lenient

  private:
    bool mExists = false;
    int mCycle = 0;
    FieldSet mFields;
    FieldSet mStart;
};

} // namespace coop::server
