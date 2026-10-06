// Saving commands (server/Commands/SaveCommands.cpp: /backup, /restaurar, /importar) and the admin commands that only
// make sense in the server's world (/unlockall, /give).
#include "TestSaves.h"
#include "TestWorld.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace coop;
using namespace coop_test;
namespace fs = std::filesystem;

namespace {

server::ServerConfig FilesIn(const TempDir& dir) {
    server::ServerConfig cfg;
    cfg.worldPath = dir.File("world.json");
    cfg.playersDir = dir.File("players");
    return cfg;
}

// The next "sys" message whose text contains needle (older ones are dropped).
bool WaitForText(TestServer& s, TestClient& c, const std::string& needle) {
    return c.WaitUntil(s, 1000, [&] {
        while (auto ev = c.TakeEvent("sys")) {
            if (GetString(*ev, "text").find(needle) != std::string::npos) {
                return true;
            }
        }
        return false;
    });
}

uint8_t WeekByte(const json& full, int index) {
    std::vector<uint8_t> week;
    CHECK(FromHex(full["fields"]["weekEventReg"].get<std::string>(), week));
    return week[(size_t)index];
}

} // namespace

TEST_CASE(UnlockAllAndGiveNeedThePlayerInTheWorld) {
    TestServer s;
    auto a = Join(s, "Alice");
    s.server->ExecuteConsoleLine("op Alice");
    Drain(s, { a.get() });
    a->Cmd("/unlockall Alice"); // Alice plays her own save: nothing may reach it
    CHECK(a->WaitForSys("error", s));
    a->Cmd("/give Alice 10");
    CHECK(a->WaitForSys("error", s));
    s.PumpFor(100);
    CHECK(!a->TakeEvent("unlock_all").has_value());
    CHECK(!a->TakeEvent("give").has_value());
    CreateWorld(s, *a);
    Drain(s, { a.get() });
    a->Cmd("/unlockall Alice");
    CHECK(a->WaitFor("unlock_all", s).has_value());
    CHECK(WaitForText(s, *a, "Alice")); // everyone in the world hears who unlocked everything
    a->Cmd("/give Alice 10");
    CHECK(a->WaitFor("give", s).has_value());
}

TEST_CASE(BackupCommandMakesAndLists) {
    TempDir dir("coop_test_cmd_backup");
    TestServer s(FilesIn(dir));
    auto a = Join(s, "Alice");
    CreateWorld(s, *a);
    Drain(s, { a.get() });
    a->Cmd("/backup");
    CHECK(a->WaitForSys("error", s)); // only admins
    CHECK(s.server->World().Backups().empty());
    s.server->ExecuteConsoleLine("op Alice");
    Drain(s, { a.get() }); // the "you are an admin" notice
    a->Cmd("/backup");
    CHECK(WaitForText(s, *a, "Copia de seguridad guardada"));
    std::vector<server::BackupInfo> backups = s.server->World().Backups();
    CHECK_EQ(backups.size(), (size_t)1);
    CHECK_EQ(backups[0].reason, std::string("manual"));
    a->Cmd("/backup lista");
    CHECK(WaitForText(s, *a, backups[0].name));
}

TEST_CASE(RestoreCommandFromTheConsole) {
    TempDir dir("coop_test_cmd_restore");
    TestServer s(FilesIn(dir));
    auto a = Join(s, "Alice");
    CreateWorld(s, *a);
    s.server->ExecuteConsoleLine("backup"); // the world as it was created
    a->Send({ { "t", "wops" }, { "cycle", 1 }, { "bits", json::array({ Arr(Field("weekEventReg"), 3, 0x08, 0) }) } });
    s.PumpFor(100);
    s.server->ExecuteConsoleLine("op Alice");
    Drain(s, { a.get() });
    a->Cmd("/restaurar ultima");
    CHECK(a->WaitForSys("error", s)); // the console's alone: it replaces the world
    s.server->ExecuteConsoleLine("restaurar ultima");
    auto full = a->WaitFor("world_full", s);
    CHECK(full.has_value());
    CHECK_EQ(GetString(*full, "reset"), std::string("restore"));
    CHECK_EQ(WeekByte(*full, 3), (uint8_t)0); // back to before the change
    s.server->ExecuteConsoleLine("restore nada"); // the English name, and a backup that does not exist
    s.PumpFor(100);
    CHECK(!a->TakeEvent("world_full").has_value());
}

TEST_CASE(ImportCommandFromTheConsole) {
    TempDir dir("coop_test_cmd_import");
    json save = MakeSave();
    save["saveInfo"]["weekEventReg"][40] = 0x40;
    std::string path = dir.File("file1.json");
    std::ofstream(path) << MakeFile(save).dump();
    TestServer s(FilesIn(dir));
    auto a = Join(s, "Alice");
    CreateWorld(s, *a);
    s.server->ExecuteConsoleLine("op Alice");
    Drain(s, { a.get() });
    a->Cmd("/importar " + path);
    CHECK(a->WaitForSys("error", s)); // the console's alone: it reads the server's files
    s.server->ExecuteConsoleLine("importar \"" + path + "\" Bob");
    auto full = a->WaitFor("world_full", s);
    CHECK(full.has_value());
    CHECK_EQ(GetString(*full, "reset"), std::string("import"));
    CHECK_EQ(WeekByte(*full, 40), (uint8_t)0x40);
    CHECK(fs::exists(dir.path / "players" / "bob.json"));
    // Without a nick: the save's own name ("Link"); the part may come second
    s.server->ExecuteConsoleLine("import \"" + path + "\" ciclo");
    CHECK(a->WaitFor("world_full", s).has_value());
    CHECK(fs::exists(dir.path / "players" / "link.json"));
    // A bad part, a bad nick, a file that is not a save: nothing changes
    Drain(s, { a.get() });
    std::ofstream(dir.File("otro.json")) << "{\"a\":1}";
    s.server->ExecuteConsoleLine("importar \"" + path + "\" Bob raro");
    s.server->ExecuteConsoleLine("importar \"" + path + "\" no-vale");
    s.server->ExecuteConsoleLine("importar \"" + dir.File("otro.json") + "\" Bob");
    s.server->ExecuteConsoleLine("importar \"" + dir.File("nada.json") + "\" Bob");
    s.PumpFor(150);
    CHECK(!a->TakeEvent("world_full").has_value());
}
