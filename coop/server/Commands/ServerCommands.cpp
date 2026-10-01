// /say, /stop and /lang.
#include "server/CommandRegistry.h"
#include "server/Server.h"

#include "server/ServerConfig.h"

#include "common/Text.h"

namespace coop::server {

namespace {

void Say(CommandContext& ctx, const std::vector<std::string>& args) {
    std::string text = SanitizeChat(JoinFrom(args, 0), kChatMaxChars);
    if (text.empty()) {
        return;
    }
    for (RemoteClient* p : ctx.server.Players().Welcomed()) {
        ctx.server.SendSystem(p, Tr(Msg::SayPrefix, { text }), level::kInfo);
    }
    ctx.server.Log().Info(Tr(Msg::SayPrefix, { text }));
}

void Stop(CommandContext& ctx, const std::vector<std::string>& args) {
    ctx.server.Stop(args.empty() ? Tr(Msg::StopFromConsole) : JoinFrom(args, 0));
}

// /lang without arguments shows the language; /lang <es|en|zh|ru> changes it for the whole server and saves it.
void LangCmd(CommandContext& ctx, const std::vector<std::string>& args) {
    if (args.empty()) {
        ctx.Reply(Tr(Msg::LangCurrent, { LangName(GetLang()) }));
        return;
    }
    coop::Lang lang;
    if (!ParseLang(args[0], lang)) {
        ctx.Reply(Tr(Msg::LangUnknown, { args[0] }), level::kError);
        return;
    }
    SetLang(lang);
    if (!ctx.server.Config().configPath.empty()) {
        SaveConfigLanguage(ctx.server.Config().configPath, lang);
    }
    ctx.Reply(Tr(Msg::LangChanged, { LangName(lang) }), level::kOk); // already in the new language
}

} // namespace

COOP_COMMAND(say, "say", Msg::SayUsage, Msg::SayHelp, Perm::Op, 1, Say);
COOP_COMMAND(stop, "stop", Msg::StopUsage, Msg::StopHelp, Perm::Console, 0, Stop);
COOP_COMMAND(lang, "lang", Msg::LangUsage, Msg::LangHelp, Perm::Op, 0, LangCmd);

} // namespace coop::server
