// Mod API: server.* (the server itself: its settings, its log, its console).
#include "server/CommandRegistry.h"
#include "server/Mods/ModApi.h"
#include "server/Mods/ModHost.h"
#include "server/Server.h"

#include "common/Text.h"

namespace coop::server {

namespace {

json ServerInfo(ApiCall& call) {
    const ServerConfig& cfg = call.server.Config();
    return { { "protocol", kProtocolVersion },
             { "language", LangCode(GetLang()) },
             { "port", cfg.port },
             { "maxPlayers", cfg.maxPlayers },
             { "players", call.server.Players().WelcomedCount() },
             { "uptimeMs", call.server.NowMs() },
             { "timeSpeed", call.server.World().Speed() },
             { "mods", call.host.All().size() } };
}

json ServerNow(ApiCall& call) {
    return call.server.NowMs();
}

json ServerLog(ApiCall& call) {
    std::string text = call.Str(0, "text");
    std::string level = call.StrOr(1, "level", "info", 8);
    if (level != "info" && level != "warn" && level != "error") {
        call.Fail(Tr(Msg::ApiArgValue, { "level", "info, warn, error" }));
    }
    call.host.Log(call.mod, level, text);
    return nullptr;
}

json ServerExec(ApiCall& call) {
    std::string out;
    ExecuteCommandLine(call.server, nullptr, call.Str(0, "line", 1000), &out);
    return out;
}

json ServerStop(ApiCall& call) {
    call.server.Stop(SanitizeChat(call.StrOr(0, "reason", Tr(Msg::StopByMod, { call.mod.Info().name })), kChatMaxChars));
    return nullptr;
}

// One line per key of server.json a mod may read; `set` for the ones that may change while the server runs (the
// new value is argument 1; it is not written to server.json).
struct ConfigKey {
    const char* name;
    json (*get)(const ServerConfig& cfg);
    void (*set)(ApiCall& call, ServerConfig& cfg);
};

const ConfigKey kConfigKeys[] = {
    { "port", [](const ServerConfig& c) -> json { return c.port; }, nullptr },
    { "language", [](const ServerConfig&) -> json { return LangCode(GetLang()); }, nullptr },
    { "maxPlayers", [](const ServerConfig& c) -> json { return c.maxPlayers; },
      [](ApiCall& call, ServerConfig& c) { c.maxPlayers = (int)call.Int(1, "value", 1, kMaxPlayers); } },
    { "motd", [](const ServerConfig& c) -> json { return c.motd; },
      [](ApiCall& call, ServerConfig& c) { c.motd = SanitizeChat(call.Str(1, "value"), 400); } },
    { "sharedEnemies", [](const ServerConfig& c) -> json { return c.sharedEnemies; }, nullptr },
    { "sharedProps", [](const ServerConfig& c) -> json { return c.sharedProps; }, nullptr },
    { "endingForAll", [](const ServerConfig& c) -> json { return c.endingForAll; }, nullptr },
    { "groups", [](const ServerConfig& c) -> json { return c.groups; }, nullptr },
    { "bossCutscenes", [](const ServerConfig& c) -> json { return c.bossCutscenes; },
      [](ApiCall& call, ServerConfig& c) { c.bossCutscenes = call.Bool(1, "value"); } },
    { "inviteSeconds", [](const ServerConfig& c) -> json { return c.inviteMs / 1000; },
      [](ApiCall& call, ServerConfig& c) { c.inviteMs = (int)call.Int(1, "value", 10, 600) * 1000; } },
    { "effects", [](const ServerConfig& c) -> json { return c.effects; },
      [](ApiCall& call, ServerConfig& c) { c.effects = call.Bool(1, "value"); } },
    { "timeSpeed", [](const ServerConfig& c) -> json { return c.timeSpeed; },
      [](ApiCall& call, ServerConfig& c) {
          c.timeSpeed = call.Number(1, "value", kMinTimeSpeed, kMaxTimeSpeed);
          call.server.World().SetSpeed(c.timeSpeed, call.mod.Info().name);
      } },
    { "voteSeconds", [](const ServerConfig& c) -> json { return c.voteTimeoutMs / 1000; },
      [](ApiCall& call, ServerConfig& c) { c.voteTimeoutMs = (int)call.Int(1, "value", 10, 300) * 1000; } },
    { "saveSeconds", [](const ServerConfig& c) -> json { return c.worldSaveMs / 1000; },
      [](ApiCall& call, ServerConfig& c) { c.worldSaveMs = (int)call.Int(1, "value", 2, 600) * 1000; } },
    { "giftMax", [](const ServerConfig& c) -> json { return c.giftMax; },
      [](ApiCall& call, ServerConfig& c) { c.giftMax = (int)call.Int(1, "value", 1, kGiftMaxAmount); } },
    { "commandPermissions", [](const ServerConfig& c) -> json { return c.commandPermissions; }, nullptr },
    { "gameSettings", [](const ServerConfig& c) -> json { return c.gameSettings; }, nullptr },
    // Secrets: never readable. The password can be set.
    { "password", nullptr, [](ApiCall& call, ServerConfig& c) { c.password = call.Str(1, "value", 64); } },
    { "hostToken", nullptr, nullptr },
};

const ConfigKey& KeyOf(ApiCall& call) {
    std::string key = call.Str(0, "key", 64);
    for (const ConfigKey& k : kConfigKeys) {
        if (key == k.name) {
            return k;
        }
    }
    call.Fail(Tr(Msg::ApiUnknownKey, { SanitizeChat(key, 40) }));
}

json ServerConfigGet(ApiCall& call) {
    const ConfigKey& key = KeyOf(call);
    if (key.get == nullptr) {
        call.Fail(Tr(Msg::ApiSecretKey, { key.name }));
    }
    return key.get(call.server.Config());
}

json ServerConfigSet(ApiCall& call) {
    const ConfigKey& key = KeyOf(call);
    if (key.set == nullptr) {
        call.Fail(Tr(Msg::ApiReadOnlyKey, { key.name }));
    }
    key.set(call, call.server.EditConfig());
    return nullptr;
}

} // namespace

COOP_MOD_API(serverInfo, "server.info", "", "table",
             "Server data: `protocol` (protocol version), `language` (`es`, `en`, `zh`, `ru`), `port`, "
             "`maxPlayers`, `players` (connected now), `uptimeMs`, `timeSpeed` and `mods` (how many are loaded).",
             ServerInfo);
COOP_MOD_API(serverNow, "server.now", "", "number",
             "Milliseconds since the server started. Useful to measure durations; it is not the time of day.",
             ServerNow);
COOP_MOD_API(serverLog, "server.log", "text, level?", "nothing",
             "Writes a line in the server log (console and `logs/server.log`) with the mod's name in front. "
             "`level`: `info` (default), `warn` or `error`.",
             ServerLog);
COOP_MOD_API(serverExec, "server.exec", "line", "text",
             "Runs a command as if it were typed in the server console (with every permission), for example "
             "`coop.server.exec(\"kick Ana spam\")`. Returns what the command answered.",
             ServerExec);
COOP_MOD_API(serverConfig, "server.config", "key", "value",
             "Reads an option of `server.json`: `port`, `language`, `maxPlayers`, `motd`, `sharedEnemies`, "
             "`sharedProps`, `endingForAll`, `groups`, `bossCutscenes`, `inviteSeconds`, `effects`, `timeSpeed`, "
             "`voteSeconds`, `saveSeconds`, `giftMax`, `commandPermissions`, `gameSettings`. The password cannot "
             "be read.",
             ServerConfigGet);
COOP_MOD_API(serverSetConfig, "server.setConfig", "key, value", "nothing",
             "Changes an option while the server is running (it is not saved to `server.json`): `motd`, `password`, "
             "`maxPlayers` (1-4), `inviteSeconds`, `bossCutscenes`, `effects`, `voteSeconds`, `saveSeconds`, "
             "`giftMax` and `timeSpeed`.",
             ServerConfigSet);
COOP_MOD_API(serverStop, "server.stop", "reason?", "nothing",
             "Stops the server in an orderly way (warns the players and saves the world).", ServerStop);

} // namespace coop::server
