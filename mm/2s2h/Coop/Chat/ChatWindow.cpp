#include "ChatWindow.h"

#include "ChatModel.h"

#include "2s2h/Coop/Client/Session.h"

#include "2s2h/ShipInit.hpp"

#include <libultraship/bridge/consolevariablebridge.h>
#include <libultraship/libultraship.h>

#include <algorithm>
#include <cstring>
#include <memory>

namespace coop::client {

namespace {

constexpr int32_t kChatInputBlockId = 73216001; // any id unique among ControlDeck blockers
constexpr double kLineLifetime = 10.0;           // seconds a line stays visible while closed
constexpr double kFadeTime = 2.0;
constexpr int kClosedLines = 6;
constexpr int kOpenLines = 12;

std::shared_ptr<ChatWindow> sWindow;

ImVec4 ColorFor(ChatKind kind) {
    switch (kind) {
        case ChatKind::Private:
            return ImVec4(0.85f, 0.65f, 1.0f, 1.0f);
        case ChatKind::Info:
            return ImVec4(1.0f, 0.85f, 0.4f, 1.0f);
        case ChatKind::Ok:
            return ImVec4(0.5f, 1.0f, 0.5f, 1.0f);
        case ChatKind::Warn:
            return ImVec4(1.0f, 0.65f, 0.3f, 1.0f);
        case ChatKind::Error:
            return ImVec4(1.0f, 0.45f, 0.45f, 1.0f);
        case ChatKind::Presence:
            return ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
        case ChatKind::Room:
            return ImVec4(0.45f, 0.9f, 0.95f, 1.0f);
        case ChatKind::Chat:
        default:
            return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

int HistoryCallback(ImGuiInputTextCallbackData* data) {
    auto* self = static_cast<ChatWindow*>(data->UserData);
    const auto& history = Chat_SentHistory();
    if (history.empty()) {
        return 0;
    }
    int size = (int)history.size();
    if (data->EventKey == ImGuiKey_UpArrow) {
        self->mHistoryPos = self->mHistoryPos < 0 ? size - 1 : std::max(0, self->mHistoryPos - 1);
    } else if (data->EventKey == ImGuiKey_DownArrow) {
        self->mHistoryPos = self->mHistoryPos < 0 ? -1 : self->mHistoryPos + 1;
        if (self->mHistoryPos >= size) {
            self->mHistoryPos = -1;
        }
    }
    const char* text = self->mHistoryPos >= 0 ? history[self->mHistoryPos].c_str() : "";
    data->DeleteChars(0, data->BufTextLen);
    data->InsertChars(0, text);
    return 0;
}

bool TypedSlashThisFrame() {
    if (ImGui::IsKeyPressed(ImGuiKey_Slash, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadDivide, false)) {
        return true;
    }
    for (ImWchar c : ImGui::GetIO().InputQueueCharacters) {
        if (c == '/') {
            return true;
        }
    }
    return false;
}

} // namespace

void ChatWindow::Open(const char* initialText) {
    mOpen = true;
    mFramesOpen = 0;
    mHistoryPos = -1;
    std::strncpy(mBuffer, initialText, sizeof(mBuffer) - 1);
    mBuffer[sizeof(mBuffer) - 1] = '\0';
    Ship::Context::GetRawInstance()->GetControlDeck()->BlockGameInput(kChatInputBlockId);
}

void ChatWindow::Close() {
    mOpen = false;
    mBuffer[0] = '\0';
    Ship::Context::GetRawInstance()->GetControlDeck()->UnblockGameInput(kChatInputBlockId);
}

void ChatWindow::HandleOpenKeys() {
    if (!Session_IsConnected()) {
        if (mOpen) {
            Close();
        }
        return;
    }
    if (mOpen || ImGui::GetIO().WantTextInput || ImGui::GetTopMostPopupModal() != nullptr) {
        return; // already typing somewhere else (menus, popups...)
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
        Open("");
    } else if (TypedSlashThisFrame()) {
        Open("/");
    }
}

void ChatWindow::Draw() {
    HandleOpenKeys();

    const auto& lines = Chat_Lines();
    double now = Chat_Now();
    if (!mOpen && (lines.empty() || now - lines.back().time > kLineLifetime)) {
        return;
    }

    ImGuiViewport* vp = ImGui::GetMainViewport();
    float scale = CVarGetFloat("gCoop.Chat.Scale", 1.4f);
    float opacity = CVarGetFloat("gCoop.Chat.Opacity", 0.45f);
    float width = std::clamp(vp->Size.x * 0.42f, 320.0f, 900.0f);

    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + 16.0f, vp->Pos.y + vp->Size.y * 0.80f), ImGuiCond_Always,
                            ImVec2(0.0f, 1.0f));
    ImGui::SetNextWindowSize(ImVec2(width, 0.0f), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, mOpen ? opacity : opacity * 0.6f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav |
                             ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize;
    if (!mOpen) {
        flags |= ImGuiWindowFlags_NoInputs;
    }
    if (ImGui::Begin("##CoopChat", nullptr, flags)) {
        ImGui::SetWindowFontScale(scale);
        DrawLines(mOpen);
        if (mOpen) {
            DrawInput();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void ChatWindow::DrawLines(bool open) {
    const auto& lines = Chat_Lines();
    double now = Chat_Now();
    int maxLines = open ? kOpenLines : kClosedLines;
    size_t first = lines.size() > (size_t)maxLines ? lines.size() - maxLines : 0;

    ImGui::PushTextWrapPos(0.0f);
    for (size_t i = first; i < lines.size(); i++) {
        const ChatLine& line = lines[i];
        double age = now - line.time;
        if (!open && age > kLineLifetime) {
            continue;
        }
        ImVec4 color = ColorFor(line.kind);
        if (!open && age > kLineLifetime - kFadeTime) {
            color.w = (float)std::max(0.0, (kLineLifetime - age) / kFadeTime);
        }
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextWrapped("%s", line.text.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::PopTextWrapPos();
}

void ChatWindow::DrawInput() {
    // The key that opened the chat must not also submit it: take focus from the next frame on.
    if (mFramesOpen++ == 0) {
        return;
    }
    if (mFramesOpen == 2) {
        ImGui::SetKeyboardFocusHere();
    }
    ImGui::PushItemWidth(-1.0f);
    ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory;
    bool submitted = ImGui::InputText("##CoopChatInput", mBuffer, sizeof(mBuffer), inputFlags, HistoryCallback, this);
    ImGui::PopItemWidth();
    if (submitted) {
        std::string text = mBuffer;
        Close();
        Chat_Submit(text);
    } else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        Close();
    }
}

static void ChatWindow_Register() {
    if (sWindow != nullptr) {
        return; // presets run this function again
    }
    auto gui = Ship::Context::GetRawInstance()->GetWindow()->GetGui();
    sWindow = std::make_shared<ChatWindow>("gCoop.Chat.Window", "Co-op Chat");
    gui->AddGuiWindow(sWindow);
    sWindow->Show();
}

} // namespace coop::client

static RegisterShipInitFunc sChatWindowInit(coop::client::ChatWindow_Register);
