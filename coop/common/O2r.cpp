#include "O2r.h"

#include "StreamIds.h"
#include "Text.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace coop::o2r {

namespace {

bool SafeChar(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           (c != 0 && std::strchr(" _-.()[]+,'&!", c) != nullptr);
}

bool EndsWithO2r(const std::string& name) {
    return name.size() >= 4 && ToLower(name.substr(name.size() - 4)) == ".o2r";
}

// CON, PRN, AUX, NUL, COM0-9, LPT0-9: Windows opens the device instead of a file named so, whatever comes after the
// first dot (the copy in coop_mods/ is "<stem>.<hash>.o2r").
bool IsDevice(const std::string& stem) {
    std::string base = ToLower(stem.substr(0, stem.find('.')));
    while (!base.empty() && base.back() == ' ') {
        base.pop_back();
    }
    if (base == "con" || base == "prn" || base == "aux" || base == "nul") {
        return true;
    }
    return base.size() == 4 && (base.compare(0, 3, "com") == 0 || base.compare(0, 3, "lpt") == 0) && base[3] >= '0' &&
           base[3] <= '9';
}

} // namespace

bool ValidName(const std::string& name) {
    if (name.size() < 5 || name.size() > (size_t)kMaxO2rName || !EndsWithO2r(name)) {
        return false;
    }
    for (unsigned char c : name) {
        if (!SafeChar(c)) {
            return false;
        }
    }
    if (name[0] == '.' || name[0] == ' ' || name.find("..") != std::string::npos) {
        return false;
    }
    return !IsDevice(name.substr(0, name.size() - 4));
}

std::string SafeName(const std::string& fileName) {
    std::string stem = EndsWithO2r(fileName) ? fileName.substr(0, fileName.size() - 4) : fileName;
    std::string out;
    for (unsigned char c : stem) {
        if ((c & 0xC0) == 0x80) {
            continue; // the rest of a UTF-8 character: one '_' for all of it
        }
        char put = SafeChar(c) ? (char)c : '_';
        if (put == '.' && !out.empty() && out.back() == '.') {
            put = '_';
        }
        out += put;
    }
    if (out.empty()) {
        out = "_";
    }
    if (out[0] == '.' || out[0] == ' ') {
        out[0] = '_';
    }
    if (IsDevice(out)) {
        out = "_" + out;
    }
    out = out.substr(0, (size_t)kMaxO2rName - 4);
    if (out.back() == '.') {
        out.back() = '_'; // "x." + ".o2r" would make ".."
    }
    return out + ".o2r";
}

bool ValidSha256(const std::string& hex) {
    if (hex.size() != 64) {
        return false;
    }
    for (char c : hex) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return false;
        }
    }
    return true;
}

std::string CacheFileName(const Entry& entry) {
    return entry.name.substr(0, entry.name.size() - 4) + "." + entry.sha256.substr(0, 16) + ".o2r";
}

std::string FormatBytes(uint64_t bytes) {
    char text[32];
    if (bytes < 1024) {
        std::snprintf(text, sizeof(text), "%llu B", (unsigned long long)bytes);
        return text;
    }
    static const char* const kUnits[] = { "KB", "MB", "GB" };
    double value = (double)bytes / 1024.0;
    int unit = 0;
    while (value >= 1024.0 && unit < 2) {
        value /= 1024.0;
        unit++;
    }
    std::snprintf(text, sizeof(text), "%.1f %s", value, kUnits[unit]);
    return text;
}

json ListToJson(const std::vector<Entry>& list) {
    json out = json::array();
    for (const Entry& e : list) {
        out.push_back({ { "name", e.name }, { "size", e.size }, { "sha256", e.sha256 } });
    }
    return out;
}

bool ListFromJson(const json& value, std::vector<Entry>& out, std::string* why) {
    out.clear();
    auto fail = [&](const char* text) {
        if (why != nullptr) {
            *why = text;
        }
        out.clear();
        return false;
    };
    if (value.is_null()) {
        return true;
    }
    if (!value.is_array()) {
        return fail("the list is not an array");
    }
    if (value.size() > (size_t)kMaxO2rFiles) {
        return fail("too many files");
    }
    for (const json& item : value) {
        if (!item.is_object()) {
            return fail("an entry is not an object");
        }
        auto name = item.find("name");
        auto size = item.find("size");
        auto sha = item.find("sha256");
        if (name == item.end() || !name->is_string() || !ValidName(name->get<std::string>())) {
            return fail("a name is not valid");
        }
        if (size == item.end() || !size->is_number_integer() ||
            (!size->is_number_unsigned() && size->get<int64_t>() <= 0)) {
            return fail("a size is not valid");
        }
        uint64_t bytes = size->get<uint64_t>();
        if (bytes == 0 || bytes > kMaxO2rBytes) {
            return fail("a size is not valid");
        }
        if (sha == item.end() || !sha->is_string() || !ValidSha256(sha->get<std::string>())) {
            return fail("a sha256 is not valid");
        }
        Entry entry{ name->get<std::string>(), bytes, sha->get<std::string>() };
        for (const Entry& other : out) {
            if (ToLower(other.name) == ToLower(entry.name) || other.sha256 == entry.sha256) {
                return fail("a file is listed twice");
            }
        }
        out.push_back(std::move(entry));
    }
    return true;
}

std::vector<uint8_t> EncodeChunk(uint8_t index, uint32_t offset, const uint8_t* data, size_t size) {
    std::vector<uint8_t> out;
    out.reserve(kChunkHeader + size);
    out.push_back(kStreamO2r);
    out.push_back(index);
    for (int i = 0; i < 4; i++) {
        out.push_back((uint8_t)(offset >> (8 * i)));
    }
    out.insert(out.end(), data, data + size);
    return out;
}

bool DecodeChunk(const uint8_t* packet, size_t size, Chunk& out) {
    if (packet == nullptr || size <= kChunkHeader || size > kChunkHeader + kO2rChunkBytes || packet[0] != kStreamO2r) {
        return false;
    }
    out.index = packet[1];
    out.offset = (uint32_t)packet[2] | (uint32_t)packet[3] << 8 | (uint32_t)packet[4] << 16 | (uint32_t)packet[5] << 24;
    out.data = packet + kChunkHeader;
    out.size = size - kChunkHeader;
    return true;
}

Download::Download(std::vector<Job> jobs, Sink& sink) : mJobs(std::move(jobs)), mSink(sink) {
    for (const Job& job : mJobs) {
        mTotal += job.entry.size;
    }
}

Download::~Download() {
    if (mOpen) {
        mSink.Close(false);
    }
}

void Download::Start() {
    mStarted = true;
    mReceived = 0;
    mRequested = 0;
    mInFlight = 0;
    mHash = Sha256();
    if (mCurrent >= mJobs.size()) {
        return;
    }
    mOpen = mSink.Open(mJobs[mCurrent].entry);
    if (!mOpen) {
        FailWith(Error::Open);
    }
}

void Download::FailWith(Error error) {
    mError = error;
    if (mOpen) {
        mOpen = false;
        mSink.Close(false);
    }
}

std::vector<Download::Request> Download::TakeRequests() {
    std::vector<Request> out;
    if (!mStarted && mError == Error::None) {
        Start();
    }
    if (Done() || Failed()) {
        return out;
    }
    const Job& job = mJobs[mCurrent];
    while (mInFlight < (size_t)kO2rWindow && mRequested < job.entry.size) {
        out.push_back({ job.index, (uint32_t)mRequested });
        mRequested += std::min<uint64_t>(kO2rChunkBytes, job.entry.size - mRequested);
        mInFlight++;
    }
    return out;
}

bool Download::OnChunk(const Chunk& chunk) {
    if (Done() || Failed()) {
        return false; // nothing is expected any more
    }
    const Job& job = mJobs[mCurrent];
    uint64_t expected = std::min<uint64_t>(kO2rChunkBytes, job.entry.size - mReceived);
    if (!mOpen || mInFlight == 0 || chunk.index != job.index || chunk.offset != mReceived || chunk.size != expected) {
        FailWith(Error::BadChunk);
        return false;
    }
    if (!mSink.Write(chunk.data, chunk.size)) {
        FailWith(Error::Write);
        return false;
    }
    mHash.Update(chunk.data, chunk.size);
    mReceived += chunk.size;
    mInFlight--;
    if (mReceived < job.entry.size) {
        return true;
    }
    mOpen = false;
    if (mHash.FinalHex() != job.entry.sha256) {
        mSink.Close(false);
        mError = Error::Hash;
        return false;
    }
    if (!mSink.Close(true)) {
        mError = Error::Write;
        return false;
    }
    mFinished += job.entry.size;
    mCurrent++;
    Start();
    return !Failed();
}

bool Download::Done() const {
    return mError == Error::None && mCurrent >= mJobs.size();
}

bool Download::Failed() const {
    return mError != Error::None;
}

Download::Error Download::Fail() const {
    return mError;
}

const Entry* Download::Current() const {
    return Done() || Failed() ? nullptr : &mJobs[mCurrent].entry;
}

size_t Download::Number() const {
    return std::min(mCurrent + 1, mJobs.size());
}

size_t Download::Count() const {
    return mJobs.size();
}

uint64_t Download::BytesDone() const {
    return mFinished + (mCurrent < mJobs.size() ? mReceived : 0);
}

uint64_t Download::BytesTotal() const {
    return mTotal;
}

} // namespace coop::o2r
