#pragma once
// [COOP] The server's game mods (.o2r) (docs/superpowers/specs/2026-10-03-coop-mods-o2r-design.md): on "welcome" the
// game sees what it lacks, asks, downloads it to coop_mods/, loads it while running and only then enters the server's
// world. O2rSync.cpp: states and download. O2rLoader.cpp: loading at the frame's safe point and "Enable Mods".
// O2rWindow.cpp: the window at the top and the Co-op menu's section.
#include "common/O2r.h"

#include <cstdint>
#include <string>
#include <vector>

namespace coop::client {

enum class O2rState {
    Idle,        // not connected (or not welcomed yet)
    Asking,      // the window asks whether to download what is missing
    Downloading,
    Loading,     // downloaded: waiting for the frame's safe point
    Ready,       // every mod of the server is loaded (also when it shares none)
};

// O2rSync.cpp
O2rState O2r_State();
bool O2r_Ready();
const std::vector<o2r::Entry>& O2r_Required(); // the server's list
std::vector<std::string> O2r_LoadedHashes();   // of that list, what this game has loaded (world_enter "o2r")
bool O2r_InCache(const o2r::Entry& entry);     // already in coop_mods/
void O2r_Accept(bool always);                  // "Descargar" (always: never ask again)
void O2r_Decline();                            // "No" or "Cancelar": disconnects
struct O2rProgress {
    std::string name; // the file being fetched
    size_t number = 0;
    size_t count = 0;
    uint64_t done = 0;
    uint64_t total = 0;
};
O2rProgress O2r_Progress(); // what is being (or would be) downloaded
void O2r_FrameStart();      // CoopInit.cpp, after the network: timeouts, end of the loading, "Enable Mods"

// O2rLoader.cpp
struct O2rLoadItem {
    o2r::Entry entry;
    std::string path; // its copy in coop_mods/
};
void O2rLoader_Queue(std::vector<O2rLoadItem> items); // loaded at the next safe point, in this order
bool O2rLoader_Busy();
bool O2rLoader_IsLoaded(const std::string& sha256);
bool O2rLoader_HasAlt(const std::vector<o2r::Entry>& entries); // one of them is loaded and has "alt/" files
void O2rLoader_AltFrame(); // "Enable Mods" on while the server's world is asked for or played, if its mods need it
// BenPort.cpp, Graph_ProcessGfxCommands, after the alternate assets switch: nothing is loading and the audio thread
// waits. Adds what O2rLoader_Queue left.
void O2r_FrameSafePoint();

// O2rWindow.cpp
void O2rMenu_Draw(); // the Co-op menu's section

} // namespace coop::client
