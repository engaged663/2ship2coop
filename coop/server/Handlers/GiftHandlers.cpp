// Second half of /gift: the sender's payment, the receiver's confirmation, refunds, timeouts, leaves.
#include "server/Registry.h"
#include "server/Server.h"

#include <algorithm>

namespace coop::server {

namespace {

void SendRefund(Server& server, const PendingGift& gift, int amount, const std::string& reason) {
    RemoteClient* sender = server.Players().ById(gift.fromId);
    if (sender == nullptr || amount <= 0) {
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
    if (gift == nullptr || gift->fromId != client.id || gift->stage != PendingGift::WaitDebit) {
        return;
    }
    int paid = (int)std::clamp<int64_t>(GetInt(ev, "paid"), 0, gift->amount);
    if (paid == 0) {
        server.SendSystem(&client, "No tienes suficientes rupias para regalar " + std::to_string(gift->amount) + ".",
                          level::kError);
        server.Gifts().Remove(gift->gid);
        return;
    }
    RemoteClient* target = server.Players().ById(gift->toId);
    if (target == nullptr) {
        PendingGift copy = *gift;
        server.Gifts().Remove(gift->gid);
        SendRefund(server, copy, paid, "el jugador se ha desconectado");
        return;
    }
    gift->paid = paid;
    gift->stage = PendingGift::WaitCredit;
    json credit = MakeEvent(ev::kGiftCredit);
    credit["gid"] = gift->gid;
    credit["from"] = client.nick;
    credit["amount"] = paid;
    server.SendEvent(*target, credit);
}

void OnReceived(Server& server, RemoteClient& client, const json& ev) {
    PendingGift* found = server.Gifts().Find((uint32_t)GetInt(ev, "gid"));
    if (found == nullptr || found->toId != client.id || found->stage != PendingGift::WaitCredit) {
        return;
    }
    PendingGift gift = *found;
    server.Gifts().Remove(gift.gid);
    int accepted = (int)std::clamp<int64_t>(GetInt(ev, "accepted"), 0, gift.paid);
    RemoteClient* sender = server.Players().ById(gift.fromId);
    std::string senderNick = sender != nullptr ? sender->nick : "Alguien";

    if (accepted > 0) {
        server.SendSystem(&client, senderNick + " te ha regalado " + std::to_string(accepted) + " rupias.",
                          level::kOk);
        if (sender != nullptr) {
            server.SendSystem(sender, "Has regalado " + std::to_string(accepted) + " rupias a " + client.nick + ".",
                              level::kOk);
        }
        server.Log().Info(senderNick + " regaló " + std::to_string(accepted) + " rupias a " + client.nick);
    } else if (sender != nullptr) {
        server.SendSystem(sender, client.nick + " no tiene espacio en la cartera.", level::kWarn);
    }
    SendRefund(server, gift, gift.paid - accepted, "la cartera de " + client.nick + " está llena");
}

void OnDisconnect(Server& server, RemoteClient& client) {
    if (!client.welcomed) {
        return;
    }
    for (const PendingGift& gift : server.Gifts().TakeCancelledBy(client.id)) {
        if (gift.stage == PendingGift::WaitCredit && gift.toId == client.id) {
            SendRefund(server, gift, gift.paid, client.nick + " se ha desconectado");
        }
    }
}

void OnTick(Server& server) {
    for (const PendingGift& gift : server.Gifts().TakeExpired(server.NowMs(), kGiftTimeoutMs)) {
        if (gift.stage == PendingGift::WaitCredit) {
            SendRefund(server, gift, gift.paid, "no hubo respuesta");
        } else if (RemoteClient* sender = server.Players().ById(gift.fromId)) {
            server.SendSystem(sender, "El regalo ha caducado.", level::kWarn);
        }
    }
}

} // namespace

COOP_SERVER_EVENT(giftPaid, ev::kGiftPaid, true, OnPaid);
COOP_SERVER_EVENT(giftReceived, ev::kGiftRecv, true, OnReceived);
COOP_SERVER_ON_DISCONNECT(giftLeave, OnDisconnect);
COOP_SERVER_ON_TICK(giftTimeouts, OnTick);

} // namespace coop::server
