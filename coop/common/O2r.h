#pragma once
// The game mods (.o2r archives) a server shares with its players (docs/superpowers/specs/2026-10-03-coop-mods-o2r-design.md):
// the list announced in "welcome", the chunks on kChannelFiles and the game's download plan. Server and game.
#include "Events.h"
#include "Sha256.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace coop::o2r {

struct Entry {
    std::string name;   // what the players see and keep it as (ValidName)
    uint64_t size = 0;  // bytes, 1..kMaxO2rBytes
    std::string sha256; // 64 lowercase hex digits
};

// A name a server may announce: 5..kMaxO2rName characters of letters, digits and " _-.()[]+,'&!", ending in ".o2r"
// (any case), not starting with '.' or ' ', without "..", and not a Windows device (con.o2r, lpt1.x.o2r...).
bool ValidName(const std::string& name);
// The server's file name made announceable: other characters become '_' (one per character), cut to kMaxO2rName.
std::string SafeName(const std::string& fileName);
bool ValidSha256(const std::string& hex); // 64 lowercase hex digits
// The game's copy, inside coop_mods/: "<name without .o2r>.<first 16 hex digits of the hash>.o2r".
std::string CacheFileName(const Entry& entry);
std::string FormatBytes(uint64_t bytes); // "512 B", "48.7 KB", "1.2 MB", "3.4 GB"

json ListToJson(const std::vector<Entry>& list);
// The "o2r" of a welcome (null: the server shares nothing). False, with why, at the first thing out of the rules
// (out is then empty).
bool ListFromJson(const json& value, std::vector<Entry>& out, std::string* why);

// One chunk on kChannelFiles: [kStreamO2r][u8 index][u32 offset, little endian][1..kO2rChunkBytes bytes].
constexpr size_t kChunkHeader = 6;
static_assert(kO2rChunkBytes + kChunkHeader <= kMaxPacketBytes, "a chunk must fit in one server packet");
struct Chunk {
    uint8_t index = 0;   // in the server's list
    uint32_t offset = 0; // of its first byte in the file
    const uint8_t* data = nullptr;
    size_t size = 0;
};
std::vector<uint8_t> EncodeChunk(uint8_t index, uint32_t offset, const uint8_t* data, size_t size);
bool DecodeChunk(const uint8_t* packet, size_t size, Chunk& out); // out.data points into packet

// The game's download of what it lacks: one file after the other, kO2rWindow chunk requests in flight. The chunks
// come back in order (kChannelFiles is reliable and ordered): anything else fails the download. Each file is hashed
// as it arrives and only kept if its SHA-256 is the announced one.
class Download {
  public:
    enum class Error { None, BadChunk, Open, Write, Hash };
    struct Job {
        uint8_t index; // in the server's list
        Entry entry;
    };
    struct Request {
        uint8_t index;
        uint32_t offset;
    };
    // Where the bytes go: a ".part" file renamed once verified in the game, memory in the tests.
    struct Sink {
        virtual ~Sink() = default;
        virtual bool Open(const Entry& entry) = 0;
        virtual bool Write(const uint8_t* data, size_t size) = 0;
        virtual bool Close(bool keep) = 0; // keep: verified, keep it (false = it could not); !keep: throw it away
    };

    Download(std::vector<Job> jobs, Sink& sink);
    ~Download(); // a file left open is thrown away
    Download(const Download&) = delete;
    Download& operator=(const Download&) = delete;

    // The o2r_get to send now (they keep the current file's window full). Empty when done or failed.
    std::vector<Request> TakeRequests();
    // A chunk from the server. False when it failed the download (Fail() says why), or when nothing is expected.
    bool OnChunk(const Chunk& chunk);

    bool Done() const; // every file kept
    bool Failed() const;
    Error Fail() const;
    const Entry* Current() const; // the file being fetched; nullptr when done or failed
    size_t Number() const;        // 1-based number of the current file ("2/3")
    size_t Count() const;
    uint64_t BytesDone() const;
    uint64_t BytesTotal() const;

  private:
    void Start(); // opens the current file
    void FailWith(Error error);

    std::vector<Job> mJobs;
    Sink& mSink;
    size_t mCurrent = 0;
    bool mStarted = false;
    bool mOpen = false;
    uint64_t mReceived = 0;  // of the current file
    uint64_t mRequested = 0; // where its next request starts
    size_t mInFlight = 0;
    uint64_t mFinished = 0;  // bytes of the files already kept
    uint64_t mTotal = 0;
    Sha256 mHash;
    Error mError = Error::None;
};

} // namespace coop::o2r
