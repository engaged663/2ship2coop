#pragma once
// server.lock next to world.json while a server runs, holding its process id: the converter tool
// (tools/SaveConvert.cpp) never writes into the folder of a live server, which would overwrite the import on its next
// save. A server that crashed leaves a dead id behind, which blocks nothing; a second server started in the same folder
// leaves a live one's lock alone (SharedWorld::Load warns instead).
#include <cstdint>
#include <string>

namespace coop::server {

uint32_t CurrentProcessId();
bool WriteServerLock(const std::string& path);
void RemoveServerLock(const std::string& path); // only a lock of this process
// True when path names a process that is alive and is not this one (pid gets its id).
bool ServerLockHeld(const std::string& path, uint32_t* pid);

} // namespace coop::server
