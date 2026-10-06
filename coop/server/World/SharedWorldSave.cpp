// Keeping the shared world safe on disk: loading world.json (lenient), damaged files (set aside, then the newest
// backup), the backups (at start, every backupMinutes, before what cannot be undone) and replacing the whole world
// while people play (an import, a restored backup).
#include "SharedWorld.h"

#include "JsonFile.h"
#include "ServerLock.h"

#include "server/Mods/ModHost.h"
#include "server/Server.h"

#include "common/Clock.h"

#include <cstring>

namespace coop::server {

SharedWorld::~SharedWorld() {
    if (!mLockPath.empty()) {
        RemoveServerLock(mLockPath);
    }
}

void SharedWorld::Load() {
    std::string warnings;
    mPlayers.LoadAll(&warnings);
    if (!warnings.empty()) {
        mServer.Log().Warn(warnings);
    }
    if (mWorldPath.empty()) {
        return;
    }
    uint32_t other = 0;
    if (ServerLockHeld(mLockPath, &other)) {
        // A live server already uses these files (this one was started twice by mistake): its lock stays, so the
        // converter keeps refusing to write here
        mServer.Log().Warn(Tr(Msg::ServerLockOther, { std::to_string(other) }));
    } else {
        WriteServerLock(mLockPath);
    }
    mLastBackupMs = Now();
    json saved;
    std::string err;
    bool missing = false;
    if (!LoadJsonFile(mWorldPath, saved, &err, &missing)) {
        if (missing) {
            mServer.Log().Info(Tr(Msg::NoWorldLog));
        } else {
            Quarantine(err);
        }
        return;
    }
    if (!LoadSaved(saved, &err)) {
        Quarantine(err);
        return;
    }
    if (mBackups.Enabled() && !mBackups.SameAsNewest(mWorldPath)) {
        MakeBackup("start", nullptr); // the world as this server found it
    }
}

bool SharedWorld::LoadSaved(const json& saved, std::string* err) {
    WorldImage image;
    std::vector<std::string> warnings;
    if (!ParseWorldFile(saved, image, &warnings, err)) {
        return false;
    }
    for (const std::string& warning : warnings) {
        mServer.Log().Warn(warning);
    }
    int64_t now = Now();
    mStore.Replace(image.fields, image.start, image.cycle);
    mClock.Set(image.clockAbs, now);
    mClock.SetInverted(image.inverted, now);
    mFrozen = image.frozen;
    mServer.Log().Info(Tr(Msg::WorldLoaded, { std::to_string(mStore.Cycle()), clock::Format(image.clockAbs) }));
    return true;
}

void SharedWorld::Quarantine(const std::string& why) {
    std::string aside;
    if (!SetAside(mWorldPath, &aside)) {
        mBlocked = true; // never a new world on top of a file nobody could look at
        mServer.Log().Error(Tr(Msg::WorldBlocked, { mWorldPath, why }));
        return;
    }
    mServer.Log().Error(Tr(Msg::WorldCorrupt, { mWorldPath, why, aside }));
    std::string newest = mBackups.Newest();
    if (newest.empty()) {
        mServer.Log().Warn(Tr(Msg::WorldNoBackup));
        return;
    }
    json saved;
    std::string err;
    if (LoadJsonFile(mBackups.WorldFile(newest), saved, &err) && LoadSaved(saved, &err)) {
        mServer.Log().Warn(Tr(Msg::WorldRecovered, { newest }));
        mDirty = true;
        Flush(); // world.json is whole again
    } else {
        mServer.Log().Error(Tr(Msg::WorldRecoverFail, { newest, err }));
    }
}

void SharedWorld::TickBackups(int64_t now) {
    int64_t every = mServer.Config().backupMs;
    if (every > 0 && mBackupDirty && mStore.Exists() && now - mLastBackupMs >= every) {
        MakeBackup("auto", nullptr);
    }
}

std::string SharedWorld::MakeBackup(const std::string& reason, std::string* err) {
    std::string why;
    std::string name;
    if (!mBackups.Enabled()) {
        why = Tr(Msg::BackupOff);
    } else if (!mStore.Exists()) {
        why = Tr(Msg::NoWorldYet);
    } else {
        Flush(); // the copy is of the world as it is now
        mLastBackupMs = Now(); // made or not: the next automatic one waits a whole period, never the next tick
        if (mBackups.Make(mWorldPath, mPlayersDir, reason, &name, &why)) {
            mBackupDirty = false;
            mServer.Log().Info(Tr(Msg::BackupMade, { name }));
            return name;
        }
        mServer.Log().Error(Tr(Msg::BackupFail, { why }));
    }
    if (err != nullptr) {
        *err = why;
    }
    return "";
}

std::vector<BackupInfo> SharedWorld::Backups() const {
    return mBackups.List();
}

bool SharedWorld::RestoreBackup(const std::string& nameOrLast, const std::string& by, std::string* err) {
    std::string name = mBackups.Resolve(nameOrLast);
    if (name.empty()) {
        *err = Tr(Msg::BackupNotFound, { nameOrLast });
        return false;
    }
    WorldImage image;
    std::vector<std::string> warnings;
    if (!LoadWorldImage(mBackups.WorldFile(name), mBackups.PlayersDir(name), image, &warnings, err)) {
        return false;
    }
    for (const std::string& warning : warnings) {
        mServer.Log().Warn(warning);
    }
    return Replace(image, "restore", by, name, err);
}

bool SharedWorld::Replace(const WorldImage& image, const char* reason, const std::string& by, const std::string& what,
                          std::string* err) {
    if (mBlocked) {
        *err = Tr(Msg::WorldBlockedEnter);
        return false;
    }
    if (mResetting || mCreator != 0) {
        *err = Tr(Msg::ReplaceBusy);
        return false;
    }
    if (image.fields.size() != world::kFieldCount || image.start.size() != world::kFieldCount) {
        *err = Tr(Msg::MissingFields);
        return false;
    }
    if (mStore.Exists()) {
        MakeBackup(reason, nullptr); // the world it replaces, to go back to it
    }
    // A new number: the changes and uploads of the old world still on their way say its number and are dropped. The
    // players' cycles keep their distance to the world's (who had missed a Song of Time still has).
    int newCycle = (mStore.Exists() ? mStore.Cycle() : 0) + 1;
    int offset = newCycle - image.cycle;
    std::vector<PlayerRecord> players = image.players;
    for (PlayerRecord& record : players) {
        record.cycle += offset;
        if (record.startCycle >= 0) {
            record.startCycle += offset;
        }
    }
    int64_t now = Now();
    mVote.Cancel();
    mStore.Replace(image.fields, image.start, newCycle);
    mPlayers.ReplaceAll(std::move(players));
    mClock.Set(image.clockAbs, now);
    mClock.SetInverted(image.inverted, now);
    mFrozen = image.frozen;
    mDirty = true;
    Flush();
    for (RemoteClient* p : Receivers()) {
        mServer.SendEvent(*p, FullEvent(*p, reason));
    }
    bool import = std::strcmp(reason, "import") == 0;
    Announce(Tr(import ? Msg::WorldImported : Msg::WorldRestored, { by, what }), level::kWarn, nullptr);
    if (mServer.Mods().Wants(ModEvent::CycleReset)) {
        json e = { { "cycle", mStore.Cycle() }, { "reason", reason } };
        mServer.Mods().Fire(ModEvent::CycleReset, e);
    }
    EnterWaiting();
    UpdateClock();
    return true;
}

} // namespace coop::server
