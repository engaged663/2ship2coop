#include "SotVote.h"

#include <algorithm>

namespace coop::server {

void SotVote::Start(uint32_t proposerPeer, int64_t nowMs, int durationMs) {
    mActive = true;
    mProposer = proposerPeer;
    mDeadlineMs = nowMs + durationMs;
    mVotes.clear();
    mVotes[proposerPeer] = true;
}

void SotVote::Cast(uint32_t peer, bool yes) {
    if (mActive) {
        mVotes[peer] = yes;
    }
}

void SotVote::Cancel() {
    mActive = false;
    mVotes.clear();
}

int SotVote::Count(const std::vector<uint32_t>& eligible, bool yes) const {
    int count = 0;
    for (uint32_t peer : eligible) {
        auto it = mVotes.find(peer);
        if (it != mVotes.end() && it->second == yes) {
            count++;
        }
    }
    return count;
}

int SotVote::Yes(const std::vector<uint32_t>& eligible) const {
    return Count(eligible, true);
}

int SotVote::No(const std::vector<uint32_t>& eligible) const {
    return Count(eligible, false);
}

SotVote::Result SotVote::Evaluate(const std::vector<uint32_t>& eligible, int64_t nowMs) const {
    int n = (int)eligible.size();
    if (n == 0) {
        return Result::Failed;
    }
    int needed = n / 2 + 1;
    int yes = Yes(eligible);
    int undecided = n - yes - No(eligible);
    if (yes >= needed) {
        return Result::Passed;
    }
    if (yes + undecided < needed || nowMs >= mDeadlineMs) {
        return Result::Failed;
    }
    return Result::Pending;
}

int SotVote::SecondsLeft(int64_t nowMs) const {
    return (int)std::max<int64_t>(0, (mDeadlineMs - nowMs + 999) / 1000);
}

} // namespace coop::server
