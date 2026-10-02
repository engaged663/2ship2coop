#include "ModStorage.h"

#include "server/World/JsonFile.h"

#include <filesystem>
#include <system_error>

namespace coop::server {

ModStorage::ModStorage(std::string path) : mPath(std::move(path)) {
    if (mPath.empty()) {
        return;
    }
    json saved;
    bool missing = false;
    if (LoadJsonFile(mPath, saved, nullptr, &missing) && saved.is_object()) {
        mData = std::move(saved);
    } else {
        mUnreadable = !missing;
    }
}

json ModStorage::Get(const std::string& key) const {
    auto it = mData.find(key);
    return it != mData.end() ? *it : json(nullptr);
}

void ModStorage::Set(const std::string& key, const json& value) {
    if (value.is_null()) {
        Remove(key);
        return;
    }
    auto it = mData.find(key);
    if (it != mData.end() && *it == value) {
        return;
    }
    mData[key] = value;
    mDirty = true;
}

bool ModStorage::Remove(const std::string& key) {
    if (mData.erase(key) == 0) {
        return false;
    }
    mDirty = true;
    return true;
}

std::vector<std::string> ModStorage::Keys() const {
    std::vector<std::string> keys;
    for (auto it = mData.begin(); it != mData.end(); ++it) {
        keys.push_back(it.key());
    }
    return keys; // a JSON object keeps its keys sorted
}

bool ModStorage::Save(std::string* err) {
    if (!mDirty || mPath.empty()) {
        mDirty = false;
        return true;
    }
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(mPath).parent_path(), ec);
    if (!SaveJsonFile(mPath, mData, err)) {
        return false; // stays dirty: tried again on the next save
    }
    mDirty = false;
    return true;
}

} // namespace coop::server
