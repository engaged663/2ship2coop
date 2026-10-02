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

COOP_MOD_API(worldExists, "world.exists", "", "boolean",
             "Whether the server's world already exists (the first player to enter the game creates it).",
             WorldExists);
COOP_MOD_API(worldCycle, "world.cycle", "", "number",
             "The number of the current three-day cycle (1 the first; 0 if there is no world yet).", WorldCycle);
COOP_MOD_API(worldTime, "world.time", "", "table or nil",
             "The world clock: `day` (1-3), `hour`, `minute`, `night`, `stopped`, `inverted` (Inverted Song of "
             "Time), `speed` (time speed), `abs` (units since day 1 at 6:00; one day is 65536) and `time` (the time "
             "as the game stores it). `nil` if there is no world.",
             WorldTime);
COOP_MOD_API(worldSetTime, "world.setTime", "day, hour, minute?", "nothing",
             "Jumps everyone to that moment (day 1-3, hour 0-23). The games reload their scene, as with `/settime`.",
             WorldSetTime);
COOP_MOD_API(worldSetStopped, "world.setStopped", "stopped", "nothing",
             "Stops (`true`) or resumes (`false`) the world clock, like `/freezetime`. It stays that way even as "
             "players come and go.",
             WorldSetStopped);
COOP_MOD_API(worldSetSpeed, "world.setSpeed", "speed", "nothing",
             "Changes the speed at which the three days pass: 1 is the original game's, 0.5 half (days twice as "
             "long), 2 double. Between 0.1 and 10.",
             WorldSetSpeed);
COOP_MOD_API(worldRestart, "world.restart", "", "nothing",
             "Goes back to Dawn of the First Day without a vote (like `/reiniciar`): each player keeps what the Song "
             "of Time keeps.",
             WorldRestart);
COOP_MOD_API(worldCrashMoon, "world.crashMoon", "", "nothing",
             "The moon falls now: the world goes back to the start of the cycle, as if nobody had played the Song of "
             "Time.",
             WorldCrashMoon);
COOP_MOD_API(worldFields, "world.fields", "", "list",
             "The fields of the shared world: `{ name, size (bytes), kind }`, with `kind` = `bits` (flags), "
             "`bytes` (values) or `counter` (counters).",
             WorldFields);
COOP_MOD_API(worldGet, "world.get", "field, offset", "number",
             "Reads a byte of a world field (by name or index); in a `counter` field, the counter that starts at "
             "that byte.",
             WorldGet);
COOP_MOD_API(worldGetBit, "world.getBit", "field, offset, mask", "boolean",
             "Whether all the bits of `mask` are set in that byte. Example: `coop.world.getBit(\"owls\", 0, 0x01)`.",
             WorldGetBit);
COOP_MOD_API(worldSet, "world.set", "field, offset, value", "boolean",
             "Writes a byte of a `bytes` field (for example a mask in `masks`) or sets a counter, for all players. "
             "Returns whether anything changed.",
             WorldSet);
COOP_MOD_API(worldSetBits, "world.setBits", "field, offset, set, clear?", "boolean",
             "Sets the bits of `set` and clears those of `clear` in a byte of a `bits` field (quest flags, chests, "
             "owls...), for all players. Returns whether anything changed.",
             WorldSetBits);
COOP_MOD_API(worldAdd, "world.add", "field, offset, delta", "number",
             "Adds `delta` (may be negative) to a counter (`heartQuarters`, `keys`, `fairies`, `skulls`, "
             "`bottles`). Returns how much was actually added (the counter does not leave its range).",
             WorldAdd);

} // namespace coop::server
