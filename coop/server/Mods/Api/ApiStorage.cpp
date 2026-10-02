// Mod API: storage.* (what a mod keeps between restarts: one JSON file per mod in the data folder).
#include "server/Mods/ModApi.h"
#include "server/Mods/ModHost.h"
#include "server/Mods/ModStorage.h"

namespace coop::server {

namespace {

constexpr size_t kMaxKeyBytes = 200;

json StorageGet(ApiCall& call) {
    json value = call.host.Storage(call.mod).Get(call.Str(0, "key", kMaxKeyBytes));
    return value.is_null() ? call.At(1) : value;
}

json StorageSet(ApiCall& call) {
    call.host.Storage(call.mod).Set(call.Str(0, "key", kMaxKeyBytes), call.At(1));
    return nullptr;
}

json StorageRemove(ApiCall& call) {
    return call.host.Storage(call.mod).Remove(call.Str(0, "key", kMaxKeyBytes));
}

json StorageKeys(ApiCall& call) {
    return call.host.Storage(call.mod).Keys();
}

json StorageSave(ApiCall& call) {
    std::string err;
    if (!call.host.Storage(call.mod).Save(&err)) {
        call.Fail(err);
    }
    return nullptr;
}

} // namespace

COOP_MOD_API(storageGet, "storage.get", "key, default?", "value",
             "What this mod saved under that key, or `default` if there is nothing. Each mod's data is its own: "
             "another mod does not see it.",
             StorageGet);
COOP_MOD_API(storageSet, "storage.set", "key, value", "nothing",
             "Saves a value (number, text, boolean or table) that will still be there when the server restarts. "
             "`nil` deletes it. It is written to `<dataDir>/<mod>.json` at most every 2 seconds and on stop.",
             StorageSet);
COOP_MOD_API(storageRemove, "storage.remove", "key", "boolean", "Deletes a key. Returns whether it existed.",
             StorageRemove);
COOP_MOD_API(storageKeys, "storage.keys", "", "list", "The keys this mod has saved, in alphabetical order.",
             StorageKeys);
COOP_MOD_API(storageSave, "storage.save", "", "nothing",
             "Writes the data to disk right now (normally not needed).", StorageSave);

} // namespace coop::server
