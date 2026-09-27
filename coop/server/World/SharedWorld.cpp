#include "SharedWorld.h"

#include "JsonFile.h"

#include "server/Server.h"

#include "common/Clock.h"

#include <algorithm>
#include <filesystem>
#include <system_error>

namespace coop::server {

namespace {

// More than `levels` objects/arrays inside each other (the value itself counts). Stops at that depth, so a hostile
// inventory never makes it recurse further: saved with indentation, deep nesting would take megabytes.
bool NestedDeeperThan(const json& value, int levels) {
    if (!value.is_structured()) {
        return false;
    }
    if (levels <= 0) {
        return true;
    }
    for (const json& child : value) {
        if (NestedDeeperThan(child, levels - 1)) {
            return true;
        }
    }
    return false;
}

} // namespace

SharedWorld::SharedWorld(Server& server, std::string worldPath, std::string playersDir)
    : mServer(server), mWorldPath(std::move(worldPath)), mPlayers(std::move(playersDir)) {
}

int64_t SharedWorld::Now() const {
    return mServer.NowMs();
}

uint32_t SharedWorld::ClockAbs() {
    return mClock.Abs(Now());
}

std::vector<RemoteClient*> SharedWorld::InWorld() {
    std::vector<RemoteClient*> players;
    for (RemoteClient* c : mServer.Players().Welcomed()) {
        if (c->inWorld && !c->closing) {
            players.push_back(c);
        }
    }
    return players;
}

std::vector<RemoteClient*> SharedWorld::Receivers() {
    std::vector<RemoteClient*> games;
    for (RemoteClient* c : mServer.Players().WelcomedAll()) {
        if (c->inWorld && !c->closing) {
            games.push_back(c);
        }
    }
    return games;
}

std::vector<uint32_t> SharedWorld::EligiblePeers() {
    std::vector<uint32_t> peers;
    for (RemoteClient* c : InWorld()) {
        peers.push_back(c->peer);
    }
    return peers;
}

void SharedWorld::Load() {
    std::string warnings;
    mPlayers.LoadAll(&warnings);
    if (!warnings.empty()) {
        mServer.Log().Warn(warnings);
    }
    if (mWorldPath.empty()) {
        return;
    }
    json saved;
    std::string err;
    bool missing = false;
    if (!LoadJsonFile(mWorldPath, saved, &err, &missing)) {
        if (missing) {
            mServer.Log().Info("Todavía no hay mundo: lo creará el primer jugador que entre en la partida.");
        } else {
            Quarantine(err);
        }
        return;
    }
    if (!mStore.FromJson(saved, &err)) {
        Quarantine(err);
        return;
    }
    json savedClock = saved.value("clock", json::object());
    uint32_t abs = (uint32_t)std::clamp<int64_t>(GetInt(savedClock, "abs", 0), 0, clock::kMoonAbs);
    mClock.Set(abs, Now());
    mClock.SetInverted(GetBool(savedClock, "inv"), Now());
    mServer.Log().Info("Mundo cargado: ciclo " + std::to_string(mStore.Cycle()) + ", " + clock::Format(abs) + ".");
}

void SharedWorld::Quarantine(const std::string& why) {
    std::error_code ec;
    std::filesystem::rename(mWorldPath, mWorldPath + ".bad", ec);
    mServer.Log().Error(mWorldPath + " está dañado (" + why + "); se ha apartado como " + mWorldPath +
                        ".bad y el primer jugador que entre creará un mundo nuevo.");
}

void SharedWorld::Flush() {
    int64_t now = Now();
    std::string err;
    if (!mWorldPath.empty() && mStore.Exists()) {
        json saved = mStore.ToJson();
        saved["clock"] = json{ { "abs", mClock.Abs(now) }, { "inv", mClock.Inverted() } };
        if (!SaveJsonFile(mWorldPath, saved, &err)) {
            mServer.Log().Error("No se pudo guardar el mundo: " + err);
        }
    }
    if (!mPlayers.SaveAll(&err)) {
        mServer.Log().Error("No se pudo guardar a los jugadores: " + err);
    }
    mDirty = false;
    mLastSaveMs = now;
}

void SharedWorld::Tick() {
    int64_t now = Now();
    if (mCreator != 0 && now >= mCreatorDeadlineMs) {
        if (RemoteClient* late = mServer.Players().ByPeer(mCreator)) {
            mServer.SendSystem(late, "Tu juego no creó el mundo a tiempo; lo creará otro jugador. Vuelve a entrar en "
                                     "la partida del servidor.",
                               level::kWarn);
        }
        mServer.Log().Warn("El juego encargado de crear el mundo no respondió a tiempo.");
        NextCreator();
    }
    TickCycle(now);
    if (mStore.Exists() && now >= mNextClockMs) {
        mNextClockMs = now + mServer.Config().clockBroadcastMs;
        SendClock(nullptr, false);
    }
    if ((mDirty || mClock.Running()) && now - mLastSaveMs >= mServer.Config().worldSaveMs) {
        Flush();
    }
}

void SharedWorld::Enter(RemoteClient& c) {
    bool waiting = std::find(mWaiting.begin(), mWaiting.end(), c.peer) != mWaiting.end();
    if (c.inWorld || c.peer == mCreator || waiting) {
        return; // asked twice
    }
    if (c.host) {
        // The server's own game follows a world that a player created; it never creates or resets one.
        if (!mStore.Exists() || mResetting) {
            mWaiting.push_back(c.peer);
            return;
        }
        c.inWorld = true;
        mServer.SendEvent(c, FullEvent(c, ""));
        mServer.Log().Info("El anfitrión " + std::to_string(c.id) + " sigue la partida del servidor.");
        return;
    }
    if (!mStore.Exists() && mCreator == 0) {
        AssignCreator(c);
        return;
    }
    if (!mStore.Exists() || mResetting) {
        mWaiting.push_back(c.peer);
        mServer.SendSystem(&c,
                           mResetting ? "Se está volviendo al Amanecer del Primer Día; entrarás en cuanto termine."
                                      : "Se está creando el mundo; entrarás en cuanto esté listo.",
                           level::kInfo);
        return;
    }
    c.inWorld = true;
    mServer.SendEvent(c, FullEvent(c, ""));
    Announce(c.nick + " entra en la partida del servidor.", level::kInfo, &c);
    UpdateClock();
}

void SharedWorld::AssignCreator(RemoteClient& c) {
    mCreator = c.peer;
    mCreatorDeadlineMs = Now() + mServer.Config().worldCreateTimeoutMs;
    json ev = MakeEvent(ev::kWorldFull);
    ev["create"] = true;
    ev["cycle"] = 1;
    ev["fields"] = json::object();
    ev["clock"] = json{ { "abs", 0 }, { "inv", false }, { "stopped", true } };
    ev["you"] = json{ { "inv", nullptr }, { "stale", false } };
    ev["reset"] = "";
    mServer.SendEvent(c, ev);
    mServer.Log().Info(c.nick + " crea el mundo del servidor.");
}

void SharedWorld::NextCreator() {
    mCreator = 0;
    size_t i = 0;
    while (i < mWaiting.size()) {
        RemoteClient* next = mServer.Players().ByPeer(mWaiting[i]);
        if (next != nullptr && next->host) {
            i++; // hosts never create the world: they stay in the queue until it exists
            continue;
        }
        mWaiting.erase(mWaiting.begin() + i); // the next entry is now at i
        if (next != nullptr && next->welcomed && !next->closing) {
            AssignCreator(*next);
            return;
        }
    }
}

void SharedWorld::EnterWaiting() {
    std::vector<uint32_t> waiting;
    waiting.swap(mWaiting);
    for (uint32_t peer : waiting) {
        RemoteClient* c = mServer.Players().ByPeer(peer);
        if (c != nullptr && c->welcomed && !c->closing) {
            Enter(*c);
        }
    }
}

void SharedWorld::Create(RemoteClient& c, const json& ev) {
    if (c.peer != mCreator || mStore.Exists()) {
        mServer.NoteInvalid(c, "world_init sin haberlo pedido el servidor");
        return;
    }
    FieldSet fields;
    std::string err = "falta 'fields'";
    auto it = ev.find("fields");
    if (it == ev.end() || !WorldStore::ParseFields(*it, fields, &err)) {
        mServer.NoteInvalid(c, "world_init inválido: " + err);
        return;
    }
    int64_t now = Now();
    mCreator = 0;
    mPlayers.Clear();
    mStore.StartCycle(fields, 1);
    mClock.Set(0, now);
    mClock.SetInverted(false, now);
    c.inWorld = true;
    mServer.Log().Info("Mundo creado por " + c.nick + ".");
    Flush();
    EnterWaiting();
    UpdateClock();
}

void SharedWorld::Leave(RemoteClient& c, bool disconnected) {
    mWaiting.erase(std::remove(mWaiting.begin(), mWaiting.end(), c.peer), mWaiting.end());
    if (c.peer == mCreator) {
        NextCreator();
    }
    if (!c.inWorld) {
        return;
    }
    c.inWorld = false;
    if (c.host) {
        return; // nothing of it is saved or announced
    }
    if (mResetting && c.peer == mComputer) {
        NextComputer();
    }
    std::string text = c.nick + " sale de la partida del servidor.";
    if (disconnected) {
        mServer.Log().Info(text); // the others already see "se ha ido"
    } else {
        Announce(text, level::kInfo, &c);
    }
    if (mVote.Active()) {
        EvaluateVote();
    }
    UpdateClock();
    mDirty = true;
}

void SharedWorld::ApplyOps(RemoteClient& c, const json& ev) {
    if (!c.inWorld) {
        return; // sent just before leaving
    }
    if (c.host) {
        return; // D1: the players' games own the world's changes; the host only follows them
    }
    if (!c.wopsBudget.Take(Now())) {
        mServer.NoteInvalid(c, "demasiados cambios del mundo por segundo");
        return;
    }
    if (GetInt(ev, "cycle", -1) != mStore.Cycle()) {
        return; // made before that game saw a cycle reset: that world is gone
    }
    world::Ops ops;
    std::string err;
    if (!world::FromJson(ev, ops, &err)) {
        mServer.NoteInvalid(c, "wops mal formado: " + err);
        return;
    }
    world::Ops relay;
    world::Ops corrections;
    int invalid = 0;
    if (mStore.Apply(ops, relay, corrections, &invalid)) {
        mDirty = true;
    }
    if (invalid > 0) {
        mServer.NoteInvalid(c, std::to_string(invalid) + " cambios del mundo fuera del esquema");
    }
    if (!corrections.Empty()) {
        json out = world::ToJson(corrections);
        out["t"] = ev::kWops;
        out["from"] = 0;
        mServer.SendEvent(c, out);
    }
    if (!relay.Empty()) {
        json out = world::ToJson(relay);
        out["t"] = ev::kWops;
        out["from"] = c.id;
        // The sender gets its bits and bytes back too: every game then applies the changes of a byte in the
        // server's order, so two players writing the same byte at once end with the same value. Its counters are
        // already settled by the correction above.
        world::Ops echo = relay;
        echo.adds.clear();
        json back = world::ToJson(echo);
        back["t"] = ev::kWops;
        back["from"] = c.id;
        for (RemoteClient* p : Receivers()) {
            if (p != &c) {
                mServer.SendEvent(*p, out);
            } else if (!echo.Empty()) {
                mServer.SendEvent(*p, back);
            }
        }
    }
}

void SharedWorld::Upload(RemoteClient& c, const json& ev) {
    if (!c.inWorld || c.host) {
        return; // a host has no inventory of its own
    }
    if (!c.invBudget.Take(Now())) {
        mServer.NoteInvalid(c, "demasiadas subidas de inventario");
        return;
    }
    auto inv = ev.find("inv");
    if (inv == ev.end() || !inv->is_object() || SerializeEvent(*inv).size() > kMaxInventoryBytes ||
        NestedDeeperThan(*inv, kMaxInventoryDepth)) {
        mServer.NoteInvalid(c, "inventario inválido o demasiado grande");
        return;
    }
    if (GetInt(ev, "cycle", -1) != mStore.Cycle()) {
        return; // sent before that game saw the new cycle: it will upload again
    }
    mPlayers.Upload(c.nick, *inv, mStore.Cycle());
    mDirty = true;
}

void SharedWorld::UpdateClock() {
    bool someone = false;
    bool stopped = false;
    for (RemoteClient* p : InWorld()) {
        someone = true;
        stopped = stopped || p->timeStopped;
    }
    bool run = mStore.Exists() && someone && !stopped && !mResetting;
    if (run == mClock.Running()) {
        return;
    }
    mClock.SetRunning(run, Now());
    SendClock(nullptr, false);
}

json SharedWorld::ClockJson() {
    return json{ { "abs", mClock.Abs(Now()) }, { "inv", mClock.Inverted() }, { "stopped", !mClock.Running() } };
}

void SharedWorld::SendClock(RemoteClient* to, bool jump) {
    json ev = ClockJson();
    ev["t"] = ev::kClock;
    ev["jump"] = jump;
    if (to != nullptr) {
        mServer.SendEvent(*to, ev);
        return;
    }
    for (RemoteClient* p : Receivers()) {
        mServer.SendEvent(*p, ev);
    }
}

json SharedWorld::FullEvent(const RemoteClient& c, const char* reset) {
    const PlayerRecord* record = mPlayers.Get(c.nick);
    json ev = MakeEvent(ev::kWorldFull);
    ev["create"] = false;
    ev["cycle"] = mStore.Cycle();
    ev["fields"] = mStore.FieldsJson();
    ev["clock"] = ClockJson();
    ev["you"] = json{ { "inv", record != nullptr ? record->inv : json(nullptr) },
                      { "stale", mPlayers.IsStale(c.nick, mStore.Cycle()) } };
    ev["reset"] = reset;
    return ev;
}

void SharedWorld::Announce(const std::string& text, const char* level, const RemoteClient* except) {
    for (RemoteClient* p : InWorld()) {
        if (p != except) {
            mServer.SendSystem(p, text, level);
        }
    }
    mServer.Log().Info(text);
}

std::string SharedWorld::StopReason() {
    if (mClock.Running()) {
        return "";
    }
    if (mResetting) {
        return "se está volviendo al Amanecer del Primer Día";
    }
    for (RemoteClient* p : InWorld()) {
        if (p->timeStopped) {
            return p->nick + " está en un lugar donde el tiempo no corre";
        }
    }
    return "no hay nadie en la partida";
}

std::string SharedWorld::TimeText() {
    if (!mStore.Exists()) {
        return "Todavía no hay mundo: lo creará el primer jugador que entre en la partida del servidor.";
    }
    std::string text = clock::Format(ClockAbs()) + " · " +
                       (mClock.Inverted() ? "tiempo ralentizado" : "velocidad normal") + " · ciclo " +
                       std::to_string(mStore.Cycle());
    std::string why = StopReason();
    if (!why.empty()) {
        text += " · parado: " + why;
    }
    return text;
}

std::string SharedWorld::Describe() {
    if (!mStore.Exists()) {
        return mCreator != 0 ? "Se está creando el mundo." : "Todavía no hay mundo.";
    }
    std::string names;
    for (RemoteClient* p : InWorld()) {
        names += (names.empty() ? "" : ", ") + p->nick;
    }
    std::string text = "Mundo: " + TimeText();
    text += "\nEn la partida: " + (names.empty() ? std::string("nadie") : names);
    text += "\nJugadores guardados: " + std::to_string(mPlayers.Count());
    if (mVote.Active()) {
        text += "\nVotación de la Canción del Tiempo en marcha (" + std::to_string(mVote.SecondsLeft(Now())) + " s).";
    }
    if (!mWorldPath.empty()) {
        text += "\nArchivo: " + mWorldPath;
    }
    return text;
}

} // namespace coop::server
