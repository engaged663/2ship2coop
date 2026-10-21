// [COOP] See CoopFonts.h.
#include "CoopFonts.h"

#include "CoopText.h"

#include <libultraship/bridge/consolevariablebridge.h>
#include <libultraship/libultraship.h>
#include <ship/resource/File.h>
#include <ship/resource/ResourceManager.h>
#include <ship/window/gui/resource/Font.h>
#include <spdlog/spdlog.h>

#include <imgui_impl_opengl3.h>
#ifdef _WIN32
#include <imgui_impl_dx11.h>
#endif

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <vector>

namespace {

// Latin, Cyrillic, punctuation (… — « » ·) and arrows. Chinese is added apart (it is big).
const ImWchar kBaseRanges[] = { 0x0020, 0x00FF, 0x0100, 0x017F, 0x0400, 0x052F, 0x2000, 0x206F, 0x2190, 0x21FF, 0 };
const ImWchar kCyrillicRanges[] = { 0x0400, 0x052F, 0x2000, 0x206F, 0x2190, 0x21FF, 0 };
const ImWchar kCjkPunctuation[] = { 0x3000, 0x303F, 0xFF00, 0xFFEF, 0 };

struct CoopFont {
    ImFont* font;
    float size;
};
std::vector<CoopFont> sMade;      // every font BenPort.cpp made, with its size
std::vector<ImFont*> sCoopFonts;  // the ones that get Chinese
std::shared_ptr<Ship::Font> sMontserrat; // Inconsolata's Cyrillic (ImGui copies it: kept only while fonts are made)
ImVector<ImWchar> sCjkRanges;     // ImGui reads them at every build of the atlas: they live as long as the game
bool sWantChinese = false;
bool sChineseIn = false;          // merged into sCoopFonts
bool sChineseMissing = false;     // no font with Chinese on this system

std::shared_ptr<Ship::Font> LoadGameFont(const std::string& path) {
    auto initData = std::make_shared<Ship::ResourceInitData>();
    initData->Format = RESOURCE_FORMAT_BINARY;
    initData->Type = static_cast<uint32_t>(RESOURCE_TYPE_FONT);
    initData->ResourceVersion = 0;
    initData->Path = path;
    return std::static_pointer_cast<Ship::Font>(
        Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(path, false, initData));
}

std::vector<std::string> ChineseFontFiles() {
    std::vector<std::string> files;
    std::string custom = CVarGetString("gCoop.ChineseFont", "");
    if (!custom.empty()) {
        files.push_back(custom);
    }
#ifdef _WIN32
    const char* windir = std::getenv("WINDIR");
    std::string fonts = std::string(windir != nullptr ? windir : "C:\\Windows") + "\\Fonts\\";
    for (const char* name : { "msyh.ttc", "msyh.ttf", "simhei.ttf", "Deng.ttf", "simsun.ttc", "msjh.ttc" }) {
        files.push_back(fonts + name);
    }
#elif defined(__APPLE__)
    files.push_back("/System/Library/Fonts/PingFang.ttc");
    files.push_back("/System/Library/Fonts/STHeiti Light.ttc");
    files.push_back("/Library/Fonts/Arial Unicode.ttf");
#else
    files.push_back("/usr/share/fonts/truetype/wqy/wqy-microhei.ttc");
    files.push_back("/usr/share/fonts/wenquanyi/wqy-microhei/wqy-microhei.ttc");
    files.push_back("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc");
    files.push_back("/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc");
    files.push_back("/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf");
#endif
    return files;
}

bool ReadFile(const std::string& path, std::vector<char>& out) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) {
        return false;
    }
    std::streamsize size = in.tellg();
    if (size <= 100 || size > 128 * 1024 * 1024) {
        return false;
    }
    out.resize((size_t)size);
    in.seekg(0);
    return (bool)in.read(out.data(), size);
}

// The common Chinese characters plus every one our texts and the server's use.
void BuildChineseRanges() {
    if (!sCjkRanges.empty()) {
        return;
    }
    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesChineseSimplifiedCommon());
    builder.AddRanges(kCjkPunctuation);
    for (size_t i = 0; i < (size_t)coop::client::Txt::Count; i++) {
        builder.AddText(coop::client::TcIn(coop::Lang::Zh, (coop::client::Txt)i));
    }
    for (size_t i = 0; i < (size_t)coop::Msg::Count; i++) {
        builder.AddText(coop::TrRaw(coop::Lang::Zh, (coop::Msg)i));
    }
    builder.BuildRanges(&sCjkRanges);
}

// The atlas changed after the renderer made its texture: the renderer makes it again in its next NewFrame.
// false: a renderer we cannot ask (its texture would stay the old one).
bool CanRemakeTexture() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.BackendRendererUserData == nullptr || io.BackendRendererName == nullptr) {
        return true; // no renderer yet (or the headless one, which builds the atlas when it is not built)
    }
    return std::strcmp(io.BackendRendererName, "imgui_impl_opengl3") == 0
#ifdef _WIN32
           || std::strcmp(io.BackendRendererName, "imgui_impl_dx11") == 0
#endif
        ;
}

void RemakeTexture() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.BackendRendererUserData == nullptr || io.BackendRendererName == nullptr) {
        return;
    }
    if (std::strcmp(io.BackendRendererName, "imgui_impl_opengl3") == 0) {
        ImGui_ImplOpenGL3_DestroyFontsTexture();
    }
#ifdef _WIN32
    if (std::strcmp(io.BackendRendererName, "imgui_impl_dx11") == 0) {
        ImGui_ImplDX11_InvalidateDeviceObjects();
    }
#endif
}

void AddChinese() {
    if (sCoopFonts.empty() || !CanRemakeTexture()) {
        return;
    }
    std::vector<char> data;
    std::string used;
    for (const std::string& path : ChineseFontFiles()) {
        if (ReadFile(path, data)) {
            used = path;
            break;
        }
    }
    if (used.empty()) {
        sChineseMissing = true;
        SPDLOG_WARN("[Coop] No font with Chinese characters was found (gCoop.ChineseFont: a .ttf/.ttc of your own)");
        return;
    }
    BuildChineseRanges();
    ImFontAtlas* atlas = ImGui::GetIO().Fonts;
    for (ImFont* font : sCoopFonts) {
        float size = 0.0f;
        for (const CoopFont& made : sMade) {
            size = made.font == font ? made.size : size;
        }
        if (size <= 0.0f) {
            continue;
        }
        ImFontConfig cfg;
        cfg.MergeMode = true;
        cfg.DstFont = font;
        cfg.FontDataOwnedByAtlas = false; // ImGui keeps a copy of its own
        cfg.OversampleH = 1;
        cfg.OversampleV = 1;
        cfg.PixelSnapH = true;
        atlas->AddFontFromMemoryTTF(data.data(), (int)data.size(), size, &cfg, sCjkRanges.Data);
    }
    RemakeTexture();
    sChineseIn = true;
    SPDLOG_INFO("[Coop] Chinese characters from {} added to {} font(s)", used, sCoopFonts.size());
}

} // namespace

const ImWchar* CoopFonts_BaseRanges() {
    return kBaseRanges;
}

void CoopFonts_OnFontCreated(ImFont* font, float size, const std::string& path) {
    if (font == nullptr) {
        return;
    }
    sMade.push_back({ font, size });
    if (path.find("Inconsolata") == std::string::npos) {
        return;
    }
    if (sMontserrat == nullptr) {
        sMontserrat = LoadGameFont("fonts/Montserrat-Regular.ttf");
    }
    if (sMontserrat == nullptr || sMontserrat->Data == nullptr) {
        return;
    }
    ImFontConfig cfg;
    cfg.MergeMode = true;
    cfg.DstFont = font;
    cfg.FontDataOwnedByAtlas = false; // ImGui keeps a copy of its own
    cfg.PixelSnapH = true;
    ImGui::GetIO().Fonts->AddFontFromMemoryTTF(sMontserrat->Data, (int)sMontserrat->DataSize, size, &cfg,
                                                kCyrillicRanges);
}

void CoopFonts_SetCoopFonts(std::initializer_list<ImFont*> fonts) {
    sCoopFonts.clear();
    for (ImFont* font : fonts) {
        if (font != nullptr) {
            sCoopFonts.push_back(font);
        }
    }
    sMontserrat = nullptr; // every font is made
}

namespace coop::client {

void CoopFonts_WantChinese(bool want) {
    sWantChinese = want;
}

void CoopFonts_FrameStart() {
    if (sWantChinese && !sChineseIn && !sChineseMissing) {
        AddChinese();
    }
}

bool CoopFonts_ChineseMissing() {
    return sChineseMissing;
}

} // namespace coop::client
