// /help: lists only the commands the caller may use.
#include "server/CommandRegistry.h"
#include "server/Server.h"

namespace coop::server {

static void Run(CommandContext& ctx, const std::vector<std::string>& args) {
    std::string text = "Comandos disponibles:";
    for (const CommandDef* def : AllCommands()) {
        if (HasPermission(ctx.server, ctx.sender, def->perm)) {
            text += "\n" + def->usage + " - " + def->help;
        }
    }
    ctx.Reply(text);
}

COOP_COMMAND(help, "help", "/help", "muestra esta lista", Perm::Player, 0, Run);

} // namespace coop::server
