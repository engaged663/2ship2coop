#include "Hex.h"

namespace coop {

namespace {

int Nibble(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

} // namespace

std::string ToHex(const uint8_t* data, size_t size) {
    static const char kDigits[] = "0123456789abcdef";
    std::string out(size * 2, '0');
    for (size_t i = 0; i < size; i++) {
        out[i * 2] = kDigits[data[i] >> 4];
        out[i * 2 + 1] = kDigits[data[i] & 0xF];
    }
    return out;
}

bool FromHex(const std::string& text, std::vector<uint8_t>& out) {
    if (text.size() % 2 != 0) {
        return false;
    }
    std::vector<uint8_t> bytes(text.size() / 2);
    for (size_t i = 0; i < bytes.size(); i++) {
        int hi = Nibble(text[i * 2]);
        int lo = Nibble(text[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        bytes[i] = (uint8_t)((hi << 4) | lo);
    }
    out = std::move(bytes);
    return true;
}

} // namespace coop
