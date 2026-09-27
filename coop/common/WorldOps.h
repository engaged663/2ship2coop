#pragma once
// Changes to the shared world, as sent in "wops" events. The same code merges them on the server and in the
// game, and computes them in the game by comparing each field with its last synced copy:
//   {"bits":[[field, byte, set, clear]...], "bytes":[[field, byte, value]...], "adds":[[field, byte, delta]...]}
#include "Events.h"
#include "WorldFields.h"

#include <cstdint>
#include <string>
#include <vector>

namespace coop::world {

struct BitsOp {
    uint16_t field;
    uint16_t offset;
    uint8_t set;
    uint8_t clear;
};

struct ByteOp {
    uint16_t field;
    uint16_t offset;
    uint8_t value;
};

struct AddOp {
    uint16_t field;
    uint16_t offset;
    int32_t delta;
};

struct Ops {
    std::vector<BitsOp> bits;
    std::vector<ByteOp> bytes;
    std::vector<AddOp> adds;

    bool Empty() const {
        return bits.empty() && bytes.empty() && adds.empty();
    }
    size_t Count() const {
        return bits.size() + bytes.size() + adds.size();
    }
};

// Appends the ops that turn `before` into `after` (both kFields[field].size bytes).
void Diff(uint16_t field, const uint8_t* before, const uint8_t* after, Ops& out);

// Apply one op to a field's bytes. False for an op that does not fit the schema (unknown field, offset out of
// range, wrong kind, misaligned counter, absurd delta): callers count it as an invalid packet.
// `changed` tells whether the bytes changed; `applied` is the delta really added after clamping.
bool ApplyBits(uint8_t* bytes, const BitsOp& op, bool* changed);
bool ApplyByte(uint8_t* bytes, const ByteOp& op, bool* changed);
bool ApplyAdd(uint8_t* bytes, const AddOp& op, int32_t* applied);

json ToJson(const Ops& ops); // only the non-empty lists
// Structure only (lists of small integers); the Apply functions check the schema.
bool FromJson(const json& ev, Ops& out, std::string* err);

// Little-endian counters of the given kind (width from the kind; writes clamp to its range).
int32_t ReadCounter(Kind kind, const uint8_t* at);
void WriteCounter(Kind kind, uint8_t* at, int32_t value);

} // namespace coop::world
