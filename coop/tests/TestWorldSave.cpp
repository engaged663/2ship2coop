// Saving the shared world (server/World/SharedWorld.cpp): backups, recovery from damaged files, saving on demand,
// the frozen clock, the lock while running, and replacing the world while people play (import / restore).
#include "TestWorld.h"

#include "common/Clock.h"
#include "common/I18n.h"
#include "server/ServerConfig.h"
#include "server/World/JsonFile.h"
#include "server/World/ServerLock.h"
#include "server/World/WorldImage.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#endif

using namespace coop;
using namespace coop::server;
using namespace coop_test;
namespace fs = std::filesystem;

namespace {

server::ServerConfig FilesIn(const TempDir& dir) {
    server::ServerConfig cfg;
    cfg.worldPath = dir.File("world.json");
    cfg.playersDir = dir.File("players");
    return cfg;
}

std::vector<std::string> NamesStartingWith(const fs::path& dir, const std::string& prefix) {
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        std::string name = entry.path().filename().string();
        if (name.rfind(prefix, 0) == 0) {
            names.push_back(name);
        }
    }
    return names;
}

std::string ReadText(const std::string& path) {
    std::ifstream in(path);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void StopServer(TestServer& s, TestClient& c) {
    s.server->Stop("prueba");
    c.WaitUntil(s, 3000, [&] { return !s.server->IsRunning(); });
}

// A world (cycle 1) with a mark in weekEventReg[5], saved and the server stopped.
void MakeWorld(const server::ServerConfig& cfg, uint8_t mark) {
    TestServer s(cfg);
    auto a = Join(s, "Alice");
    CreateWorld(s, *a);
    a->Send({ { "t", "wops" }, { "cycle", 1 }, { "bits", json::array({ Arr(Field("weekEventReg"), 5, mark, 0) }) } });
    s.PumpFor(150);
    StopServer(s, *a);
}

uint8_t MarkOf(const json& full) {
    std::vector<uint8_t> week;
    CHECK(FromHex(full["fields"]["weekEventReg"].get<std::string>(), week));
    return week[5];
}

PlayerRecord Record(const std::string& nick, int cycle) {
    PlayerRecord r;
    r.nick = nick;
    r.inv = { { "v", 1 }, { "who", nick } };
    r.cycle = cycle;
    r.start = r.inv;
    r.startCycle = cycle;
    return r;
}

} // namespace

TEST_CASE(DamagedWorldRecoversFromNewestBackup) {
    TempDir dir("coop_test_save_recover");
    server::ServerConfig cfg = FilesIn(dir);
    MakeWorld(cfg, 0x10);
    {
        TestServer s(cfg); // starting with a world takes a copy of it ("start")...
        CHECK(s.server->World().Exists());
        CHECK_EQ(s.server->World().Backups().size(), (size_t)1);
    }
    {
        TestServer s(cfg); // ...but not again when nothing changed since
        CHECK_EQ(s.server->World().Backups().size(), (size_t)1);
    }
    std::ofstream(cfg.worldPath) << "{roto";
    TestServer s(cfg);
    CHECK(s.server->World().Exists()); // back from the copy
    CHECK_EQ(NamesStartingWith(dir.path, "world.json.bad-").size(), (size_t)1);
    json saved;
    std::string err;
    CHECK(LoadJsonFile(cfg.worldPath, saved, &err)); // whole again on disk
    auto a = Join(s, "Alice");
    CHECK_EQ(MarkOf(EnterWorld(s, *a)), (uint8_t)0x10);
}

TEST_CASE(DamagedWorldWithoutBackupsIsSetAside) {
    TempDir dir("coop_test_save_aside");
    server::ServerConfig cfg = FilesIn(dir);
    std::ofstream(cfg.worldPath) << "{roto";
    {
        TestServer s(cfg);
        CHECK(!s.server->World().Exists());
        CHECK(!s.server->World().Blocked());
    }
    CHECK_EQ(NamesStartingWith(dir.path, "world.json.bad-").size(), (size_t)1);
    std::ofstream(cfg.worldPath) << "{otro";
    {
        TestServer s(cfg);
    }
    CHECK_EQ(NamesStartingWith(dir.path, "world.json.bad-").size(), (size_t)2); // never on top of the first one
}

#ifdef _WIN32
TEST_CASE(UnmovableWorldBlocksCreation) {
    TempDir dir("coop_test_save_blocked");
    server::ServerConfig cfg = FilesIn(dir);
    std::ofstream(cfg.worldPath) << "{\"intacto\":1}";
    int fd = -1;
    CHECK(_sopen_s(&fd, cfg.worldPath.c_str(), _O_RDONLY, _SH_DENYRW, _S_IREAD) == 0); // another program holds it
    {
        TestServer s(cfg);
        CHECK(s.server->World().Blocked());
        auto a = Join(s, "Alice");
        a->Send({ { "t", "world_enter" } });
        CHECK(a->WaitForSys("error", s));
        CHECK(!a->TakeEvent("world_full").has_value());
    }
    _close(fd);
    CHECK_EQ(ReadText(cfg.worldPath), std::string("{\"intacto\":1}")); // never touched
}
#endif

TEST_CASE(InvWithSaveFlushesAtOnce) {
    TempDir dir("coop_test_save_now");
    server::ServerConfig cfg = FilesIn(dir);
    cfg.worldSaveMs = 600000; // the timer never saves during the test
    TestServer s(cfg);
    auto a = Join(s, "Alice");
    CreateWorld(s, *a);
    json inv = { { "v", 1 }, { "fields", { { "rupees", "0a00" } } } };
    a->Send({ { "t", "inv" }, { "inv", inv }, { "cycle", 1 } });
    s.PumpFor(150);
    json saved;
    CHECK(!LoadJsonFile(dir.File("players/alice.json"), saved, nullptr)); // waits for the timer
    a->Send({ { "t", "inv" }, { "inv", inv }, { "cycle", 1 }, { "save", true } });
    CHECK(a->WaitUntil(s, 1000, [&] { return LoadJsonFile(dir.File("players/alice.json"), saved, nullptr); }));
    CHECK_EQ(saved["inv"], inv);
}

TEST_CASE(FrozenClockSurvivesRestart) {
    TempDir dir("coop_test_save_frozen");
    server::ServerConfig cfg = FilesIn(dir);
    {
        TestServer s(cfg);
        auto a = Join(s, "Alice");
        CreateWorld(s, *a);
        std::string err;
        CHECK(s.server->World().SetClockStopped(true, "prueba", &err));
        s.PumpFor(50);
        StopServer(s, *a);
    }
    TestServer s(cfg);
    CHECK(s.server->World().Frozen());
}

TEST_CASE(BackupsBeforeTheMoon) {
    TempDir dir("coop_test_save_moon");
    server::ServerConfig cfg = FilesIn(dir);
    MakeWorld(cfg, 0x01);
    TestServer s(cfg);
    auto a = Join(s, "Alice");
    EnterWorld(s, *a);
    s.server->World().CrashMoon();
    s.PumpFor(100);
    std::vector<BackupInfo> backups = s.server->World().Backups();
    CHECK_EQ(backups.size(), (size_t)2);
    CHECK_EQ(backups[0].reason, std::string("moon"));
    CHECK_EQ(backups[1].reason, std::string("start"));
    WorldImage image;
    std::string err;
    std::string folder = dir.File("backups") + "/" + backups[0].name;
    CHECK(LoadWorldImage(folder + "/world.json", folder + "/players", image, nullptr, &err));
    CHECK_EQ(image.fields[Field("weekEventReg")][5], (uint8_t)0x01); // the world as it was before the moon
    CHECK_EQ(s.server->World().Store().Fields()[Field("weekEventReg")][5], (uint8_t)0); // the moon took it back
}

TEST_CASE(ReplaceSwitchesPlayersInside) {
    TempDir dir("coop_test_save_replace");
    server::ServerConfig cfg = FilesIn(dir);
    TestServer s(cfg);
    auto a = Join(s, "Alice");
    CreateWorld(s, *a);
    a->Send({ { "t", "inv" }, { "inv", { { "v", 1 }, { "old", true } } }, { "cycle", 1 }, { "save", true } });
    s.PumpFor(100);
    Drain(s, { a.get() });
    WorldImage image;
    image.fields = WorldStore::EmptyFields();
    image.fields[Field("weekEventReg")][7] = 0x42;
    image.start = image.fields;
    image.clockAbs = 0x8000;
    image.players = { Record("Bob", 1) };
    std::string err;
    CHECK(s.server->World().Replace(image, "import", "prueba", "Bob", &err));
    auto full = a->WaitFor("world_full", s);
    CHECK(full.has_value());
    CHECK_EQ(GetString(*full, "reset"), std::string("import"));
    CHECK_EQ(GetInt(*full, "cycle"), 2); // a new number: nothing of the old world can sneak in
    std::vector<uint8_t> week;
    CHECK(FromHex((*full)["fields"]["weekEventReg"].get<std::string>(), week));
    CHECK_EQ(week[7], (uint8_t)0x42);
    CHECK((*full)["you"]["inv"].is_null()); // Alice's data belonged to the old world
    CHECK(GetInt((*full)["clock"], "abs") >= 0x8000);
    CHECK(a->WaitForSys("warn", s)); // everyone is told
    // A change Alice made in the old world arrives late: dropped
    a->Send({ { "t", "wops" }, { "cycle", 1 }, { "bits", json::array({ Arr(Field("weekEventReg"), 9, 0x01, 0) }) } });
    a->Send({ { "t", "inv" }, { "inv", { { "v", 1 }, { "late", true } } }, { "cycle", 1 } });
    s.PumpFor(100);
    CHECK_EQ(s.server->World().Store().Fields()[Field("weekEventReg")][9], (uint8_t)0);
    s.server->World().Flush();
    CHECK(fs::exists(dir.path / "players" / "bob.json"));
    CHECK(fs::exists(dir.path / "players" / "alice.json.old"));
    CHECK(!fs::exists(dir.path / "players" / "alice.json"));
    CHECK_EQ(s.server->World().Backups()[0].reason, std::string("import")); // the world before it
}

TEST_CASE(RestoreBackupKeepsWhoWasBehind) {
    TempDir dir("coop_test_save_restore");
    server::ServerConfig cfg = FilesIn(dir);
    WorldImage image;
    image.fields = WorldStore::EmptyFields();
    image.start = image.fields;
    image.cycle = 5;
    image.players = { Record("Ana", 5), Record("Bob", 4) }; // Bob missed the last Song of Time
    std::string folder = dir.File("backups") + "/2026-01-01_00-00-00-manual";
    std::error_code ec;
    fs::create_directories(folder, ec);
    std::string err;
    CHECK(SaveWorldImage(image, folder + "/world.json", folder + "/players", &err));
    TestServer s(cfg);
    auto a = Join(s, "Ana");
    CreateWorld(s, *a); // the world now: cycle 1
    Drain(s, { a.get() });
    CHECK(!s.server->World().RestoreBackup("nada", "prueba", &err));
    CHECK(s.server->World().RestoreBackup("2026-01-01", "prueba", &err));
    auto full = a->WaitFor("world_full", s);
    CHECK(full.has_value());
    CHECK_EQ(GetString(*full, "reset"), std::string("restore"));
    CHECK_EQ(GetInt(*full, "cycle"), 2);
    CHECK_EQ((*full)["you"]["inv"], image.players[0].inv);
    CHECK(!GetBool((*full)["you"], "stale"));
    auto b = Join(s, "Bob");
    CHECK(GetBool(EnterWorld(s, *b)["you"], "stale")); // still one cycle behind
}

TEST_CASE(ServerLockNamesALiveServer) {
    TempDir dir("coop_test_save_lock");
    server::ServerConfig cfg = FilesIn(dir);
    std::string lock = dir.File("server.lock");
    uint32_t pid = 0;
    {
        TestServer s(cfg);
        CHECK(fs::exists(lock));
        CHECK(!ServerLockHeld(lock, &pid)); // this very process: not another server
    }
    CHECK(!fs::exists(lock)); // gone with the server
    std::ofstream(lock) << "2000000000"; // left by a process that no longer exists
    CHECK(!ServerLockHeld(lock, &pid));
#ifdef _WIN32
    std::ofstream(lock) << "4"; // the System process: alive, and not us
    CHECK(ServerLockHeld(lock, &pid));
    CHECK_EQ(pid, 4u);
#endif
    CHECK(!ServerLockHeld(dir.File("nada.lock"), &pid));
}

#ifdef _WIN32
TEST_CASE(ServerKeepsAnotherLiveServersLock) {
    TempDir dir("coop_test_save_twolocks");
    server::ServerConfig cfg = FilesIn(dir);
    std::string lock = dir.File("server.lock");
    std::ofstream(lock) << "4\n"; // a server already runs here (the System process stands in for it: alive, not us)
    uint32_t pid = 0;
    {
        TestServer s(cfg); // a second one started in the same folder by mistake
        CHECK(ServerLockHeld(lock, &pid));
        CHECK_EQ(pid, 4u);
        bool warned = false;
        for (const std::string& line : s.log.Lines()) {
            warned = warned || line.find(Tr(Msg::ServerLockOther, { "4" })) != std::string::npos;
        }
        CHECK(warned); // the admin is told
    }
    CHECK(ServerLockHeld(lock, &pid)); // still the first one's: the converter keeps refusing to write here
    CHECK_EQ(pid, 4u);
}
#endif

TEST_CASE(PeriodicBackupWhenTheWorldChanged) {
    TempDir dir("coop_test_save_periodic");
    server::ServerConfig cfg = FilesIn(dir);
    cfg.backupMs = 200; // server.json "backupMinutes" in minutes; tests go faster
    cfg.worldSaveMs = 50;
    TestServer s(cfg);
    auto a = Join(s, "Alice");
    CreateWorld(s, *a); // the clock runs: the world is saved again and again
    CHECK(a->WaitUntil(s, 3000, [&] { return !s.server->World().Backups().empty(); }));
    CHECK_EQ(s.server->World().Backups()[0].reason, std::string("auto"));
}

TEST_CASE(FailingBackupWaitsForTheNextPeriod) {
    TempDir dir("coop_test_save_backupfail");
    server::ServerConfig cfg = FilesIn(dir);
    cfg.backupMs = 300;
    cfg.worldSaveMs = 50;
    std::ofstream(dir.File("backups")) << "no es una carpeta"; // backups/<name>/ cannot be made
    TestServer s(cfg);
    auto a = Join(s, "Alice");
    CreateWorld(s, *a); // the clock runs: the world keeps changing
    s.PumpFor(1000);
    std::string failed = Tr(Msg::BackupFail, { "" });
    int failures = 0;
    for (const std::string& line : s.log.Lines()) {
        failures += line.find(failed) != std::string::npos ? 1 : 0;
    }
    CHECK(failures >= 1); // it tried
    CHECK(failures <= 4); // about once a period (3 in a second), never on every tick
}

TEST_CASE(BackupOptionsInServerJson) {
    TempDir dir("coop_cfg_backups");
    std::string path = dir.File("server.json");
    std::string warn;
    server::ServerConfig cfg;
    CHECK(server::LoadOrCreateConfig(path, cfg, &warn));
    CHECK_EQ(cfg.backupMs, 30 * 60000);
    CHECK_EQ(cfg.backupKeep, 20);
    json file;
    CHECK(LoadJsonFile(path, file, nullptr));
    CHECK_EQ(file["backupMinutes"].get<int>(), 30);
    CHECK_EQ(file["backupKeep"].get<int>(), 20);
    std::ofstream(path) << R"({"backupMinutes": 0, "backupKeep": 5})";
    server::ServerConfig off;
    CHECK(server::LoadOrCreateConfig(path, off, &warn));
    CHECK_EQ(off.backupMs, 0); // no periodic copies (the others still happen)
    CHECK_EQ(off.backupKeep, 5);
    std::ofstream(path) << R"({"backupMinutes": -3, "backupKeep": 0})";
    server::ServerConfig bad;
    warn.clear();
    CHECK(server::LoadOrCreateConfig(path, bad, &warn));
    CHECK_EQ(bad.backupMs, 30 * 60000);
    CHECK_EQ(bad.backupKeep, 20);
    CHECK(!warn.empty());
}
