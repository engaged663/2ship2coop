#include "WorldFields.h"

#include <cstring>

namespace coop::world {

int FindField(const char* name) {
    for (size_t i = 0; i < kFieldCount; i++) {
        if (std::strcmp(kFields[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

bool IsCounter(Kind kind) {
    return kind == Kind::CounterU8 || kind == Kind::CounterS8 || kind == Kind::CounterU16;
}

int CounterWidth(Kind kind) {
    if (kind == Kind::CounterU16) {
        return 2;
    }
    return IsCounter(kind) ? 1 : 0;
}

void CounterRange(Kind kind, int& min, int& max) {
    switch (kind) {
        case Kind::CounterS8:
            min = -128;
            max = 127;
            break;
        case Kind::CounterU16:
            min = 0;
            max = 0xFFFF;
            break;
        default:
            min = 0;
            max = 0xFF;
            break;
    }
}

} // namespace coop::world
