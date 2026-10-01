// Admin commands: /unlockall (every item, song and heart for a player), /give (rupees, the overflow to the bank),
// /freezetime (stop the shared clock) and /time set <day|night|dawn> (jump the world's time of day).
// The time ones go through the shared clock, so every game follows at once.
#include "server/CommandRegistry.h"
#include "server/Server.h"

#include "common/Clock.h"
#include "common/Events.h"
#include "common/Text.h"

namespace coop::server {

namespace {

RemoteClient* TargetOf(CommandContext& ctx, const std::string& name) {
    RemoteClient* target = ctx.server.Players().ByNick(name);
    if (target == nullptr || target->host) {
        ctx.Reply(Tr(Msg::PlayerNotFound, { name }), level::kError);
        return nullptr;
    }
    return target;
}

void UnlockAll(CommandContext& ctx, const std::vector<std::string>& args) {
    RemoteClient* target = TargetOf(ctx, args[0]);
    if (target == nullptr) {
        return;
    }
    json ev = MakeEvent(ev::kUnlockAll);
    ev["nick"] = target->nick;
    ctx.server.SendEvent(*target, ev);
    ctx.Reply(Tr(Msg::UnlockAllReply, { target->nick }), level::kOk);
    ctx.server.SendSystem(target, Tr(Msg::UnlockAllNotice), level::kOk);
}

void Give(CommandContext& ctx, const std::vector<std::string>& args) {
    RemoteClient* target = TargetOf(ctx, args[0]);
    if (target == nullptr) {
        return;
    }
    int amount = 0;
    if (!ParseInt(args[1], amount) || amount < 1 || amount > 999999) {
        ctx.Reply(Tr(Msg::GiveRange), level::kError);
        return;
    }
    json ev = MakeEvent(ev::kGive);
    ev["nick"] = target->nick;
    ev["amount"] = amount;
    ctx.server.SendEvent(*target, ev);
    ctx.Reply(Tr(Msg::GiveReply, { std::to_string(amount), target->nick }), level::kOk);
}

// /freezetime without arguments toggles; "on"/"off" set it.
void FreezeTime(CommandContext& ctx, const std::vector<std::string>& args) {
    SharedWorld& world = ctx.server.World();
    if (!world.Exists()) {
        ctx.Reply(Tr(Msg::NoWorldYet), level::kError);
        return;
    }
    bool stopped = args.empty() ? !world.ClockStopped() : (args[0] == "on" || args[0] == "si");
    std::string err;
    if (!world.SetClockStopped(stopped, ctx.SenderName(), &err)) {
        ctx.Reply(err, level::kError);
        return;
    }
}

// /time set <day|night|dawn>: the next (or current) stretch of that kind of time, like the songs do.
void TimeSet(CommandContext& ctx, const std::vector<std::string>& args) {
    SharedWorld& world = ctx.server.World();
    if (!world.Exists()) {
        ctx.Reply(Tr(Msg::NoWorldYet), level::kError);
        return;
    }
    if (args.empty() || args[0] != "set" || args.size() < 2) {
        ctx.Reply(Tr(Msg::TimeSetUsage), level::kError);
        return;
    }
    const std::string& what = args[1];
    uint32_t now = world.ClockAbs();
    uint32_t next = clock::NextHalfDay(now);
    uint16_t nowTime = clock::TimeOfAbs(now);
    uint32_t target;
    if (what == "day") {
        // The day half that contains now, or the next one if it is already night.
        target = clock::IsNight(nowTime) ? next : (now - nowTime) + clock::kDawnTime;
    } else if (what == "night") {
        // The night half that contains now, or the next one if it is day.
        target = clock::IsNight(nowTime) ? (now - nowTime) + (clock::kDawnTime + clock::kHalfDayUnits) : next;
    } else if (what == "dawn") {
        target = (now - nowTime) + clock::kDawnTime;
    } else {
        ctx.Reply(Tr(Msg::TimeUnknown), level::kError);
        return;
    }
    if (target == now) {
        ctx.Reply(Tr(Msg::AlreadyIs, { what }), level::kWarn);
        return;
    }
    if (target < now) {
        // "day"/"night" while already inside it means the current stretch: no change.
        ctx.Reply(Tr(Msg::AlreadyIs, { what }), level::kWarn);
        return;
    }
    std::string err;
    if (!world.SetTime(target, ctx.SenderName(), &err)) {
        ctx.Reply(err, level::kError);
    }
}

} // namespace

COOP_COMMAND(unlockall, "unlockall", Msg::UnlockAllUsage, Msg::UnlockAllHelp, Perm::Op, 1, UnlockAll);
COOP_COMMAND(give, "give", Msg::GiveUsage, Msg::GiveHelp, Perm::Op, 2, Give);
COOP_COMMAND(freezetime, "freezetime", Msg::FreezeTimeUsage, Msg::FreezeTimeHelp, Perm::Op, 0, FreezeTime);
COOP_COMMAND(time, "time", Msg::TimeUsage, Msg::TimeHelp, Perm::Op, 2, TimeSet);

} // namespace coop::server
