#include "core/app/game/stat_picker_screen.h"

#include "tunables.h"
#include "core/ui/items_screen.h"

namespace mal {

void StatPickerScreen::open(const StatPickerHost& host, const ItemDef& item) {
    item_ = &item;
    from_ = -1;
    int first = -1;
    for (int i = 0; i < kLevelStatCount; ++i)
        if (host.statCanGiveUp(i)) { first = i; break; }
    row_ = first < 0 ? 0 : first;
}

int StatPickerScreen::nextGiver(const StatPickerHost& host, int cur) const {
    for (int step = 1; step <= kLevelStatCount; ++step) {
        const int i = (cur + step) % kLevelStatCount;
        if (host.statCanGiveUp(i)) return i;
    }
    return host.statCanGiveUp(cur) ? cur : -1;
}

int StatPickerScreen::nextTarget(int cur) const {
    // Every other stat is a legal destination, including one at zero earned points, so
    // the only test is that the point moves somewhere new.
    for (int step = 1; step <= kLevelStatCount; ++step) {
        const int i = (cur + step) % kLevelStatCount;
        if (i != from_) return i;
    }
    return cur;
}

void StatPickerScreen::onRollback(StatPickerHost& host, const ButtonEvent& ev) {
    if (ev.button == Button::A) {
        const int nxt = nextGiver(host, row_);
        if (nxt >= 0) row_ = nxt;
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
        const int nxt = picking ? nextGiver(host, row_) : nextTarget(row_);
        if (nxt >= 0) row_ = nxt;
    } else if (ev.button == Button::B) {
        if (picking) {
            // Lock the source in and hand the cursor to the destination step, parked on
            // a stat that is not the source (so B can never be a no-op move onto itself).
            if (host.statCanGiveUp(row_)) {
                from_ = row_;
                row_ = nextTarget(row_);
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
