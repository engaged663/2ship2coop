#include "JsonFile.h"

#include "common/I18n.h"

#include <filesystem>
#include <fstream>
#include <system_error>

namespace coop::server {

bool LoadJsonFile(const std::string& path, json& out, std::string* err, bool* missing) {
    auto fail = [err](const std::string& why) {
        if (err != nullptr) {
            *err = why;
        }
        return false;
    };
    std::error_code ec;
    bool exists = std::filesystem::exists(path, ec);
    if (missing != nullptr) {
        *missing = !exists;
    }
    if (!exists) {
        return fail(Tr(Msg::FileMissing, { path }));
    }
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return fail(Tr(Msg::FileOpenFail, { path }));
    }
    json parsed = json::parse(in, nullptr, false);
    if (parsed.is_discarded()) {
        return fail(Tr(Msg::FileNotJson, { path }));
    }
    out = std::move(parsed);
    return true;
}

bool SaveJsonFile(const std::string& path, const json& value, std::string* err) {
    auto fail = [err](const std::string& why) {
        if (err != nullptr) {
            *err = why;
        }
        return false;
    };
    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            return fail(Tr(Msg::FileWriteFail, { tmp }));
        }
        out << value.dump(1, '\t', false, json::error_handler_t::replace) << '\n';
        out.flush();
        if (!out.good()) {
            return fail(Tr(Msg::FileWriteFail, { tmp }));
        }
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        return fail(Tr(Msg::FileReplaceFail, { path, ec.message() }));
    }
    return true;
}

} // namespace coop::server
