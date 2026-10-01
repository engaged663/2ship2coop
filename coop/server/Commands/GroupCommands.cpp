// /invitar, /aceptar, /rechazar, /grupo, /dejargrupo (spec 2026-09-30-coop-grupos-actividades §2). The rules live in
// GroupBook; this file checks who may use them, sends the invitation, the trip to the inviter on accepting ("tp",
// the path of /tp) and the texts.
#include "server/CommandRegistry.h"
#include "server/Groups.h"
#include "server/Server.h"

#include "common/Text.h"

#include <algorithm>

namespace coop::server {

namespace {

bool CheckSender(CommandContext& ctx) {
    if (ctx.IsConsole()) {
        ctx.Reply(Tr(Msg::GroupOnlyPlayers), level::kError);
        return false;
    }
    if (!Groups_Enabled(ctx.server)) {
        ctx.Reply(Tr(Msg::GroupsOff), level::kError);
        return false;
    }
    if (!Groups_InWorld(*ctx.sender)) {
        ctx.Reply(Tr(Msg::GroupNotInWorld), level::kError);
        return false;
    }
    if (!ctx.sender->inviteBudget.Take(ctx.server.NowMs())) {
        ctx.Reply(Tr(Msg::RateLimited), level::kWarn);
        return false;
    }
    return true;
}

// The mover appears where target is (the game warps as for /tp).
void SendTrip(Server& server, RemoteClient& mover, const RemoteClient& target) {
    json tp = MakeEvent(ev::kTp);
    tp["scene"] = target.scene;
    tp["entrance"] = target.entrance;
    tp["room"] = target.room;
    tp["pos"] = { target.pos[0], target.pos[1], target.pos[2] };
    tp["rot"] = target.rotY;
    tp["target"] = target.nick;
    server.SendEvent(mover, tp);
}

void InviteOne(CommandContext& ctx, RemoteClient& to, GroupChanges& ch) {
    RemoteClient& from = *ctx.sender;
    if (!Groups_InWorld(to)) {
        ctx.Reply(Tr(Msg::InviteNotInWorld, { to.nick }), level::kError);
        return;
    }
    int64_t ttl = ctx.server.Config().inviteMs;
    switch (ctx.server.Groups().Invite(from.id, to.id, ctx.server.NowMs(), ttl, ch)) {
        case InviteResult::Self:
            ctx.Reply(Tr(Msg::InviteSelf), level::kError);
            return;
        case InviteResult::AlreadyMate:
            ctx.Reply(Tr(Msg::InviteAlreadyMate, { to.nick }), level::kWarn);
            return;
        case InviteResult::Full:
            ctx.Reply(Tr(Msg::InviteFull, { std::to_string(kMaxPlayers) }), level::kError);
            return;
        case InviteResult::Sent:
        case InviteResult::Renewed:
            break;
    }
    json ev = MakeEvent(ev::kInvite);
    ev["from"] = from.id;
    ev["nick"] = from.nick;
    ev["activity"] = from.activityKey;
    ev["name"] = from.activityName;
    ev["ms"] = ttl;
    ctx.server.SendEvent(to, ev);
    ctx.server.SendSystem(&to, from.activityName.empty() ? Tr(Msg::InviteToGroup, { from.nick })
                                                         : Tr(Msg::InviteToActivity, { from.nick, from.activityName }));
    ctx.server.Log().Info(Tr(Msg::LogInvite, { from.nick, to.nick }));
    ctx.Reply(Tr(Msg::InviteSent, { to.nick }), level::kOk);
}

void RunInvite(CommandContext& ctx, const std::vector<std::string>& args) {
    if (!CheckSender(ctx)) {
        return;
    }
    GroupChanges ch;
    if (args.size() == 1 && (EqualsIgnoreCase(args[0], "todos") || EqualsIgnoreCase(args[0], "all"))) {
        int invited = 0;
        for (RemoteClient* p : ctx.server.Players().Welcomed()) {
            if (p != ctx.sender && Groups_InWorld(*p) && !ctx.server.Groups().SameGroup(ctx.sender->id, p->id)) {
                InviteOne(ctx, *p, ch);
                invited++;
            }
        }
        if (invited == 0) {
            ctx.Reply(Tr(Msg::InviteNobody), level::kWarn);
        }
    } else {
        for (const std::string& nick : args) {
            RemoteClient* to = ctx.server.Players().ByNick(nick);
            if (to == nullptr) {
                ctx.Reply(Tr(Msg::PlayerNotFound, { nick }), level::kError);
                continue;
            }
            InviteOne(ctx, *to, ch);
        }
    }
    Groups_Publish(ctx.server, ch, ctx.sender->id);
}

// The player named in args[0], or 0 when there is none (err: replied).
bool NamedPlayer(CommandContext& ctx, const std::vector<std::string>& args, uint8_t& out) {
    out = 0;
    if (args.empty()) {
        return true;
    }
    RemoteClient* p = ctx.server.Players().ByNick(args[0]);
    if (p == nullptr) {
        ctx.Reply(Tr(Msg::PlayerNotFound, { args[0] }), level::kError);
        return false;
    }
    out = p->id;
    return true;
}

void RunAccept(CommandContext& ctx, const std::vector<std::string>& args) {
    uint8_t from = 0;
    if (!CheckSender(ctx) || !NamedPlayer(ctx, args, from)) {
        return;
    }
    RemoteClient& me = *ctx.sender;
    GroupChanges ch;
    uint8_t inviterId = 0;
    AcceptResult r = ctx.server.Groups().Accept(me.id, from, ch, &inviterId);
    RemoteClient* inviter = ctx.server.Players().ById(inviterId);
    std::string inviterNick = inviter != nullptr ? inviter->nick : (args.empty() ? std::string("?") : args[0]);
    switch (r) {
        case AcceptResult::NoInvite:
            ctx.Reply(args.empty() ? Tr(Msg::InviteNone) : Tr(Msg::InviteNoneFrom, { args[0] }), level::kError);
            break;
        case AcceptResult::GroupGone:
            ctx.Reply(Tr(Msg::InviteGone, { inviterNick }), level::kError);
            break;
        case AcceptResult::Full:
            ctx.Reply(Tr(Msg::InviteFullOther, { inviterNick }), level::kError);
            break;
        case AcceptResult::Joined:
            ctx.Reply(Tr(Msg::InviteAcceptedYou, { inviterNick }), level::kOk);
            for (RemoteClient* m : Groups_Mates(ctx.server, me)) {
                ctx.server.SendSystem(m, Tr(Msg::InviteAcceptedOther, { me.nick }), level::kOk);
            }
            ctx.server.Log().Info(Tr(Msg::LogGroupJoin, { me.nick, inviterNick }));
            if (inviter != nullptr && inviter->hasState && inviter->scene >= 0) {
                SendTrip(ctx.server, me, *inviter);
            }
            break;
    }
    Groups_Publish(ctx.server, ch, me.id);
}

void RunDecline(CommandContext& ctx, const std::vector<std::string>& args) {
    uint8_t from = 0;
    if (!CheckSender(ctx) || !NamedPlayer(ctx, args, from)) {
        return;
    }
    GroupChanges ch;
    int count = ctx.server.Groups().Decline(ctx.sender->id, from, ch);
    if (count == 0) {
        ctx.Reply(args.empty() ? Tr(Msg::InviteNone) : Tr(Msg::InviteNoneFrom, { args[0] }), level::kError);
    } else if (!args.empty()) {
        ctx.Reply(Tr(Msg::InviteDeclinedYou, { args[0] }));
    } else {
        ctx.Reply(Tr(Msg::InviteDeclinedAll, { std::to_string(count) }));
    }
    Groups_Publish(ctx.server, ch, ctx.sender->id);
}

void RunGroup(CommandContext& ctx, const std::vector<std::string>&) {
    if (ctx.IsConsole()) {
        ctx.Reply(Tr(Msg::GroupOnlyPlayers), level::kError);
        return;
    }
    Server& server = ctx.server;
    RemoteClient& me = *ctx.sender;
    std::string text;
    if (const Group* g = server.Groups().GroupOf(me.id)) {
        RemoteClient* leader = server.Players().ById(g->Leader());
        text = Tr(Msg::GroupTitle, { leader != nullptr ? leader->nick : std::string("?"),
                                     std::to_string(g->members.size()), std::to_string(kMaxPlayers) });
        for (uint8_t id : g->members) {
            RemoteClient* m = server.Players().ById(id);
            text += "\n" + (m != nullptr ? m->nick : std::string("?"));
            if (id == g->Leader()) {
                text += Tr(Msg::GroupLeaderTag);
            }
            if (m != nullptr && !m->activityName.empty()) {
                text += Tr(Msg::ListActivityTag, { m->activityName });
            }
        }
    } else {
        text = Tr(Msg::GroupNone);
    }
    std::vector<Invitation> invites = server.Groups().InvitesTo(me.id);
    if (!invites.empty()) {
        text += "\n" + Tr(Msg::GroupInvitesTitle);
        for (const Invitation& inv : invites) {
            RemoteClient* from = server.Players().ById(inv.from);
            int64_t secs = std::max<int64_t>(0, (inv.expiresMs - server.NowMs() + 999) / 1000);
            text += "\n" + Tr(Msg::GroupInviteLine, { from != nullptr ? from->nick : std::string("?"),
                                                      std::to_string(secs) });
        }
    }
    ctx.Reply(text);
}

void RunLeaveGroup(CommandContext& ctx, const std::vector<std::string>&) {
    if (ctx.IsConsole()) {
        ctx.Reply(Tr(Msg::GroupOnlyPlayers), level::kError);
        return;
    }
    RemoteClient& me = *ctx.sender;
    if (ctx.server.Groups().GroupOf(me.id) == nullptr) {
        ctx.Reply(Tr(Msg::GroupNone), level::kWarn);
        return;
    }
    std::vector<RemoteClient*> mates = Groups_Mates(ctx.server, me);
    GroupChanges ch;
    ctx.server.Groups().Leave(me.id, ch);
    ctx.Reply(Tr(Msg::GroupLeftYou));
    for (RemoteClient* m : mates) {
        ctx.server.SendSystem(m, Tr(Msg::GroupLeftOther, { me.nick }));
    }
    ctx.server.Log().Info(Tr(Msg::LogGroupLeave, { me.nick }));
    Groups_Publish(ctx.server, ch, me.id);
}

} // namespace

COOP_COMMAND(invitar, "invitar", Msg::InviteUsage, Msg::InviteHelp, Perm::Player, 1, RunInvite);
COOP_COMMAND(aceptar, "aceptar", Msg::AcceptUsage, Msg::AcceptHelp, Perm::Player, 0, RunAccept);
COOP_COMMAND(rechazar, "rechazar", Msg::DeclineUsage, Msg::DeclineHelp, Perm::Player, 0, RunDecline);
COOP_COMMAND(grupo, "grupo", Msg::GroupUsage, Msg::GroupHelp, Perm::Player, 0, RunGroup);
COOP_COMMAND(dejargrupo, "dejargrupo", Msg::LeaveGroupUsage, Msg::LeaveGroupHelp, Perm::Player, 0, RunLeaveGroup);
COOP_ALIAS(invite, "invite", "invitar");
COOP_ALIAS(accept, "accept", "aceptar");
COOP_ALIAS(decline, "decline", "rechazar");
COOP_ALIAS(group, "group", "grupo");
COOP_ALIAS(leavegroup, "leavegroup", "dejargrupo");

} // namespace coop::server
