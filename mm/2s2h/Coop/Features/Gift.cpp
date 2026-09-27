// Game side of "/gift": pay when the server asks (gift_debit), receive up to the wallet size
// (gift_credit) and take back whatever did not fit or was not delivered (gift_refund).
#include "2s2h/Coop/Chat/ChatModel.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Puppet/PoseCapture.h"

#include "common/Protocol.h"

#include <algorithm>

extern "C" {
#include "functions.h"
#include "variables.h"
}

using coop::client::Chat_Add;
using coop::client::ChatKind;

namespace {

// Rupees including the ones still being counted up/down by the HUD.
int CurrentRupees() {
    return gSaveContext.save.saveInfo.playerData.rupees + gSaveContext.rupeeAccumulator;
}

void Reply(const char* type, int64_t gid, const char* field, int value) {
    coop::json ev = coop::MakeEvent(type);
    ev["gid"] = gid;
    ev[field] = value;
    coop::client::NetClient::Get().SendEvent(ev);
}

void OnDebit(const coop::json& ev) {
    int amount = (int)coop::GetInt(ev, "amount");
    int paid = 0;
    if (PoseCapture_InGameplay() && amount > 0 && CurrentRupees() >= amount) {
        Rupees_ChangeBy((s16)-amount);
        paid = amount;
    }
    Reply(coop::ev::kGiftPaid, coop::GetInt(ev, "gid"), "paid", paid);
}

void OnCredit(const coop::json& ev) {
    int amount = (int)coop::GetInt(ev, "amount");
    int accepted = 0;
    if (PoseCapture_InGameplay() && amount > 0) {
        accepted = std::clamp(CUR_CAPACITY(UPG_WALLET) - CurrentRupees(), 0, amount);
        if (accepted > 0) {
            Rupees_ChangeBy((s16)accepted);
        }
    }
    Reply(coop::ev::kGiftRecv, coop::GetInt(ev, "gid"), "accepted", accepted);
}

void OnRefund(const coop::json& ev) {
    int amount = (int)coop::GetInt(ev, "amount");
    if (amount <= 0) {
        return;
    }
    std::string reason = coop::GetString(ev, "reason");
    if (PoseCapture_InGameplay()) {
        Rupees_ChangeBy((s16)amount);
        Chat_Add(ChatKind::Warn,
                 "Se te han devuelto " + std::to_string(amount) + " rupias" + (reason.empty() ? "." : " (" + reason + ")."));
    } else {
        Chat_Add(ChatKind::Error, "No se pudieron devolver " + std::to_string(amount) + " rupias: no estás en una partida.");
    }
}

} // namespace

COOP_ON_EVENT(giftDebit, coop::ev::kGiftDebit, OnDebit);
COOP_ON_EVENT(giftCredit, coop::ev::kGiftCredit, OnCredit);
COOP_ON_EVENT(giftRefund, coop::ev::kGiftRefund, OnRefund);
