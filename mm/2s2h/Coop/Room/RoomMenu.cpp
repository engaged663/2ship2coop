// [COOP] Co-op menu (F1), section "Salas de actividad": our room (open its window, leave it), the open rooms of the
// server's game (Unirme) and the player's options (gCoop.Room.*).
#include "Room.h"

#include "2s2h/Coop/Activities/Activities.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/I18n/CoopText.h"
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
            return Tc(Txt::RoomPhaseLobby);
        case RoomPhase::Starting:
            return Tc(Txt::RoomPhaseStarting);
        case RoomPhase::Running:
            return Tc(Txt::RoomPhaseRunning);
        case RoomPhase::Ended:
            return Tc(Txt::RoomPhaseEnded);
        default:
            return "";
    }
}

const char* StateText(const std::string& state) {
    if (state == "lobby") {
        return Tc(Txt::RoomStateWaiting);
    }
    if (state == "starting") {
        return Tc(Txt::RoomStateStarting);
    }
    if (state == "running") {
        return Tc(Txt::RoomPhaseRunning);
    }
    return Tc(Txt::RoomPhaseEnded);
}

void DrawOwn() {
    if (!Room_Has()) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(kGray, "%s", Tc(Txt::RoomNoneHint));
        ImGui::PopTextWrapPos();
        return;
    }
    const RoomInfo& r = Room_Get();
    ImGui::Text("%s", Tx(Txt::RoomYours, { Activity_ShownName(r.key, r.name), StateText(r.phase),
                                           std::to_string(r.members.size()) })
                          .c_str());
    if (UIWidgets::Button(Tc(Txt::RoomOpenWindow),
                          UIWidgets::ButtonOptions().Color(THEME_COLOR).Size(UIWidgets::Sizes::Inline))) {
        RoomWindow_Show();
    }
    ImGui::SameLine();
    if (UIWidgets::Button(Tc(Txt::RoomLeave),
                          UIWidgets::ButtonOptions().Color(THEME_COLOR).Size(UIWidgets::Sizes::Inline))) {
        Room_Leave();
    }
}

void DrawOpen() {
    const std::vector<OpenRoom>& open = Room_OpenRooms();
    ImGui::Text("%s", Tc(Txt::RoomOpenList));
    if (open.empty()) {
        ImGui::TextColored(kGray, "%s", Tc(Txt::RoomOpenNone));
        return;
    }
    for (const OpenRoom& o : open) {
        ImGui::PushID((int)o.id);
        ImGui::Text("%s - %s (%d/4, %s)", Activity_ShownName(o.key, o.name).c_str(), o.nick.c_str(), o.count,
                    StateText(o.state));
        if (Room_Get().id != o.id) {
            ImGui::SameLine();
            if (ImGui::SmallButton(Tc(Txt::RoomJoin))) {
                Room_Join(o.id);
            }
        }
        ImGui::PopID();
    }
}

void DrawOptions() {
    auto opts = [](Txt tooltip) {
        return UIWidgets::CheckboxOptions().Color(THEME_COLOR).DefaultValue(true).Tooltip(Tip(tooltip));
    };
    UIWidgets::CVarCheckbox(Tc(Txt::RoomOptEnabled), "gCoop.Room.Enabled", opts(Txt::RoomOptEnabledTip));
    UIWidgets::CVarCheckbox(Tc(Txt::RoomOptHold), "gCoop.Room.Hold", opts(Txt::RoomOptHoldTip));
    UIWidgets::CVarSliderFloat(
        Tc(Txt::RoomScale), "gCoop.Room.Scale",
        UIWidgets::FloatSliderOptions().Color(THEME_COLOR).Min(0.8f).Max(2.5f).DefaultValue(1.1f).Format("%.1f"));
}

} // namespace

void RoomMenu_Draw() {
    ImGui::SeparatorText(Tc(Txt::RoomsTitle));
    if (!Session_IsConnected() || !WorldSession_InWorld()) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(kGray, "%s", Tc(Txt::RoomsEnterFirst));
        ImGui::PopTextWrapPos();
    } else {
        DrawOwn();
        ImGui::Spacing();
        DrawOpen();
    }
    DrawOptions();
}

} // namespace coop::client
