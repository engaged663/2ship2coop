#include "SceneFlags.h"

namespace coop::scene_flags {

namespace {

bool U32(const json& v, uint32_t& out) {
    if (!v.is_number_integer()) {
        return false;
    }
    if (v.is_number_unsigned()) {
        uint64_t x = v.get<uint64_t>();
        if (x > 0xFFFFFFFFull) {
            return false;
        }
        out = (uint32_t)x;
        return true;
    }
    int64_t x = v.get<int64_t>();
    if (x < 0 || x > 0xFFFFFFFFll) {
        return false;
    }
    out = (uint32_t)x;
    return true;
}

} // namespace

std::vector<Op> Diff(const Words& from, const Words& to) {
    std::vector<Op> ops;
    for (uint8_t w = 0; w < kWordCount; w++) {
        if (from[w] != to[w]) {
            ops.push_back({ w, to[w] & ~from[w], from[w] & ~to[w] });
        }
    }
    return ops;
}

void Apply(Words& words, const Op& op) {
    if (op.word < kWordCount) {
        words[op.word] = (words[op.word] | op.set) & ~op.clear;
    }
}

bool OpsFromJson(const json& ev, std::vector<Op>& out) {
    out.clear();
    auto it = ev.find("ops");
    if (it == ev.end() || !it->is_array() || it->empty() || it->size() > kWordCount) {
        return false;
    }
    for (const json& o : *it) {
        Op op;
        uint32_t word = 0;
        if (!o.is_array() || o.size() != 3 || !U32(o[0], word) || word >= kWordCount || !U32(o[1], op.set) ||
            !U32(o[2], op.clear) || (op.set & op.clear) != 0 || (op.set | op.clear) == 0) {
            out.clear();
            return false;
        }
        op.word = (uint8_t)word;
        out.push_back(op);
    }
    return true;
}

json OpsToJson(const std::vector<Op>& ops) {
    json list = json::array();
    for (const Op& op : ops) {
        list.push_back(json::array({ op.word, op.set, op.clear }));
    }
    return list;
}

bool WordsFromJson(const json& ev, Words& out) {
    auto it = ev.find("words");
    if (it == ev.end() || !it->is_array() || it->size() != kWordCount) {
        return false;
    }
    Words w{};
    for (size_t i = 0; i < kWordCount; i++) {
        if (!U32((*it)[i], w[i])) {
            return false;
        }
    }
    out = w;
    return true;
}

json WordsToJson(const Words& words) {
    json list = json::array();
    for (uint32_t v : words) {
        list.push_back(v);
    }
    return list;
}

Words TemporaryOf(const Words& words) {
    Words t{};
    for (uint8_t w = 0; w < kWordCount; w++) {
        if (IsTemporary(w)) {
            t[w] = words[w];
        }
    }
    return t;
}

} // namespace coop::scene_flags
