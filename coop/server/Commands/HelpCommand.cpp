// /help: lists only the commands the caller may use.
#include "server/CommandRegistry.h"
#include "server/Server.h"

namespace coop::server {

static void Run(CommandContext& ctx, const std::vector<std::string>& args) {
    std::string text = Tr(Msg::HelpTitle);
    for (const CommandDef* def : AllCommands()) {
        if (HasPermission(ctx.server, ctx.sender, def->perm)) {
            text += "\n" + Tr(def->usage) + " - " + Tr(def->help);
        }
    }
    ctx.Reply(text);
}

COOP_COMMAND(help, "help", Msg::HelpUsage, Msg::HelpHelp, Perm::Player, 0, Run);

} // namespace coop::server
