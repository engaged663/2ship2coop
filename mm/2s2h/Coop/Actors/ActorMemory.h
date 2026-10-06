#pragma once
// [COOP] Sub-project D3: an actor's memory as 8-byte slots. The sender classifies each slot (plain data, or a
// pointer it can translate: into the exe, into a replicated actor, into an actor of the room's list that every game
// has for itself (LocalListKey: the receiver points at its own), into a Link); the receiver writes it back with its
// own addresses. The engine's bookkeeping of the base Actor (lists, culling, distances to the player...) never
// travels: every game keeps its own.
#include "ActorRegistry.h"

namespace coop::client {

// Once per frame before sending: which memory is whose (replicated actors, our Link, the puppets).
void ActorMemory_RebuildResolver();

// The slot as it travels (the engine's own bytes of the base Actor read as zero).
Slot ActorMemory_Capture(const TrackedActor& t, size_t region, size_t slot);
// After its regions exist: which bytes of its instance are this game's own (the base Actor's engine fields, and a
// DynaPolyActor's collision index and "who stands on it", which belong to each game's collision context).
void ActorMemory_BuildLocalMask(TrackedActor& t);
// True if the whole slot is the engine's own (never sent).
bool ActorMemory_IsLocalSlot(const TrackedActor& t, size_t region, size_t slot);
// Writes a received slot into the copy (translating pointers; "keep" restores its value after Init).
void ActorMemory_Apply(TrackedActor& t, const SlotSpan& span);
// Restores every slot of this copy that points into that memory (an actor or a puppet being destroyed).
void ActorMemory_ForgetPointersTo(TrackedActor& copy, const void* start, size_t size);

// For other things that travel like an actor's memory (an effect's init data: Features/EffectEcho.cpp).
// A raw 8-byte value as it would travel (pointers into the exe, a replicated actor or a Link translated; one into an
// actor every game has for itself is Keep here: its effects and sounds stay in its game).
Slot ActorMemory_Classify(uint64_t raw);
// Where a received slot points in this game (nullptr: nowhere here).
uint8_t* ActorMemory_Resolve(const Slot& s);

} // namespace coop::client
