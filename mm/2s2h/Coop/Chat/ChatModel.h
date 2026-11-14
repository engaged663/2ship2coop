#pragma once
// Chat history shown by ChatWindow. Lines come from the server (chat, pm, sys, join/leave) and local events.
// The game's own lines go in the player's language: Chat_Add(ChatKind::Ok, Tx(Txt::WorldSaved)) (I18n/CoopText.h).
#include "2s2h/Coop/I18n/CoopText.h"

#include "common/PlayerColor.h"

#include <deque>
#include <string>
#include <vector>

namespace coop::client {

enum class ChatKind { Chat, Private, Info, Ok, Warn, Error, Presence, Room }; // Room: an activity room's chat

struct ChatLine {
    ChatKind kind;
    std::string text;
    double time;                 // Chat_Now() when added
    size_t nickLen = 0;          // the first nickLen bytes ("<nick>") go in nickColor (0 = all in the kind's color)
    coop::PlayerColor nickColor; // the sender's color when it was said (Features/PlayerColors.h)
};

void Chat_Add(ChatKind kind, const std::string& text); // "\n" splits into several lines
// A player's line "<nick> text" with the nick in that player's color (playerId 0 = nobody we know: a plain line).
void Chat_AddFrom(ChatKind kind, const std::string& nick, const std::string& text, uint8_t playerId);
const std::deque<ChatLine>& Chat_Lines();
double Chat_Now(); // seconds, monotonic

// What the player typed: "/..." is sent as a command (interpreted by the server), anything else as chat.
void Chat_Submit(const std::string& text);
const std::vector<std::string>& Chat_SentHistory(); // oldest first, for the up/down arrows

} // namespace coop::client
