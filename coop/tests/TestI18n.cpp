#include "TestNet.h"

#include "server/CommandRegistry.h"

#include "common/Clock.h"
#include "common/I18n.h"

#include <cstdio>

using namespace coop_test;

namespace {

// Puts the language back at the end of the test (the language is one per process).
struct LangGuard {
    coop::Lang saved = coop::GetLang();
    explicit LangGuard(coop::Lang lang) {
        coop::SetLang(lang);
    }
    ~LangGuard() {
        coop::SetLang(saved);
    }
};

const coop::Lang kAllLangs[] = { coop::Lang::Es, coop::Lang::En, coop::Lang::Zh, coop::Lang::Ru };

// Which of {0}..{9} a text uses, as a bit mask.
unsigned PlaceholderMask(coop::Msg id) {
    std::string text = coop::Tr(id, { "@0@", "@1@", "@2@", "@3@", "@4@", "@5@", "@6@", "@7@", "@8@", "@9@" });
    unsigned mask = 0;
    for (unsigned i = 0; i < 10; i++) {
        if (text.find("@" + std::to_string(i) + "@") != std::string::npos) {
            mask |= 1u << i;
        }
    }
    return mask;
}

} // namespace

TEST_CASE(TrReplacesPlaceholders) {
    LangGuard g(coop::Lang::En);
    CHECK_EQ(coop::Tr(coop::Msg::PlayerNotFound, { "Bob" }), std::string("No player named 'Bob' is connected."));
    coop::SetLang(coop::Lang::Es);
    CHECK_EQ(coop::Tr(coop::Msg::PlayerNotFound, { "Bob" }), std::string("No hay ningún jugador conectado llamado 'Bob'."));
    // A missing argument leaves nothing behind; an argument with braces is not expanded again.
    CHECK_EQ(coop::Tr(coop::Msg::UsageLine, {}), std::string("Uso: "));
    CHECK_EQ(coop::Tr(coop::Msg::UsageLine, { "{0}" }), std::string("Uso: {0}"));
}

TEST_CASE(EveryTextExistsInEveryLanguageWithTheSamePlaceholders) {
    LangGuard g(coop::Lang::Es);
    for (size_t i = 0; i < (size_t)coop::Msg::Count; i++) {
        coop::Msg id = (coop::Msg)i;
        coop::SetLang(coop::Lang::Es);
        unsigned reference = PlaceholderMask(id);
        for (coop::Lang lang : kAllLangs) {
            coop::SetLang(lang);
            CHECK(!coop::Tr(id).empty());
            if (PlaceholderMask(id) != reference) {
                std::printf("  placeholders differ: message %zu in %s\n", i, coop::LangCode(lang));
                CHECK(false);
            }
        }
    }
}

TEST_CASE(ParseLangAcceptsCodesAndNames) {
    coop::Lang l = coop::Lang::Es;
    CHECK(coop::ParseLang("en", l) && l == coop::Lang::En);
    CHECK(coop::ParseLang("ZH", l) && l == coop::Lang::Zh);
    CHECK(coop::ParseLang("ru", l) && l == coop::Lang::Ru);
    CHECK(coop::ParseLang("Español", l) && l == coop::Lang::Es);
    CHECK(coop::ParseLang("english", l) && l == coop::Lang::En);
    CHECK(coop::ParseLang("chino", l) && l == coop::Lang::Zh);
    CHECK(coop::ParseLang("Русский", l) && l == coop::Lang::Ru);
    CHECK(!coop::ParseLang("klingon", l));
    CHECK(!coop::ParseLang("", l));
}

TEST_CASE(ClockFormatFollowsTheLanguage) {
    LangGuard g(coop::Lang::Es);
    CHECK_EQ(coop::clock::Format(0), std::string("Día 1, 06:00"));
    coop::SetLang(coop::Lang::En);
    CHECK_EQ(coop::clock::Format(0), std::string("Day 1, 06:00"));
    coop::SetLang(coop::Lang::Ru);
    CHECK_EQ(coop::clock::Format(0), std::string("День 1, 06:00"));
}

TEST_CASE(RepliesAndLogsUseTheServerLanguage) {
    LangGuard g(coop::Lang::En);
    TestServer s;
    auto a = Join(s, "Alice");
    a->Cmd("/nope");
    auto e = a->WaitFor("sys", s);
    CHECK(e.has_value());
    CHECK(std::string((*e)["text"]).find("Unknown command") != std::string::npos);
    a->Cmd("/help");
    auto h = a->WaitFor("sys", s);
    CHECK(h.has_value());
    CHECK(std::string((*h)["text"]).find("Available commands:") != std::string::npos);
    CHECK(std::string((*h)["text"]).find("/pm <player> <message> - private message") != std::string::npos);
    bool loggedInEnglish = false;
    for (const std::string& line : s.log.Lines()) {
        loggedInEnglish = loggedInEnglish || line.find("joined") != std::string::npos;
    }
    CHECK(loggedInEnglish);
}

TEST_CASE(LangCommandSwitchesTheWholeServer) {
    LangGuard g(coop::Lang::Es);
    TestServer s;
    auto a = Join(s, "Alice");
    a->Cmd("/lang zh");
    CHECK(a->WaitForSys("error", s)); // not an admin yet
    CHECK(coop::GetLang() == coop::Lang::Es);
    s.server->ExecuteConsoleLine("op Alice");
    CHECK(a->WaitForSys("ok", s));
    a->Cmd("/lang zh");
    CHECK(a->WaitForSys("ok", s));
    CHECK(coop::GetLang() == coop::Lang::Zh);
    a->Cmd("/lang");
    auto cur = a->WaitFor("sys", s);
    CHECK(cur.has_value());
    CHECK(std::string((*cur)["text"]).find("服务器语言") != std::string::npos);
    a->Cmd("/lang klingon");
    CHECK(a->WaitForSys("error", s));
    s.server->ExecuteConsoleLine("lang ru");
    CHECK(coop::GetLang() == coop::Lang::Ru);
    s.server->ExecuteConsoleLine("lang es");
    CHECK(coop::GetLang() == coop::Lang::Es);
}

TEST_CASE(EnglishAliasesRunTheSpanishCommands) {
    LangGuard g(coop::Lang::En);
    TestServer s;
    auto a = Join(s, "Alice");
    a->Cmd("/clock");
    auto t = a->WaitFor("sys", s);
    CHECK(t.has_value());
    CHECK(std::string((*t)["text"]).find("There is no world yet") != std::string::npos);
    a->Cmd("/yes");
    CHECK(a->WaitForSys("warn", s)); // "There is no vote in progress" (and not "unknown command")
    CHECK(coop::server::FindCommand("world") == coop::server::FindCommand("mundo"));
    CHECK(coop::server::FindCommand("restart") == coop::server::FindCommand("reiniciar"));
}
