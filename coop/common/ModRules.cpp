#include "ModRules.h"

#include "Protocol.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace coop::mods {

namespace {

bool StartsWith(const std::string& text, const char* prefix) {
    return text.rfind(prefix, 0) == 0;
}

} // namespace

bool SettingAllowed(const std::string& name) {
    if (name.size() < 3 || name.size() > (size_t)kMaxModSettingName) {
        return false;
    }
    for (unsigned char c : name) {
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.';
        if (!ok) {
            return false;
        }
    }
    bool gameplay = false;
    for (const char* prefix : { "gEnhancements.", "gCheats.", "gModes.", "gFixes." }) {
        if (StartsWith(name, prefix) && name.size() > std::char_traits<char>::length(prefix)) {
            gameplay = true;
        }
    }
    if (!gameplay) {
        return false;
    }
    // Each player's own: how they see, hear, save and control the game.
    for (const char* own : { "gEnhancements.A11y.", "gEnhancements.Camera.", "gEnhancements.Graphics.",
                             "gEnhancements.Saving.", "gEnhancements.Mods.", "gEnhancements.Playback.",
                             "gEnhancements.Dpad." }) {
        if (StartsWith(name, own)) {
            return false;
        }
    }
    // Erases the player's save file on death (and resets the game): never a server's to decide.
    return name != "gEnhancements.DifficultyOptions.DeleteFileOnDeath";
}

bool SettingValueAllowed(const json& value) {
    if (!value.is_number()) {
        return false;
    }
    double v = value.get<double>();
    return std::isfinite(v) && std::fabs(v) <= 1.0e6;
}

bool ItemGivable(int id) {
    // The game's Item_Give (z_parameter.c) has no case for SWORD_DEITY, WALLET_DEFAULT, FISHING_ROD, STRAY_FAIRIES and
    // INVALID_1..7: it would put them in the inventory slot gItemSlots[id], read past the end of that table (77
    // entries). The two unnamed ids 0x71 and 0x72 would set quest bits that are no song.
    static constexpr int kNoCase[] = { 0x50, 0x59, 0x5C, 0x71, 0x72, 0x77, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82 };
    if (id < 0 || id > kLastModItem) {
        return false;
    }
    return std::find(std::begin(kNoCase), std::end(kNoCase), id) == std::end(kNoCase);
}

} // namespace coop::mods
