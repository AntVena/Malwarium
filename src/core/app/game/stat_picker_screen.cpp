#include "core/app/game/stat_picker_screen.h"

#include "tunables.h"
#include "core/ui/items_screen.h"
#include "core/ui/layout.h"

namespace mal {

void StatPickerScreen::open(const StatPickerHost& host, const ItemDef& item) {
    item_ = &item;
    from_ = -1;
    int first = -1;
    for (int i = 0; i < kLevelStatCount; ++i)
        if (host.statCanGiveUp(i)) { first = i; break; }
    row_ = first < 0 ? 0 : first;
}

int StatPickerScreen::nextGiver(const StatPickerHost& host, int cur, int dir) const {
    const int i = stepRow(cur, kLevelStatCount, dir, [&](int s) { return host.statCanGiveUp(s); });
    return host.statCanGiveUp(i) ? i : -1;
}

int StatPickerScreen::nextTarget(int cur, int dir) const {
    // Every other stat is a legal destination, including one at zero earned points, so
    // the only test is that the point moves somewhere new.
    return stepRow(cur, kLevelStatCount, dir, [&](int s) { return s != from_; });
}

void StatPickerScreen::stepRollback(const StatPickerHost& host, int dir) {
    const int nxt = nextGiver(host, row_, dir);
    if (nxt >= 0) row_ = nxt;
}

void StatPickerScreen::stepRepartition(const StatPickerHost& host, int dir) {
    const int nxt = from_ < 0 ? nextGiver(host, row_, dir) : nextTarget(row_, dir);
    if (nxt >= 0) row_ = nxt;
}

void StatPickerScreen::onRollback(StatPickerHost& host, const ButtonEvent& ev) {
    if (ev.button == Button::A) {
        stepRollback(host, +1);
    } else if (ev.button == Button::B) {
        if (item_) host.rollBackStat(*item_, row_);
        host.closeDetail();   // back to the ITEMS list (the item may have run out)
    } else if (ev.button == Button::C) {
        host.openDetail();    // cancel: item untouched, back to its detail
    }
}

void StatPickerScreen::onRepartition(StatPickerHost& host, const ButtonEvent& ev) {
    const bool picking = from_ < 0;   // step one: which stat gives the point
    if (ev.button == Button::A) {
        stepRepartition(host, +1);
    } else if (ev.button == Button::B) {
        if (picking) {
            // Lock the source in and hand the cursor to the destination step, parked on
            // a stat that is not the source (so B can never be a no-op move onto itself).
            if (host.statCanGiveUp(row_)) {
                from_ = row_;
                row_ = nextTarget(row_, +1);
            }
        } else if (host.statCanGiveUp(from_) && row_ != from_ && item_) {
            host.moveStatPoint(*item_, from_, row_);
            from_ = -1;
            host.closeDetail();   // back to the ITEMS list (the item may have run out)
        }
    } else if (ev.button == Button::C) {
        // C walks back one step rather than straight out: from the destination step it
        // re-opens the source question (cursor back on the source, so the mis-pick is
        // visible), and from the source step it leaves. Nothing has been spent either way.
        if (picking) {
            host.openDetail();    // cancel: item untouched, back to its detail
        } else {
            row_ = from_;
            from_ = -1;
        }
    }
}

void StatPickerScreen::drawRollback(Framebuffer& fb, const StatPickerHost& host) const {
    const int points[4] = {host.levelStatPoint(0), host.levelStatPoint(1),
                           host.levelStatPoint(2), host.levelStatPoint(3)};
    drawRollbackPicker(fb, points, row_);
}

void StatPickerScreen::drawRepartition(Framebuffer& fb, const StatPickerHost& host) const {
    const int points[4] = {host.levelStatPoint(0), host.levelStatPoint(1),
                           host.levelStatPoint(2), host.levelStatPoint(3)};
    drawRepartitionPicker(fb, points, row_, from_);
}

}  // namespace mal
