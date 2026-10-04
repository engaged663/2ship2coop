// [COOP] See Sync.h: the menu section (F1 -> Co-op -> Sincronización) with the switches of the total sync, the volume
// of the others' sounds, the lists of actors to share or keep local, and what is synced near our Link.
#include "Sync.h"

#include "2s2h/Coop/Actors/ActorRegistry.h"
#include "2s2h/Coop/Actors/Leases.h"
#include "2s2h/Coop/Client/Session.h"
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
    if (!ImGui::CollapsingHeader("Ver lo sincronizado cerca (para avisar de un fallo)")) {
        return;
    }
    PlayState* play = gPlayState;
    Player* link = play != nullptr ? (Player*)play->actorCtx.actorLists[ACTORCAT_PLAYER].first : nullptr;
    if (link == nullptr || !WorldSession_Active()) {
        ImGui::TextColored(kGray, "Solo dentro de la partida del servidor.");
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
        ImGui::TextColored(kGray, "Nada sincronizado a menos de 1500 unidades.");
        return;
    }
    if (ImGui::BeginTable("coopSyncNearby", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Actor");
        ImGui::TableSetupColumn("Id");
        ImGui::TableSetupColumn("Sala");
        ImGui::TableSetupColumn("Lo simula");
        ImGui::TableSetupColumn("Distancia");
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
                ImGui::Text("jugador");
            } else {
                ImGui::Text("%d", (int)r.t->room);
            }
            ImGui::TableNextColumn();
            if (owner == 0) {
                ImGui::Text("nadie aún");
            } else if (owner == Session_LocalId()) {
                ImGui::Text("tú");
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

} // namespace

void SyncMenu_Draw() {
    ImGui::SeparatorText("Sincronización");
    for (int i = 0; i < (int)SyncPart::Count; i++) {
        SyncPart part = (SyncPart)i;
        UIWidgets::CVarCheckbox(Sync_Name(part), Sync_CVar(part),
                                UIWidgets::CheckboxOptions().Color(THEME_COLOR).DefaultValue(true).Tooltip(
                                    "Apágalo si algo de esta parte falla: se nota sin reiniciar."));
        if (!Sync_ServerAllows(part)) {
            ImGui::SameLine();
            ImGui::TextColored(kGray, "(apagado en el servidor)");
        }
    }
    UIWidgets::CVarCheckbox(
        "Recrear objetos que otro jugador cambia (paredes, rocas, bloques)", "gCoop.Sync.FlagReload",
        UIWidgets::CheckboxOptions().Color(THEME_COLOR).DefaultValue(true).Tooltip(
            "Un objeto que solo mira su bandera al crearse se crea de nuevo cuando otro jugador la cambia."));
    UIWidgets::CVarSliderInt("Volumen de los demás (%)", "gCoop.Sync.RemoteVolume",
                             UIWidgets::IntSliderOptions().Color(THEME_COLOR).Min(0).Max(100).DefaultValue(100));
    UIWidgets::CVarInputString("Compartir también (ids o nombres)", "gCoop.Sync.Shared",
                               UIWidgets::InputOptions().Color(THEME_COLOR).Tooltip(
                                   "Actores que no estaban en la lista y quieres que sean iguales para todos, "
                                   "separados por comas: OBJ_RAILLIFT, 0x1C3... Se aplica al recargar la escena."));
    UIWidgets::CVarInputString("Nunca compartir (ids o nombres)", "gCoop.Sync.Local",
                               UIWidgets::InputOptions().Color(THEME_COLOR).Tooltip(
                                   "Actores que cada juego tendrá a su aire aunque estén en una lista. Se aplica al "
                                   "recargar la escena."));
    DrawNearby();
}

} // namespace coop::client
