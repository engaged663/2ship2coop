#pragma once
// [COOP] Minigames and side quests with the group (spec §3-§5). Map:
//   ActivityTable.cpp  one line per minigame / quest: how it is played together (EDIT THIS to change one)
//   Director.cpp       this game runs one: says so, keeps its NPC, sends its HUD, takes the group to its entrances
//   Guest.cpp          a mate runs one here: our Link joins it (equipment, HUD, place) or follows it
//   Follow.cpp         "follow": go where the director goes (a minigame's entrance, the end of a cutscene)
//   Rewards.cpp        prizes: what is each player's own is copied to the mates who were there
#include <cstdint>
#include <string>

struct Actor;

namespace coop::client {

struct TrackedActor;

enum class ActivityMode : uint8_t {
    Together, // everyone plays the same game: what the guests do counts (targets simulated by the director's game)
    EachOwn,  // everyone follows the director in and each game runs its own copy (a race with the same rivals)
    Turns,    // one player at a time: the others watch its time and points; when it ends, the next one plays
};

struct ActivityProp {
    int16_t id; // ACTOR_* of the room's list the minigame uses (0 = end): the director borrows every one of them
    bool ours;  // machinery acting with the director's Link (boat, lifts, walls); false: targets any Link may touch
};

struct ActivityHooks {
    void (*guestStart)();                   // a guest follows the director into its entrance: start its own run
    bool (*result)(bool* won, int32_t* cs); // this game's run ended: won?, time in centiseconds (false: no result)
};

enum class ActivityKind : uint8_t { Minigame, Quest };

struct GuestSlot {
    float right;   // units to the director's right
    float forward; // units in front of it
};

struct ActivityDef {
    const char* key;  // ASCII [a-z0-9_]: travels in the protocol
    const char* name; // Spanish, shown to the players
    ActivityKind kind;
    int16_t scene;      // SCENE_* where the minigame is played (-1: quests)
    int16_t npcs[6];    // ACTOR_* of its NPCs (0 = end); npcs[0] runs a minigame
    ActivityMode mode;
    uint16_t entrances[3]; // the scene reloaded for the minigame (ENTRANCE(...)); 0 = none
    bool follow;           // the mates near the director go with it to those entrances (and out again)
    bool loadout;          // Together: the guests get the director's minigame equipment
    GuestSlot slots[3];    // Together: where the guests stand when it starts (by their order in the group)
    ActivityProp props[4];      // room actors the minigame uses (Director.cpp borrows them while it runs)
    const ActivityHooks* hooks; // Cada uno: its own run (nullptr: none)
};

const ActivityDef* Activity_All(size_t* count);
const ActivityDef* Activity_ByKey(const std::string& key);
const ActivityDef* Activity_MinigameOfScene(int16_t scene);
const ActivityDef* Activity_QuestOfNpc(int16_t actorId);
bool Activity_IsSpecialEntrance(const ActivityDef& def, uint16_t entrance);
bool Activity_IsPropId(int16_t actorId); // some line borrows it (ReplicationRules.cpp replicates it)
const ActivityProp* Activity_Prop(const ActivityDef& def, int16_t actorId); // nullptr: not one of its props
std::string Activity_TimeText(int64_t cs); // "01:23.45"

// Director.cpp
const ActivityDef* Director_Running();                      // the minigame this game runs now (nullptr: none)
const ActivityDef* Director_Recent();                       // running, or ended less than 20 s ago
bool Director_PinsActorId(int16_t actorId, int16_t scene);  // its NPC and props stay ours (lease never given back)
bool Director_PinsFamily(const Actor* root);                // its NPC's family and "ours" props deal with our Link only
int64_t Director_LastOwnRunMs(const std::string& key); // when this game last ran that activity (0: never)
void Director_ReportResult(const ActivityDef& def);    // Cada uno: this game's run ended (sends "act result")

// Guest.cpp
bool Guest_Joined();                  // a mate's activity counts us in here (we never announce one as ours)
uint8_t Guest_Director();             // whose (0: none)
const ActivityDef* Guest_Activity();
bool Guest_SkipsLease(int16_t actorId); // its NPC is the director's: we never ask for it
bool Guest_ContactTarget(const TrackedActor& t); // a target of our Together activity: our contacts with it count
void Guest_OnFollow(uint8_t from, const std::string& key); // Follow.cpp: we go with it into its activity

// Rewards.cpp
void Rewards_SuppressBegin(); // Gift.cpp, AdminFeatures.cpp, Guest/Rewards: these rupees are not a prize
void Rewards_SuppressEnd();

} // namespace coop::client
