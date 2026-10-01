#include "WorldStore.h"

#include "common/Hex.h"
#include "common/I18n.h"

namespace coop::server {

using namespace coop::world;

namespace {

json FieldsToJson(const FieldSet& fields) {
    json out = json::object();
    for (size_t i = 0; i < kFieldCount && i < fields.size(); i++) {
        out[kFields[i].name] = ToHex(fields[i]);
    }
    return out;
}

} // namespace

FieldSet WorldStore::EmptyFields() {
    FieldSet fields;
    for (const FieldDef& def : kFields) {
        fields.emplace_back(def.size, (uint8_t)0);
    }
    return fields;
}

void WorldStore::StartCycle(const FieldSet& fields, int cycle) {
    mFields = fields;
    mStart = fields;
    mCycle = cycle;
    mExists = true;
}

void WorldStore::RestoreCycleStart(int newCycle) {
    mFields = mStart;
    mCycle = newCycle;
}

bool WorldStore::Apply(const Ops& ops, Ops& relay, Ops& corrections, int* invalid) {
    int bad = 0;
    bool changed = false;
    for (const BitsOp& op : ops.bits) {
        bool opChanged = false;
        if (op.field >= mFields.size() || !ApplyBits(mFields[op.field].data(), op, &opChanged)) {
            bad++;
        } else if (opChanged) {
            relay.bits.push_back(op);
            changed = true;
        }
    }
    for (const ByteOp& op : ops.bytes) {
        bool opChanged = false;
        if (op.field >= mFields.size() || !ApplyByte(mFields[op.field].data(), op, &opChanged)) {
            bad++;
        } else if (opChanged) {
            relay.bytes.push_back(op);
            changed = true;
        }
    }
    for (const AddOp& op : ops.adds) {
        int32_t applied = 0;
        if (op.field >= mFields.size() || !ApplyAdd(mFields[op.field].data(), op, &applied)) {
            bad++;
            continue;
        }
        if (applied != 0) {
            relay.adds.push_back({ op.field, op.offset, applied });
            changed = true;
        }
        if (applied != op.delta) {
            corrections.adds.push_back({ op.field, op.offset, applied - op.delta });
        }
    }
    if (invalid != nullptr) {
        *invalid = bad;
    }
    return changed;
}

json WorldStore::FieldsJson() const {
    return FieldsToJson(mFields);
}

bool WorldStore::ParseFields(const json& fields, FieldSet& out, std::string* err) {
    auto fail = [err](const std::string& why) {
        if (err != nullptr) {
            *err = why;
        }
        return false;
    };
    if (!fields.is_object()) {
        return fail(Tr(Msg::WsFieldsNotObject));
    }
    FieldSet parsed;
    for (const FieldDef& def : kFields) {
        auto it = fields.find(def.name);
        if (it == fields.end() || !it->is_string()) {
            return fail(Tr(Msg::WsMissingField, { def.name }));
        }
        std::vector<uint8_t> bytes;
        if (!FromHex(it->get<std::string>(), bytes)) {
            return fail(Tr(Msg::WsNotHex, { def.name }));
        }
        if (bytes.size() != def.size) {
            return fail(Tr(Msg::WsBadSize, { def.name, std::to_string(def.size) }));
        }
        parsed.push_back(std::move(bytes));
    }
    out = std::move(parsed);
    return true;
}

json WorldStore::ToJson() const {
    return { { "version", 1 }, { "cycle", mCycle }, { "fields", FieldsToJson(mFields) },
             { "start", FieldsToJson(mStart) } };
}

bool WorldStore::FromJson(const json& saved, std::string* err) {
    auto fail = [err](const std::string& why) {
        if (err != nullptr) {
            *err = why;
        }
        return false;
    };
    if (!saved.is_object()) {
        return fail(Tr(Msg::NotAnObject));
    }
    int64_t cycle = GetInt(saved, "cycle", 0);
    if (cycle < 1 || cycle > 1000000) {
        return fail(Tr(Msg::WsBadCycle));
    }
    auto fields = saved.find("fields");
    if (fields == saved.end()) {
        return fail(Tr(Msg::MissingFields));
    }
    FieldSet current;
    if (!ParseFields(*fields, current, err)) {
        return false;
    }
    FieldSet start;
    auto startJson = saved.find("start");
    if (startJson == saved.end() || !ParseFields(*startJson, start, nullptr)) {
        start = current; // missing or damaged copy: the moon will bring back the current world
    }
    mFields = std::move(current);
    mStart = std::move(start);
    mCycle = (int)cycle;
    mExists = true;
    return true;
}

} // namespace coop::server
