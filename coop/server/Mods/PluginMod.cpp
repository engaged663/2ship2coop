#include "PluginMod.h"

#include "ModApi.h"
#include "ModHost.h"

#include "common/I18n.h"
#include "common/Text.h"

#include <filesystem>
#include <system_error>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace coop::server {

namespace {

constexpr size_t kMaxLogChars = 2000;

std::string Dump(const json& value) {
    return value.dump(-1, ' ', false, json::error_handler_t::replace);
}

// The library at that path, or nullptr with why.
void* OpenLibrary(const std::string& path, std::string* why) {
    std::error_code ec;
    std::filesystem::path full = std::filesystem::absolute(path, ec); // a bare name would search the system's folders
    if (ec) {
        full = path;
    }
#ifdef _WIN32
    UINT mode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX); // never a dialog on a headless server
    HMODULE lib = LoadLibraryW(full.c_str());
    DWORD code = GetLastError();
    SetErrorMode(mode);
    if (lib == nullptr) {
        *why = "error " + std::to_string(code);
    }
    return lib;
#else
    void* lib = dlopen(full.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (lib == nullptr) {
        const char* text = dlerror();
        *why = text != nullptr ? text : "dlopen";
    }
    return lib;
#endif
}

void* Symbol(void* library, const char* name) {
#ifdef _WIN32
    return (void*)GetProcAddress((HMODULE)library, name);
#else
    return dlsym(library, name);
#endif
}

void CloseLibrary(void* library) {
#ifdef _WIN32
    FreeLibrary((HMODULE)library);
#else
    dlclose(library);
#endif
}

std::string Text(const char* text) { // what a plugin says of itself
    return text != nullptr ? SanitizeChat(ToValidUtf8(text), 200) : "";
}

} // namespace

PluginMod::PluginMod(ModHost& host) : mHost(host) {
}

std::unique_ptr<PluginMod> PluginMod::Open(ModHost& host, const std::string& name, const std::string& path,
                                           std::string* err) {
    std::string why;
    void* library = OpenLibrary(path, &why);
    if (library == nullptr) {
        *err = Tr(Msg::PluginOpenFailed, { path, why });
        return nullptr;
    }
    std::unique_ptr<PluginMod> mod(new PluginMod(host));
    mod->mLibrary = library; // closed by the destructor from here on
    mod->mInfo.name = name;
    mod->mInfo.kind = "plugin";
    mod->mInfo.path = path;
    auto abi = (uint32_t(*)())Symbol(library, "CoopPlugin_Abi");
    mod->mLoad = (int32_t(*)(const CoopApi*, CoopPluginInfo*))Symbol(library, "CoopPlugin_Load");
    mod->mUnload = (void (*)())Symbol(library, "CoopPlugin_Unload");
    if (abi == nullptr || mod->mLoad == nullptr) {
        *err = Tr(Msg::PluginNoEntry, { path });
        return nullptr;
    }
    uint32_t theirs = abi();
    if (theirs != COOP_PLUGIN_ABI) {
        *err = Tr(Msg::PluginAbiMismatch, { path, std::to_string(theirs), std::to_string(COOP_PLUGIN_ABI) });
        return nullptr;
    }
    return mod;
}

PluginMod::~PluginMod() {
    if (mStarted && mUnload != nullptr) {
        mUnload();
    }
    if (mLibrary != nullptr) {
        CloseLibrary(mLibrary);
    }
}

bool PluginMod::Start(std::string* err) {
    mApi.abi = COOP_PLUGIN_ABI;
    mApi.size = sizeof(CoopApi);
    mApi.host = this;
    mApi.call = AbiCall;
    mApi.on = AbiOn;
    mApi.off = AbiOff;
    mApi.timer = AbiTimer;
    mApi.cancel_timer = AbiCancelTimer;
    mApi.command = AbiCommand;
    mApi.defer = AbiDefer;
    mApi.log = AbiLog;
    CoopPluginInfo info{};
    if (mLoad(&mApi, &info) == 0) {
        *err = Tr(Msg::PluginLoadRefused, { mInfo.path });
        return false;
    }
    mStarted = true;
    mInfo.title = Text(info.name); // the plugin's texts live in the library: copied now
    mInfo.version = Text(info.version);
    mInfo.author = Text(info.author);
    mInfo.description = Text(info.description);
    return true;
}

int PluginMod::Keep(const Callback& callback) {
    for (size_t i = 0; i < mCallbacks.size(); i++) {
        if (!mCallbacks[i].used) {
            mCallbacks[i] = callback;
            mCallbacks[i].used = true;
            return (int)i + 1;
        }
    }
    mCallbacks.push_back(callback);
    mCallbacks.back().used = true;
    return (int)mCallbacks.size();
}

// A copy: the plugin may add callbacks (and move the list) while one of them runs.
bool PluginMod::Take(int handler, Callback& out) const {
    if (handler < 1 || handler > (int)mCallbacks.size() || !mCallbacks[handler - 1].used) {
        return false;
    }
    out = mCallbacks[handler - 1];
    return true;
}

void PluginMod::ReleaseHandler(int handler) {
    if (handler >= 1 && handler <= (int)mCallbacks.size()) {
        mCallbacks[handler - 1] = Callback{};
    }
}

bool PluginMod::InvokeEvent(int handler, json& payload, bool* cancel, std::string* err) {
    Callback cb;
    if (!Take(handler, cb) || cb.event == nullptr) {
        *err = "no such handler";
        return false;
    }
    std::string text = Dump(payload);
    const char* answer = cb.event(cb.user, cb.name.c_str(), text.c_str());
    if (answer == nullptr) {
        return true; // nothing to change
    }
    json changes = json::parse(answer, nullptr, false);
    if (!changes.is_object()) {
        *err = Tr(Msg::PluginBadAnswer);
        return false;
    }
    for (auto it = changes.begin(); it != changes.end(); ++it) {
        if (it.key() == "cancel") {
            *cancel = *cancel || (it.value().is_boolean() && it.value().get<bool>());
        } else {
            payload[it.key()] = it.value(); // the host keeps only what the event lets a handler change
        }
    }
    return true;
}

bool PluginMod::InvokeTimer(int handler, std::string* err) {
    Callback cb;
    if (!Take(handler, cb) || cb.timer == nullptr) {
        *err = "no such handler";
        return false;
    }
    cb.timer(cb.user);
    return true;
}

bool PluginMod::InvokeCommand(int handler, const json& ctx, const std::vector<std::string>& args, CommandReply* reply,
                              std::string* err) {
    Callback cb;
    if (!Take(handler, cb) || cb.command == nullptr) {
        *err = "no such handler";
        return false;
    }
    std::string who = Dump(ctx);
    std::string list = Dump(json(args));
    const char* answer = cb.command(cb.user, who.c_str(), list.c_str());
    if (answer == nullptr) {
        return true;
    }
    json said = json::parse(answer, nullptr, false);
    if (said.is_object() && said.contains("text")) { // {"text", "level"}
        reply->text = ToValidUtf8(GetString(said, "text"));
        reply->level = GetString(said, "level", "info");
    } else {
        reply->text = ToValidUtf8(answer); // a plain text
    }
    return true;
}

// ---- The functions the plugin calls ----

const char* PluginMod::AbiCall(void* host, const char* function, const char* argsJson) {
    PluginMod* self = (PluginMod*)host;
    json answer;
    json args = json::parse(argsJson != nullptr ? argsJson : "[]", nullptr, false);
    if (function == nullptr || args.is_discarded()) {
        answer = { { "ok", false }, { "error", Tr(Msg::ApiArgsNotList) } };
    } else {
        try {
            json value = self->mHost.Call(*self, function, args);
            answer = json::object();
            answer["ok"] = true;
            answer["value"] = std::move(value);
        } catch (const ApiError& e) {
            answer = { { "ok", false }, { "error", e.what() } };
        }
    }
    self->mAnswer = Dump(answer);
    return self->mAnswer.c_str();
}

uint32_t PluginMod::AbiOn(void* host, const char* event, CoopEventFn fn, void* user) {
    PluginMod* self = (PluginMod*)host;
    if (event == nullptr || fn == nullptr) {
        return 0;
    }
    Callback cb;
    cb.event = fn;
    cb.user = user;
    cb.name = event;
    int handler = self->Keep(cb);
    std::string err;
    uint32_t id = self->mHost.Subscribe(*self, event, handler, &err);
    if (id == 0) {
        self->ReleaseHandler(handler);
        self->mHost.Log(*self, "warn", err);
    }
    return id;
}

void PluginMod::AbiOff(void* host, uint32_t subscription) {
    PluginMod* self = (PluginMod*)host;
    self->mHost.Unsubscribe(*self, subscription);
}

uint32_t PluginMod::AbiTimer(void* host, int32_t ms, int32_t repeat, CoopTimerFn fn, void* user) {
    PluginMod* self = (PluginMod*)host;
    if (fn == nullptr) {
        return 0;
    }
    Callback cb;
    cb.timer = fn;
    cb.user = user;
    return self->mHost.AddTimer(*self, self->Keep(cb), ms, repeat != 0);
}

void PluginMod::AbiCancelTimer(void* host, uint32_t timer) {
    PluginMod* self = (PluginMod*)host;
    self->mHost.CancelTimer(*self, timer);
}

int32_t PluginMod::AbiCommand(void* host, const char* name, const char* optsJson, CoopCommandFn fn, void* user) {
    PluginMod* self = (PluginMod*)host;
    if (name == nullptr || fn == nullptr) {
        return 0;
    }
    json opts = json::parse(optsJson != nullptr ? optsJson : "{}", nullptr, false);
    if (!opts.is_object()) {
        opts = json::object();
    }
    Callback cb;
    cb.command = fn;
    cb.user = user;
    int handler = self->Keep(cb);
    std::string err;
    if (!self->mHost.AddCommand(*self, name, opts, handler, &err)) {
        self->ReleaseHandler(handler);
        self->mHost.Log(*self, "warn", err);
        return 0;
    }
    return 1;
}

// The one function another thread may call: the host's queue has its own lock.
void PluginMod::AbiDefer(void* host, CoopTimerFn fn, void* user) {
    PluginMod* self = (PluginMod*)host;
    if (fn != nullptr) {
        self->mHost.Defer([fn, user] { fn(user); }, self);
    }
}

void PluginMod::AbiLog(void* host, int32_t level, const char* text) {
    PluginMod* self = (PluginMod*)host;
    const char* name = level == COOP_LOG_ERROR ? "error" : level == COOP_LOG_WARN ? "warn" : "info";
    self->mHost.Log(*self, name, SanitizeText(text != nullptr ? text : "", kMaxLogChars));
}

} // namespace coop::server
