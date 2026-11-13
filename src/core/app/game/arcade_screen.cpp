#include "core/app/game/arcade_screen.h"

#include "tunables.h"
#include "core/ui/arcade_screen.h"
#include "core/ui/layout.h"

namespace mal {

void ArcadeScreen::stepList(const ArcadeHost& host, int dir) {
    row_ = stepRow(row_, arcadeGameCount(), dir,
                   [&host](int r) { return host.arcadeCabinetOffered(r); });
}

void ArcadeScreen::onList(ArcadeHost& host, const ButtonEvent& ev) {
    if (ev.button == Button::A) {
        stepList(host, +1);
    } else if (ev.button == Button::B) {
        if (host.arcadeCabinetOffered(row_)) host.openDetail();
    } else if (ev.button == Button::C) {
        host.leaveToCarousel();
    }
}

void ArcadeScreen::onCabinet(ArcadeHost& host, const ButtonEvent& ev) {
    if (ev.button == Button::A) {
        difficulty_ = static_cast<ArcadeDifficulty>(
            stepRow(static_cast<int>(difficulty_), kArcadeDifficulties, +1));
    } else if (ev.button == Button::B) {
        host.startArcadeRun(row_, difficulty_);
    } else if (ev.button == Button::C) {
        host.closeDetail();
    }
}

void ArcadeScreen::drawList(Framebuffer& fb, const ArcadeHost& host) const {
    int plays[kArcadeMaxCabinets] = {0};
    for (int i = 0; i < arcadeGameCount() && i < kArcadeMaxCabinets; ++i)
        plays[i] = host.arcadePlays(i);
    int rows[kArcadeMaxCabinets] = {0};
    ArcadeListView v;
    v.rows = rows;
    v.n = host.arcadeOfferedRows(rows, kArcadeMaxCabinets);
    v.plays = plays;
    v.cursor = row_;
    v.beat = host.screenBeat();
    drawArcadeList(fb, host.content(), v);
}

void ArcadeScreen::drawCabinet(Framebuffer& fb, const ArcadeHost& host) const {
    if (row_ < 0 || row_ >= arcadeGameCount()) return;
    ArcadeTally tally;
    tally.plays = host.arcadePlays(row_);
    tally.wins = host.arcadeWins(row_);
    tally.best = host.arcadeBest(row_);
    drawArcadeCabinet(fb, host.content(), arcadeGames()[row_], difficulty_, tally);
}

void ArcadeScreen::drawResult(Framebuffer& fb, const ArcadeHost& host) const {
    const ArcadeResult& r = result_;
    if (r.cabinet < 0 || r.cabinet >= arcadeGameCount()) return;
    ArcadeResultView v;
    v.won = r.won;
    v.score = r.score;
    v.scoreMax = r.scoreMax;
    v.best = host.arcadeBest(r.cabinet);
    v.newBest = r.newBest;
    v.bits = r.bits;
    v.happy = kArcadePlayHappy;
    drawArcadeResult(fb, arcadeGames()[r.cabinet], v);
}

}  // namespace mal
