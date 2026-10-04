#include "Sha256.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <vector>

namespace coop {

namespace {

constexpr uint32_t kRound[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

uint32_t Rotr(uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

} // namespace

Sha256::Sha256()
    : mState{ 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 } {
}

void Sha256::Block(const uint8_t* p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = (uint32_t)p[i * 4] << 24 | (uint32_t)p[i * 4 + 1] << 16 | (uint32_t)p[i * 4 + 2] << 8 | p[i * 4 + 3];
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = Rotr(w[i - 15], 7) ^ Rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = Rotr(w[i - 2], 17) ^ Rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = mState[0], b = mState[1], c = mState[2], d = mState[3];
    uint32_t e = mState[4], f = mState[5], g = mState[6], h = mState[7];
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + (Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25)) + ((e & f) ^ (~e & g)) + kRound[i] + w[i];
        uint32_t t2 = (Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    mState[0] += a;
    mState[1] += b;
    mState[2] += c;
    mState[3] += d;
    mState[4] += e;
    mState[5] += f;
    mState[6] += g;
    mState[7] += h;
}

void Sha256::Update(const void* data, size_t size) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    mBytes += size;
    if (mBuffered > 0) {
        size_t take = std::min(size, sizeof(mBuffer) - mBuffered);
        std::memcpy(mBuffer + mBuffered, p, take);
        mBuffered += take;
        p += take;
        size -= take;
        if (mBuffered < sizeof(mBuffer)) {
            return;
        }
        Block(mBuffer);
        mBuffered = 0;
    }
    for (; size >= sizeof(mBuffer); p += sizeof(mBuffer), size -= sizeof(mBuffer)) {
        Block(p);
    }
    if (size > 0) {
        std::memcpy(mBuffer, p, size);
        mBuffered = size;
    }
}

std::string Sha256::FinalHex() {
    // 0x80, zeros up to 56 bytes into a block (or into the next one), then the length in bits, big-endian.
    uint64_t bits = mBytes * 8;
    uint8_t pad[72] = { 0x80 };
    size_t zeros = (mBuffered < 56 ? 56 : 120) - mBuffered;
    for (int i = 0; i < 8; i++) {
        pad[zeros + i] = (uint8_t)(bits >> (56 - 8 * i));
    }
    Update(pad, zeros + 8);
    static const char kDigits[] = "0123456789abcdef";
    std::string out;
    out.reserve(64);
    for (uint32_t word : mState) {
        for (int shift = 28; shift >= 0; shift -= 4) {
            out += kDigits[(word >> shift) & 0xF];
        }
    }
    return out;
}

std::string Sha256Hex(const void* data, size_t size) {
    Sha256 h;
    h.Update(data, size);
    return h.FinalHex();
}

std::string Sha256File(const std::filesystem::path& path, uint64_t* size) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return "";
    }
    Sha256 h;
    std::vector<char> buffer(1 << 20);
    uint64_t total = 0;
    while (in) {
        in.read(buffer.data(), (std::streamsize)buffer.size());
        std::streamsize n = in.gcount();
        if (n <= 0) {
            break;
        }
        h.Update(buffer.data(), (size_t)n);
        total += (uint64_t)n;
    }
    if (in.bad()) {
        return "";
    }
    if (size != nullptr) {
        *size = total;
    }
    return h.FinalHex();
}

} // namespace coop
