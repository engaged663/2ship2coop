// [COOP] Co-op menu (F1), section "Grupo e invitaciones": invite players of the server's world, see the group and its
// activity, answer invitations, and the player's options (gCoop.Group.*).
#include "Group.h"

#include "2s2h/Coop/Client/Session.h"
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
        ImGui::TextColored(kGray, "No hay nadie más conectado.");
        return;
    }
    for (const auto& [id, player] : players) {
        ImGui::PushID(id);
        ImGui::Text("%s", player.nick.c_str());
        ImGui::SameLine();
        if (Group_IsMate(id)) {
            ImGui::TextColored(kGreen, "(en tu grupo)");
        } else if (ImGui::SmallButton("Invitar")) {
            Group_Invite(player.nick);
        }
        ImGui::PopID();
    }
    if (UIWidgets::Button("Invitar a todos", UIWidgets::ButtonOptions().Color(THEME_COLOR).Size(UIWidgets::Sizes::Inline))) {
        Group_Invite("todos");
    }
}

void DrawGroup() {
    if (!Group_Has()) {
        ImGui::TextColored(kGray, "No estás en ningún grupo.");
        return;
    }
    for (const GroupMember& m : Group_Members()) {
        bool me = m.id == Session_LocalId();
        std::string line = m.nick + (m.id == Group_Leader() ? " (líder)" : "") + (me ? " (tú)" : "");
        ImGui::TextColored(me ? kGreen : ImVec4(1, 1, 1, 1), "%s", line.c_str());
    }
    if (Group_Members().size() <= 1) {
        ImGui::TextColored(kGray, "Esperando a que acepten tu invitación...");
    }
    std::string activity = Group_ActivityName();
    if (!activity.empty()) {
        ImGui::Text("Actividad del líder: %s", activity.c_str());
    }
    if (UIWidgets::Button("Salir del grupo", UIWidgets::ButtonOptions().Color(THEME_COLOR).Size(UIWidgets::Sizes::Inline))) {
        Group_Leave();
    }
}

void DrawInvites() {
    std::vector<GroupInvite> invites = Group_Invites();
    if (invites.empty()) {
        return;
    }
    ImGui::Spacing();
    ImGui::Text("Invitaciones:");
    int64_t now = Group_NowMs();
    for (const GroupInvite& inv : invites) {
        ImGui::PushID(1000 + inv.from);
        int secs = (int)std::max<int64_t>(0, (inv.expiresMs - now + 999) / 1000);
        ImGui::Text("%s te invita a %s (%d s)", inv.nick.c_str(), inv.name.empty() ? "su grupo" : inv.name.c_str(),
                    secs);
        ImGui::SameLine();
        if (ImGui::SmallButton("Aceptar")) {
            Group_Accept(inv.nick);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Rechazar")) {
            Group_Decline(inv.nick);
        }
        ImGui::PopID();
    }
}

void DrawOptions() {
    auto opts = [](const char* tooltip) {
        return UIWidgets::CheckboxOptions().Color(THEME_COLOR).DefaultValue(true).Tooltip(tooltip);
    };
    UIWidgets::CVarCheckbox("Ver los diálogos de mi grupo", "gCoop.Group.Dialogues",
                            opts("Cuando alguien de tu grupo habla con un NPC cerca de ti, ves el mismo texto."));
    UIWidgets::CVarCheckbox("Ver las cinemáticas de mi grupo y de los jefes", "gCoop.Group.Cutscenes",
                            opts("Las cinemáticas de tu grupo (y las de los jefes, para todos) se ven con su cámara."));
    UIWidgets::CVarCheckbox("Unirme a los minijuegos de mi grupo", "gCoop.Group.Minigames",
                            opts("Juegas con el mismo equipo y marcador, o le acompañas a su minijuego."));
    UIWidgets::CVarCheckbox("Recibir los premios de mi grupo", "gCoop.Group.Rewards",
                            opts("Rupias, botellas y munición que gane tu grupo cerca de ti."));
    UIWidgets::CVarCheckbox("Ver los actores de las cinemáticas que miro", "gCoop.Group.CinemaActors",
                            opts("En la cinemática de otro ves también sus personajes de cinemática (Dm_*, Demo_*)."));
    UIWidgets::CVarCheckbox("Ver los efectos de los demás", "gCoop.Effects",
                            opts("Partículas de los otros Links, de los enemigos que simulan y de sus cinemáticas."));
    UIWidgets::CVarCheckbox("Avisarme cuando me toque (minijuegos por turnos)", "gCoop.Group.Turns",
                            opts("Cuando alguien de tu grupo termina un minijuego de un jugador, te avisa de tu ronda."));
}

} // namespace

void GroupMenu_Draw() {
    ImGui::SeparatorText("Grupo e invitaciones");
    if (!Session_IsConnected() || !WorldSession_InWorld()) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(kGray, "Entra en la partida del servidor para invitar a otros a minijuegos y misiones.");
        ImGui::PopTextWrapPos();
    } else {
        DrawPlayers();
        ImGui::Spacing();
        DrawGroup();
        DrawInvites();
    }
    ImGui::SeparatorText("Opciones del grupo");
    DrawOptions();
}

} // namespace coop::client
