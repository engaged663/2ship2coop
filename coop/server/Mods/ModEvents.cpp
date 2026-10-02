#include "ModEvents.h"

namespace coop::server {

namespace {

// "name:type[*] what it is|name:type what it is" -> the fields.
std::vector<ModFieldDef> ParseFields(const std::string& spec) {
    std::vector<ModFieldDef> out;
    size_t start = 0;
    while (start < spec.size()) {
        size_t end = spec.find('|', start);
        std::string part = spec.substr(start, end == std::string::npos ? std::string::npos : end - start);
        start = end == std::string::npos ? spec.size() : end + 1;
        size_t colon = part.find(':');
        size_t space = colon == std::string::npos ? std::string::npos : part.find(' ', colon);
        ModFieldDef field;
        if (colon == std::string::npos || space == std::string::npos) {
            field.name = part; // a malformed line shows up in TestModsHost (no type, no description)
            out.push_back(field);
            continue;
        }
        field.name = part.substr(0, colon);
        field.type = part.substr(colon + 1, space - colon - 1);
        if (!field.type.empty() && field.type.back() == '*') {
            field.writable = true;
            field.type.pop_back();
        }
        field.doc = part.substr(space + 1);
        out.push_back(field);
    }
    return out;
}

} // namespace

const std::vector<ModEventDef>& ModEventDefs() {
    static const std::vector<ModEventDef> defs = {
#define X(id, name, cancelable, doc, fields) { ModEvent::id, name, cancelable, doc, ParseFields(fields) },
#include "ModEvents.inc"
#undef X
    };
    return defs;
}

const ModEventDef& ModEventInfo(ModEvent ev) {
    return ModEventDefs()[(size_t)ev];
}

bool FindModEvent(const std::string& name, ModEvent& out) {
    for (const ModEventDef& def : ModEventDefs()) {
        if (name == def.name) {
            out = def.id;
            return true;
        }
    }
    return false;
}

} // namespace coop::server
