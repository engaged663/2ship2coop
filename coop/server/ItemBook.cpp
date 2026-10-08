// [COOP] See ItemBook.h.
#include "ItemBook.h"

#include "common/Protocol.h"

#include <iterator>

namespace coop::server {

bool ItemBook::Add(int16_t stage, const ItemState& item, uint8_t from, int64_t nowMs) {
    Stage& s = mStages[stage];
    if (s.live.count(item.key) != 0 || s.taken.count(item.key) != 0 || s.live.size() >= kMaxItemsPerStage) {
        return false;
    }
    LiveItem& it = s.live[item.key];
    it.state = item;
    it.from = from;
    it.bornMs = nowMs;
    it.lifeMs = ItemLifeMs(item);
    return true;
}

TakeResult ItemBook::Take(int16_t stage, uint32_t key) {
    Stage& s = mStages[stage];
    if (s.taken.count(key) != 0) {
        return TakeResult::Gone;
    }
    s.live.erase(key); // it lay there, or every game has it (an item of the list, a twin) and nobody took it yet
    if (s.order.size() >= kMaxTakenItemsPerStage) {
        s.taken.erase(s.order.front());
        s.order.pop_front();
    }
    s.taken.insert(key);
    s.order.push_back(key);
    return TakeResult::Yours;
}

bool ItemBook::Rest(int16_t stage, uint32_t key, uint8_t from, const float pos[3]) {
    auto s = mStages.find(stage);
    if (s == mStages.end()) {
        return false;
    }
    auto it = s->second.live.find(key);
    if (it == s->second.live.end() || it->second.from != from) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        it->second.state.pos[i] = pos[i];
    }
    it->second.rested = true;
    return true;
}

const LiveItem* ItemBook::Find(int16_t stage, uint32_t key) const {
    auto s = mStages.find(stage);
    if (s == mStages.end()) {
        return nullptr;
    }
    auto it = s->second.live.find(key);
    return it == s->second.live.end() ? nullptr : &it->second;
}

std::vector<const LiveItem*> ItemBook::Live(int16_t stage) const {
    std::vector<const LiveItem*> out;
    auto s = mStages.find(stage);
    if (s != mStages.end()) {
        for (const auto& [key, it] : s->second.live) {
            out.push_back(&it);
        }
    }
    return out;
}

std::vector<uint32_t> ItemBook::Taken(int16_t stage) const {
    std::vector<uint32_t> out;
    auto s = mStages.find(stage);
    if (s != mStages.end()) {
        for (uint32_t key : s->second.order) {
            if (ItemKeyKindOf(key) != ItemKeyKind::Unique) {
                out.push_back(key);
            }
        }
    }
    return out;
}

void ItemBook::Expire(int64_t nowMs) {
    for (auto& [stage, s] : mStages) {
        for (auto it = s.live.begin(); it != s.live.end();) {
            bool over = it->second.lifeMs > 0 && nowMs - it->second.bornMs >= it->second.lifeMs;
            it = over ? s.live.erase(it) : std::next(it);
        }
    }
}

void ItemBook::Forget(int16_t stage) {
    mStages.erase(stage);
}

std::vector<int16_t> ItemBook::Stages() const {
    std::vector<int16_t> out;
    for (const auto& [stage, s] : mStages) {
        out.push_back(stage);
    }
    return out;
}

} // namespace coop::server
