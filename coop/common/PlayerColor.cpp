#include "PlayerColor.h"

#include <algorithm>
#include <cmath>

namespace coop {

namespace {

constexpr int kSecondShift = 24;
constexpr int kFxShift = 48;
constexpr int kSpeedShift = 51;
constexpr int64_t kRgbMask = 0xFFFFFF;
constexpr int64_t kKnownBits = (int64_t(1) << 53) - 1; // up to the speed

constexpr double kPi = 3.14159265358979323846;

// Seconds of one cycle of an effect at each speed (Slow, Normal, Fast).
constexpr double kCycleSeconds[] = { 3.2, 1.6, 0.8 };

uint8_t Lerp8(uint8_t a, uint8_t b, double t) {
    return (uint8_t)std::lround(a + (b - a) * t);
}

Rgb Mix(const Rgb& a, const Rgb& b, double t) {
    return { Lerp8(a.r, b.r, t), Lerp8(a.g, b.g, t), Lerp8(a.b, b.b, t) };
}

// 0 at the start of each cycle, 1 halfway, back to 0: a smooth wave.
double Wave(double seconds, double cycle) {
    return 0.5 - 0.5 * std::cos(2.0 * kPi * seconds / cycle);
}

} // namespace

uint32_t RgbToInt(const Rgb& c) {
    return ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b;
}

Rgb RgbFromInt(uint32_t value) {
    return { (uint8_t)((value >> 16) & 0xFF), (uint8_t)((value >> 8) & 0xFF), (uint8_t)(value & 0xFF) };
}

int64_t PackPlayerColor(const PlayerColor& color) {
    return (int64_t)RgbToInt(color.main) | ((int64_t)RgbToInt(color.second) << kSecondShift) |
           ((int64_t)color.fx << kFxShift) | ((int64_t)color.speed << kSpeedShift);
}

bool IsValidPackedColor(int64_t packed) {
    if (packed == kNoColor) {
        return true;
    }
    PlayerColor unused;
    return UnpackPlayerColor(packed, unused);
}

bool UnpackPlayerColor(int64_t packed, PlayerColor& out) {
    if (packed < 0 || (packed & ~kKnownBits) != 0) {
        return false;
    }
    int fx = (int)((packed >> kFxShift) & 0x7);
    int speed = (int)((packed >> kSpeedShift) & 0x3);
    if (fx >= (int)ColorFx::Count || speed >= (int)ColorSpeed::Count) {
        return false;
    }
    out.main = RgbFromInt((uint32_t)(packed & kRgbMask));
    out.second = RgbFromInt((uint32_t)((packed >> kSecondShift) & kRgbMask));
    out.fx = (ColorFx)fx;
    out.speed = (ColorSpeed)speed;
    return true;
}

const std::vector<Rgb>& PlayerColorPalette() {
    // The first eight are the defaults by id. None of them is the yellow-green of our own arrow on the minimap
    // (200, 255, 0), so another player's arrow never looks like ours.
    static const std::vector<Rgb> palette = {
        { 70, 150, 255 },  { 255, 80, 80 },   { 255, 170, 40 },  { 190, 110, 255 }, // blue, red, orange, purple
        { 40, 230, 220 },  { 255, 110, 200 }, { 255, 255, 255 }, { 120, 255, 120 }, // cyan, pink, white, green
        { 255, 230, 60 },  { 0, 90, 255 },    { 200, 20, 40 },   { 255, 120, 0 },   // gold, deep blue, crimson, tangerine
        { 120, 40, 200 },  { 0, 170, 140 },   { 255, 170, 190 }, { 160, 160, 170 }, // violet, teal, rose, silver
        { 150, 90, 40 },   { 90, 200, 60 },   { 30, 30, 40 },    { 255, 0, 255 },   // brown, grass, black, magenta
        { 140, 220, 255 }, { 255, 210, 150 }, { 80, 120, 40 },   { 255, 60, 0 },    // sky, peach, olive, fire
    };
    return palette;
}

PlayerColor DefaultPlayerColor(uint8_t playerId) {
    PlayerColor color;
    color.main = PlayerColorPalette()[(size_t)((playerId + 7) % 8)]; // id 1 = the first swatch
    return color;
}

PlayerColor ResolvePlayerColor(int64_t packed, uint8_t playerId) {
    PlayerColor color;
    if (UnpackPlayerColor(packed, color)) {
        return color;
    }
    return DefaultPlayerColor(playerId);
}

Rgb HsvToRgb(float h, float s, float v) {
    h = h - std::floor(h);
    s = std::clamp(s, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);
    float h6 = h * 6.0f;
    int sector = (int)h6 % 6;
    float f = h6 - std::floor(h6);
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));
    float r, g, b;
    switch (sector) {
        case 0:
            r = v, g = t, b = p;
            break;
        case 1:
            r = q, g = v, b = p;
            break;
        case 2:
            r = p, g = v, b = t;
            break;
        case 3:
            r = p, g = q, b = v;
            break;
        case 4:
            r = t, g = p, b = v;
            break;
        default:
            r = v, g = p, b = q;
            break;
    }
    return { (uint8_t)std::lround(r * 255.0f), (uint8_t)std::lround(g * 255.0f), (uint8_t)std::lround(b * 255.0f) };
}

void RgbToHsv(const Rgb& c, float& h, float& s, float& v) {
    float r = c.r / 255.0f, g = c.g / 255.0f, b = c.b / 255.0f;
    float maxC = std::max({ r, g, b });
    float minC = std::min({ r, g, b });
    float delta = maxC - minC;
    v = maxC;
    s = maxC > 0.0f ? delta / maxC : 0.0f;
    if (delta <= 0.0f) {
        h = 0.0f;
        return;
    }
    if (maxC == r) {
        h = (g - b) / delta;
    } else if (maxC == g) {
        h = 2.0f + (b - r) / delta;
    } else {
        h = 4.0f + (r - g) / delta;
    }
    h /= 6.0f;
    if (h < 0.0f) {
        h += 1.0f;
    }
}

Rgb ShownPlayerColor(const PlayerColor& color, double seconds) {
    int speed = std::clamp((int)color.speed, 0, (int)ColorSpeed::Count - 1);
    double cycle = kCycleSeconds[speed];
    if (seconds < 0.0) {
        seconds = 0.0;
    }
    switch (color.fx) {
        case ColorFx::Pulse: {
            Rgb dark = Mix(color.main, Rgb{ 0, 0, 0 }, 0.6);
            return Mix(color.main, dark, Wave(seconds, cycle));
        }
        case ColorFx::Blend:
            return Mix(color.main, color.second, Wave(seconds, cycle));
        case ColorFx::Rainbow: {
            float h, s, v;
            RgbToHsv(color.main, h, s, v);
            // a gray or a black would stay colorless: every rainbow is bright enough to be seen on the map
            double turn = std::fmod(seconds / (cycle * 2.5), 1.0);
            return HsvToRgb(h + (float)turn, std::max(s, 0.65f), std::max(v, 0.85f));
        }
        case ColorFx::Blink:
            return std::fmod(seconds, cycle) < cycle * 0.5 ? color.main : color.second;
        case ColorFx::Solid:
        default:
            return color.main;
    }
}

} // namespace coop
