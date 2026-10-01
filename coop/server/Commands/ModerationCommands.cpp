// /kick, /ban, /unban, /banlist.
#include "server/CommandRegistry.h"
#include "server/Server.h"

#include "common/Text.h"

namespace coop::server {

namespace {

void Kick(CommandContext& ctx, const std::vector<std::string>& args) {
    RemoteClient* target = ctx.server.Players().ByNick(args[0]);
    if (target == nullptr) {
        ctx.Reply(Tr(Msg::PlayerNotFound, { args[0] }), level::kError);
        return;
    }
    std::string nick = target->nick;
    ctx.server.Kick(*target, JoinFrom(args, 1));
    ctx.Reply(Tr(Msg::KickDone, { nick }), level::kOk);
}

void Ban(CommandContext& ctx, const std::vector<std::string>& args) {
    std::string reason = JoinFrom(args, 1);
    RemoteClient* target = ctx.server.Players().ByNick(args[0]);
    if (target != nullptr) {
        std::string nick = target->nick;
        ctx.server.Access().Ban(nick, target->ip, reason);
        ctx.server.Kick(*target, reason.empty() ? Tr(Msg::BanKickReason) : Tr(Msg::BanKickReasonText, { reason }));
        ctx.Reply(Tr(Msg::BanDone, { nick }), level::kOk);
    } else {
        if (!IsValidNick(args[0])) {
            ctx.Reply(Tr(Msg::BadNick, { args[0] }), level::kError);
            return;
        }
        ctx.server.Access().Ban(args[0], "", reason);
        ctx.Reply(Tr(Msg::BanNickOnly, { args[0] }), level::kOk);
    }
}

void Unban(CommandContext& ctx, const std::vector<std::string>& args) {
    if (ctx.server.Access().Unban(args[0])) {
        ctx.Reply(Tr(Msg::UnbanDone, { args[0] }), level::kOk);
    } else {
        ctx.Reply(Tr(Msg::UnbanNone, { args[0] }), level::kError);
    }
}

void BanList(CommandContext& ctx, const std::vector<std::string>& args) {
    const auto& bans = ctx.server.Access().Bans();
    if (bans.empty()) {
        ctx.Reply(Tr(Msg::BanListEmpty));
        return;
    }
    std::string text = Tr(Msg::BanListTitle, { std::to_string(bans.size()) });
    for (const auto& b : bans) {
        text += "\n" + (b.nick.empty() ? std::string("-") : b.nick) + " " + (b.ip.empty() ? "" : "[" + b.ip + "] ") +
                b.date + (b.reason.empty() ? "" : " - " + b.reason);
    }
    ctx.Reply(text);
}

} // namespace

COOP_COMMAND(kick, "kick", Msg::KickUsage, Msg::KickHelp, Perm::Op, 1, Kick);
COOP_COMMAND(ban, "ban", Msg::BanUsage, Msg::BanHelp, Perm::Op, 1, Ban);
COOP_COMMAND(unban, "unban", Msg::UnbanUsage, Msg::UnbanHelp, Perm::Op, 1, Unban);
COOP_COMMAND(banlist, "banlist", Msg::BanlistUsage, Msg::BanlistHelp, Perm::Op, 0, BanList);

} // namespace coop::server
