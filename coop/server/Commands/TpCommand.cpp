// /tp <jugador>: sends the caller the target's last known location; the game does the warp.
// /tp <jugador> <destino> (ops and console): moves another player to <destino>.
#include "server/CommandRegistry.h"
#include "server/Server.h"

#include "common/Events.h"

namespace coop::server {

static void Run(CommandContext& ctx, const std::vector<std::string>& args) {
    RemoteClient* mover = ctx.sender;
    std::string destination = args[0];
    if (args.size() >= 2) {
        if (!ctx.IsConsole() && !ctx.server.IsOp(*ctx.sender)) {
            ctx.Reply(Tr(Msg::TpOnlyAdmins), level::kError);
            return;
        }
        mover = ctx.server.Players().ByNick(args[0]);
        if (mover == nullptr) {
            ctx.Reply(Tr(Msg::PlayerNotFound, { args[0] }), level::kError);
            return;
        }
        destination = args[1];
    } else if (ctx.IsConsole()) {
        ctx.Reply(Tr(Msg::TpConsoleUsage), level::kError);
        return;
    }

    RemoteClient* target = ctx.server.Players().ByNick(destination);
    if (target == nullptr) {
        ctx.Reply(Tr(Msg::PlayerNotFound, { destination }), level::kError);
        return;
    }
    if (target == mover) {
        ctx.Reply(Tr(Msg::TpSelf), level::kError);
        return;
    }
    if (!target->hasState || target->scene < 0) {
        ctx.Reply(Tr(Msg::TpNotInGame, { target->nick }), level::kError);
        return;
    }
    json tp = MakeEvent(ev::kTp);
    tp["scene"] = target->scene;
    tp["entrance"] = target->entrance;
    tp["room"] = target->room;
    tp["pos"] = { target->pos[0], target->pos[1], target->pos[2] };
    tp["rot"] = target->rotY;
    tp["target"] = target->nick;
    ctx.server.SendEvent(*mover, tp);
    ctx.Reply(Tr(Msg::TpTeleporting, { mover->nick, target->nick }));
    ctx.server.SendSystem(target, Tr(Msg::TpNotifyTarget, { mover->nick }));
}

COOP_COMMAND(tp, "tp", Msg::TpUsage, Msg::TpHelp, Perm::Player, 1, Run);

} // namespace coop::server
