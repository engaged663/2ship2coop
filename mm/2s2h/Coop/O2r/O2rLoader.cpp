// [COOP] Loading the server's .o2r mods while the game runs (docs/superpowers/specs/2026-10-03-coop-mods-o2r-design.md
// §3.4). Each archive is added at the frame's safe point (BenPort.cpp: after the alternate assets switch, the audio
// thread waits and nothing is loading), and the resource caches that remember the files it replaces are cleared
// without freeing anything the game may still point at. Also "Enable Mods" (the alternate assets) while the server's
// world is asked for or played, if its mods have "alt/" files. Map: O2r.h.
#include "O2r.h"

#include "2s2h/Coop/World/WorldSession.h"
#include "2s2h/Enhancements/GfxPatcher/PlayerCustomFlipbooks.h"
#include "2s2h/resource/type/Skeleton.h"

#include <libultraship/bridge/consolevariablebridge.h>
#include <libultraship/libultraship.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/Archive.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <memory>

extern "C" void OTRExtScanner();          // BenPort.cpp: the file name cache the "alt/" preloading of 2 Ship reads
extern "C" void gfx_texture_cache_clear(); // the textures already sent to the GPU

namespace coop::client {

namespace {

constexpr const char* kAltCVar = "gEnhancements.Mods.AlternateAssets";  // 2 Ship's "Enable Mods" (Tab)
constexpr const char* kAltRestoreCVar = "gCoop.World.AltAssetsRestore"; // its value before: "" none, "-" unset

struct Loaded {
    std::string sha256;
    std::shared_ptr<Ship::Archive> archive;
    std::vector<std::string> altFiles; // its "alt/..." paths
};

std::vector<Loaded> sLoaded;
std::vector<O2rLoadItem> sQueue;
std::vector<std::shared_ptr<Ship::IResource>> sKeptAlive; // replaced resources: the game may still point at them
bool sAltWasOn = false;
bool sAltForced = false;

std::shared_ptr<Ship::ResourceManager> Resources() {
    return Ship::Context::GetRawInstance()->GetResourceManager();
}

// What the cache has under path goes: a resource is kept alive (never freed under the game's feet), and a "not found"
// left by an "alt/" lookup made before this archive existed stops hiding the new file.
void Forget(const std::string& path) {
    auto resources = Resources();
    if (std::shared_ptr<Ship::IResource> old = resources->GetCachedResource(path, true)) {
        sKeptAlive.push_back(old);
    }
    resources->UnloadResource(path);
}

// "alt/X" whose X is in use: the cache keeps giving X, even with "Enable Mods" on, to the display lists that name it by
// hash until alt/X itself is cached, so it is loaded now.
void PreloadAlt(const Loaded& loaded) {
    auto resources = Resources();
    for (const std::string& alt : loaded.altFiles) {
        std::string base = alt.substr(Ship::IResource::gAltAssetPrefix.size());
        if (resources->GetCachedResource(base, true) != nullptr) {
            resources->LoadResource(alt, true);
        }
    }
}

bool Add(const O2rLoadItem& item) {
    std::shared_ptr<Ship::Archive> archive = Resources()->GetArchiveManager()->AddArchive(item.path);
    if (archive == nullptr) {
        SPDLOG_ERROR("[Coop] .o2r: could not load {}", item.path);
        return false;
    }
    Loaded loaded{ item.entry.sha256, archive, {} };
    auto files = archive->ListFiles();
    for (const auto& [hash, path] : *files) {
        if (path.empty() || path.back() == '/') {
            continue; // a folder
        }
        Forget(path);
        if (path.ends_with(".meta")) { // an alias is cached under its base path
            Forget(path.substr(0, path.size() - 5));
        }
        if (path.starts_with(Ship::IResource::gAltAssetPrefix)) {
            loaded.altFiles.push_back(path);
        }
    }
    PreloadAlt(loaded);
    SPDLOG_INFO("[Coop] .o2r: loaded {} ({} files, {} alt)", item.entry.name, files->size(), loaded.altFiles.size());
    sLoaded.push_back(std::move(loaded));
    return true;
}

// "Enable Mods" goes back to what it was before a server's world needed it.
void RestoreAlt() {
    std::string saved = CVarGetString(kAltRestoreCVar, "");
    if (saved.empty()) {
        return;
    }
    if (saved == "-") {
        CVarClear(kAltCVar);
    } else {
        CVarSetInteger(kAltCVar, std::atoi(saved.c_str()));
    }
    CVarClear(kAltRestoreCVar);
    CVarSave();
}

} // namespace

void O2rLoader_Queue(std::vector<O2rLoadItem> items) {
    sQueue = std::move(items);
}

bool O2rLoader_Busy() {
    return !sQueue.empty();
}

bool O2rLoader_IsLoaded(const std::string& sha256) {
    for (const Loaded& loaded : sLoaded) {
        if (loaded.sha256 == sha256) {
            return true;
        }
    }
    return false;
}

bool O2rLoader_HasAlt(const std::vector<o2r::Entry>& entries) {
    for (const Loaded& loaded : sLoaded) {
        if (loaded.altFiles.empty()) {
            continue;
        }
        for (const o2r::Entry& entry : entries) {
            if (entry.sha256 == loaded.sha256) {
                return true;
            }
        }
    }
    return false;
}

void O2rLoader_AltFrame() {
    static bool sRecovered = false;
    if (!sRecovered) { // a game closed inside the server's world gets its own value back
        sRecovered = true;
        RestoreAlt();
    }
    bool want = CVarGetInteger("gCoop.O2r.AltAssets", 1) != 0 && WorldSession_State() != WorldState::Outside &&
                O2rLoader_HasAlt(O2r_Required());
    if (want == sAltForced) {
        return;
    }
    sAltForced = want;
    if (!want) {
        RestoreAlt();
        return;
    }
    std::string saved = CVarGet(kAltCVar) == nullptr ? "-" : std::to_string(CVarGetInteger(kAltCVar, 0));
    CVarSetString(kAltRestoreCVar, saved.c_str());
    CVarSetInteger(kAltCVar, 1); // BenPort.cpp applies it at the end of this frame
    CVarSave();
    SPDLOG_INFO("[Coop] .o2r: \"Enable Mods\" on while playing in the server's world (it was {})", saved);
}

void O2r_FrameSafePoint() {
    bool altOn = Resources()->IsAltAssetsEnabled();
    if (!sQueue.empty()) {
        std::vector<O2rLoadItem> items;
        items.swap(sQueue);
        bool added = false;
        for (const O2rLoadItem& item : items) {
            added = Add(item) || added;
        }
        if (added) {
            OTRExtScanner();
            gfx_texture_cache_clear();
            SOH::SkeletonPatcher::UpdateSkeletons();
            PlayerCustomFlipbooks_Refresh();
        }
    } else if (altOn && !sAltWasOn) { // "Enable Mods" just came on (Tab, or O2rLoader_AltFrame)
        for (const Loaded& loaded : sLoaded) {
            PreloadAlt(loaded);
        }
    }
    sAltWasOn = altOn;
}

} // namespace coop::client
