#pragma once
// Text helpers shared by client and server: nick rules, chat sanitizing, command-line splitting.
#include <string>
#include <vector>

namespace coop {

// 3..16 chars of [A-Za-z0-9_].
bool IsValidNick(const std::string& nick);

// Tabs/newlines become spaces, other control chars are removed, both ends are trimmed and the
// result is cut to at most maxChars UTF-8 code points (never splitting a multi-byte sequence).
std::string SanitizeChat(const std::string& text, size_t maxChars);

// Splits on spaces; "double quoted" parts form a single token (quotes removed).
std::vector<std::string> SplitCommandLine(const std::string& line);

// Joins tokens[start..] with single spaces ("" if start is past the end).
std::string JoinFrom(const std::vector<std::string>& tokens, size_t start);

std::string ToLower(std::string text); // ASCII only
bool EqualsIgnoreCase(const std::string& a, const std::string& b);

// Whole string must be an optional '-' followed by digits that fit in an int.
bool ParseInt(const std::string& text, int& out);

} // namespace coop
