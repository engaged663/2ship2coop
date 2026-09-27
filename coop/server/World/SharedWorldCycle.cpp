// Cycle rules of the shared world: Song of Double Time, Inverted Song of Time, the Song of Time vote, resets
// computed by one game (cycle_compute -> cycle_result, the original end-of-cycle code), the moon and /settime.
#include "SharedWorld.h"

#include "server/Server.h"

#include "common/Clock.h"

#include <algorithm>

namespace coop::server {

void SharedWorld::TickCycle(int64_t now) {
    if (mResetting && now >= mComputeDeadlineMs) {
        mServer.Log().Warn("Un juego no calculó el nuevo ciclo a tiempo; se pide a otro.");
        NextComputer();
    }
    if (mVote.Active()) {
        EvaluateVote();
    }
    if (mStore.Exists() && !mResetting && mClock.MoonReached(now)) {
        MoonFalls();
    }
}

void SharedWorld::RequestJump(RemoteClient& c) {
    if (!c.inWorld || c.host) {
        return;
    }
    int64_t now = Now();
    uint32_t next = clock::NextHalfDay(mClock.Abs(now));
    if (mResetting || next >= clock::kMoonAbs) {
        if (!mResetting) {
            mServer.SendSystem(&c, "La Canción del Doble Tiempo no puede adelantar el tiempo más allá de la última "
                                   "noche.",
                               level::kWarn);
        }
        SendClock(&c, false); // that game already put its time back: this confirms it
        return;
    }
    mClock.Set(next, now);
    mDirty = true;
    Announce(c.nick + " ha tocado la Canción del Doble Tiempo: " + clock::Format(next) + ".", level::kInfo, nullptr);
    SendClock(nullptr, true);
}

void SharedWorld::RequestSpeed(RemoteClient& c, bool inverted) {
    if (!c.inWorld || c.host) {
        return;
    }
    if (inverted != mClock.Inverted()) {
        mClock.SetInverted(inverted, Now());
        mDirty = true;
        Announce(c.nick + (inverted ? " ha ralentizado el tiempo (Canción del Tiempo Invertida)."
                                    : " ha devuelto el tiempo a su velocidad normal."),
                 level::kInfo, nullptr);
    }
    SendClock(nullptr, false);
}

void SharedWorld::ProposeSot(RemoteClient& c) {
    if (!c.inWorld || c.host) {
        return;
    }
    if (mResetting) {
        mServer.SendSystem(&c, "Ya se está volviendo al Amanecer del Primer Día.", level::kWarn);
        return;
    }
    if (mVote.Active()) {
        mServer.SendSystem(&c, "Ya hay una votación en marcha: escribe /si o /no.", level::kWarn);
        return;
    }
    int64_t now = Now();
    mVote.Start(c.peer, now, mServer.Config().voteTimeoutMs);
    if (InWorld().size() > 1) {
        Announce(c.nick + " quiere volver al Amanecer del Primer Día. Escribe /si o /no (" +
                     std::to_string(mVote.SecondsLeft(now)) + " s).",
                 level::kInfo, nullptr);
    }
    EvaluateVote();
}

void SharedWorld::Vote(RemoteClient& c, bool yes) {
    if (!mVote.Active()) {
        mServer.SendSystem(&c, "No hay ninguna votación en marcha.", level::kWarn);
        return;
    }
    if (!c.inWorld) {
        mServer.SendSystem(&c, "Solo votan los jugadores que están en la partida del servidor.", level::kWarn);
        return;
    }
    mVote.Cast(c.peer, yes);
    std::vector<uint32_t> eligible = EligiblePeers();
    Announce(c.nick + " vota " + (yes ? "sí" : "no") + " (" + std::to_string(mVote.Yes(eligible)) + " sí, " +
                 std::to_string(mVote.No(eligible)) + " no, de " + std::to_string(eligible.size()) + ").",
             level::kInfo, nullptr);
    EvaluateVote();
}

void SharedWorld::EvaluateVote() {
    SotVote::Result result = mVote.Evaluate(EligiblePeers(), Now());
    if (result == SotVote::Result::Pending) {
        return;
    }
    uint32_t proposer = mVote.Proposer();
    mVote.Cancel();
    if (result == SotVote::Result::Failed) {
        Announce("La votación para volver al Amanecer del Primer Día no ha salido adelante.", level::kWarn, nullptr);
        return;
    }
    RemoteClient* p = mServer.Players().ByPeer(proposer);
    BeginReset(p != nullptr ? p->nick : "votación", proposer);
}

void SharedWorld::BeginReset(const std::string& by, uint32_t preferredPeer) {
    mResetting = true;
    mTried.clear();
    UpdateClock(); // stops while resetting
    Announce("Volvéis al Amanecer del Primer Día (" + by + ").", level::kOk, nullptr);
    AskCompute(preferredPeer);
}

void SharedWorld::AskCompute(uint32_t preferredPeer) {
    auto untried = [this](const RemoteClient* p) {
        return p != nullptr && p->inWorld && !p->closing &&
               std::find(mTried.begin(), mTried.end(), p->peer) == mTried.end();
    };
    RemoteClient* pick = mServer.Players().ByPeer(preferredPeer);
    if (!untried(pick)) {
        pick = nullptr;
        for (RemoteClient* p : InWorld()) {
            if (untried(p)) {
                pick = p;
                break;
            }
        }
    }
    if (pick == nullptr) {
        AbortReset();
        return;
    }
    mComputer = pick->peer;
    mTried.push_back(pick->peer);
    mComputeDeadlineMs = Now() + mServer.Config().cycleComputeTimeoutMs;
    mServer.SendEvent(*pick, MakeEvent(ev::kCycleCompute));
}

void SharedWorld::NextComputer() {
    mComputer = 0;
    AskCompute(0);
}

void SharedWorld::AbortReset() {
    mResetting = false;
    mComputer = 0;
    Announce("No se pudo volver al Amanecer del Primer Día: ningún juego respondió.", level::kError, nullptr);
    UpdateClock();
    EnterWaiting();
}

void SharedWorld::CycleResult(RemoteClient& c, const json& ev) {
    if (!mResetting || c.peer != mComputer) {
        mServer.NoteInvalid(c, "cycle_result sin haberlo pedido el servidor");
        return;
    }
    FieldSet fields;
    std::string err = "falta 'fields'";
    auto it = ev.find("fields");
    if (it == ev.end() || !WorldStore::ParseFields(*it, fields, &err)) {
        mServer.NoteInvalid(c, "cycle_result inválido: " + err);
        NextComputer();
        return;
    }
    int64_t now = Now();
    mResetting = false;
    mComputer = 0;
    mStore.StartCycle(fields, mStore.Cycle() + 1);
    mClock.Set(0, now);
    mClock.SetInverted(false, now);
    UpdateClock();
    Flush();
    for (RemoteClient* p : Receivers()) {
        mServer.SendEvent(*p, FullEvent(*p, "sot"));
    }
    mServer.Log().Info("Empieza el ciclo " + std::to_string(mStore.Cycle()) + ".");
    EnterWaiting();
}

void SharedWorld::MoonFalls() {
    int64_t now = Now();
    int oldCycle = mStore.Cycle();
    mVote.Cancel();
    mStore.RestoreCycleStart(oldCycle + 1);
    mPlayers.RestoreCycleStart(oldCycle, oldCycle + 1);
    mClock.Set(0, now);
    mClock.SetInverted(false, now);
    Flush();
    Announce("La luna ha caído. Volvéis al Amanecer del Primer Día con lo que teníais al empezar el ciclo.",
             level::kWarn, nullptr);
    for (RemoteClient* p : Receivers()) {
        mServer.SendEvent(*p, FullEvent(*p, "moon"));
    }
}

bool SharedWorld::SetTime(uint32_t abs, const std::string& by, std::string* err) {
    if (!mStore.Exists()) {
        *err = "Todavía no hay mundo: lo creará el primer jugador que entre en la partida del servidor.";
        return false;
    }
    if (mResetting) {
        *err = "Se está volviendo al Amanecer del Primer Día; espera un momento.";
        return false;
    }
    mClock.Set(abs, Now());
    mDirty = true;
    Announce("La hora ha cambiado: " + clock::Format(abs) + " (" + by + ").", level::kInfo, nullptr);
    SendClock(nullptr, true);
    return true;
}

bool SharedWorld::Restart(const std::string& by, std::string* err) {
    if (!mStore.Exists()) {
        *err = "Todavía no hay mundo: lo creará el primer jugador que entre en la partida del servidor.";
        return false;
    }
    if (mResetting) {
        *err = "Ya se está volviendo al Amanecer del Primer Día.";
        return false;
    }
    if (InWorld().empty()) {
        *err = "No hay nadie en la partida: el nuevo ciclo lo calcula el juego de un jugador.";
        return false;
    }
    mVote.Cancel();
    BeginReset(by, 0);
    return true;
}

} // namespace coop::server
