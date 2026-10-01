// Shared world commands: /tiempo, /si, /no (Song of Time vote); for admins /settime, /mundo, /reiniciar.
#include "server/CommandRegistry.h"
#include "server/Server.h"

#include "common/Clock.h"
#include "common/Text.h"

namespace coop::server {

namespace {

void Time(CommandContext& ctx, const std::vector<std::string>&) {
    ctx.Reply(ctx.server.World().TimeText());
}

void CastVote(CommandContext& ctx, bool yes) {
    if (ctx.IsConsole()) {
        ctx.Reply(Tr(Msg::VoteOnlyPlayers), level::kError);
        return;
    }
    ctx.server.World().Vote(*ctx.sender, yes);
}

void VoteYes(CommandContext& ctx, const std::vector<std::string>&) {
    CastVote(ctx, true);
}

void VoteNo(CommandContext& ctx, const std::vector<std::string>&) {
    CastVote(ctx, false);
}

void SetTime(CommandContext& ctx, const std::vector<std::string>& args) {
    int day = 0;
    uint32_t abs = 0;
    if (!ParseInt(args[0], day) || !clock::Parse(day, args[1], abs)) {
        ctx.Reply(Tr(Msg::SetTimeUsageLine), level::kError);
        return;
    }
    std::string err;
    if (!ctx.server.World().SetTime(abs, ctx.SenderName(), &err)) {
        ctx.Reply(err, level::kError);
    }
}

void ShowWorld(CommandContext& ctx, const std::vector<std::string>&) {
    ctx.Reply(ctx.server.World().Describe());
}

void Restart(CommandContext& ctx, const std::vector<std::string>&) {
    std::string err;
    if (!ctx.server.World().Restart(ctx.SenderName(), &err)) {
        ctx.Reply(err, level::kError);
    }
}

} // namespace

COOP_COMMAND(tiempo, "tiempo", Msg::ClockUsage, Msg::ClockHelp, Perm::Player, 0, Time);
COOP_COMMAND(si, "si", Msg::YesUsage, Msg::YesHelp, Perm::Player, 0, VoteYes);
COOP_COMMAND(no, "no", Msg::NoUsage, Msg::NoHelp, Perm::Player, 0, VoteNo);
COOP_COMMAND(settime, "settime", Msg::SetTimeUsage, Msg::SetTimeHelp, Perm::Op, 2, SetTime);
COOP_COMMAND(mundo, "mundo", Msg::WorldUsage, Msg::WorldHelp, Perm::Op, 0, ShowWorld);
COOP_COMMAND(reiniciar, "reiniciar", Msg::RestartUsage, Msg::RestartHelp, Perm::Op, 0, Restart);

// English names for the Spanish ones (every language can use them; commands are typed in ASCII).
COOP_ALIAS(clock, "clock", "tiempo");
COOP_ALIAS(yes, "yes", "si");
COOP_ALIAS(world, "world", "mundo");
COOP_ALIAS(restart, "restart", "reiniciar");

} // namespace coop::server
