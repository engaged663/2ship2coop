// [COOP] Each Link's tunic in their player's color (see CoopTunics.h and coop/common/TunicSplit.h).
//
// Link's tunic color is one SetPrimColor inside a few display lists of object_link_child, the same lists for every Link
// (2Ship's cosmetic editor turns that instruction into a call to its own color: still one color for all). We keep a
// copy of each of those lists with an "end of list" at every color and, while a Link's skeleton is drawn, swap each
// tunic piece for a tiny list made for that frame: copy up to the color, that Link's color, copy from just after it.
// The shared lists are never touched, so 2Ship's cosmetics and the .o2r mods keep working as they are.
//
// The places of the colors are the ones 2Ship patches for its "Human Tunic" (BenGui/CosmeticEditor.cpp). A list that
// is not as expected (a custom model, a place that is neither a color nor a call) is drawn as always.
#include "CoopTunics.h"

#include "PlayerColors.h"

#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/I18n/CoopText.h"
#include "2s2h/Coop/Puppet/PuppetActor.h"

#include "2s2h/BenGui/BenGui.hpp"
#include "2s2h/BenGui/UIWidgets.hpp"

#include "common/TunicSplit.h"

#include <fast/resource/type/DisplayList.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <libultraship/libultraship.h>
#include <ship/resource/ResourceManager.h>

#include <array>
#include <cstring>
#include <memory>
#include <vector>

extern "C" {
#include "macros.h"
int ResourceMgr_OTRSigCheck(char* imgData);       // BenPort.cpp
uint8_t Player_IsCustomLinkModel(Player* player); // z_player_lib.c
}

static_assert(sizeof(Gfx) == sizeof(coop::TunicWord), "a Gfx is two 64-bit words on this build");

namespace coop::client {

namespace {

constexpr const char* kPlayersCvar = "gCoop.Tunic.Players";
constexpr const char* kMineCvar = "gCoop.Tunic.Mine";
constexpr size_t kOtrPrefix = 7; // "__OTR__"

struct TunicList {
    const char* path;            // as the resource manager knows it (the limbs carry it after "__OTR__")
    std::vector<uint32_t> sites; // the tunic colors in it (instruction indices)

    std::shared_ptr<Fast::DisplayList> res;   // held: the copy points at things this resource owns
    std::vector<Gfx> source;                  // the resource's instructions when the copy was made
    std::vector<TunicWord> copy;              // them, with an end of list at every site
    std::vector<TunicWord> retired;           // the copy before, in case a frame still reads it
    std::vector<TunicWord> originals;         // the color each site sets (its LOD fraction and alpha are kept)
    bool usable = false;
    bool checked = false;
    uint32_t checkedFrame = 0;
};

// 2Ship's "Human Tunic" places (BenGui/CosmeticEditor.cpp, humanTunicPatch).
std::array<TunicList, 9> sLists = { {
    { "objects/object_link_child/gLinkHumanWaistDL", { 5 } },
    { "objects/object_link_child/gLinkHumanRightThighDL", { 10 } },
    { "objects/object_link_child/gLinkHumanLeftThighDL", { 10 } },
    { "objects/object_link_child/gLinkHumanHeadDL", { 92 } },
    { "objects/object_link_child/gLinkHumanHatDL", { 10 } },
    { "objects/object_link_child/gLinkHumanCollarDL", { 5 } },
    { "objects/object_link_child/gLinkHumanLeftShoulderDL", { 10, 65 } },
    { "objects/object_link_child/gLinkHumanRightShoulderDL", { 10, 65 } },
    { "objects/object_link_child/gLinkHumanTorsoDL", { 5 } },
} };

// The Link being drawn right now (Player_DrawImpl between Coop_TunicWrapOverride and Coop_TunicDrawDone).
OverrideLimbDrawFlex sInner = nullptr;
Rgb sColor;
bool sActive = false;

TunicWord ToWord(const Gfx& g) {
    return TunicWord{ (uint64_t)g.words.w0, (uint64_t)g.words.w1 };
}

void Put(Gfx* g, const TunicWord& w) {
    g->words.w0 = (uintptr_t)w.w0;
    g->words.w1 = (uintptr_t)w.w1;
}

// The color a site sets: the SetPrimColor itself or, when 2Ship's cosmetic made it a call to its little color list,
// that list's first instruction (only a real pointer: a segmented address has its low bit set).
TunicWord OriginalColor(const Gfx& site) {
    TunicWord w = ToWord(site);
    if (TunicIsCall(w) && w.w1 != 0 && (w.w1 & 1) == 0) {
        TunicWord first = ToWord(*(const Gfx*)(uintptr_t)w.w1);
        if (TunicIsPrimColor(first)) {
            return first;
        }
    }
    return w;
}

// Once a frame per list: is the copy still the resource's (2Ship's cosmetics patch it in place, a .o2r may replace
// it)? Rebuilt when not; the old copy lives one frame more.
void Refresh(TunicList& l, uint32_t frame) {
    if (l.checked && l.checkedFrame == frame) {
        return;
    }
    l.checked = true;
    l.checkedFrame = frame;

    auto res = std::dynamic_pointer_cast<Fast::DisplayList>(
        Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(l.path));
    if (res == nullptr || res->GetInitData() == nullptr || res->GetInitData()->IsCustom || res->Instructions.empty()) {
        l.usable = false;
        l.res = nullptr;
        return;
    }
    const std::vector<Gfx>& now = res->Instructions;
    bool same = res == l.res && now.size() == l.source.size() &&
                std::memcmp(now.data(), l.source.data(), now.size() * sizeof(Gfx)) == 0;
    if (same) {
        return;
    }

    const Gfx end = gsSPEndDisplayList();
    std::optional<std::vector<TunicWord>> copy =
        BuildTunicCopy((const TunicWord*)now.data(), now.size(), l.sites, ToWord(end));
    l.retired = std::move(l.copy);
    l.copy.clear();
    l.originals.clear();
    l.res = res;
    l.source = now;
    l.usable = copy.has_value();
    if (!l.usable) {
        SPDLOG_WARN("[Coop] Tunic colors: {} is not as expected ({} instructions), drawn as always", l.path,
                    now.size());
        return;
    }
    l.copy = std::move(*copy);
    for (uint32_t site : l.sites) {
        l.originals.push_back(OriginalColor(now[site]));
    }
}

// The tunic list this limb is about to draw, ready to use; nullptr when it is not one (or not usable).
TunicList* Find(Gfx* dList, PlayState* play) {
    const char* path = nullptr;
    if (ResourceMgr_OTRSigCheck((char*)dList) == 1) {
        path = (const char*)dList + kOtrPrefix;
    }
    for (TunicList& l : sLists) {
        bool match = path != nullptr ? std::strcmp(path, l.path) == 0
                                     : (l.res != nullptr && dList == l.res->Instructions.data());
        if (match) {
            Refresh(l, play->state.frames);
            return l.usable ? &l : nullptr;
        }
    }
    return nullptr;
}

// The piece for this Link, in this frame's graphics memory:
//   call(copy) · color · call(copy after site 1) · color · call(copy after site 2) · end
Gfx* MakePiece(PlayState* play, const TunicList& l, const Rgb& color) {
    size_t n = l.sites.size();
    Gfx* head = (Gfx*)GRAPH_ALLOC(play->state.gfxCtx, sizeof(Gfx) * (2 * n + 2));
    Gfx* g = head;
    *g++ = gsSPDisplayList((Gfx*)l.copy.data());
    for (size_t i = 0; i < n; i++) {
        Put(g++, TunicPrimColor(l.originals[i], color));
        *g++ = gsSPDisplayList((Gfx*)&l.copy[l.sites[i] + 1]);
    }
    *g++ = gsSPEndDisplayList();
    return head;
}

s32 TunicOverride(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* thisx) {
    if (sInner != nullptr && sInner(play, limbIndex, dList, pos, rot, thisx)) {
        return true; // the engine drew (or skipped) this limb itself
    }
    if (sActive && dList != nullptr && *dList != nullptr) {
        if (TunicList* l = Find(*dList, play)) {
            *dList = MakePiece(play, *l, sColor);
        }
    }
    return false;
}

// Whose color this Link wears: our id for our Link, the puppet's player; 0 = leave it as it is.
uint8_t WearerOf(Actor* actor) {
    if (actor->id == ACTOR_EN_COOP_PUPPET) {
        return CoopTunics_PlayersOn() ? ((EnCoopPuppet*)actor)->playerId : 0;
    }
    if (actor->id == ACTOR_PLAYER) {
        return CoopTunics_MineOn() ? Session_LocalId() : 0;
    }
    return 0;
}

} // namespace

bool CoopTunics_PlayersOn() {
    return CVarGetInteger(kPlayersCvar, 1) != 0;
}

bool CoopTunics_MineOn() {
    return CVarGetInteger(kMineCvar, 1) != 0;
}

void CoopTunics_DrawMenu() {
    UIWidgets::CVarCheckbox(
        Tc(Txt::TunicPlayers), kPlayersCvar,
        UIWidgets::CheckboxOptions().Color(THEME_COLOR).DefaultValue(true).Tooltip(Tip(Txt::TunicPlayersTip)));
    UIWidgets::CVarCheckbox(
        Tc(Txt::TunicMine), kMineCvar,
        UIWidgets::CheckboxOptions().Color(THEME_COLOR).DefaultValue(true).Tooltip(Tip(Txt::TunicMineTip)));
}

} // namespace coop::client

using namespace coop::client;

extern "C" OverrideLimbDrawFlex Coop_TunicWrapOverride(PlayState* play, Actor* actor, s32 form,
                                                       OverrideLimbDrawFlex inner) {
    sActive = false;
    sInner = nullptr;
    if (play == nullptr || actor == nullptr || form != PLAYER_FORM_HUMAN || !Session_IsConnected()) {
        return inner;
    }
    uint8_t wearer = WearerOf(actor);
    if (wearer == 0 || Player_IsCustomLinkModel((Player*)actor)) {
        return inner;
    }
    sColor = PlayerColors_Shown(wearer);
    sInner = inner;
    sActive = true;
    return TunicOverride;
}

extern "C" void Coop_TunicDrawDone(void) {
    sActive = false;
    sInner = nullptr;
}
