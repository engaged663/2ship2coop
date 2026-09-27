#include "Server.h"

#include "CommandRegistry.h"
#include "Registry.h"

#include "common/Text.h"

#include <thread>

namespace coop::server {

namespace {

std::string Describe(const RemoteClient& c) {
    return c.welcomed ? c.nick : c.ip;
}

} // namespace

Server::Server(const ServerConfig& config, AccessLists& access, Logger& log)
    : mConfig(config), mAccess(access), mLog(log), mWorld(*this, config.worldPath, config.playersDir) {
}

Server::~Server() {
    mWorld.Flush(); // also saved when stopping; this covers a server destroyed without Stop
    mTransport.Close();
}

bool Server::Start(std::string* err) {
    mStartTime = std::chrono::steady_clock::now();
    mWorld.Load();
    // Extra slots let a client connect just to be told why it cannot join (full, banned...).
    if (!mTransport.Listen(mConfig.port, (size_t)mConfig.maxPlayers + 4, err)) {
        return false;
    }
    mRunning = true;
    mLog.Info("Servidor co-op escuchando en el puerto UDP " + std::to_string(mConfig.port) + " (máx. " +
              std::to_string(mConfig.maxPlayers) + " jugadores, protocolo v" + std::to_string(kProtocolVersion) +
              ")");
    return true;
}

bool Server::IsRunning() const {
    return mRunning;
}

int64_t Server::NowMs() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - mStartTime)
        .count();
}

void Server::Tick(int timeoutMs) {
    if (!mRunning) {
        return;
    }
    std::vector<NetEvent> events;
    mTransport.Service(timeoutMs, events);
    if (mStopping) {
        TickStopping(events);
        return;
    }
    for (auto& ev : events) {
        if (ev.type == NetEvent::Connect) {
            RemoteClient& c = mPlayers.Add(ev.peer, mTransport.PeerIp(ev.peer), NowMs());
            mLog.Info("Conexión entrante desde " + c.ip);
        } else if (ev.type == NetEvent::Receive) {
            HandleReceive(ev);
        } else {
            HandleDisconnect(ev.peer);
        }
    }
    CheckHandshakeTimeouts();
    for (TickHookFn hook : TickHooks()) {
        hook(*this);
    }
}

void Server::HandleReceive(NetEvent& ev) {
    RemoteClient* c = mPlayers.ByPeer(ev.peer);
    if (c == nullptr || c->closing) {
        return;
    }
    if (ev.channel == kChannelStream) {
        if (!c->welcomed || ev.data.empty()) {
            NoteInvalid(*c, "stream antes del saludo");
        } else if (StreamHandlerFn fn = FindStreamHandler(ev.data[0])) {
            fn(*this, *c, ev.data.data(), ev.data.size());
        } else {
            NoteInvalid(*c, "stream desconocido");
        }
        return;
    }

    json parsed;
    std::string error;
    if (!ParseEvent(ev.data.data(), ev.data.size(), parsed, &error)) {
        NoteInvalid(*c, "evento inválido (" + error + ")");
        return;
    }
    std::string type = EventType(parsed);
    const EventHandlerEntry* handler = FindEventHandler(type);
    if (handler == nullptr) {
        NoteInvalid(*c, "evento desconocido '" + SanitizeChat(type, 40) + "'");
        return;
    }
    if (handler->requiresWelcome && !c->welcomed) {
        return;
    }
    handler->fn(*this, *c, parsed);
}

void Server::HandleDisconnect(uint32_t peer) {
    RemoteClient* c = mPlayers.ByPeer(peer);
    if (c == nullptr) {
        return;
    }
    for (ClientHookFn hook : DisconnectHooks()) {
        hook(*this, *c);
    }
    mPlayers.Remove(peer);
}

void Server::CheckHandshakeTimeouts() {
    int64_t now = NowMs();
    for (RemoteClient* c : mPlayers.All()) {
        if (!c->welcomed && now - c->connectedAtMs > mConfig.handshakeTimeoutMs) {
            mLog.Warn(c->ip + " no completó el saludo a tiempo; desconectado");
            uint32_t peer = c->peer;
            mTransport.DisconnectNow(peer);
            mPlayers.Remove(peer);
        }
    }
}

void Server::SendEvent(RemoteClient& to, const json& ev) {
    std::string text = SerializeEvent(ev);
    mTransport.Send(to.peer, kChannelEvents, text.data(), text.size());
}

void Server::Broadcast(const json& ev, const RemoteClient* except) {
    std::string text = SerializeEvent(ev);
    for (RemoteClient* c : mPlayers.Welcomed()) {
        if (c != except && !c->closing) {
            mTransport.Send(c->peer, kChannelEvents, text.data(), text.size());
        }
    }
}

void Server::SendSystem(RemoteClient* to, const std::string& text, const char* level) {
    if (to == nullptr) {
        mLog.Info(text);
        return;
    }
    json ev = MakeEvent(ev::kSys);
    ev["text"] = text;
    ev["level"] = level;
    SendEvent(*to, ev);
}

void Server::SendStream(RemoteClient& to, const uint8_t* data, size_t size) {
    mTransport.Send(to.peer, kChannelStream, data, size);
}

void Server::Reject(RemoteClient& client, const std::string& reason) {
    json ev = MakeEvent(ev::kReject);
    ev["reason"] = reason;
    SendEvent(client, ev);
    client.closing = true;
    mTransport.Disconnect(client.peer);
    mLog.Info("Rechazado " + client.ip + ": " + reason);
}

void Server::Kick(RemoteClient& client, const std::string& reason) {
    json ev = MakeEvent(ev::kKicked);
    ev["reason"] = reason;
    SendEvent(client, ev);
    client.closing = true;
    client.leaveReason = reason.empty() ? "expulsado" : "expulsado: " + reason;
    mTransport.Disconnect(client.peer);
    mLog.Info(Describe(client) + " " + client.leaveReason);
}

bool Server::AllowMessage(RemoteClient& client) {
    int64_t now = NowMs();
    auto& recent = client.recentMessagesMs;
    while (!recent.empty() && now - recent.front() > kRateLimitWindowMs) {
        recent.pop_front();
    }
    if ((int)recent.size() >= kRateLimitCount) {
        SendSystem(&client, "Estás enviando mensajes demasiado rápido. Espera un momento.", level::kWarn);
        return false;
    }
    recent.push_back(now);
    return true;
}

bool Server::IsOp(const RemoteClient& client) const {
    return mAccess.IsOp(client.nick, client.ip);
}

void Server::NoteInvalid(RemoteClient& client, const std::string& what) {
    client.invalidMessages++;
    if (client.invalidMessages <= (uint32_t)kInvalidLogCount) {
        mLog.Warn(SanitizeChat(what, 120) + " de " + Describe(client));
    }
    if (client.invalidMessages == (uint32_t)kInvalidKickCount) {
        if (client.welcomed) {
            Kick(client, "demasiados paquetes inválidos");
        } else {
            Reject(client, "demasiados paquetes inválidos");
        }
    }
}

void Server::ExecuteConsoleLine(const std::string& line) {
    ExecuteCommandLine(*this, nullptr, line);
}

void Server::Stop(const std::string& reason) {
    if (!mRunning || mStopping) {
        return;
    }
    mWorld.Flush();
    mLog.Info("Deteniendo el servidor: " + reason);
    for (RemoteClient* c : mPlayers.Welcomed()) {
        SendSystem(c, "Servidor detenido: " + reason, level::kWarn);
    }
    for (RemoteClient* c : mPlayers.All()) {
        c->closing = true;
        mTransport.Disconnect(c->peer); // delivered after the goodbye above
    }
    mStopping = true;
    mStopDeadlineMs = NowMs() + 1000;
}

// While stopping, Tick only waits for the clients to acknowledge (or the deadline) and then closes.
void Server::TickStopping(std::vector<NetEvent>& events) {
    for (auto& ev : events) {
        if (ev.type == NetEvent::Disconnect) {
            mPlayers.Remove(ev.peer);
        }
    }
    if (mPlayers.All().empty() || NowMs() > mStopDeadlineMs) {
        mTransport.Close();
        mRunning = false;
        mStopping = false;
        mLog.Info("Servidor detenido.");
    }
}

} // namespace coop::server
