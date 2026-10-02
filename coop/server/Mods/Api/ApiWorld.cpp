// Mod API: world.* (the server's shared world: its 3-day clock, its cycles and the fields of common/WorldFields.h).
#include "server/Mods/ModApi.h"
#include "server/Mods/ModHost.h"
#include "server/Server.h"

#include "common/Clock.h"
#include "common/Text.h"
#include "common/WorldFields.h"
#include "common/WorldOps.h"

#include <cstdio>

namespace coop::server {

namespace {

using world::FieldDef;
using world::Kind;

const char* KindName(Kind kind) {
    if (kind == Kind::Bits) {
        return "bits";
    }
    return kind == Kind::Bytes ? "bytes" : "counter";
}

SharedWorld& World(ApiCall& call) {
    SharedWorld& world = call.server.World();
    if (!world.Exists()) {
        call.Fail(Tr(Msg::NoWorldYet));
    }
    return world;
}

// A field (by name or index) and a byte of it: arguments 0 and 1 of the functions below.
struct Place {
    uint16_t field;
    uint16_t offset;
    const FieldDef* def;
};

Place PlaceOf(ApiCall& call) {
    int index = -1;
    const json& which = call.At(0);
    if (which.is_string()) {
        index = world::FindField(which.get<std::string>().c_str());
    } else if (which.is_number()) {
        index = (int)call.Int(0, "field", 0, (int64_t)world::kFieldCount - 1);
    }
    if (index < 0) {
        call.Fail(Tr(Msg::ApiBadField, { which.is_string() ? SanitizeChat(which.get<std::string>(), 40) : which.dump() }));
    }
    const FieldDef& def = world::kFields[index];
    uint16_t offset = (uint16_t)call.Int(1, "offset", 0, def.size - 1);
    int width = world::CounterWidth(def.kind);
    if (width > 1 && (offset % width != 0 || offset + width > def.size)) {
        call.Fail(Tr(Msg::ApiFieldAlign));
    }
    return { (uint16_t)index, offset, &def };
}

void Need(ApiCall& call, const Place& place, bool ok) {
    if (!ok) {
        call.Fail(Tr(Msg::ApiFieldKind, { place.def->name, KindName(place.def->kind) }));
    }
}

int32_t ValueAt(SharedWorld& world, const Place& place) {
    const std::vector<uint8_t>& bytes = world.Store().Fields()[place.field];
    return world::IsCounter(place.def->kind) ? world::ReadCounter(place.def->kind, bytes.data() + place.offset)
                                             : bytes[place.offset];
}

// Applies one change of a mod; what really changed comes back.
world::Ops Apply(ApiCall& call, SharedWorld& world, const world::Ops& ops) {
    world::Ops applied;
    std::string err;
    if (!world.ApplyServerOps(ops, &applied, &err)) {
        call.Fail(err);
    }
    return applied;
}

json WorldExists(ApiCall& call) {
    return call.server.World().Exists();
}

json WorldCycle(ApiCall& call) {
    return call.server.World().Exists() ? call.server.World().Cycle() : 0;
}

json WorldTime(ApiCall& call) {
    SharedWorld& world = call.server.World();
    if (!world.Exists()) {
        return nullptr;
    }
    uint32_t abs = world.ClockAbs();
    uint16_t time = clock::TimeOfAbs(abs);
    int minutes = (int)((time * 1440u + 0x8000u) / 0x10000u) % 1440; // as the game's clock shows it
    return { { "abs", abs },
             { "day", clock::DayOfAbs(abs) },
             { "hour", minutes / 60 },
             { "minute", minutes % 60 },
             { "time", time },
             { "night", clock::IsNight(time) },
             { "stopped", world.ClockStopped() },
             { "inverted", world.Inverted() },
             { "speed", world.Speed() } };
}

json WorldSetTime(ApiCall& call) {
    SharedWorld& world = World(call);
    int day = (int)call.Int(0, "day", 1, 3);
    char hhmm[16];
    std::snprintf(hhmm, sizeof(hhmm), "%02d:%02d", (int)call.Int(1, "hour", 0, 23),
                  (int)call.IntOr(2, "minute", 0, 0, 59));
    uint32_t abs = 0;
    std::string err;
    if (!clock::Parse(day, hhmm, abs) || !world.SetTime(abs, call.mod.Info().name, &err)) {
        call.Fail(err);
    }
    return nullptr;
}

json WorldSetStopped(ApiCall& call) {
    SharedWorld& world = World(call);
    bool stopped = call.Bool(0, "stopped");
    std::string err;
    if (stopped != world.Frozen() && !world.SetClockStopped(stopped, call.mod.Info().name, &err)) {
        call.Fail(err);
    }
    return nullptr;
}

json WorldSetSpeed(ApiCall& call) {
    double speed = call.Number(0, "speed", kMinTimeSpeed, kMaxTimeSpeed);
    call.server.EditConfig().timeSpeed = speed;
    call.server.World().SetSpeed(speed, call.mod.Info().name);
    return nullptr;
}

json WorldRestart(ApiCall& call) {
    std::string err;
    if (!World(call).Restart(call.mod.Info().name, &err)) {
        call.Fail(err);
    }
    return nullptr;
}

json WorldCrashMoon(ApiCall& call) {
    World(call).CrashMoon();
    return nullptr;
}

json WorldFields(ApiCall&) {
    json out = json::array();
    for (const FieldDef& def : world::kFields) {
        out.push_back({ { "name", def.name }, { "size", def.size }, { "kind", KindName(def.kind) } });
    }
    return out;
}

json WorldGet(ApiCall& call) {
    SharedWorld& world = World(call);
    return ValueAt(world, PlaceOf(call));
}

json WorldGetBit(ApiCall& call) {
    SharedWorld& world = World(call);
    Place place = PlaceOf(call);
    Need(call, place, !world::IsCounter(place.def->kind));
    int64_t mask = call.Int(2, "mask", 1, 0xFF);
    return (ValueAt(world, place) & mask) == mask;
}

json WorldSet(ApiCall& call) {
    SharedWorld& world = World(call);
    Place place = PlaceOf(call);
    Need(call, place, place.def->kind != Kind::Bits); // bits change one by one: setBits
    world::Ops ops;
    if (world::IsCounter(place.def->kind)) {
        int min = 0;
        int max = 0;
        world::CounterRange(place.def->kind, min, max);
        int32_t delta = (int32_t)call.Int(2, "value", min, max) - ValueAt(world, place);
        if (delta == 0) {
            return false;
        }
        ops.adds.push_back({ place.field, place.offset, delta });
    } else {
        ops.bytes.push_back({ place.field, place.offset, (uint8_t)call.Int(2, "value", 0, 0xFF) });
    }
    return !Apply(call, world, ops).Empty();
}

json WorldSetBits(ApiCall& call) {
    SharedWorld& world = World(call);
    Place place = PlaceOf(call);
    Need(call, place, place.def->kind == Kind::Bits);
    world::Ops ops;
    ops.bits.push_back({ place.field, place.offset, (uint8_t)call.Int(2, "set", 0, 0xFF),
                         (uint8_t)call.IntOr(3, "clear", 0, 0, 0xFF) });
    return !Apply(call, world, ops).Empty();
}

json WorldAdd(ApiCall& call) {
    SharedWorld& world = World(call);
    Place place = PlaceOf(call);
    Need(call, place, world::IsCounter(place.def->kind));
    world::Ops ops;
    ops.adds.push_back({ place.field, place.offset, (int32_t)call.Int(2, "delta", -0xFFFF, 0xFFFF) });
    world::Ops applied = Apply(call, world, ops);
    return applied.adds.empty() ? 0 : applied.adds[0].delta;
}

} // namespace

COOP_MOD_API(worldExists, "world.exists", "", "booleano",
             "Si el mundo del servidor ya existe (lo crea el primer jugador que entra en la partida).", WorldExists);
COOP_MOD_API(worldCycle, "world.cycle", "", "número",
             "El número del ciclo de tres días en curso (1 el primero; 0 si todavía no hay mundo).", WorldCycle);
COOP_MOD_API(worldTime, "world.time", "", "tabla o nil",
             "El reloj del mundo: `day` (1-3), `hour`, `minute`, `night`, `stopped`, `inverted` (Canción del Tiempo "
             "Invertida), `speed` (velocidad del tiempo), `abs` (unidades desde el día 1 a las 6:00; un día son "
             "65536) y `time` (la hora como la guarda el juego). `nil` si no hay mundo.",
             WorldTime);
COOP_MOD_API(worldSetTime, "world.setTime", "day, hour, minute?", "nada",
             "Salta a ese momento para todos (día 1-3, hora 0-23). Los juegos recargan su escena, como con `/settime`.",
             WorldSetTime);
COOP_MOD_API(worldSetStopped, "world.setStopped", "stopped", "nada",
             "Detiene (`true`) o reanuda (`false`) el reloj del mundo, como `/freezetime`. Se queda así aunque entren "
             "o salgan jugadores.",
             WorldSetStopped);
COOP_MOD_API(worldSetSpeed, "world.setSpeed", "speed", "nada",
             "Cambia la velocidad a la que pasan los tres días: 1 es la del juego original, 0.5 la mitad (días el "
             "doble de largos), 2 el doble. Entre 0.1 y 10.",
             WorldSetSpeed);
COOP_MOD_API(worldRestart, "world.restart", "", "nada",
             "Vuelve al Amanecer del Primer Día sin votación (como `/reiniciar`): cada jugador conserva lo que "
             "conserva la Canción del Tiempo.",
             WorldRestart);
COOP_MOD_API(worldCrashMoon, "world.crashMoon", "", "nada",
             "La luna cae ahora: el mundo vuelve al principio del ciclo, como si nadie hubiera tocado la Canción del "
             "Tiempo.",
             WorldCrashMoon);
COOP_MOD_API(worldFields, "world.fields", "", "lista",
             "Los campos del mundo compartido: `{ name, size (bytes), kind }`, con `kind` = `bits` (banderas), "
             "`bytes` (valores) o `counter` (contadores).",
             WorldFields);
COOP_MOD_API(worldGet, "world.get", "field, offset", "número",
             "Lee un byte de un campo del mundo (por nombre o por índice); en un campo `counter`, el contador que "
             "empieza en ese byte.",
             WorldGet);
COOP_MOD_API(worldGetBit, "world.getBit", "field, offset, mask", "booleano",
             "Si todos los bits de `mask` están puestos en ese byte. Ejemplo: `coop.world.getBit(\"owls\", 0, 0x01)`.",
             WorldGetBit);
COOP_MOD_API(worldSet, "world.set", "field, offset, value", "booleano",
             "Escribe un byte de un campo `bytes` (por ejemplo una máscara en `masks`) o fija un contador, para todos "
             "los jugadores. Devuelve si algo cambió.",
             WorldSet);
COOP_MOD_API(worldSetBits, "world.setBits", "field, offset, set, clear?", "booleano",
             "Pone los bits de `set` y quita los de `clear` en un byte de un campo `bits` (banderas de misiones, "
             "cofres, búhos...), para todos los jugadores. Devuelve si algo cambió.",
             WorldSetBits);
COOP_MOD_API(worldAdd, "world.add", "field, offset, delta", "número",
             "Suma `delta` (puede ser negativo) a un contador (`heartQuarters`, `keys`, `fairies`, `skulls`, "
             "`bottles`). Devuelve lo que se sumó de verdad (el contador no sale de su rango).",
             WorldAdd);

} // namespace coop::server
