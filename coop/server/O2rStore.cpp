#include "O2rStore.h"

#include "common/I18n.h"
#include "common/Sha256.h"
#include "common/Text.h"

#include <algorithm>
#include <fstream>
#include <system_error>

namespace coop::server {

namespace {

namespace fs = std::filesystem;

// server.json's strings are UTF-8; Windows paths are not.
fs::path PathOf(const std::string& utf8) {
    return fs::path(std::u8string(utf8.begin(), utf8.end()));
}

std::string Utf8(const fs::path& path) {
    std::u8string text = path.u8string();
    return std::string(text.begin(), text.end());
}

bool IsO2r(const fs::path& file) {
    std::error_code ec;
    return fs::is_regular_file(file, ec) && ToLower(Utf8(file.extension())) == ".o2r";
}

// The .o2r files of the folder itself (not of its subfolders), by name ignoring case.
std::vector<fs::path> FolderFiles(const fs::path& dir) {
    std::vector<fs::path> all;
    std::error_code ec;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        if (IsO2r(it->path())) {
            all.push_back(it->path());
        }
    }
    std::sort(all.begin(), all.end(), [](const fs::path& a, const fs::path& b) {
        return ToLower(Utf8(a.filename())) < ToLower(Utf8(b.filename()));
    });
    return all;
}

void Warn(std::string* warnings, const std::string& text) {
    if (warnings != nullptr) {
        *warnings += (warnings->empty() ? "" : "; ") + text;
    }
}

// A name of the list: the file's name or its name without ".o2r", in any case.
bool Matches(const fs::path& file, const std::string& lowerName) {
    return ToLower(Utf8(file.filename())) == lowerName || ToLower(Utf8(file.stem())) == lowerName;
}

// server.json "o2r.files": "*" = every file of the folder; a name = that file, in that place of the list; "!name"
// leaves one out.
std::vector<fs::path> Resolve(const O2rConfig& config, std::string* warnings) {
    std::vector<fs::path> inFolder = FolderFiles(PathOf(config.dir));
    std::vector<fs::path> files;
    std::vector<std::string> leftOut;
    auto add = [&files](const fs::path& file) {
        if (std::find(files.begin(), files.end(), file) == files.end()) {
            files.push_back(file);
        }
    };
    for (const std::string& entry : config.files) {
        if (entry.empty()) {
            continue;
        }
        if (entry[0] == '!') {
            leftOut.push_back(ToLower(entry.substr(1)));
        } else if (entry == "*") {
            for (const fs::path& file : inFolder) {
                add(file);
            }
        } else {
            std::string name = ToLower(entry);
            auto it = std::find_if(inFolder.begin(), inFolder.end(),
                                   [&name](const fs::path& file) { return Matches(file, name); });
            if (it == inFolder.end()) {
                Warn(warnings, Tr(Msg::ModFileMissing, { entry }));
            } else {
                add(*it);
            }
        }
    }
    std::vector<fs::path> out;
    for (const fs::path& file : files) {
        bool skip = std::any_of(leftOut.begin(), leftOut.end(),
                                [&file](const std::string& name) { return Matches(file, name); });
        if (!skip) {
            out.push_back(file);
        }
    }
    return out;
}

} // namespace

void O2rStore::Load(const O2rConfig& config, std::string* warnings) {
    mEntries.clear();
    mPaths.clear();
    if (config.dir.empty()) {
        return;
    }
    for (const fs::path& file : Resolve(config, warnings)) {
        if (mEntries.size() >= (size_t)kMaxO2rFiles) {
            Warn(warnings, Tr(Msg::O2rTooMany, { std::to_string(kMaxO2rFiles) }));
            break;
        }
        std::string shown = Utf8(file.filename());
        uint64_t size = 0;
        std::string sha = Sha256File(file, &size);
        o2r::Entry entry{ o2r::SafeName(shown), size, sha };
        bool repeated = std::any_of(mEntries.begin(), mEntries.end(), [&entry](const o2r::Entry& e) {
            return ToLower(e.name) == ToLower(entry.name) || e.sha256 == entry.sha256;
        });
        Msg why = Msg::O2rWhyRepeated;
        if (sha.empty()) {
            why = Msg::O2rWhyUnreadable;
        } else if (size == 0) {
            why = Msg::O2rWhyEmpty;
        } else if (size > kMaxO2rBytes) {
            why = Msg::O2rWhyTooBig;
        } else if (!repeated) {
            mEntries.push_back(std::move(entry));
            mPaths.push_back(file);
            continue;
        }
        Warn(warnings, Tr(Msg::O2rSkipped, { shown, Tr(why) }));
    }
}

bool O2rStore::ReadChunk(size_t index, uint64_t offset, size_t max, std::vector<uint8_t>& out) const {
    if (index >= mEntries.size() || offset >= mEntries[index].size) {
        return false;
    }
    std::ifstream in(mPaths[index], std::ios::binary);
    if (!in.seekg((std::streamoff)offset)) {
        return false;
    }
    size_t want = (size_t)std::min<uint64_t>(max, mEntries[index].size - offset);
    out.resize(want);
    in.read(reinterpret_cast<char*>(out.data()), (std::streamsize)want);
    return (size_t)in.gcount() == want;
}

bool O2rStore::Accepts(const json& worldEnter) const {
    if (mEntries.empty()) {
        return true;
    }
    auto list = worldEnter.find("o2r");
    if (list == worldEnter.end() || !list->is_array()) {
        return false;
    }
    for (const o2r::Entry& e : mEntries) {
        bool has = std::any_of(list->begin(), list->end(),
                               [&e](const json& v) { return v.is_string() && v.get<std::string>() == e.sha256; });
        if (!has) {
            return false;
        }
    }
    return true;
}

bool O2rStore::HasO2rFiles(const std::string& dir) {
    return !dir.empty() && !FolderFiles(PathOf(dir)).empty();
}

} // namespace coop::server
