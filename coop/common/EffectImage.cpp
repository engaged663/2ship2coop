#include "EffectImage.h"

#include "SlotCodec.h"
#include "StreamIds.h"

namespace coop {

namespace {

using namespace effect_limits;

constexpr size_t kHeaderBytes = 1 + 1 + 2 + 1 + 1;
constexpr size_t kEffectHeaderBytes = 1 + 1 + 1;
constexpr uint8_t kFlagCinema = 1;

bool Valid(const EffectRecord& r) {
    return r.type < kEffectTypes && r.slots.size() <= (size_t)kSlots;
}

size_t RecordBytes(const EffectRecord& r) {
    size_t n = kEffectHeaderBytes;
    for (const Slot& s : r.slots) {
        n += ImageSlotBytes(s);
    }
    return n;
}

std::vector<uint8_t> Finish(const EffectPacket& p, const std::vector<const EffectRecord*>& records) {
    Writer w;
    w.U8(kStreamEffects);
    w.U8(p.playerId);
    w.S16(p.scene);
    w.U8(p.cinema ? kFlagCinema : 0);
    w.U8((uint8_t)records.size());
    for (const EffectRecord* r : records) {
        w.U8(r->type);
        w.U8(r->priority);
        w.U8((uint8_t)r->slots.size());
        for (const Slot& s : r->slots) {
            WriteImageSlot(w, s);
        }
    }
    return w.Take();
}

} // namespace

std::vector<std::vector<uint8_t>> EncodeEffects(const EffectPacket& p) {
    std::vector<std::vector<uint8_t>> out;
    std::vector<const EffectRecord*> current;
    size_t bytes = kHeaderBytes;
    for (const EffectRecord& r : p.effects) {
        if (!Valid(r)) {
            continue;
        }
        size_t n = RecordBytes(r);
        if (!current.empty() && (current.size() >= (size_t)kEffects || bytes + n > kPacketBytes)) {
            out.push_back(Finish(p, current));
            current.clear();
            bytes = kHeaderBytes;
        }
        current.push_back(&r);
        bytes += n;
    }
    if (!current.empty()) {
        out.push_back(Finish(p, current));
    }
    return out;
}

bool DecodeEffects(const uint8_t* data, size_t size, EffectPacket& out) {
    if (data == nullptr || size < kHeaderBytes || size > kPacketBytes || data[0] != kStreamEffects) {
        return false;
    }
    Reader r(data + 1, size - 1);
    EffectPacket p;
    uint8_t flags = 0;
    uint8_t count = 0;
    if (!r.U8(p.playerId) || !r.S16(p.scene) || !r.U8(flags) || !r.U8(count) || p.scene < 0 ||
        (flags & ~kFlagCinema) != 0 || count == 0 || count > kEffects) {
        return false;
    }
    p.cinema = (flags & kFlagCinema) != 0;
    for (uint8_t i = 0; i < count; i++) {
        EffectRecord e;
        uint8_t slots = 0;
        if (!r.U8(e.type) || !r.U8(e.priority) || !r.U8(slots) || e.type >= kEffectTypes || slots > kSlots) {
            return false;
        }
        e.slots.resize(slots);
        for (Slot& s : e.slots) {
            if (!ReadImageSlot(r, s)) {
                return false;
            }
        }
        p.effects.push_back(std::move(e));
    }
    if (r.Remaining() != 0) {
        return false;
    }
    out = std::move(p);
    return true;
}

} // namespace coop
