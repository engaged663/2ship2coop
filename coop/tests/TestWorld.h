#pragma once
// Shared-world test helpers: whole worlds as JSON, entering/creating the world, waiting for the clock.
#include "TestNet.h"

#include "common/Hex.h"
#include "common/WorldFields.h"
#include "common/WorldOps.h"

#include <filesystem>
#include <initializer_list>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace coop_test {

// Every field of the schema, every byte = fill (hex, as games send them).
inline coop::json WorldFields(uint8_t fill = 0) {
    coop::json fields = coop::json::object();
    for (const auto& def : coop::world::kFields) {
        fields[def.name] = coop::ToHex(std::vector<uint8_t>(def.size, fill));
    }
    return fields;
}

inline uint16_t Field(const char* name) {
    int index = coop::world::FindField(name);
    if (index < 0) {
        Fail(__FILE__, __LINE__, std::string("unknown field ") + name);
    }
    return (uint16_t)index;
}

template <class... T> coop::json Arr(T... values) {
    return coop::json::array({ coop::json(values)... });
}

// Sends world_enter and returns the world_full.
inline coop::json EnterWorld(TestServer& s, TestClient& c) {
    c.Send({ { "t", "world_enter" } });
    auto full = c.WaitFor("world_full", s);
    if (!full.has_value()) {
        Fail(__FILE__, __LINE__, "no world_full");
    }
    return *full;
}

// The next "clock" event that matches pred (older ones are dropped).
template <class Pred>
std::optional<coop::json> WaitClock(TestServer& s, TestClient& c, Pred pred, int timeoutMs = 2000) {
    std::optional<coop::json> found;
    c.WaitUntil(s, timeoutMs, [&] {
        while (auto ev = c.TakeEvent("clock")) {
            if (pred(*ev)) {
                found = ev;
                return true;
            }
        }
        return false;
    });
    return found;
}

// c enters a server without a world, is asked to create it and does (the clock starts once it exists).
inline void CreateWorld(TestServer& s, TestClient& c, uint8_t fill = 0) {
    coop::json full = EnterWorld(s, c);
    if (!coop::GetBool(full, "create")) {
        Fail(__FILE__, __LINE__, "expected create:true");
    }
    c.Send({ { "t", "world_init" }, { "fields", WorldFields(fill) } });
    if (!WaitClock(s, c, [](const coop::json& ev) { return !coop::GetBool(ev, "stopped"); }).has_value()) {
        Fail(__FILE__, __LINE__, "world not created");
    }
}

// Lets pending traffic arrive and forgets it: the test looks only at what comes next.
inline void Drain(TestServer& s, std::initializer_list<TestClient*> clients) {
    s.PumpFor(100);
    for (TestClient* c : clients) {
        c->events.clear();
    }
}

// A fresh directory under %TEMP%, removed at the end of the scope.
struct TempDir {
    std::filesystem::path path;

    explicit TempDir(const char* name) : path(std::filesystem::temp_directory_path() / name) {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
        std::filesystem::create_directories(path, ec);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
    std::string File(const char* name) const {
        return (path / name).string();
    }
};

} // namespace coop_test
