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
