#include "CoopMenu.h"

#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Group/Group.h"
#include "2s2h/Coop/I18n/CoopText.h"
#include "2s2h/Coop/O2r/O2r.h"
#include "2s2h/Coop/Room/Room.h"
#include "2s2h/Coop/Sync/Sync.h"
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
    UIWidgets::CVarInputString(Tc(Txt::MenuNick), "gCoop.Nick", UIWidgets::InputOptions().Color(THEME_COLOR));
    UIWidgets::CVarInputString(Tc(Txt::MenuHost), "gCoop.Host",
                               UIWidgets::InputOptions().Color(THEME_COLOR).DefaultValue("127.0.0.1"));
    int32_t port = CVarGetInteger("gCoop.Port", coop::kDefaultPort);
    if (UIWidgets::InputInt(Tc(Txt::MenuPort), &port, UIWidgets::InputOptions().Color(THEME_COLOR))) {
        CVarSetInteger("gCoop.Port", port);
        SaveCVars();
    }
    UIWidgets::CVarInputString(Tc(Txt::MenuPassword), "gCoop.Password",
                               UIWidgets::InputOptions().Color(THEME_COLOR).IsSecret(true));
    ImGui::EndDisabled();

    ImGui::Spacing();
    if (idle) {
        if (UIWidgets::Button(Tc(Txt::MenuConnect), UIWidgets::ButtonOptions().Color(THEME_COLOR))) {
            Session_ConnectFromSettings();
        }
    } else {
        const char* label = state == ConnState::Connected ? Tc(Txt::MenuDisconnect) : Tc(Txt::Cancel);
        if (UIWidgets::Button(label, UIWidgets::ButtonOptions().Color(THEME_COLOR))) {
            NetClient::Get().Disconnect();
        }
    }

    switch (state) {
        case ConnState::Disconnected:
            ImGui::TextColored(kGray, "%s", Tc(Txt::StatusDisconnected));
            break;
        case ConnState::Connecting:
            ImGui::TextColored(kYellow, "%s", Tx(Txt::StatusConnecting, { NetClient::Get().ServerLabel() }).c_str());
            break;
        case ConnState::Handshaking:
            ImGui::TextColored(kYellow, "%s", Tc(Txt::StatusHandshaking));
            break;
        case ConnState::Connected:
            ImGui::TextColored(kGreen, "%s",
                               Tx(Txt::StatusConnected, { NetClient::Get().ServerLabel(), Session_LocalNick(),
                                                          std::to_string(NetClient::Get().PingMs()) })
                                   .c_str());
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
    ImGui::SeparatorText(Tc(Txt::MenuPlayers));
    ImGui::TextColored(kGreen, "%s %s", Session_LocalNick().c_str(), Tc(Txt::TagYou));
    for (const auto& [id, player] : Session_Players()) {
        if (player.sceneName.empty()) {
            ImGui::Text("%s", player.nick.c_str());
        } else {
            ImGui::Text("%s - %s", player.nick.c_str(), player.sceneName.c_str());
        }
    }
}

void DrawWorld() {
    ImGui::SeparatorText(Tc(Txt::MenuWorld));
    WorldState state = WorldSession_State();
    if (state == WorldState::Outside) {
        ImGui::BeginDisabled(!Session_IsConnected() || !O2r_Ready()); // the server's .o2r mods first
        if (UIWidgets::Button(Tc(Txt::WorldEnter),
                              UIWidgets::ButtonOptions().Color(THEME_COLOR).Tooltip(Tip(Txt::WorldEnterTip)))) {
            WorldSession_RequestEnter();
        }
        ImGui::EndDisabled();
    } else if (UIWidgets::Button(Tc(Txt::WorldLeave),
                                 UIWidgets::ButtonOptions().Color(THEME_COLOR).Tooltip(Tip(Txt::WorldLeaveTip)))) {
        WorldSession_RequestLeave();
    }
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(state == WorldState::Active ? kGreen : kGray, "%s", WorldSession_StatusText().c_str());
    ImGui::PopTextWrapPos();
    UIWidgets::CVarCheckbox(Tc(Txt::AutoEnter), "gCoop.AutoEnter",
                            UIWidgets::CheckboxOptions().Color(THEME_COLOR).Tooltip(Tip(Txt::AutoEnterTip)));
}

void DrawOptions() {
    ImGui::SeparatorText(Tc(Txt::MenuOptions));
    UIWidgets::CVarCheckbox(Tc(Txt::AutoConnect), "gCoop.AutoConnect", UIWidgets::CheckboxOptions().Color(THEME_COLOR));
    UIWidgets::CVarCheckbox(Tc(Txt::ShowOwnName), "gCoop.ShowOwnNameTag",
                            UIWidgets::CheckboxOptions().Color(THEME_COLOR));
    UIWidgets::CVarCheckbox(
        Tc(Txt::LiveMenu), "gCoop.LiveMenu",
        UIWidgets::CheckboxOptions().Color(THEME_COLOR).DefaultValue(true).Tooltip(Tip(Txt::LiveMenuTip)));
    UIWidgets::CVarCheckbox(
        Tc(Txt::ModsAllow), "gCoop.Mods",
        UIWidgets::CheckboxOptions().Color(THEME_COLOR).DefaultValue(true).Tooltip(Tip(Txt::ModsAllowTip)));
    UIWidgets::CVarSliderFloat(
        Tc(Txt::ChatScale), "gCoop.Chat.Scale",
        UIWidgets::FloatSliderOptions().Color(THEME_COLOR).Min(0.8f).Max(3.0f).DefaultValue(1.4f).Format("%.1f"));
    UIWidgets::CVarSliderFloat(
        Tc(Txt::ChatOpacity), "gCoop.Chat.Opacity",
        UIWidgets::FloatSliderOptions().Color(THEME_COLOR).Min(0.0f).Max(1.0f).DefaultValue(0.45f).Format("%.2f"));
    ImGui::Spacing();
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(kGray, "%s", Tc(Txt::ChatHelp));
    ImGui::PopTextWrapPos();

    ImGui::SeparatorText(Tc(Txt::MenuTesting));
    UIWidgets::CVarCheckbox(Tc(Txt::BootClockTown), "gCoop.Debug.BootToClockTown",
                            UIWidgets::CheckboxOptions().Color(THEME_COLOR).Tooltip(Tip(Txt::BootClockTownTip)));
}

void RegisterCoopMenu() {
    // The sidebar's name is chosen once, in the language of the start (gCoop.Language, or the system's)
    CoopText_FrameStart();
    std::string section = Tc(Txt::MenuConnection);
    BenGui::mBenMenu->AddMenuEntry("Co-op", "gSettings.Menu.CoopSidebarSection");
    BenGui::mBenMenu->AddSidebarEntry("Co-op", section, 1);
    WidgetPath path = { "Co-op", section, SECTION_COLUMN_1 };
    BenGui::mBenMenu->AddWidget(path, "Co-op", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) { CoopMenu_Draw(); });
}

} // namespace

void CoopMenu_Draw() {
    CoopText_DrawMenu();
    ImGui::SeparatorText(Tc(Txt::MenuConnection));
    DrawConnection();
    O2rMenu_Draw();
    DrawPlayers();
    DrawWorld();
    GroupMenu_Draw();
    RoomMenu_Draw();
    SyncMenu_Draw();
    DrawOptions();
}

static RegisterMenuInitFunc sCoopMenuInit(RegisterCoopMenu);
