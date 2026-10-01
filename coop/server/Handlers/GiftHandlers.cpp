// Second half of /gift: the sender's payment, the receiver's confirmation, refunds, timeouts, leaves.
// Rupees are never lost: a payment that arrives after the gift was cancelled is refunded.
#include "server/Registry.h"
#include "server/Server.h"

#include <algorithm>

namespace coop::server {

namespace {

// The sender's connection, if it is still here (never a newcomer who reused its player id).
RemoteClient* Sender(Server& server, const PendingGift& gift) {
    RemoteClient* sender = server.Players().ByPeer(gift.fromPeer);
    return sender != nullptr && sender->welcomed && !sender->closing ? sender : nullptr;
}

void SendRefund(Server& server, const PendingGift& gift, int amount, const std::string& reason) {
    if (amount <= 0) {
        return;
    }
    RemoteClient* sender = Sender(server, gift);
    if (sender == nullptr) {
        server.Log().Warn(Tr(Msg::GiftRefundLost, { std::to_string(amount), gift.fromNick }));
        return;
    }
    json refund = MakeEvent(ev::kGiftRefund);
    refund["gid"] = gift.gid;
    refund["amount"] = amount;
    refund["reason"] = reason;
    server.SendEvent(*sender, refund);
}

void OnPaid(Server& server, RemoteClient& client, const json& ev) {
    PendingGift* gift = server.Gifts().Find((uint32_t)GetInt(ev, "gid"));
    if (gift == nullptr || gift->fromPeer != client.peer || gift->stage == PendingGift::WaitCredit) {
        return;
    }
    int paid = (int)std::clamp<int64_t>(GetInt(ev, "paid"), 0, gift->amount);
    PendingGift copy = *gift;
    if (gift->stage == PendingGift::RefundOnDebit) {
        server.Gifts().Remove(copy.gid);
        SendRefund(server, copy, paid, Tr(Msg::GiftCancelled));
        return;
    }
    if (paid == 0) {
        server.SendSystem(&client, Tr(Msg::GiftNotEnough, { std::to_string(gift->amount) }), level::kError);
        server.Gifts().Remove(copy.gid);
        return;
    }
    RemoteClient* target = server.Players().ByPeer(gift->toPeer);
    if (target == nullptr || !target->welcomed || target->closing) {
        server.Gifts().Remove(copy.gid);
        SendRefund(server, copy, paid, Tr(Msg::GiftDisconnected, { copy.toNick }));
        return;
    }
    gift->paid = paid;
    gift->stage = PendingGift::WaitCredit;
    json credit = MakeEvent(ev::kGiftCredit);
    credit["gid"] = gift->gid;
    credit["from"] = gift->fromNick;
    credit["amount"] = paid;
    server.SendEvent(*target, credit);
}

void OnReceived(Server& server, RemoteClient& client, const json& ev) {
    PendingGift* found = server.Gifts().Find((uint32_t)GetInt(ev, "gid"));
    if (found == nullptr || found->toPeer != client.peer || found->stage != PendingGift::WaitCredit) {
        return;
    }
    PendingGift gift = *found;
    server.Gifts().Remove(gift.gid);
    int accepted = (int)std::clamp<int64_t>(GetInt(ev, "accepted"), 0, gift.paid);
    RemoteClient* sender = Sender(server, gift);

    if (accepted > 0) {
        server.SendSystem(&client, Tr(Msg::GiftReceived, { gift.fromNick, std::to_string(accepted) }), level::kOk);
        if (sender != nullptr) {
            server.SendSystem(sender, Tr(Msg::GiftSent, { std::to_string(accepted), client.nick }), level::kOk);
        }
        server.Log().Info(Tr(Msg::GiftLog, { gift.fromNick, std::to_string(accepted), client.nick }));
    } else if (sender != nullptr) {
        server.SendSystem(sender, Tr(Msg::GiftNoRoom, { client.nick }), level::kWarn);
    }
    SendRefund(server, gift, gift.paid - accepted, Tr(Msg::GiftWalletFull, { client.nick }));
}

void OnDisconnect(Server& server, RemoteClient& client) {
    for (const PendingGift& gift : server.Gifts().OnPlayerLeft(client.peer)) {
        SendRefund(server, gift, gift.paid, Tr(Msg::GiftDisconnected, { client.nick }));
    }
}

void OnTick(Server& server) {
    const int timeoutMs = server.Config().giftTimeoutMs;
    for (const PendingGift& gift : server.Gifts().Expire(server.NowMs(), timeoutMs, kGiftOrphanMs)) {
        if (gift.stage == PendingGift::WaitCredit) {
            SendRefund(server, gift, gift.paid, Tr(Msg::GiftNoReply));
        } else if (RemoteClient* sender = Sender(server, gift)) {
            server.SendSystem(sender, Tr(Msg::GiftExpired), level::kWarn);
        }
    }
}

} // namespace

COOP_SERVER_EVENT(giftPaid, ev::kGiftPaid, true, OnPaid);
COOP_SERVER_EVENT(giftReceived, ev::kGiftRecv, true, OnReceived);
COOP_SERVER_ON_DISCONNECT(giftLeave, OnDisconnect);
COOP_SERVER_ON_TICK(giftTimeouts, OnTick);

} // namespace coop::server
