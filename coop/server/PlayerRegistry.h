#pragma once
// Every connected peer, from the moment ENet connects (handshaking) until it disconnects.
#include "common/Protocol.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace coop::server {

// Refills perSecond tokens per second up to burst; Take() spends one.
struct TokenBucket {
    double tokens;
    double burst;
    double perSecond;
    int64_t lastMs = 0;

    TokenBucket(double burstSize, double rate) : tokens(burstSize), burst(burstSize), perSecond(rate) {
    }
    bool Take(int64_t nowMs);
};

struct RemoteClient {
    uint32_t peer = 0;
    uint8_t id = 0; // 1..kMaxPlayers once welcomed
    std::string nick;
    std::string ip;
    bool welcomed = false;
    bool host = false;       // the server's own headless game (sub-project D): not a player
    uint8_t followId = 0;    // host: the player whose room it keeps loaded
    bool closing = false;    // kicked/rejected: ignore anything else it sends
    std::string leaveReason; // shown to the others when it leaves
    int64_t connectedAtMs = 0;

    // Last known location (from "loc" events and pose streams).
    int16_t scene = -1;
    int8_t room = -1;
    uint16_t entrance = 0;
    std::string sceneName;
    float pos[3] = { 0.f, 0.f, 0.f };
    int16_t rotY = 0;
    bool hasState = false;
    uint32_t streamsIn = 0;      // pose packets received (stats command)
    uint32_t streamsRelayed = 0; // pose packets forwarded to others
    uint32_t invalidMessages = 0; // malformed/unknown packets (kicked at kInvalidKickCount)

    std::deque<int64_t> recentMessagesMs; // rate limiting (chat and commands)
    TokenBucket streamBudget{ kStreamBurst, kStreamPerSecond };
    TokenBucket locBudget{ kLocBurst, kLocPerSecond };
    bool locDirty = false; // a location change is waiting for locBudget to be broadcast

    // Shared world (sub-project B)
    bool inWorld = false;     // playing in the server's world: gets its clock and changes, counts for votes
    bool timeStopped = false; // in a scene where the original game stops time (from "loc"): the clock waits
    TokenBucket wopsBudget{ kWopsBurst, kWopsPerSecond };
    TokenBucket invBudget{ kInvBurst, kInvPerSecond };
    TokenBucket worldEntryBudget{ kWorldEntryBurst, kWorldEntryPerSecond };

    // Shared enemies (sub-project C)
    bool busy = false;        // paused, text or cutscene (from "loc"): hands its rooms' enemies to someone else
    int64_t roomSinceMs = 0;  // when it arrived in its current scene/room (the oldest one there owns the room)
    TokenBucket actorBudget{ kActorStreamBurst, kActorStreamPerSecond };
    TokenBucket hitBudget{ kHitBurst, kHitPerSecond };
    TokenBucket dropBudget{ kDropBurst, kDropPerSecond };
    TokenBucket propBudget{ kPropBurst, kPropPerSecond }; // pots, grass, items (PropHandlers.cpp)
    TokenBucket endingBudget{ kEndingBurst, kEndingPerSecond }; // the end of the game (EndingHandlers.cpp)
    std::string authSent;     // the last "auth" it got (serialized): only changes are sent
    // Full replication (sub-project D3)
    TokenBucket leaseBudget{ kLeaseBurst, kLeasePerSecond };
    std::string leasesSent;   // the last "leases" it got (serialized)
    // Groups and activities (GroupHandlers.cpp, ActivityHandlers.cpp)
    std::string activityKey;      // what it runs or stands next to ("act"), "" = nothing
    std::string activityName;
    bool activityRunning = false; // it runs that activity now (act start .. act end)
    TokenBucket inviteBudget{ kInviteBurst, kInvitePerSecond };
    TokenBucket activityBudget{ kActivityBurst, kActivityPerSecond };
    TokenBucket rewardBudget{ kRewardBurst, kRewardPerSecond };
    TokenBucket effectBudget{ kEffectBurst, kEffectPerSecond }; // effects echo (EffectHandlers.cpp)
    int64_t cinemaMs = -1; // when its last "cinema" was accepted (-1: never): its actor frames pass meanwhile

    // Epona calls: monotonically increasing for one connection.
    uint32_t eponaLastCallSequence = 0;

    // Mods (Mods/ModHost.h): what scripts read of a player and what they were last told
    uint8_t form = 0;      // its Link's form and mask, from the pose
    uint8_t mask = 0;
    int16_t modScene = -1; // the last scene and room the mods heard of ("player_scene")
    int8_t modRoom = -1;
    TokenBucket modEventBudget{ kModEventBurst, kModEventPerSecond }; // "gev" (ModHandlers.cpp)
    TokenBucket statBudget{ kStatBurst, kStatPerSecond };             // "stat"
    bool hasStats = false; // its game told us its health, magic and rupees
    int32_t health = 0;    // 16 = a heart
    int32_t maxHealth = 0;
    int32_t magic = 0;
    int32_t rupees = 0;
};

class PlayerRegistry {
  public:
    RemoteClient& Add(uint32_t peer, const std::string& ip, int64_t nowMs);
    void Remove(uint32_t peer);

    RemoteClient* ByPeer(uint32_t peer);
    RemoteClient* ByNick(const std::string& nick); // welcomed only, case-insensitive
    RemoteClient* ById(uint8_t id);                 // welcomed only

    int WelcomedCount() const;  // players only (hosts do not take a slot)
    uint8_t AllocateId() const; // smallest free id in 1..kMaxPlayers + kMaxHosts, 0 if none
    std::vector<RemoteClient*> Welcomed();      // players only
    std::vector<RemoteClient*> WelcomedHosts(); // the server's headless games
    std::vector<RemoteClient*> WelcomedAll();   // both
    std::vector<RemoteClient*> All();

  private:
    std::vector<std::unique_ptr<RemoteClient>> mClients;
};

} // namespace coop::server
