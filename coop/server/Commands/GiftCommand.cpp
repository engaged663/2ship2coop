// /gift <jugador> <cantidad>: starts a rupee transfer (see GiftManager.h for the flow).
#include "server/CommandRegistry.h"
#include "server/Server.h"

#include "common/Events.h"
#include "common/Text.h"

namespace coop::server {

static void Run(CommandContext& ctx, const std::vector<std::string>& args) {
    if (ctx.IsConsole()) {
        ctx.Reply(Tr(Msg::GiftOnlyPlayers), level::kError);
        return;
    }
    RemoteClient* target = ctx.server.Players().ByNick(args[0]);
    if (target == nullptr) {
        ctx.Reply(Tr(Msg::PlayerNotFound, { args[0] }), level::kError);
        return;
    }
    if (target == ctx.sender) {
        ctx.Reply(Tr(Msg::GiftSelf), level::kError);
        return;
    }
    int amount = 0;
    if (!ParseInt(args[1], amount)) {
        ctx.Reply(Tr(Msg::GiftAmountInt), level::kError);
        return;
    }
    int giftMax = ctx.server.Config().giftMax; // server.json "giftMax"
    if (amount < 1 || amount > giftMax) {
        ctx.Reply(Tr(Msg::GiftAmountRange, { std::to_string(giftMax) }), level::kError);
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

COOP_COMMAND(gift, "gift", Msg::GiftUsage, Msg::GiftHelp, Perm::Player, 2, Run);

} // namespace coop::server
