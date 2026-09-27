#pragma once
// Single source of truth for the co-op wire protocol (client + server).
// Bump kProtocolVersion whenever an event's meaning or the binary stream layout changes.
#include <cstddef>
#include <cstdint>

namespace coop {

constexpr uint32_t kProtocolVersion = 2; // v2: shared world (sub-project B)
constexpr uint16_t kDefaultPort = 7780; // UDP

constexpr int kMaxPlayers = 4;
constexpr int kNickMin = 3;
constexpr int kNickMax = 16;
constexpr int kChatMaxChars = 150;    // after sanitizing, in UTF-8 code points
constexpr int kCommandMaxChars = 200; // raw "/..." line
constexpr size_t kMaxClientEventBytes = 12288;  // biggest event the server accepts (a whole world fits)
constexpr size_t kMaxServerEventBytes = 262144; // biggest event a game accepts from the server
constexpr int kRateLimitCount = 6; // chat + cmd messages...
constexpr int kRateLimitWindowMs = 3000; // ...per window
constexpr int kHandshakeTimeoutMs = 5000;
constexpr int kGiftTimeoutMs = 10000;
constexpr int kGiftMaxAmount = 999;
constexpr int kGiftOrphanMs = 60000;    // an unpaid gift that was cancelled still refunds a late payment
constexpr int kInvalidLogCount = 3;     // invalid packets logged per connection...
constexpr int kInvalidKickCount = 50;   // ...and how many get it kicked
constexpr size_t kMaxPacketBytes = 16384; // ENet refuses to reassemble anything bigger (server)
constexpr int kStreamBurst = 40;        // pose packets relayed per player: burst...
constexpr int kStreamPerSecond = 30;    // ...and sustained rate (clients send 20/s)
constexpr int kLocBurst = 10;           // location broadcasts per player: burst...
constexpr int kLocPerSecond = 5;        // ...and sustained rate

// Shared world (sub-project B)
constexpr int kWopsBurst = 60;          // world change packets per player: burst...
constexpr int kWopsPerSecond = 30;      // ...and sustained rate (games send at most 10/s)
constexpr int kInvBurst = 10;           // inventory uploads per player: burst...
constexpr int kInvPerSecond = 2;        // ...and sustained rate (games send one every 5 s)
constexpr size_t kMaxInventoryBytes = 6144; // with the whole world it must fit in one world_full (< kMaxPacketBytes)
constexpr int kClockBroadcastMs = 1000; // the clock goes to everyone in the world this often (and on changes)
constexpr int kWorldSaveMs = 10000;     // world.json and players/ are saved this often
constexpr int kSotVoteMs = 30000;       // Song of Time vote
constexpr int kCycleComputeMs = 15000;  // a game has this long to compute the new cycle
constexpr int kWorldCreateMs = 15000;   // the first player has this long to create the world

enum Channel : uint8_t {
    kChannelEvents = 0, // reliable + ordered, JSON events
    kChannelStream = 1, // unreliable + sequenced, binary streams
    kChannelCount = 2,
};

// Event names (JSON field "t"). Direction and fields are documented in coop/README.md.
namespace ev {
inline constexpr const char* kHello = "hello";             // C->S proto, nick, pass
inline constexpr const char* kWelcome = "welcome";         // S->C id, nick, motd, players[]
inline constexpr const char* kReject = "reject";           // S->C reason
inline constexpr const char* kKicked = "kicked";           // S->C reason
inline constexpr const char* kJoin = "join";               // S->C id, nick
inline constexpr const char* kLeave = "leave";             // S->C id, nick, reason
inline constexpr const char* kLoc = "loc";                 // C->S scene, room, entrance, sceneName, timeStopped; S->C id, scene, sceneName
inline constexpr const char* kChat = "chat";               // C->S text; S->C from, text
inline constexpr const char* kPm = "pm";                   // S->C from, to, text
inline constexpr const char* kCmd = "cmd";                 // C->S line
inline constexpr const char* kSys = "sys";                 // S->C text, level
inline constexpr const char* kTp = "tp";                   // S->C scene, entrance, room, pos[3], rot
inline constexpr const char* kGiftDebit = "gift_debit";    // S->C gid, to, amount
inline constexpr const char* kGiftPaid = "gift_paid";      // C->S gid, paid
inline constexpr const char* kGiftCredit = "gift_credit";  // S->C gid, from, amount
inline constexpr const char* kGiftRecv = "gift_recv";      // C->S gid, accepted
inline constexpr const char* kGiftRefund = "gift_refund";  // S->C gid, amount, reason
// Shared world (sub-project B)
inline constexpr const char* kWorldEnter = "world_enter";     // C->S (asks to play in the server's world)
inline constexpr const char* kWorldLeave = "world_leave";     // C->S
inline constexpr const char* kWorldFull = "world_full";       // S->C create, fields{}, cycle, clock{}, you{inv, stale}, reset
inline constexpr const char* kWorldInit = "world_init";       // C->S fields{} (the creator, once)
inline constexpr const char* kWops = "wops";                  // C->S bits[], bytes[], adds[], cycle; S->C same + from
inline constexpr const char* kInv = "inv";                    // C->S inv{} (this player's own data, opaque), cycle (the world cycle it belongs to)
inline constexpr const char* kClock = "clock";                // S->C abs, inv, stopped, jump
inline constexpr const char* kClockJump = "clock_jump";       // C->S (Song of Double Time)
inline constexpr const char* kClockSpeed = "clock_speed";     // C->S inv (Inverted Song of Time)
inline constexpr const char* kSotPropose = "sot_propose";     // C->S (Song of Time: starts a vote)
inline constexpr const char* kCycleCompute = "cycle_compute"; // S->C (run the end-of-cycle rules, send the world)
inline constexpr const char* kCycleResult = "cycle_result";   // C->S fields{}
} // namespace ev

// Levels used by "sys" events.
namespace level {
inline constexpr const char* kInfo = "info";
inline constexpr const char* kOk = "ok";
inline constexpr const char* kWarn = "warn";
inline constexpr const char* kError = "error";
} // namespace level

} // namespace coop
