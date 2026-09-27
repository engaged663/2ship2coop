#include "PlayerRegistry.h"

#include "common/Protocol.h"
#include "common/Text.h"

#include <algorithm>

namespace coop::server {

bool TokenBucket::Take(int64_t nowMs) {
    tokens = std::min(burst, tokens + (double)(nowMs - lastMs) * perSecond / 1000.0);
    lastMs = nowMs;
    if (tokens < 1.0) {
        return false;
    }
    tokens -= 1.0;
    return true;
}

RemoteClient& PlayerRegistry::Add(uint32_t peer, const std::string& ip, int64_t nowMs) {
    auto client = std::make_unique<RemoteClient>();
    client->peer = peer;
    client->ip = ip;
    client->connectedAtMs = nowMs;
    mClients.push_back(std::move(client));
    return *mClients.back();
}

void PlayerRegistry::Remove(uint32_t peer) {
    std::erase_if(mClients, [peer](const std::unique_ptr<RemoteClient>& c) { return c->peer == peer; });
}

RemoteClient* PlayerRegistry::ByPeer(uint32_t peer) {
    for (auto& c : mClients) {
        if (c->peer == peer) {
            return c.get();
        }
    }
    return nullptr;
}

RemoteClient* PlayerRegistry::ByNick(const std::string& nick) {
    for (auto& c : mClients) {
        if (c->welcomed && !c->host && EqualsIgnoreCase(c->nick, nick)) {
            return c.get();
        }
    }
    return nullptr;
}

RemoteClient* PlayerRegistry::ById(uint8_t id) {
    for (auto& c : mClients) {
        if (c->welcomed && c->id == id) {
            return c.get();
        }
    }
    return nullptr;
}

int PlayerRegistry::WelcomedCount() const {
    int count = 0;
    for (auto& c : mClients) {
        count += (c->welcomed && !c->host) ? 1 : 0;
    }
    return count;
}

uint8_t PlayerRegistry::AllocateId() const {
    for (int id = 1; id <= kMaxPlayers + kMaxHosts; id++) {
        bool used = false;
        for (auto& c : mClients) {
            used = used || (c->welcomed && c->id == id);
        }
        if (!used) {
            return (uint8_t)id;
        }
    }
    return 0;
}

std::vector<RemoteClient*> PlayerRegistry::Welcomed() {
    std::vector<RemoteClient*> out;
    for (auto& c : mClients) {
        if (c->welcomed && !c->host) {
            out.push_back(c.get());
        }
    }
    return out;
}

std::vector<RemoteClient*> PlayerRegistry::WelcomedHosts() {
    std::vector<RemoteClient*> out;
    for (auto& c : mClients) {
        if (c->welcomed && c->host) {
            out.push_back(c.get());
        }
    }
    return out;
}

std::vector<RemoteClient*> PlayerRegistry::WelcomedAll() {
    std::vector<RemoteClient*> out;
    for (auto& c : mClients) {
        if (c->welcomed) {
            out.push_back(c.get());
        }
    }
    return out;
}

std::vector<RemoteClient*> PlayerRegistry::All() {
    std::vector<RemoteClient*> out;
    for (auto& c : mClients) {
        out.push_back(c.get());
    }
    return out;
}

} // namespace coop::server
