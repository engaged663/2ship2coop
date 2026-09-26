#pragma once
// Little-endian binary Writer/Reader for the stream channel.
// Explicit byte order so client and server agree regardless of compiler/platform.
#include <cstdint>
#include <cstring>
#include <vector>

namespace coop {

class Writer {
  public:
    void U8(uint8_t v) {
        mBuf.push_back(v);
    }
    void U16(uint16_t v) {
        U8((uint8_t)(v & 0xFF));
        U8((uint8_t)(v >> 8));
    }
    void U32(uint32_t v) {
        U16((uint16_t)(v & 0xFFFF));
        U16((uint16_t)(v >> 16));
    }
    void S8(int8_t v) {
        U8((uint8_t)v);
    }
    void S16(int16_t v) {
        U16((uint16_t)v);
    }
    void F32(float v) {
        uint32_t bits;
        std::memcpy(&bits, &v, sizeof(bits));
        U32(bits);
    }
    const std::vector<uint8_t>& Data() const {
        return mBuf;
    }
    std::vector<uint8_t> Take() {
        return std::move(mBuf);
    }

  private:
    std::vector<uint8_t> mBuf;
};

// Every read returns false once the data runs out; callers treat that as a malformed packet.
class Reader {
  public:
    Reader(const uint8_t* data, size_t size) : mData(data), mSize(size) {
    }
    bool U8(uint8_t& v) {
        if (mPos >= mSize) {
            return false;
        }
        v = mData[mPos++];
        return true;
    }
    bool U16(uint16_t& v) {
        uint8_t lo, hi;
        if (!U8(lo) || !U8(hi)) {
            return false;
        }
        v = (uint16_t)(lo | (hi << 8));
        return true;
    }
    bool U32(uint32_t& v) {
        uint16_t lo, hi;
        if (!U16(lo) || !U16(hi)) {
            return false;
        }
        v = (uint32_t)lo | ((uint32_t)hi << 16);
        return true;
    }
    bool S8(int8_t& v) {
        uint8_t u;
        if (!U8(u)) {
            return false;
        }
        v = (int8_t)u;
        return true;
    }
    bool S16(int16_t& v) {
        uint16_t u;
        if (!U16(u)) {
            return false;
        }
        v = (int16_t)u;
        return true;
    }
    bool F32(float& v) {
        uint32_t bits;
        if (!U32(bits)) {
            return false;
        }
        std::memcpy(&v, &bits, sizeof(v));
        return true;
    }
    size_t Remaining() const {
        return mSize - mPos;
    }

  private:
    const uint8_t* mData;
    size_t mSize;
    size_t mPos = 0;
};

} // namespace coop
