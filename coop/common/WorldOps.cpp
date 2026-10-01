#include "WorldOps.h"

#include "I18n.h"

#include <algorithm>
#include <limits>

namespace coop::world {

namespace {

// [offset, offset + width) inside the field and aligned to width.
bool Fits(uint16_t field, uint16_t offset, int width) {
    return field < kFieldCount && width > 0 && offset % width == 0 && offset + width <= kFields[field].size;
}

bool ReadInts(const json& entry, size_t count, int64_t* out) {
    if (!entry.is_array() || entry.size() != count) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (!entry[i].is_number_integer()) {
            return false;
        }
        out[i] = entry[i].get<int64_t>();
    }
    return true;
}

bool IsU16(int64_t v) {
    return v >= 0 && v <= 0xFFFF;
}

bool IsU8(int64_t v) {
    return v >= 0 && v <= 0xFF;
}

bool IsS32(int64_t v) {
    return v >= std::numeric_limits<int32_t>::min() && v <= std::numeric_limits<int32_t>::max();
}

} // namespace

int32_t ReadCounter(Kind kind, const uint8_t* at) {
    switch (kind) {
        case Kind::CounterS8:
            return (int8_t)at[0];
        case Kind::CounterU16:
            return at[0] | (at[1] << 8);
        default:
            return at[0];
    }
}

void WriteCounter(Kind kind, uint8_t* at, int32_t value) {
    int min = 0;
    int max = 0;
    CounterRange(kind, min, max);
    value = std::clamp(value, min, max);
    at[0] = (uint8_t)(value & 0xFF);
    if (kind == Kind::CounterU16) {
        at[1] = (uint8_t)((value >> 8) & 0xFF);
    }
}

void Diff(uint16_t field, const uint8_t* before, const uint8_t* after, Ops& out) {
    const FieldDef& def = kFields[field];
    int width = CounterWidth(def.kind);
    if (width > 0) {
        for (uint16_t o = 0; o + width <= def.size; o = (uint16_t)(o + width)) {
            int32_t delta = ReadCounter(def.kind, after + o) - ReadCounter(def.kind, before + o);
            if (delta != 0) {
                out.adds.push_back({ field, o, delta });
            }
        }
        return;
    }
    for (uint16_t o = 0; o < def.size; o++) {
        if (before[o] == after[o]) {
            continue;
        }
        if (def.kind == Kind::Bits) {
            out.bits.push_back({ field, o, (uint8_t)(after[o] & ~before[o]), (uint8_t)(before[o] & ~after[o]) });
        } else {
            out.bytes.push_back({ field, o, after[o] });
        }
    }
}

bool ApplyBits(uint8_t* bytes, const BitsOp& op, bool* changed) {
    if (!Fits(op.field, op.offset, 1) || kFields[op.field].kind != Kind::Bits) {
        return false;
    }
    uint8_t before = bytes[op.offset];
    bytes[op.offset] = (uint8_t)((before | op.set) & ~op.clear);
    *changed = bytes[op.offset] != before;
    return true;
}

bool ApplyByte(uint8_t* bytes, const ByteOp& op, bool* changed) {
    if (!Fits(op.field, op.offset, 1) || kFields[op.field].kind != Kind::Bytes) {
        return false;
    }
    *changed = bytes[op.offset] != op.value;
    bytes[op.offset] = op.value;
    return true;
}

bool ApplyAdd(uint8_t* bytes, const AddOp& op, int32_t* applied) {
    if (op.field >= kFieldCount) {
        return false;
    }
    Kind kind = kFields[op.field].kind;
    if (!IsCounter(kind) || !Fits(op.field, op.offset, CounterWidth(kind)) || op.delta < -0xFFFF ||
        op.delta > 0xFFFF) {
        return false;
    }
    int32_t before = ReadCounter(kind, bytes + op.offset);
    WriteCounter(kind, bytes + op.offset, before + op.delta);
    *applied = ReadCounter(kind, bytes + op.offset) - before;
    return true;
}

json ToJson(const Ops& ops) {
    json out = json::object();
    if (!ops.bits.empty()) {
        json list = json::array();
        for (const BitsOp& op : ops.bits) {
            list.push_back(json::array({ op.field, op.offset, op.set, op.clear }));
        }
        out["bits"] = std::move(list);
    }
    if (!ops.bytes.empty()) {
        json list = json::array();
        for (const ByteOp& op : ops.bytes) {
            list.push_back(json::array({ op.field, op.offset, op.value }));
        }
        out["bytes"] = std::move(list);
    }
    if (!ops.adds.empty()) {
        json list = json::array();
        for (const AddOp& op : ops.adds) {
            list.push_back(json::array({ op.field, op.offset, op.delta }));
        }
        out["adds"] = std::move(list);
    }
    return out;
}

bool FromJson(const json& ev, Ops& out, std::string* err) {
    auto fail = [err](const std::string& why) {
        if (err != nullptr) {
            *err = why;
        }
        return false;
    };
    if (!ev.is_object()) {
        return fail(Tr(Msg::NotAnObject));
    }
    auto list = [&ev](const char* key) -> const json* {
        auto it = ev.find(key);
        return it == ev.end() ? nullptr : &*it;
    };
    Ops ops;
    int64_t v[4] = {};
    if (const json* bits = list("bits")) {
        if (!bits->is_array()) {
            return fail(Tr(Msg::OpsNotList, { "bits" }));
        }
        for (const json& e : *bits) {
            if (!ReadInts(e, 4, v) || !IsU16(v[0]) || !IsU16(v[1]) || !IsU8(v[2]) || !IsU8(v[3])) {
                return fail(Tr(Msg::OpsBadBits));
            }
            ops.bits.push_back({ (uint16_t)v[0], (uint16_t)v[1], (uint8_t)v[2], (uint8_t)v[3] });
        }
    }
    if (const json* bytes = list("bytes")) {
        if (!bytes->is_array()) {
            return fail(Tr(Msg::OpsNotList, { "bytes" }));
        }
        for (const json& e : *bytes) {
            if (!ReadInts(e, 3, v) || !IsU16(v[0]) || !IsU16(v[1]) || !IsU8(v[2])) {
                return fail(Tr(Msg::OpsBadBytes));
            }
            ops.bytes.push_back({ (uint16_t)v[0], (uint16_t)v[1], (uint8_t)v[2] });
        }
    }
    if (const json* adds = list("adds")) {
        if (!adds->is_array()) {
            return fail(Tr(Msg::OpsNotList, { "adds" }));
        }
        for (const json& e : *adds) {
            if (!ReadInts(e, 3, v) || !IsU16(v[0]) || !IsU16(v[1]) || !IsS32(v[2])) {
                return fail(Tr(Msg::OpsBadAdds));
            }
            ops.adds.push_back({ (uint16_t)v[0], (uint16_t)v[1], (int32_t)v[2] });
        }
    }
    out = std::move(ops);
    return true;
}

} // namespace coop::world
