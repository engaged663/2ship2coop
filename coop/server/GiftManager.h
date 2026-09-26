#pragma once
// Rupee gifts in flight. Flow: /gift -> gift_debit (sender pays) -> gift_paid -> gift_credit (receiver)
// -> gift_recv -> gift_refund of whatever did not fit in the receiver's wallet.
#include <cstdint>
#include <vector>

namespace coop::server {

struct PendingGift {
    enum Stage { WaitDebit, WaitCredit };
    uint32_t gid = 0;
    uint8_t fromId = 0;
    uint8_t toId = 0;
    int amount = 0; // requested
    int paid = 0;   // actually debited from the sender
    Stage stage = WaitDebit;
    int64_t createdMs = 0;
};

class GiftManager {
  public:
    PendingGift& Create(uint8_t fromId, uint8_t toId, int amount, int64_t nowMs);
    PendingGift* Find(uint32_t gid);
    void Remove(uint32_t gid);
    // Removes and returns gifts older than timeoutMs.
    std::vector<PendingGift> TakeExpired(int64_t nowMs, int timeoutMs);
    // Removes and returns the gifts that cannot finish because playerId left: every gift addressed to
    // it, and its own gifts that were not paid yet (paid ones stay so the receiver still gets them).
    std::vector<PendingGift> TakeCancelledBy(uint8_t playerId);

  private:
    std::vector<PendingGift> mGifts;
    uint32_t mNextGid = 1;
};

} // namespace coop::server
