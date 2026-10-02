#pragma once
// The shared world (sub-project B): the world's fields, the 3-day clock, each player's saved data and the cycle
// rules. Owned by Server (Server::World()); events arrive through Handlers/WorldHandlers.cpp and
// Handlers/ClockHandlers.cpp, commands through Commands/WorldCommands.cpp, mods through Mods/Api/ApiWorld.cpp.
//   SharedWorld.cpp       entering, creating, leaving, change relay, inventories, saving, when the clock runs
//   SharedWorldCycle.cpp  Double Time, Inverted Song, the Song of Time vote, resets, the moon, /settime...
// Only players "in the world" (RemoteClient::inWorld) get its clock and changes and count for the clock and votes.
#include "PlayerStore.h"
#include "SotVote.h"
#include "WorldClock.h"
#include "WorldStore.h"

#include "common/Events.h"

#include <cstdint>
#include <string>
#include <vector>

namespace coop::server {

class Server;
struct RemoteClient;

class SharedWorld {
  public:
    // Empty paths keep everything in memory (tests).
    SharedWorld(Server& server, std::string worldPath, std::string playersDir);

    void Load();  // Server::Start
    void Flush(); // saves world.json and players/ now (also every worldSaveMs and when stopping)
    void Tick();

    // Events from games
    void Enter(RemoteClient& c);
    void Leave(RemoteClient& c, bool disconnected);
    void Create(RemoteClient& c, const json& ev);
    void ApplyOps(RemoteClient& c, const json& ev);
    void Upload(RemoteClient& c, const json& ev);
    void UpdateClock(); // someone entered/left or a player's timeStopped changed
    void RequestJump(RemoteClient& c);
    void RequestSpeed(RemoteClient& c, bool inverted);
    void ProposeSot(RemoteClient& c);
    void CycleResult(RemoteClient& c, const json& ev);

    // Commands and mods
    void Vote(RemoteClient& c, bool yes);
    bool SetTime(uint32_t abs, const std::string& by, std::string* err);
    // /freezetime: the clock stays stopped until it is resumed, whoever comes or goes.
    bool SetClockStopped(bool stopped, const std::string& by, std::string* err);
    // How fast the three days pass (server.json "timeSpeed"); works without a world too (it is the speed of the one
    // to come).
    void SetSpeed(double scale, const std::string& by);
    bool Restart(const std::string& by, std::string* err);
    // A change made by the server itself (a mod): applied, saved and sent to every game as "wops" from 0. applied
    // gets what really changed (nothing when the world already was that way). False + err: no world, a reset in
    // progress or ops that do not fit the schema.
    bool ApplyServerOps(const world::Ops& ops, world::Ops* applied, std::string* err);
    // The end of the game (EndingHandlers.cpp): the rooftop's countdown ran out; a player saw the whole ending.
    void CrashMoon();
    void FinishGame(RemoteClient& by);
    bool Resetting() const {
        return mResetting;
    }
    bool ClockStopped() const {
        return mStore.Exists() && !mClock.Running();
    }
    bool Frozen() const { // stopped on purpose (/freezetime, a mod)
        return mFrozen;
    }
    bool Inverted() const {
        return mClock.Inverted();
    }
    double Speed() const {
        return mClock.Scale();
    }
    std::string TimeText();
    std::string Describe();

    bool Exists() const {
        return mStore.Exists();
    }
    int Cycle() const {
        return mStore.Cycle();
    }
    uint32_t ClockAbs();
    const WorldStore& Store() const {
        return mStore;
    }

  private:
    int64_t Now() const;
    std::vector<RemoteClient*> InWorld();   // players in the world (hosts never vote, stop the clock or compute)
    std::vector<RemoteClient*> Receivers(); // players and hosts: every game that follows the world
    std::vector<uint32_t> EligiblePeers();
    json ClockJson();
    void SendClock(RemoteClient* to, bool jump); // to == nullptr: everyone in the world
    json FullEvent(const RemoteClient& c, const char* reset);
    void Announce(const std::string& text, const char* level, const RemoteClient* except);
    void AssignCreator(RemoteClient& c);
    void NextCreator();
    void EnterWaiting();
    void Entered(RemoteClient& c); // a player is in the world now (entered, or created it): the mods hear of it
    void TellModsOfChange(uint8_t by, const world::Ops& changed);
    void Quarantine(const std::string& why);
    std::string StopReason();
    // SharedWorldCycle.cpp
    void TickCycle(int64_t now);
    void EvaluateVote();
    void BeginReset(const std::string& by, uint32_t preferredPeer, const char* reason);
    void AskCompute(uint32_t preferredPeer);
    void NextComputer();
    void AbortReset();
    void MoonFalls();

    Server& mServer;
    std::string mWorldPath;
    WorldStore mStore;
    WorldClock mClock;
    PlayerStore mPlayers;
    SotVote mVote;
    uint32_t mCreator = 0; // peer asked to create the world
    int64_t mCreatorDeadlineMs = 0;
    std::vector<uint32_t> mWaiting; // asked to enter while the world was being created or reset
    bool mResetting = false;        // waiting for a game's cycle_result
    const char* mResetReason = "";  // why: "sot", "restart", "ending" (the mods' cycle_reset event)
    uint32_t mComputer = 0;         // peer computing the new cycle
    int64_t mComputeDeadlineMs = 0;
    std::vector<uint32_t> mTried; // peers already asked during this reset
    bool mFrozen = false;         // /freezetime: the clock does not run whatever else says
    bool mDirty = false;
    int64_t mLastSaveMs = 0;
    int64_t mNextClockMs = 0;
};

} // namespace coop::server
