// [COOP] Co-op menu (F1), section "Salas de actividad": our room (open its window, leave it), the open rooms of the
// server's game (Unirme) and the player's options (gCoop.Room.*).
#include "Room.h"

#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/World/WorldSession.h"

#include "2s2h/BenGui/BenGui.hpp"
#include "2s2h/BenGui/UIWidgets.hpp"

#include <libultraship/libultraship.h>

namespace coop::client {

namespace {

const ImVec4 kGray = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);

const char* StateText(RoomPhase phase) {
    switch (phase) {
        case RoomPhase::Lobby:
            return "esperando confirmaciones";
        case RoomPhase::Starting:
            return "a punto de empezar";
        case RoomPhase::Running:
            return "en marcha";
        case RoomPhase::Ended:
            return "terminada";
        default:
            return "";
    }
}

const char* StateText(const std::string& state) {
    if (state == "lobby") {
        return "esperando";
    }
    if (state == "starting") {
        return "empezando";
    }
    if (state == "running") {
        return "en marcha";
    }
    return "terminada";
}

void DrawOwn() {
    if (!Room_Has()) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(kGray, "No estás en ninguna sala. Se abre una al empezar un minijuego (con o sin grupo).");
        ImGui::PopTextWrapPos();
        return;
    }
    const RoomInfo& r = Room_Get();
    ImGui::Text("Tu sala: %s (%s, %d jugadores)", r.name.c_str(), StateText(r.phase), (int)r.members.size());
    if (UIWidgets::Button("Abrir la ventana de la sala",
                          UIWidgets::ButtonOptions().Color(THEME_COLOR).Size(UIWidgets::Sizes::Inline))) {
        RoomWindow_Show();
    }
    ImGui::SameLine();
    if (UIWidgets::Button("Salir de la sala",
                          UIWidgets::ButtonOptions().Color(THEME_COLOR).Size(UIWidgets::Sizes::Inline))) {
        Room_Leave();
    }
}

void DrawOpen() {
    const std::vector<OpenRoom>& open = Room_OpenRooms();
    ImGui::Text("Salas abiertas:");
    if (open.empty()) {
        ImGui::TextColored(kGray, "Ninguna.");
        return;
    }
    for (const OpenRoom& o : open) {
        ImGui::PushID((int)o.id);
        ImGui::Text("%s - %s (%d/4, %s)", o.name.c_str(), o.nick.c_str(), o.count, StateText(o.state));
        if (Room_Get().id != o.id) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Unirme")) {
                Room_Join(o.id);
            }
        }
        ImGui::PopID();
    }
}

void DrawOptions() {
    auto opts = [](const char* tooltip) {
        return UIWidgets::CheckboxOptions().Color(THEME_COLOR).DefaultValue(true).Tooltip(tooltip);
    };
    UIWidgets::CVarCheckbox("Abrir una sala al empezar un minijuego", "gCoop.Room.Enabled",
                            opts("Tu grupo entra solo y puedes invitar a quien quieras solo para esa actividad. "
                                 "Desactivado: los minijuegos funcionan como antes (solo el grupo, de cerca)."));
    UIWidgets::CVarCheckbox("Esperar a que todos confirmen antes de empezar", "gCoop.Room.Hold",
                            opts("Tu partida se congela al empezar el minijuego hasta que todos los de la sala pulsan "
                                 "Listo (tú también: botón Listo o START). Desactivado: empieza al momento y los demás "
                                 "se unen al confirmar."));
    UIWidgets::CVarSliderFloat(
        "Tamaño de la ventana de la sala", "gCoop.Room.Scale",
        UIWidgets::FloatSliderOptions().Color(THEME_COLOR).Min(0.8f).Max(2.5f).DefaultValue(1.1f).Format("%.1f"));
}

} // namespace

void RoomMenu_Draw() {
    ImGui::SeparatorText("Salas de actividad");
    if (!Session_IsConnected() || !WorldSession_InWorld()) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(kGray, "Entra en la partida del servidor para usar las salas de actividad.");
        ImGui::PopTextWrapPos();
    } else {
        DrawOwn();
        ImGui::Spacing();
        DrawOpen();
    }
    DrawOptions();
}

} // namespace coop::client
