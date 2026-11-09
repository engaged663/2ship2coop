#include "ActivityTrips.h"

namespace coop {

bool ActivityTrip_TakesOthers(bool followed, bool guest) {
    return !followed && !guest;
}

bool ActivityTrip_Follows(bool weRunIt, bool wholeRoom) {
    return wholeRoom || !weRunIt;
}

} // namespace coop
