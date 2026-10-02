#pragma once
// The events a mod can listen to (ModEvents.inc): their names, their fields and which ones a handler may change.
// New event: one X(...) line in ModEvents.inc and, where it happens in the server,
//   if (server.Mods().Wants(ModEvent::MyEvent)) { json e = {...}; server.Mods().Fire(ModEvent::MyEvent, e); }
#include <cstdint>
#include <string>
#include <vector>

namespace coop::server {

enum class ModEvent : uint8_t {
#define X(id, name, cancelable, doc, fields) id,
#include "ModEvents.inc"
#undef X
    Count
};

struct ModFieldDef {
    std::string name;
    std::string type; // int, number, string, bool, list, table
    bool writable = false;
    std::string doc;
};

struct ModEventDef {
    ModEvent id;
    const char* name;
    bool cancelable;
    const char* doc;
    std::vector<ModFieldDef> fields;
};

const std::vector<ModEventDef>& ModEventDefs(); // in the order of ModEvent
const ModEventDef& ModEventInfo(ModEvent ev);
bool FindModEvent(const std::string& name, ModEvent& out);

} // namespace coop::server
