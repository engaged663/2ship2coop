#pragma once
// [COOP] The game's co-op texts in the server's four languages (es, en, zh, ru): the Co-op menu, the windows (room,
// invitations, mods) and the chat lines the game writes itself. What the server says arrives already in its language.
// The texts live in CoopTextMessages.inc, one line per text with the languages side by side (like the server's
// coop/common/I18nMessages.inc):
//   Tx(Txt::StatusConnecting, { server })   // "{0}" is replaced by the first argument
//   Tc(Txt::Connect)                        // a text without arguments (ImGui labels; the pointer stays valid)
//   Tip(Txt::LiveMenuTip)                   // the same for tooltips (Chinese gets line breaks: it has no spaces)
// To add a text: one line in the .inc. The language: gCoop.Language ("auto": the server's while connected, otherwise
// the system's; "es", "en", "zh", "ru"), applied once per frame by CoopText_FrameStart, which also sets the language
// of coop/common's texts (clock, network errors) and loads the Chinese font when it is needed (CoopFonts.h).
#include "common/I18n.h"

#include <initializer_list>
#include <string>

namespace coop::client {

enum class Txt {
#define X(id, es, en, zh, ru) id,
#include "CoopTextMessages.inc"
#undef X
    Count
};

std::string Tx(Txt id, std::initializer_list<std::string> args = {});
const char* Tc(Txt id);
const char* TcIn(Lang lang, Txt id);
const char* Tip(Txt id);

Lang CoopText_Lang();       // the language the co-op texts are shown in now
Lang CoopText_ServerLang(); // the server's (names the server repeats, like an activity's, go in it); ours if unknown
void CoopText_FrameStart(); // CoopInit.cpp: applies a change of language before the frame's work
void CoopText_DrawMenu();   // the language selector of the Co-op menu

} // namespace coop::client
