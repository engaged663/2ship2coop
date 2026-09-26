// /pm <jugador> <mensaje>: private message (echoed back to the sender).
#include "server/CommandRegistry.h"
#include "server/Server.h"

#include "common/Events.h"
#include "common/Text.h"

namespace coop::server {

static void Run(CommandContext& ctx, const std::vector<std::string>& args) {
    RemoteClient* target = ctx.server.Players().ByNick(args[0]);
    if (target == nullptr) {
        ctx.Reply("No hay ningún jugador conectado llamado '" + args[0] + "'.", level::kError);
        return;
    }
    std::string text = SanitizeChat(JoinFrom(args, 1), kChatMaxChars);
    if (text.empty()) {
        ctx.Reply("El mensaje está vacío.", level::kError);
        return;
    }
    json pm = MakeEvent(ev::kPm);
    pm["from"] = ctx.SenderName();
    pm["to"] = target->nick;
    pm["text"] = text;
    ctx.server.SendEvent(*target, pm);
    if (ctx.sender != nullptr && ctx.sender != target) {
        ctx.server.SendEvent(*ctx.sender, pm);
    } else if (ctx.IsConsole()) {
        ctx.Reply("[privado a " + target->nick + "] " + text);
    }
}

COOP_COMMAND(pm, "pm", "/pm <jugador> <mensaje>", "mensaje privado", Perm::Player, 2, Run);

} // namespace coop::server
