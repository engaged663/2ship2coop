#pragma once
// Single source of truth for the co-op wire protocol (client + server).
// Bump kProtocolVersion whenever an event's meaning or the binary stream layout changes.
#include <cstddef>
#include <cstdint>

namespace coop {

constexpr uint32_t kProtocolVersion = 1;
constexpr uint16_t kDefaultPort = 7780; // UDP

constexpr int kMaxPlayers = 4;
constexpr int kNickMin = 3;
constexpr int kNickMax = 16;
constexpr int kChatMaxChars = 150;    // after sanitizing, in UTF-8 code points
constexpr int kCommandMaxChars = 200; // raw "/..." line
constexpr size_t kMaxEventBytes = 4096;
constexpr int kRateLimitCount = 6; // chat + cmd messages...
constexpr int kRateLimitWindowMs = 3000; // ...per window
constexpr int kHandshakeTimeoutMs = 5000;
constexpr int kGiftTimeoutMs = 10000;
constexpr int kGiftMaxAmount = 999;

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
inline constexpr const char* kLoc = "loc";                 // C->S scene, room, entrance, sceneName; S->C id, scene, sceneName
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
} // namespace ev

// Levels used by "sys" events.
namespace level {
inline constexpr const char* kInfo = "info";
inline constexpr const char* kOk = "ok";
inline constexpr const char* kWarn = "warn";
inline constexpr const char* kError = "error";
} // namespace level

} // namespace coop
