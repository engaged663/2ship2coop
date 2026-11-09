#pragma once
// [COOP] Activity trips ("follow" with an activity's key): who a trip into or out of a minigame's entrance takes along.
// The game asks these (mm/2s2h/Coop/Activities/Director.cpp and Follow.cpp); tested in coop/tests/TestActivities.cpp.

namespace coop {

// A trip of ours takes the others along only when it is our own: never one we make because another game's "follow"
// took us (passed on, it went back to that game and both reloaded the entrance forever: the Goron race's countdown
// never ended), nor one of an activity we play as another game's guest (its director leads the trips; our false start
// or retry is ours alone).
bool ActivityTrip_TakesOthers(bool followed, bool guest);

// Another game's trip into an activity takes us along unless we run that activity ourselves (we direct its round, or
// it is our own run): that game's trip (its false start, its retry) is not ours. A trip of the whole room ("all": the
// round starting, the end of the Bombers' hide-and-seek) takes everyone in it.
bool ActivityTrip_Follows(bool weRunIt, bool wholeRoom);

} // namespace coop
