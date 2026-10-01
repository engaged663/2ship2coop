#include "CoopMenu.h"

#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Group/Group.h"
#include "2s2h/Coop/World/WorldSession.h"

#include "common/Protocol.h"

#include "2s2h/BenGui/BenGui.hpp"
#include "2s2h/BenGui/BenMenu.h"
#include "2s2h/BenGui/UIWidgets.hpp"

#include <libultraship/bridge/consolevariablebridge.h>
#include <libultraship/libultraship.h>

namespace BenGui {
extern std::shared_ptr<BenMenu> mBenMenu;
}

using namespace coop::client;

namespace {

const ImVec4 kGreen = ImVec4(0.5f, 1.0f, 0.5f, 1.0f);
const ImVec4 kYellow = ImVec4(1.0f, 0.85f, 0.4f, 1.0f);
const ImVec4 kGray = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
const ImVec4 kRed = ImVec4(1.0f, 0.45f, 0.45f, 1.0f);

void SaveCVars() {
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

void DrawConnection() {
    ConnState state = NetClient::Get().State();
    bool idle = state == ConnState::Disconnected;

    ImGui::BeginDisabled(!idle);
    UIWidgets::CVarInputString("Nick (3-16 letras, números o _)", "gCoop.Nick",
                               UIWidgets::InputOptions().Color(THEME_COLOR));
    UIWidgets::CVarInputString("Servidor (IP o dominio)", "gCoop.Host",
                               UIWidgets::InputOptions().Color(THEME_COLOR).DefaultValue("127.0.0.1"));
    int32_t port = CVarGetInteger("gCoop.Port", coop::kDefaultPort);
    if (UIWidgets::InputInt("Puerto (UDP)", &port, UIWidgets::InputOptions().Color(THEME_COLOR))) {
        CVarSetInteger("gCoop.Port", port);
        SaveCVars();
    }
    UIWidgets::CVarInputString("Contraseña del servidor (opcional)", "gCoop.Password",
                               UIWidgets::InputOptions().Color(THEME_COLOR).IsSecret(true));
    ImGui::EndDisabled();

    ImGui::Spacing();
    if (idle) {
        if (UIWidgets::Button("Conectar", UIWidgets::ButtonOptions().Color(THEME_COLOR))) {
            Session_ConnectFromSettings();
        }
    } else {
        const char* label = state == ConnState::Connected ? "Desconectar" : "Cancelar";
        if (UIWidgets::Button(label, UIWidgets::ButtonOptions().Color(THEME_COLOR))) {
            NetClient::Get().Disconnect();
        }
    }

    switch (state) {
        case ConnState::Disconnected:
            ImGui::TextColored(kGray, "Estado: desconectado");
            break;
        case ConnState::Connecting:
            ImGui::TextColored(kYellow, "Estado: conectando con %s...", NetClient::Get().ServerLabel().c_str());
            break;
        case ConnState::Handshaking:
            ImGui::TextColored(kYellow, "Estado: identificándose...");
            break;
        case ConnState::Connected:
            ImGui::TextColored(kGreen, "Estado: conectado a %s como %s (ping %u ms)",
                               NetClient::Get().ServerLabel().c_str(), Session_LocalNick().c_str(),
                               NetClient::Get().PingMs());
            break;
    }
    std::string lastError = NetClient::Get().LastError();
    if (idle && !lastError.empty()) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(kRed, "%s", lastError.c_str());
        ImGui::PopTextWrapPos();
    }
}

void DrawPlayers() {
    if (!Session_IsConnected()) {
        return;
    }
    ImGui::SeparatorText("Jugadores conectados");
    ImGui::TextColored(kGreen, "%s (tú)", Session_LocalNick().c_str());
    for (const auto& [id, player] : Session_Players()) {
        if (player.sceneName.empty()) {
            ImGui::Text("%s", player.nick.c_str());
        } else {
            ImGui::Text("%s - %s", player.nick.c_str(), player.sceneName.c_str());
        }
    }
}

void DrawWorld() {
    ImGui::SeparatorText("Partida del servidor");
    WorldState state = WorldSession_State();
    if (state == WorldState::Outside) {
        ImGui::BeginDisabled(!Session_IsConnected());
        if (UIWidgets::Button("Entrar en la partida del servidor",
                              UIWidgets::ButtonOptions().Color(THEME_COLOR).Tooltip(
                                  "Deja la partida que tengas abierta (sin guardarla) y entra en el mundo compartido "
                                  "del servidor."))) {
            WorldSession_RequestEnter();
        }
        ImGui::EndDisabled();
    } else if (UIWidgets::Button("Salir de la partida del servidor",
                                 UIWidgets::ButtonOptions().Color(THEME_COLOR).Tooltip(
                                     "Tu progreso queda en el servidor. Vuelves a la selección de archivo."))) {
        WorldSession_RequestLeave();
    }
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(state == WorldState::Active ? kGreen : kGray, "%s", WorldSession_StatusText().c_str());
    ImGui::PopTextWrapPos();
    UIWidgets::CVarCheckbox("Entrar automáticamente al conectar", "gCoop.AutoEnter",
                            UIWidgets::CheckboxOptions().Color(THEME_COLOR).Tooltip(
                                "Al conectarte desde la pantalla de título o la selección de archivo entras directamente "
                                "en la partida del servidor. Allí tu progreso se guarda en el servidor, nunca en tus "
                                "archivos de guardado."));
}

void DrawOptions() {
    ImGui::SeparatorText("Opciones");
    UIWidgets::CVarCheckbox("Conectar automáticamente al abrir el juego", "gCoop.AutoConnect",
                            UIWidgets::CheckboxOptions().Color(THEME_COLOR));
    UIWidgets::CVarCheckbox("Mostrar mi propio nick encima de mi Link", "gCoop.ShowOwnNameTag",
                            UIWidgets::CheckboxOptions().Color(THEME_COLOR));
    UIWidgets::CVarCheckbox(
        "El inventario no pausa el juego (partida del servidor)", "gCoop.LiveMenu",
        UIWidgets::CheckboxOptions().Color(THEME_COLOR).DefaultValue(true).Tooltip(
            "Con el menú de pausa abierto el mundo sigue en directo detrás: Link, la cámara, el sonido y los demás "
            "jugadores. Link no recibe los botones del menú, pero puede caer o recibir daño. El menú se cierra solo "
            "si el juego necesita a Link (un diálogo, una cinemática, cambiar de escena, morir). Desactivado: el "
            "menú sobre una imagen fija y Link esperando, como antes. Se aplica la próxima vez que abras el menú."));
    UIWidgets::CVarSliderFloat(
        "Tamaño del chat", "gCoop.Chat.Scale",
        UIWidgets::FloatSliderOptions().Color(THEME_COLOR).Min(0.8f).Max(3.0f).DefaultValue(1.4f).Format("%.1f"));
    UIWidgets::CVarSliderFloat(
        "Opacidad del fondo del chat", "gCoop.Chat.Opacity",
        UIWidgets::FloatSliderOptions().Color(THEME_COLOR).Min(0.0f).Max(1.0f).DefaultValue(0.45f).Format("%.2f"));
    ImGui::Spacing();
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(kGray, "Chat: Enter para escribir, / para comandos (/help), Esc para cerrar. "
                              "Los otros Links solo se ven cuando estáis en el mismo escenario.");
    ImGui::PopTextWrapPos();

    ImGui::SeparatorText("Pruebas");
    UIWidgets::CVarCheckbox(
        "Arrancar directamente en Ciudad Reloj (partida de prueba)", "gCoop.Debug.BootToClockTown",
        UIWidgets::CheckboxOptions().Color(THEME_COLOR).Tooltip(
            "Al abrir el juego entra en Ciudad Reloj Sur con la partida de depuración (todos los objetos y "
            "máscaras). Esa partida nunca se guarda. Desactívalo para jugar con tus archivos normales."));
}

void RegisterCoopMenu() {
    BenGui::mBenMenu->AddMenuEntry("Co-op", "gSettings.Menu.CoopSidebarSection");
    BenGui::mBenMenu->AddSidebarEntry("Co-op", "Conexión", 1);
    WidgetPath path = { "Co-op", "Conexión", SECTION_COLUMN_1 };
    BenGui::mBenMenu->AddWidget(path, "Co-op", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) { CoopMenu_Draw(); });
}

} // namespace

void CoopMenu_Draw() {
    ImGui::SeparatorText("Conexión");
    DrawConnection();
    DrawPlayers();
    DrawWorld();
    GroupMenu_Draw();
    DrawOptions();
}

static RegisterMenuInitFunc sCoopMenuInit(RegisterCoopMenu);
