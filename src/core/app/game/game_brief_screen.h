// game_brief_screen.h: the mid-play RULES overlay, a paged explainer of one game engine's
// own rules drawn over that engine while it is paused.
//
// A+C on any engine opens it (Game::openGameBrief), and the first egg of a line opens it
// as the hatch's briefing. One overlay serves every engine: the active board is one Nav
// at a time, so two can never be open together, and the rows are keyed by ArcadeGameKind
// so they read the same from a cabinet, a MAINT played variant or a hatch. Which engine
// is up and the rows it reads are the game's (Game::gameBriefRows); the screen holds
// whether it is open, the window it is scrolled to, and who opened it.
#pragma once

#include <vector>

#include "core/device/platform.h"

namespace mal {

class Framebuffer;
struct ProseRow;

class GameBriefScreen {
public:
    void open() {
        open_ = true;
        scroll_ = 0;
    }
    // The hatch opened it. That reading leads with what the egg stakes, B walks it off
    // its last page into play, and C skips straight to the board. No-op while closed.
    void markIntro() { intro_ = open_; }
    void close() {
        open_ = false;
        intro_ = false;
        scroll_ = 0;
    }
    bool isOpen() const { return open_; }
    bool intro() const { return intro_; }

    // B pages, C or the chord closes. Returns whether the page changed.
    bool onButton(const std::vector<ProseRow>& rows, const ButtonEvent& ev);
    void draw(Framebuffer& fb, const char* title, const std::vector<ProseRow>& rows,
              int beat) const;

private:
    bool open_ = false;
    int scroll_ = 0;
    bool intro_ = false;
};

}  // namespace mal
