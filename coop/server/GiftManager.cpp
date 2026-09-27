#include "GiftManager.h"

namespace coop::server {

PendingGift& GiftManager::Create(uint32_t fromPeer, uint32_t toPeer, const std::string& fromNick,
                                 const std::string& toNick, int amount, int64_t nowMs) {
    PendingGift gift;
    gift.gid = mNextGid++;
    gift.fromPeer = fromPeer;
    gift.toPeer = toPeer;
    gift.fromNick = fromNick;
    gift.toNick = toNick;
    gift.amount = amount;
    gift.createdMs = nowMs;
    mGifts.push_back(gift);
    return mGifts.back();
}

PendingGift* GiftManager::Find(uint32_t gid) {
    for (auto& gift : mGifts) {
        if (gift.gid == gid) {
            return &gift;
        }
    }
    return nullptr;
}

void GiftManager::Remove(uint32_t gid) {
    std::erase_if(mGifts, [gid](const PendingGift& g) { return g.gid == gid; });
}

std::vector<PendingGift> GiftManager::OnPlayerLeft(uint32_t peer) {
    std::vector<PendingGift> refunds;
    std::erase_if(mGifts, [&](PendingGift& g) {
        if (g.toPeer == peer && g.stage == PendingGift::WaitCredit) {
            refunds.push_back(g);
            return true;
        }
        if (g.toPeer == peer && g.stage == PendingGift::WaitDebit) {
            g.stage = PendingGift::RefundOnDebit; // the payment may already be on its way
            return false;
        }
        return g.fromPeer == peer && g.stage != PendingGift::WaitCredit;
    });
    return refunds;
}

std::vector<PendingGift> GiftManager::Expire(int64_t nowMs, int timeoutMs, int orphanMs) {
    std::vector<PendingGift> taken;
    std::erase_if(mGifts, [&](PendingGift& g) {
        int64_t age = nowMs - g.createdMs;
        if (g.stage == PendingGift::RefundOnDebit) {
            return age > orphanMs;
        }
        if (age <= timeoutMs) {
            return false;
        }
        if (g.stage == PendingGift::WaitDebit) {
            g.stage = PendingGift::RefundOnDebit;
            taken.push_back(g);
            return false;
        }
        taken.push_back(g); // WaitCredit
        return true;
    });
    return taken;
}

} // namespace coop::server
