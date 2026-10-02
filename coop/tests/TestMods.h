#pragma once
// Mod tests: a server whose scripts live in a fresh temp folder, and a mod written in C++ for the host's own tests.
#include "TestWorld.h"

#include "server/Mods/ModHost.h"

#include <fstream>
#include <functional>

namespace coop_test {

struct FakeMod : coop::server::Mod {
    std::function<void(int, coop::json&, bool*)> onEvent;
    std::function<bool(int)> onTimer; // false = the handler "failed"
    std::vector<int> timers;          // handlers of the timers that fired, in order
    std::vector<int> released;

    explicit FakeMod(const std::string& name) {
        mInfo.name = name;
        mInfo.kind = "test";
    }
    bool InvokeEvent(int handler, coop::json& payload, bool* cancel, std::string* err) override {
        if (onEvent) {
            onEvent(handler, payload, cancel);
        }
        return true;
    }
    bool InvokeTimer(int handler, std::string* err) override {
        timers.push_back(handler);
        if (onTimer && !onTimer(handler)) {
            *err = "boom";
            return false;
        }
        return true;
    }
    bool InvokeCommand(int handler, const coop::json& ctx, const std::vector<std::string>& args,
                       coop::server::CommandReply* reply, std::string* err) override {
        reply->text = "fake " + std::to_string(handler) + " " + std::to_string(args.size());
        return true;
    }
    void ReleaseHandler(int handler) override {
        released.push_back(handler);
    }
};

// A fake mod inside that server's host; nullptr when the name is taken.
inline FakeMod* AddFake(TestServer& s, const std::string& name) {
    auto* mod = new FakeMod(name);
    return s.server->Mods().Adopt(std::unique_ptr<coop::server::Mod>(mod)) != nullptr ? mod : nullptr;
}

inline bool Logged(TestServer& s, const std::string& text) {
    for (const std::string& line : s.log.Lines()) {
        if (line.find(text) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// A server whose mods folder is a temp directory; nothing is loaded until the test asks.
struct ModServer {
    TempDir dir;
    TestServer s;

    explicit ModServer(const char* name, coop::server::ServerConfig cfg = {}) : dir(name), s(WithMods(cfg, dir)) {
    }
    static coop::server::ServerConfig WithMods(coop::server::ServerConfig cfg, const TempDir& dir) {
        cfg.mods.scriptsDir = dir.path.string();
        return cfg;
    }
    coop::server::ModHost& Mods() {
        return s.server->Mods();
    }
    std::string Write(const std::string& file, const std::string& code) {
        std::string path = dir.File(file.c_str());
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
        std::ofstream f(path, std::ios::binary);
        f << code;
        return path;
    }
    // Writes <name>.lua and loads it; returns the error ("" = loaded).
    std::string Load(const std::string& name, const std::string& code) {
        std::string err;
        Mods().LoadFile(Write(name + ".lua", code), &err);
        return err;
    }
    bool Logged(const std::string& text) {
        return coop_test::Logged(s, text);
    }
};

} // namespace coop_test
