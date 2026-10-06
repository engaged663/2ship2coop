// WorldBackups: copies of world.json and players/ in backups/<stamp>-<reason>/ (server/World/WorldBackups.h).
#include "TestWorld.h"

#include "server/World/WorldBackups.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace coop;
using namespace coop::server;
using namespace coop_test;
namespace fs = std::filesystem;

namespace {

void WriteText(const fs::path& path, const std::string& text) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream(path) << text;
}

std::string ReadText(const fs::path& path) {
    std::ifstream in(path);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

} // namespace

TEST_CASE(BackupsCopyWorldAndPlayers) {
    TempDir dir("coop_test_backups_copy");
    WriteText(dir.path / "world.json", "{\"w\":1}");
    WriteText(dir.path / "players" / "ana.json", "{\"p\":1}");
    WriteText(dir.path / "players" / "bob.json.old", "{}"); // set-aside files stay out
    WorldBackups backups((dir.path / "backups").string(), 20);
    CHECK(backups.Enabled());
    std::string name;
    std::string err;
    CHECK(backups.Make(dir.File("world.json"), dir.File("players"), "manual", &name, &err));
    CHECK(name.find("-manual") != std::string::npos);
    CHECK_EQ(ReadText(backups.WorldFile(name)), std::string("{\"w\":1}"));
    CHECK_EQ(ReadText(fs::path(backups.PlayersDir(name)) / "ana.json"), std::string("{\"p\":1}"));
    CHECK(!fs::exists(fs::path(backups.PlayersDir(name)) / "bob.json.old"));
    std::vector<BackupInfo> list = backups.List();
    CHECK_EQ(list.size(), (size_t)1);
    CHECK_EQ(list[0].name, name);
    CHECK_EQ(list[0].reason, std::string("manual"));
    CHECK_EQ(backups.Newest(), name);
    std::string again; // two in the same second never share a folder
    CHECK(backups.Make(dir.File("world.json"), dir.File("players"), "manual", &again, &err));
    CHECK(again != name);
    CHECK_EQ(backups.Newest(), again);
    // Without a world.json there is nothing to copy; without a folder there are no backups at all
    CHECK(!backups.Make(dir.File("nada.json"), dir.File("players"), "auto", &name, &err));
    CHECK(!err.empty());
    WorldBackups off("", 20);
    CHECK(!off.Enabled());
    CHECK(!off.Make(dir.File("world.json"), dir.File("players"), "auto", &name, &err));
    CHECK(off.List().empty());
}

TEST_CASE(BackupsKeepTheNewestButManual) {
    TempDir dir("coop_test_backups_keep");
    WriteText(dir.path / "world.json", "{}");
    WorldBackups backups((dir.path / "backups").string(), 2);
    std::string manual;
    std::string err;
    CHECK(backups.Make(dir.File("world.json"), dir.File("players"), "manual", &manual, &err));
    std::vector<std::string> autos;
    for (int i = 0; i < 3; i++) {
        std::string name;
        CHECK(backups.Make(dir.File("world.json"), dir.File("players"), "auto", &name, &err));
        autos.push_back(name);
    }
    std::vector<BackupInfo> list = backups.List();
    CHECK_EQ(list.size(), (size_t)3); // the two newest automatic ones and the manual one
    CHECK_EQ(list[0].name, autos[2]); // newest first
    CHECK_EQ(list[1].name, autos[1]);
    CHECK_EQ(list[2].name, manual);
    CHECK(!fs::exists(fs::path(dir.path / "backups") / autos[0]));
}

TEST_CASE(BackupsResolveNames) {
    TempDir dir("coop_test_backups_resolve");
    WriteText(dir.path / "world.json", "{}");
    WorldBackups backups((dir.path / "backups").string(), 20);
    std::string manual;
    std::string moon;
    std::string err;
    CHECK(backups.Make(dir.File("world.json"), dir.File("players"), "manual", &manual, &err));
    CHECK(backups.Make(dir.File("world.json"), dir.File("players"), "moon", &moon, &err));
    CHECK_EQ(backups.Resolve("ultima"), moon);
    CHECK_EQ(backups.Resolve("LAST"), moon);
    CHECK_EQ(backups.Resolve("latest"), moon);
    CHECK_EQ(backups.Resolve(manual), manual);
    CHECK_EQ(backups.Resolve("nada"), std::string());
    CHECK_EQ(backups.Resolve(manual.substr(0, 10)), std::string()); // both start with today's date: ambiguous
    CHECK_EQ(backups.Resolve(manual.substr(0, manual.size() - 2)), manual); // "...-manu": only one
}

TEST_CASE(BackupsSameAsNewest) {
    TempDir dir("coop_test_backups_same");
    WriteText(dir.path / "world.json", "{\"a\":1}");
    WorldBackups backups((dir.path / "backups").string(), 20);
    CHECK(!backups.SameAsNewest(dir.File("world.json"))); // no backups yet
    std::string name;
    std::string err;
    CHECK(backups.Make(dir.File("world.json"), dir.File("players"), "start", &name, &err));
    CHECK(backups.SameAsNewest(dir.File("world.json")));
    WriteText(dir.path / "world.json", "{\"a\":2}");
    CHECK(!backups.SameAsNewest(dir.File("world.json")));
}
