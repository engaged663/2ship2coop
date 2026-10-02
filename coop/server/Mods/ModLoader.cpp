// Which files the mod host loads: the lists of server.json ("mods.scripts", "mods.plugins") turned into paths.
#include "ModHost.h"

#include "common/I18n.h"
#include "common/Text.h"

#include <algorithm>
#include <filesystem>
#include <system_error>

namespace coop::server {

namespace {

namespace fs = std::filesystem;

// The same file named in two ways is one file.
std::string KeyOf(const fs::path& file) {
    std::error_code ec;
    fs::path full = fs::absolute(file, ec);
    return ToLower((ec ? file : full).lexically_normal().generic_string());
}

bool IsFile(const fs::path& file) {
    std::error_code ec;
    return fs::is_regular_file(file, ec);
}

bool HasExtension(const fs::path& file, const std::vector<std::string>& extensions) {
    std::string ext = ToLower(file.extension().string());
    return std::find(extensions.begin(), extensions.end(), ext) != extensions.end();
}

// An entry of the list: a file of the folder (its extension may be left out) or a path. Empty: no such file.
fs::path Resolve(const std::string& dir, const std::string& entry, const std::vector<std::string>& extensions) {
    std::vector<fs::path> bases;
    fs::path given(entry);
    if (!dir.empty() && given.is_relative()) {
        bases.push_back(fs::path(dir) / given);
    }
    bases.push_back(given);
    for (const fs::path& base : bases) {
        if (IsFile(base)) {
            return base;
        }
        for (const std::string& ext : extensions) {
            fs::path with = base;
            with += ext;
            if (IsFile(with)) {
                return with;
            }
        }
    }
    return {};
}

} // namespace

std::vector<std::string> ResolveModFiles(const std::string& dir, const std::vector<std::string>& entries,
                                         const std::vector<std::string>& extensions, std::string* warnings) {
    std::vector<fs::path> files;
    std::vector<std::string> seen;
    std::vector<std::string> leftOut; // "!name": file names (with or without extension), in lowercase
    auto add = [&](const fs::path& file) {
        std::string key = KeyOf(file);
        if (std::find(seen.begin(), seen.end(), key) == seen.end()) {
            seen.push_back(key);
            files.push_back(file);
        }
    };
    for (const std::string& entry : entries) {
        if (entry.empty()) {
            continue;
        }
        if (entry[0] == '!') {
            leftOut.push_back(ToLower(entry.substr(1)));
        } else if (entry == "*") { // every file of the folder itself (not of its subfolders), by name
            std::vector<fs::path> all;
            std::error_code ec;
            if (!dir.empty()) {
                for (const auto& item : fs::directory_iterator(dir, ec)) {
                    if (IsFile(item.path()) && HasExtension(item.path(), extensions)) {
                        all.push_back(item.path());
                    }
                }
            }
            std::sort(all.begin(), all.end(), [](const fs::path& a, const fs::path& b) {
                return ToLower(a.filename().string()) < ToLower(b.filename().string());
            });
            for (const fs::path& file : all) {
                add(file);
            }
        } else {
            fs::path file = Resolve(dir, entry, extensions);
            if (file.empty()) {
                if (warnings != nullptr) {
                    *warnings += (warnings->empty() ? "" : "; ") + Tr(Msg::ModFileMissing, { entry });
                }
            } else {
                add(file);
            }
        }
    }
    std::vector<std::string> out;
    for (const fs::path& file : files) {
        std::string name = ToLower(file.filename().string());
        std::string stem = ToLower(file.stem().string());
        bool skip = std::find(leftOut.begin(), leftOut.end(), name) != leftOut.end() ||
                    std::find(leftOut.begin(), leftOut.end(), stem) != leftOut.end();
        if (!skip) {
            out.push_back(file.string());
        }
    }
    return out;
}

} // namespace coop::server
