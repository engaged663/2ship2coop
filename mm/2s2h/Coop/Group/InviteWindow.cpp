// [COOP] The pending invitations at the top of the screen: "Ana te invita a Galería de tiro (45 s)" with Aceptar and
// Rechazar (the same as /aceptar Ana and /rechazar Ana in the chat).
// Activity rooms' invitations too: "Ana te invita a su sala: Galería de tiro (45 s)" with Participar and Rechazar.
#include "Group.h"

#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Room/Room.h"

#include "2s2h/ShipInit.hpp"

#include <libultraship/bridge/consolevariablebridge.h>
#include <libultraship/libultraship.h>
#include <ship/window/gui/GuiWindow.h>

#include <algorithm>
#include <memory>

namespace coop::client {

namespace {

class InviteWindow : public Ship::GuiWindow {
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

std::shared_ptr<InviteWindow> sWindow;

void InviteWindow::Draw() {
    if (!Session_IsConnected()) {
        return;
    }
    std::vector<GroupInvite> invites = Group_Invites();           // a copy: a click changes the list
    std::vector<RoomInvitation> rooms = Room_Invitations(); // invitations to an activity room
    if (invites.empty() && rooms.empty()) {
        return;
    }
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + 12.0f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.12f, 0.85f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize;
    if (ImGui::Begin("##CoopInvites", nullptr, flags)) {
        ImGui::SetWindowFontScale(CVarGetFloat("gCoop.Chat.Scale", 1.4f));
        int64_t now = Group_NowMs();
        for (const GroupInvite& inv : invites) {
            int secs = (int)std::max<int64_t>(0, (inv.expiresMs - now + 999) / 1000);
            std::string what = inv.name.empty() ? std::string("su grupo") : inv.name;
            ImGui::Text("%s te invita a %s (%d s)", inv.nick.c_str(), what.c_str(), secs);
            ImGui::PushID(inv.from);
            if (ImGui::Button("Aceptar")) {
                Group_Accept(inv.nick);
            }
            ImGui::SameLine();
            if (ImGui::Button("Rechazar")) {
                Group_Decline(inv.nick);
            }
            ImGui::PopID();
        }
        for (const RoomInvitation& inv : rooms) {
            int secs = (int)std::max<int64_t>(0, (inv.expiresMs - now + 999) / 1000);
            ImGui::Text("%s te invita a su sala: %s (%d s)", inv.nick.c_str(), inv.name.c_str(), secs);
            ImGui::PushID(10000 + (int)inv.room);
            if (ImGui::Button("Participar")) {
                Room_Answer(inv.room, true);
            }
            ImGui::SameLine();
            if (ImGui::Button("Rechazar")) {
                Room_Answer(inv.room, false);
            }
            ImGui::PopID();
        }
        ImGui::TextDisabled(rooms.empty() ? "(o en el chat: /aceptar, /rechazar)"
                                          : "(o en el chat: /aceptar, /rechazar, /sala aceptar, /sala rechazar)");
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void RegisterInviteWindow() {
    if (sWindow != nullptr) {
        return; // presets run this function again
    }
    auto gui = Ship::Context::GetRawInstance()->GetWindow()->GetGui();
    sWindow = std::make_shared<InviteWindow>("gCoop.Group.InviteWindow", "Co-op Invitaciones");
    gui->AddGuiWindow(sWindow);
    sWindow->Show();
}

} // namespace

} // namespace coop::client

static RegisterShipInitFunc sInviteWindowInit(coop::client::RegisterInviteWindow);
