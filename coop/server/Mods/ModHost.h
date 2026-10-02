#pragma once
// The mod host: owns the loaded mods (Lua scripts, DLL plugins), hands them the server's events and runs their
// timers. Server owns one (Server::Mods()). Everything runs on the server's thread, inside Server::Tick; only Defer
// may be called from another thread. Map of the mod system: coop/README.md ("Mods").
#include "GameSettings.h"
#include "Mod.h"
#include "ModEvents.h"
#include "ModStorage.h"

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace coop::server {

class Server;
struct CommandContext;
struct RemoteClient;

constexpr int kModMaxDepth = 8;        // an event fired from inside a handler, and so on: this deep at most
constexpr int64_t kModMinTimerMs = 10; // the shortest timer
constexpr int kModErrorLogLimit = 20;  // errors of one handler that reach the log
constexpr size_t kModMaxReplyChars = 2000; // characters of what a mod's command answers
constexpr int64_t kModStorageSaveMs = 2000; // the mods' data is written this often (when it changed)

// The files a list of server.json names, in order (ModLoader.cpp): "*" = every file of dir with one of the
// extensions (not of its subfolders), sorted by name; "!name" leaves that file out; anything else is a file of dir
// (its extension may be left out) or a path. A file named twice comes once. Missing files are named in warnings.
std::vector<std::string> ResolveModFiles(const std::string& dir, const std::vector<std::string>& entries,
                                         const std::vector<std::string>& extensions, std::string* warnings);

class ModHost {
  public:
    explicit ModHost(Server& server);
    ~ModHost(); // unloads every mod

    void Start();                             // Server::Start, once the server listens
    void Tick();                              // every Server::Tick: pending unloads, deferred work, timers
    void Shutdown(const std::string& reason); // Server::Stop: the mods hear server_stop

    // --- Events (ModEvents.inc) ---
    bool Wants(ModEvent ev) const; // someone listens: only then is the payload worth building
    // Calls the handlers in the order they subscribed. False = one of them cancelled it (cancelable events only).
    // payload comes back with the changes the handlers made to the fields the event lets them change.
    bool Fire(ModEvent ev, json& payload);
    // An event between mods (its name has a ':'): the whole payload may be changed, and it may be cancelled.
    bool FireCustom(const std::string& name, json& payload);

    // --- Mods ---
    // A script (.lua) or a plugin (.dll, .so); its name is the file's name without the extension.
    bool LoadFile(const std::string& path, std::string* err);
    // At once, or on the next tick while a handler runs (it could be that mod's own).
    bool Unload(const std::string& name, std::string* err);
    // Unloads it and loads its file again (what it stored is kept). False + err when it is not loaded or its file no
    // longer loads (then it stays unloaded). While a handler runs it waits for the next tick, like Unload.
    bool Reload(const std::string& name, std::string* err);
    Mod* Find(const std::string& name); // any case
    std::vector<Mod*> All();            // in load order
    // The host owns it from now on. nullptr (and the mod is gone) when the name is taken.
    Mod* Adopt(std::unique_ptr<Mod> mod);

    // --- For the engines (LuaMod, PluginMod) ---
    // 0 (with err) for an event that does not exist.
    uint32_t Subscribe(Mod& mod, const std::string& event, int handler, std::string* err);
    bool Unsubscribe(Mod& mod, uint32_t id);
    uint32_t AddTimer(Mod& mod, int handler, int64_t delayMs, bool repeat); // kModMinTimerMs at least
    bool CancelTimer(Mod& mod, uint32_t id);
    // A /command of that mod. opts: {usage, help, perm ("player" | "op" | "console"), minArgs, aliases [...]}.
    // False (with err) for a name that is not valid or that already exists.
    bool AddCommand(Mod& mod, const std::string& name, const json& opts, int handler, std::string* err);
    bool RemoveCommand(Mod& mod, const std::string& name); // only its own
    // From any thread: fn runs on the server's thread in its next Tick. With an owner, only if that mod is still
    // loaded by then.
    void Defer(std::function<void()> fn, Mod* owner = nullptr);
    // A function of the API (ModApi.h) called by that mod. Throws ApiError.
    json Call(Mod& mod, const std::string& function, const json& args);
    json SettingsOf(const Mod& mod) const; // server.json mods.settings.<its name>; {} when it has none
    // That mod's own data (coop.storage): read on first use, kept across its reloads.
    ModStorage& Storage(Mod& mod);
    void Log(const Mod& mod, const std::string& level, const std::string& text); // "[name] text"
    // A handler failed: logged with the mod's name, kModErrorLogLimit times per handler at most.
    void ReportError(Mod& mod, const std::string& where, const std::string& err);

    // --- The games of the players in the server's world (Api/ApiGame.cpp) ---
    // An order ({op, ...}) for the games of those players, as a "mod" event. How many games got it.
    int SendOp(const std::vector<RemoteClient*>& targets, const json& op);
    // The game settings the server forces (GameSettings.h); player 0 = everyone. A change reaches the games it
    // touches on the next tick, in one "mod_cfg" each however many settings changed.
    bool SetSetting(uint8_t player, const std::string& name, const json& value, std::string* err);
    bool ClearSetting(uint8_t player, const std::string& name);
    const GameSettings& Settings() const {
        return mSettings;
    }
    // Sends that game its forced settings now (it entered the world); evenIfEmpty: also when it has none.
    void SyncSettings(RemoteClient& client, bool evenIfEmpty);
    void OnPlayerGone(RemoteClient& client); // disconnected: what was forced on it alone goes with it

    Server& Owner() {
        return mServer;
    }

  private:
    struct Subscription {
        uint32_t id;
        Mod* mod;
        int handler;
        std::string name;
        int event; // index of its ModEvent, -1 for an event between mods
    };
    struct Timer {
        uint32_t id;
        Mod* mod;
        int handler;
        int64_t dueMs;
        int64_t everyMs; // 0: once
    };

    struct ModCommand {
        std::string name;
        Mod* mod;
        int handler;
    };

    bool Dispatch(const ModEventDef* def, const std::string& name, json& payload);
    const Subscription* FindSub(uint32_t id) const;
    void UnloadNow(Mod* mod, bool notify);
    bool ReloadNow(const std::string& name, std::string* err);
    void LoadConfigured(); // the plugins and scripts of the config, then server_start
    void SaveStorage();
    void RunCommand(const std::string& name, CommandContext& ctx, const std::vector<std::string>& args);
    void RunDeferred();
    void RunTimers();
    void WatchClock();    // the world_hour event
    void FlushSettings(); // mod_cfg to the games whose forced settings changed

    Server& mServer;
    std::vector<std::unique_ptr<Mod>> mMods;
    std::vector<Subscription> mSubs; // in subscription order: the order the handlers are called in
    std::array<int, (size_t)ModEvent::Count> mWanted{};
    std::vector<Timer> mTimers;
    std::vector<ModCommand> mCommands;
    uint32_t mNextId = 0;
    int mDepth = 0; // handlers running now (nested)
    int mLastHour = -1; // the hour of the world last seen (hours since day 1, 6:00); -1: no world
    std::vector<std::string> mPendingUnload;
    std::vector<std::string> mPendingReload;
    std::map<std::string, std::unique_ptr<ModStorage>> mStorage; // by mod name: it outlives a reload of its mod
    int64_t mNextStorageSaveMs = 0;
    std::map<std::pair<const Mod*, std::string>, int> mErrorCounts;
    std::mutex mDeferMutex;
    std::vector<std::pair<Mod*, std::function<void()>>> mDeferred; // (owner or nullptr, work)
    GameSettings mSettings;
    std::set<uint8_t> mSettingsChanged; // players whose game must hear of its settings (0: everyone's)
};

} // namespace coop::server
