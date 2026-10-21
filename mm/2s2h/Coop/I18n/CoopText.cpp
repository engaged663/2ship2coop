// [COOP] See CoopText.h.
#include "CoopText.h"

#include "CoopFonts.h"

#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Host/HostMode.h"

#include "2s2h/BenGui/BenGui.hpp"
#include "2s2h/BenGui/UIWidgets.hpp"

#include <libultraship/bridge/consolevariablebridge.h>
#include <libultraship/libultraship.h>

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <vector>

#ifdef _WIN32
extern "C" __declspec(dllimport) unsigned short __stdcall GetUserDefaultUILanguage(void); // <windows.h>, alone
#endif

namespace coop::client {

namespace {

struct Entry {
    const char* text[kLangCount]; // indexed by Lang
};

const Entry kTexts[] = {
#define X(id, es, en, zh, ru) { { es, en, zh, ru } },
#include "CoopTextMessages.inc"
#undef X
};

static_assert(std::size(kTexts) == (size_t)Txt::Count, "one entry per Txt");

constexpr const char* kLangCVar = "gCoop.Language";
constexpr const char* kChoices[] = { "auto", "es", "en", "zh", "ru" }; // the selector's order

std::atomic<bool> sServerLangKnown{ false };
std::atomic<Lang> sServerLang{ Lang::Es };
std::vector<std::string> sChineseTips; // made once each (Tip)

Lang SystemLang() {
#ifdef _WIN32
    switch (GetUserDefaultUILanguage() & 0x3FF) { // PRIMARYLANGID
        case 0x0A: // LANG_SPANISH
            return Lang::Es;
        case 0x04: // LANG_CHINESE
            return Lang::Zh;
        case 0x19: // LANG_RUSSIAN
            return Lang::Ru;
        default:
            return Lang::En;
    }
#else
    const char* env = std::getenv("LANG");
    std::string code = env != nullptr ? std::string(env).substr(0, 2) : std::string();
    Lang lang;
    return ParseLang(code, lang) ? lang : Lang::En;
#endif
}

// "auto": the server's while connected, otherwise the system's.
Lang AutoLang() {
    return sServerLangKnown ? sServerLang.load() : SystemLang();
}

Lang Wanted() {
    const char* choice = CVarGetString(kLangCVar, "auto");
    Lang lang;
    if (choice != nullptr && std::strcmp(choice, "auto") != 0 && ParseLang(choice, lang)) {
        return lang;
    }
    return AutoLang();
}

void Apply() {
    Lang lang = Wanted();
    if (lang != GetLang()) {
        SetLang(lang); // also coop/common's texts: the clock, the network errors
    }
    CoopFonts_WantChinese(lang == Lang::Zh && !HostMode_Enabled()); // a headless host draws nothing
}

void OnServerLang(const json& ev) {
    Lang lang;
    bool known = ParseLang(GetString(ev, "lang"), lang);
    if (known) {
        sServerLang = lang;
    }
    sServerLangKnown = known;
    Apply(); // first of this event's handlers: what the others write goes in the new language
}

void OnLost(const std::string&) {
    sServerLangKnown = false;
}

// UTF-8 bytes of the character that starts at p.
size_t CharBytes(const char* p) {
    unsigned char c = (unsigned char)*p;
    size_t n = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : 4;
    for (size_t i = 1; i < n; i++) {
        if (p[i] == '\0') {
            return i;
        }
    }
    return n;
}

// Chinese has no spaces where the tooltips' wrapping (UIWidgets::WrappedText) breaks lines: break after its
// punctuation once a line is long enough, or anywhere when it is too long (widths: a Chinese character counts 2).
std::string BreakChineseLines(const char* text) {
    static const char* kBreakAfter[] = { "，", "。", "；", "：", "！", "？", "、", "）" };
    std::string out;
    int width = 0;
    for (const char* p = text; *p != '\0';) {
        size_t n = CharBytes(p);
        std::string ch(p, n);
        p += n;
        out += ch;
        if (ch == "\n") {
            width = 0;
            continue;
        }
        width += n >= 3 ? 2 : 1;
        bool punctuation = false;
        for (const char* mark : kBreakAfter) {
            punctuation = punctuation || ch == mark;
        }
        if (*p != '\0' && ((punctuation && width >= 36) || width >= 60)) {
            out += '\n';
            width = 0;
        }
    }
    return out;
}

const char* ShownName(Lang lang) {
    if (lang == Lang::Zh && ImGui::GetFont()->FindGlyphNoFallback(0x4E2D) == nullptr) {
        return "Chinese"; // its characters are not in the font yet
    }
    return LangName(lang);
}

} // namespace

std::string Tx(Txt id, std::initializer_list<std::string> args) {
    return FormatText(kTexts[(size_t)id].text[(size_t)GetLang()], args);
}

const char* Tc(Txt id) {
    return kTexts[(size_t)id].text[(size_t)GetLang()];
}

const char* TcIn(Lang lang, Txt id) {
    return kTexts[(size_t)id].text[(size_t)lang];
}

const char* Tip(Txt id) {
    if (GetLang() != Lang::Zh) {
        return Tc(id);
    }
    if (sChineseTips.empty()) {
        sChineseTips.resize((size_t)Txt::Count);
    }
    std::string& tip = sChineseTips[(size_t)id];
    if (tip.empty()) {
        tip = BreakChineseLines(TcIn(Lang::Zh, id));
    }
    return tip.c_str();
}

Lang CoopText_Lang() {
    return GetLang();
}

Lang CoopText_ServerLang() {
    return sServerLangKnown ? sServerLang.load() : GetLang();
}

void CoopText_FrameStart() {
    Apply();
    CoopFonts_FrameStart();
}

void CoopText_DrawMenu() {
    const char* choice = CVarGetString(kLangCVar, "auto");
    size_t index = 0;
    for (size_t i = 0; i < std::size(kChoices); i++) {
        if (choice != nullptr && std::strcmp(choice, kChoices[i]) == 0) {
            index = i;
        }
    }
    std::string autoName = Tx(Txt::LangAuto, { ShownName(AutoLang()) });
    std::vector<const char*> names = { autoName.c_str(), ShownName(Lang::Es), ShownName(Lang::En),
                                       ShownName(Lang::Zh), ShownName(Lang::Ru) };
    std::string label = std::string(Tc(Txt::LangLabel)) + " / Language";
    if (GetLang() == Lang::En) {
        label = "Language";
    }
    if (UIWidgets::Combobox(label.c_str(), &index, names,
                            UIWidgets::ComboboxOptions().Color(THEME_COLOR).Tooltip(Tip(Txt::LangTip)))) {
        CVarSetString(kLangCVar, kChoices[index]);
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    }
    if (GetLang() == Lang::Zh && CoopFonts_ChineseMissing()) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", TcIn(Lang::En, Txt::LangNoChineseFont));
        ImGui::PopTextWrapPos();
    }
}

} // namespace coop::client

COOP_ON_EVENT_FIRST(textWelcome, coop::ev::kWelcome, coop::client::OnServerLang);
COOP_ON_EVENT_FIRST(textLang, coop::ev::kLang, coop::client::OnServerLang);
COOP_ON_LOST(textLost, coop::client::OnLost);
