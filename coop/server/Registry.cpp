#include "Registry.h"

#include <unordered_map>

namespace coop::server {

// Function-local statics: safe to use from other files' static registrars (no init-order issues).
static std::unordered_map<std::string, EventHandlerEntry>& EventTable() {
    static std::unordered_map<std::string, EventHandlerEntry> table;
    return table;
}

static std::unordered_map<uint8_t, StreamHandlerFn>& StreamTable() {
    static std::unordered_map<uint8_t, StreamHandlerFn> table;
    return table;
}

static std::vector<ClientHookFn>& DisconnectTable() {
    static std::vector<ClientHookFn> table;
    return table;
}

static std::vector<TickHookFn>& TickTable() {
    static std::vector<TickHookFn> table;
    return table;
}

void RegisterEventHandler(const std::string& type, EventHandlerFn fn, bool requiresWelcome) {
    EventTable()[type] = { fn, requiresWelcome };
}

const EventHandlerEntry* FindEventHandler(const std::string& type) {
    auto it = EventTable().find(type);
    return it != EventTable().end() ? &it->second : nullptr;
}

void RegisterStreamHandler(uint8_t streamType, StreamHandlerFn fn) {
    StreamTable()[streamType] = fn;
}

StreamHandlerFn FindStreamHandler(uint8_t streamType) {
    auto it = StreamTable().find(streamType);
    return it != StreamTable().end() ? it->second : nullptr;
}

void RegisterDisconnectHook(ClientHookFn fn) {
    DisconnectTable().push_back(fn);
}

const std::vector<ClientHookFn>& DisconnectHooks() {
    return DisconnectTable();
}

void RegisterTickHook(TickHookFn fn) {
    TickTable().push_back(fn);
}

const std::vector<TickHookFn>& TickHooks() {
    return TickTable();
}

} // namespace coop::server
