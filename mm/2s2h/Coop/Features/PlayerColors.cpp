// [COOP] Players' colors in the game: the menu section, sending ours, and the nicks above the Links painted with each
// player's color (see PlayerColors.h; the minimap is CoopMinimap.cpp).
#include "PlayerColors.h"

#include "CoopTunics.h"

#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/I18n/CoopText.h"
#include "2s2h/Coop/Puppet/PoseCapture.h"
#include "2s2h/Coop/Puppet/PuppetManager.h"

#include "2s2h/BenGui/BenGui.hpp"
#include "2s2h/BenGui/UIWidgets.hpp"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/NameTag/NameTag.h"
#include "2s2h/ShipInit.hpp"

#include <libultraship/bridge/consolevariablebridge.h>
#include <libultraship/libultraship.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <vector>

extern "C" {
#include "variables.h"
}

namespace coop::client {

namespace {

constexpr const char* kCustom = "gCoop.Color.Custom";
constexpr const char* kMain = "gCoop.Color.Main";
constexpr const char* kSecond = "gCoop.Color.Second";
constexpr const char* kFx = "gCoop.Color.Fx";
constexpr const char* kSpeed = "gCoop.Color.Speed";
constexpr const char* kNameTags = "gCoop.Color.NameTags";
constexpr const char* kChat = "gCoop.Color.Chat";
constexpr const char* kMinimapPlayers = "gCoop.Minimap.Players";
constexpr const char* kMinimapOwnColor = "gCoop.Minimap.OwnColor";

constexpr double kSendEverySeconds = 0.25; // dragging a color picker sends at most 4 changes a second
constexpr int kSwatchesPerRow = 12;

double sLastSent = -1.0;

double Now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// Our id for the colors: the server's while connected; the first player's for the menu's preview before that.
uint8_t LocalId() {
    return Session_IsConnected() && Session_LocalId() != 0 ? Session_LocalId() : 1;
}

PlayerColor FromSettings() {
    PlayerColor c;
    c.main = RgbFromInt((uint32_t)CVarGetInteger(kMain, (int32_t)RgbToInt(DefaultPlayerColor(LocalId()).main)));
    c.second = RgbFromInt((uint32_t)CVarGetInteger(kSecond, 0xFFFFFF));
    c.fx = (ColorFx)std::clamp(CVarGetInteger(kFx, 0), 0, (int)ColorFx::Count - 1);
    c.speed = (ColorSpeed)std::clamp(CVarGetInteger(kSpeed, (int)ColorSpeed::Normal), 0, (int)ColorSpeed::Count - 1);
    return c;
}

PlayerColor LocalColor() {
    return ResolvePlayerColor(PlayerColors_SettingsPacked(), LocalId());
}

ImVec4 ToImVec4(const Rgb& c) {
    return ImVec4(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 1.0f);
}

void SaveCVars() {
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

void SetRgb(const char* cvar, const Rgb& c) {
    CVarSetInteger(cvar, (int32_t)RgbToInt(c));
    CVarSetInteger(kCustom, 1); // touching any color means "my own"
    SaveCVars();
}

// An ImGui color editor over one of our 0xRRGGBB CVars (the wheel picker opens on its square).
void ColorEditor(const char* label, const char* cvar, const Rgb& current) {
    float rgb[3] = { current.r / 255.0f, current.g / 255.0f, current.b / 255.0f };
    if (ImGui::ColorEdit3(label, rgb, ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_DisplayRGB)) {
        SetRgb(cvar, Rgb{ (uint8_t)std::lround(rgb[0] * 255.0f), (uint8_t)std::lround(rgb[1] * 255.0f),
                          (uint8_t)std::lround(rgb[2] * 255.0f) });
    }
}

void DrawPalette(const char* cvar, const char* idPrefix) {
    const std::vector<Rgb>& palette = PlayerColorPalette();
    for (size_t i = 0; i < palette.size(); i++) {
        if (i % kSwatchesPerRow != 0) {
            ImGui::SameLine();
        }
        ImGui::PushID(idPrefix);
        ImGui::PushID((int)i);
        if (ImGui::ColorButton("##swatch", ToImVec4(palette[i]), ImGuiColorEditFlags_NoTooltip, ImVec2(22, 22))) {
            SetRgb(cvar, palette[i]);
        }
        ImGui::PopID();
        ImGui::PopID();
    }
}

Rgb RandomColor() {
    static std::mt19937 rng((uint32_t)std::chrono::steady_clock::now().time_since_epoch().count());
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    return HsvToRgb(unit(rng), 0.6f + 0.4f * unit(rng), 0.8f + 0.2f * unit(rng));
}

bool UsesSecond(ColorFx fx) {
    return fx == ColorFx::Blend || fx == ColorFx::Blink;
}

// Color_RGBA8 for a nick above a Link: alpha 0 = NameTag's own white.
Color_RGBA8 TagColor(uint8_t playerId, bool on) {
    if (!on) {
        return { 0, 0, 0, 0 };
    }
    Rgb c = PlayerColors_Shown(playerId);
    return { c.r, c.g, c.b, 255 };
}

// Every frame: our color to the server (when it changed) and the nicks above the Links in each one's color.
void FrameEnd() {
    int64_t wanted = PlayerColors_SettingsPacked();
    if (wanted != Session_LocalColor()) {
        double now = Now();
        if (sLastSent < 0.0 || now - sLastSent >= kSendEverySeconds) {
            sLastSent = now;
            Session_SetLocalColor(wanted);
        }
    }
    if (!Session_IsConnected() || gPlayState == nullptr || !PoseCapture_InGameplay()) {
        return;
    }
    bool on = CVarGetInteger(kNameTags, 1) != 0;
    for (const auto& [id, player] : Session_Players()) {
        if (Actor* actor = PuppetManager_Actor(id)) {
            NameTag_SetTextColor(actor, "coop", TagColor(id, on)); // PuppetActor.cpp's tag
        }
    }
    NameTag_SetTextColor(&GET_PLAYER(gPlayState)->actor, "coop-self", TagColor(Session_LocalId(), on)); // SelfNameTag
}

void RegisterPlayerColors() {
    COND_HOOK(OnGameStateMainFinish, true, FrameEnd);
}

} // namespace

int64_t PlayerColors_SettingsPacked() {
    return CVarGetInteger(kCustom, 0) ? PackPlayerColor(FromSettings()) : kNoColor;
}

PlayerColor PlayerColors_Of(uint8_t playerId) {
    if (Session_IsConnected() && playerId == Session_LocalId()) {
        return LocalColor();
    }
    if (const RemotePlayer* player = Session_FindPlayer(playerId)) {
        return ResolvePlayerColor(player->color, playerId);
    }
    return DefaultPlayerColor(playerId);
}

Rgb PlayerColors_ShownOf(const PlayerColor& color) {
    return ShownPlayerColor(color, Now());
}

Rgb PlayerColors_Shown(uint8_t playerId) {
    return PlayerColors_ShownOf(PlayerColors_Of(playerId));
}

uint8_t PlayerColors_IdOfNick(const std::string& nick) {
    if (Session_IsConnected() && nick == Session_LocalNick()) {
        return Session_LocalId();
    }
    for (const auto& [id, player] : Session_Players()) {
        if (player.nick == nick) {
            return id;
        }
    }
    return 0;
}

bool PlayerColors_ChatOn() {
    return CVarGetInteger(kChat, 1) != 0;
}

bool PlayerColors_MinimapOn() {
    return CVarGetInteger(kMinimapPlayers, 1) != 0;
}

bool PlayerColors_MinimapOwnColor() {
    return CVarGetInteger(kMinimapOwnColor, 0) != 0;
}

void PlayerColors_DrawMenu() {
    ImGui::SeparatorText(Tc(Txt::MenuColors));
    PlayerColor mine = LocalColor();
    PlayerColor settings = FromSettings();

    // How the others see us, with the effect moving
    std::string nick = Session_IsConnected() ? Session_LocalNick() : CVarGetString("gCoop.Nick", "");
    if (nick.empty()) {
        nick = "Link";
    }
    ImGui::TextUnformatted(Tc(Txt::ColorPreview));
    ImGui::SameLine();
    Rgb shown = PlayerColors_ShownOf(mine);
    ImGui::ColorButton("##preview", ToImVec4(shown), ImGuiColorEditFlags_NoTooltip, ImVec2(16, 16));
    ImGui::SameLine();
    ImGui::TextColored(ToImVec4(shown), "%s", nick.c_str());

    bool automatic = CVarGetInteger(kCustom, 0) == 0;
    if (UIWidgets::Checkbox(Tc(Txt::ColorAuto), &automatic,
                            UIWidgets::CheckboxOptions().Color(THEME_COLOR).Tooltip(Tip(Txt::ColorAutoTip)))) {
        CVarSetInteger(kCustom, automatic ? 0 : 1);
        SaveCVars();
    }

    ImGui::TextUnformatted(Tc(Txt::ColorPalette));
    DrawPalette(kMain, "main");
    ColorEditor(Tc(Txt::ColorMain), kMain, settings.main);

    std::vector<const char*> fxNames = { Tc(Txt::ColorFxSolid), Tc(Txt::ColorFxPulse), Tc(Txt::ColorFxBlend),
                                         Tc(Txt::ColorFxRainbow), Tc(Txt::ColorFxBlink) };
    int32_t fx = (int32_t)settings.fx;
    if (UIWidgets::Combobox(Tc(Txt::ColorEffect), &fx, fxNames, UIWidgets::ComboboxOptions().Color(THEME_COLOR))) {
        CVarSetInteger(kFx, fx);
        CVarSetInteger(kCustom, 1);
        SaveCVars();
    }
    if (UsesSecond((ColorFx)fx)) {
        ColorEditor(Tc(Txt::ColorSecond), kSecond, settings.second);
        DrawPalette(kSecond, "second");
    }
    if ((ColorFx)fx != ColorFx::Solid) {
        std::vector<const char*> speedNames = { Tc(Txt::ColorSpeedSlow), Tc(Txt::ColorSpeedNormal),
                                                Tc(Txt::ColorSpeedFast) };
        int32_t speed = (int32_t)settings.speed;
        if (UIWidgets::Combobox(Tc(Txt::ColorSpeed), &speed, speedNames,
                                UIWidgets::ComboboxOptions().Color(THEME_COLOR))) {
            CVarSetInteger(kSpeed, speed);
            CVarSetInteger(kCustom, 1);
            SaveCVars();
        }
    }

    if (UIWidgets::Button(Tc(Txt::ColorRandom), UIWidgets::ButtonOptions().Color(THEME_COLOR).Size(ImVec2(0, 0)))) {
        SetRgb(kMain, RandomColor());
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(automatic);
    if (UIWidgets::Button(Tc(Txt::ColorReset), UIWidgets::ButtonOptions().Color(THEME_COLOR).Size(ImVec2(0, 0)))) {
        CVarSetInteger(kCustom, 0);
        SaveCVars();
    }
    ImGui::EndDisabled();
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", Tc(Txt::ColorHelp));
    ImGui::PopTextWrapPos();

    CoopTunics_DrawMenu(); // [COOP] the tunics in each player's color (CoopTunics.cpp)
    UIWidgets::CVarCheckbox(
        Tc(Txt::MinimapPlayers), kMinimapPlayers,
        UIWidgets::CheckboxOptions().Color(THEME_COLOR).DefaultValue(true).Tooltip(Tip(Txt::MinimapPlayersTip)));
    UIWidgets::CVarCheckbox(Tc(Txt::MinimapOwnColor), kMinimapOwnColor,
                            UIWidgets::CheckboxOptions().Color(THEME_COLOR).Tooltip(Tip(Txt::MinimapOwnColorTip)));
    UIWidgets::CVarCheckbox(Tc(Txt::ColorNameTags), kNameTags,
                            UIWidgets::CheckboxOptions().Color(THEME_COLOR).DefaultValue(true));
    UIWidgets::CVarCheckbox(Tc(Txt::ColorChat), kChat, UIWidgets::CheckboxOptions().Color(THEME_COLOR).DefaultValue(true));
}

} // namespace coop::client

static RegisterShipInitFunc sPlayerColorsInit(coop::client::RegisterPlayerColors);
