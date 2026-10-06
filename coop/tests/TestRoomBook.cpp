// Activity rooms (spec 2026-10-04-coop-salas-actividades §2): RoomBook's rules, without a server.
#include "TestMain.h"

#include "server/RoomBook.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace coop;
using server::RoomAcceptResult;
using server::RoomBook;
using server::RoomChanges;
using server::RoomInviteResult;
using server::RoomJoinResult;
using server::RoomOpenResult;
using server::RoomPlace;
using server::RoomSettings;
using server::RoomState;
using server::RoomTimes;

namespace {

constexpr int64_t kTtl = 60000;

RoomTimes Times() {
    RoomTimes t;
    t.countdownMs = 3000;
    t.lobbyMs = 600000;
    t.lingerMs = 120000;
    return t;
}

// director opens key with autoMembers at nowMs.
RoomOpenResult Open(RoomBook& b, uint8_t director, std::vector<uint8_t> autoMembers = {},
                    RoomPlace place = RoomPlace::Anywhere, const char* key = "galeria_ciudad", int64_t nowMs = 0) {
    RoomChanges ch;
    return b.Open(director, key, "Galeria", "together", place, autoMembers, nowMs, ch);
}

bool OutWith(const RoomChanges& ch, uint8_t player, const char* reason) {
    for (const auto& o : ch.out) {
        if (o.player == player && std::string(o.reason) == reason) {
            return true;
        }
    }
    return false;
}

bool InviteEndedWith(const RoomChanges& ch, uint8_t to, const char* reason) {
    for (const auto& e : ch.ended) {
        if (e.invite.to == to && std::string(e.reason) == reason) {
            return true;
        }
    }
    return false;
}

bool JoinedHow(const RoomChanges& ch, uint8_t player, const char* how) {
    for (const auto& j : ch.joined) {
        if (j.player == player && std::string(j.how) == how) {
            return true;
        }
    }
    return false;
}

bool Has(const std::vector<uint32_t>& v, uint32_t id) {
    return std::find(v.begin(), v.end(), id) != v.end();
}

// Everyone in director's room ready (in this order).
void AllReady(RoomBook& b, uint8_t director, int64_t nowMs = 0) {
    RoomChanges ch;
    std::vector<uint8_t> ids;
    for (const auto& m : b.RoomOf(director)->members) {
        ids.push_back(m.id);
    }
    for (uint8_t id : ids) {
        b.SetReady(id, true, nowMs, ch);
    }
}

} // namespace

TEST_CASE(RoomOpenMakesALobbyWithTheGroup) {
    RoomBook b;
    RoomChanges ch;
    CHECK(b.Open(1, "galeria_ciudad", "Galeria", "together", RoomPlace::Here, { 2, 3 }, 0, ch) ==
          RoomOpenResult::Created);
    const server::Room* r = b.RoomOf(1);
    CHECK(r != nullptr);
    CHECK(r->state == RoomState::Lobby);
    CHECK_EQ(r->host, (uint8_t)1);
    CHECK_EQ(r->director, (uint8_t)1);
    CHECK(r->place == RoomPlace::Here);
    CHECK_EQ(r->key, std::string("galeria_ciudad"));
    CHECK_EQ(r->mode, std::string("together"));
    CHECK_EQ(r->members.size(), (size_t)3);
    CHECK_EQ(r->members[0].id, (uint8_t)1);
    CHECK(!r->members[1].ready && !r->members[2].ready);
    CHECK(b.RoomOf(2) == r && b.RoomOf(3) == r);
    CHECK(Has(ch.rooms, r->id));
    CHECK(JoinedHow(ch, 2, "auto") && JoinedHow(ch, 3, "auto"));
    CHECK(b.Mates(2) == std::vector<uint8_t>({ 1, 3 }));
    CHECK(b.Mates(9).empty());
}

TEST_CASE(RoomOpenSkipsBusyPlayersAndRepeatsNothing) {
    RoomBook b;
    Open(b, 4, {}, RoomPlace::Here, "cofres");           // 4 directs another activity
    Open(b, 1, { 2, 4, 1 }, RoomPlace::Here);           // 4 is busy; the director itself is no auto member
    CHECK_EQ(b.RoomOf(1)->members.size(), (size_t)2);
    CHECK_EQ(b.RoomOf(4)->key, std::string("cofres"));
    RoomChanges again;
    CHECK(b.Open(1, "galeria_ciudad", "Galeria", "together", RoomPlace::Here, { 2 }, 10, again) ==
          RoomOpenResult::Same);
    CHECK(again.rooms.empty() && again.out.empty());
    // Another activity: it leaves the first room (2 stays and leads it; its round was 1's: over)
    RoomChanges other;
    CHECK(b.Open(1, "honey_darling", "Honey", "together", RoomPlace::Here, {}, 20, other) ==
          RoomOpenResult::Created);
    CHECK_EQ(b.RoomOf(1)->key, std::string("honey_darling"));
    CHECK(OutWith(other, 1, "left"));
    const server::Room* old = b.RoomOf(2);
    CHECK(old != nullptr && old->key == "galeria_ciudad");
    CHECK_EQ(old->host, (uint8_t)2);
    CHECK(old->state == RoomState::Ended);
    CHECK(Has(other.hostChanged, old->id));
}

TEST_CASE(RoomStartsWhenEveryoneIsReadyAfterTheCountdown) {
    RoomBook b;
    Open(b, 1, { 2 });
    RoomChanges ch;
    CHECK(b.SetReady(1, true, 0, ch));
    CHECK(b.RoomOf(1)->state == RoomState::Lobby);
    RoomChanges ready;
    CHECK(b.SetReady(2, true, 100, ready));
    CHECK(b.RoomOf(1)->state == RoomState::Starting);
    CHECK_EQ(b.RoomOf(1)->startAtMs, (int64_t)3100);
    CHECK(Has(ready.rooms, b.RoomOf(1)->id));
    RoomChanges early;
    b.Tick(3099, early);
    CHECK(b.RoomOf(1)->state == RoomState::Starting);
    CHECK(early.started.empty());
    RoomChanges go;
    b.Tick(3100, go);
    CHECK(b.RoomOf(1)->state == RoomState::Running);
    CHECK(go.started == std::vector<uint32_t>({ b.RoomOf(1)->id }));
    CHECK(Has(go.rooms, b.RoomOf(1)->id));
    RoomChanges late;
    CHECK(!b.SetReady(2, false, 3200, late)); // running: nothing to confirm
    CHECK(b.RoomOf(1)->state == RoomState::Running);
}

TEST_CASE(RoomAloneStartsAtOnce) {
    RoomBook b;
    Open(b, 1);
    RoomChanges ch;
    CHECK(b.SetReady(1, true, 50, ch));
    CHECK(b.RoomOf(1)->state == RoomState::Running);
    CHECK(ch.started == std::vector<uint32_t>({ b.RoomOf(1)->id }));
}

TEST_CASE(RoomCountdownStopsWhenSomeoneIsNotReadyOrArrives) {
    RoomBook b;
    Open(b, 1, { 2 });
    AllReady(b, 1, 0);
    CHECK(b.RoomOf(1)->state == RoomState::Starting);
    RoomChanges ch;
    b.SetReady(2, false, 500, ch);
    CHECK(b.RoomOf(1)->state == RoomState::Lobby);
    b.SetReady(2, true, 1000, ch);
    CHECK(b.RoomOf(1)->state == RoomState::Starting);
    CHECK_EQ(b.RoomOf(1)->startAtMs, (int64_t)4000);
    // Someone new comes in: it has not confirmed, so the room waits again
    CHECK(b.Invite(1, 3, 1100, kTtl, ch) == RoomInviteResult::Sent);
    CHECK(b.Accept(3, 0, 0, 1200, ch) == RoomAcceptResult::Joined);
    CHECK(b.RoomOf(1)->state == RoomState::Lobby);
    RoomChanges tick;
    b.Tick(5000, tick);
    CHECK(b.RoomOf(1)->state == RoomState::Lobby);
}

TEST_CASE(RoomHereWaitsForEveryoneInTheDirectorsScene) {
    RoomBook b;
    Open(b, 1, { 2 }, RoomPlace::Here);
    RoomChanges ch;
    b.SetHere(2, false, 0, ch);
    AllReady(b, 1, 0);
    CHECK(b.RoomOf(1)->state == RoomState::Lobby); // 2 is still on its way
    b.SetHere(2, true, 50, ch);
    CHECK(b.RoomOf(1)->state == RoomState::Starting);
    b.SetHere(2, false, 60, ch);
    CHECK(b.RoomOf(1)->state == RoomState::Lobby);
    // Anywhere: where it is does not matter
    RoomBook any;
    Open(any, 1, { 2 }, RoomPlace::Anywhere);
    any.SetHere(2, false, 0, ch);
    AllReady(any, 1, 0);
    CHECK(any.RoomOf(1)->state == RoomState::Starting);
}

TEST_CASE(RoomInvitationsAreTheHostsAndForThisRoomOnly) {
    RoomBook b;
    Open(b, 1, { 2 });
    RoomChanges ch;
    CHECK(b.Invite(2, 3, 0, kTtl, ch) == RoomInviteResult::NotHost);
    CHECK(b.Invite(1, 1, 0, kTtl, ch) == RoomInviteResult::Self);
    CHECK(b.Invite(1, 2, 0, kTtl, ch) == RoomInviteResult::AlreadyMember);
    CHECK(b.Invite(5, 3, 0, kTtl, ch) == RoomInviteResult::NoRoom);
    CHECK(b.Invite(1, 3, 0, kTtl, ch) == RoomInviteResult::Sent);
    CHECK(b.Invite(1, 3, 10, kTtl, ch) == RoomInviteResult::Renewed);
    CHECK_EQ(b.InvitesTo(3).size(), (size_t)1);
    CHECK_EQ(b.InvitesTo(3)[0].expiresMs, (int64_t)10 + kTtl);
    CHECK_EQ(b.InvitesTo(3)[0].room, b.RoomOf(1)->id);
    RoomChanges acc;
    CHECK(b.Accept(3, 0, 0, 20, acc) == RoomAcceptResult::Joined);
    CHECK(b.RoomOf(3) == b.RoomOf(1));
    CHECK(!b.RoomOf(1)->Member(3)->ready);
    CHECK(InviteEndedWith(acc, 3, "accepted"));
    CHECK(JoinedHow(acc, 3, "invite"));
    CHECK(b.InvitesTo(3).empty());
    CHECK(b.Accept(3, 0, 0, 30, acc) == RoomAcceptResult::NoInvite);
}

TEST_CASE(RoomIsFullAtMaxPlayers) {
    RoomBook b;
    Open(b, 1, { 2, 3, 4 });
    CHECK_EQ(b.RoomOf(1)->members.size(), (size_t)kMaxPlayers);
    RoomChanges ch;
    CHECK(b.Invite(1, 5, 0, kTtl, ch) == RoomInviteResult::Full);
    // A room that filled up after the invitation: accepting tells so
    RoomBook c;
    Open(c, 1, { 2, 3 });
    c.Invite(1, 5, 0, kTtl, ch);
    c.Invite(1, 6, 0, kTtl, ch);
    CHECK(c.Accept(5, 0, 0, 10, ch) == RoomAcceptResult::Joined);
    RoomChanges full;
    CHECK(c.Accept(6, 0, 0, 20, full) == RoomAcceptResult::Full);
    CHECK(InviteEndedWith(full, 6, "full"));
    CHECK(c.RoomOf(6) == nullptr);
}

TEST_CASE(RoomAcceptingLeavesTheOldRoom) {
    RoomBook b;
    Open(b, 1, { 2 });
    Open(b, 3, {}, RoomPlace::Here, "cofres");
    RoomChanges ch;
    b.Invite(3, 2, 0, kTtl, ch);
    RoomChanges acc;
    CHECK(b.Accept(2, 0, 3, 10, acc) == RoomAcceptResult::Joined);
    CHECK_EQ(b.RoomOf(2)->key, std::string("cofres"));
    CHECK_EQ(b.RoomOf(1)->members.size(), (size_t)1);
    CHECK(OutWith(acc, 2, "left"));
    // An invitation to a room that is gone
    RoomBook g;
    Open(g, 1);
    g.Invite(1, 2, 0, kTtl, ch);
    RoomChanges closed;
    CHECK(g.Close(1, closed));
    CHECK(InviteEndedWith(closed, 2, "cancelled"));
    CHECK(g.Accept(2, 0, 0, 10, closed) == RoomAcceptResult::NoInvite);
}

TEST_CASE(RoomDeclineAndExpiry) {
    RoomBook b;
    Open(b, 1);
    RoomChanges ch;
    b.Invite(1, 2, 0, kTtl, ch);
    b.Invite(1, 3, 0, kTtl, ch);
    RoomChanges declined;
    CHECK_EQ(b.Decline(2, 0, declined), 1);
    CHECK(InviteEndedWith(declined, 2, "declined"));
    CHECK_EQ(b.Decline(2, 0, declined), 0);
    RoomChanges early;
    b.Tick(kTtl - 1, early);
    CHECK(early.ended.empty());
    RoomChanges expired;
    b.Tick(kTtl, expired);
    CHECK(InviteEndedWith(expired, 3, "expired"));
    CHECK(b.InvitesTo(3).empty());
}

TEST_CASE(RoomLeaveKickAndClose) {
    RoomBook b;
    Open(b, 1, { 2, 3 });
    RoomChanges ch;
    CHECK(!b.Kick(2, 3, 0, ch)); // only the host
    CHECK(!b.Kick(1, 1, 0, ch)); // never itself
    CHECK(!b.Kick(1, 9, 0, ch)); // not in the room
    RoomChanges kicked;
    CHECK(b.Kick(1, 3, 0, kicked));
    CHECK(OutWith(kicked, 3, "kicked"));
    CHECK(b.RoomOf(3) == nullptr);
    RoomChanges left;
    CHECK(b.Leave(2, "left", 0, left));
    CHECK(OutWith(left, 2, "left"));
    CHECK(!b.Leave(2, "left", 0, left));
    RoomChanges closed;
    CHECK(!b.Close(2, closed));
    CHECK(b.Close(1, closed));
    CHECK(OutWith(closed, 1, "closed"));
    CHECK(b.RoomOf(1) == nullptr);
    CHECK(b.Rooms().empty());
}

TEST_CASE(RoomHostLeavingPassesTheRoomAndDirectorLeavingEndsTheRound) {
    RoomBook b;
    Open(b, 1, { 2, 3 });
    AllReady(b, 1, 0);
    RoomChanges go;
    b.Tick(3000, go);
    CHECK(b.RoomOf(1)->state == RoomState::Running);
    RoomChanges left;
    CHECK(b.Leave(1, "left", 4000, left));
    const server::Room* r = b.RoomOf(2);
    CHECK(r != nullptr);
    CHECK_EQ(r->host, (uint8_t)2);
    CHECK_EQ(r->members[0].id, (uint8_t)2);
    CHECK(r->state == RoomState::Ended); // the round was 1's game
    CHECK(Has(left.hostChanged, r->id));
    // A member who is not the director leaves: nothing else changes
    RoomBook c;
    Open(c, 1, { 2, 3 });
    RoomChanges ch;
    c.SetReady(1, true, 0, ch);
    c.SetReady(2, true, 0, ch);
    CHECK(c.RoomOf(1)->state == RoomState::Lobby);
    c.Leave(3, "left", 10, ch); // the only one missing: everyone left is ready now
    CHECK(c.RoomOf(1)->state == RoomState::Starting);
    CHECK_EQ(c.RoomOf(1)->host, (uint8_t)1);
}

TEST_CASE(RoomEndsAndReopensForAnotherRound) {
    RoomBook b;
    Open(b, 1, { 2 }, RoomPlace::Here, "cartero");
    AllReady(b, 1, 0);
    RoomChanges ch;
    b.Tick(3000, ch);
    CHECK(b.RoomOf(1)->state == RoomState::Running);
    b.SetResult(1, "cartero", 25, 1234, -1, ch);
    b.SetResult(1, "otra_cosa", 99, 99, 1, ch); // another activity: not this room's
    RoomChanges end;
    b.EndRound(2, "cartero", 4900, end); // not the director: nothing
    CHECK(b.RoomOf(1)->state == RoomState::Running);
    b.EndRound(1, "cartero", 5000, end);
    const server::Room* r = b.RoomOf(1);
    CHECK(r->state == RoomState::Ended);
    CHECK(Has(end.rooms, r->id));
    CHECK_EQ(r->Member(1)->score, (int64_t)25);
    CHECK_EQ(r->Member(1)->cs, (int64_t)1234);
    // 2 starts its own round (Por turnos): the same room, directed by 2, everyone confirms again
    RoomChanges re;
    CHECK(b.Open(2, "cartero", "Cartero", "turns", RoomPlace::Here, {}, 6000, re) ==
          RoomOpenResult::Reopened);
    r = b.RoomOf(2);
    CHECK(r == b.RoomOf(1));
    CHECK_EQ(r->director, (uint8_t)2);
    CHECK_EQ(r->host, (uint8_t)1);
    CHECK(r->state == RoomState::Lobby);
    CHECK(!r->Member(1)->ready && !r->Member(2)->ready);
    CHECK_EQ(r->Member(1)->score, (int64_t)-1);
    CHECK(Has(re.rooms, r->id));
    // Over again and nobody starts another round: it closes after the linger
    AllReady(b, 2, 6100);
    b.Tick(9100, ch);
    b.EndRound(2, "cartero", 10000, ch);
    RoomChanges still;
    b.Tick(10000 + Times().lingerMs - 1, still);
    CHECK(b.RoomOf(1) != nullptr);
    RoomChanges closed;
    b.Tick(10000 + Times().lingerMs, closed);
    CHECK(b.RoomOf(1) == nullptr && b.RoomOf(2) == nullptr);
    CHECK(OutWith(closed, 1, "ended") && OutWith(closed, 2, "ended"));
}

TEST_CASE(RoomLobbyTimesOut) {
    RoomBook b;
    Open(b, 1, { 2 }, RoomPlace::Anywhere, "galeria_ciudad", 1000);
    RoomChanges early;
    b.Tick(1000 + Times().lobbyMs - 1, early);
    CHECK(b.RoomOf(1) != nullptr);
    RoomChanges late;
    b.Tick(1000 + Times().lobbyMs, late);
    CHECK(b.RoomOf(1) == nullptr);
    CHECK(OutWith(late, 1, "timeout") && OutWith(late, 2, "timeout"));
}

TEST_CASE(RoomJoinsOnlyOpenRooms) {
    RoomBook b;
    Open(b, 1);
    uint32_t id = b.RoomOf(1)->id;
    RoomChanges ch;
    CHECK(b.Join(2, id, 0, ch) == RoomJoinResult::NotOpen);
    RoomSettings s;
    s.open = true;
    s.travel = false;
    CHECK(!b.SetSettings(2, s, ch)); // only the host
    CHECK(b.SetSettings(1, s, ch));
    CHECK(b.RoomOf(1)->settings.open && !b.RoomOf(1)->settings.travel);
    RoomChanges joined;
    CHECK(b.Join(2, id, 10, joined) == RoomJoinResult::Joined);
    CHECK(JoinedHow(joined, 2, "open"));
    CHECK(b.Join(2, id, 10, ch) == RoomJoinResult::Already);
    CHECK(b.Join(3, 999, 10, ch) == RoomJoinResult::Gone);
}

TEST_CASE(RoomForgetDropsThePlayerAndItsInvitations) {
    RoomBook b;
    Open(b, 1, { 2 });
    RoomChanges ch;
    b.Invite(1, 3, 0, kTtl, ch);
    RoomChanges gone;
    b.Forget(1, 10, gone); // the host and director left the world
    CHECK(OutWith(gone, 1, "gone"));
    const server::Room* r = b.RoomOf(2);
    CHECK(r != nullptr && r->host == 2 && r->state == RoomState::Ended);
    CHECK_EQ(b.InvitesTo(3).size(), (size_t)1); // the room's invitation still stands
    RoomChanges invitee;
    b.Forget(3, 20, invitee);
    CHECK(InviteEndedWith(invitee, 3, "cancelled"));
    RoomChanges last;
    b.Forget(2, 30, last);
    CHECK(b.Rooms().empty());
}

TEST_CASE(RoomResultsAreKept) {
    RoomBook b;
    Open(b, 1, { 2 }, RoomPlace::Entrance, "carrera_goron");
    AllReady(b, 1, 0);
    RoomChanges ch;
    b.Tick(3000, ch);
    RoomChanges res;
    b.SetResult(2, "carrera_goron", -1, 8345, 1, res);
    CHECK_EQ(b.RoomOf(2)->Member(2)->won, (int8_t)1);
    CHECK_EQ(b.RoomOf(2)->Member(2)->cs, (int64_t)8345);
    CHECK(Has(res.rooms, b.RoomOf(2)->id));
}
