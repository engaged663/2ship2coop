#pragma once
// Chat history shown by ChatWindow. Lines come from the server (chat, pm, sys, join/leave) and local events.
#include <deque>
#include <string>
#include <vector>

namespace coop::client {

enum class ChatKind { Chat, Private, Info, Ok, Warn, Error, Presence };

struct ChatLine {
    ChatKind kind;
    std::string text;
    double time; // Chat_Now() when added
};

void Chat_Add(ChatKind kind, const std::string& text); // "\n" splits into several lines
const std::deque<ChatLine>& Chat_Lines();
double Chat_Now(); // seconds, monotonic

// What the player typed: "/..." is sent as a command (interpreted by the server), anything else as chat.
void Chat_Submit(const std::string& text);
const std::vector<std::string>& Chat_SentHistory(); // oldest first, for the up/down arrows

} // namespace coop::client
