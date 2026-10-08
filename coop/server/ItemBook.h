#pragma once
// [COOP] Drops compartidos: the items lying in each stage (scene + layer, Stage.h) of the shared world and the keys taken
// there. Pure logic (Handlers/ItemHandlers.cpp sends and receives): the first one who takes an item gets it.
#include "common/ItemState.h"

#include <cstdint>
#include <deque>
#include <map>
#include <set>
#include <vector>

namespace coop::server {

struct LiveItem {
    ItemState state;     // pos: where it lies once it rested
    uint8_t from = 0;    // who announced it (only they say where it came to lie)
    int64_t bornMs = 0;
    int64_t lifeMs = 0;  // 0: until someone takes it or the stage empties
    bool rested = false;
};

enum class TakeResult : uint8_t { Yours, Gone };

class ItemBook {
  public:
    // False when it lies there already or was taken (a twin another game announced first), or the stage is full.
    bool Add(int16_t stage, const ItemState& item, uint8_t from, int64_t nowMs);
    // Yours when it lay there or nobody took that key before (an item every game has); the key is remembered.
    TakeResult Take(int16_t stage, uint32_t key);
    // Only the one who announced it; false otherwise or if it is gone.
    bool Rest(int16_t stage, uint32_t key, uint8_t from, const float pos[3]);
    const LiveItem* Find(int16_t stage, uint32_t key) const;
    std::vector<const LiveItem*> Live(int16_t stage) const;
    std::vector<uint32_t> Taken(int16_t stage) const; // the ones a game could make again (list items, twins)
    void Expire(int64_t nowMs);                       // their life ran out: they vanished in every game
    void Forget(int16_t stage);                       // nobody plays there any more
    std::vector<int16_t> Stages() const;

  private:
    struct Stage {
        std::map<uint32_t, LiveItem> live;
        std::set<uint32_t> taken;
        std::deque<uint32_t> order; // taken, oldest first
    };
    std::map<int16_t, Stage> mStages;
};

} // namespace coop::server
