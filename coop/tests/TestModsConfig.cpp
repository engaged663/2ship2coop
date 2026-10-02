// server.json: the mod section and the new server parameters; the command line.
#include "TestWorld.h"

#include "common/ModRules.h"
#include "common/Protocol.h"
#include "server/ServerConfig.h"

#include <fstream>
#include <sstream>

using namespace coop;
using namespace coop_test;

namespace {

void WriteText(const std::string& path, const std::string& text) {
    std::ofstream f(path, std::ios::binary);
    f << text;
}

json ReadJson(const std::string& path) {
    std::ifstream f(path);
    return json::parse(f, nullptr, false);
}

} // namespace

TEST_CASE(ConfigIsCreatedWithTheModSection) {
    TempDir dir("coop_cfg_new");
    std::string path = dir.File("server.json");
    server::ServerConfig cfg;
    std::string warn;
    CHECK(server::LoadOrCreateConfig(path, cfg, &warn));
    CHECK(cfg.mods.enabled);
    CHECK_EQ(cfg.mods.scriptsDir, std::string("mods"));
    CHECK_EQ(cfg.mods.scripts.size(), (size_t)1);
    CHECK_EQ(cfg.mods.scripts[0], std::string("*"));
    CHECK_EQ(cfg.mods.pluginsDir, std::string("plugins"));
    CHECK_EQ(cfg.mods.dataDir, std::string("mods/data"));
    CHECK_EQ(cfg.timeSpeed, 1.0);
    json file = ReadJson(path);
    CHECK(file["mods"].is_object());
    CHECK_EQ(file["mods"]["scripts"][0].get<std::string>(), std::string("*"));
    CHECK_EQ(file["voteSeconds"].get<int>(), 30);
    CHECK(file["gameSettings"].is_object());
    CHECK(file["commandPermissions"].is_object());
}

TEST_CASE(ConfigReadsTheNewKeys) {
    TempDir dir("coop_cfg_read");
    std::string path = dir.File("server.json");
    WriteText(path, R"({
        "timeSpeed": 0.5, "voteSeconds": 60, "saveSeconds": 5, "giftMax": 100,
        "commandPermissions": { "tp": "op", "GIFT": "console" },
        "gameSettings": { "gCheats.InfiniteMagic": 1, "gEnhancements.DifficultyOptions.DamageMultiplier": 2 },
        "mods": { "enabled": false, "scriptsDir": "misMods", "scripts": ["a.lua", "!b.lua"], "pluginsDir": "dll",
                  "plugins": [], "dataDir": "datos", "unsafeLua": true, "scriptTimeoutMs": 500,
                  "scriptMemoryMb": 16, "settings": { "a": { "x": 1 } } }
    })");
    server::ServerConfig cfg;
    std::string warn;
    CHECK(server::LoadOrCreateConfig(path, cfg, &warn));
    CHECK(warn.empty());
    CHECK_EQ(cfg.timeSpeed, 0.5);
    CHECK_EQ(cfg.voteTimeoutMs, 60000);
    CHECK_EQ(cfg.worldSaveMs, 5000);
    CHECK_EQ(cfg.giftMax, 100);
    CHECK_EQ(cfg.commandPermissions.at("tp"), std::string("op"));
    CHECK_EQ(cfg.commandPermissions.at("gift"), std::string("console")); // names in lowercase
    CHECK_EQ(cfg.gameSettings["gCheats.InfiniteMagic"].get<int>(), 1);
    CHECK(!cfg.mods.enabled);
    CHECK_EQ(cfg.mods.scriptsDir, std::string("misMods"));
    CHECK_EQ(cfg.mods.scripts.size(), (size_t)2);
    CHECK(cfg.mods.plugins.empty());
    CHECK_EQ(cfg.mods.dataDir, std::string("datos"));
    CHECK(cfg.mods.unsafeLua);
    CHECK_EQ(cfg.mods.scriptTimeoutMs, 500);
    CHECK_EQ(cfg.mods.scriptMemoryMb, 16);
    CHECK_EQ(cfg.mods.settings["a"]["x"].get<int>(), 1);
}

TEST_CASE(ConfigWithBadNewKeysKeepsDefaultsAndWarns) {
    TempDir dir("coop_cfg_bad");
    std::string path = dir.File("server.json");
    WriteText(path, R"({
        "timeSpeed": "rapido", "voteSeconds": 1, "giftMax": 5000, "commandPermissions": { "tp": "nadie" },
        "gameSettings": { "gSettings.Fullscreen": 1, "gCheats.MoonJumpOnL": "si" },
        "mods": { "scripts": "todos", "scriptTimeoutMs": 5, "settings": [] }
    })");
    server::ServerConfig cfg;
    std::string warn;
    CHECK(server::LoadOrCreateConfig(path, cfg, &warn));
    CHECK(!warn.empty());
    CHECK_EQ(cfg.timeSpeed, 1.0);
    CHECK_EQ(cfg.voteTimeoutMs, kSotVoteMs);
    CHECK_EQ(cfg.giftMax, kGiftMaxAmount);
    CHECK(cfg.commandPermissions.empty());
    CHECK(cfg.gameSettings.empty());
    CHECK_EQ(cfg.mods.scripts.size(), (size_t)1); // the default list
    CHECK_EQ(cfg.mods.scriptTimeoutMs, 2000);
    CHECK(cfg.mods.settings.is_object());
}

TEST_CASE(OldConfigFilesGainTheNewKeysOnce) {
    TempDir dir("coop_cfg_old");
    std::string path = dir.File("server.json");
    WriteText(path, "{\"port\": 7000, \"motd\": \"hola\"}");
    server::ServerConfig cfg;
    std::string warn;
    CHECK(server::LoadOrCreateConfig(path, cfg, &warn));
    CHECK_EQ(cfg.port, (uint16_t)7000);
    json file = ReadJson(path);
    CHECK_EQ(file["port"].get<int>(), 7000);
    CHECK_EQ(file["motd"].get<std::string>(), std::string("hola"));
    CHECK(file.contains("timeSpeed"));
    CHECK(file["mods"].contains("scripts"));
    CHECK(!file.contains("hostToken")); // never written: it is a secret for tests
    auto stamp = std::filesystem::last_write_time(path);
    CHECK(server::LoadOrCreateConfig(path, cfg, &warn));
    CHECK(std::filesystem::last_write_time(path) == stamp); // nothing missing: not rewritten
}

TEST_CASE(CommandLineAddsScriptsAndPlugins) {
    server::ServerConfig cfg;
    cfg.mods.scripts = { "*" };
    const char* argv[] = { "server", "--port", "7001", "--mods-dir", "m", "--plugins-dir", "p", "--script",
                           "uno.lua", "--script", "dos.lua", "--plugin", "x.dll", "--mod-docs", "docs" };
    server::CommandLine line;
    std::string warn = server::ApplyCommandLine(cfg, (int)std::size(argv), (char**)argv, &line);
    CHECK(warn.empty());
    CHECK_EQ(cfg.port, (uint16_t)7001);
    CHECK_EQ(cfg.mods.scriptsDir, std::string("m"));
    CHECK_EQ(cfg.mods.pluginsDir, std::string("p"));
    CHECK_EQ(cfg.mods.scripts.size(), (size_t)3);
    CHECK_EQ(cfg.mods.scripts[2], std::string("dos.lua"));
    CHECK_EQ(cfg.mods.plugins.back(), std::string("x.dll"));
    CHECK_EQ(line.modDocsDir, std::string("docs"));

    const char* off[] = { "server", "--no-mods", "--port", "abc", "--script" };
    server::ServerConfig other;
    warn = server::ApplyCommandLine(other, (int)std::size(off), (char**)off, &line);
    CHECK(!other.mods.enabled);
    CHECK(!warn.empty()); // the bad port and the --script without a file
}

TEST_CASE(OnlyGameplaySettingsMayBeForced) {
    CHECK(mods::SettingAllowed("gEnhancements.DifficultyOptions.DamageMultiplier"));
    CHECK(mods::SettingAllowed("gCheats.InfiniteHealth"));
    CHECK(mods::SettingAllowed("gModes.PlayAsKafei"));
    CHECK(!mods::SettingAllowed("gSettings.Fullscreen"));
    CHECK(!mods::SettingAllowed("gCoop.Nick"));
    CHECK(!mods::SettingAllowed("gEnhancements.A11y.NoScreenDistortion"));
    CHECK(!mods::SettingAllowed("gEnhancements.Camera.FreeLook.Enable"));
    CHECK(!mods::SettingAllowed("gEnhancements.Saving.Autosave"));
    CHECK(!mods::SettingAllowed("gEnhancements.DifficultyOptions.DeleteFileOnDeath")); // it erases a player's file
    CHECK(mods::SettingAllowed("gEnhancements.DifficultyOptions.DamageMultiplier"));
    CHECK(!mods::SettingAllowed("gEnhancements."));
    CHECK(!mods::SettingAllowed("gCheats.Bad Name"));
    CHECK(!mods::SettingAllowed(std::string(200, 'a')));
    CHECK(mods::SettingValueAllowed(json(3)));
    CHECK(mods::SettingValueAllowed(json(0.5)));
    CHECK(!mods::SettingValueAllowed(json("x")));
    CHECK(!mods::SettingValueAllowed(json(1e9)));
}

TEST_CASE(OnlyItemsTheGameKnowsHowToGiveMayBeGiven) {
    // SWORD_DEITY, WALLET_DEFAULT, FISHING_ROD, 0x71, 0x72, STRAY_FAIRIES, INVALID_1..7: the game's Item_Give has no
    // case for them and would write into the inventory through a slot read past the end of its table.
    for (int id : { 0x50, 0x59, 0x5C, 0x71, 0x72, 0x77, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82 }) {
        CHECK(!mods::ItemGivable(id));
    }
    CHECK(mods::ItemGivable(0x00)); // OCARINA_OF_TIME
    CHECK(mods::ItemGivable(0x39)); // MASK_BUNNY
    CHECK(mods::ItemGivable(0x4F)); // SWORD_GILDED
    CHECK(mods::ItemGivable(0x73)); // SONG_LULLABY_INTRO
    CHECK(mods::ItemGivable(0x8A)); // RUPEE_HUGE
    CHECK(mods::ItemGivable(kLastModItem)); // SEAHORSE_CAUGHT
    CHECK(!mods::ItemGivable(kLastModItem + 1)); // the map's points
    CHECK(!mods::ItemGivable(0xFF));             // ITEM_NONE
    CHECK(!mods::ItemGivable(-1));
}
