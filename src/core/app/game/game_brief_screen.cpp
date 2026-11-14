#include "core/app/game/game_brief_screen.h"

#include "core/ui/prose_page.h"
#include "core/ui/widgets.h"

namespace mal {

namespace {
constexpr int kGameBriefReaderTop = 46;
}  // namespace

bool GameBriefScreen::onButton(const std::vector<ProseRow>& rows, const ButtonEvent& ev) {
    const int total = static_cast<int>(rows.size());
    if (ev.button == Button::B && total > 0) {
        scroll_ += proseRowsFitting(rows, scroll_, kGameBriefReaderTop);
        // The chord's reader wraps, since it is something to re-read; the hatch's
        // briefing is something to get through, so B off its last page starts play.
        if (scroll_ >= total) {
            if (intro_) close();
            else scroll_ = 0;
        }
        return true;
    }
    if (ev.button == Button::C || ev.chordAC) {
        close();
        return true;
    }
    return false;
}

void GameBriefScreen::draw(Framebuffer& fb, const char* title,
                           const std::vector<ProseRow>& rows, int beat) const {
    drawHeaderBand(fb, title, "RULES");
    const char* hint = "B MORE   C BACK";
    if (intro_) {
        const bool last = scroll_ + proseRowsFitting(rows, scroll_, kGameBriefReaderTop) >=
                          static_cast<int>(rows.size());
        hint = last ? "B PLAY" : "B NEXT   C SKIP";
    }
    drawProseRows(fb, {rows, scroll_, kGameBriefReaderTop, beat, hint});
}

}  // namespace mal
