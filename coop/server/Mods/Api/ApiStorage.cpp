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

COOP_MOD_API(storageGet, "storage.get", "key, default?", "valor",
             "Lo que este mod guardó con esa clave, o `default` si no hay nada. Los datos de cada mod son suyos: "
             "otro mod no los ve.",
             StorageGet);
COOP_MOD_API(storageSet, "storage.set", "key, value", "nada",
             "Guarda un valor (número, texto, booleano o tabla) que seguirá ahí cuando el servidor se reinicie. "
             "`nil` lo borra. Se escribe en `<dataDir>/<mod>.json` como mucho cada 2 segundos y al parar.",
             StorageSet);
COOP_MOD_API(storageRemove, "storage.remove", "key", "booleano", "Borra una clave. Devuelve si existía.",
             StorageRemove);
COOP_MOD_API(storageKeys, "storage.keys", "", "lista", "Las claves que este mod tiene guardadas, en orden alfabético.",
             StorageKeys);
COOP_MOD_API(storageSave, "storage.save", "", "nada",
             "Escribe ahora mismo los datos en el disco (normalmente no hace falta).", StorageSave);

} // namespace coop::server
