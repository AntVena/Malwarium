// items_menu.h: the pet face's ITEMS slot, as a feature that owns its own state.
//
// ITEMS is two screens behind one slot: the category tile picker (when the Rig Shop's
// Type-Picker is owned) and the grouped list behind it. ItemsMenu holds which screen is
// up, both cursors, the active filter and the list's rows. The rows are rebuilt only
// when the bag, the filter or the order changes, so stepping and redrawing reuse them.
// The item detail page and the Use path stay on the game, which ItemsMenu reaches
// through ItemsHost.
//
// This header names no core/ui type, so game.h can hold an ItemsMenu by value: the
// rows' element type is only declared here, and every member that needs it complete is
// defined in items_menu.cpp.
#pragma once

#include <cstdint>
#include <vector>

#include "core/device/platform.h"
#include "core/model/pet_upgrades.h"
#include "core/ui/ui_state.h"

namespace mal {

class ContentRegistry;
class Framebuffer;
class Inventory;
struct InvRow;
struct ItemDef;

enum class ItemsView : uint8_t { Picker, List };

// What ITEMS needs from the game. Game implements it; a test can too.
class ItemsHost {
public:
    virtual const ContentRegistry& content() const = 0;
    virtual const Inventory& inventory() const = 0;
    // ITEMS was opened from the Lockout modal: the list floats the resolving items to
    // the top and C goes back to the modal.
    virtual bool itemsFromLockout() const = 0;
    virtual bool itemTabsUnlocked() const = 0;
    virtual bool itemPickerUnlocked() const = 0;
    virtual PetLifetimeGates petLifetimeGates() const = 0;

    virtual void openItemDetail(const ItemDef& d) = 0;
    virtual void returnToLockout() = 0;   // C on a list opened from the Lockout modal
    virtual void leaveToCarousel() = 0;
    virtual void armHoldB() = 0;          // B pressed; the release or the hold decides

protected:
    ~ItemsHost() = default;
};

class ItemsMenu {
public:
    ItemsMenu();
    ~ItemsMenu();
    ItemsMenu(const ItemsMenu&);
    ItemsMenu& operator=(const ItemsMenu&);

    // Every entry resets the filter and cursors. A Lockout entry lands on the list, since
    // a crisis gets no extra screen; otherwise an owned picker comes first.
    void enter(const ItemsHost& host);
    void onButton(ItemsHost& host, const ButtonEvent& ev);
    // Step the focused screen's cursor by `dir`, wrapping. The A tap, the A hold-repeat
    // and the C backward walk (game_listnav.cpp) all land here.
    void step(const ItemsHost& host, int dir);
    // B released on the list before the filter hold landed: open the focused item.
    void releaseB(ItemsHost& host);
    // B held `heldMs` so far. On the list, once the type tabs are owned, past
    // kItemFilterHoldMs it moves to the next filter and re-parks the cursor on the new
    // list's first row. True once the hold has landed, so the release that follows is
    // not also a tap.
    bool holdB(const ItemsHost& host, uint32_t heldMs);
    void draw(Framebuffer& fb, const ItemsHost& host, int beat) const;

    ItemsView view() const { return view_; }
    ItemFilter filter() const { return filter_; }
    int row() const { return row_; }
    int pickRow() const { return pickRow_; }
    void setFilter(ItemFilter f) { filter_ = f; }

private:
    void onPicker(ItemsHost& host, const ButtonEvent& ev);
    void onList(ItemsHost& host, const ButtonEvent& ev);
    void openFocused(ItemsHost& host);
    void cycleFilter(const ItemsHost& host);
    void parkOnFirstRow(const ItemsHost& host);
    const std::vector<InvRow>& rows(const ItemsHost& host) const;

    ItemsView view_ = ItemsView::List;
    ItemFilter filter_ = ItemFilter::All;
    int row_ = 0;       // index into rows(); -1 on an empty list
    int pickRow_ = 0;   // index into kItemPickerTiles

    // What rows_ was built from. A draw is const and still has to see a bag that changed
    // since the last press, so the cache refreshes from const members.
    struct RowsKey {
        uint32_t stamp = 0;   // Inventory::stamp; 0 is never issued, so it reads stale
        ItemFilter filter = ItemFilter::All;
        ItemOrder order = ItemOrder::ByGroup;
    };
    mutable std::vector<InvRow> rows_;
    mutable RowsKey rowsKey_;
};

}  // namespace mal
