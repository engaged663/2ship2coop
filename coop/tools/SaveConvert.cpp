// 2ship-coop-convert: a base-game save (2 Ship's saves/fileN.json) into the co-op server's world.json and
// players/<nick>.json (common/SaveImport.h does the conversion, server/World/WorldImage.h writes the files).
// Drag the save onto this exe in the server's folder, or:
//   2ship-coop-convert <save.json> [--nick N] [--out DIR] [--source auto|owl|cycle] [--player-only] [--info] [--yes]
//                      [--force] [--lang es|en|zh|ru]
// It never writes into the folder of a running server (server.lock: use /importar in its console instead), and what
// was there goes to backups/ first.
#include "server/World/JsonFile.h"
#include "server/World/PlayerStore.h"
#include "server/World/ServerLock.h"
#include "server/World/WorldBackups.h"
#include "server/World/WorldImage.h"

#include "common/I18n.h"
#include "common/SaveImport.h"
#include "common/Text.h"

#include <filesystem>
#include <iostream>
#include <limits>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

using namespace coop;
using namespace coop::server;
namespace fs = std::filesystem;

namespace {

struct Args {
    std::string save;
    std::string nick;
    std::string out;
    save::ImportSource source = save::ImportSource::Auto;
    bool playerOnly = false;
    bool info = false;
    bool yes = false;
    bool force = false;
    bool interactive = false; // dropped onto the exe (or started without anything): it asks and waits for Enter
};

// This program's folder: a server's files are there when it sits next to 2ship-coop-server.exe.
std::string ExeFolder(const char* argv0) {
#ifdef _WIN32
    wchar_t buffer[MAX_PATH];
    DWORD size = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (size > 0 && size < MAX_PATH) {
        return fs::path(std::wstring(buffer, size)).parent_path().string();
    }
#endif
    std::error_code ec;
    return fs::absolute(argv0, ec).parent_path().string();
}

// --lang, else the language of the server.json in the output folder, else Spanish.
void PickLanguage(int argc, char** argv) {
    std::string lang;
    std::string out;
    for (int i = 1; i + 1 < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--lang") {
            lang = argv[i + 1];
        } else if (arg == "--out") {
            out = argv[i + 1];
        }
    }
    Lang chosen = Lang::Es;
    if (!lang.empty() && ParseLang(lang, chosen)) {
        SetLang(chosen);
        return;
    }
    json config;
    std::string folder = out.empty() ? ExeFolder(argv[0]) : out;
    if (LoadJsonFile((fs::path(folder) / "server.json").string(), config, nullptr) &&
        ParseLang(GetString(config, "language"), chosen)) {
        SetLang(chosen);
    }
}

bool ParseArgs(int argc, char** argv, Args& out, std::string& err) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        auto value = [&](std::string& into) {
            if (i + 1 >= argc) {
                err = Tr(Msg::CvBadArg, { arg });
                return false;
            }
            into = argv[++i];
            return true;
        };
        std::string ignored;
        if (arg == "--nick") {
            if (!value(out.nick)) {
                return false;
            }
        } else if (arg == "--out") {
            if (!value(out.out)) {
                return false;
            }
        } else if (arg == "--lang") {
            if (!value(ignored)) { // already applied by PickLanguage
                return false;
            }
        } else if (arg == "--source") {
            std::string text;
            if (!value(text)) {
                return false;
            }
            if (!save::ParseSource(text, out.source)) {
                err = Tr(Msg::ImportBadSource, { text });
                return false;
            }
        } else if (arg == "--player-only") {
            out.playerOnly = true;
        } else if (arg == "--info") {
            out.info = true;
        } else if (arg == "--yes") {
            out.yes = true;
        } else if (arg == "--force") {
            out.force = true;
        } else if (!arg.empty() && arg[0] != '-' && out.save.empty()) {
            out.save = arg;
        } else {
            err = Tr(Msg::CvBadArg, { arg });
            return false;
        }
    }
    out.interactive = argc <= 2; // just the save (drag and drop) or nothing at all (double click)
    return true;
}

std::string AskLine(const std::string& question) {
    std::cout << question << std::flush;
    std::string line;
    std::getline(std::cin, line);
    size_t first = line.find_first_not_of(" \t\r");
    size_t last = line.find_last_not_of(" \t\r");
    return first == std::string::npos ? "" : line.substr(first, last - first + 1);
}

// s, y (any case) or the first letter of "да" / "是": what people answer to (s/n) or (y/n) in the four languages.
bool IsYes(const std::string& answer) {
    if (answer.empty()) {
        return false;
    }
    char c = answer[0];
    return c == 's' || c == 'S' || c == 'y' || c == 'Y' || answer.rfind("д", 0) == 0 || answer.rfind("Д", 0) == 0 ||
           answer.rfind("是", 0) == 0;
}

int Finish(const Args& args, int code) {
    if (args.interactive) {
        AskLine(Tr(Msg::CvPressEnter)); // the window of a dragged file would close before anyone read it
    }
    return code;
}

int Fail(const Args& args, const std::string& why) {
    std::cerr << why << std::endl;
    return Finish(args, 1);
}

// Copies what is in the folder before anything is overwritten. False: no copy could be made (then nothing is written).
bool BackupFirst(const fs::path& out, const fs::path& worldPath, const fs::path& playersDir, std::string& err) {
    std::error_code ec;
    if (!fs::exists(worldPath, ec)) {
        return true; // nothing to keep
    }
    // The server prunes its backups with its own backupKeep: this copy never removes any
    WorldBackups backups((out / "backups").string(), std::numeric_limits<int>::max());
    std::string name;
    if (!backups.Make(worldPath.string(), playersDir.string(), "import", &name, &err)) {
        return false;
    }
    std::cout << Tr(Msg::CvBackup, { (out / "backups" / name).string() }) << std::endl;
    return true;
}

int Run(int argc, char** argv) {
    PickLanguage(argc, argv);
    Args args;
    std::string err;
    if (!ParseArgs(argc, argv, args, err)) {
        std::cerr << err << "\n\n" << Tr(Msg::CvUsage) << std::endl;
        return 2;
    }
    if (args.save.empty()) {
        std::cout << Tr(Msg::CvUsage) << std::endl;
        return Finish(args, 2);
    }
    fs::path out = args.out.empty() ? fs::path(ExeFolder(argv[0])) : fs::path(args.out);
    fs::path worldPath = out / "world.json";
    fs::path playersDir = out / "players";

    std::cout << Tr(Msg::CvReading, { args.save }) << std::endl;
    json file;
    if (!LoadJsonFile(args.save, file, &err)) {
        return Fail(args, Tr(Msg::CvNoFile, { args.save, err }));
    }
    save::ImportOptions opts;
    opts.source = args.source;
    save::Imported imported;
    if (!save::ImportSave(file, opts, imported, &err)) {
        return Fail(args, err);
    }
    for (const std::string& line : save::DescribeImport(imported)) {
        std::cout << "  " << line << std::endl;
    }
    if (args.info) {
        return Finish(args, 0);
    }

    uint32_t pid = 0;
    if (!args.force && ServerLockHeld((out / "server.lock").string(), &pid)) {
        return Fail(args, Tr(Msg::CvServerRunning, { out.string(), std::to_string(pid) }));
    }

    std::string nick = args.nick;
    if (nick.empty()) {
        std::string suggested = IsValidNick(imported.playerName) ? imported.playerName : "";
        nick = args.interactive ? AskLine(Tr(Msg::CvAskNick, { suggested })) : "";
        if (nick.empty()) {
            nick = suggested;
        }
    }
    if (!IsValidNick(nick)) {
        return Fail(args, nick.empty() ? Tr(Msg::ImportNeedNick, { imported.playerName })
                                       : Tr(Msg::BadNick, { nick }) + " " + Tr(Msg::RejectBadNick));
    }

    std::error_code ec;
    if (args.playerOnly) {
        // Only this player's file, for the world that is already there (a friend's own items)
        WorldImage current;
        if (!LoadWorldImage(worldPath.string(), playersDir.string(), current, nullptr, &err)) {
            return Fail(args, Tr(Msg::CvNeedWorld, { out.string(), err }));
        }
        if (!BackupFirst(out, worldPath, playersDir, err)) {
            return Fail(args, Tr(Msg::BackupFail, { err }));
        }
        PlayerRecord record;
        record.nick = nick;
        record.inv = imported.inv;
        record.cycle = current.cycle;
        record.start = imported.invStart;
        record.startCycle = current.cycle;
        PlayerStore players(playersDir.string());
        players.LoadAll(nullptr);
        players.Set(record);
        if (!players.SaveAll(&err)) {
            return Fail(args, Tr(Msg::CvFailed, { err }));
        }
        std::cout << Tr(Msg::CvWritten, { (playersDir / (ToLower(nick) + ".json")).string() }) << std::endl;
        std::cout << Tr(Msg::CvDonePlayer, { nick, std::to_string(current.cycle) }) << std::endl;
        return Finish(args, 0);
    }

    if (fs::exists(worldPath, ec) && !args.yes) {
        if (!args.interactive) {
            return Fail(args, Tr(Msg::CvNeedYes, { out.string() }));
        }
        if (!IsYes(AskLine(Tr(Msg::CvAskReplace, { out.string() })))) {
            std::cout << Tr(Msg::CvCancelled) << std::endl;
            return Finish(args, 1);
        }
    }
    if (!BackupFirst(out, worldPath, playersDir, err)) {
        return Fail(args, Tr(Msg::BackupFail, { err }));
    }
    fs::create_directories(out, ec);
    if (!SaveWorldImage(ImageFromImport(imported, nick), worldPath.string(), playersDir.string(), &err)) {
        return Fail(args, Tr(Msg::CvFailed, { err }));
    }
    std::cout << Tr(Msg::CvWritten, { worldPath.string() }) << std::endl;
    std::cout << Tr(Msg::CvWritten, { (playersDir / (ToLower(nick) + ".json")).string() }) << std::endl;
    std::cout << Tr(Msg::CvDone) << std::endl;
    return Finish(args, 0);
}

} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    return Run(argc, argv);
}
