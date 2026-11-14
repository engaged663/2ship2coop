// Each Link's tunic in their player's color (common/TunicSplit.h): the copy of a tunic display list with an "end of
// list" at every color, and the color put back with the original's LOD fraction and alpha.
#include "TestMain.h"

#include "common/TunicSplit.h"

using namespace coop;

namespace {

constexpr TunicWord kEnd{ 0xDF000000ull, 0 };

TunicWord Prim(uint8_t minLevel, uint8_t lodFrac, uint32_t rgba) {
    return TunicWord{ (0xFAull << 24) | ((uint64_t)minLevel << 8) | lodFrac, rgba };
}

TunicWord Call(uint64_t target) {
    return TunicWord{ 0xDEull << 24, target };
}

TunicWord Other(uint64_t n) {
    return TunicWord{ (0xE7ull << 24) | n, n }; // anything but a color, a call or an end
}

// A list like Link's waist: some setup, the tunic color at 2, triangles, end.
std::vector<TunicWord> Waist() {
    return { Other(1), Other(2), Prim(0, 0x80, 0x1E691BFF), Other(3), Other(4), kEnd };
}

bool Same(const TunicWord& a, const TunicWord& b) {
    return a.w0 == b.w0 && a.w1 == b.w1;
}

} // namespace

TEST_CASE(TunicCopyEndsTheListAtEveryColor) {
    std::vector<TunicWord> list = Waist();
    std::optional<std::vector<TunicWord>> copy = BuildTunicCopy(list.data(), list.size(), { 2 }, kEnd);
    CHECK(copy.has_value());
    CHECK_EQ(copy->size(), list.size());
    for (size_t i = 0; i < list.size(); i++) {
        CHECK(Same((*copy)[i], i == 2 ? kEnd : list[i]));
    }

    // Two colors in one list (the shoulders): both become ends, the rest is untouched
    std::vector<TunicWord> shoulder = { Other(1), Prim(0, 0, 0x1E691BFF), Other(2), Other(3),
                                        Prim(0, 0, 0x1E691BFF), Other(4), kEnd };
    copy = BuildTunicCopy(shoulder.data(), shoulder.size(), { 1, 4 }, kEnd);
    CHECK(copy.has_value());
    CHECK(Same((*copy)[1], kEnd));
    CHECK(Same((*copy)[4], kEnd));
    CHECK(Same((*copy)[2], shoulder[2]));
    CHECK(Same((*copy)[5], shoulder[5]));
}

TEST_CASE(TunicCopyAcceptsTwoShipsCosmeticCall) {
    // 2Ship's own tunic color replaces the color with a call to its little list: still a tunic place
    std::vector<TunicWord> list = Waist();
    list[2] = Call(0x1234560);
    std::optional<std::vector<TunicWord>> copy = BuildTunicCopy(list.data(), list.size(), { 2 }, kEnd);
    CHECK(copy.has_value());
    CHECK(Same((*copy)[2], kEnd));
}

TEST_CASE(TunicCopyRefusesPlacesThatAreNotTheColor) {
    std::vector<TunicWord> list = Waist();
    // Not a color nor a call (a custom model, another version of the asset)
    CHECK(!BuildTunicCopy(list.data(), list.size(), { 1 }, kEnd).has_value());
    // Out of the list, or the list's own end (nothing would follow the color)
    CHECK(!BuildTunicCopy(list.data(), list.size(), { 6 }, kEnd).has_value());
    CHECK(!BuildTunicCopy(list.data(), list.size(), { 5 }, kEnd).has_value());
    // Places must go forward, each once
    std::vector<TunicWord> two = { Prim(0, 0, 1), Other(1), Prim(0, 0, 1), kEnd };
    CHECK(BuildTunicCopy(two.data(), two.size(), { 0, 2 }, kEnd).has_value());
    CHECK(!BuildTunicCopy(two.data(), two.size(), { 2, 0 }, kEnd).has_value());
    CHECK(!BuildTunicCopy(two.data(), two.size(), { 0, 0 }, kEnd).has_value());
    // No places, or no list
    CHECK(!BuildTunicCopy(list.data(), list.size(), {}, kEnd).has_value());
    CHECK(!BuildTunicCopy(nullptr, 0, { 0 }, kEnd).has_value());
}

TEST_CASE(TunicColorKeepsTheOriginalsLodAndAlpha) {
    TunicWord original = Prim(3, 0x80, 0x1E691B7F);
    TunicWord c = TunicPrimColor(original, Rgb{ 200, 30, 40 });
    CHECK_EQ(c.w0, original.w0); // same command, minimum level and LOD fraction
    CHECK_EQ(c.w1, (uint64_t)0xC81E287Full);
    CHECK(TunicIsPrimColor(c));
    CHECK(!TunicIsPrimColor(Call(5)));
    CHECK(TunicIsCall(Call(5)));
    CHECK(!TunicIsCall(original));
}

TEST_CASE(TunicColorFromSomethingElseIsAPlainColor) {
    // When the original is not a color (2Ship's call we could not read), a full color: LOD fraction 0, opaque
    TunicWord c = TunicPrimColor(Call(5), Rgb{ 1, 2, 3 });
    CHECK(TunicIsPrimColor(c));
    CHECK_EQ(c.w0, (uint64_t)(0xFAull << 24));
    CHECK_EQ(c.w1, (uint64_t)0x010203FFull);
}
