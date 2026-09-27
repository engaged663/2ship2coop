#pragma once
// Rupee gifts in flight. Flow: /gift -> gift_debit (sender pays) -> gift_paid -> gift_credit (receiver)
// -> gift_recv -> gift_refund of whatever did not fit in the receiver's wallet.
// Gifts are keyed by connection (peer), never by player id: ids are reused as soon as someone leaves.
#include <cstdint>
#include <string>
#include <vector>

namespace coop::server {

struct PendingGift {
    // WaitDebit: the sender was asked to pay. WaitCredit: paid; the receiver was asked to take it.
    // RefundOnDebit: cancelled before the payment arrived; a late payment is refunded to the sender.
    enum Stage { WaitDebit, WaitCredit, RefundOnDebit };
    uint32_t gid = 0;
    uint32_t fromPeer = 0;
    uint32_t toPeer = 0;
    std::string fromNick;
    std::string toNick;
    int amount = 0; // requested
    int paid = 0;   // actually debited from the sender
    Stage stage = WaitDebit;
    int64_t createdMs = 0;
};

class GiftManager {
  public:
    PendingGift& Create(uint32_t fromPeer, uint32_t toPeer, const std::string& fromNick, const std::string& toNick,
                        int amount, int64_t nowMs);
    PendingGift* Find(uint32_t gid);
    void Remove(uint32_t gid);
    // A connection left. Returns (and removes) the paid gifts addressed to it: refund them. Its incoming
    // unpaid gifts become RefundOnDebit; its own unpaid gifts are dropped; its own paid gifts stay so the
    // receiver still gets the rupees.
    std::vector<PendingGift> OnPlayerLeft(uint32_t peer);
    // Returns (and removes) paid gifts nobody confirmed within timeoutMs: refund them. Unpaid gifts that
    // time out become RefundOnDebit and are returned once so the sender can be told. RefundOnDebit gifts
    // older than orphanMs are forgotten.
    std::vector<PendingGift> Expire(int64_t nowMs, int timeoutMs, int orphanMs);

  private:
    std::vector<PendingGift> mGifts;
    uint32_t mNextGid = 1;
};

} // namespace coop::server
