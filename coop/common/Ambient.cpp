#include "Ambient.h"

namespace coop::ambient {

namespace {

bool Int(const json& v, int64_t& out) {
    if (!v.is_number_integer()) {
        return false;
    }
    if (v.is_number_unsigned()) {
        uint64_t x = v.get<uint64_t>();
        if (x > 0xFFFFFFFFull) {
            return false;
        }
        out = (int64_t)x;
        return true;
    }
    out = v.get<int64_t>();
    return true;
}

bool S16(const json& v, int16_t& out) {
    int64_t x = 0;
    if (!Int(v, x) || x < -32768 || x > 32767) {
        return false;
    }
    out = (int16_t)x;
    return true;
}

} // namespace

bool SeqCmdAllowed(uint32_t cmd) {
    uint32_t op = cmd >> 28;
    uint32_t player = (cmd >> 24) & 0xF;
    return op <= 0xD && (player == 0 || player == 1 || player == 3 || player == 4);
}

bool MusicFromJson(const json& ev, std::vector<Music>& out) {
    out.clear();
    auto it = ev.find("music");
    if (it == ev.end()) {
        return true;
    }
    if (!it->is_array() || it->size() > kMaxMusic) {
        return false;
    }
    for (const json& m : *it) {
        int64_t kind = 0;
        int64_t value = 0;
        if (!m.is_array() || m.size() != 2 || !Int(m[0], kind) || !Int(m[1], value) || kind < 0 ||
            kind >= kMusicKinds || value < 0 || value > 0xFFFFFFFFll) {
            out.clear();
            return false;
        }
        Music music{ (MusicKind)kind, (uint32_t)value };
        bool ok = true;
        switch (music.kind) {
            case MusicKind::Cmd:
                ok = SeqCmdAllowed(music.value);
                break;
            case MusicKind::StorePrevBgm:
            case MusicKind::PlaySubBgm:
            case MusicKind::Fanfare:
                ok = music.value <= 0xFFFF;
                break;
            default:
                ok = music.value == 0;
                break;
        }
        if (!ok) {
            out.clear();
            return false;
        }
        out.push_back(music);
    }
    return true;
}

bool QuakesFromJson(const json& ev, std::vector<Quake>& out) {
    out.clear();
    auto it = ev.find("quake");
    if (it == ev.end()) {
        return true;
    }
    if (!it->is_array() || it->size() > kMaxQuakes) {
        return false;
    }
    for (const json& q : *it) {
        Quake quake;
        if (!q.is_array() || q.size() != 7 || !S16(q[0], quake.type) || !S16(q[1], quake.speed) ||
            !S16(q[2], quake.y) || !S16(q[3], quake.x) || !S16(q[4], quake.fov) || !S16(q[5], quake.roll) ||
            !S16(q[6], quake.duration) || quake.type < 1 || quake.type >= kQuakeTypes || quake.duration < 1 ||
            quake.duration > kMaxQuakeDuration) {
            out.clear();
            return false;
        }
        out.push_back(quake);
    }
    return true;
}

json MusicToJson(const std::vector<Music>& music) {
    json list = json::array();
    for (const Music& m : music) {
        list.push_back(json::array({ (int)m.kind, m.value }));
    }
    return list;
}

json QuakesToJson(const std::vector<Quake>& quakes) {
    json list = json::array();
    for (const Quake& q : quakes) {
        list.push_back(json::array({ q.type, q.speed, q.y, q.x, q.fov, q.roll, q.duration }));
    }
    return list;
}

} // namespace coop::ambient
