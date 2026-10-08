#pragma once
// server.json: created with defaults on first run, edited by hand by the server owner.
#include "common/Events.h"
#include "common/I18n.h"
#include "common/Protocol.h"

#include <map>
#include <string>
#include <vector>

namespace coop::server {

// server.json "mods": which scripts and plugins the server loads and their limits (Mods/ModHost.h).
struct ModsConfig {
    bool enabled = true;
    std::string scriptsDir;           // "" = no scripts (tests); server.json default "mods"
    std::vector<std::string> scripts; // "*" = every .lua of the folder; names or paths; "!name" leaves one out
    std::string pluginsDir;           // "" = no plugins; server.json default "plugins"
    std::vector<std::string> plugins;
    std::string dataDir;              // "" = memory only (tests); server.json default "mods/data"
    bool unsafeLua = false;           // true: scripts get the whole Lua (io, os, package)
    int scriptTimeoutMs = 2000;       // a handler that runs longer is aborted (100..60000)
    int scriptMemoryMb = 64;          // memory of one script (8..1024)
    json settings = json::object();   // per mod: {"name": {...}} (coop.mod.setting)
};

// server.json "o2r": the game mods (.o2r) the players download from this server before playing (O2rStore.h).
struct O2rConfig {
    std::string dir;                        // "" = none (tests); server.json default "o2r"
    std::vector<std::string> files = { "*" }; // "*" = every .o2r of the folder by name; names (in that order of
                                              // loading: the last one wins); "!name" leaves one out
};

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
    // Activity rooms (docs/superpowers/specs/2026-10-04-coop-salas-actividades-design.md): every minigame a game
    // starts opens a room (lobby, invitations, chat, ready check). false: no rooms (the groups work as before).
    bool rooms = true;
    int64_t roomCountdownMs = kRoomCountdownMs; // not stored in server.json (tests only)
    int64_t roomLobbyMs = kRoomLobbyMs;
    int64_t roomLingerMs = kRoomLingerMs;
    bool effects = true;       // the effects echo (stream kStreamEffects): false = never relayed
    // Total sync (docs/superpowers/specs/2026-10-04-coop-sincronizacion-total-design.md): what the games may share.
    // They are told in "welcome" ("sync") and the server drops what is off.
    bool sounds = true;        // the sounds of each Link and of what each game simulates
    bool ambient = true;       // music orders and quakes of what each game simulates ("ambient")
    bool playerObjects = true; // arrows, bombs, hookshot... and what a player carries (room kPlayerRoom)
    bool sceneFlags = true;    // the scene's flags live ("sflag", "sflags")
    bool sceneObjects = true;  // the scene's machinery (platforms, lifts...) is the same for everyone
    bool ocarina = true;       // the notes of the others' ocarina (in the pose)
    bool drops = true;         // what falls is one item for its whole stage (Handlers/ItemHandlers.cpp)
    // Mods and what the owner tunes (docs/superpowers/specs/2026-10-02-coop-mods-api-design.md §4)
    double timeSpeed = 1.0;    // how fast the three days pass (kMinTimeSpeed..kMaxTimeSpeed); 1 = the original game
    int giftMax = kGiftMaxAmount; // rupees of one /gift (1..kGiftMaxAmount)
    std::map<std::string, std::string> commandPermissions; // command -> "player" | "op" | "console"
    json gameSettings = json::object(); // 2 Ship options forced on every game in the world: {"gCheats.X": 1}
    ModsConfig mods;
    O2rConfig o2r; // docs/superpowers/specs/2026-10-03-coop-mods-o2r-design.md
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
    int voteTimeoutMs = kSotVoteMs;           // server.json "voteSeconds" (10..300)
    int worldSaveMs = kWorldSaveMs;           // server.json "saveSeconds" (2..600)
    int backupMs = kBackupMs;                 // server.json "backupMinutes" (0..1440; 0 = no periodic backups)
    int backupKeep = kBackupKeep;             // server.json "backupKeep" (1..500): automatic backups kept
    // Not stored in server.json (tests only)
    int cycleComputeTimeoutMs = kCycleComputeMs;
    int worldCreateTimeoutMs = kWorldCreateMs;
    int clockBroadcastMs = kClockBroadcastMs;
};

// Reads path; if it does not exist it is created with the defaults. False (with err) on invalid JSON.
// A valid "language" also becomes the process language (SetLang) so the warnings below already use it.
// Fields with a wrong type keep their default and are reported in err while still returning true.
// A file from an older server gains the keys it lacks, with their defaults (written back once).
bool LoadOrCreateConfig(const std::string& path, ServerConfig& out, std::string* err);

// Writes "language" into the existing file (other keys are kept). False if it cannot be read or written.
bool SaveConfigLanguage(const std::string& path, Lang language);

// "sync" of "welcome": the total sync's switches above, for the games.
json SyncOptionsJson(const ServerConfig& config);

// What the command line asked for besides the config.
struct CommandLine {
    std::string modDocsDir; // --mod-docs <dir>: write the mod reference there and exit
};
// 2ship-coop-server [--port N] [--lang es|en|zh|ru] [--mods-dir D] [--plugins-dir D] [--script F]... [--plugin F]...
//                   [--no-mods] [--mod-docs D] [--o2r-dir D] [--no-o2r]
// --script and --plugin add to the lists of server.json. Returns the warnings ("" = none).
std::string ApplyCommandLine(ServerConfig& config, int argc, char** argv, CommandLine* out);

} // namespace coop::server
