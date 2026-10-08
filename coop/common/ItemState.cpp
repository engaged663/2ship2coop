// [COOP] See ItemState.h.
#include "ItemState.h"

#include "PlayerState.h"

#include <cmath>
#include <initializer_list>

namespace coop {

namespace {

bool Float(const json& ev, const char* key, float limit, float& out) {
    auto it = ev.find(key);
    if (it == ev.end() || !it->is_number()) {
        return false;
    }
    double v = it->get<double>();
    if (!std::isfinite(v) || std::fabs(v) > limit) {
        return false;
    }
    out = (float)v;
    return true;
}

bool Int(const json& ev, const char* key, int64_t min, int64_t max, int64_t& out) {
    auto it = ev.find(key);
    if (it == ev.end() || !it->is_number_integer()) {
        return false;
    }
    out = it->get<int64_t>();
    return out >= min && out <= max;
}

} // namespace

uint32_t MakeTwinItemKey(int16_t spawnerId, uint32_t spawnerListKey, uint32_t slot) {
    uint32_t h = 2166136261u;
    for (uint32_t v : { (uint32_t)(uint16_t)spawnerId, spawnerListKey, slot }) {
        for (int i = 0; i < 4; i++) {
            h = (h ^ ((v >> (8 * i)) & 0xFF)) * 16777619u;
        }
    }
    return h & ~(kUniqueItemKeyBit | kListItemKeyBit);
}

void WriteItemState(const ItemState& s, json& out) {
    out["key"] = s.key;
    out["id"] = s.id;
    out["params"] = s.params;
    out["pos"] = { s.pos[0], s.pos[1], s.pos[2] };
    if (s.id == kItemActorCollectible) {
        out["vy"] = s.vy;
        out["speed"] = s.speed;
        out["grav"] = s.grav;
        out["scale"] = s.scale;
        out["yaw"] = s.yaw;
        out["phase"] = s.phase;
        out["timer"] = s.timer;
        out["act"] = s.act;
    }
}

bool ReadItemState(const json& ev, ItemState& out) {
    ItemState s;
    int64_t key, id, params;
    if (!Int(ev, "key", 0, 0xFFFFFFFFll, key) || !Int(ev, "id", 0, 0x7FFF, id) ||
        !Int(ev, "params", 0, 0xFFFF, params)) {
        return false;
    }
    if (id != kItemActorCollectible && id != kItemActorFairy && id != kItemActorStrayFairy) {
        return false;
    }
    auto pos = ev.find("pos");
    if (pos == ev.end() || !pos->is_array() || pos->size() != 3) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        const json& v = (*pos)[i];
        if (!v.is_number() || !std::isfinite(v.get<double>()) ||
            std::fabs(v.get<double>()) > pose_limits::kWorldLimit) {
            return false;
        }
        s.pos[i] = (float)v.get<double>();
    }
    s.key = (uint32_t)key;
    s.id = (int16_t)id;
    s.params = (int32_t)params;
    if (s.id == kItemActorCollectible) {
        int64_t yaw, phase, timer, act;
        if (!Float(ev, "vy", item_limits::kMotion, s.vy) || !Float(ev, "speed", item_limits::kMotion, s.speed) ||
            !Float(ev, "grav", item_limits::kMotion, s.grav) || !Float(ev, "scale", item_limits::kScale, s.scale) ||
            s.scale < 0.f || !Int(ev, "yaw", -0x8000, 0x7FFF, yaw) || !Int(ev, "phase", -0x8000, 0x7FFF, phase) ||
            !Int(ev, "timer", -0x8000, 0x7FFF, timer) || !Int(ev, "act", 0, item_limits::kActions - 1, act)) {
            return false;
        }
        s.yaw = (int16_t)yaw;
        s.phase = (int16_t)phase;
        s.timer = (int16_t)timer;
        s.act = (uint8_t)act;
    }
    out = s;
    return true;
}

int64_t ItemLifeMs(const ItemState& s) {
    if (s.id == kItemActorFairy) {
        return item_limits::kFairyLifeMs;
    }
    if (s.id == kItemActorCollectible && s.timer > 0) {
        return (int64_t)s.timer * 50 + item_limits::kLifeGraceMs;
    }
    return 0;
}

} // namespace coop
