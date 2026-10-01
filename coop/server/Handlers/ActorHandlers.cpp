// Shared enemies (sub-project C): who owns each room ("auth"), and the relay of the actor stream, hits, hurts and
// drops. Only players in the server's world take part, and only while server.json has "sharedEnemies": true.
// Every relay checks the sender's role: only a room's owner streams its enemies, hurts players and drops items;
// hits go only to the owner of the room of the enemy that was hit.
#include "server/Registry.h"
#include "server/Server.h"
#include "server/World/RoomAuthority.h"

#include "common/ActorImage.h"
#include "common/EponaState.h"
#include "common/PlayerState.h"
#include "common/StreamIds.h"

#include <algorithm>
#include <tuple>
#include <cmath>

namespace coop::server {

namespace {

std::map<RoomKey, uint8_t> sOwners;

// D3: NPCs lent to the player next to them. (scene, room, key of the NPC) -> who simulates it now.
struct Lease {
    uint8_t holder = 0;
    double dist = 0.0;
    bool talking = false;
    int64_t renewedMs = 0;
};
struct LeaseKey {
    int16_t scene;
    int8_t room;
    uint32_t key;
    bool operator<(const LeaseKey& o) const {
        return std::tie(scene, room, key) < std::tie(o.scene, o.room, o.key);
    }
};
std::map<LeaseKey, Lease> sLeases;
constexpr double kLeaseCloserRatio = 0.75; // a new player must be 25 % closer to take it

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

uint8_t LesseeOf(int16_t scene, int8_t room, uint32_t key) {
    auto it = sLeases.find({ scene, room, key });
    return it == sLeases.end() ? 0 : it->second.holder;
}

bool HoldsLeaseIn(const RemoteClient& c, int16_t scene, int8_t room) {
    for (const auto& [k, l] : sLeases) {
        if (k.scene == scene && k.room == room && l.holder == c.id) {
            return true;
        }
    }
    return false;
}

// Simulates something in that room: its owner, or a player one of its NPCs is lent to.
bool ActsIn(const RemoteClient& c, int16_t scene, int8_t room) {
    return OwnerOf(scene, room) == c.id || HoldsLeaseIn(c, scene, room);
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

bool TakesEpona(const RemoteClient& client) {
    return client.welcomed && !client.closing && !client.host && client.inWorld && client.hasState;
}

std::vector<RemoteClient*> EponaMates(Server& server, const RemoteClient& from, int16_t scene) {
    std::vector<RemoteClient*> out;
    for (RemoteClient* other : server.Players().WelcomedAll()) {
        if (other != &from && TakesEpona(*other) && other->scene == scene) {
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

json LeasesEventFor(const RemoteClient& c, bool takes) {
    json ev = MakeEvent(ev::kLeases);
    json list = json::array();
    if (takes) {
        for (const auto& [k, l] : sLeases) {
            if (k.scene == c.scene) {
                list.push_back({ k.room, k.key, l.holder });
            }
        }
    }
    ev["scene"] = takes ? c.scene : -1;
    ev["list"] = list;
    return ev;
}

// Leases end when not renewed, or when their holder is no longer taking part in that scene.
void TickLeases(Server& server) {
    int64_t now = server.NowMs();
    for (auto it = sLeases.begin(); it != sLeases.end();) {
        RemoteClient* holder = server.Players().ById(it->second.holder);
        bool keep = holder != nullptr && !holder->host && Takes(server, *holder) && holder->scene == it->first.scene &&
                    now - it->second.renewedMs <= server.Config().leaseExpireMs;
        it = keep ? std::next(it) : sLeases.erase(it);
    }
    for (RemoteClient* c : server.Players().WelcomedAll()) {
        bool takes = Takes(server, *c) && !c->host;
        json ev = LeasesEventFor(*c, takes);
        bool empty = ev["list"].empty();
        if (empty && c->leasesSent.empty()) {
            continue; // nothing lent, and nothing was said before: the default
        }
        std::string text = SerializeEvent(ev);
        if (text != c->leasesSent) {
            c->leasesSent = empty ? "" : text;
            server.SendEvent(*c, ev);
        }
    }
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
    TickLeases(server);
    TickHostFollow(server);
}

void OnActors(Server& server, RemoteClient& client, uint8_t* data, size_t size) {
    if (!Takes(server, client) || !client.actorBudget.Take(server.NowMs())) {
        return;
    }
    int16_t scene = -1;
    int8_t room = -1;
    if (!PeekActorImageHeader(data, size, scene, room) || size > image_limits::kPacketBytes) {
        server.NoteInvalid(client, Tr(Msg::InvActorPacket));
        return;
    }
    // A game showing its cutscene to others sends its cutscene actors, whoever owns the room (the watchers take them)
    bool showsCutscene = client.cinemaMs >= 0 && server.NowMs() - client.cinemaMs <= kCinemaActorsMs;
    if (scene != client.scene || !(ActsIn(client, scene, room) || showsCutscene)) {
        return; // neither the owner nor a lessee of that room (a handover in flight): the games check each record
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
    if (!InRange(ev, "scene", 0, 0x7FFF) || !InRange(ev, "room", 0, image_limits::kRoomMax) ||
        !InRange(ev, "key", 0, 0xFFFFFFFFll) || !InRange(ev, "col", 0, image_limits::kColliders - 1) ||
        !InRange(ev, "elem", 0, 31) || !InRange(ev, "attackerId", 0, 0xFFFF) || !InRange(ev, "form", 0, 255) ||
        !ValidDamage(ev)) {
        server.NoteInvalid(client, Tr(Msg::InvHit));
        return;
    }
    int16_t scene = (int16_t)GetInt(ev, "scene");
    if (scene != client.scene) {
        return;
    }
    int8_t room = (int8_t)GetInt(ev, "room");
    // The family of a lent NPC (its root key) is simulated by the lessee; everything else by the room's owner.
    uint32_t root = (uint32_t)GetInt(ev, "root", GetInt(ev, "key"));
    uint8_t owner = LesseeOf(scene, room, root);
    if (owner == 0) {
        owner = OwnerOf(scene, room);
    }
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
        server.NoteInvalid(client, Tr(Msg::InvDamage));
        return;
    }
    RemoteClient* victim = server.Players().ById((uint8_t)GetInt(ev, "to"));
    bool lessee = false;
    for (const auto& [k, l] : sLeases) {
        lessee = lessee || (k.scene == client.scene && l.holder == client.id);
    }
    if (victim == nullptr || victim == &client || victim->host || !Takes(server, *victim) ||
        victim->scene != client.scene || !(OwnsAnyRoomOf(client, client.scene) || lessee)) {
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
    if (!InRange(ev, "scene", 0, 0x7FFF) || !InRange(ev, "room", 0, image_limits::kRoomMax) ||
        !InRange(ev, "params", -0x8000, 0xFFFF) || !InRange(ev, "fn", 0, 1) || !FiniteVec3(ev, "pos")) {
        server.NoteInvalid(client, Tr(Msg::InvDrop));
        return;
    }
    int16_t scene = (int16_t)GetInt(ev, "scene");
    if (scene != client.scene || !ActsIn(client, scene, (int8_t)GetInt(ev, "room"))) {
        return;
    }
    json out = ev;
    out["from"] = client.id;
    for (RemoteClient* other : SceneMates(server, client, scene)) {
        server.SendEvent(*other, out);
    }
}

void OnLeaseReq(Server& server, RemoteClient& client, const json& ev) {
    if (!Takes(server, client) || client.host || !client.leaseBudget.Take(server.NowMs())) {
        return;
    }
    auto dist = ev.find("dist");
    if (!InRange(ev, "scene", 0, 0x7FFF) || !InRange(ev, "room", 0, image_limits::kRoomMax) ||
        !InRange(ev, "key", 0, 0xFFFFFFFFll) || dist == ev.end() || !dist->is_number() ||
        !std::isfinite(dist->get<double>()) || dist->get<double>() < 0.0 || dist->get<double>() > 100000.0) {
        server.NoteInvalid(client, Tr(Msg::InvLoan));
        return;
    }
    int16_t scene = (int16_t)GetInt(ev, "scene");
    uint32_t key = (uint32_t)GetInt(ev, "key");
    if (scene != client.scene) {
        return; // changing scene
    }
    // A runtime actor (Epona: tracked by whoever just mounted her, Ride.cpp) is lent to the first one who asks
    // (the rider): they simulate it for as long as they keep asking. List actors (NPCs and props) follow the
    // "closer player" rule below.
    bool runtime = (key & kRuntimeKeyBits) != 0;
    LeaseKey k{ scene, (int8_t)GetInt(ev, "room"), key };
    double d = dist->get<double>();
    bool talking = GetBool(ev, "talking");
    auto it = sLeases.find(k);
    if (it == sLeases.end()) {
        int held = 0;
        for (const auto& [lk, l] : sLeases) {
            held += l.holder == client.id ? 1 : 0;
        }
        if (held >= kMaxLeasesPerPlayer) {
            return;
        }
        sLeases[k] = { client.id, d, talking, server.NowMs() };
        return;
    }
    Lease& l = it->second;
    if (l.holder == client.id) {
        l = { client.id, d, talking, server.NowMs() };
    } else if (runtime) {
        return; // the rider's: only its holder renews it
    } else if (!l.talking && d < l.dist * kLeaseCloserRatio) {
        l = { client.id, d, talking, server.NowMs() };
    }
}

void OnLeaseDrop(Server& server, RemoteClient& client, const json& ev) {
    if (!Takes(server, client) || !client.leaseBudget.Take(server.NowMs())) {
        return;
    }
    if (!InRange(ev, "scene", 0, 0x7FFF) || !InRange(ev, "room", 0, image_limits::kRoomMax) ||
        !InRange(ev, "key", 0, 0xFFFFFFFFll)) {
        server.NoteInvalid(client, Tr(Msg::InvLoan));
        return;
    }
    auto it = sLeases.find({ (int16_t)GetInt(ev, "scene"), (int8_t)GetInt(ev, "room"), (uint32_t)GetInt(ev, "key") });
    if (it != sLeases.end() && it->second.holder == client.id) {
        sLeases.erase(it);
    }
}

// An actor every game creates its own copy of (a warp, a heart container): from whoever simulates that room.
void OnEcho(Server& server, RemoteClient& client, const json& ev) {
    if (!Takes(server, client) || !client.leaseBudget.Take(server.NowMs())) {
        return;
    }
    if (!InRange(ev, "scene", 0, 0x7FFF) || !InRange(ev, "room", 0, image_limits::kRoomMax) ||
        !InRange(ev, "id", 0, kMaxActorId) || !InRange(ev, "params", -0x8000, 0xFFFF) || !FiniteVec3(ev, "pos") ||
        !FiniteVec3(ev, "rot")) {
        server.NoteInvalid(client, Tr(Msg::InvEchoActor));
        return;
    }
    int16_t scene = (int16_t)GetInt(ev, "scene");
    if (scene != client.scene || !ActsIn(client, scene, (int8_t)GetInt(ev, "room"))) {
        return;
    }
    json out = ev;
    out["from"] = client.id;
    for (RemoteClient* other : SceneMates(server, client, scene)) {
        if (!other->host) {
            server.SendEvent(*other, out);
        }
    }
}

// A player played Epona's Song. The reliable event identifies one permanent horse; it never replaces older keys.
void OnEponaCall(Server& server, RemoteClient& client, const json& ev) {
    if (!TakesEpona(client) || !client.leaseBudget.Take(server.NowMs())) {
        return;
    }
    if (!InRange(ev, "scene", 0, 0x7FFF) || !InRange(ev, "horse", 1, 0xFFFFFFFFll) ||
        !FiniteVec3(ev, "pos") || !FiniteVec3(ev, "rot")) {
        server.NoteInvalid(client, Tr(Msg::InvEponaCall));
        return;
    }
    int16_t scene = (int16_t)GetInt(ev, "scene");
    uint32_t sequence = (uint32_t)GetInt(ev, "horse");
    if (scene != client.scene) {
        return;
    }
    if (sequence <= client.eponaLastCallSequence) {
        return;
    }
    client.eponaLastCallSequence = sequence;
    json out = ev;
    out["from"] = client.id;
    for (RemoteClient* other : EponaMates(server, client, scene)) {
        server.SendEvent(*other, out);
    }
}

void OnEponaPassenger(Server& server, RemoteClient& client, const json& ev) {
    if (!TakesEpona(client) || !client.leaseBudget.Take(server.NowMs())) {
        return;
    }
    auto mounted = ev.find("mounted");
    if (!InRange(ev, "scene", 0, 0x7FFF) || !InRange(ev, "owner", 1, kMaxPlayers) ||
        !InRange(ev, "horse", 1, 0xFFFFFFFFll) || mounted == ev.end() || !mounted->is_boolean()) {
        server.NoteInvalid(client, Tr(Msg::InvEponaPassenger));
        return;
    }
    int16_t scene = (int16_t)GetInt(ev, "scene");
    uint8_t owner = (uint8_t)GetInt(ev, "owner");
    RemoteClient* target = server.Players().ById(owner);
    if (scene != client.scene || target == nullptr || target == &client || target->host || !TakesEpona(*target) ||
        target->scene != scene || (uint32_t)GetInt(ev, "horse") > target->eponaLastCallSequence) {
        return;
    }
    json out = ev;
    out["from"] = client.id;
    server.SendEvent(*target, out);
}

void OnEponaState(Server& server, RemoteClient& client, uint8_t* data, size_t size) {
    if (!TakesEpona(client) || !client.streamBudget.Take(server.NowMs())) {
        return;
    }
    EponaPacket packet;
    if (!DecodeEponaState(data, size, packet) || packet.sceneId != client.scene) {
        server.NoteInvalid(client, Tr(Msg::InvEponaState));
        return;
    }
    packet.ownerPlayerId = client.id;
    auto stamped = EncodeEponaState(packet);
    for (RemoteClient* other : EponaMates(server, client, packet.sceneId)) {
        server.SendStream(*other, stamped.data(), stamped.size());
        client.streamsRelayed++;
    }
    client.streamsIn++;
    (void)size;
}

} // namespace

COOP_SERVER_STREAM(actorStream, kStreamActors, OnActors);
COOP_SERVER_STREAM(eponaState, kStreamEponaState, OnEponaState);
COOP_SERVER_EVENT(actorLeaseReq, ev::kLeaseReq, true, OnLeaseReq);
COOP_SERVER_EVENT(actorLeaseDrop, ev::kLeaseDrop, true, OnLeaseDrop);
COOP_SERVER_EVENT(actorEcho, ev::kEcho, true, OnEcho);
COOP_SERVER_EVENT(eponaCall, ev::kEponaCall, true, OnEponaCall);
COOP_SERVER_EVENT(eponaPassenger, ev::kEponaPassenger, true, OnEponaPassenger);
COOP_SERVER_EVENT(actorHit, ev::kHit, true, OnHit);
COOP_SERVER_EVENT(actorHurt, ev::kHurt, true, OnHurt);
COOP_SERVER_EVENT(actorDrop, ev::kDrop, true, OnDrop);
COOP_SERVER_ON_TICK(actorAuthority, TickAuthority);

} // namespace coop::server
