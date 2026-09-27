// /gift <jugador> <cantidad>: starts a rupee transfer (see GiftManager.h for the flow).
#include "server/CommandRegistry.h"
#include "server/Server.h"

#include "common/Events.h"
#include "common/Text.h"

namespace coop::server {

static void Run(CommandContext& ctx, const std::vector<std::string>& args) {
    if (ctx.IsConsole()) {
        ctx.Reply("Solo los jugadores pueden usar /gift.", level::kError);
        return;
    }
    RemoteClient* target = ctx.server.Players().ByNick(args[0]);
    if (target == nullptr) {
        ctx.Reply("No hay ningún jugador conectado llamado '" + args[0] + "'.", level::kError);
        return;
    }
    if (target == ctx.sender) {
        ctx.Reply("No puedes regalarte rupias a ti mismo.", level::kError);
        return;
    }
    int amount = 0;
    if (!ParseInt(args[1], amount)) {
        ctx.Reply("La cantidad debe ser un número entero.", level::kError);
        return;
    }
    if (amount < 1 || amount > kGiftMaxAmount) {
        ctx.Reply("La cantidad debe estar entre 1 y " + std::to_string(kGiftMaxAmount) + ".", level::kError);
        return;
    }
    PendingGift& gift = ctx.server.Gifts().Create(ctx.sender->peer, target->peer, ctx.sender->nick, target->nick, amount,
                                                  ctx.server.NowMs());
    json debit = MakeEvent(ev::kGiftDebit);
    debit["gid"] = gift.gid;
    debit["to"] = target->nick;
    debit["amount"] = amount;
    ctx.server.SendEvent(*ctx.sender, debit);
}

COOP_COMMAND(gift, "gift", "/gift <jugador> <cantidad>", "regalar rupias a un jugador", Perm::Player, 2, Run);

} // namespace coop::server
