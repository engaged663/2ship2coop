// Shared enemies (sub-project C): who owns each room ("auth"), and the relay of the actor stream, hits, hurts and
// drops. Only players in the server's world take part, and only while server.json has "sharedEnemies": true.
// Every relay checks the sender's role: only a room's owner streams its enemies, hurts players and drops items;
// hits go only to the owner of the room of the enemy that was hit.
#include "server/Registry.h"
#include "server/Server.h"
#include "server/World/RoomAuthority.h"

#include "common/ActorState.h"

#include <algorithm>
#include <cmath>

namespace coop::server {

namespace {

std::map<RoomKey, uint8_t> sOwners;

bool Enabled(Server& server) {
    return server.Config().sharedEnemies;
}

bool Takes(Server& server, const RemoteClient& c) {
    return Enabled(server) && c.welcomed && !c.closing && c.inWorld && c.hasState;
}

uint8_t OwnerOf(int16_t scene, int8_t room) {
    auto it = sOwners.find({ scene, room });
    return it == sOwners.end() ? 0 : it->second;
}

bool OwnsAnyRoomOf(const RemoteClient& c, int16_t scene) {
    for (const auto& [key, id] : sOwners) {
        if (key.first == scene && id == c.id) {
            return true;
        }
    }
    return false;
}

// Everyone else taking part who is in that scene.
std::vector<RemoteClient*> SceneMates(Server& server, const RemoteClient& from, int16_t scene) {
    std::vector<RemoteClient*> out;
    for (RemoteClient* other : server.Players().WelcomedAll()) {
        if (other != &from && Takes(server, *other) && other->scene == scene) {
            out.push_back(other);
        }
    }
    return out;
}

// Every host follows one player (its invisible Link goes wherever that player is, so their room is loaded there).
// D1: the host keeps its player while they stay in the world; else it takes the player in the world longest who
// no other host follows. Sent when it changes.
void TickHostFollow(Server& server) {
    std::vector<uint8_t> taken;
    for (RemoteClient* host : server.Players().WelcomedHosts()) {
        RemoteClient* current = server.Players().ById(host->followId);
        if (current != nullptr && !current->host && Takes(server, *current)) {
            taken.push_back(current->id);
        }
    }
    for (RemoteClient* host : server.Players().WelcomedHosts()) {
        if (!Takes(server, *host)) {
            continue;
        }
        RemoteClient* best = server.Players().ById(host->followId);
        if (best == nullptr || best->host || !Takes(server, *best)) {
            best = nullptr;
            for (RemoteClient* p : server.Players().Welcomed()) {
                bool free = std::find(taken.begin(), taken.end(), p->id) == taken.end();
                if (Takes(server, *p) && free && (best == nullptr || p->roomSinceMs < best->roomSinceMs)) {
                    best = p;
                }
            }
            if (best != nullptr) {
                taken.push_back(best->id);
            }
        }
        uint8_t id = best != nullptr ? best->id : 0;
        if (id != host->followId) {
            host->followId = id;
            json ev = MakeEvent(ev::kHostFollow);
            ev["id"] = id;
            server.SendEvent(*host, ev);
        }
    }
}

json AuthEventFor(const RemoteClient& c, bool takes) {
    json ev = MakeEvent(ev::kAuth);
    json rooms = json::array();
    if (takes) {
        for (const auto& [key, id] : sOwners) {
            if (key.first == c.scene) {
                rooms.push_back({ key.second, id });
            }
        }
    }
    ev["scene"] = takes ? c.scene : -1;
    ev["rooms"] = rooms;
    return ev;
}

// Recomputes the owners and sends "auth" to whoever's view of its scene changed.
void TickAuthority(Server& server) {
    std::vector<AuthMember> members;
    for (RemoteClient* c : server.Players().WelcomedAll()) {
        if (Takes(server, *c)) {
            members.push_back({ c->id, c->scene, c->room, c->busy, c->roomSinceMs, c->host });
        }
    }
    sOwners = ComputeAuthority(members);
    for (RemoteClient* c : server.Players().WelcomedAll()) {
        bool takes = Takes(server, *c);
        if (!takes && c->authSent.empty()) {
            continue; // never took part: nothing to take back
        }
        json ev = AuthEventFor(*c, takes);
        std::string text = SerializeEvent(ev);
        if (text != c->authSent) {
            c->authSent = takes ? text : "";
            server.SendEvent(*c, ev);
        }
    }
    TickHostFollow(server);
}

void OnActors(Server& server, RemoteClient& client, uint8_t* data, size_t size) {
    if (!Takes(server, client) || !client.actorBudget.Take(server.NowMs())) {
        return;
    }
    int16_t scene = -1;
    int8_t room = -1;
    if (!PeekActorHeader(data, size, scene, room) || size > actor_limits::kPacketBytes) {
        server.NoteInvalid(client, "paquete de enemigos inválido");
        return;
    }
    if (scene != client.scene || OwnerOf(scene, room) != client.id) {
        return; // not (or no longer) the owner of that room: a handover in flight
    }
    StampPlayerId(data, size, client.id);
    for (RemoteClient* other : SceneMates(server, client, scene)) {
        server.SendStream(*other, data, size);
    }
}

bool FiniteVec3(const json& ev, const char* key) {
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

bool InRange(const json& ev, const char* key, int64_t min, int64_t max) {
    auto it = ev.find(key);
    if (it == ev.end() || !it->is_number_integer()) {
        return false;
    }
    int64_t v = it->get<int64_t>();
    return v >= min && v <= max;
}

// The damage fields shared by hit and hurt.
bool ValidDamage(const json& ev) {
    return InRange(ev, "dmgFlags", 0, 0xFFFFFFFFll) && InRange(ev, "effect", 0, 255) && InRange(ev, "damage", 0, 255) &&
           InRange(ev, "hitEffect", 0, 255) && FiniteVec3(ev, "pos");
}

void OnHit(Server& server, RemoteClient& client, const json& ev) {
    if (!Takes(server, client) || !client.hitBudget.Take(server.NowMs())) {
        return;
    }
    if (!InRange(ev, "scene", 0, 0x7FFF) || !InRange(ev, "room", 0, actor_limits::kRoomMax) ||
        !InRange(ev, "key", 0, 0xFFFF) || !InRange(ev, "col", 0, actor_limits::kColliders - 1) ||
        !InRange(ev, "elem", 0, 31) || !InRange(ev, "attackerId", 0, 0xFFFF) || !InRange(ev, "form", 0, 255) ||
        !ValidDamage(ev)) {
        server.NoteInvalid(client, "golpe inválido");
        return;
    }
    int16_t scene = (int16_t)GetInt(ev, "scene");
    if (scene != client.scene) {
        return;
    }
    uint8_t owner = OwnerOf(scene, (int8_t)GetInt(ev, "room"));
    RemoteClient* target = server.Players().ById(owner);
    if (target == nullptr || target == &client || !Takes(server, *target) || target->scene != scene) {
        return;
    }
    json out = ev;
    out["from"] = client.id;
    server.SendEvent(*target, out);
}

void OnHurt(Server& server, RemoteClient& client, const json& ev) {
    if (!Takes(server, client) || !client.hitBudget.Take(server.NowMs())) {
        return;
    }
    std::string kind = GetString(ev, "kind");
    if (!InRange(ev, "to", 1, kMaxPlayers + kMaxHosts) || (kind != "col" && kind != "knock") || !ValidDamage(ev)) {
        server.NoteInvalid(client, "daño inválido");
        return;
    }
    RemoteClient* victim = server.Players().ById((uint8_t)GetInt(ev, "to"));
    if (victim == nullptr || victim == &client || victim->host || !Takes(server, *victim) ||
        victim->scene != client.scene ||
        !OwnsAnyRoomOf(client, client.scene)) {
        return;
    }
    json out = ev;
    out["damage"] = std::min<int64_t>(GetInt(ev, "damage"), kMaxHurtDamage);
    out["from"] = client.id;
    server.SendEvent(*victim, out);
}

void OnDrop(Server& server, RemoteClient& client, const json& ev) {
    if (!Takes(server, client) || !client.dropBudget.Take(server.NowMs())) {
        return;
    }
    if (!InRange(ev, "scene", 0, 0x7FFF) || !InRange(ev, "room", 0, actor_limits::kRoomMax) ||
        !InRange(ev, "params", -0x8000, 0xFFFF) || !InRange(ev, "fn", 0, 1) || !FiniteVec3(ev, "pos")) {
        server.NoteInvalid(client, "objeto soltado inválido");
        return;
    }
    int16_t scene = (int16_t)GetInt(ev, "scene");
    if (scene != client.scene || OwnerOf(scene, (int8_t)GetInt(ev, "room")) != client.id) {
        return;
    }
    json out = ev;
    out["from"] = client.id;
    for (RemoteClient* other : SceneMates(server, client, scene)) {
        server.SendEvent(*other, out);
    }
}

} // namespace

COOP_SERVER_STREAM(actorStream, kStreamActors, OnActors);
COOP_SERVER_EVENT(actorHit, ev::kHit, true, OnHit);
COOP_SERVER_EVENT(actorHurt, ev::kHurt, true, OnHurt);
COOP_SERVER_EVENT(actorDrop, ev::kDrop, true, OnDrop);
COOP_SERVER_ON_TICK(actorAuthority, TickAuthority);

} // namespace coop::server
