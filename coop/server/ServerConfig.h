#pragma once
// server.json: created with defaults on first run, edited by hand by the server owner.
#include "common/I18n.h"
#include "common/Protocol.h"

#include <string>

namespace coop::server {

struct ServerConfig {
    uint16_t port = kDefaultPort;
    int maxPlayers = kMaxPlayers; // clamped to 1..kMaxPlayers
    std::string password;         // empty = no password
    std::string motd; // empty = the default welcome text, in the server's language
    Lang language = Lang::Es; // language of the logs and of the commands' replies: es, en, zh or ru
    bool sharedEnemies = true; // sub-project C: enemies simulated by one player per room; false = each game its own
    bool sharedProps = true;   // pots, grass, crates, rupees lying around: broken/picked up for everyone
    bool endingForAll = true;  // a player on the Clock Tower's rooftop, in Majora's lair or in the ending takes
                               // everyone there
    // Groups (docs/superpowers/specs/2026-09-30-coop-grupos-actividades-design.md): invitations and everything a
    // group shares (minigames, dialogues, cutscenes, prizes). false: no groups.
    bool groups = true;
    bool bossCutscenes = true; // bosses' cutscenes, title cards and dialogues go to everyone in their scene
    int inviteMs = kInviteMs;  // server.json "inviteSeconds" (10..600)
    bool effects = true;       // the effects echo (stream kStreamEffects): false = never relayed
    // Sub-project D: the secret a headless host game shows to be accepted as the server's own. Empty = no hosts.
    // The server generates one for the hosts it starts; server.json may fix one for testing.
    std::string hostToken;
    // Sub-project D3: only games with the same executable as the first one accepted (actors copy code pointers).
    bool requireSameBuild = true;
    int64_t leaseExpireMs = 3000; // an NPC lent to a player goes back if it is not asked for again (tests shorten it)
    int handshakeTimeoutMs = kHandshakeTimeoutMs; // not stored in server.json (tests only)
    int giftTimeoutMs = kGiftTimeoutMs;           // not stored in server.json (tests only)
    // Shared world files, set by main.cpp (world.json and players/ next to the exe); "" = memory only (tests).
    std::string worldPath;
    std::string playersDir;
    std::string configPath; // server.json, where /lang saves the language ("" = not saved, tests)
    // Not stored in server.json (tests only)
    int voteTimeoutMs = kSotVoteMs;
    int cycleComputeTimeoutMs = kCycleComputeMs;
    int worldCreateTimeoutMs = kWorldCreateMs;
    int worldSaveMs = kWorldSaveMs;
    int clockBroadcastMs = kClockBroadcastMs;
};

// Reads path; if it does not exist it is created with the defaults. False (with err) on invalid JSON.
// A valid "language" also becomes the process language (SetLang) so the warnings below already use it.
// Fields with a wrong type keep their default and are reported in err while still returning true.
bool LoadOrCreateConfig(const std::string& path, ServerConfig& out, std::string* err);

// Writes "language" into the existing file (other keys are kept). False if it cannot be read or written.
bool SaveConfigLanguage(const std::string& path, Lang language);

} // namespace coop::server
