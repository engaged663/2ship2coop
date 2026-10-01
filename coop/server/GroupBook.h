#pragma once
// Groups and invitations (docs/superpowers/specs/2026-09-30-coop-grupos-actividades-design.md §2). Pure bookkeeping by
// player id: Groups.cpp turns its changes into events and texts, the commands (Commands/GroupCommands.cpp) and the
// handlers (Handlers/GroupHandlers.cpp) call it. Rules: a player is in one group at most, a group has kMaxPlayers
// members at most (the leader first, then by arrival), an invitation expires, and a group of one with no invitation
// pending is gone.
#include "common/Protocol.h"

#include <cstdint>
#include <vector>

namespace coop::server {

struct Group {
    uint32_t id = 0;
    std::vector<uint8_t> members; // the leader first
    bool formed = false;          // it had two members at some point (breaking it up is news)
    uint8_t Leader() const {
        return members.empty() ? 0 : members.front();
    }
};

struct Invitation {
    uint8_t from = 0;
    uint8_t to = 0;
    uint32_t group = 0;
    int64_t expiresMs = 0;
};

enum class InviteResult { Sent, Renewed, Self, AlreadyMate, Full };
enum class AcceptResult { Joined, NoInvite, GroupGone, Full };

// What a call changed: groups whose members must hear their new state, players out of any group now (or that
// switched groups), invitations that ended and why, groups that are gone.
struct GroupChanges {
    std::vector<uint32_t> groups;
    std::vector<uint8_t> removed;
    std::vector<uint8_t> lonely; // removed from a group that never had anyone else: nothing to tell them
    struct Ended {
        Invitation invite;
        const char* reason; // "accepted", "declined", "expired", "cancelled", "full"
    };
    std::vector<Ended> ended;
    std::vector<uint32_t> dissolved;
};

class GroupBook {
  public:
    const Group* Find(uint32_t id) const;
    const Group* GroupOf(uint8_t player) const;
    bool SameGroup(uint8_t a, uint8_t b) const;         // two different players of the same group
    std::vector<uint8_t> Mates(uint8_t player) const;   // the others of its group
    std::vector<Invitation> InvitesTo(uint8_t player) const; // oldest first
    std::vector<Invitation> InvitesFrom(uint8_t player) const;
    const std::vector<Group>& Groups() const {
        return mGroups;
    }
    const std::vector<Invitation>& Invites() const {
        return mInvites;
    }

    // from invites to into from's group (created with from as leader if it has none).
    InviteResult Invite(uint8_t from, uint8_t to, int64_t nowMs, int64_t ttlMs, GroupChanges& out);
    // to accepts the invitation of from (0: the latest). Leaves its old group; *inviter = who invited it.
    AcceptResult Accept(uint8_t to, uint8_t from, GroupChanges& out, uint8_t* inviter);
    // to declines the invitation of from (0: all of them). Returns how many.
    int Decline(uint8_t to, uint8_t from, GroupChanges& out);
    // player leaves its group; its invitations (sent and received) end.
    void Leave(uint8_t player, GroupChanges& out);
    void Expire(int64_t nowMs, GroupChanges& out);

  private:
    Group* FindMut(uint32_t id);
    void RemoveMember(uint8_t player, GroupChanges& out);
    void DissolveLonely(GroupChanges& out);

    std::vector<Group> mGroups;
    std::vector<Invitation> mInvites;
    uint32_t mNextId = 1;
};

} // namespace coop::server
