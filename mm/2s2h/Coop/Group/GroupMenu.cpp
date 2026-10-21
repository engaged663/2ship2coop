// [COOP] Co-op menu (F1), section "Grupo e invitaciones": invite players of the server's world, see the group and its
// activity, answer invitations, and the player's options (gCoop.Group.*).
#include "Group.h"

#include "2s2h/Coop/Activities/Activities.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/I18n/CoopText.h"
#include "2s2h/Coop/World/WorldSession.h"

#include "2s2h/BenGui/BenGui.hpp"
#include "2s2h/BenGui/UIWidgets.hpp"

#include <libultraship/libultraship.h>

namespace coop::client {

namespace {

const ImVec4 kGreen = ImVec4(0.5f, 1.0f, 0.5f, 1.0f);
const ImVec4 kGray = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);

void DrawPlayers() {
    const auto& players = Session_Players();
    if (players.empty()) {
        ImGui::TextColored(kGray, "%s", Tc(Txt::GroupNobody));
        return;
    }
    for (const auto& [id, player] : players) {
        ImGui::PushID(id);
        ImGui::Text("%s", player.nick.c_str());
        ImGui::SameLine();
        if (Group_IsMate(id)) {
            ImGui::TextColored(kGreen, "%s", Tc(Txt::GroupInYours));
        } else if (ImGui::SmallButton(Tc(Txt::Invite))) {
            Group_Invite(player.nick);
        }
        ImGui::PopID();
    }
    if (UIWidgets::Button(Tc(Txt::InviteAll),
                          UIWidgets::ButtonOptions().Color(THEME_COLOR).Size(UIWidgets::Sizes::Inline))) {
        Group_Invite("all");
    }
}

void DrawGroup() {
    if (!Group_Has()) {
        ImGui::TextColored(kGray, "%s", Tc(Txt::GroupNone));
        return;
    }
    for (const GroupMember& m : Group_Members()) {
        bool me = m.id == Session_LocalId();
        std::string line = m.nick;
        if (m.id == Group_Leader()) {
            line += std::string(" ") + Tc(Txt::TagLeader);
        }
        if (me) {
            line += std::string(" ") + Tc(Txt::TagYou);
        }
        ImGui::TextColored(me ? kGreen : ImVec4(1, 1, 1, 1), "%s", line.c_str());
    }
    if (Group_Members().size() <= 1) {
        ImGui::TextColored(kGray, "%s", Tc(Txt::GroupWaiting));
    }
    std::string activity = Group_ActivityName();
    if (!activity.empty()) {
        ImGui::Text("%s", Tx(Txt::GroupActivity, { activity }).c_str());
    }
    if (UIWidgets::Button(Tc(Txt::GroupLeave),
                          UIWidgets::ButtonOptions().Color(THEME_COLOR).Size(UIWidgets::Sizes::Inline))) {
        Group_Leave();
    }
}

void DrawInvites() {
    std::vector<GroupInvite> invites = Group_Invites();
    if (invites.empty()) {
        return;
    }
    ImGui::Spacing();
    ImGui::Text("%s", Tc(Txt::GroupInvites));
    int64_t now = Group_NowMs();
    for (const GroupInvite& inv : invites) {
        ImGui::PushID(1000 + inv.from);
        int secs = (int)std::max<int64_t>(0, (inv.expiresMs - now + 999) / 1000);
        std::string name = Activity_ShownName(inv.activity, inv.name);
        ImGui::Text("%s", (name.empty() ? Tx(Txt::InviteToGroupLine, { inv.nick, std::to_string(secs) })
                                        : Tx(Txt::InviteToActivityLine, { inv.nick, name, std::to_string(secs) }))
                              .c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton(Tc(Txt::Accept))) {
            Group_Accept(inv.nick);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(Tc(Txt::Decline))) {
            Group_Decline(inv.nick);
        }
        ImGui::PopID();
    }
}

void DrawOptions() {
    auto opts = [](Txt tooltip) {
        return UIWidgets::CheckboxOptions().Color(THEME_COLOR).DefaultValue(true).Tooltip(Tip(tooltip));
    };
    UIWidgets::CVarCheckbox(Tc(Txt::GroupOptDialogues), "gCoop.Group.Dialogues", opts(Txt::GroupOptDialoguesTip));
    UIWidgets::CVarCheckbox(Tc(Txt::GroupOptCutscenes), "gCoop.Group.Cutscenes", opts(Txt::GroupOptCutscenesTip));
    UIWidgets::CVarCheckbox(Tc(Txt::GroupOptMinigames), "gCoop.Group.Minigames", opts(Txt::GroupOptMinigamesTip));
    UIWidgets::CVarCheckbox(Tc(Txt::GroupOptRewards), "gCoop.Group.Rewards", opts(Txt::GroupOptRewardsTip));
    UIWidgets::CVarCheckbox(Tc(Txt::GroupOptCinemaActors), "gCoop.Group.CinemaActors",
                            opts(Txt::GroupOptCinemaActorsTip));
    UIWidgets::CVarCheckbox(Tc(Txt::GroupOptEffects), "gCoop.Effects", opts(Txt::GroupOptEffectsTip));
    UIWidgets::CVarCheckbox(Tc(Txt::GroupOptTurns), "gCoop.Group.Turns", opts(Txt::GroupOptTurnsTip));
}

} // namespace

void GroupMenu_Draw() {
    ImGui::SeparatorText(Tc(Txt::GroupTitle));
    if (!Session_IsConnected() || !WorldSession_InWorld()) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(kGray, "%s", Tc(Txt::GroupEnterFirst));
        ImGui::PopTextWrapPos();
    } else {
        DrawPlayers();
        ImGui::Spacing();
        DrawGroup();
        DrawInvites();
    }
    ImGui::SeparatorText(Tc(Txt::GroupOptions));
    DrawOptions();
}

} // namespace coop::client
