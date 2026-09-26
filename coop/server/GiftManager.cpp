#include "GiftManager.h"

namespace coop::server {

PendingGift& GiftManager::Create(uint8_t fromId, uint8_t toId, int amount, int64_t nowMs) {
    PendingGift gift;
    gift.gid = mNextGid++;
    gift.fromId = fromId;
    gift.toId = toId;
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

std::vector<PendingGift> GiftManager::TakeExpired(int64_t nowMs, int timeoutMs) {
    std::vector<PendingGift> taken;
    std::erase_if(mGifts, [&](const PendingGift& g) {
        if (nowMs - g.createdMs > timeoutMs) {
            taken.push_back(g);
            return true;
        }
        return false;
    });
    return taken;
}

std::vector<PendingGift> GiftManager::TakeCancelledBy(uint8_t playerId) {
    std::vector<PendingGift> taken;
    std::erase_if(mGifts, [&](const PendingGift& g) {
        bool cancel = g.toId == playerId || (g.fromId == playerId && g.stage == PendingGift::WaitDebit);
        if (cancel) {
            taken.push_back(g);
        }
        return cancel;
    });
    return taken;
}

} // namespace coop::server
