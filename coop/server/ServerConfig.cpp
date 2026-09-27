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
            { "port", cfg.port }, { "maxPlayers", cfg.maxPlayers }, { "password", cfg.password }, { "motd", cfg.motd },
            { "sharedEnemies", cfg.sharedEnemies }
        };
        std::ofstream file(path);
        file << defaults.dump(4) << '\n';
        out = cfg;
        return true;
    }

    nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
    if (j.is_discarded() || !j.is_object()) {
        if (err != nullptr) {
            *err = path + " no es un JSON válido";
        }
        return false;
    }
    std::string warnings;
    auto warn = [&](const char* key, const char* expected) {
        warnings += (warnings.empty() ? "" : "; ") + path + ": '" + key + "' debe ser " + expected +
                    "; se usa el valor por defecto";
    };
    auto readInt = [&](const char* key, int def, int min, int max) {
        auto it = j.find(key);
        if (it == j.end()) {
            return def;
        }
        if (!it->is_number_integer() || it->get<int64_t>() < min || it->get<int64_t>() > max) {
            warn(key, "un número entero");
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
            warn(key, "un texto entre comillas");
            return def;
        }
        return it->get<std::string>();
    };
    cfg.port = (uint16_t)readInt("port", cfg.port, 1, 65535);
    cfg.maxPlayers = std::clamp(readInt("maxPlayers", cfg.maxPlayers, 1, 1000), 1, kMaxPlayers);
    cfg.password = readString("password", cfg.password);
    cfg.motd = readString("motd", cfg.motd);
    cfg.hostToken = readString("hostToken", cfg.hostToken);
    if (auto it = j.find("sharedEnemies"); it != j.end()) {
        if (it->is_boolean()) {
            cfg.sharedEnemies = it->get<bool>();
        } else {
            warn("sharedEnemies", "true o false");
        }
    }
    if (err != nullptr) {
        *err = warnings;
    }
    out = cfg;
    return true;
}

} // namespace coop::server
