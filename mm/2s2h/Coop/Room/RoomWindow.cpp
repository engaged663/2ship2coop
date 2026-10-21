// [COOP] The activity room's window (spec 2026-10-04-coop-salas-actividades §5.5), on the right of the screen while we
// are in a room. Waiting (or just over) it is open in full: the activity, who plays it, who is ready, the pending
// invitations, inviting (the host: its group and anyone in the server's game), the settings (the host changes them),
// the room's own chat and the buttons Listo / Salir / Quitar / Cerrar. Running it shrinks to a bar (the activity, who
// is in, its progress, the last lines of the chat) with a button to open it again.
#include "Room.h"

#include "2s2h/Coop/Activities/Activities.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Group/Group.h"
#include "2s2h/Coop/I18n/CoopText.h"

#include "common/Protocol.h"

#include "2s2h/ShipInit.hpp"

#include <libultraship/bridge/consolevariablebridge.h>
#include <libultraship/libultraship.h>
#include <ship/window/gui/GuiWindow.h>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>

namespace coop::client {

namespace {

const ImVec4 kGreen = ImVec4(0.5f, 1.0f, 0.5f, 1.0f);
const ImVec4 kYellow = ImVec4(1.0f, 0.85f, 0.4f, 1.0f);
const ImVec4 kGray = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
const ImVec4 kTitle = ImVec4(0.45f, 0.9f, 0.95f, 1.0f);
constexpr int kCompactChatLines = 3;
constexpr int kChatLines = 8;

class RoomWindow : public Ship::GuiWindow {
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

std::shared_ptr<RoomWindow> sWindow;
bool sExpanded = true;
uint32_t sShownId = 0;
RoomPhase sShownPhase = RoomPhase::None;
char sInput[kChatMaxChars * 4 + 1] = {};
bool sRefocus = false;

std::string Clock(int64_t ms) {
    int64_t secs = std::max<int64_t>(0, (ms + 999) / 1000);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d:%02d", (int)(secs / 60), (int)(secs % 60));
    return buf;
}

const ActivityDef* Def(const RoomInfo& r) {
    return Activity_ByKey(r.key);
}

bool Waiting(RoomPhase p) {
    return p == RoomPhase::Lobby || p == RoomPhase::Starting;
}

void DrawState(const RoomInfo& r) {
    int64_t left = r.untilMs - Group_NowMs();
    switch (r.phase) {
        case RoomPhase::Lobby:
            ImGui::TextColored(kYellow, "%s", Tx(Txt::RoomWaitAll, { Clock(left) }).c_str());
            break;
        case RoomPhase::Starting:
            ImGui::TextColored(
                kGreen, "%s",
                Tx(Txt::RoomAllReady, { std::to_string(std::max<int64_t>(1, (left + 999) / 1000)) }).c_str());
            break;
        case RoomPhase::Running:
            ImGui::TextColored(kGreen, "%s", Tc(Txt::RoomRunning));
            break;
        case RoomPhase::Ended:
            ImGui::TextColored(kGray, "%s", Tx(Txt::RoomEnded, { Clock(left) }).c_str());
            break;
        default:
            break;
    }
    if (RoomHold_Active()) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(kYellow, "%s", Tc(Txt::RoomHeld));
        ImGui::PopTextWrapPos();
    }
    const ActivityDef* def = Def(r);
    char progress[160]; // UTF-8: Russian and Chinese take more bytes
    if (def != nullptr && def->hooks != nullptr && def->hooks->progress != nullptr &&
        def->hooks->progress(progress, sizeof(progress))) {
        ImGui::Text("%s", progress);
    }
}

// A member's line: its name, who it is, and how it stands.
void DrawMember(const RoomInfo& r, const RoomMemberInfo& m, bool compact) {
    bool me = m.id == Session_LocalId();
    std::string name = m.nick;
    if (m.id == r.host) {
        name += std::string(" ") + Tc(Txt::TagHost);
    }
    if (m.id == r.director && r.director != r.host) {
        name += std::string(" ") + Tc(Txt::TagPlays);
    }
    if (me) {
        name += std::string(" ") + Tc(Txt::TagYou);
    }
    ImGui::TextColored(me ? kGreen : ImVec4(1, 1, 1, 1), "%s", name.c_str());
    if (Waiting(r.phase)) {
        ImGui::SameLine();
        if (m.ready) {
            ImGui::TextColored(kGreen, "%s", Tc(Txt::MemberReady));
        } else if (r.place == "here" && !m.here) {
            ImGui::TextColored(kYellow, "%s", Tc(Txt::MemberTravelling));
        } else {
            ImGui::TextColored(kGray, "%s", Tc(Txt::MemberWaiting));
        }
    } else if (!compact && (m.score >= 0 || m.cs >= 0 || m.won >= 0)) {
        std::string result;
        if (m.won >= 0) {
            result = Tc(m.won ? Txt::ResultWon : Txt::ResultLost);
        }
        if (m.score > 0) {
            result += (result.empty() ? "" : ", ") + Tx(Txt::Points, { std::to_string(m.score) });
        }
        if (m.cs > 0) {
            result += (result.empty() ? "" : ", ") + Activity_TimeText(m.cs);
        }
        ImGui::SameLine();
        ImGui::TextColored(kGray, "- %s", result.c_str());
    }
    if (!compact && Room_IsHost() && !me) {
        ImGui::SameLine();
        ImGui::PushID(m.id);
        if (ImGui::SmallButton(Tc(Txt::RoomKick))) {
            Room_Kick(m.id);
        }
        ImGui::PopID();
    }
}

void DrawInvite(const RoomInfo& r) {
    ImGui::SeparatorText(Tc(Txt::RoomInviteTitle));
    bool groupLeft = false;
    for (const GroupMember& m : Group_Members()) {
        if (m.id != Session_LocalId() && !Room_IsMate(m.id)) {
            groupLeft = true;
        }
    }
    if (groupLeft && ImGui::SmallButton(Tc(Txt::RoomInviteGroup))) {
        Room_InviteGroup();
    }
    int shown = 0;
    for (const auto& [id, player] : Session_Players()) {
        if (Room_IsMate(id)) {
            continue;
        }
        bool pending = std::any_of(r.invites.begin(), r.invites.end(),
                                   [&](const RoomPendingInvite& i) { return i.id == id; });
        ImGui::PushID(2000 + id);
        ImGui::Text("%s", player.nick.c_str());
        ImGui::SameLine();
        if (pending) {
            ImGui::TextColored(kGray, "%s", Tc(Txt::RoomInvited));
        } else if (ImGui::SmallButton(Tc(Txt::Invite))) {
            Room_Invite(id);
        }
        ImGui::PopID();
        shown++;
    }
    if (shown == 0 && !groupLeft) {
        ImGui::TextColored(kGray, "%s", Tc(Txt::RoomNobodyToInvite));
    }
}

void DrawSettings(const RoomInfo& r) {
    ImGui::SeparatorText(Tc(Room_IsHost() ? Txt::RoomSettings : Txt::RoomSettingsGuest));
    bool open = r.open;
    bool travel = r.travel;
    bool rewards = r.rewards;
    ImGui::BeginDisabled(!Room_IsHost());
    bool changed = ImGui::Checkbox(Tc(Txt::RoomSetOpen), &open);
    changed |= ImGui::Checkbox(Tc(Txt::RoomSetTravel), &travel);
    changed |= ImGui::Checkbox(Tc(Txt::RoomSetRewards), &rewards);
    ImGui::EndDisabled();
    if (changed && Room_IsHost()) {
        Room_Settings(open, travel, rewards);
    }
}

void DrawChat(int lines, bool input) {
    const std::vector<RoomChatLine>& chat = Room_ChatLines();
    size_t first = chat.size() > (size_t)lines ? chat.size() - lines : 0;
    ImGui::PushTextWrapPos(0.0f);
    for (size_t i = first; i < chat.size(); i++) {
        ImGui::TextColored(kTitle, "%s:", chat[i].nick.c_str());
        ImGui::SameLine();
        ImGui::TextWrapped("%s", chat[i].text.c_str());
    }
    ImGui::PopTextWrapPos();
    if (!input) {
        return;
    }
    if (sRefocus) {
        ImGui::SetKeyboardFocusHere(); // after sending: keep typing (and Enter never reaches the main chat)
        sRefocus = false;
    }
    ImGui::PushItemWidth(-60.0f);
    bool send = ImGui::InputText("##CoopRoomChat", sInput, sizeof(sInput), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    send |= ImGui::Button(Tc(Txt::RoomSend));
    if (send && sInput[0] != '\0') {
        Room_Chat(sInput);
        sInput[0] = '\0';
        sRefocus = true;
    }
}

void DrawButtons(const RoomInfo& r) {
    const RoomMemberInfo* me = Room_Me();
    if (Waiting(r.phase) && me != nullptr) {
        if (!me->ready) {
            if (ImGui::Button(Tc(r.director == Session_LocalId() ? Txt::RoomReadyStart : Txt::RoomReady))) {
                Room_SetReady(true);
            }
        } else if (ImGui::Button(Tc(Txt::RoomNotReady))) {
            Room_SetReady(false);
        }
        ImGui::SameLine();
    }
    if (ImGui::Button(Tc(Waiting(r.phase) ? Txt::RoomNoParticipate : Txt::RoomLeave))) {
        Room_Leave();
    }
    if (Room_IsHost()) {
        ImGui::SameLine();
        if (ImGui::Button(Tc(Txt::RoomClose))) {
            Room_Close();
        }
    }
}

void DrawCompact(const RoomInfo& r) {
    DrawState(r);
    for (const RoomMemberInfo& m : r.members) {
        DrawMember(r, m, true);
    }
    DrawChat(kCompactChatLines, false);
}

void DrawFull(const RoomInfo& r) {
    const char* mode = Activity_ModeText(r.mode);
    if (mode[0] != '\0') {
        ImGui::TextColored(kGray, "%s", mode);
    }
    DrawState(r);
    ImGui::SeparatorText(Tx(Txt::RoomParticipants, { std::to_string(r.members.size()) }).c_str());
    for (const RoomMemberInfo& m : r.members) {
        DrawMember(r, m, false);
    }
    int64_t now = Group_NowMs();
    for (const RoomPendingInvite& i : r.invites) {
        ImGui::TextColored(
            kGray, "%s",
            Tx(Txt::RoomInvitedLine, { i.nick, std::to_string(std::max<int64_t>(0, (i.expiresMs - now + 999) / 1000)) })
                .c_str());
    }
    if (Room_IsHost()) {
        DrawInvite(r);
    }
    DrawSettings(r);
    ImGui::SeparatorText(Tc(Txt::RoomChatTitle));
    DrawChat(kChatLines, true);
    ImGui::Spacing();
    DrawButtons(r);
}

void RoomWindow::Draw() {
    if (!Session_IsConnected() || !Room_Has()) {
        sShownId = 0;
        return;
    }
    const RoomInfo& r = Room_Get();
    // Open in full when a room (or a round) starts waiting or is over; small while it runs
    if (r.id != sShownId || r.phase != sShownPhase) {
        if (r.phase == RoomPhase::Running) {
            sExpanded = false;
        } else if (r.id != sShownId || r.phase == RoomPhase::Lobby || r.phase == RoomPhase::Ended) {
            sExpanded = true;
        }
        sShownId = r.id;
        sShownPhase = r.phase;
    }
    ImGuiViewport* vp = ImGui::GetMainViewport();
    float width = std::clamp(vp->Size.x * 0.30f, 340.0f, 620.0f);
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x - 16.0f, vp->Pos.y + vp->Size.y * 0.22f), ImGuiCond_Always,
                            ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(width, 0.0f), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.04f, 0.05f, 0.10f, sExpanded ? 0.90f : 0.65f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize;
    if (ImGui::Begin("##CoopRoom", nullptr, flags)) {
        ImGui::SetWindowFontScale(CVarGetFloat("gCoop.Room.Scale", 1.1f));
        ImGui::TextColored(kTitle, "%s", Tx(Txt::RoomWindowTitle, { Activity_ShownName(r.key, r.name) }).c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton(Tc(sExpanded ? Txt::RoomCollapse : Txt::RoomExpand))) {
            sExpanded = !sExpanded;
        }
        if (sExpanded) {
            DrawFull(r);
        } else {
            DrawCompact(r);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void RegisterRoomWindow() {
    if (sWindow != nullptr) {
        return; // presets run this function again
    }
    auto gui = Ship::Context::GetRawInstance()->GetWindow()->GetGui();
    sWindow = std::make_shared<RoomWindow>("gCoop.Room.Window", "Co-op Room");
    gui->AddGuiWindow(sWindow);
    sWindow->Show();
}

} // namespace

void RoomWindow_Show() {
    sExpanded = true;
    if (sWindow != nullptr) {
        sWindow->Show();
    }
}

} // namespace coop::client

static RegisterShipInitFunc sRoomWindowInit(coop::client::RegisterRoomWindow);
