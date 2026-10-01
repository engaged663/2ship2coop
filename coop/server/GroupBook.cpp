#include "GroupBook.h"

#include <algorithm>

namespace coop::server {

namespace {

template <class T> void AddUnique(std::vector<T>& v, T value) {
    if (std::find(v.begin(), v.end(), value) == v.end()) {
        v.push_back(value);
    }
}

} // namespace

const Group* GroupBook::Find(uint32_t id) const {
    for (const Group& g : mGroups) {
        if (g.id == id) {
            return &g;
        }
    }
    return nullptr;
}

Group* GroupBook::FindMut(uint32_t id) {
    for (Group& g : mGroups) {
        if (g.id == id) {
            return &g;
        }
    }
    return nullptr;
}

const Group* GroupBook::GroupOf(uint8_t player) const {
    for (const Group& g : mGroups) {
        if (std::find(g.members.begin(), g.members.end(), player) != g.members.end()) {
            return &g;
        }
    }
    return nullptr;
}

bool GroupBook::SameGroup(uint8_t a, uint8_t b) const {
    const Group* g = GroupOf(a);
    return a != b && g != nullptr && g == GroupOf(b);
}

std::vector<uint8_t> GroupBook::Mates(uint8_t player) const {
    std::vector<uint8_t> out;
    if (const Group* g = GroupOf(player)) {
        for (uint8_t m : g->members) {
            if (m != player) {
                out.push_back(m);
            }
        }
    }
    return out;
}

std::vector<Invitation> GroupBook::InvitesTo(uint8_t player) const {
    std::vector<Invitation> out;
    for (const Invitation& inv : mInvites) {
        if (inv.to == player) {
            out.push_back(inv);
        }
    }
    return out;
}

std::vector<Invitation> GroupBook::InvitesFrom(uint8_t player) const {
    std::vector<Invitation> out;
    for (const Invitation& inv : mInvites) {
        if (inv.from == player) {
            out.push_back(inv);
        }
    }
    return out;
}

InviteResult GroupBook::Invite(uint8_t from, uint8_t to, int64_t nowMs, int64_t ttlMs, GroupChanges& out) {
    if (from == to) {
        return InviteResult::Self;
    }
    if (SameGroup(from, to)) {
        return InviteResult::AlreadyMate;
    }
    const Group* g = GroupOf(from);
    if (g == nullptr) {
        mGroups.push_back(Group{ mNextId++, { from } });
        g = &mGroups.back();
        AddUnique(out.groups, g->id);
    }
    if (g->members.size() >= (size_t)kMaxPlayers) {
        return InviteResult::Full;
    }
    for (Invitation& inv : mInvites) {
        if (inv.from == from && inv.to == to && inv.group == g->id) {
            inv.expiresMs = nowMs + ttlMs;
            return InviteResult::Renewed;
        }
    }
    mInvites.push_back(Invitation{ from, to, g->id, nowMs + ttlMs });
    return InviteResult::Sent;
}

AcceptResult GroupBook::Accept(uint8_t to, uint8_t from, GroupChanges& out, uint8_t* inviter) {
    int index = -1;
    for (size_t i = 0; i < mInvites.size(); i++) {
        if (mInvites[i].to == to && (from == 0 || mInvites[i].from == from)) {
            index = (int)i; // the last matching one is the latest
        }
    }
    if (index < 0) {
        return AcceptResult::NoInvite;
    }
    Invitation chosen = mInvites[index];
    if (inviter != nullptr) {
        *inviter = chosen.from;
    }
    const Group* target = Find(chosen.group);
    if (target == nullptr || target->members.size() >= (size_t)kMaxPlayers) {
        mInvites.erase(mInvites.begin() + index);
        out.ended.push_back({ chosen, target == nullptr ? "cancelled" : "full" });
        DissolveLonely(out);
        return target == nullptr ? AcceptResult::GroupGone : AcceptResult::Full;
    }
    uint32_t groupId = chosen.group;
    // It chose: its other invitations and the ones it sent (for its old group) end here.
    for (auto it = mInvites.begin(); it != mInvites.end();) {
        if (it->to == to || it->from == to) {
            bool accepted = it->to == to && it->from == chosen.from && it->group == groupId;
            out.ended.push_back({ *it, accepted ? "accepted" : "cancelled" });
            it = mInvites.erase(it);
        } else {
            ++it;
        }
    }
    RemoveMember(to, out); // its old group, if any
    Group* g = FindMut(groupId);
    if (g == nullptr) {
        return AcceptResult::GroupGone; // cannot happen: the old group is never the target
    }
    g->members.push_back(to);
    g->formed = true;
    AddUnique(out.groups, groupId);
    out.removed.erase(std::remove(out.removed.begin(), out.removed.end(), to), out.removed.end());
    DissolveLonely(out);
    return AcceptResult::Joined;
}

int GroupBook::Decline(uint8_t to, uint8_t from, GroupChanges& out) {
    int count = 0;
    for (auto it = mInvites.begin(); it != mInvites.end();) {
        if (it->to == to && (from == 0 || it->from == from)) {
            out.ended.push_back({ *it, "declined" });
            it = mInvites.erase(it);
            count++;
        } else {
            ++it;
        }
    }
    DissolveLonely(out);
    return count;
}

void GroupBook::Leave(uint8_t player, GroupChanges& out) {
    for (auto it = mInvites.begin(); it != mInvites.end();) {
        if (it->to == player || it->from == player) {
            out.ended.push_back({ *it, "cancelled" });
            it = mInvites.erase(it);
        } else {
            ++it;
        }
    }
    RemoveMember(player, out);
    DissolveLonely(out);
}

void GroupBook::Expire(int64_t nowMs, GroupChanges& out) {
    bool any = false;
    for (auto it = mInvites.begin(); it != mInvites.end();) {
        if (it->expiresMs <= nowMs) {
            out.ended.push_back({ *it, "expired" });
            it = mInvites.erase(it);
            any = true;
        } else {
            ++it;
        }
    }
    if (any) {
        DissolveLonely(out);
    }
}

void GroupBook::RemoveMember(uint8_t player, GroupChanges& out) {
    for (auto g = mGroups.begin(); g != mGroups.end(); ++g) {
        auto m = std::find(g->members.begin(), g->members.end(), player);
        if (m == g->members.end()) {
            continue;
        }
        g->members.erase(m);
        AddUnique(out.removed, player);
        if (g->members.empty()) {
            AddUnique(out.dissolved, g->id);
            mGroups.erase(g);
        } else {
            AddUnique(out.groups, g->id);
        }
        return;
    }
}

// A group of one that waits for nobody is gone.
void GroupBook::DissolveLonely(GroupChanges& out) {
    for (auto g = mGroups.begin(); g != mGroups.end();) {
        bool waiting = std::any_of(mInvites.begin(), mInvites.end(),
                                   [&](const Invitation& inv) { return inv.group == g->id; });
        if (g->members.size() <= 1 && !waiting) {
            if (!g->members.empty()) {
                AddUnique(out.removed, g->members.front());
                if (!g->formed) {
                    AddUnique(out.lonely, g->members.front());
                }
            }
            AddUnique(out.dissolved, g->id);
            g = mGroups.erase(g);
        } else {
            ++g;
        }
    }
}

} // namespace coop::server
