#pragma once
// The rules of the "live" pause menu. In the server's world the pause menu stops nothing (the game side is
// mm/2s2h/Coop/Actors/LiveMenu.cpp), so the game can need Link or the screen while the menu is open: these rules say
// when the menu must then close by itself, and how often it updates. Pure logic, no engine types.

#include <cstdint>

namespace coop {

// What the menu cares about, read from the game at one point of a frame.
struct LiveMenuView {
    bool transition = false; // a scene change is under way
    bool gameOver = false;   // the game over sequence runs
    bool taken = false;      // the game took Link: a cutscene, an item, a forced walk, death...
    bool textOpen = false;   // a text box is open...
    uint16_t textId = 0;     // ...with this text
};

enum class LiveMenuClose : uint8_t {
    No,
    DropText, // close; a text box still open is the menu's own (an item's description): it goes with the menu
    KeepText, // close; the open text box is the game's: it stays
};

class LiveMenuWatch {
  public:
    // The menu is not open (forgets everything).
    void Closed();
    // The menu has just opened. What is there now came with it and is no reason to close it: the owls' map of the
    // Song of Soaring opens while Link plays the ocarina and the song's text is still closing.
    void Opened(const LiveMenuView& v);
    // After the menu's own update: a text box open now is the menu's.
    void AfterMenu(const LiveMenuView& v);
    // After the world ran: must the menu close?
    LiveMenuClose AfterWorld(const LiveMenuView& v);

    bool IsOpen() const {
        return mOpen;
    }

  private:
    bool mOpen = false;
    bool mTaken = false;   // Link was already taken the last time we looked
    bool mOwnText = false; // the menu has a text box of its own open...
    uint16_t mOwnTextId = 0; // ...with this text
};

// How many times the menu updates in a frame of the game (frames of an open menu, counted from 0): the game keeps its
// 20 frames a second, the menu its own 30 updates a second.
int LiveMenu_Steps(uint32_t frame);

} // namespace coop
