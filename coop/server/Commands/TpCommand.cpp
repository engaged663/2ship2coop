// /tp <jugador>: sends the caller the target's last known location; the game does the warp.
#include "server/CommandRegistry.h"
#include "server/Server.h"

#include "common/Events.h"

namespace coop::server {

static void Run(CommandContext& ctx, const std::vector<std::string>& args) {
    if (ctx.IsConsole()) {
        ctx.Reply("Solo los jugadores pueden usar /tp.", level::kError);
        return;
    }
    RemoteClient* target = ctx.server.Players().ByNick(args[0]);
    if (target == nullptr) {
        ctx.Reply("No hay ningún jugador conectado llamado '" + args[0] + "'.", level::kError);
        return;
    }
    if (target == ctx.sender) {
        ctx.Reply("No puedes teletransportarte a ti mismo.", level::kError);
        return;
    }
    if (!target->hasState || target->scene < 0) {
        ctx.Reply(target->nick + " todavía no está dentro del juego.", level::kError);
        return;
    }
    json tp = MakeEvent(ev::kTp);
    tp["scene"] = target->scene;
    tp["entrance"] = target->entrance;
    tp["room"] = target->room;
    tp["pos"] = { target->pos[0], target->pos[1], target->pos[2] };
    tp["rot"] = target->rotY;
    tp["target"] = target->nick;
    ctx.server.SendEvent(*ctx.sender, tp);
    ctx.Reply("Teletransportando a " + target->nick + "...");
    ctx.server.SendSystem(target, ctx.sender->nick + " se está teletransportando a ti.");
}

COOP_COMMAND(tp, "tp", "/tp <jugador>", "teletransportarte a un jugador", Perm::Player, 1, Run);

} // namespace coop::server
