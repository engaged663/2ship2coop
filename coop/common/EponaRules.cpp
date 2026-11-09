#include "EponaRules.h"

namespace coop {

bool EponaRules_MayMount(const void* currentRide, const void* horse) {
    return currentRide == nullptr || currentRide == horse;
}

EponaSong EponaRules_OnSong(bool ownHorseHere, bool restingHorseHere) {
    if (ownHorseHere) {
        return EponaSong::CallOurs;
    }
    return restingHorseHere ? EponaSong::Wake : EponaSong::Spawn;
}

} // namespace coop
