#pragma once
// One loaded mod: a Lua script (LuaMod) or a DLL plugin (PluginMod). ModHost owns them and calls back into them
// through numbered handlers: the number stands for a Lua function or a plugin's callback, and only its mod knows
// which.
#include "common/Events.h"

#include <string>
#include <vector>

namespace coop::server {

struct ModInfo {
    std::string name;        // unique, lowercase [a-z0-9_-]: the file name without its extension
    std::string kind;        // "lua", "plugin"
    std::string path;
    std::string title;       // what the mod says of itself (coop.mod.describe, CoopPluginInfo): a name to show...
    std::string version;
    std::string author;
    std::string description;
};

// What a command of a mod answers to whoever ran it.
struct CommandReply {
    std::string text; // "" = nothing to say
    std::string level = "info";
};

class Mod {
  public:
    virtual ~Mod() = default;

    const ModInfo& Info() const {
        return mInfo;
    }
    ModInfo& EditInfo() {
        return mInfo;
    }

    // False (with err) when the handler failed; the host logs it and goes on.
    // An event: the handler may change payload and ask to cancel it (the host keeps only what the event allows).
    virtual bool InvokeEvent(int handler, json& payload, bool* cancel, std::string* err) = 0;
    virtual bool InvokeTimer(int handler, std::string* err) = 0;
    // ctx: {player (missing for the console), nick, isConsole, isOp}.
    virtual bool InvokeCommand(int handler, const json& ctx, const std::vector<std::string>& args, CommandReply* reply,
                               std::string* err) = 0;
    // The host no longer refers to that handler (its subscription, timer or command is gone).
    virtual void ReleaseHandler(int handler) = 0;

  protected:
    ModInfo mInfo;
};

} // namespace coop::server
