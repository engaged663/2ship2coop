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
            { "port", cfg.port }, { "maxPlayers", cfg.maxPlayers }, { "password", cfg.password }, { "motd", cfg.motd }
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
    cfg.port = (uint16_t)j.value("port", (int)cfg.port);
    cfg.maxPlayers = std::clamp(j.value("maxPlayers", cfg.maxPlayers), 1, kMaxPlayers);
    cfg.password = j.value("password", cfg.password);
    cfg.motd = j.value("motd", cfg.motd);
    out = cfg;
    return true;
}

} // namespace coop::server
