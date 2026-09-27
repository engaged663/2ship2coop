#pragma once
// Song of Time vote: passes with more than half of the players in the world; proposing counts as yes.
#include <cstdint>
#include <map>
#include <vector>

namespace coop::server {

class SotVote {
  public:
    enum class Result { Pending, Passed, Failed };

    void Start(uint32_t proposerPeer, int64_t nowMs, int durationMs);
    void Cast(uint32_t peer, bool yes);
    void Cancel();
    bool Active() const {
        return mActive;
    }
    uint32_t Proposer() const {
        return mProposer;
    }
    // eligible = peers of the players in the world right now (votes of anyone else are ignored).
    Result Evaluate(const std::vector<uint32_t>& eligible, int64_t nowMs) const;
    int Yes(const std::vector<uint32_t>& eligible) const;
    int No(const std::vector<uint32_t>& eligible) const;
    int SecondsLeft(int64_t nowMs) const;

  private:
    int Count(const std::vector<uint32_t>& eligible, bool yes) const;

    bool mActive = false;
    uint32_t mProposer = 0;
    int64_t mDeadlineMs = 0;
    std::map<uint32_t, bool> mVotes;
};

} // namespace coop::server
