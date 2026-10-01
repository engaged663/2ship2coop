#pragma once
// [COOP] One ActorImage slot on the wire (kind + payload), for the streams that reuse it (EffectImage.cpp).
#include "ActorImage.h"
#include "ByteStream.h"

namespace coop {

void WriteImageSlot(Writer& w, const Slot& s);
bool ReadImageSlot(Reader& r, Slot& s); // false: cut short or not a valid kind/payload
size_t ImageSlotBytes(const Slot& s);   // kind byte + payload

} // namespace coop
