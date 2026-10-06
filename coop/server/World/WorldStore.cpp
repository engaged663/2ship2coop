#include "WorldStore.h"

#include "common/Hex.h"
#include "common/I18n.h"
#include "common/SaveImport.h"

namespace coop::server {

using namespace coop::world;
using save::FieldsToJson;

FieldSet WorldStore::EmptyFields() {
    return save::EmptyFields();
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

void WorldStore::Replace(const FieldSet& fields, const FieldSet& start, int cycle) {
    mFields = fields;
    mStart = start;
    mCycle = cycle;
    mExists = true;
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

bool WorldStore::ParseFieldsLenient(const json& fields, FieldSet& out, std::vector<std::string>* warnings,
                                    std::string* err) {
    auto warn = [warnings](const std::string& text) {
        if (warnings != nullptr) {
            warnings->push_back(text);
        }
    };
    if (!fields.is_object()) {
        if (err != nullptr) {
            *err = Tr(Msg::WsFieldsNotObject);
        }
        return false;
    }
    FieldSet parsed;
    for (const FieldDef& def : kFields) {
        std::vector<uint8_t> bytes;
        auto it = fields.find(def.name);
        if (it == fields.end()) {
            warn(Tr(Msg::WsFieldAdded, { def.name }));
            bytes.assign(def.size, 0);
        } else if (!it->is_string() || !FromHex(it->get<std::string>(), bytes)) {
            if (err != nullptr) {
                *err = Tr(Msg::WsNotHex, { def.name });
            }
            return false;
        } else if (bytes.size() != def.size) {
            warn(Tr(Msg::WsFieldResized, { def.name, std::to_string(bytes.size()), std::to_string(def.size) }));
            bytes.resize(def.size, 0);
        }
        parsed.push_back(std::move(bytes));
    }
    for (auto it = fields.begin(); it != fields.end(); ++it) {
        if (FindField(it.key().c_str()) < 0) {
            warn(Tr(Msg::WsFieldUnknown, { it.key() }));
        }
    }
    out = std::move(parsed);
    return true;
}

json WorldStore::ToJson() const {
    return { { "version", 2 }, { "cycle", mCycle }, { "fields", FieldsToJson(mFields) },
             { "start", FieldsToJson(mStart) } };
}

bool WorldStore::FromJson(const json& saved, std::string* err, std::vector<std::string>* warnings) {
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
    if (!ParseFieldsLenient(*fields, current, warnings, err)) {
        return false;
    }
    FieldSet start;
    auto startJson = saved.find("start");
    if (startJson == saved.end() || !ParseFieldsLenient(*startJson, start, nullptr, nullptr)) {
        start = current; // missing or damaged copy: the moon will bring back the current world
    }
    mFields = std::move(current);
    mStart = std::move(start);
    mCycle = (int)cycle;
    mExists = true;
    return true;
}

} // namespace coop::server
