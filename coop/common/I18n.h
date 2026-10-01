#pragma once
// Translations of everything the server says (logs, command help and replies, rejection reasons).
// The texts live in I18nMessages.inc, one line per text with all the languages side by side.
//   Tr(Msg::PlayerNotFound, { nick })   // "{0}" in the text is replaced by the first argument
// The language is one per process: the server sets it at startup (server.json "language", --lang) or with /lang.
// To add a language: add it to Lang, LangCode/LangName and one more column in I18n.cpp's table and in every X(...)
// line of the .inc (the compiler rejects a line with a missing column).
#include <initializer_list>
#include <string>

namespace coop {

enum class Lang { Es, En, Zh, Ru };

enum class Msg {
#define X(id, es, en, zh, ru) id,
#include "I18nMessages.inc"
#undef X
    Count
};

void SetLang(Lang lang);
Lang GetLang();
const char* LangCode(Lang lang);                        // "es", "en", "zh", "ru"
const char* LangName(Lang lang);                        // "Español", "English", "中文", "Русский"
bool ParseLang(const std::string& text, Lang& out);     // accepts the code or the name, any case

std::string Tr(Msg id, std::initializer_list<std::string> args = {});

} // namespace coop
