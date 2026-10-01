// [COOP] See Group.h. This game's copy of its group and invitations, and the group's commands: sent as the chat sends
// "/..." lines, so the server answers them (in its language) like any other command.
#include "Group.h"

#include "2s2h/Coop/Chat/ChatModel.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Puppet/PuppetManager.h"

#include "common/Protocol.h"

#include <libultraship/bridge/consolevariablebridge.h>

#include <algorithm>
#include <chrono>

extern "C" {
#include "functions.h"
#include "variables.h"
}

namespace coop::client {

namespace {

uint32_t sGroupId = 0;
uint8_t sLeader = 0;
std::vector<GroupMember> sMembers;
std::string sActivityName;
std::vector<GroupInvite> sInvites;

void Reset() {
    sGroupId = 0;
    sLeader = 0;
    sMembers.clear();
    sActivityName.clear();
    sInvites.clear();
}

std::string NickIn(const std::vector<GroupMember>& members, uint8_t id) {
    for (const GroupMember& m : members) {
        if (m.id == id) {
            return m.nick;
        }
    }
    return std::string();
}

void DropInvitesFrom(uint8_t from) {
    sInvites.erase(std::remove_if(sInvites.begin(), sInvites.end(),
                                  [&](const GroupInvite& i) { return i.from == from; }),
                   sInvites.end());
}

void OnGroup(const json& ev) {
    uint32_t id = (uint32_t)GetInt(ev, "id");
    uint8_t leader = (uint8_t)GetInt(ev, "leader");
    std::vector<GroupMember> members;
    auto list = ev.find("members");
    if (id != 0 && list != ev.end() && list->is_array()) {
        for (const json& m : *list) {
            if (m.is_array() && m.size() == 2 && m[0].is_number_integer() && m[1].is_string()) {
                members.push_back({ (uint8_t)m[0].get<int>(), m[1].get<std::string>() });
            }
        }
    }
    // Same group, another leader (the old one left): say who leads now
    if (id != 0 && id == sGroupId && leader != sLeader && members.size() > 1) {
        Chat_Add(ChatKind::Info, leader == Session_LocalId() ? std::string("Ahora eres el líder del grupo.")
                                                              : NickIn(members, leader) + " es ahora el líder del grupo.");
    }
    sGroupId = id;
    sLeader = id != 0 ? leader : 0;
    sMembers = std::move(members);
    sActivityName = id != 0 ? GetString(ev, "name") : std::string();
}

// The server also says it in the chat (in its language): this is for the window and the menu.
void OnInvite(const json& ev) {
    GroupInvite inv;
    inv.from = (uint8_t)GetInt(ev, "from");
    inv.nick = GetString(ev, "nick");
    inv.activity = GetString(ev, "activity");
    inv.name = GetString(ev, "name");
    inv.expiresMs = Group_NowMs() + std::clamp<int64_t>(GetInt(ev, "ms", 60000), 1000, 600000);
    if (inv.from == 0 || inv.nick.empty()) {
        return;
    }
    DropInvitesFrom(inv.from);
    sInvites.push_back(inv);
}

void OnInviteEnd(const json& ev) {
    DropInvitesFrom((uint8_t)GetInt(ev, "from"));
}

void OnPlayerLeft(const json& ev) {
    DropInvitesFrom((uint8_t)GetInt(ev, "id"));
}

void SendCommand(const std::string& line) {
    if (!Session_IsConnected()) {
        Chat_Add(ChatKind::Error, "No estás conectado a ningún servidor.");
        return;
    }
    json ev = MakeEvent(ev::kCmd);
    ev["line"] = line;
    NetClient::Get().SendEvent(ev);
}

Player* LocalLink() {
    return gPlayState != nullptr ? (Player*)gPlayState->actorCtx.actorLists[ACTORCAT_PLAYER].first : nullptr;
}

} // namespace

int64_t Group_NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

bool Group_Has() {
    return sGroupId != 0;
}

uint8_t Group_Leader() {
    return sLeader;
}

const std::vector<GroupMember>& Group_Members() {
    return sMembers;
}

bool Group_IsMate(uint8_t playerId) {
    if (playerId == 0 || playerId == Session_LocalId()) {
        return false;
    }
    for (const GroupMember& m : sMembers) {
        if (m.id == playerId) {
            return true;
        }
    }
    return false;
}

std::string Group_ActivityName() {
    return sActivityName;
}

const std::vector<GroupInvite>& Group_Invites() {
    int64_t now = Group_NowMs();
    sInvites.erase(std::remove_if(sInvites.begin(), sInvites.end(),
                                  [&](const GroupInvite& i) { return i.expiresMs <= now; }),
                   sInvites.end());
    return sInvites;
}

Actor* Group_MateActor(uint8_t playerId) {
    if (!Group_IsMate(playerId) || gPlayState == nullptr) {
        return nullptr;
    }
    Actor* puppet = PuppetManager_Actor(playerId);
    return (puppet != nullptr && puppet->update != nullptr) ? puppet : nullptr;
}

bool Group_MateNear(uint8_t playerId, float maxDist) {
    Actor* mate = Group_MateActor(playerId);
    Player* link = LocalLink();
    if (mate == nullptr || link == nullptr) {
        return false;
    }
    return maxDist <= 0.f || Actor_WorldDistXYZToActor(&link->actor, mate) <= maxDist;
}

bool Group_AnyMateNear(float maxDist) {
    for (const GroupMember& m : sMembers) {
        if (Group_MateNear(m.id, maxDist)) {
            return true;
        }
    }
    return false;
}

void Group_Invite(const std::string& nick) {
    SendCommand("/invitar " + nick);
}

void Group_Accept(const std::string& nick) {
    SendCommand(nick.empty() ? std::string("/aceptar") : "/aceptar " + nick);
}

void Group_Decline(const std::string& nick) {
    SendCommand(nick.empty() ? std::string("/rechazar") : "/rechazar " + nick);
}

void Group_Leave() {
    SendCommand("/dejargrupo");
}

bool Group_OptDialogues() {
    return CVarGetInteger("gCoop.Group.Dialogues", 1) != 0;
}

bool Group_OptCutscenes() {
    return CVarGetInteger("gCoop.Group.Cutscenes", 1) != 0;
}

bool Group_OptMinigames() {
    return CVarGetInteger("gCoop.Group.Minigames", 1) != 0;
}

bool Group_OptRewards() {
    return CVarGetInteger("gCoop.Group.Rewards", 1) != 0;
}

} // namespace coop::client

COOP_ON_EVENT(groupState, coop::ev::kGroup, coop::client::OnGroup);
COOP_ON_EVENT(groupInvite, coop::ev::kInvite, coop::client::OnInvite);
COOP_ON_EVENT(groupInviteEnd, coop::ev::kInviteEnd, coop::client::OnInviteEnd);
COOP_ON_EVENT(groupPlayerLeft, coop::ev::kLeave, coop::client::OnPlayerLeft);
COOP_ON_EVENT(groupWelcome, coop::ev::kWelcome, [](const coop::json&) { coop::client::Reset(); });
COOP_ON_LOST(groupLost, [](const std::string&) { coop::client::Reset(); });
