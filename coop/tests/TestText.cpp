#include "TestMain.h"

#include "common/ByteStream.h"
#include "common/Text.h"

TEST_CASE(NickValidation) {
    CHECK(coop::IsValidNick("Link_64"));
    CHECK(!coop::IsValidNick("ab"));                // too short
    CHECK(!coop::IsValidNick("abcdefghijklmnopq")); // 17 chars
    CHECK(!coop::IsValidNick("con espacio"));
    CHECK(!coop::IsValidNick("\xC3\xB1" "and\xC3\xBA")); // "ñandú": non-ASCII
}

TEST_CASE(SanitizeChatStripsControlAndTrims) {
    CHECK_EQ(coop::SanitizeChat("  hola\tmundo\x01 ", 150), std::string("hola mundo"));
    CHECK_EQ(coop::SanitizeChat("", 150), std::string(""));
}

TEST_CASE(SanitizeChatTruncatesOnUtf8Boundary) {
    // "ñññññ" = 5 code points, 10 bytes; keeping 3 code points must not split a sequence.
    std::string s = coop::SanitizeChat("\xC3\xB1\xC3\xB1\xC3\xB1\xC3\xB1\xC3\xB1", 3);
    CHECK_EQ(s, std::string("\xC3\xB1\xC3\xB1\xC3\xB1"));
}

TEST_CASE(SplitCommandLineQuotes) {
    auto t = coop::SplitCommandLine("/pm \"Bob\" \"hola que tal\"");
    CHECK_EQ(t.size(), (size_t)3);
    CHECK_EQ(t[0], std::string("/pm"));
    CHECK_EQ(t[1], std::string("Bob"));
    CHECK_EQ(t[2], std::string("hola que tal"));
    auto u = coop::SplitCommandLine("  /gift   Bob   50 ");
    CHECK_EQ(u.size(), (size_t)3);
    CHECK_EQ(u[2], std::string("50"));
}

TEST_CASE(JoinFromAndCaseHelpers) {
    std::vector<std::string> t = { "/pm", "Bob", "hola", "que", "tal" };
    CHECK_EQ(coop::JoinFrom(t, 2), std::string("hola que tal"));
    CHECK_EQ(coop::JoinFrom(t, 9), std::string(""));
    CHECK(coop::EqualsIgnoreCase("LiNk", "link"));
    CHECK(!coop::EqualsIgnoreCase("Link", "Links"));
    CHECK_EQ(coop::ToLower("AbC_1"), std::string("abc_1"));
}

TEST_CASE(ParseIntStrict) {
    int v = 0;
    CHECK(coop::ParseInt("50", v));
    CHECK_EQ(v, 50);
    CHECK(coop::ParseInt("-3", v));
    CHECK_EQ(v, -3);
    CHECK(!coop::ParseInt("5x", v));
    CHECK(!coop::ParseInt("", v));
    CHECK(!coop::ParseInt("99999999999", v));
}

TEST_CASE(ByteStreamRoundTrip) {
    coop::Writer w;
    w.U8(1);
    w.S16(-2);
    w.U32(0xDEADBEEF);
    w.F32(1.5f);
    CHECK_EQ(w.Data().size(), (size_t)11);
    coop::Reader r(w.Data().data(), w.Data().size());
    uint8_t a = 0;
    int16_t b = 0;
    uint32_t c = 0;
    float d = 0;
    CHECK(r.U8(a) && r.S16(b) && r.U32(c) && r.F32(d));
    CHECK_EQ(a, (uint8_t)1);
    CHECK_EQ(b, (int16_t)-2);
    CHECK_EQ(c, 0xDEADBEEFu);
    CHECK_EQ(d, 1.5f);
    CHECK_EQ(r.Remaining(), (size_t)0);
    CHECK(!r.U8(a)); // past the end
}

TEST_CASE(ByteStreamIsLittleEndian) {
    coop::Writer w;
    w.U16(0x1234);
    CHECK_EQ(w.Data().size(), (size_t)2);
    CHECK_EQ(w.Data()[0], (uint8_t)0x34);
    CHECK_EQ(w.Data()[1], (uint8_t)0x12);
}

TEST_CASE(GameFontTextIsPlainAscii) {
    // The game's text boxes have no UTF-8 (a mod's "message"): accents go, the opening marks go, the rest is '?'.
    CHECK_EQ(coop::ToGameFontText("\xC2\xA1Hola, Ni\xC3\xB1o! \xC2\xBFQu\xC3\xA9 tal?"),
             std::string("Hola, Nino! Que tal?"));
    CHECK_EQ(coop::ToGameFontText("\xC3\x81\xC3\x89\xC3\x8D\xC3\x93\xC3\x9A\xC3\x9C\xC3\x91 \xC3\xA7\xC3\xA0\xC3\xBC"),
             std::string("AEIOUUN cau"));
    CHECK_EQ(coop::ToGameFontText("\xE2\x80\x9C" "fin\xE2\x80\x9D \xE2\x80\x94 ya\xE2\x80\xA6"),
             std::string("\"fin\" - ya...")); // typographic quotes, dash and ellipsis
    CHECK_EQ(coop::ToGameFontText("\xE4\xB8\xAD \xE2\x82\xAC"), std::string("? ?")); // a Chinese character, the euro
    CHECK_EQ(coop::ToGameFontText("l\xC3\xADnea 1\nl\xC3\xADnea 2"), std::string("linea 1\nlinea 2"));
    // Bytes below 0x20 are the font's own codes (colours, pauses, the end of the text): never through. A tab is a
    // space; a byte that is not UTF-8 is '?' (0xBF would end the text).
    CHECK_EQ(coop::ToGameFontText(std::string("a\x01" "b\x10" "c\tc\x7F" "d\xBF" "\0e", 12)),
             std::string("abc cd?e"));
}

TEST_CASE(TextBoxLinesSplitIntoBoxes) {
    // A line break that would start the 5th line of a text box becomes a box break; a box break starts a new count.
    std::string text = "1\x11" "2\x11" "3\x11" "4\x11" "5\x11" "6";
    coop::SplitTextBoxes(text, '\x11', '\x10', 4);
    CHECK_EQ(text, std::string("1\x11" "2\x11" "3\x11" "4\x10" "5\x11" "6"));
    std::string boxed = "1\x10" "2\x11" "3\x11" "4\x11" "5\x10" "6";
    coop::SplitTextBoxes(boxed, '\x11', '\x10', 4);
    CHECK_EQ(boxed, std::string("1\x10" "2\x11" "3\x11" "4\x11" "5\x10" "6"));
}
