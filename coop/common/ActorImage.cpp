#include "ActorImage.h"

#include "ByteStream.h"
#include "SlotCodec.h"

#include <algorithm>
#include <cmath>

namespace coop {

namespace {

using namespace image_limits;

// Packet: [type u8][playerId u8][scene s16][room s8][seq u16][part u8][parts u8][flags u8: 1 = alive list]
//         [aliveCount u8][alive u32...][goneCount u8][gone u32...][recordCount u8][records...]
// Record: [key u32][actorId u16][flags u8: 1 masks, 2 sfx, 4 spawn, 8 continuation, 16 dyna]
//         [acMask u8][ocMask u8] (masks) [n u8][SoundEntry...] (sfx)
//         [actorId u16][params s16][pos f32 x3][rot s16 x3][parentKey u32] (spawn) [flags u8] (dyna)
//         [spanCount u8] then spans: [region u8][first u16][count u8] + per slot [kind u8][payload]
// Payload: Raw u64, Zero/Keep none, Exe u32, Actor key u32 + region u8 + offset u16, Link player u8 + offset u32.
constexpr size_t kHeaderBytes = 1 + 1 + 2 + 1 + 2 + 1 + 1 + 1;
constexpr size_t kListsBytes = 1 + 1 + 1; // the three counts
constexpr size_t kSpanHeaderBytes = 1 + 2 + 1;
constexpr size_t kSpawnBytes = 2 + 2 + 12 + 6 + 4;
constexpr int kMaxPackets = 32;
constexpr size_t kMaxAliveSent = 200; // 800 bytes: a room never has that many replicated actors
constexpr size_t kMaxGoneSent = 64;
constexpr size_t kChunkSlots = 32;    // a span is cut in pieces this big when a packet fills up
constexpr uint8_t kRecMasks = 1;
constexpr uint8_t kRecSfx = 2;
constexpr uint8_t kRecSpawn = 4;
constexpr uint8_t kRecContinuation = 8;
constexpr uint8_t kRecDyna = 16;

size_t PayloadBytes(SlotKind kind) {
    switch (kind) {
        case SlotKind::Raw:
            return 8;
        case SlotKind::Exe:
        case SlotKind::Scene:
            return 4;
        case SlotKind::Actor:
            return 7;
        case SlotKind::Link:
            return 5;
        default:
            return 0;
    }
}

size_t SlotsBytes(const std::vector<Slot>& slots, size_t from, size_t count) {
    size_t n = 0;
    for (size_t i = from; i < from + count; i++) {
        n += 1 + PayloadBytes(slots[i].kind);
    }
    return n;
}

bool HasMasks(const ActorImageRecord& r) {
    return r.acMask != 0 || r.ocMask != 0;
}

size_t RecordHeadBytes(const ActorImageRecord& r, bool first) {
    size_t n = 4 + 2 + 1 + 1; // key, actorId, flags, spanCount
    if (first) {
        n += HasMasks(r) ? 2 : 0;
        n += r.sfx.empty() ? 0 : SoundsBytes(r.sfx, kSfx);
        n += r.hasSpawn ? kSpawnBytes : 0;
        n += r.dyna >= 0 ? 1 : 0;
    }
    return n;
}

void WriteSlot(Writer& w, const Slot& s) {
    w.U8((uint8_t)s.kind);
    switch (s.kind) {
        case SlotKind::Raw:
            w.U64(s.value);
            break;
        case SlotKind::Exe:
        case SlotKind::Scene:
            w.U32((uint32_t)s.value);
            break;
        case SlotKind::Actor:
            w.U32((uint32_t)(s.value >> 32));
            w.U8((uint8_t)(s.value >> 16));
            w.U16((uint16_t)s.value);
            break;
        case SlotKind::Link:
            w.U8((uint8_t)(s.value >> 32));
            w.U32((uint32_t)s.value);
            break;
        default:
            break;
    }
}

void WriteRecord(Writer& w, const ActorImageRecord& r) {
    uint8_t flags = r.continuation ? kRecContinuation
                                   : (uint8_t)((HasMasks(r) ? kRecMasks : 0) | (!r.sfx.empty() ? kRecSfx : 0) |
                                               (r.hasSpawn ? kRecSpawn : 0) | (r.dyna >= 0 ? kRecDyna : 0));
    w.U32(r.key);
    w.U16(r.actorId);
    w.U8(flags);
    if (flags & kRecMasks) {
        w.U8(r.acMask);
        w.U8(r.ocMask);
    }
    if (flags & kRecSfx) {
        WriteSounds(w, r.sfx, kSfx);
    }
    if (flags & kRecSpawn) {
        w.U16(r.spawn.actorId);
        w.S16(r.spawn.params);
        for (float v : r.spawn.pos) {
            w.F32(v);
        }
        for (int16_t v : r.spawn.rot) {
            w.S16(v);
        }
        w.U32(r.spawn.parentKey);
    }
    if (flags & kRecDyna) {
        w.U8((uint8_t)r.dyna);
    }
    w.U8((uint8_t)r.spans.size());
    for (const SlotSpan& s : r.spans) {
        w.U8(s.region);
        w.U16(s.first);
        w.U8((uint8_t)s.slots.size());
        for (const Slot& slot : s.slots) {
            WriteSlot(w, slot);
        }
    }
}

std::vector<uint8_t> WritePacket(const ActorImagePacket& p) {
    Writer w;
    w.U8(kStreamActors);
    w.U8(p.playerId);
    w.S16(p.scene);
    w.S8(p.room);
    w.U16(p.seq);
    w.U8(p.part);
    w.U8(p.parts);
    w.U8(p.hasAlive ? 1 : 0);
    w.U8((uint8_t)p.alive.size());
    for (uint32_t k : p.alive) {
        w.U32(k);
    }
    w.U8((uint8_t)p.gone.size());
    for (uint32_t k : p.gone) {
        w.U32(k);
    }
    w.U8((uint8_t)p.records.size());
    for (const ActorImageRecord& r : p.records) {
        WriteRecord(w, r);
    }
    return w.Take();
}

bool ReadSlot(Reader& r, Slot& s) {
    uint8_t kind = 0;
    if (!r.U8(kind) || kind >= kSlotKinds) {
        return false;
    }
    s.kind = (SlotKind)kind;
    s.value = 0;
    switch (s.kind) {
        case SlotKind::Raw:
            return r.U64(s.value);
        case SlotKind::Exe:
        case SlotKind::Scene: {
            uint32_t v = 0;
            if (!r.U32(v)) {
                return false;
            }
            s.value = v;
            return true;
        }
        case SlotKind::Actor: {
            uint32_t key = 0;
            uint8_t region = 0;
            uint16_t offset = 0;
            if (!r.U32(key) || !r.U8(region) || !r.U16(offset) || region >= kRegions) {
                return false;
            }
            s.value = ActorRefValue(key, region, offset);
            return true;
        }
        case SlotKind::Link: {
            uint8_t player = 0;
            uint32_t offset = 0;
            if (!r.U8(player) || !r.U32(offset) || offset >= kRegionBytes) {
                return false;
            }
            s.value = LinkRefValue(player, offset);
            return true;
        }
        default:
            return true;
    }
}

bool ReadRecord(Reader& r, ActorImageRecord& out) {
    uint8_t flags = 0;
    if (!r.U32(out.key) || !r.U16(out.actorId) || !r.U8(flags) ||
        (flags & ~(kRecMasks | kRecSfx | kRecSpawn | kRecContinuation | kRecDyna)) ||
        ((flags & kRecContinuation) && flags != kRecContinuation)) {
        return false;
    }
    out.continuation = (flags & kRecContinuation) != 0;
    if ((flags & kRecMasks) && (!r.U8(out.acMask) || !r.U8(out.ocMask))) {
        return false;
    }
    if ((flags & kRecSfx) && (!ReadSounds(r, out.sfx, kSfx) || out.sfx.empty())) {
        return false;
    }
    out.hasSpawn = (flags & kRecSpawn) != 0;
    if (out.hasSpawn) {
        SpawnInfo& sp = out.spawn;
        if (!r.U16(sp.actorId) || !r.S16(sp.params)) {
            return false;
        }
        for (float& v : sp.pos) {
            if (!r.F32(v) || !std::isfinite(v) || std::fabs(v) > kWorldLimit) {
                return false;
            }
        }
        for (int16_t& v : sp.rot) {
            if (!r.S16(v)) {
                return false;
            }
        }
        if (!r.U32(sp.parentKey)) {
            return false;
        }
    }
    if (flags & kRecDyna) {
        uint8_t dyna = 0;
        if (!r.U8(dyna)) {
            return false;
        }
        out.dyna = dyna;
    }
    uint8_t spans = 0;
    if (!r.U8(spans) || spans > kSpans) {
        return false;
    }
    out.spans.resize(spans);
    for (SlotSpan& s : out.spans) {
        uint8_t count = 0;
        if (!r.U8(s.region) || !r.U16(s.first) || !r.U8(count) || s.region >= kRegions || count == 0 ||
            ((uint32_t)s.first + count) * 8 > kRegionBytes) {
            return false;
        }
        s.slots.resize(count);
        for (Slot& slot : s.slots) {
            if (!ReadSlot(r, slot)) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

void WriteImageSlot(Writer& w, const Slot& s) {
    WriteSlot(w, s);
}

bool ReadImageSlot(Reader& r, Slot& s) {
    return ReadSlot(r, s);
}

size_t ImageSlotBytes(const Slot& s) {
    return 1 + PayloadBytes(s.kind);
}

std::vector<std::vector<uint8_t>> EncodeActorImage(const ActorImagePacket& frame) {
    std::vector<ActorImagePacket> packets;
    auto newPacket = [&]() -> ActorImagePacket& {
        ActorImagePacket p;
        p.playerId = frame.playerId;
        p.scene = frame.scene;
        p.room = frame.room;
        p.seq = frame.seq;
        packets.push_back(std::move(p));
        return packets.back();
    };
    ActorImagePacket* cur = &newPacket();
    // Lists first, whole (a cut alive list would make the others delete actors): too long, it is not sent.
    if (frame.hasAlive && frame.alive.size() <= kMaxAliveSent) {
        cur->hasAlive = true;
        cur->alive = frame.alive;
    }
    cur->gone.assign(frame.gone.begin(), frame.gone.begin() + std::min(frame.gone.size(), kMaxGoneSent));
    size_t used = kHeaderBytes + kListsBytes + 4 * (cur->alive.size() + cur->gone.size());

    bool full = false; // out of packets: the rest waits for the next frame
    for (const ActorImageRecord& rec : frame.records) {
        if (full) {
            break;
        }
        bool first = true;
        size_t span = 0;
        size_t slot = 0;
        do {
            size_t head = RecordHeadBytes(rec, first);
            if (used + head + kSpanHeaderBytes + 9 > kPacketBytes || cur->records.size() >= (size_t)kRecords) {
                if ((int)packets.size() >= kMaxPackets) {
                    full = true;
                    break;
                }
                cur = &newPacket();
                used = kHeaderBytes + kListsBytes;
            }
            ActorImageRecord piece;
            piece.key = rec.key;
            piece.actorId = rec.actorId;
            piece.continuation = !first;
            if (first) {
                piece.acMask = rec.acMask;
                piece.ocMask = rec.ocMask;
                piece.sfx.assign(rec.sfx.begin(), rec.sfx.begin() + std::min<size_t>(rec.sfx.size(), kSfx));
                piece.hasSpawn = rec.hasSpawn;
                piece.spawn = rec.spawn;
                piece.dyna = rec.dyna;
            }
            used += head;
            // Whole spans (or pieces of kChunkSlots slots) while they fit.
            while (span < rec.spans.size() && piece.spans.size() < (size_t)kSpans) {
                const SlotSpan& s = rec.spans[span];
                size_t take = std::min({ s.slots.size() - slot, kChunkSlots, (size_t)kSpanSlots });
                size_t bytes = kSpanHeaderBytes + SlotsBytes(s.slots, slot, take);
                if (used + bytes > kPacketBytes) {
                    break;
                }
                SlotSpan out;
                out.region = s.region;
                out.first = (uint16_t)(s.first + slot);
                out.slots.assign(s.slots.begin() + slot, s.slots.begin() + slot + take);
                piece.spans.push_back(std::move(out));
                used += bytes;
                slot += take;
                if (slot >= s.slots.size()) {
                    span++;
                    slot = 0;
                }
            }
            cur->records.push_back(std::move(piece));
            first = false;
            if (span < rec.spans.size()) {
                used = kPacketBytes; // this packet is full: the rest of the record goes on in the next one
            }
        } while (span < rec.spans.size());
    }
    std::vector<std::vector<uint8_t>> out;
    for (size_t i = 0; i < packets.size(); i++) {
        packets[i].part = (uint8_t)i;
        packets[i].parts = (uint8_t)packets.size();
        out.push_back(WritePacket(packets[i]));
    }
    return out;
}

bool DecodeActorImage(const uint8_t* data, size_t size, ActorImagePacket& out) {
    if (data == nullptr || size > kPacketBytes) {
        return false;
    }
    Reader r(data, size);
    ActorImagePacket p;
    uint8_t type = 0;
    uint8_t flags = 0;
    if (!r.U8(type) || type != kStreamActors || !r.U8(p.playerId) || !r.S16(p.scene) || !r.S8(p.room) ||
        !r.U16(p.seq) || !r.U8(p.part) || !r.U8(p.parts) || !r.U8(flags)) {
        return false;
    }
    if (p.scene < 0 || p.room < 0 || p.room > kRoomMax || p.parts == 0 || p.parts > kMaxPackets ||
        p.part >= p.parts || (flags & ~1u)) {
        return false;
    }
    p.hasAlive = (flags & 1) != 0;
    uint8_t n = 0;
    if (!r.U8(n) || (!p.hasAlive && n != 0)) {
        return false;
    }
    p.alive.resize(n);
    for (uint32_t& k : p.alive) {
        if (!r.U32(k)) {
            return false;
        }
    }
    if (!r.U8(n)) {
        return false;
    }
    p.gone.resize(n);
    for (uint32_t& k : p.gone) {
        if (!r.U32(k)) {
            return false;
        }
    }
    if (!r.U8(n) || n > kRecords) {
        return false;
    }
    p.records.resize(n);
    for (ActorImageRecord& rec : p.records) {
        if (!ReadRecord(r, rec)) {
            return false;
        }
    }
    if (r.Remaining() != 0) {
        return false;
    }
    out = std::move(p);
    return true;
}

bool PeekActorImageHeader(const uint8_t* data, size_t size, int16_t& scene, int8_t& room) {
    if (data == nullptr || size < kHeaderBytes || data[0] != kStreamActors) {
        return false;
    }
    Reader r(data + 2, size - 2);
    int16_t s = -1;
    int8_t rm = -1;
    if (!r.S16(s) || !r.S8(rm) || s < 0 || rm < 0 || rm > kRoomMax) {
        return false;
    }
    scene = s;
    room = rm;
    return true;
}

Slot ClassifySlot(uint64_t raw, const PointerResolver& resolver) {
    if (raw == 0) {
        return { SlotKind::Zero, 0 };
    }
    uint64_t v = 0;
    if (resolver.ExeOffset(raw, v)) {
        return { SlotKind::Exe, v };
    }
    if (resolver.ActorRef(raw, v)) {
        return { SlotKind::Actor, v };
    }
    if (resolver.LinkRef(raw, v)) {
        return { SlotKind::Link, v };
    }
    if (resolver.SceneOffset(raw, v)) {
        return { SlotKind::Scene, v };
    }
    if (resolver.IsMapped(raw)) {
        return { SlotKind::Keep, 0 };
    }
    return { SlotKind::Raw, raw };
}

} // namespace coop
