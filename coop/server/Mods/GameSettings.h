#pragma once
// The game settings (2 Ship CVars) the server forces on the games playing in its world: the ones for everyone
// (server.json "gameSettings", game.setSetting("*", ...)) and, on top of them, each player's own. A game always
// gets its whole map ("mod_cfg"); what may be forced is in common/ModRules.h.
#include "common/Events.h"

#include <cstdint>
#include <map>
#include <string>

namespace coop::server {

class GameSettings {
  public:
    // player 0: everyone. A whole number forces an integer option, a number with decimals a decimal one; true and
    // false count as 1 and 0. False (with err) for a name or a value that may not be forced, or when a player would
    // end up with more than kMaxModSettings.
    bool Set(uint8_t player, const std::string& name, const json& value, std::string* err);
    bool Clear(uint8_t player, const std::string& name); // false: it was not set
    json For(uint8_t player) const; // everyone's settings and, on top, that player's own; For(0): everyone's
    void Forget(uint8_t player);    // it left: its own settings go with it

  private:
    json mEveryone = json::object();
    std::map<uint8_t, json> mOwn; // by player id; never an empty map
};

} // namespace coop::server
