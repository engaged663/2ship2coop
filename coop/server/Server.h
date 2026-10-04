#pragma once
// Server core: owns the transport and the player registry and routes packets to the registered
// handlers (Registry.h). Single-threaded: everything runs inside Tick().
#include "AccessLists.h"
#include "GiftManager.h"
#include "GroupBook.h"
#include "Logger.h"
#include "O2rStore.h"
#include "PlayerRegistry.h"
#include "ServerConfig.h"
#include "World/SharedWorld.h"
#include "common/Events.h"
#include "common/Protocol.h"
#include "common/Transport.h"

#include <chrono>
#include <memory>
#include <string>

namespace coop::server {

class ModHost; // Mods/ModHost.h: the scripts and plugins loaded into this server

class Server {
  public:
    Server(const ServerConfig& config, AccessLists& access, Logger& log);
    ~Server();

    bool Start(std::string* err);
    void Tick(int timeoutMs);
    void ExecuteConsoleLine(const std::string& line);
    // Graceful: says goodbye, then keeps ticking until every client acknowledged (max 1 s).
    void Stop(const std::string& reason);
    bool IsRunning() const; // true until the graceful stop has finished

    void SendEvent(RemoteClient& to, const json& ev);
    void Broadcast(const json& ev, const RemoteClient* except = nullptr); // welcomed players only
    // to == nullptr means the console (the text is logged instead).
    void SendSystem(RemoteClient* to, const std::string& text, const char* level = level::kInfo);
    void SendStream(RemoteClient& to, const uint8_t* data, size_t size);
    void SendFile(RemoteClient& to, const uint8_t* data, size_t size); // kChannelFiles: an .o2r chunk
    void Reject(RemoteClient& client, const std::string& reason); // handshake refused
    void Kick(RemoteClient& client, const std::string& reason);
    bool AllowMessage(RemoteClient& client); // chat/cmd rate limit
    bool IsOp(const RemoteClient& client) const;
    // A malformed/unknown/impossible packet: logged (cleaned) the first few times, kicked when it keeps on.
    void NoteInvalid(RemoteClient& client, const std::string& what);

    PlayerRegistry& Players() {
        return mPlayers;
    }
    AccessLists& Access() {
        return mAccess;
    }
    GiftManager& Gifts() {
        return mGifts;
    }
    GroupBook& Groups() {
        return mGroups;
    }
    SharedWorld& World() {
        return mWorld;
    }
    ModHost& Mods() {
        return *mMods;
    }
    O2rStore& O2r() { // the game mods (.o2r) the players download first
        return mO2r;
    }
    const ServerConfig& Config() const {
        return mConfig;
    }
    ServerConfig& EditConfig() { // what may change while running (mods: server.setConfig)
        return mConfig;
    }
    Logger& Log() {
        return mLog;
    }
    int64_t NowMs() const;

  private:
    void HandleReceive(NetEvent& ev);
    void HandleDisconnect(uint32_t peer);
    void CheckHandshakeTimeouts();
    void TickStopping(std::vector<NetEvent>& events);
    void LoadO2r();

    ServerConfig mConfig;
    AccessLists& mAccess;
    Logger& mLog;
    Transport mTransport;
    PlayerRegistry mPlayers;
    GiftManager mGifts;
    GroupBook mGroups;
    SharedWorld mWorld; // after mPlayers: it reads the registry
    O2rStore mO2r;
    bool mRunning = false;
    bool mStopping = false;
    int64_t mStopDeadlineMs = 0;
    std::chrono::steady_clock::time_point mStartTime;
    std::unique_ptr<ModHost> mMods; // the last member: the mods go first, while everything else still exists
};

} // namespace coop::server
