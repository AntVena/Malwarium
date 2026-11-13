#include "core/app/game/items_menu.h"

#include "tunables.h"
#include "core/model/inventory.h"
#include "core/ui/items_screen.h"
#include "core/ui/layout.h"

namespace mal {

namespace {

ItemOrder orderFor(const ItemsHost& host) {
    return host.itemsFromLockout() ? ItemOrder::LockoutFirst : ItemOrder::ByGroup;
}

}  // namespace

ItemsMenu::ItemsMenu() = default;
ItemsMenu::~ItemsMenu() = default;
ItemsMenu::ItemsMenu(const ItemsMenu&) = default;
ItemsMenu& ItemsMenu::operator=(const ItemsMenu&) = default;

const std::vector<InvRow>& ItemsMenu::rows(const ItemsHost& host) const {
    const RowsKey want{host.inventory().stamp(), filter_, orderFor(host)};
    if (want.stamp != rowsKey_.stamp || want.filter != rowsKey_.filter ||
        want.order != rowsKey_.order) {
        rows_ = buildInventoryRows(host.content(), host.inventory(), want.order, filter_);
        rowsKey_ = want;
    }
    return rows_;
}

void ItemsMenu::parkOnFirstRow(const ItemsHost& host) {
    row_ = firstSelectableRow(rows(host));
}

void ItemsMenu::enter(const ItemsHost& host) {
    filter_ = ItemFilter::All;
    const bool picker = !host.itemsFromLockout() && host.itemPickerUnlocked();
    view_ = picker ? ItemsView::Picker : ItemsView::List;
    pickRow_ = 0;
    parkOnFirstRow(host);
}

void ItemsMenu::onButton(ItemsHost& host, const ButtonEvent& ev) {
    if (view_ == ItemsView::Picker) onPicker(host, ev);
    else onList(host, ev);
}

void ItemsMenu::onPicker(ItemsHost& host, const ButtonEvent& ev) {
    if (pickRow_ < 0 || pickRow_ >= kItemPickerTileCount) pickRow_ = 0;
    if (ev.button == Button::A) {
        stepList(host, +1);
    } else if (ev.button == Button::B) {
        // An EMPTY category still opens: the list says "- NO MATCHING ITEMS -" and C
        // walks back, which reads better than a dead button on a row the screen drew.
        filter_ = kItemPickerTiles[pickRow_];
        view_ = ItemsView::List;
        parkOnFirstRow(host);
        if (row_ < 0) row_ = 0;
    } else if (ev.button == Button::C) {
        host.leaveToCarousel();
    }
}

void ItemsMenu::onList(ItemsHost& host, const ButtonEvent& ev) {
    if (ev.button == Button::A) {
        stepList(host, +1);
    } else if (ev.button == Button::B) {
        // With the type-tabs upgrade, B is a tap/hold pair settled on the release
        // (releaseB) or on the hold (holdB). Without it B opens on the press.
        if (host.itemTabsUnlocked()) host.armHoldB();
        else openFocused(host);
    } else if (ev.button == Button::C) {
        if (host.itemsFromLockout()) host.returnToLockout();
        else if (host.itemPickerUnlocked()) view_ = ItemsView::Picker;
        else host.leaveToCarousel();
    }
}

void ItemsMenu::stepList(const ItemsHost& host, int dir) {
    if (view_ == ItemsView::Picker) {
        pickRow_ = stepRow(pickRow_, kItemPickerTileCount, dir);
        return;
    }
    // Section headers are drawn but never landed on, in either direction.
    const auto& r = rows(host);
    if (!r.empty()) row_ = stepSelectableRow(r, row_, dir);
}

// A header row has no item behind it, so it is inert. The cursor never rests on one,
// but a bag that changed under a held button can move the rows beneath it.
void ItemsMenu::openFocused(ItemsHost& host) {
    const auto& r = rows(host);
    if (row_ < 0 || row_ >= static_cast<int>(r.size()) || r[row_].header) return;
    host.openItemDetail(*r[row_].def);
}

void ItemsMenu::releaseB(ItemsHost& host) {
    if (view_ != ItemsView::List || !host.itemTabsUnlocked()) return;
    openFocused(host);
}

bool ItemsMenu::holdB(const ItemsHost& host, uint32_t heldMs) {
    if (view_ != ItemsView::List || !host.itemTabsUnlocked() || heldMs < kItemFilterHoldMs)
        return false;
    cycleFilter(host);
    return true;
}

void ItemsMenu::cycleFilter(const ItemsHost& host) {
    filter_ = nextItemFilter(filter_, host.itemPickerUnlocked());
    parkOnFirstRow(host);
    if (row_ < 0) row_ = 0;
}

void ItemsMenu::draw(Framebuffer& fb, const ItemsHost& host, int beat) const {
    if (view_ == ItemsView::Picker) {
        drawItemTypePicker(fb, pickRow_);
        return;
    }
    const PetLifetimeGates gates = host.petLifetimeGates();
    ItemsListView v;
    v.cursor = row_;
    v.lockout = host.itemsFromLockout();
    v.filter = filter_;
    v.tabsOwned = host.itemTabsUnlocked();
    v.pickerOwned = host.itemPickerUnlocked();
    v.gates = &gates;
    v.beat = beat;
    drawItemsList(fb, rows(host), v);
}

}  // namespace mal
