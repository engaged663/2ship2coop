// Saving commands: /backup [lista] (a copy of the world now, or the list), /restaurar <copia|ultima> (put a backup in
// place of the world) and /importar <archivo> [nick] [auto|buho|ciclo] (a base-game save as the server's world). The
// last two read files of the server's PC and replace the world: console only unless commandPermissions says else.
#include "server/CommandRegistry.h"
#include "server/Server.h"
#include "server/World/JsonFile.h"
#include "server/World/WorldImage.h"

#include "common/SaveImport.h"
#include "common/Text.h"

namespace coop::server {

namespace {

constexpr size_t kBackupsListed = 10;

void Backup(CommandContext& ctx, const std::vector<std::string>& args) {
    SharedWorld& world = ctx.server.World();
    if (!args.empty() && (ToLower(args[0]) == "lista" || ToLower(args[0]) == "list")) {
        std::vector<BackupInfo> backups = world.Backups();
        if (backups.empty()) {
            ctx.Reply(Tr(Msg::BackupNone), level::kInfo);
            return;
        }
        std::string text = Tr(Msg::BackupListHead);
        for (size_t i = 0; i < backups.size() && i < kBackupsListed; i++) {
            text += "\n  " + backups[i].name;
        }
        ctx.Reply(text, level::kInfo);
        return;
    }
    std::string err;
    std::string name = world.MakeBackup("manual", &err);
    if (name.empty()) {
        ctx.Reply(Tr(Msg::BackupFail, { err }), level::kError);
        return;
    }
    ctx.Reply(Tr(Msg::BackupMade, { name }), level::kOk);
}

void Restore(CommandContext& ctx, const std::vector<std::string>& args) {
    std::string err;
    if (!ctx.server.World().RestoreBackup(args[0], ctx.SenderName(), &err)) {
        ctx.Reply(err, level::kError);
    }
}

void Import(CommandContext& ctx, const std::vector<std::string>& args) {
    // /importar <archivo> [nick] [parte]: the part may also come second when the nick is the save's own name
    std::string nick;
    save::ImportOptions opts;
    for (size_t i = 1; i < args.size(); i++) {
        save::ImportSource source;
        if (save::ParseSource(args[i], source)) {
            opts.source = source;
        } else if (i == 1) {
            nick = args[i];
        } else {
            ctx.Reply(Tr(Msg::ImportBadSource, { SanitizeChat(args[i], 40) }), level::kError);
            return;
        }
    }
    json file;
    std::string err;
    if (!LoadJsonFile(args[0], file, &err)) {
        ctx.Reply(err, level::kError);
        return;
    }
    save::Imported imported;
    if (!save::ImportSave(file, opts, imported, &err)) {
        ctx.Reply(err, level::kError);
        return;
    }
    if (nick.empty()) {
        if (!IsValidNick(imported.playerName)) {
            ctx.Reply(Tr(Msg::ImportNeedNick, { imported.playerName }), level::kError);
            return;
        }
        nick = imported.playerName;
    } else if (!IsValidNick(nick)) {
        ctx.Reply(Tr(Msg::BadNick, { SanitizeChat(nick, 40) }) + " " + Tr(Msg::RejectBadNick), level::kError);
        return;
    }
    for (const std::string& line : save::DescribeImport(imported)) {
        ctx.Reply(line, level::kInfo);
    }
    if (!ctx.server.World().Replace(ImageFromImport(imported, nick), "import", ctx.SenderName(), nick, &err)) {
        ctx.Reply(err, level::kError);
        return;
    }
    ctx.Reply(Tr(Msg::ImportDone, { nick }), level::kOk);
}

} // namespace

COOP_COMMAND(backup, "backup", Msg::BackupUsage, Msg::BackupHelp, Perm::Op, 0, Backup);
COOP_COMMAND(restaurar, "restaurar", Msg::RestoreUsage, Msg::RestoreHelp, Perm::Console, 1, Restore);
COOP_ALIAS(restore, "restore", "restaurar");
COOP_COMMAND(importar, "importar", Msg::ImportUsage, Msg::ImportHelp, Perm::Console, 1, Import);
COOP_ALIAS(importAlias, "import", "importar");

} // namespace coop::server
