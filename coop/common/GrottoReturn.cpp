#include "GrottoReturn.h"

#include "PlayerState.h"

#include <cmath>

namespace coop {

namespace {

bool InRange(const json& j, const char* key, int64_t lo, int64_t hi, int64_t& out) {
    auto it = j.find(key);
    if (it == j.end() || !it->is_number_integer()) {
        return false;
    }
    out = it->get<int64_t>();
    return out >= lo && out <= hi;
}

} // namespace

bool GrottoReturnFromJson(const json& j, GrottoReturn& out) {
    if (!j.is_object()) {
        return false;
    }
    int64_t entrance = 0;
    int64_t room = 0;
    int64_t yaw = 0;
    int64_t params = 0;
    int64_t data = 0;
    if (!InRange(j, "entrance", 0, 0xFFFF, entrance) || (entrance >> 9) >= pose_limits::kEntranceScenes ||
        !InRange(j, "room", 0, 127, room) || !InRange(j, "yaw", INT16_MIN, INT16_MAX, yaw) ||
        !InRange(j, "params", 0, 0xFFFF, params) || !InRange(j, "data", INT8_MIN, INT8_MAX, data)) {
        return false;
    }
    auto pos = j.find("pos");
    if (pos == j.end() || !pos->is_array() || pos->size() != 3) {
        return false;
    }
    GrottoReturn g;
    for (size_t i = 0; i < 3; i++) {
        const json& v = (*pos)[i];
        if (!v.is_number() || !std::isfinite(v.get<double>()) ||
            std::fabs(v.get<double>()) > pose_limits::kWorldLimit) {
            return false;
        }
        g.pos[i] = (float)v.get<double>();
    }
    g.entrance = (uint16_t)entrance;
    g.room = (int8_t)room;
    g.yaw = (int16_t)yaw;
    g.playerParams = (uint16_t)params;
    g.data = (int8_t)data;
    out = g;
    return true;
}

json GrottoReturnToJson(const GrottoReturn& g) {
    return json{ { "entrance", g.entrance },
                 { "room", g.room },
                 { "pos", json::array({ g.pos[0], g.pos[1], g.pos[2] }) },
                 { "yaw", g.yaw },
                 { "params", g.playerParams },
                 { "data", g.data } };
}

} // namespace coop
