// [COOP] See Sync.h: the menu section (F1 -> Co-op -> Sincronización) with the switches of the total sync, the volume
// of the others' sounds, the lists of actors to share or keep local, and what is synced near our Link.
#include "Sync.h"

#include "2s2h/Coop/Actors/ActorRegistry.h"
#include "2s2h/Coop/Actors/Leases.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/I18n/CoopText.h"
#include "2s2h/Coop/World/WorldSession.h"

#include "2s2h/BenGui/BenGui.hpp"
#include "2s2h/BenGui/UIWidgets.hpp"

#include <algorithm>
#include <vector>

extern "C" {
#include "functions.h"
#include "variables.h"
}

namespace coop::client {

namespace {

const ImVec4 kGray = { 0.6f, 0.6f, 0.6f, 1.0f };

void DrawNearby() {
    if (!ImGui::CollapsingHeader(Tc(Txt::SyncNearby))) {
        return;
    }
    PlayState* play = gPlayState;
    Player* link = play != nullptr ? (Player*)play->actorCtx.actorLists[ACTORCAT_PLAYER].first : nullptr;
    if (link == nullptr || !WorldSession_Active()) {
        ImGui::TextColored(kGray, "%s", Tc(Txt::OnlyInWorld));
        return;
    }
    struct Row {
        float dist;
        TrackedActor* t;
    };
    std::vector<Row> rows;
    for (TrackedActor* t : ActorRegistry_All()) {
        if (t->actor != nullptr && t->actor->update != nullptr) {
            float d = Actor_WorldDistXYZToActor(&link->actor, t->actor);
            if (d < 1500.f) {
                rows.push_back({ d, t });
            }
        }
    }
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.dist < b.dist; });
    if (rows.empty()) {
        ImGui::TextColored(kGray, "%s", Tc(Txt::SyncNothingNear));
        return;
    }
    if (ImGui::BeginTable("coopSyncNearby", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn(Tc(Txt::ColActor));
        ImGui::TableSetupColumn(Tc(Txt::ColId));
        ImGui::TableSetupColumn(Tc(Txt::ColRoom));
        ImGui::TableSetupColumn(Tc(Txt::ColSimulatedBy));
        ImGui::TableSetupColumn(Tc(Txt::ColDistance));
        ImGui::TableHeadersRow();
        for (const Row& r : rows) {
            uint8_t owner = Leases_Owner(*r.t);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", SceneObjects_ActorName(r.t->actor->id));
            ImGui::TableNextColumn();
            ImGui::Text("0x%03X", (unsigned)(uint16_t)r.t->actor->id);
            ImGui::TableNextColumn();
            if (r.t->room == image_limits::kPlayerRoom) {
                ImGui::Text("%s", Tc(Txt::CellPlayer));
            } else {
                ImGui::Text("%d", (int)r.t->room);
            }
            ImGui::TableNextColumn();
            if (owner == 0) {
                ImGui::Text("%s", Tc(Txt::CellNobodyYet));
            } else if (owner == Session_LocalId()) {
                ImGui::Text("%s", Tc(Txt::CellYou));
            } else {
                const RemotePlayer* p = Session_FindPlayer(owner);
                ImGui::Text("%s", p != nullptr ? p->nick.c_str() : "?");
            }
            ImGui::TableNextColumn();
            ImGui::Text("%.0f", r.dist);
        }
        ImGui::EndTable();
    }
}

void DrawNearbyItems() {
    if (!ImGui::CollapsingHeader(Tc(Txt::SyncItems))) {
        return;
    }
    if (!WorldSession_Active()) {
        ImGui::TextColored(kGray, "%s", Tc(Txt::OnlyInWorld));
        return;
    }
    std::vector<SharedDropRow> rows = SharedDrops_Nearby(1500.f);
    if (rows.empty()) {
        ImGui::TextColored(kGray, "%s", Tc(Txt::SyncNoItems));
        return;
    }
    if (ImGui::BeginTable("coopSyncItems", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn(Tc(Txt::ColItem));
        ImGui::TableSetupColumn(Tc(Txt::ColKey));
        ImGui::TableSetupColumn(Tc(Txt::ColDroppedIn));
        ImGui::TableSetupColumn(Tc(Txt::ColState));
        ImGui::TableSetupColumn(Tc(Txt::ColDistance));
        ImGui::TableHeadersRow();
        for (const SharedDropRow& r : rows) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", SceneObjects_ActorName(r.actorId));
            ImGui::TableNextColumn();
            ImGui::Text("%08X", r.key);
            ImGui::TableNextColumn();
            ImGui::Text("%s", Tc(r.mine ? Txt::CellYourGame : Txt::CellOtherGame));
            ImGui::TableNextColumn();
            ImGui::Text("%s", Tc(r.claim == 0 ? Txt::CellOnGround : (r.claim == 1 ? Txt::CellAsking : Txt::CellYours)));
            ImGui::TableNextColumn();
            ImGui::Text("%.0f", r.dist);
        }
        ImGui::EndTable();
    }
}

} // namespace

void SyncMenu_Draw() {
    ImGui::SeparatorText(Tc(Txt::SyncTitle));
    for (int i = 0; i < (int)SyncPart::Count; i++) {
        SyncPart part = (SyncPart)i;
        UIWidgets::CVarCheckbox(
            Sync_Name(part), Sync_CVar(part),
            UIWidgets::CheckboxOptions().Color(THEME_COLOR).DefaultValue(true).Tooltip(Tip(Txt::SyncPartTip)));
        if (!Sync_ServerAllows(part)) {
            ImGui::SameLine();
            ImGui::TextColored(kGray, "%s", Tc(Txt::SyncServerOff));
        }
    }
    UIWidgets::CVarCheckbox(
        Tc(Txt::SyncFlagReload), "gCoop.Sync.FlagReload",
        UIWidgets::CheckboxOptions().Color(THEME_COLOR).DefaultValue(true).Tooltip(Tip(Txt::SyncFlagReloadTip)));
    UIWidgets::CVarCheckbox(
        Tc(Txt::SyncListPointers), "gCoop.Sync.ListPointers",
        UIWidgets::CheckboxOptions().Color(THEME_COLOR).DefaultValue(true).Tooltip(Tip(Txt::SyncListPointersTip)));
    UIWidgets::CVarCheckbox(
        Tc(Txt::SyncSongEffects), "gCoop.Sync.SongEffects",
        UIWidgets::CheckboxOptions().Color(THEME_COLOR).DefaultValue(true).Tooltip(Tip(Txt::SyncSongEffectsTip)));
    UIWidgets::CVarSliderInt(Tc(Txt::SyncVolume), "gCoop.Sync.RemoteVolume",
                             UIWidgets::IntSliderOptions().Color(THEME_COLOR).Min(0).Max(100).DefaultValue(100));
    UIWidgets::CVarInputString(Tc(Txt::SyncShared), "gCoop.Sync.Shared",
                               UIWidgets::InputOptions().Color(THEME_COLOR).Tooltip(Tip(Txt::SyncSharedTip)));
    UIWidgets::CVarInputString(Tc(Txt::SyncLocal), "gCoop.Sync.Local",
                               UIWidgets::InputOptions().Color(THEME_COLOR).Tooltip(Tip(Txt::SyncLocalTip)));
    DrawNearby();
    DrawNearbyItems();
}

} // namespace coop::client
