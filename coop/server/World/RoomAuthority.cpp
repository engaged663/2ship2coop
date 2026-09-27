#include "RoomAuthority.h"

#include <tuple>

namespace coop::server {

std::map<RoomKey, uint8_t> ComputeAuthority(const std::vector<AuthMember>& members) {
    std::map<RoomKey, const AuthMember*> best;
    // Busy members lose to anyone who is not; then the oldest, then the lower id.
    auto better = [](const AuthMember& a, const AuthMember& b) {
        return std::make_tuple(a.busy, a.sinceMs, a.id) < std::make_tuple(b.busy, b.sinceMs, b.id);
    };
    for (const AuthMember& m : members) {
        if (m.scene < 0 || m.room < 0) {
            continue;
        }
        const AuthMember*& slot = best[{ m.scene, m.room }];
        if (slot == nullptr || better(m, *slot)) {
            slot = &m;
        }
    }
    std::map<RoomKey, uint8_t> owners;
    for (const auto& [key, member] : best) {
        owners[key] = member->id;
    }
    return owners;
}

} // namespace coop::server
