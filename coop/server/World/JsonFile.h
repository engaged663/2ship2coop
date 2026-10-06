#pragma once
// Small JSON files next to the server (world.json, players/*.json): read them, and replace them atomically.
#include "common/Events.h"

#include <string>

namespace coop::server {

// False with err for a file that is missing, unreadable or not JSON; *missing tells the first case apart.
bool LoadJsonFile(const std::string& path, json& out, std::string* err, bool* missing = nullptr);
// Writes path + ".tmp" and renames it over path: a crash never leaves a half-written file.
bool SaveJsonFile(const std::string& path, const json& value, std::string* err);

// "2026-10-05_14-30-00" (local time): the names of the backups and of the files set aside.
std::string FileStamp();
// Renames a damaged file to "<path>.bad-<stamp>" ("-2", "-3"... when that name is taken) so nothing ever overwrites
// it. False when there is no such file or it cannot be renamed (another program holds it).
bool SetAside(const std::string& path, std::string* newPath);

} // namespace coop::server
