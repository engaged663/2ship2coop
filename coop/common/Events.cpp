#include "Events.h"

namespace coop {

json MakeEvent(const char* type) {
    return json{ { "t", type } };
}

std::string SerializeEvent(const json& ev) {
    return ev.dump(-1, ' ', false, json::error_handler_t::replace);
}

bool ParseEvent(const uint8_t* data, size_t size, json& out, std::string* error, size_t maxBytes) {
    auto fail = [error](const char* why) {
        if (error != nullptr) {
            *error = why;
        }
        return false;
    };
    if (data == nullptr || size == 0) {
        return fail("empty event");
    }
    if (size > maxBytes) {
        return fail("event too large");
    }
    json parsed = json::parse(data, data + size, nullptr, false);
    if (parsed.is_discarded()) {
        return fail("invalid json");
    }
    if (!parsed.is_object()) {
        return fail("event is not an object");
    }
    auto type = parsed.find("t");
    if (type == parsed.end() || !type->is_string()) {
        return fail("event without type");
    }
    out = std::move(parsed);
    return true;
}

std::string EventType(const json& ev) {
    return GetString(ev, "t");
}

std::string GetString(const json& ev, const char* key, const std::string& def) {
    if (!ev.is_object()) {
        return def;
    }
    auto it = ev.find(key);
    return (it != ev.end() && it->is_string()) ? it->get<std::string>() : def;
}

int64_t GetInt(const json& ev, const char* key, int64_t def) {
    if (!ev.is_object()) {
        return def;
    }
    auto it = ev.find(key);
    return (it != ev.end() && it->is_number_integer()) ? it->get<int64_t>() : def;
}

double GetNumber(const json& ev, const char* key, double def) {
    if (!ev.is_object()) {
        return def;
    }
    auto it = ev.find(key);
    return (it != ev.end() && it->is_number()) ? it->get<double>() : def;
}

bool GetBool(const json& ev, const char* key, bool def) {
    if (!ev.is_object()) {
        return def;
    }
    auto it = ev.find(key);
    return (it != ev.end() && it->is_boolean()) ? it->get<bool>() : def;
}

} // namespace coop
