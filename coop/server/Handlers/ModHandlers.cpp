// Mods (Mods/ModHost.h): their share of every tick (timers, deferred work, pending unloads) and what the games
// playing in the server's world report for them:
//   gev    something happened in that game: an item was given, Link died, a boss or an enemy was defeated
//   stat   its health, magic and rupees, when they change
// Nothing of this is relayed to other games: the server checks it, keeps the numbers and tells the mods.
#include "server/Groups.h"
#include "server/Mods/GameIds.h"
#include "server/Mods/ModHost.h"
#include "server/Registry.h"
#include "server/Server.h"

#include "common/PlayerState.h"

#include <cmath>

namespace coop::server {

namespace {

void TickMods(Server& server) {
    server.Mods().Tick();
}

bool IntIn(const json& ev, const char* key, int64_t min, int64_t max) {
    auto it = ev.find(key);
    if (it == ev.end() || !it->is_number_integer()) {
        return false;
    }
    int64_t v = it->get<int64_t>();
    return v >= min && v <= max;
}

bool OptionalIntIn(const json& ev, const char* key, int64_t min, int64_t max) {
    return !ev.contains(key) || IntIn(ev, key, min, max);
}

bool Position(const json& ev, const char* key) {
    auto it = ev.find(key);
    if (it == ev.end() || !it->is_array() || it->size() != 3) {
        return false;
    }
    for (const json& v : *it) {
        if (!v.is_number() || !std::isfinite(v.get<double>()) || std::fabs(v.get<double>()) > pose_limits::kWorldLimit) {
            return false;
        }
    }
    return true;
}

json Who(const RemoteClient& c) {
    return { { "player", c.id }, { "nick", c.nick } };
}

void OnGameEvent(Server& server, RemoteClient& client, const json& ev) {
    if (!Groups_InWorld(client) || !client.modEventBudget.Take(server.NowMs())) {
        return; // a game on its own save has nothing to say to the server's mods
    }
    ModHost& mods = server.Mods();
    std::string kind = GetString(ev, "k");
    if (kind == "item" && IntIn(ev, "id", 0, 255)) {
        if (mods.Wants(ModEvent::PlayerItem)) {
            int item = (int)GetInt(ev, "id");
            json e = Who(client);
            e["item"] = item;
            e["itemKey"] = ids::NameOf(ids::Kind::Item, item);
            mods.Fire(ModEvent::PlayerItem, e);
        }
    } else if (kind == "death") {
        if (mods.Wants(ModEvent::PlayerDeath)) {
            json e = Who(client);
            e["scene"] = client.scene;
            mods.Fire(ModEvent::PlayerDeath, e);
        }
    } else if (kind == "boss" && IntIn(ev, "actor", 0, kMaxActorId)) {
        if (mods.Wants(ModEvent::BossDefeated)) {
            int actor = (int)GetInt(ev, "actor");
            json e = Who(client);
            e["actor"] = actor;
            e["actorKey"] = ids::NameOf(ids::Kind::Actor, actor);
            e["scene"] = client.scene;
            mods.Fire(ModEvent::BossDefeated, e);
        }
    } else if (kind == "kill" && IntIn(ev, "actor", 0, kMaxActorId) && OptionalIntIn(ev, "params", -32768, 65535) &&
               OptionalIntIn(ev, "by", 0, 255) && OptionalIntIn(ev, "room", -1, 127) && Position(ev, "pos")) {
        if (mods.Wants(ModEvent::EnemyKilled)) {
            // Whoever gave the last blow, when the game that simulated the enemy knows and it still plays here.
            RemoteClient* killer = server.Players().ById((uint8_t)GetInt(ev, "by", 0));
            if (killer == nullptr || !Groups_InWorld(*killer)) {
                killer = &client;
            }
            int actor = (int)GetInt(ev, "actor");
            const json& pos = ev["pos"];
            json e = Who(*killer);
            e["actor"] = actor;
            e["actorKey"] = ids::NameOf(ids::Kind::Actor, actor);
            e["params"] = GetInt(ev, "params", 0);
            e["scene"] = client.scene;
            e["room"] = GetInt(ev, "room", client.room);
            e["x"] = pos[0];
            e["y"] = pos[1];
            e["z"] = pos[2];
            mods.Fire(ModEvent::EnemyKilled, e);
        }
    } else {
        server.NoteInvalid(client, Tr(Msg::InvGameEvent));
    }
}

void OnStat(Server& server, RemoteClient& client, const json& ev) {
    if (!Groups_InWorld(client) || !client.statBudget.Take(server.NowMs())) {
        return;
    }
    if (!IntIn(ev, "hp", 0, kMaxModHealth) || !IntIn(ev, "hpMax", 0, kMaxModHealth) || !IntIn(ev, "mp", 0, 255) ||
        !IntIn(ev, "rupees", 0, 99999)) {
        server.NoteInvalid(client, Tr(Msg::InvStat));
        return;
    }
    int32_t health = (int32_t)GetInt(ev, "hp");
    int32_t maxHealth = (int32_t)GetInt(ev, "hpMax");
    int32_t magic = (int32_t)GetInt(ev, "mp");
    int32_t rupees = (int32_t)GetInt(ev, "rupees");
    bool first = !client.hasStats; // nothing to compare with: its "previous" values are these
    int32_t prevHealth = first ? health : client.health;
    int32_t prevMagic = first ? magic : client.magic;
    int32_t prevRupees = first ? rupees : client.rupees;
    bool changed = first || health != client.health || maxHealth != client.maxHealth || magic != client.magic ||
                   rupees != client.rupees;
    client.hasStats = true;
    client.health = health;
    client.maxHealth = maxHealth;
    client.magic = magic;
    client.rupees = rupees;
    if (changed && server.Mods().Wants(ModEvent::PlayerStats)) {
        json e = Who(client);
        e["health"] = health;
        e["maxHealth"] = maxHealth;
        e["magic"] = magic;
        e["rupees"] = rupees;
        e["prevHealth"] = prevHealth;
        e["prevMagic"] = prevMagic;
        e["prevRupees"] = prevRupees;
        server.Mods().Fire(ModEvent::PlayerStats, e);
    }
}

void OnGone(Server& server, RemoteClient& client) {
    server.Mods().OnPlayerGone(client);
}

} // namespace

COOP_SERVER_ON_TICK(modsTick, TickMods);
COOP_SERVER_EVENT(modsGameEvent, ev::kGameEvent, true, OnGameEvent);
COOP_SERVER_EVENT(modsStat, ev::kStat, true, OnStat);
COOP_SERVER_ON_DISCONNECT(modsGone, OnGone);

} // namespace coop::server
