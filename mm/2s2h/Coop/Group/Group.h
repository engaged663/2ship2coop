#pragma once
// [COOP] Groups (spec: docs/superpowers/specs/2026-09-30-coop-grupos-actividades-design.md). The server keeps who is
// in which group and the invitations; this is this game's copy ("group", "invite", "invite_end"). Map:
//   GroupState.cpp    the state, the mates next to us, the commands (sent as "/..." lines, like the chat)
//   InviteWindow.cpp  the pop-up of the pending invitations (Aceptar / Rechazar)
//   GroupMenu.cpp     the "Grupo e invitaciones" section of the Co-op menu (F1)
// Being in a group is what shares minigames, dialogues, cutscenes and prizes (Activities/, Features/TalkSync.cpp,
// Features/Cinema.cpp) with the mates who are near.
#include <cstdint>
#include <string>
#include <vector>

struct Actor;

namespace coop::client {

struct GroupMember {
    uint8_t id = 0;
    std::string nick;
};

struct GroupInvite {
    uint8_t from = 0;
    std::string nick;
    std::string activity; // key of what the inviter was doing ("" = just its group)
    std::string name;     // its name, to show
    int64_t expiresMs = 0; // Group_NowMs() clock
};

int64_t Group_NowMs();
bool Group_Has();                                // in a group (maybe still waiting for someone to accept)
uint8_t Group_Leader();
const std::vector<GroupMember>& Group_Members(); // the leader first; includes us
bool Group_IsMate(uint8_t playerId);             // in our group and not us
std::string Group_ActivityName();                // what the leader is doing ("" = nothing known)
const std::vector<GroupInvite>& Group_Invites(); // pending, oldest first (expired ones are dropped)

Actor* Group_MateActor(uint8_t playerId);             // a mate's Link (its puppet) in our scene, or nullptr
bool Group_MateNear(uint8_t playerId, float maxDist); // ...within maxDist of our Link (<= 0: anywhere in the scene)
bool Group_AnyMateNear(float maxDist);

void Group_Invite(const std::string& nick);  // "todos" invites everyone in the server's world
void Group_Accept(const std::string& nick);  // "" = the latest invitation
void Group_Decline(const std::string& nick); // "" = all of them
void Group_Leave();

// The player's options (gCoop.Group.*, on by default)
bool Group_OptDialogues(); // see the mates' dialogues
bool Group_OptCutscenes(); // see the mates' and the bosses' cutscenes
bool Group_OptMinigames(); // join the mates' minigames (equipment, HUD, place, following them)
bool Group_OptRewards();   // get a copy of the mates' prizes

void GroupMenu_Draw(); // GroupMenu.cpp (CoopMenu.cpp calls it)

} // namespace coop::client
