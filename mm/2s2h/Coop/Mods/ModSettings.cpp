// [COOP] "mod_cfg": the 2 Ship options the server forces on this game (server.json "gameSettings" and its mods'
// game.setSetting) while it plays in the server's world. The event brings the whole map every time; what is no longer
// in it goes back. Only gameplay options (common/ModRules.h), never the ones the co-op itself forces
// (WorldSession_IsForcedCVar), and only numbers. The player's own values are kept (in the settings file too) and come
// back on leaving the world, losing the connection, turning gCoop.Mods off, or at the next start if the game closed
// inside. Applied at the end of a frame: an option's ShipInit may register OnGameStateMainStart hooks.
#include "Mods.h"

#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/World/WorldSession.h"

#include "common/ModRules.h"
#include "common/Protocol.h"
#include "common/Text.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <libultraship/bridge/consolevariablebridge.h>
#include <spdlog/spdlog.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>

namespace coop::client {

namespace {

// What the forced options were ("name=I:5;name=F:0.5;name=;", empty = did not exist). Kept in the settings file while
// they are forced: a game closed inside the world gets them back at its next start. (Not under gCoop.Mods: that one
// is a number, and the settings file nests names by their dots.)
constexpr const char* kRestoreListCVar = "gCoop.World.ModRestoreCVars";

struct Saved {
    enum Kind { Missing, Int, Float, Other } kind = Missing; // Other: a text, a colour
    int32_t i = 0;
    float f = 0.f;
};

json sWanted = json::object(); // the last map from the server
bool sDirty = false;           // sWanted changed (or must be applied again)
std::map<std::string, Saved> sSaved; // the options forced now and what they were

bool Allowed(const std::string& name, const json& value) {
    return mods::SettingAllowed(name) && !WorldSession_IsForcedCVar(name.c_str()) && mods::SettingValueAllowed(value);
}

Saved Read(const std::string& name) {
    Saved s;
    std::shared_ptr<Ship::CVar> cvar = CVarGet(name.c_str());
    if (cvar == nullptr) {
        return s;
    }
    if (cvar->Type == Ship::ConsoleVariableType::Integer) {
        s.kind = Saved::Int;
        s.i = cvar->Integer;
    } else if (cvar->Type == Ship::ConsoleVariableType::Float) {
        s.kind = Saved::Float;
        s.f = cvar->Float;
    } else {
        s.kind = Saved::Other; // a text, a colour: not an option a server may set
    }
    return s;
}

void Write(const std::string& name, const Saved& s) {
    if (s.kind == Saved::Int) {
        CVarSetInteger(name.c_str(), s.i);
    } else if (s.kind == Saved::Float) {
        CVarSetFloat(name.c_str(), s.f);
    } else {
        CVarClear(name.c_str());
    }
}

void SaveList() {
    std::string list;
    for (const auto& [name, s] : sSaved) {
        list += name + "=";
        if (s.kind == Saved::Int) {
            list += "I:" + std::to_string(s.i);
        } else if (s.kind == Saved::Float) {
            char text[32];
            std::snprintf(text, sizeof(text), "F:%.9g", s.f); // every digit a float has
            list += text;
        }
        list += ";";
    }
    if (list.empty()) {
        CVarClear(kRestoreListCVar);
    } else {
        CVarSetString(kRestoreListCVar, list.c_str());
    }
    CVarSave();
}

void InitAll(const std::set<std::string>& names) {
    for (const std::string& name : names) {
        ShipInit::Init(name); // the enhancement registers or drops its hooks
    }
}

// Sets what the server wants and gives back what it no longer forces.
void Apply() {
    std::set<std::string> changed;
    for (auto it = sSaved.begin(); it != sSaved.end();) {
        auto wanted = sWanted.find(it->first);
        if (wanted == sWanted.end() || !Allowed(it->first, *wanted)) {
            Write(it->first, it->second);
            changed.insert(it->first);
            it = sSaved.erase(it);
        } else {
            ++it;
        }
    }
    for (const auto& [name, value] : sWanted.items()) {
        if (!Allowed(name, value) || (sSaved.count(name) == 0 && sSaved.size() >= (size_t)kMaxModSettings)) {
            SPDLOG_WARN("[Coop] Mods: the server cannot force the option '{}'", SanitizeChat(name, 96));
            continue;
        }
        Saved now = Read(name);
        if (now.kind == Saved::Other) {
            SPDLOG_WARN("[Coop] Mods: the option '{}' is not a number", SanitizeChat(name, 96));
            continue;
        }
        if (sSaved.count(name) == 0) {
            sSaved[name] = now;
        }
        // An option that exists keeps its kind (a slider stays a float); a new one takes the number's.
        bool asFloat = now.kind == Saved::Float || (now.kind == Saved::Missing && value.is_number_float());
        if (asFloat) {
            float v = value.get<float>();
            if (now.kind != Saved::Float || now.f != v) {
                CVarSetFloat(name.c_str(), v);
                changed.insert(name);
            }
        } else {
            int32_t v = value.is_number_integer() ? value.get<int32_t>() : (int32_t)std::lround(value.get<double>());
            if (now.kind != Saved::Int || now.i != v) {
                CVarSetInteger(name.c_str(), v);
                changed.insert(name);
            }
        }
    }
    SaveList();
    InitAll(changed);
}

void RestoreAll() {
    std::set<std::string> names;
    for (const auto& [name, s] : sSaved) {
        Write(name, s);
        names.insert(name);
    }
    sSaved.clear();
    SaveList();
    InitAll(names);
}

// First frame after starting the game: the options forced on a game that closed inside the world go back.
void Recover() {
    std::string list = CVarGetString(kRestoreListCVar, "");
    if (list.empty()) {
        return;
    }
    std::set<std::string> names;
    size_t at = 0;
    while (at < list.size()) {
        size_t end = list.find(';', at);
        std::string entry = list.substr(at, end == std::string::npos ? std::string::npos : end - at);
        at = end == std::string::npos ? list.size() : end + 1;
        size_t eq = entry.find('=');
        if (eq == std::string::npos || !mods::SettingAllowed(entry.substr(0, eq))) {
            continue;
        }
        std::string name = entry.substr(0, eq);
        std::string value = entry.substr(eq + 1);
        Saved s;
        if (value.rfind("I:", 0) == 0 && ParseInt(value.substr(2), s.i)) {
            s.kind = Saved::Int;
        } else if (value.rfind("F:", 0) == 0) {
            s.kind = Saved::Float;
            s.f = std::strtof(value.c_str() + 2, nullptr);
        }
        Write(name, s);
        names.insert(name);
    }
    CVarClear(kRestoreListCVar);
    CVarSave();
    InitAll(names);
}

void FrameEnd() {
    static bool sRecovered = false;
    if (!sRecovered) {
        sRecovered = true;
        Recover();
    }
    bool outside = WorldSession_State() == WorldState::Outside;
    if (outside || !Mods_Enabled()) {
        if (!sSaved.empty()) {
            RestoreAll();
        }
        if (outside) {
            sWanted = json::object(); // the next world brings its own
            sDirty = false;
        } else {
            sDirty = true; // gCoop.Mods back on: they go again
        }
        return;
    }
    if (sDirty) {
        sDirty = false;
        Apply();
    }
}

void OnModCfg(const json& ev) {
    if (WorldSession_State() == WorldState::Outside) {
        return;
    }
    auto settings = ev.find("settings");
    sWanted = (settings != ev.end() && settings->is_object()) ? *settings : json::object();
    sDirty = true;
}

void RegisterModSettings() {
    COND_HOOK(OnGameStateMainFinish, true, FrameEnd);
}

} // namespace

COOP_ON_EVENT(modSettingsCfg, ev::kModCfg, OnModCfg);

} // namespace coop::client

static RegisterShipInitFunc sModSettingsInit(coop::client::RegisterModSettings);
