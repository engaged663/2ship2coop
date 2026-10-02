#include "ModHost.h"

#include "LuaMod.h"
#include "ModApi.h"
#include "PluginMod.h"

#include "server/CommandRegistry.h"
#include "server/Server.h"

#include "common/Clock.h"
#include "common/Text.h"

#include <algorithm>
#include <filesystem>

namespace coop::server {

namespace {

// While it lives, a handler of some mod is running: unloads asked meanwhile wait for the next tick.
struct Running {
    int& depth;
    explicit Running(int& d) : depth(d) {
        depth++;
    }
    ~Running() {
        depth--;
    }
};

bool TypeMatches(const std::string& type, const json& value) {
    if (type == "int") {
        return value.is_number_integer();
    }
    if (type == "number") {
        return value.is_number();
    }
    if (type == "string") {
        return value.is_string();
    }
    if (type == "bool") {
        return value.is_boolean();
    }
    if (type == "list") {
        return value.is_array();
    }
    return type == "table" && value.is_object();
}

// What a handler may change goes back into the payload: for a server event, the fields it marks as changeable (with
// their type); for an event between mods, everything.
void MergeChanges(const ModEventDef* def, const json& seen, json& payload) {
    if (!seen.is_object()) {
        return;
    }
    if (def == nullptr) {
        payload = seen;
        return;
    }
    for (const ModFieldDef& field : def->fields) {
        auto it = seen.find(field.name);
        if (field.writable && it != seen.end() && TypeMatches(field.type, *it)) {
            payload[field.name] = *it;
        }
    }
}

// A mod's name: its file's name without the extension, in lowercase, with anything outside [a-z0-9_-] as '_'.
std::string ModNameOf(const std::filesystem::path& file) {
    std::string name = ToLower(file.stem().string());
    for (char& c : name) {
        bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) {
            c = '_';
        }
    }
    return name.empty() ? "mod" : name;
}

bool ValidCommandName(const std::string& name) {
    if (name.empty() || name.size() > 24) {
        return false;
    }
    for (char c : name) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) {
            return false;
        }
    }
    return true;
}

const char* LevelOf(const std::string& text) {
    for (const char* known : { level::kOk, level::kWarn, level::kError }) {
        if (text == known) {
            return known;
        }
    }
    return level::kInfo;
}

} // namespace

ModHost::ModHost(Server& server) : mServer(server) {
}

ModHost::~ModHost() {
    while (!mMods.empty()) {
        UnloadNow(mMods.back().get(), true); // the last one loaded goes first
    }
    SaveStorage(); // after their mod_unload: a mod may store something as it goes
}

void ModHost::Start() {
    // server.json "commandPermissions". Always set, also when empty: the table of commands is shared by every server
    // of this process (the tests start many).
    std::map<std::string, Perm> perms;
    for (const auto& [name, who] : mServer.Config().commandPermissions) {
        perms[name] = who == "op" ? Perm::Op : who == "console" ? Perm::Console : Perm::Player;
    }
    SetCommandPermissions(perms);
    // server.json "gameSettings": forced on every game from the start, with or without mods.
    const json& forced = mServer.Config().gameSettings;
    if (forced.is_object()) {
        for (auto it = forced.begin(); it != forced.end(); ++it) {
            std::string err;
            if (!mSettings.Set(0, it.key(), it.value(), &err)) {
                mServer.Log().Warn(Tr(Msg::GameSettingRefused, { err }));
            }
        }
    }
    LoadConfigured();
}

void ModHost::LoadConfigured() {
    const ModsConfig& cfg = mServer.Config().mods;
    bool any = !cfg.scripts.empty() || !cfg.plugins.empty();
    if (!cfg.enabled) {
        if (any) {
            mServer.Log().Info(Tr(Msg::ModsDisabledLog));
        }
        return;
    }
    std::string warnings;
    std::vector<std::string> files = ResolveModFiles(cfg.pluginsDir, cfg.plugins, { ".dll", ".so" }, &warnings);
    std::vector<std::string> scripts = ResolveModFiles(cfg.scriptsDir, cfg.scripts, { ".lua" }, &warnings);
    files.insert(files.end(), scripts.begin(), scripts.end()); // plugins first: scripts may count on them
    if (!warnings.empty()) {
        mServer.Log().Warn(warnings);
    }
    for (const std::string& file : files) {
        std::string err;
        if (!LoadFile(file, &err)) { // one that fails is no reason to stop the server or the others
            mServer.Log().Error(Tr(Msg::ModLoadFailed, { file, err }));
        }
    }
    if (Wants(ModEvent::ServerStart)) {
        json e = json::object();
        Fire(ModEvent::ServerStart, e);
    }
}

void ModHost::Tick() {
    std::vector<std::string> unload;
    unload.swap(mPendingUnload);
    for (const std::string& name : unload) {
        if (Mod* mod = Find(name)) {
            UnloadNow(mod, true);
            mServer.Log().Info(Tr(Msg::ModUnloaded, { name }));
        }
    }
    std::vector<std::string> reload;
    reload.swap(mPendingReload);
    for (const std::string& name : reload) {
        std::string err;
        if (Find(name) != nullptr && !ReloadNow(name, &err)) {
            mServer.Log().Error(Tr(Msg::ModLoadFailed, { name, err }));
        }
    }
    RunDeferred();
    RunTimers();
    WatchClock();
    FlushSettings();
    if (mServer.NowMs() >= mNextStorageSaveMs) {
        mNextStorageSaveMs = mServer.NowMs() + kModStorageSaveMs;
        SaveStorage();
    }
}

void ModHost::Shutdown(const std::string& reason) {
    if (Wants(ModEvent::ServerStop)) {
        json e = { { "reason", reason } };
        Fire(ModEvent::ServerStop, e);
    }
    SaveStorage();
}

bool ModHost::Wants(ModEvent ev) const {
    return mWanted[(size_t)ev] > 0;
}

bool ModHost::Fire(ModEvent ev, json& payload) {
    if (!Wants(ev)) {
        return true;
    }
    const ModEventDef& def = ModEventInfo(ev);
    return Dispatch(&def, def.name, payload);
}

bool ModHost::FireCustom(const std::string& name, json& payload) {
    return Dispatch(nullptr, name, payload);
}

bool ModHost::Dispatch(const ModEventDef* def, const std::string& name, json& payload) {
    if (mDepth >= kModMaxDepth) {
        mServer.Log().Warn(Tr(Msg::ModDepthLimit, { name }));
        return true;
    }
    std::vector<uint32_t> ids; // a handler may subscribe or unsubscribe while we run
    for (const Subscription& s : mSubs) {
        if (s.name == name) {
            ids.push_back(s.id);
        }
    }
    for (uint32_t id : ids) {
        const Subscription* s = FindSub(id);
        if (s == nullptr) {
            continue; // unsubscribed by an earlier handler
        }
        Mod* mod = s->mod;
        int handler = s->handler;
        json seen = payload;
        bool cancel = false;
        std::string err;
        bool ok = false;
        {
            Running running(mDepth);
            ok = mod->InvokeEvent(handler, seen, &cancel, &err);
        }
        if (!ok) {
            ReportError(*mod, name, err);
            continue;
        }
        MergeChanges(def, seen, payload);
        if (cancel && (def == nullptr || def->cancelable)) {
            return false;
        }
    }
    return true;
}

const ModHost::Subscription* ModHost::FindSub(uint32_t id) const {
    for (const Subscription& s : mSubs) {
        if (s.id == id) {
            return &s;
        }
    }
    return nullptr;
}

bool ModHost::LoadFile(const std::string& path, std::string* err) {
    std::string ignored;
    if (err == nullptr) {
        err = &ignored;
    }
    err->clear();
    std::filesystem::path file(path);
    std::error_code ec;
    if (!std::filesystem::is_regular_file(file, ec)) {
        *err = Tr(Msg::ModFileMissing, { path });
        return false;
    }
    std::string extension = ToLower(file.extension().string());
    std::string name = ModNameOf(file);
    bool plugin = extension == ".dll" || extension == ".so";
    if (extension != ".lua" && !plugin) {
        *err = Tr(Msg::ModUnknownKind, { path });
        return false;
    }
    if (Find(name) != nullptr) {
        *err = Tr(Msg::ModNameTaken, { name });
        return false;
    }
    if (plugin) {
        std::unique_ptr<PluginMod> library = PluginMod::Open(*this, name, path, err);
        if (library == nullptr) {
            return false;
        }
        PluginMod* raw = library.get();
        Adopt(std::move(library)); // before it starts: CoopPlugin_Load already subscribes, adds commands...
        bool started = false;
        {
            Running running(mDepth);
            started = raw->Start(err);
        }
        if (!started) {
            UnloadNow(raw, false);
            return false;
        }
        mServer.Log().Info(Tr(Msg::ModLoaded, { name, raw->Info().kind }));
        return true;
    }
    const ModsConfig& cfg = mServer.Config().mods;
    LuaMod::Limits limits{ cfg.unsafeLua, cfg.scriptTimeoutMs, cfg.scriptMemoryMb, cfg.scriptsDir };
    auto script = std::make_unique<LuaMod>(*this, name, path, limits);
    LuaMod* lua = script.get();
    if (!lua->Open(err)) {
        return false;
    }
    Adopt(std::move(script)); // before it runs: its first lines already subscribe, add commands, call the API
    bool ok = false;
    {
        Running running(mDepth); // it could ask to unload a mod, itself included
        ok = lua->RunFile(err);
    }
    if (!ok) {
        UnloadNow(lua, false); // nothing of a script that failed half way stays behind
        return false;
    }
    mServer.Log().Info(Tr(Msg::ModLoaded, { name, lua->Info().kind }));
    return true;
}

bool ModHost::Unload(const std::string& name, std::string* err) {
    Mod* mod = Find(name);
    if (mod == nullptr) {
        if (err != nullptr) {
            *err = Tr(Msg::ModNotFound, { name });
        }
        return false;
    }
    if (mDepth > 0) { // a handler is running, maybe that mod's own: it goes on the next tick
        if (std::find(mPendingUnload.begin(), mPendingUnload.end(), mod->Info().name) == mPendingUnload.end()) {
            mPendingUnload.push_back(mod->Info().name);
        }
        return true;
    }
    std::string unloaded = mod->Info().name;
    UnloadNow(mod, true);
    mServer.Log().Info(Tr(Msg::ModUnloaded, { unloaded }));
    return true;
}

bool ModHost::Reload(const std::string& name, std::string* err) {
    Mod* mod = Find(name);
    if (mod == nullptr) {
        if (err != nullptr) {
            *err = Tr(Msg::ModNotFound, { name });
        }
        return false;
    }
    if (mDepth > 0) { // a handler is running, maybe that mod's own: on the next tick
        if (std::find(mPendingReload.begin(), mPendingReload.end(), mod->Info().name) == mPendingReload.end()) {
            mPendingReload.push_back(mod->Info().name);
        }
        return true;
    }
    return ReloadNow(mod->Info().name, err);
}

bool ModHost::ReloadNow(const std::string& name, std::string* err) {
    Mod* mod = Find(name);
    if (mod == nullptr) {
        return false;
    }
    std::string path = mod->Info().path;
    UnloadNow(mod, true);
    return LoadFile(path, err);
}

ModStorage& ModHost::Storage(Mod& mod) {
    const std::string& name = mod.Info().name;
    auto it = mStorage.find(name);
    if (it == mStorage.end()) {
        const std::string& dir = mServer.Config().mods.dataDir;
        std::string path = dir.empty() ? "" : (std::filesystem::path(dir) / (name + ".json")).string();
        it = mStorage.emplace(name, std::make_unique<ModStorage>(path)).first;
        if (it->second->Unreadable()) {
            mServer.Log().Warn(Tr(Msg::ModStorageBad, { path }));
        }
    }
    return *it->second;
}

void ModHost::SaveStorage() {
    for (auto& [name, storage] : mStorage) {
        std::string err;
        if (storage->Dirty() && !storage->Save(&err)) {
            mServer.Log().Error(Tr(Msg::ModStorageSaveFail, { name, err }));
        }
    }
}

void ModHost::UnloadNow(Mod* mod, bool notify) {
    if (notify) { // mod_unload: only that mod hears it
        std::vector<int> handlers;
        for (const Subscription& s : mSubs) {
            if (s.mod == mod && s.event == (int)ModEvent::ModUnload) {
                handlers.push_back(s.handler);
            }
        }
        for (int handler : handlers) {
            json e = json::object();
            bool cancel = false;
            std::string err;
            Running running(mDepth);
            if (!mod->InvokeEvent(handler, e, &cancel, &err)) {
                ReportError(*mod, ModEventInfo(ModEvent::ModUnload).name, err);
            }
        }
    }
    // Its handlers die with it: nothing of it is called again, so they are not released one by one.
    for (const Subscription& s : mSubs) {
        if (s.mod == mod && s.event >= 0) {
            mWanted[(size_t)s.event]--;
        }
    }
    mSubs.erase(std::remove_if(mSubs.begin(), mSubs.end(), [mod](const Subscription& s) { return s.mod == mod; }),
                mSubs.end());
    mTimers.erase(std::remove_if(mTimers.begin(), mTimers.end(), [mod](const Timer& t) { return t.mod == mod; }),
                  mTimers.end());
    for (const ModCommand& c : mCommands) {
        if (c.mod == mod) {
            UnregisterCommand(c.name);
        }
    }
    mCommands.erase(
        std::remove_if(mCommands.begin(), mCommands.end(), [mod](const ModCommand& c) { return c.mod == mod; }),
        mCommands.end());
    {
        std::lock_guard<std::mutex> lock(mDeferMutex);
        mDeferred.erase(std::remove_if(mDeferred.begin(), mDeferred.end(),
                                       [mod](const auto& work) { return work.first == mod; }),
                        mDeferred.end());
    }
    for (auto it = mErrorCounts.begin(); it != mErrorCounts.end();) {
        it = it->first.first == mod ? mErrorCounts.erase(it) : std::next(it);
    }
    mMods.erase(std::remove_if(mMods.begin(), mMods.end(),
                               [mod](const std::unique_ptr<Mod>& m) { return m.get() == mod; }),
                mMods.end());
}

Mod* ModHost::Find(const std::string& name) {
    for (auto& mod : mMods) {
        if (EqualsIgnoreCase(mod->Info().name, name)) {
            return mod.get();
        }
    }
    return nullptr;
}

std::vector<Mod*> ModHost::All() {
    std::vector<Mod*> out;
    for (auto& mod : mMods) {
        out.push_back(mod.get());
    }
    return out;
}

Mod* ModHost::Adopt(std::unique_ptr<Mod> mod) {
    if (mod == nullptr || Find(mod->Info().name) != nullptr) {
        return nullptr;
    }
    mMods.push_back(std::move(mod));
    return mMods.back().get();
}

uint32_t ModHost::Subscribe(Mod& mod, const std::string& event, int handler, std::string* err) {
    ModEvent ev;
    int index = -1;
    if (FindModEvent(event, ev)) {
        index = (int)ev;
    } else if (event.find(':') == std::string::npos) {
        if (err != nullptr) {
            *err = Tr(Msg::ModUnknownEvent, { SanitizeChat(event, 60) });
        }
        return 0;
    }
    uint32_t id = ++mNextId;
    mSubs.push_back({ id, &mod, handler, event, index });
    if (index >= 0) {
        mWanted[(size_t)index]++;
    }
    return id;
}

bool ModHost::Unsubscribe(Mod& mod, uint32_t id) {
    for (size_t i = 0; i < mSubs.size(); i++) {
        if (mSubs[i].id == id && mSubs[i].mod == &mod) {
            int handler = mSubs[i].handler;
            if (mSubs[i].event >= 0) {
                mWanted[(size_t)mSubs[i].event]--;
            }
            mSubs.erase(mSubs.begin() + i);
            mod.ReleaseHandler(handler);
            return true;
        }
    }
    return false;
}

uint32_t ModHost::AddTimer(Mod& mod, int handler, int64_t delayMs, bool repeat) {
    int64_t delay = std::max<int64_t>(delayMs, kModMinTimerMs);
    uint32_t id = ++mNextId;
    mTimers.push_back({ id, &mod, handler, mServer.NowMs() + delay, repeat ? delay : 0 });
    return id;
}

bool ModHost::CancelTimer(Mod& mod, uint32_t id) {
    for (size_t i = 0; i < mTimers.size(); i++) {
        if (mTimers[i].id == id && mTimers[i].mod == &mod) {
            int handler = mTimers[i].handler;
            mTimers.erase(mTimers.begin() + i);
            mod.ReleaseHandler(handler);
            return true;
        }
    }
    return false;
}

bool ModHost::AddCommand(Mod& mod, const std::string& rawName, const json& opts, int handler, std::string* err) {
    std::string name = ToLower(rawName);
    std::vector<std::string> names = { name }; // the command and its other names
    if (auto it = opts.find("aliases"); it != opts.end() && it->is_array()) {
        for (const json& alias : *it) {
            names.push_back(alias.is_string() ? ToLower(alias.get<std::string>()) : "");
        }
    }
    for (const std::string& one : names) {
        if (!ValidCommandName(one)) {
            *err = Tr(Msg::ModBadCommandName, { SanitizeChat(one, 40) });
            return false;
        }
        if (FindCommand(one) != nullptr) {
            *err = Tr(Msg::ModCommandExists, { one });
            return false;
        }
    }
    CommandDef def;
    def.name = name;
    def.ownText = true;
    def.usageText = SanitizeChat(GetString(opts, "usage"), 120);
    if (def.usageText.empty()) {
        def.usageText = "/" + name;
    }
    def.helpText = SanitizeChat(GetString(opts, "help"), 200);
    std::string perm = GetString(opts, "perm", "player");
    if (perm == "player") {
        def.perm = Perm::Player;
    } else if (perm == "op") {
        def.perm = Perm::Op;
    } else if (perm == "console") {
        def.perm = Perm::Console;
    } else {
        *err = Tr(Msg::ApiArgValue, { "perm", "player, op, console" });
        return false;
    }
    def.minArgs = (int)std::clamp<int64_t>(GetInt(opts, "minArgs", 0), 0, 8);
    def.fn = [this, name](CommandContext& ctx, const std::vector<std::string>& args) { RunCommand(name, ctx, args); };
    RegisterCommand(def);
    for (size_t i = 1; i < names.size(); i++) {
        RegisterAlias(names[i], name);
    }
    mCommands.push_back({ name, &mod, handler });
    return true;
}

bool ModHost::RemoveCommand(Mod& mod, const std::string& rawName) {
    std::string name = ToLower(rawName);
    for (size_t i = 0; i < mCommands.size(); i++) {
        if (mCommands[i].name == name && mCommands[i].mod == &mod) {
            int handler = mCommands[i].handler;
            mCommands.erase(mCommands.begin() + i);
            UnregisterCommand(name);
            mod.ReleaseHandler(handler);
            return true;
        }
    }
    return false;
}

void ModHost::RunCommand(const std::string& name, CommandContext& ctx, const std::vector<std::string>& args) {
    auto it = std::find_if(mCommands.begin(), mCommands.end(), [&name](const ModCommand& c) { return c.name == name; });
    if (it == mCommands.end()) {
        return;
    }
    Mod* mod = it->mod;
    int handler = it->handler;
    json who = { { "nick", ctx.SenderName() },
                 { "isConsole", ctx.IsConsole() },
                 { "isOp", ctx.IsConsole() || mServer.IsOp(*ctx.sender) } };
    if (!ctx.IsConsole()) {
        who["player"] = ctx.sender->id;
    }
    CommandReply reply;
    std::string err;
    bool ok = false;
    {
        Running running(mDepth);
        ok = mod->InvokeCommand(handler, who, args, &reply, &err);
    }
    if (!ok) {
        ReportError(*mod, "/" + name, err);
        ctx.Reply(Tr(Msg::ModCommandFailed), level::kError);
        return;
    }
    std::string text = SanitizeText(reply.text, kModMaxReplyChars);
    if (!text.empty()) {
        ctx.Reply(text, LevelOf(reply.level));
    }
}

void ModHost::RunTimers() {
    int64_t now = mServer.NowMs();
    std::vector<uint32_t> due; // a timer may add or cancel timers while we run
    for (const Timer& t : mTimers) {
        if (t.dueMs <= now) {
            due.push_back(t.id);
        }
    }
    for (uint32_t id : due) {
        auto it = std::find_if(mTimers.begin(), mTimers.end(), [id](const Timer& t) { return t.id == id; });
        if (it == mTimers.end()) {
            continue; // cancelled by an earlier one
        }
        Mod* mod = it->mod;
        int handler = it->handler;
        bool repeat = it->everyMs > 0;
        if (repeat) {
            it->dueMs = now + it->everyMs;
        } else {
            mTimers.erase(it);
        }
        std::string err;
        bool ok = false;
        {
            Running running(mDepth);
            ok = mod->InvokeTimer(handler, &err);
        }
        if (!ok) {
            ReportError(*mod, "timer", err);
        }
        if (!repeat) {
            mod->ReleaseHandler(handler);
        }
    }
}

// "world_hour": the world's clock moved into another hour of the game, by itself or with a jump.
void ModHost::WatchClock() {
    SharedWorld& world = mServer.World();
    if (!world.Exists()) {
        mLastHour = -1;
        return;
    }
    uint32_t abs = world.ClockAbs();
    int hour = (int)((uint64_t)abs * 24 / clock::kDayUnits);
    if (mLastHour < 0 || hour == mLastHour) {
        mLastHour = hour; // the first hour seen is not news
        return;
    }
    bool jump = hour != mLastHour + 1;
    mLastHour = hour;
    if (Wants(ModEvent::WorldHour)) {
        json e = { { "day", clock::DayOfAbs(abs) },
                   { "hour", (6 + hour) % 24 },
                   { "night", clock::IsNight(clock::TimeOfAbs(abs)) },
                   { "abs", abs },
                   { "jump", jump } };
        Fire(ModEvent::WorldHour, e);
    }
}

void ModHost::Defer(std::function<void()> fn, Mod* owner) {
    std::lock_guard<std::mutex> lock(mDeferMutex);
    mDeferred.emplace_back(owner, std::move(fn));
}

void ModHost::RunDeferred() {
    std::vector<std::pair<Mod*, std::function<void()>>> work;
    {
        std::lock_guard<std::mutex> lock(mDeferMutex);
        work.swap(mDeferred);
    }
    for (auto& [owner, fn] : work) {
        bool loaded = owner == nullptr || std::any_of(mMods.begin(), mMods.end(), [owner = owner](const auto& m) {
            return m.get() == owner;
        });
        if (loaded) { // work of a plugin that is gone would run code that is no longer there
            Running running(mDepth);
            fn();
        }
    }
}

json ModHost::Call(Mod& mod, const std::string& function, const json& args) {
    const ApiDef* def = FindApi(function);
    if (def == nullptr) {
        throw ApiError(Tr(Msg::ApiUnknownFunction, { SanitizeChat(function, 60) }));
    }
    if (!args.is_array()) {
        throw ApiError(std::string(def->name) + ": " + Tr(Msg::ApiArgsNotList));
    }
    ApiCall call{ mServer, *this, mod, def->name, args };
    try {
        return def->fn(call);
    } catch (const json::exception& e) { // a function that trusted the shape of a value
        throw ApiError(std::string(def->name) + ": " + e.what());
    }
}

json ModHost::SettingsOf(const Mod& mod) const {
    const json& all = mServer.Config().mods.settings;
    auto it = all.find(mod.Info().name);
    return (it != all.end() && it->is_object()) ? *it : json::object();
}

void ModHost::Log(const Mod& mod, const std::string& level, const std::string& text) {
    std::string line = Tr(Msg::ModLogLine, { mod.Info().name, text });
    if (level == "error") {
        mServer.Log().Error(line);
    } else if (level == "warn") {
        mServer.Log().Warn(line);
    } else {
        mServer.Log().Info(line);
    }
}

int ModHost::SendOp(const std::vector<RemoteClient*>& targets, const json& op) {
    json ev = MakeEvent(ev::kMod);
    ev["ops"] = json::array({ op });
    int sent = 0;
    for (RemoteClient* to : targets) {
        // Only the games playing in the server's world take orders: never a host, never a game on its own save.
        if (to != nullptr && to->welcomed && !to->closing && !to->host && to->inWorld) {
            mServer.SendEvent(*to, ev);
            sent++;
        }
    }
    return sent;
}

bool ModHost::SetSetting(uint8_t player, const std::string& name, const json& value, std::string* err) {
    if (!mSettings.Set(player, name, value, err)) {
        return false;
    }
    mSettingsChanged.insert(player);
    return true;
}

bool ModHost::ClearSetting(uint8_t player, const std::string& name) {
    if (!mSettings.Clear(player, name)) {
        return false;
    }
    mSettingsChanged.insert(player);
    return true;
}

void ModHost::SyncSettings(RemoteClient& client, bool evenIfEmpty) {
    json settings = mSettings.For(client.id);
    if (settings.empty() && !evenIfEmpty) {
        return;
    }
    json ev = MakeEvent(ev::kModCfg);
    ev["settings"] = std::move(settings);
    mServer.SendEvent(client, ev);
}

void ModHost::FlushSettings() {
    if (mSettingsChanged.empty()) {
        return;
    }
    std::set<uint8_t> changed;
    changed.swap(mSettingsChanged);
    bool everyone = changed.count(0) > 0;
    for (RemoteClient* c : mServer.Players().Welcomed()) {
        if (c->inWorld && !c->closing && (everyone || changed.count(c->id) > 0)) {
            SyncSettings(*c, true); // also when nothing is left: its game gives the options back to the player
        }
    }
}

void ModHost::OnPlayerGone(RemoteClient& client) {
    if (client.id != 0) { // 0 are everyone's settings, and a connection that never got in has that id
        mSettings.Forget(client.id);
        mSettingsChanged.erase(client.id);
    }
}

void ModHost::ReportError(Mod& mod, const std::string& where, const std::string& err) {
    int count = ++mErrorCounts[{ &mod, where }];
    if (count > kModErrorLogLimit) {
        return;
    }
    mServer.Log().Error(Tr(Msg::ModHandlerError, { mod.Info().name, where, err }));
    if (count == kModErrorLogLimit) {
        mServer.Log().Warn(Tr(Msg::ModErrorsSilenced, { mod.Info().name, where }));
    }
}

} // namespace coop::server
