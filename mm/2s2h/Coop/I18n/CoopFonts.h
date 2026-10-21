#pragma once
// [COOP] ImGui fonts that can show the co-op's languages. The game's fonts only had Latin letters:
//  - Cyrillic: every font asks for it (Montserrat has it; Inconsolata, the menu's font, gets Montserrat's).
//  - Chinese: taken from a font of the system (Windows: Microsoft YaHei, msyh.ttc; or the file in gCoop.ChineseFont)
//    and only while Chinese is shown: each font that gets it holds its own copy of that file (~20 MB), so only the
//    three fonts the co-op's texts use get it (the menu's text, its titles and the default one of the windows).
//    Chosen at the start it goes in with the other fonts; chosen later it is added outside the ImGui frame and the
//    font texture is made again (DX11 and OpenGL; with another renderer, from the next start).
// BenPort.cpp calls the first three; CoopText.cpp the rest.
#include <imgui.h>

#include <initializer_list>
#include <string>

const ImWchar* CoopFonts_BaseRanges();                                       // instead of the default ranges
void CoopFonts_OnFontCreated(ImFont* font, float size, const std::string& path); // after each font is made
void CoopFonts_SetCoopFonts(std::initializer_list<ImFont*> fonts);           // the ones the co-op's texts use

namespace coop::client {

void CoopFonts_WantChinese(bool want); // CoopText.cpp, every frame: Chinese is shown (or not)
void CoopFonts_FrameStart();           // outside the ImGui frame: adds what was asked for
bool CoopFonts_ChineseMissing();       // asked for, and no font with Chinese was found

} // namespace coop::client
