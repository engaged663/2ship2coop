#include "ServerConfig.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>

namespace coop::server {

bool LoadOrCreateConfig(const std::string& path, ServerConfig& out, std::string* err) {
    ServerConfig cfg;
    std::ifstream in(path);
    if (!in.is_open()) {
        nlohmann::json defaults = {
            { "language", LangCode(GetLang()) }, { "port", cfg.port }, { "maxPlayers", cfg.maxPlayers },
            { "password", cfg.password }, { "motd", cfg.motd },
            { "sharedEnemies", cfg.sharedEnemies }, { "sharedProps", cfg.sharedProps },
            { "endingForAll", cfg.endingForAll }, { "groups", cfg.groups },
            { "bossCutscenes", cfg.bossCutscenes }, { "inviteSeconds", cfg.inviteMs / 1000 },
            { "effects", cfg.effects }
        };
        std::ofstream file(path);
        file << defaults.dump(4) << '\n';
        cfg.language = GetLang();
        out = cfg;
        return true;
    }

    nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
    if (j.is_discarded() || !j.is_object()) {
        if (err != nullptr) {
            *err = Tr(Msg::ConfigInvalidJson, { path });
        }
        return false;
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
    auto warn = [&](const char* key, Msg expected) {
        warnings += (warnings.empty() ? "" : "; ") + Tr(Msg::ConfigWarn, { path, key, Tr(expected) });
    };
    auto readInt = [&](const char* key, int def, int min, int max) {
        auto it = j.find(key);
        if (it == j.end()) {
            return def;
        }
        if (!it->is_number_integer() || it->get<int64_t>() < min || it->get<int64_t>() > max) {
            warn(key, Msg::ExpectInt);
            return def;
        }
        return it->get<int>();
    };
    auto readString = [&](const char* key, const std::string& def) {
        auto it = j.find(key);
        if (it == j.end()) {
            return def;
        }
        if (!it->is_string()) {
            warn(key, Msg::ExpectString);
            return def;
        }
        return it->get<std::string>();
    };
    cfg.port = (uint16_t)readInt("port", cfg.port, 1, 65535);
    cfg.maxPlayers = std::clamp(readInt("maxPlayers", cfg.maxPlayers, 1, 1000), 1, kMaxPlayers);
    cfg.password = readString("password", cfg.password);
    cfg.motd = readString("motd", cfg.motd);
    cfg.hostToken = readString("hostToken", cfg.hostToken);
    if (auto it = j.find("requireSameBuild"); it != j.end()) {
        if (it->is_boolean()) {
            cfg.requireSameBuild = it->get<bool>();
        } else {
            warn("requireSameBuild", Msg::ExpectBool);
        }
    }
    auto readBool = [&](const char* key, bool& value) {
        if (auto it = j.find(key); it != j.end()) {
            if (it->is_boolean()) {
                value = it->get<bool>();
            } else {
                warn(key, Msg::ExpectBool);
            }
        }
    };
    readBool("sharedEnemies", cfg.sharedEnemies);
    readBool("sharedProps", cfg.sharedProps);
    readBool("endingForAll", cfg.endingForAll);
    readBool("groups", cfg.groups);
    readBool("bossCutscenes", cfg.bossCutscenes);
    cfg.inviteMs = readInt("inviteSeconds", cfg.inviteMs / 1000, 10, 600) * 1000;
    readBool("effects", cfg.effects);
    if (err != nullptr) {
        *err = warnings;
    }
    out = cfg;
    return true;
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

} // namespace coop::server
