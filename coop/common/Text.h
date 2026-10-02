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

// The same for a text of several lines: line breaks stay ("\r\n" and "\r" become "\n") and bytes that are not UTF-8
// become '?'.
std::string SanitizeText(const std::string& text, size_t maxChars);

// Splits on spaces; "double quoted" parts form a single token (quotes removed).
std::vector<std::string> SplitCommandLine(const std::string& line);

// Joins tokens[start..] with single spaces ("" if start is past the end).
std::string JoinFrom(const std::vector<std::string>& tokens, size_t start);

std::string ToLower(std::string text); // ASCII only
bool EqualsIgnoreCase(const std::string& a, const std::string& b);

// Whole string must be an optional '-' followed by digits that fit in an int.
bool ParseInt(const std::string& text, int& out);

// For the game's own text boxes (its font has no UTF-8): ASCII only. Accents are dropped (á -> a, ñ -> n), the
// opening marks (¡ ¿) go, typographic quotes, dashes and the ellipsis become their ASCII form and any other character
// is '?'. A tab is a space, line breaks stay and every other control character goes (they are the font's own codes).
std::string ToGameFontText(const std::string& text);
// After the game wrapped a text into lines: a line break that would start line linesPerBox + 1 of a text box becomes a
// box break (the game only starts a new box where it wraps a line itself).
void SplitTextBoxes(std::string& text, char lineBreak, char boxBreak, int linesPerBox);

// Well-formed UTF-8 (no overlong forms, no surrogates, nothing above U+10FFFF).
bool IsValidUtf8(const std::string& text);
// The same text with every byte that is not part of a well-formed sequence turned into '?'.
std::string ToValidUtf8(std::string text);

} // namespace coop
