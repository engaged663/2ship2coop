#pragma once
// A DLL plugin as a mod: a shared library that speaks the C ABI of coop/sdk/coop_plugin.h. The functions of that
// ABI are the static members below; the plugin's callbacks are the "handlers" the host knows by number.
#include "Mod.h"

#define COOP_PLUGIN_HOST // the plugin's exports are not ours to declare
#include "sdk/coop_plugin.h"

#include <memory>
#include <string>
#include <vector>

namespace coop::server {

class ModHost;

class PluginMod : public Mod {
  public:
    // Loads the library and checks it is a plugin made for this server's ABI. nullptr (with err) when it is not.
    static std::unique_ptr<PluginMod> Open(ModHost& host, const std::string& name, const std::string& path,
                                           std::string* err);
    ~PluginMod() override; // CoopPlugin_Unload, then the library goes

    // Runs CoopPlugin_Load; the host must already own this mod (the plugin subscribes, adds commands...).
    bool Start(std::string* err);

    bool InvokeEvent(int handler, json& payload, bool* cancel, std::string* err) override;
    bool InvokeTimer(int handler, std::string* err) override;
    bool InvokeCommand(int handler, const json& ctx, const std::vector<std::string>& args, CommandReply* reply,
                       std::string* err) override;
    void ReleaseHandler(int handler) override;

  private:
    explicit PluginMod(ModHost& host);

    // One callback of the plugin; its handler number is its place in mCallbacks + 1.
    struct Callback {
        CoopEventFn event = nullptr;
        CoopTimerFn timer = nullptr;
        CoopCommandFn command = nullptr;
        void* user = nullptr;
        std::string name; // the event it listens to
        bool used = false;
    };
    int Keep(const Callback& callback);
    bool Take(int handler, Callback& out) const;

    // The CoopApi the plugin gets (host = this).
    static const char* AbiCall(void* host, const char* function, const char* argsJson);
    static uint32_t AbiOn(void* host, const char* event, CoopEventFn fn, void* user);
    static void AbiOff(void* host, uint32_t subscription);
    static uint32_t AbiTimer(void* host, int32_t ms, int32_t repeat, CoopTimerFn fn, void* user);
    static void AbiCancelTimer(void* host, uint32_t timer);
    static int32_t AbiCommand(void* host, const char* name, const char* optsJson, CoopCommandFn fn, void* user);
    static void AbiDefer(void* host, CoopTimerFn fn, void* user);
    static void AbiLog(void* host, int32_t level, const char* text);

    ModHost& mHost;
    void* mLibrary = nullptr;
    int32_t (*mLoad)(const CoopApi*, CoopPluginInfo*) = nullptr;
    void (*mUnload)() = nullptr;
    bool mStarted = false;
    CoopApi mApi{};
    std::vector<Callback> mCallbacks;
    std::string mAnswer; // what the last AbiCall returned: valid until the plugin's next call
};

} // namespace coop::server
