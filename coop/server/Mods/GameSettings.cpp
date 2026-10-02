#include "GameSettings.h"

#include "common/I18n.h"
#include "common/ModRules.h"
#include "common/Protocol.h"
#include "common/Text.h"

namespace coop::server {

bool GameSettings::Set(uint8_t player, const std::string& name, const json& given, std::string* err) {
    if (!mods::SettingAllowed(name)) {
        *err = Tr(Msg::ApiBadSetting, { SanitizeChat(name, kMaxModSettingName) });
        return false;
    }
    json value = given.is_boolean() ? json(given.get<bool>() ? 1 : 0) : given;
    if (!mods::SettingValueAllowed(value)) {
        *err = Tr(Msg::ApiBadSettingValue, { name });
        return false;
    }
    // A new name makes the map of whoever gets it one longer: nobody's may outgrow what a game accepts.
    bool full = false;
    if (player != 0) {
        json mine = For(player);
        full = !mine.contains(name) && mine.size() >= (size_t)kMaxModSettings;
    } else if (!mEveryone.contains(name)) {
        full = mEveryone.size() >= (size_t)kMaxModSettings;
        for (const auto& [id, own] : mOwn) {
            full = full || (!own.contains(name) && For(id).size() >= (size_t)kMaxModSettings);
        }
    }
    if (full) {
        *err = Tr(Msg::ApiTooManySettings, { std::to_string(kMaxModSettings) });
        return false;
    }
    if (player == 0) {
        mEveryone[name] = value;
    } else {
        mOwn[player][name] = value;
    }
    return true;
}

bool GameSettings::Clear(uint8_t player, const std::string& name) {
    if (player == 0) {
        return mEveryone.erase(name) > 0;
    }
    auto it = mOwn.find(player);
    if (it == mOwn.end() || it->second.erase(name) == 0) {
        return false;
    }
    if (it->second.empty()) {
        mOwn.erase(it);
    }
    return true;
}

json GameSettings::For(uint8_t player) const {
    json out = mEveryone;
    auto it = mOwn.find(player);
    if (it != mOwn.end()) {
        out.update(it->second);
    }
    return out;
}

void GameSettings::Forget(uint8_t player) {
    mOwn.erase(player);
}

} // namespace coop::server
