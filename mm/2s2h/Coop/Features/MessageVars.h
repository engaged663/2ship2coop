#pragma once
// [COOP] The values a mirrored text shows (spec 2026-09-30-coop-grupos-limites §7): the talker's, not ours. TalkSync.cpp
// sends ours with "talk open/id" and keeps the talker's; Coop_OnMessageDecode (z_message.c) puts them in place while
// the mirrored text is decoded and gives ours back right after.
#include <string>

namespace coop::client {

std::string MessageVars_Capture();            // this game's values now, as hex
bool MessageVars_Set(const std::string& hex); // the talker's (false: empty or not the right size: ours are used)
void MessageVars_Clear();

} // namespace coop::client
