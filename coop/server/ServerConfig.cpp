#include "ServerConfig.h"

#include "common/ModRules.h"
#include "common/Text.h"

#include <algorithm>
#include <fstream>

namespace coop::server {

namespace {

// Every public key with its default: what a new server.json holds and what an older file gains.
// (hostToken and requireSameBuild are for tests: read when present, never written.)
json Defaults(const ServerConfig& cfg) {
    json mods = { { "enabled", true },      { "scriptsDir", "mods" },
                  { "scripts", json::array({ "*" }) }, { "pluginsDir", "plugins" },
                  { "plugins", json::array({ "*" }) }, { "dataDir", "mods/data" },
                  { "unsafeLua", false },   { "scriptTimeoutMs", cfg.mods.scriptTimeoutMs },
                  { "scriptMemoryMb", cfg.mods.scriptMemoryMb }, { "settings", json::object() } };
    return { { "language", LangCode(GetLang()) },
             { "port", cfg.port },
             { "maxPlayers", cfg.maxPlayers },
             { "password", cfg.password },
             { "motd", cfg.motd },
             { "sharedEnemies", cfg.sharedEnemies },
             { "sharedProps", cfg.sharedProps },
             { "endingForAll", cfg.endingForAll },
             { "groups", cfg.groups },
             { "bossCutscenes", cfg.bossCutscenes },
             { "inviteSeconds", cfg.inviteMs / 1000 },
             { "rooms", cfg.rooms },
             { "effects", cfg.effects },
             { "sounds", cfg.sounds },
             { "ambient", cfg.ambient },
             { "playerObjects", cfg.playerObjects },
             { "sceneFlags", cfg.sceneFlags },
             { "sceneObjects", cfg.sceneObjects },
             { "ocarina", cfg.ocarina },
             { "timeSpeed", cfg.timeSpeed },
             { "voteSeconds", cfg.voteTimeoutMs / 1000 },
             { "saveSeconds", cfg.worldSaveMs / 1000 },
             { "backupMinutes", cfg.backupMs / 60000 },
             { "backupKeep", cfg.backupKeep },
             { "giftMax", cfg.giftMax },
             { "commandPermissions", json::object() },
             { "gameSettings", json::object() },
             { "mods", mods },
             { "o2r", { { "dir", "o2r" }, { "files", json::array({ "*" }) } } } };
}

// Adds the keys of defaults that j lacks (also inside "mods"). True if it added any.
bool AddMissing(json& j, const json& defaults) {
    bool added = false;
    for (auto it = defaults.begin(); it != defaults.end(); ++it) {
        if (!j.contains(it.key())) {
            j[it.key()] = it.value();
            added = true;
        } else if (it.value().is_object() && !it.value().empty() && j[it.key()].is_object()) {
            added = AddMissing(j[it.key()], it.value()) || added;
        }
    }
    return added;
}

// Reads the keys of one object of the file; a key with a wrong type or out of range keeps its default and is
// reported.
struct Reader {
    const json& obj;
    std::string prefix; // "mods." for the keys of that section
    const std::string& path;
    std::string& warnings;

    void Add(const std::string& text) {
        warnings += (warnings.empty() ? "" : "; ") + text;
    }
    void Warn(const char* key, Msg expected) {
        Add(Tr(Msg::ConfigWarn, { path, prefix + key, Tr(expected) }));
    }
    int Int(const char* key, int def, int min, int max) {
        auto it = obj.find(key);
        if (it == obj.end()) {
            return def;
        }
        if (!it->is_number_integer() || it->get<int64_t>() < min || it->get<int64_t>() > max) {
            Warn(key, Msg::ExpectInt);
            return def;
        }
        return it->get<int>();
    }
    double Number(const char* key, double def, double min, double max) {
        auto it = obj.find(key);
        if (it == obj.end()) {
            return def;
        }
        if (!it->is_number() || !(it->get<double>() >= min && it->get<double>() <= max)) {
            Warn(key, Msg::ExpectNumber);
            return def;
        }
        return it->get<double>();
    }
    std::string String(const char* key, const std::string& def) {
        auto it = obj.find(key);
        if (it == obj.end()) {
            return def;
        }
        if (!it->is_string()) {
            Warn(key, Msg::ExpectString);
            return def;
        }
        return it->get<std::string>();
    }
    bool Bool(const char* key, bool def) {
        auto it = obj.find(key);
        if (it == obj.end()) {
            return def;
        }
        if (!it->is_boolean()) {
            Warn(key, Msg::ExpectBool);
            return def;
        }
        return it->get<bool>();
    }
    std::vector<std::string> StringList(const char* key, const std::vector<std::string>& def) {
        auto it = obj.find(key);
        if (it == obj.end()) {
            return def;
        }
        bool ok = it->is_array();
        std::vector<std::string> out;
        for (size_t i = 0; ok && i < it->size(); i++) {
            ok = (*it)[i].is_string();
            if (ok) {
                out.push_back((*it)[i].get<std::string>());
            }
        }
        if (!ok) {
            Warn(key, Msg::ExpectList);
            return def;
        }
        return out;
    }
    // The object under key; nullptr (after a warning) when it is something else, or when it is missing.
    const json* Object(const char* key) {
        auto it = obj.find(key);
        if (it == obj.end()) {
            return nullptr;
        }
        if (!it->is_object()) {
            Warn(key, Msg::ExpectObject);
            return nullptr;
        }
        return &*it;
    }
};

void ReadMods(Reader top, ModsConfig& mods) {
    static const json kEmpty = json::object();
    const json* section = top.Object("mods");
    Reader r{ section != nullptr ? *section : kEmpty, "mods.", top.path, top.warnings };
    mods.enabled = r.Bool("enabled", mods.enabled);
    mods.scriptsDir = r.String("scriptsDir", mods.scriptsDir);
    mods.scripts = r.StringList("scripts", mods.scripts);
    mods.pluginsDir = r.String("pluginsDir", mods.pluginsDir);
    mods.plugins = r.StringList("plugins", mods.plugins);
    mods.dataDir = r.String("dataDir", mods.dataDir);
    mods.unsafeLua = r.Bool("unsafeLua", mods.unsafeLua);
    mods.scriptTimeoutMs = r.Int("scriptTimeoutMs", mods.scriptTimeoutMs, 100, 60000);
    mods.scriptMemoryMb = r.Int("scriptMemoryMb", mods.scriptMemoryMb, 8, 1024);
    if (const json* settings = r.Object("settings")) {
        mods.settings = *settings;
    }
}

void ReadO2r(Reader top, O2rConfig& o2r) {
    static const json kEmpty = json::object();
    const json* section = top.Object("o2r");
    Reader r{ section != nullptr ? *section : kEmpty, "o2r.", top.path, top.warnings };
    o2r.dir = r.String("dir", o2r.dir);
    o2r.files = r.StringList("files", o2r.files);
}

} // namespace

bool LoadOrCreateConfig(const std::string& path, ServerConfig& out, std::string* err) {
    ServerConfig cfg;
    json j = json::object();
    {
        std::ifstream in(path);
        if (in.is_open()) {
            j = json::parse(in, nullptr, false);
            if (j.is_discarded() || !j.is_object()) {
                if (err != nullptr) {
                    *err = Tr(Msg::ConfigInvalidJson, { path });
                }
                return false;
            }
        }
    }
    std::string warnings;
    cfg.language = GetLang();
    if (auto it = j.find("language"); it != j.end()) {
        Lang lang;
        if (it->is_string() && ParseLang(it->get<std::string>(), lang)) {
            cfg.language = lang;
            SetLang(lang); // the warnings below are already in this language
        } else {
            warnings += Tr(Msg::BadLangConfig, { path });
        }
    }
    // A new file gets every key; a file from an older server gains the ones it lacks.
    if (AddMissing(j, Defaults(cfg))) {
        std::ofstream file(path);
        file << j.dump(4) << '\n';
    }
    // What server.json means by default (a ServerConfig built in code loads no mods at all).
    cfg.mods.scriptsDir = "mods";
    cfg.mods.scripts = { "*" };
    cfg.mods.pluginsDir = "plugins";
    cfg.mods.plugins = { "*" };
    cfg.mods.dataDir = "mods/data";
    cfg.o2r.dir = "o2r";

    Reader r{ j, "", path, warnings };
    cfg.port = (uint16_t)r.Int("port", cfg.port, 1, 65535);
    cfg.maxPlayers = std::clamp(r.Int("maxPlayers", cfg.maxPlayers, 1, 1000), 1, kMaxPlayers);
    cfg.password = r.String("password", cfg.password);
    cfg.motd = r.String("motd", cfg.motd);
    cfg.hostToken = r.String("hostToken", cfg.hostToken);
    cfg.requireSameBuild = r.Bool("requireSameBuild", cfg.requireSameBuild);
    cfg.sharedEnemies = r.Bool("sharedEnemies", cfg.sharedEnemies);
    cfg.sharedProps = r.Bool("sharedProps", cfg.sharedProps);
    cfg.endingForAll = r.Bool("endingForAll", cfg.endingForAll);
    cfg.groups = r.Bool("groups", cfg.groups);
    cfg.bossCutscenes = r.Bool("bossCutscenes", cfg.bossCutscenes);
    cfg.inviteMs = r.Int("inviteSeconds", cfg.inviteMs / 1000, 10, 600) * 1000;
    cfg.rooms = r.Bool("rooms", cfg.rooms);
    cfg.effects = r.Bool("effects", cfg.effects);
    cfg.sounds = r.Bool("sounds", cfg.sounds);
    cfg.ambient = r.Bool("ambient", cfg.ambient);
    cfg.playerObjects = r.Bool("playerObjects", cfg.playerObjects);
    cfg.sceneFlags = r.Bool("sceneFlags", cfg.sceneFlags);
    cfg.sceneObjects = r.Bool("sceneObjects", cfg.sceneObjects);
    cfg.ocarina = r.Bool("ocarina", cfg.ocarina);
    cfg.timeSpeed = r.Number("timeSpeed", cfg.timeSpeed, kMinTimeSpeed, kMaxTimeSpeed);
    cfg.voteTimeoutMs = r.Int("voteSeconds", cfg.voteTimeoutMs / 1000, 10, 300) * 1000;
    cfg.worldSaveMs = r.Int("saveSeconds", cfg.worldSaveMs / 1000, 2, 600) * 1000;
    cfg.backupMs = r.Int("backupMinutes", cfg.backupMs / 60000, 0, 1440) * 60000;
    cfg.backupKeep = r.Int("backupKeep", cfg.backupKeep, 1, 500);
    cfg.giftMax = r.Int("giftMax", cfg.giftMax, 1, kGiftMaxAmount);
    if (const json* perms = r.Object("commandPermissions")) {
        for (auto it = perms->begin(); it != perms->end(); ++it) {
            std::string value = it.value().is_string() ? it.value().get<std::string>() : "";
            if (value == "player" || value == "op" || value == "console") {
                cfg.commandPermissions[ToLower(it.key())] = value;
            } else {
                r.Add(Tr(Msg::BadCommandPerm, { path, SanitizeChat(it.key(), 40), SanitizeChat(value, 40) }));
            }
        }
    }
    if (const json* settings = r.Object("gameSettings")) {
        for (auto it = settings->begin(); it != settings->end(); ++it) {
            json value = it.value().is_boolean() ? json(it.value().get<bool>() ? 1 : 0) : it.value(); // as the mods' API
            if (mods::SettingAllowed(it.key()) && mods::SettingValueAllowed(value) &&
                cfg.gameSettings.size() < (size_t)kMaxModSettings) {
                cfg.gameSettings[it.key()] = value;
            } else {
                r.Add(Tr(Msg::BadGameSetting, { path, SanitizeChat(it.key(), kMaxModSettingName) }));
            }
        }
    }
    ReadMods(r, cfg.mods);
    ReadO2r(r, cfg.o2r);
    if (err != nullptr) {
        *err = warnings;
    }
    out = cfg;
    return true;
}

std::string ApplyCommandLine(ServerConfig& config, int argc, char** argv, CommandLine* out) {
    std::string warnings;
    auto warn = [&warnings](const std::string& text) { warnings += (warnings.empty() ? "" : "; ") + text; };
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--no-mods") {
            config.mods.enabled = false;
            continue;
        }
        if (arg == "--no-o2r") {
            config.o2r.dir.clear();
            continue;
        }
        bool known = arg == "--port" || arg == "--lang" || arg == "--mods-dir" || arg == "--plugins-dir" ||
                     arg == "--script" || arg == "--plugin" || arg == "--mod-docs" || arg == "--o2r-dir";
        if (!known) {
            continue;
        }
        if (i + 1 >= argc) {
            warn(Tr(Msg::ArgNeedsValue, { arg }));
            break;
        }
        std::string value = argv[++i];
        if (arg == "--port") {
            int port = 0;
            if (ParseInt(value, port) && port > 0 && port <= 65535) {
                config.port = (uint16_t)port;
            } else {
                warn(Tr(Msg::BadPortArg, { value }));
            }
        } else if (arg == "--lang") {
            Lang lang;
            if (ParseLang(value, lang)) {
                config.language = lang;
                SetLang(lang);
            } else {
                warn(Tr(Msg::BadLangArg, { value }));
            }
        } else if (arg == "--mods-dir") {
            config.mods.scriptsDir = value;
        } else if (arg == "--plugins-dir") {
            config.mods.pluginsDir = value;
        } else if (arg == "--script") {
            config.mods.scripts.push_back(value);
        } else if (arg == "--plugin") {
            config.mods.plugins.push_back(value);
        } else if (arg == "--o2r-dir") {
            config.o2r.dir = value;
        } else if (out != nullptr) { // --mod-docs
            out->modDocsDir = value;
        }
    }
    return warnings;
}

bool SaveConfigLanguage(const std::string& path, Lang language) {
    std::ifstream in(path);
    nlohmann::json j = in.is_open() ? nlohmann::json::parse(in, nullptr, false) : nlohmann::json::object();
    if (j.is_discarded() || !j.is_object()) {
        return false;
    }
    in.close();
    j["language"] = LangCode(language);
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    file << j.dump(4) << '\n';
    return file.good();
}

json SyncOptionsJson(const ServerConfig& config) {
    return { { "sounds", config.sounds },
             { "ambient", config.ambient },
             { "playerObjects", config.playerObjects },
             { "sceneFlags", config.sceneFlags },
             { "sceneObjects", config.sceneObjects },
             { "ocarina", config.ocarina } };
}

} // namespace coop::server
