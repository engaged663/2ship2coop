// Cycle rules of the shared world: Song of Double Time, Inverted Song of Time, the Song of Time vote, resets
// computed by one game (cycle_compute -> cycle_result, the original end-of-cycle code), the moon and /settime.
#include "SharedWorld.h"

#include "server/Server.h"

#include "common/Clock.h"

#include <algorithm>

namespace coop::server {

void SharedWorld::TickCycle(int64_t now) {
    if (mResetting && now >= mComputeDeadlineMs) {
        mServer.Log().Warn(Tr(Msg::ComputeLate));
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
            mServer.SendSystem(&c, Tr(Msg::DoubleTimeLimit), level::kWarn);
        }
        SendClock(&c, false); // that game already put its time back: this confirms it
        return;
    }
    mClock.Set(next, now);
    mDirty = true;
    Announce(Tr(Msg::DoubleTimePlayed, { c.nick, clock::Format(next) }), level::kInfo, nullptr);
    SendClock(nullptr, true);
}

void SharedWorld::RequestSpeed(RemoteClient& c, bool inverted) {
    if (!c.inWorld || c.host) {
        return;
    }
    if (inverted != mClock.Inverted()) {
        mClock.SetInverted(inverted, Now());
        mDirty = true;
        Announce(Tr(inverted ? Msg::InvertedOn : Msg::InvertedOff, { c.nick }), level::kInfo, nullptr);
    }
    SendClock(nullptr, false);
}

void SharedWorld::ProposeSot(RemoteClient& c) {
    if (!c.inWorld || c.host) {
        return;
    }
    if (mResetting) {
        mServer.SendSystem(&c, Tr(Msg::ResetAlready), level::kWarn);
        return;
    }
    if (mVote.Active()) {
        mServer.SendSystem(&c, Tr(Msg::VoteAlready), level::kWarn);
        return;
    }
    int64_t now = Now();
    mVote.Start(c.peer, now, mServer.Config().voteTimeoutMs);
    if (InWorld().size() > 1) {
        Announce(Tr(Msg::VoteProposed, { c.nick, std::to_string(mVote.SecondsLeft(now)) }), level::kInfo, nullptr);
    }
    EvaluateVote();
}

void SharedWorld::Vote(RemoteClient& c, bool yes) {
    if (!mVote.Active()) {
        mServer.SendSystem(&c, Tr(Msg::VoteNone), level::kWarn);
        return;
    }
    if (!c.inWorld) {
        mServer.SendSystem(&c, Tr(Msg::VoteOnlyPlayers), level::kWarn);
        return;
    }
    mVote.Cast(c.peer, yes);
    std::vector<uint32_t> eligible = EligiblePeers();
    Announce(Tr(Msg::VoteCast, { c.nick, Tr(yes ? Msg::VoteYesWord : Msg::VoteNoWord), std::to_string(mVote.Yes(eligible)),
                     std::to_string(mVote.No(eligible)), std::to_string(eligible.size()) }),
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
        Announce(Tr(Msg::VoteFailed), level::kWarn, nullptr);
        return;
    }
    RemoteClient* p = mServer.Players().ByPeer(proposer);
    BeginReset(p != nullptr ? p->nick : Tr(Msg::VoteSourceWord), proposer);
}

void SharedWorld::BeginReset(const std::string& by, uint32_t preferredPeer) {
    mResetting = true;
    mTried.clear();
    UpdateClock(); // stops while resetting
    Announce(Tr(Msg::ResetBy, { by }), level::kOk, nullptr);
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
    Announce(Tr(Msg::ResetFailed), level::kError, nullptr);
    UpdateClock();
    EnterWaiting();
}

void SharedWorld::CycleResult(RemoteClient& c, const json& ev) {
    if (!mResetting || c.peer != mComputer) {
        mServer.NoteInvalid(c, Tr(Msg::InvCycleUnasked));
        return;
    }
    FieldSet fields;
    std::string err = Tr(Msg::MissingFields);
    auto it = ev.find("fields");
    if (it == ev.end() || !WorldStore::ParseFields(*it, fields, &err)) {
        mServer.NoteInvalid(c, Tr(Msg::InvCycle, { err }));
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
    mServer.Log().Info(Tr(Msg::CycleStarted, { std::to_string(mStore.Cycle()) }));
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
    Announce(Tr(Msg::MoonFell), level::kWarn, nullptr);
    for (RemoteClient* p : Receivers()) {
        mServer.SendEvent(*p, FullEvent(*p, "moon"));
    }
}

void SharedWorld::CrashMoon() {
    if (mStore.Exists() && !mResetting) {
        MoonFalls();
    }
}

// The ending ended with the Dawn of a New Day: as the original, the world goes on from the Dawn of the First Day
// with what the Song of Time keeps (computed by a game, as for the Song of Time).
void SharedWorld::FinishGame(RemoteClient& by) {
    if (!mStore.Exists() || mResetting || !by.inWorld || by.host) {
        return;
    }
    mVote.Cancel();
    BeginReset(Tr(Msg::FinishBy, { by.nick }), by.peer);
}

bool SharedWorld::SetTime(uint32_t abs, const std::string& by, std::string* err) {
    if (!mStore.Exists()) {
        *err = Tr(Msg::NoWorldCreate);
        return false;
    }
    if (mResetting) {
        *err = Tr(Msg::ResetWait);
        return false;
    }
    mClock.Set(abs, Now());
    mDirty = true;
    Announce(Tr(Msg::TimeChanged, { clock::Format(abs), by }), level::kInfo, nullptr);
    SendClock(nullptr, true);
    return true;
}

bool SharedWorld::SetClockStopped(bool stopped, const std::string& by, std::string* err) {
    if (!mStore.Exists()) {
        *err = Tr(Msg::NoWorldCreate);
        return false;
    }
    if (mResetting) {
        *err = Tr(Msg::ResetWait);
        return false;
    }
    bool was = !mClock.Running();
    if (stopped == was) {
        *err = Tr(stopped ? Msg::ClockAlreadyStopped : Msg::ClockAlreadyRunning);
        return false;
    }
    mClock.SetRunning(!stopped, Now());
    mDirty = true;
    Announce(Tr(stopped ? Msg::ClockStopped : Msg::ClockResumed, { by }), level::kInfo, nullptr);
    SendClock(nullptr, false);
    return true;
}

bool SharedWorld::Restart(const std::string& by, std::string* err) {
    if (!mStore.Exists()) {
        *err = Tr(Msg::NoWorldCreate);
        return false;
    }
    if (mResetting) {
        *err = Tr(Msg::ResetAlready);
        return false;
    }
    if (InWorld().empty()) {
        *err = Tr(Msg::RestartNobody);
        return false;
    }
    mVote.Cancel();
    BeginReset(by, 0);
    return true;
}

} // namespace coop::server
