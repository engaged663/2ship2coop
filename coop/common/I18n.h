#pragma once
// Translations of everything the server says (logs, command help and replies, rejection reasons).
// The texts live in I18nMessages.inc, one line per text with all the languages side by side.
//   Tr(Msg::PlayerNotFound, { nick })   // "{0}" in the text is replaced by the first argument
// The language is one per process: the server sets it at startup (server.json "language", --lang) or with /lang.
// To add a language: add it to Lang, kLangs, LangCode/LangName and one more column in I18n.cpp's table and in every
// X(...) line of the .inc (the compiler rejects a line with a missing column). The game has its own catalog with the
// same columns (mm/2s2h/Coop/I18n/CoopTextMessages.inc): add the column there too.
#include <cstddef>
#include <initializer_list>
#include <string>

namespace coop {

enum class Lang { Es, En, Zh, Ru };
inline constexpr Lang kLangs[] = { Lang::Es, Lang::En, Lang::Zh, Lang::Ru };
inline constexpr size_t kLangCount = sizeof(kLangs) / sizeof(kLangs[0]);

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
const char* TrRaw(Lang lang, Msg id); // the text as written in that language, placeholders and all

// "{0}" "{1}"... of a template replaced by the arguments (a missing argument leaves nothing; arguments are not
// expanded again). Tr and the game's catalog use it.
std::string FormatText(const char* tmpl, std::initializer_list<std::string> args);

} // namespace coop
