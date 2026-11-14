#pragma once
// [COOP] Each player's color: their nick above their Link, their arrow on the other players' minimap, their nick in
// the chat and in the player list. The player picks it in the Co-op menu ("Mi color y el minimapa"); who never picked one
// gets a color of the palette by their id (DefaultPlayerColor), so 4 players are told apart without touching anything.
//
// On the wire it is ONE integer (hello "color", welcome players[].color, join "color", the "color" event):
//   bits  0..23  main color 0xRRGGBB
//   bits 24..47  second color 0xRRGGBB (only the "two colors" and "blink" effects use it)
//   bits 48..50  effect (ColorFx)
//   bits 51..52  speed of the effect (ColorSpeed)
//   (under 2^53: exact in a double, so a Lua mod or a JSON tool reads it right)
//   -1 (kNoColor) = never picked: every game shows DefaultPlayerColor(id).
// Anything else (another bit set, an unknown effect or speed) is not a color: UnpackPlayerColor returns false.
// Tested in coop/tests/TestPlayerColor.cpp. The game: mm/2s2h/Coop/Features/PlayerColors.cpp.
#include <cstdint>
#include <vector>

namespace coop {

struct Rgb {
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t b = 255;
    bool operator==(const Rgb& o) const {
        return r == o.r && g == o.g && b == o.b;
    }
    bool operator!=(const Rgb& o) const {
        return !(*this == o);
    }
};

enum class ColorFx : uint8_t {
    Solid = 0,   // the main color
    Pulse = 1,   // the main color, getting darker and back
    Blend = 2,   // goes smoothly from the main color to the second one and back
    Rainbow = 3, // every hue, starting at the main color's
    Blink = 4,   // jumps between the main color and the second one
    Count
};

enum class ColorSpeed : uint8_t { Slow = 0, Normal = 1, Fast = 2, Count };

struct PlayerColor {
    Rgb main;
    Rgb second{ 0, 0, 0 };
    ColorFx fx = ColorFx::Solid;
    ColorSpeed speed = ColorSpeed::Normal;
    bool operator==(const PlayerColor& o) const {
        return main == o.main && second == o.second && fx == o.fx && speed == o.speed;
    }
    bool operator!=(const PlayerColor& o) const {
        return !(*this == o);
    }
};

constexpr int64_t kNoColor = -1;

int64_t PackPlayerColor(const PlayerColor& color);
bool UnpackPlayerColor(int64_t packed, PlayerColor& out); // false: kNoColor or not a color (out untouched)
bool IsValidPackedColor(int64_t packed);                  // kNoColor counts as valid

uint32_t RgbToInt(const Rgb& c); // 0xRRGGBB
Rgb RgbFromInt(uint32_t value);  // the low 24 bits

// The color of a player who never picked one (ids 1..8: players and the server's own games; any id works).
PlayerColor DefaultPlayerColor(uint8_t playerId);
// The color a player shows: theirs, or the default of their id when packed is kNoColor or not a color.
PlayerColor ResolvePlayerColor(int64_t packed, uint8_t playerId);

// The swatches of the menu (the defaults first).
const std::vector<Rgb>& PlayerColorPalette();

// What is drawn at `seconds` (any monotonic clock): the effect applied. Solid ignores the time.
Rgb ShownPlayerColor(const PlayerColor& color, double seconds);

// Helpers (h in [0, 1), s and v in [0, 1]).
Rgb HsvToRgb(float h, float s, float v);
void RgbToHsv(const Rgb& c, float& h, float& s, float& v);

} // namespace coop
