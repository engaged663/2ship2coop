// [COOP] The server's game mods (.o2r) on screen: a window at the top while the game asks whether to download them,
// downloads or loads them (the same place as the invitations), and the "Mods del juego del servidor (.o2r)" section of
// the Co-op menu. Map: O2r.h.
#include "O2r.h"

#include "2s2h/Coop/Client/Session.h"

#include "2s2h/BenGui/BenGui.hpp"
#include "2s2h/BenGui/UIWidgets.hpp"
#include "2s2h/ShipInit.hpp"

#include <libultraship/bridge/consolevariablebridge.h>
#include <libultraship/libultraship.h>
#include <ship/window/gui/GuiWindow.h>

#include <memory>
#include <string>

namespace coop::client {

namespace {

const ImVec4 kGreen = ImVec4(0.5f, 1.0f, 0.5f, 1.0f);
const ImVec4 kYellow = ImVec4(1.0f, 0.85f, 0.4f, 1.0f);
const ImVec4 kGray = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);

bool sAlways = false; // "Descargar siempre sin preguntar" before pressing "Descargar"

void DrawMissing() {
    for (const o2r::Entry& entry : O2r_Required()) {
        if (!O2rLoader_IsLoaded(entry.sha256) && !O2r_InCache(entry)) {
            ImGui::BulletText("%s (%s)", entry.name.c_str(), o2r::FormatBytes(entry.size).c_str());
        }
    }
}

// The question, in the window and in the menu (id keeps their buttons apart).
void DrawQuestion(const char* id) {
    ImGui::PushID(id);
    ImGui::Text("El servidor usa mods del juego (.o2r) que no tienes:");
    DrawMissing();
    ImGui::TextDisabled("Se guardan en la carpeta coop_mods y solo se usan mientras juegas en este servidor.");
    if (ImGui::Button("Descargar")) {
        O2r_Accept(sAlways);
    }
    ImGui::SameLine();
    if (ImGui::Button("No, desconectar")) {
        O2r_Decline();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Descargar siempre sin preguntar", &sAlways);
    ImGui::PopID();
}

void DrawProgress(const char* id) {
    O2rProgress p = O2r_Progress();
    ImGui::PushID(id);
    ImGui::Text("Descargando mods del servidor (%d/%d): %s", (int)p.number, (int)p.count, p.name.c_str());
    float fraction = p.total > 0 ? (float)((double)p.done / (double)p.total) : 0.0f;
    std::string label = o2r::FormatBytes(p.done) + " / " + o2r::FormatBytes(p.total);
    ImGui::ProgressBar(fraction, ImVec2(380.0f, 0.0f), label.c_str());
    if (ImGui::Button("Cancelar")) {
        O2r_Decline();
    }
    ImGui::PopID();
}

class O2rWindow : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void InitElement() override {
    }
    void DrawElement() override {
    }
    void UpdateElement() override {
    }
    void Draw() override;
};

std::shared_ptr<O2rWindow> sWindow;

void O2rWindow::Draw() {
    O2rState state = O2r_State();
    if (state != O2rState::Asking && state != O2rState::Downloading && state != O2rState::Loading) {
        return;
    }
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + 12.0f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.12f, 0.88f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize;
    if (ImGui::Begin("##CoopO2r", nullptr, flags)) {
        ImGui::SetWindowFontScale(CVarGetFloat("gCoop.Chat.Scale", 1.4f));
        if (state == O2rState::Asking) {
            DrawQuestion("window");
            ImGui::TextDisabled("(también en F1 -> Co-op)");
        } else if (state == O2rState::Downloading) {
            DrawProgress("window");
        } else {
            ImGui::Text("Cargando los mods del servidor...");
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void RegisterO2rWindow() {
    if (sWindow != nullptr) {
        return; // presets run this function again
    }
    auto gui = Ship::Context::GetRawInstance()->GetWindow()->GetGui();
    sWindow = std::make_shared<O2rWindow>("gCoop.O2r.Window", "Co-op Mods del servidor");
    gui->AddGuiWindow(sWindow);
    sWindow->Show();
}

} // namespace

void O2rMenu_Draw() {
    ImGui::SeparatorText("Mods del juego del servidor (.o2r)");
    if (Session_IsConnected()) {
        const std::vector<o2r::Entry>& list = O2r_Required();
        if (list.empty()) {
            ImGui::TextColored(kGray, "Este servidor no usa mods del juego.");
        } else {
            for (const o2r::Entry& entry : list) {
                const char* status = O2rLoader_IsLoaded(entry.sha256) ? "cargado"
                                     : O2r_InCache(entry)            ? "descargado"
                                                                     : "falta";
                ImGui::BulletText("%s (%s) - %s", entry.name.c_str(), o2r::FormatBytes(entry.size).c_str(), status);
            }
            switch (O2r_State()) {
                case O2rState::Asking:
                    DrawQuestion("menu");
                    break;
                case O2rState::Downloading:
                    DrawProgress("menu");
                    break;
                case O2rState::Loading:
                    ImGui::TextColored(kYellow, "Cargando los mods del servidor...");
                    break;
                case O2rState::Ready:
                    ImGui::TextColored(kGreen, "Listo: tu juego tiene los mods del servidor.");
                    break;
                default:
                    break;
            }
        }
    }
    UIWidgets::CVarCheckbox("Descargar los mods del servidor sin preguntar", "gCoop.O2r.AutoDownload",
                            UIWidgets::CheckboxOptions().Color(THEME_COLOR));
    UIWidgets::CVarCheckbox(
        "Activar \"Enable Mods\" en la partida del servidor", "gCoop.O2r.AltAssets",
        UIWidgets::CheckboxOptions().Color(THEME_COLOR).DefaultValue(true).Tooltip(
            "Los mods de gráficos de 2 Ship solo se ven con \"Enable Mods\" (tecla Tab). Si los mods del servidor lo "
            "necesitan, se activa mientras juegas en su partida y al salir vuelve a como lo tenías."));
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(kGray, "Se guardan en la carpeta coop_mods, junto a 2ship.exe. Una vez cargados siguen cargados "
                              "hasta que cierres el juego. Lo que un mod cambie de la música o los sonidos no se "
                              "aplica (2 Ship carga el audio al arrancar).");
    ImGui::PopTextWrapPos();
}

} // namespace coop::client

static RegisterShipInitFunc sO2rWindowInit(coop::client::RegisterO2rWindow);
