// /op and /deop (console only): ops may use /kick, /ban, /unban, /banlist and /say from the game chat.
// An op is bound to the IP the player has when /op runs, so the player must be connected.
#include "server/CommandRegistry.h"
#include "server/Server.h"

#include "common/Text.h"

namespace coop::server {

namespace {

void Op(CommandContext& ctx, const std::vector<std::string>& args) {
    RemoteClient* target = ctx.server.Players().ByNick(args[0]);
    if (target == nullptr) {
        ctx.Reply(Tr(Msg::OpNotConnected, { args[0] }), level::kError);
        return;
    }
    if (!ctx.server.Access().AddOp(target->nick, target->ip)) {
        ctx.Reply(Tr(Msg::OpAlready, { target->nick }), level::kWarn);
        return;
    }
    ctx.Reply(Tr(Msg::OpDone, { target->nick, target->ip }), level::kOk);
    ctx.server.SendSystem(target, Tr(Msg::OpNotice), level::kOk);
}

void Deop(CommandContext& ctx, const std::vector<std::string>& args) {
    if (!ctx.server.Access().RemoveOp(args[0])) {
        ctx.Reply(Tr(Msg::DeopNone, { args[0] }), level::kWarn);
        return;
    }
    ctx.Reply(Tr(Msg::DeopDone, { args[0] }), level::kOk);
    if (RemoteClient* target = ctx.server.Players().ByNick(args[0])) {
        ctx.server.SendSystem(target, Tr(Msg::DeopNotice), level::kWarn);
    }
}

} // namespace

COOP_COMMAND(op, "op", Msg::OpUsage, Msg::OpHelp, Perm::Console, 1, Op);
COOP_COMMAND(deop, "deop", Msg::DeopUsage, Msg::DeopHelp, Perm::Console, 1, Deop);

} // namespace coop::server
