#include "RoomBook.h"

#include <algorithm>

namespace coop::server {

namespace {

template <class T> void AddUnique(std::vector<T>& v, T value) {
    if (std::find(v.begin(), v.end(), value) == v.end()) {
        v.push_back(value);
    }
}

RoomMember* MemberMut(Room& room, uint8_t id) {
    for (RoomMember& m : room.members) {
        if (m.id == id) {
            return &m;
        }
    }
    return nullptr;
}

RoomMember NewMember(uint8_t id) {
    RoomMember m;
    m.id = id;
    return m;
}

bool Waiting(const Room& room) {
    return room.state == RoomState::Lobby || room.state == RoomState::Starting;
}

} // namespace

const RoomMember* Room::Member(uint8_t id) const {
    for (const RoomMember& m : members) {
        if (m.id == id) {
            return &m;
        }
    }
    return nullptr;
}

bool RoomChanges::Empty() const {
    return rooms.empty() && out.empty() && ended.empty() && joined.empty() && started.empty() && hostChanged.empty();
}

const Room* RoomBook::Find(uint32_t id) const {
    for (const Room& r : mRooms) {
        if (r.id == id) {
            return &r;
        }
    }
    return nullptr;
}

Room* RoomBook::FindMut(uint32_t id) {
    return const_cast<Room*>(Find(id));
}

const Room* RoomBook::RoomOf(uint8_t player) const {
    for (const Room& r : mRooms) {
        if (r.Member(player) != nullptr) {
            return &r;
        }
    }
    return nullptr;
}

Room* RoomBook::RoomOfMut(uint8_t player) {
    return const_cast<Room*>(RoomOf(player));
}

std::vector<uint8_t> RoomBook::Mates(uint8_t player) const {
    std::vector<uint8_t> out;
    if (const Room* r = RoomOf(player)) {
        for (const RoomMember& m : r->members) {
            if (m.id != player) {
                out.push_back(m.id);
            }
        }
    }
    return out;
}

std::vector<RoomInvite> RoomBook::InvitesTo(uint8_t player) const {
    std::vector<RoomInvite> out;
    for (const RoomInvite& inv : mInvites) {
        if (inv.to == player) {
            out.push_back(inv);
        }
    }
    return out;
}

void RoomBook::Changed(const Room& room, RoomChanges& out) {
    AddUnique(out.rooms, room.id);
}

void RoomBook::Run(Room& room, RoomChanges& out) {
    room.state = RoomState::Running;
    room.startAtMs = 0;
    AddUnique(out.started, room.id);
    Changed(room, out);
}

// After every change of a waiting room: everyone ready (and, where it is played where its NPC is, in the director's
// scene) starts the countdown (alone: it runs at once); anything else stops it.
void RoomBook::Evaluate(Room& room, int64_t nowMs, RoomChanges& out) {
    if (!Waiting(room)) {
        return;
    }
    bool all = !room.members.empty() && room.director != 0;
    for (const RoomMember& m : room.members) {
        if (!m.ready || (room.place == RoomPlace::Here && !m.here)) {
            all = false;
            break;
        }
    }
    if (all && room.state == RoomState::Lobby) {
        if (room.members.size() == 1 || mTimes.countdownMs <= 0) {
            Run(room, out);
        } else {
            room.state = RoomState::Starting;
            room.startAtMs = nowMs + mTimes.countdownMs;
            Changed(room, out);
        }
    } else if (!all && room.state == RoomState::Starting) {
        room.state = RoomState::Lobby;
        room.startAtMs = 0;
        Changed(room, out);
    }
}

void RoomBook::CancelInvites(uint32_t room, RoomChanges& out) {
    for (auto it = mInvites.begin(); it != mInvites.end();) {
        if (it->room == room) {
            out.ended.push_back({ *it, "cancelled" });
            it = mInvites.erase(it);
        } else {
            ++it;
        }
    }
}

void RoomBook::CloseAt(size_t index, const char* reason, RoomChanges& out) {
    const Room& room = mRooms[index];
    for (const RoomMember& m : room.members) {
        out.out.push_back({ m.id, room.id, reason });
    }
    CancelInvites(room.id, out);
    mRooms.erase(mRooms.begin() + (std::ptrdiff_t)index);
}

RoomOpenResult RoomBook::Open(uint8_t director, const std::string& key, const std::string& name,
                              const std::string& mode, RoomPlace place, const std::vector<uint8_t>& autoMembers,
                              int64_t nowMs, RoomChanges& out) {
    if (Room* cur = RoomOfMut(director); cur != nullptr && cur->key == key) {
        if (cur->state != RoomState::Ended) {
            return RoomOpenResult::Same; // its round already waits or runs (or another member's does)
        }
        // Another round of the same activity, directed by whoever started it: everyone confirms again
        cur->state = RoomState::Lobby;
        cur->director = director;
        cur->name = name;
        cur->mode = mode;
        cur->place = place;
        cur->roundMs = nowMs;
        cur->startAtMs = 0;
        cur->endedMs = 0;
        for (RoomMember& m : cur->members) {
            m.ready = false;
            m.score = -1;
            m.cs = -1;
            m.won = -1;
        }
        Changed(*cur, out);
        return RoomOpenResult::Reopened;
    }
    if (RoomOf(director) != nullptr) {
        Leave(director, "left", nowMs, out); // another activity's room
    }
    Room room;
    room.id = mNextId++;
    room.key = key;
    room.name = name;
    room.mode = mode;
    room.place = place;
    room.host = director;
    room.director = director;
    room.roundMs = nowMs;
    room.members.push_back(NewMember(director));
    for (uint8_t id : autoMembers) {
        if (id == 0 || room.Member(id) != nullptr || RoomOf(id) != nullptr ||
            room.members.size() >= (size_t)kMaxPlayers) {
            continue;
        }
        room.members.push_back(NewMember(id));
        out.joined.push_back({ room.id, id, "auto" });
    }
    mRooms.push_back(std::move(room));
    Changed(mRooms.back(), out);
    return RoomOpenResult::Created;
}

RoomInviteResult RoomBook::Invite(uint8_t from, uint8_t to, int64_t nowMs, int64_t ttlMs, RoomChanges& out) {
    (void)out;
    Room* room = RoomOfMut(from);
    if (room == nullptr) {
        return RoomInviteResult::NoRoom;
    }
    if (room->host != from) {
        return RoomInviteResult::NotHost;
    }
    if (from == to) {
        return RoomInviteResult::Self;
    }
    if (room->Member(to) != nullptr) {
        return RoomInviteResult::AlreadyMember;
    }
    if (room->members.size() >= (size_t)kMaxPlayers) {
        return RoomInviteResult::Full;
    }
    for (RoomInvite& inv : mInvites) {
        if (inv.room == room->id && inv.to == to) {
            inv.from = from;
            inv.expiresMs = nowMs + ttlMs;
            return RoomInviteResult::Renewed;
        }
    }
    mInvites.push_back(RoomInvite{ room->id, from, to, nowMs + ttlMs });
    return RoomInviteResult::Sent;
}

RoomAcceptResult RoomBook::Accept(uint8_t to, uint32_t room, uint8_t from, int64_t nowMs, RoomChanges& out) {
    int index = -1;
    for (size_t i = 0; i < mInvites.size(); i++) {
        const RoomInvite& inv = mInvites[i];
        if (inv.to == to && (room == 0 || inv.room == room) && (from == 0 || inv.from == from)) {
            index = (int)i; // the last matching one is the latest
        }
    }
    if (index < 0) {
        return RoomAcceptResult::NoInvite;
    }
    RoomInvite chosen = mInvites[(size_t)index];
    mInvites.erase(mInvites.begin() + index);
    Room* target = FindMut(chosen.room);
    if (target == nullptr) {
        out.ended.push_back({ chosen, "cancelled" });
        return RoomAcceptResult::Gone;
    }
    if (target->Member(to) != nullptr) {
        out.ended.push_back({ chosen, "accepted" }); // it came in meanwhile (an open room)
        return RoomAcceptResult::Joined;
    }
    if (target->members.size() >= (size_t)kMaxPlayers) {
        out.ended.push_back({ chosen, "full" });
        return RoomAcceptResult::Full;
    }
    out.ended.push_back({ chosen, "accepted" });
    if (RoomOf(to) != nullptr) {
        Leave(to, "left", nowMs, out); // never the target: it was not in it
    }
    target = FindMut(chosen.room);
    if (target == nullptr) {
        return RoomAcceptResult::Gone; // cannot happen: leaving another room never removes this one
    }
    target->members.push_back(NewMember(to));
    out.joined.push_back({ target->id, to, "invite" });
    Changed(*target, out);
    Evaluate(*target, nowMs, out);
    return RoomAcceptResult::Joined;
}

int RoomBook::Decline(uint8_t to, uint8_t from, RoomChanges& out) {
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
    return count;
}

RoomJoinResult RoomBook::Join(uint8_t player, uint32_t room, int64_t nowMs, RoomChanges& out) {
    Room* target = FindMut(room);
    if (target == nullptr) {
        return RoomJoinResult::Gone;
    }
    if (target->Member(player) != nullptr) {
        return RoomJoinResult::Already;
    }
    if (!target->settings.open) {
        return RoomJoinResult::NotOpen;
    }
    if (target->members.size() >= (size_t)kMaxPlayers) {
        return RoomJoinResult::Full;
    }
    if (RoomOf(player) != nullptr) {
        Leave(player, "left", nowMs, out);
    }
    target = FindMut(room);
    if (target == nullptr) {
        return RoomJoinResult::Gone;
    }
    for (auto it = mInvites.begin(); it != mInvites.end();) { // its invitation to this room is answered
        if (it->room == room && it->to == player) {
            out.ended.push_back({ *it, "accepted" });
            it = mInvites.erase(it);
        } else {
            ++it;
        }
    }
    target->members.push_back(NewMember(player));
    out.joined.push_back({ room, player, "open" });
    Changed(*target, out);
    Evaluate(*target, nowMs, out);
    return RoomJoinResult::Joined;
}

bool RoomBook::SetReady(uint8_t player, bool ready, int64_t nowMs, RoomChanges& out) {
    Room* room = RoomOfMut(player);
    if (room == nullptr || !Waiting(*room)) {
        return false;
    }
    RoomMember* m = MemberMut(*room, player);
    if (m->ready != ready) {
        m->ready = ready;
        Changed(*room, out);
    }
    Evaluate(*room, nowMs, out);
    return true;
}

void RoomBook::SetHere(uint8_t player, bool here, int64_t nowMs, RoomChanges& out) {
    Room* room = RoomOfMut(player);
    if (room == nullptr) {
        return;
    }
    RoomMember* m = MemberMut(*room, player);
    if (m->here == here) {
        return;
    }
    m->here = here;
    Changed(*room, out);
    Evaluate(*room, nowMs, out);
}

bool RoomBook::Leave(uint8_t player, const char* reason, int64_t nowMs, RoomChanges& out) {
    for (size_t i = 0; i < mRooms.size(); i++) {
        Room& room = mRooms[i];
        auto m = std::find_if(room.members.begin(), room.members.end(),
                              [&](const RoomMember& rm) { return rm.id == player; });
        if (m == room.members.end()) {
            continue;
        }
        room.members.erase(m);
        out.out.push_back({ player, room.id, reason });
        if (room.members.empty()) {
            CancelInvites(room.id, out);
            mRooms.erase(mRooms.begin() + (std::ptrdiff_t)i);
            return true;
        }
        if (room.host == player) {
            room.host = room.members.front().id;
            AddUnique(out.hostChanged, room.id);
        }
        if (room.director == player && room.state != RoomState::Ended) {
            room.state = RoomState::Ended; // the round ran (or was to run) in its game
            room.startAtMs = 0;
            room.endedMs = nowMs;
        }
        Changed(room, out);
        Evaluate(room, nowMs, out);
        return true;
    }
    return false;
}

bool RoomBook::Kick(uint8_t host, uint8_t who, int64_t nowMs, RoomChanges& out) {
    Room* room = RoomOfMut(host);
    if (room == nullptr || room->host != host || who == host || room->Member(who) == nullptr) {
        return false;
    }
    return Leave(who, "kicked", nowMs, out);
}

bool RoomBook::Close(uint8_t host, RoomChanges& out) {
    for (size_t i = 0; i < mRooms.size(); i++) {
        if (mRooms[i].host == host && mRooms[i].Member(host) != nullptr) {
            CloseAt(i, "closed", out);
            return true;
        }
    }
    return false;
}

bool RoomBook::SetSettings(uint8_t host, const RoomSettings& settings, RoomChanges& out) {
    Room* room = RoomOfMut(host);
    if (room == nullptr || room->host != host) {
        return false;
    }
    room->settings = settings;
    Changed(*room, out);
    return true;
}

void RoomBook::EndRound(uint8_t director, const std::string& key, int64_t nowMs, RoomChanges& out) {
    Room* room = RoomOfMut(director);
    if (room == nullptr || room->key != key || room->director != director || room->state == RoomState::Ended) {
        return;
    }
    room->state = RoomState::Ended;
    room->startAtMs = 0;
    room->endedMs = nowMs;
    Changed(*room, out);
}

void RoomBook::SetResult(uint8_t player, const std::string& key, int64_t score, int64_t cs, int won,
                         RoomChanges& out) {
    Room* room = RoomOfMut(player);
    if (room == nullptr || room->key != key) {
        return;
    }
    RoomMember* m = MemberMut(*room, player);
    if (score >= 0) {
        m->score = score;
    }
    if (cs >= 0) {
        m->cs = cs;
    }
    if (won >= 0) {
        m->won = (int8_t)(won != 0 ? 1 : 0);
    }
    Changed(*room, out);
}

void RoomBook::Forget(uint8_t player, int64_t nowMs, RoomChanges& out) {
    Leave(player, "gone", nowMs, out);
    for (auto it = mInvites.begin(); it != mInvites.end();) {
        if (it->to == player) {
            out.ended.push_back({ *it, "cancelled" });
            it = mInvites.erase(it);
        } else {
            ++it;
        }
    }
}

void RoomBook::Tick(int64_t nowMs, RoomChanges& out) {
    for (auto it = mInvites.begin(); it != mInvites.end();) {
        if (it->expiresMs <= nowMs) {
            out.ended.push_back({ *it, "expired" });
            it = mInvites.erase(it);
        } else {
            ++it;
        }
    }
    for (size_t i = 0; i < mRooms.size();) {
        Room& room = mRooms[i];
        if (Waiting(room) && nowMs - room.roundMs >= mTimes.lobbyMs) {
            CloseAt(i, "timeout", out);
            continue;
        }
        if (room.state == RoomState::Ended && nowMs - room.endedMs >= mTimes.lingerMs) {
            CloseAt(i, "ended", out);
            continue;
        }
        if (room.state == RoomState::Starting && nowMs >= room.startAtMs) {
            Run(room, out);
        }
        i++;
    }
}

} // namespace coop::server
