#include "WorldSync.h"

#include "FieldTable.h"
#include "WorldSession.h"

#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Puppet/PoseCapture.h"

#include "common/Protocol.h"
#include "common/WorldOps.h"

#include <array>
#include <chrono>
#include <vector>

extern "C" {
#include "functions.h"
#include "variables.h"
}

namespace coop::client {

namespace {

constexpr int kSendEveryMs = 100;       // changes are gathered and sent at most 10 times per second
constexpr size_t kMaxOpsPerEvent = 400; // keeps every wops far below the server's event size limit

std::array<std::vector<uint8_t>, world::kFieldCount> sShadow; // the world as last synced
bool sActive = false;
std::vector<json> sQueued; // wops received while the save was not ready to merge them
int64_t sLastSendMs = 0;

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// What changed since the shadow goes to `out`; the shadow becomes the live save.
void DiffLocal(world::Ops& out) {
    std::vector<uint8_t> now;
    for (size_t f = 0; f < world::kFieldCount; f++) {
        now.resize(world::kFields[f].size);
        fields::Read((int)f, now.data());
        if (now != sShadow[f]) {
            world::Diff((uint16_t)f, sShadow[f].data(), now.data(), out);
            sShadow[f] = now;
        }
    }
}

void Send(const world::Ops& ops) {
    if (ops.Empty() || !Session_IsConnected()) {
        return;
    }
    world::Ops chunk;
    auto flush = [&chunk]() {
        if (chunk.Empty()) {
            return;
        }
        json ev = world::ToJson(chunk);
        ev["t"] = ev::kWops;
        ev["cycle"] = WorldSession_Cycle();
        NetClient::Get().SendEvent(ev);
        chunk = world::Ops();
    };
    for (const world::BitsOp& op : ops.bits) {
        chunk.bits.push_back(op);
        if (chunk.Count() >= kMaxOpsPerEvent) {
            flush();
        }
    }
    for (const world::ByteOp& op : ops.bytes) {
        chunk.bytes.push_back(op);
        if (chunk.Count() >= kMaxOpsPerEvent) {
            flush();
        }
    }
    for (const world::AddOp& op : ops.adds) {
        chunk.adds.push_back(op);
        if (chunk.Count() >= kMaxOpsPerEvent) {
            flush();
        }
    }
    flush();
}

// Another player's changes (or the server's corrections): read, merge and write each touched field.
void Merge(const json& ev) {
    world::Ops ops;
    if (!world::FromJson(ev, ops, nullptr)) {
        return;
    }
    world::Ops local; // our unsent changes go first, so the shadow equals the live save while merging
    DiffLocal(local);
    Send(local);

    std::array<std::vector<uint8_t>, world::kFieldCount> live;
    std::array<bool, world::kFieldCount> touched{};
    auto field = [&live, &touched](uint16_t f) -> uint8_t* {
        if (!touched[f]) {
            live[f].resize(world::kFields[f].size);
            fields::Read(f, live[f].data());
            touched[f] = true;
        }
        return live[f].data();
    };
    bool changed = false;
    int32_t applied = 0;
    for (const world::BitsOp& op : ops.bits) {
        if (op.field < world::kFieldCount) {
            world::ApplyBits(field(op.field), op, &changed);
        }
    }
    for (const world::ByteOp& op : ops.bytes) {
        if (op.field < world::kFieldCount) {
            world::ApplyByte(field(op.field), op, &changed);
        }
    }
    for (const world::AddOp& op : ops.adds) {
        if (op.field < world::kFieldCount) {
            world::ApplyAdd(field(op.field), op, &applied);
        }
    }
    for (size_t f = 0; f < world::kFieldCount; f++) {
        if (touched[f]) {
            fields::Write((int)f, live[f].data());
            fields::Read((int)f, sShadow[f].data()); // what the save really took (items are checked, hearts clamped)
        }
    }
}

void OnWops(const json& ev) {
    if (WorldSession_State() == WorldState::Outside) {
        return; // not in the server's world (or just left it)
    }
    if (!sActive) {
        sQueued.push_back(ev);
        return;
    }
    Merge(ev);
}

} // namespace

void WorldSync_Reset() {
    sActive = false;
    sQueued.clear();
    for (auto& shadow : sShadow) {
        shadow.clear();
    }
}

void WorldSync_Hold() {
    sActive = false;
    sQueued.clear();
}

void WorldSync_TakeShadow() {
    for (size_t f = 0; f < world::kFieldCount; f++) {
        sShadow[f].resize(world::kFields[f].size);
        fields::Read((int)f, sShadow[f].data());
    }
}

void WorldSync_SetActive(bool active) {
    if (active == sActive) {
        return;
    }
    if (!active) {
        world::Ops last;
        DiffLocal(last);
        Send(last);
        sActive = false;
        return;
    }
    sActive = true;
    sLastSendMs = 0;
    std::vector<json> queued;
    queued.swap(sQueued);
    for (const json& ev : queued) {
        Merge(ev);
    }
}

bool WorldSync_Active() {
    return sActive;
}

void WorldSync_FrameEnd() {
    if (!sActive || !PoseCapture_InGameplay() || gPlayState->transitionTrigger != TRANS_TRIGGER_OFF) {
        return;
    }
    int64_t now = NowMs();
    if (now - sLastSendMs < kSendEveryMs) {
        return;
    }
    sLastSendMs = now;
    world::Ops ops;
    DiffLocal(ops);
    Send(ops);
}

void WorldSync_Flush() {
    if (!sActive) {
        return;
    }
    world::Ops ops;
    DiffLocal(ops);
    Send(ops);
    sLastSendMs = NowMs();
}

COOP_ON_EVENT(worldSyncWops, ev::kWops, OnWops);

} // namespace coop::client
