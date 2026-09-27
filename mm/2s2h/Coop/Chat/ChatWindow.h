#pragma once
// Minimal chat overlay (bottom-left). Closed: recent lines fade out. Open (Enter): history + text box.
#include <ship/window/gui/GuiWindow.h>

#include <string>

namespace coop::client {

class ChatWindow : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void InitElement() override {
    }
    void DrawElement() override {
    }
    void UpdateElement() override {
    }
    void Draw() override;

    int mHistoryPos = -1; // used by the up/down callback

  private:
    void HandleOpenKeys();
    void Open(const char* initialText);
    void Close();
    void DrawLines(bool open);
    void DrawInput();

    bool mOpen = false;
    int mFramesOpen = 0;
    char mBuffer[256] = {};
};

} // namespace coop::client
