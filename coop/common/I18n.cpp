#include "I18n.h"

#include "Text.h"

#include <atomic>
#include <iterator>

namespace coop {

namespace {

struct Entry {
    const char* text[4]; // indexed by Lang
};

const Entry kMessages[] = {
#define X(id, es, en, zh, ru) { { es, en, zh, ru } },
#include "I18nMessages.inc"
#undef X
};

static_assert(std::size(kMessages) == (size_t)Msg::Count, "one entry per Msg");

std::atomic<Lang> sLang{ Lang::Es };

} // namespace

void SetLang(Lang lang) {
    sLang = lang;
}

Lang GetLang() {
    return sLang;
}

const char* LangCode(Lang lang) {
    switch (lang) {
        case Lang::En:
            return "en";
        case Lang::Zh:
            return "zh";
        case Lang::Ru:
            return "ru";
        case Lang::Es:
        default:
            return "es";
    }
}

const char* LangName(Lang lang) {
    switch (lang) {
        case Lang::En:
            return "English";
        case Lang::Zh:
            return "中文";
        case Lang::Ru:
            return "Русский";
        case Lang::Es:
        default:
            return "Español";
    }
}

bool ParseLang(const std::string& text, Lang& out) {
    for (Lang lang : { Lang::Es, Lang::En, Lang::Zh, Lang::Ru }) {
        if (EqualsIgnoreCase(text, LangCode(lang)) || text == LangName(lang) ||
            EqualsIgnoreCase(text, LangName(lang))) {
            out = lang;
            return true;
        }
    }
    // Common aliases.
    if (EqualsIgnoreCase(text, "spanish") || EqualsIgnoreCase(text, "espanol") || EqualsIgnoreCase(text, "es-es")) {
        out = Lang::Es;
    } else if (EqualsIgnoreCase(text, "english") || EqualsIgnoreCase(text, "ingles") || EqualsIgnoreCase(text, "en-us")) {
        out = Lang::En;
    } else if (EqualsIgnoreCase(text, "chinese") || EqualsIgnoreCase(text, "chino") || EqualsIgnoreCase(text, "zh-cn")) {
        out = Lang::Zh;
    } else if (EqualsIgnoreCase(text, "russian") || EqualsIgnoreCase(text, "ruso")) {
        out = Lang::Ru;
    } else {
        return false;
    }
    return true;
}

std::string Tr(Msg id, std::initializer_list<std::string> args) {
    const char* tmpl = kMessages[(size_t)id].text[(size_t)sLang.load()];
    std::string out;
    for (const char* p = tmpl; *p != '\0'; p++) {
        if (*p == '{' && p[1] >= '0' && p[1] <= '9' && p[2] == '}') {
            size_t index = (size_t)(p[1] - '0');
            if (index < args.size()) {
                out += *(args.begin() + index);
            }
            p += 2;
        } else {
            out.push_back(*p);
        }
    }
    return out;
}

} // namespace coop
