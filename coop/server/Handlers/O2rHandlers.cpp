// The game mods (.o2r) download (docs/superpowers/specs/2026-10-03-coop-mods-o2r-design.md): a game asks for a chunk
// with "o2r_get" {i, off} and gets it on kChannelFiles. Requests wait in a short queue per player and are served at
// kO2rPerSecond (also on every tick), so a game asking too much is cut off instead of filling the server's memory.
#include "server/O2rStore.h"
#include "server/Registry.h"
#include "server/Server.h"

#include "common/O2r.h"

namespace coop::server {

namespace {

void Serve(Server& server, RemoteClient& client) {
    int64_t now = server.NowMs();
    std::vector<uint8_t> data;
    while (!client.o2rQueue.empty() && !client.closing && client.o2rBudget.Take(now)) {
        auto [index, offset] = client.o2rQueue.front();
        client.o2rQueue.pop_front();
        if (!server.O2r().ReadChunk(index, offset, kO2rChunkBytes, data)) {
            const std::string& name = server.O2r().Entries()[index].name;
            server.Log().Warn(Tr(Msg::O2rReadFailed, { name }));
            server.SendSystem(&client, Tr(Msg::O2rReadFailedPlayer, { name }), level::kError);
            client.o2rQueue.clear();
            return;
        }
        std::vector<uint8_t> packet = o2r::EncodeChunk(index, offset, data.data(), data.size());
        server.SendFile(client, packet.data(), packet.size());
    }
}

void OnGet(Server& server, RemoteClient& client, const json& ev) {
    const std::vector<o2r::Entry>& entries = server.O2r().Entries();
    int64_t index = GetInt(ev, "i", -1);
    int64_t offset = GetInt(ev, "off", -1);
    if (index < 0 || index >= (int64_t)entries.size() || offset < 0 || (uint64_t)offset >= entries[index].size) {
        server.NoteInvalid(client, Tr(Msg::InvO2rGet));
        return;
    }
    if (client.o2rQueue.size() >= (size_t)kO2rQueueMax) {
        server.NoteInvalid(client, Tr(Msg::InvO2rFlood));
        return;
    }
    client.o2rQueue.push_back({ (uint8_t)index, (uint32_t)offset });
    Serve(server, client);
}

void OnTick(Server& server) {
    for (RemoteClient* client : server.Players().WelcomedAll()) {
        if (!client->o2rQueue.empty()) {
            Serve(server, *client);
        }
    }
}

} // namespace

COOP_SERVER_EVENT(o2rGet, ev::kO2rGet, true, OnGet);
COOP_SERVER_ON_TICK(o2rTick, OnTick);

} // namespace coop::server
