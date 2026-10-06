// /sala, /listo, /s (spec 2026-10-04-coop-salas-actividades §4): the same actions as the games' room window ("room_op",
// Rooms.cpp), typed in the chat. Sub-commands in Spanish with their English names; the answers in the server's
// language.
#include "server/CommandRegistry.h"
#include "server/Groups.h"
#include "server/Rooms.h"
#include "server/Server.h"

#include "common/Text.h"

#include <initializer_list>

namespace coop::server {

namespace {

bool CheckSender(CommandContext& ctx) {
    if (ctx.IsConsole()) {
        ctx.Reply(Tr(Msg::RoomOnlyPlayers), level::kError);
        return false;
    }
    if (!Rooms_Enabled(ctx.server)) {
        ctx.Reply(Tr(Msg::RoomsOff), level::kError);
        return false;
    }
    if (!Groups_InWorld(*ctx.sender)) {
        ctx.Reply(Tr(Msg::RoomNotInWorld), level::kError);
        return false;
    }
    return true;
}

bool Is(const std::string& word, std::initializer_list<const char*> names) {
    for (const char* n : names) {
        if (EqualsIgnoreCase(word, n)) {
            return true;
        }
    }
    return false;
}

// sí / si / yes / on: 1; no / off: 0; -1 anything else.
int YesNo(const std::string& word) {
    if (Is(word, { "sí", "si", "yes", "on", "1" })) {
        return 1;
    }
    if (Is(word, { "no", "off", "0" })) {
        return 0;
    }
    return -1;
}

RemoteClient* Named(CommandContext& ctx, const std::string& nick) {
    RemoteClient* p = ctx.server.Players().ByNick(nick);
    if (p == nullptr) {
        ctx.Reply(Tr(Msg::PlayerNotFound, { nick }), level::kError);
    }
    return p;
}

void Usage(CommandContext& ctx) {
    ctx.Reply(Tr(Msg::UsageLine, { Tr(Msg::RoomUsage) }), level::kError);
}

// value -1: the other way round. Says how it stands after it (Rooms_Ready answers the refusals).
void Ready(CommandContext& ctx, int value) {
    Server& server = ctx.server;
    RemoteClient& me = *ctx.sender;
    const Room* before = server.Rooms().RoomOf(me.id);
    bool waiting = before != nullptr && (before->state == RoomState::Lobby || before->state == RoomState::Starting);
    Rooms_Ready(server, me, value);
    const Room* after = server.Rooms().RoomOf(me.id);
    const RoomMember* m = after != nullptr ? after->Member(me.id) : nullptr;
    if (waiting && m != nullptr) {
        bool ready = m->ready || after->state == RoomState::Running;
        ctx.Reply(Tr(ready ? Msg::RoomReadyYou : Msg::RoomNotReadyYou), level::kOk);
    }
}

void Invite(CommandContext& ctx, const std::vector<std::string>& who) {
    Server& server = ctx.server;
    RemoteClient& me = *ctx.sender;
    if (who.empty()) {
        Usage(ctx);
        return;
    }
    if (who.size() == 1 && Is(who[0], { "grupo", "group" })) {
        Rooms_InviteGroup(server, me);
        return;
    }
    std::vector<uint8_t> ids;
    if (who.size() == 1 && Is(who[0], { "todos", "all" })) {
        const Room* room = server.Rooms().RoomOf(me.id);
        for (RemoteClient* p : server.Players().Welcomed()) {
            if (p != &me && Groups_InWorld(*p) && (room == nullptr || room->Member(p->id) == nullptr)) {
                ids.push_back(p->id);
            }
        }
        if (ids.empty()) {
            ctx.Reply(Tr(Msg::InviteNobody), level::kWarn);
            return;
        }
    } else {
        for (const std::string& nick : who) {
            if (RemoteClient* p = Named(ctx, nick)) {
                ids.push_back(p->id);
            }
        }
    }
    if (!ids.empty()) {
        Rooms_InviteIds(server, me, ids);
    }
}

void Setting(CommandContext& ctx, const std::string& which, const std::vector<std::string>& rest) {
    int value = rest.empty() ? -1 : YesNo(rest[0]);
    if (value < 0) {
        ctx.Reply(Tr(Msg::RoomBadValue), level::kError);
        return;
    }
    int open = -1;
    int travel = -1;
    int rewards = -1;
    if (Is(which, { "abierta", "open" })) {
        open = value;
    } else if (Is(which, { "viaje", "travel" })) {
        travel = value;
    } else {
        rewards = value;
    }
    Server& server = ctx.server;
    RemoteClient& me = *ctx.sender;
    Rooms_Settings(server, me, open, travel, rewards);
    const Room* room = server.Rooms().RoomOf(me.id);
    if (room != nullptr && room->host == me.id) {
        auto yesNo = [](bool b) { return Tr(b ? Msg::RoomYes : Msg::RoomNo); };
        ctx.Reply(Tr(Msg::RoomSettingsNow,
                     { yesNo(room->settings.open), yesNo(room->settings.travel), yesNo(room->settings.rewards) }),
                  level::kOk);
    }
}

void RunRoom(CommandContext& ctx, const std::vector<std::string>& args) {
    if (!CheckSender(ctx)) {
        return;
    }
    Server& server = ctx.server;
    RemoteClient& me = *ctx.sender;
    if (args.empty()) {
        ctx.Reply(Rooms_Describe(server, me));
        return;
    }
    const std::string& sub = args[0];
    std::vector<std::string> rest(args.begin() + 1, args.end());
    if (Is(sub, { "invitar", "invite" })) {
        Invite(ctx, rest);
    } else if (Is(sub, { "aceptar", "accept" }) || Is(sub, { "rechazar", "decline" })) {
        uint8_t from = 0;
        if (!rest.empty()) {
            RemoteClient* p = Named(ctx, rest[0]);
            if (p == nullptr) {
                return;
            }
            from = p->id;
        }
        Rooms_Answer(server, me, 0, from, Is(sub, { "aceptar", "accept" }));
    } else if (Is(sub, { "listo", "ready" })) {
        int value = rest.empty() ? -1 : YesNo(rest[0]);
        if (!rest.empty() && value < 0) {
            ctx.Reply(Tr(Msg::RoomBadValue), level::kError);
            return;
        }
        Ready(ctx, value);
    } else if (Is(sub, { "salir", "leave" })) {
        Rooms_Leave(server, me);
    } else if (Is(sub, { "cerrar", "close" })) {
        Rooms_Close(server, me);
    } else if (Is(sub, { "quitar", "kick" })) {
        if (rest.empty()) {
            Usage(ctx);
            return;
        }
        if (RemoteClient* p = Named(ctx, rest[0])) {
            Rooms_Kick(server, me, p->id);
        }
    } else if (Is(sub, { "unirse", "join" })) {
        if (rest.empty()) {
            Usage(ctx);
            return;
        }
        RemoteClient* p = Named(ctx, rest[0]);
        if (p == nullptr) {
            return;
        }
        const Room* room = server.Rooms().RoomOf(p->id);
        if (room == nullptr) {
            ctx.Reply(Tr(Msg::RoomNoRoomOf, { p->nick }), level::kError);
            return;
        }
        Rooms_Join(server, me, room->id);
    } else if (Is(sub, { "abierta", "open", "viaje", "travel", "premios", "rewards" })) {
        Setting(ctx, sub, rest);
    } else if (Is(sub, { "decir", "say" })) {
        std::string text = JoinFrom(args, 1);
        if (SanitizeChat(text, kChatMaxChars).empty()) {
            ctx.Reply(Tr(Msg::RoomChatEmpty), level::kError);
            return;
        }
        Rooms_Chat(server, me, text);
    } else {
        Usage(ctx);
    }
}

void RunReady(CommandContext& ctx, const std::vector<std::string>& args) {
    if (!CheckSender(ctx)) {
        return;
    }
    int value = args.empty() ? -1 : YesNo(args[0]);
    if (!args.empty() && value < 0) {
        ctx.Reply(Tr(Msg::RoomBadValue), level::kError);
        return;
    }
    Ready(ctx, value);
}

void RunSay(CommandContext& ctx, const std::vector<std::string>& args) {
    if (!CheckSender(ctx)) {
        return;
    }
    std::string text = JoinFrom(args, 0);
    if (SanitizeChat(text, kChatMaxChars).empty()) {
        ctx.Reply(Tr(Msg::RoomChatEmpty), level::kError);
        return;
    }
    Rooms_Chat(ctx.server, *ctx.sender, text);
}

} // namespace

COOP_COMMAND(sala, "sala", Msg::RoomUsage, Msg::RoomHelp, Perm::Player, 0, RunRoom);
COOP_COMMAND(listo, "listo", Msg::ReadyUsage, Msg::ReadyHelp, Perm::Player, 0, RunReady);
COOP_COMMAND(roomSay, "s", Msg::RoomSayUsage, Msg::RoomSayHelp, Perm::Player, 1, RunSay);
COOP_ALIAS(room, "room", "sala");
COOP_ALIAS(ready, "ready", "listo");

} // namespace coop::server
