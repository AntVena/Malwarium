// loadout_screen.h: the LOADOUT slot (MODS on the carousel), as a feature that owns its
// own state.
//
// A three-row hub over three pages: the mod slots with their picker and a mod's detail,
// the move slots with their picker and a move's detail, and PRACTISE's Sim-Battle tier.
// The two pickers share a shape (slot, pick, an overwrite confirm and the id it would
// write), and live together because they sit behind the same hub. The screen holds the
// cursors, the open slot, the confirms and what a detail page reads. The equip rules and
// the loadout itself are the pet's, reached through LoadoutHost.
//
// This header names no core/ui type beyond ui_state.h, so game.h can hold a
// LoadoutScreen by value.
#pragma once

#include <cstdint>

#include "core/app/game/inline_confirm.h"
#include "core/content/defs.h"        // MoveDef::Kind
#include "core/device/platform.h"
#include "core/model/loadout.h"       // ModHoldings
#include "core/ui/ui_state.h"         // LoadoutTab

namespace mal {

class ContentRegistry;
class Framebuffer;
struct Pet;

// What LOADOUT needs from the game. Game implements it; a test can too.
class LoadoutHost {
public:
    virtual const ContentRegistry& content() const = 0;
    virtual const Pet& petRecord() const = 0;
    virtual ModHoldings modHoldings() const = 0;
    virtual int modStorageCap() const = 0;
    virtual MoveDef::Kind slotRequiredKind(int slot) const = 0;
    virtual int screenBeat() const = 0;

    // Whether `id` can go in mod `slot` now: the equip level, the hard line gate, and
    // one slot per id per pet. installMod checks the same and does nothing on a refusal.
    virtual bool modEquippable(int slot, const char* id) const = 0;
    virtual void installMod(int slot, const char* id) = 0;
    virtual void equipMove(int slot, const char* id) = 0;
    virtual void unequipMove(int slot) = 0;
    virtual void startSimBattle(int tier) = 0;
    virtual void armHoldB() = 0;   // B pressed; the release or the hold decides
    virtual void openDetail() = 0;
    virtual void closeDetail() = 0;
    virtual void leaveToCarousel() = 0;

protected:
    ~LoadoutHost() = default;
};

class LoadoutScreen {
    enum class MovePage : uint8_t { Picker, Detail };

public:
    // Every entry opens on the hub's first row.
    void enter() {
        tab_ = LoadoutTab::Hub;
        hubRow_ = 0;
        modRow_ = 0;
        modDetailId_ = nullptr;
        mods_.close();
        moveRow_ = 0;
        moves_.close();
        movePage_ = MovePage::Picker;
        moveDetailEquipped_ = false;
    }
    void reset() { *this = LoadoutScreen{}; }
    // Back to the hub from a page, or from the Sim Battle PRACTISE started. The hub
    // keeps the row that opened the page.
    void showHub() {
        tab_ = LoadoutTab::Hub;
        movePage_ = MovePage::Picker;
    }

    // L2: the hub, or the mod or move slot list.
    void onButton(LoadoutHost& host, const ButtonEvent& ev);
    void stepList(const LoadoutHost& host, int dir);
    void draw(Framebuffer& fb, const LoadoutHost& host) const;
    // L3: a picker or a detail page, or the PRACTISE tier.
    void onDetail(LoadoutHost& host, const ButtonEvent& ev);
    // The pickers are lists while no confirm or detail is over them.
    bool detailIsList() const;
    void stepDetail(const LoadoutHost& host, int dir);
    void drawDetail(Framebuffer& fb, const LoadoutHost& host) const;
    // B is a tap/hold pair on the move slots (a tap opens the picker, a hold reads the
    // slot's move) and in the move picker (a tap unequips or reads, a hold shows every
    // move). Game routes the L2 pair or the L3 one by where the screen is.
    bool movePickerUp() const { return tab_ == LoadoutTab::Moves && movePage_ == MovePage::Picker; }
    bool moveSlotsHoldB(LoadoutHost& host, uint32_t heldMs);
    void moveSlotsReleaseB(LoadoutHost& host);
    bool movePickerHoldB(uint32_t heldMs);
    void movePickerReleaseB(LoadoutHost& host);

    LoadoutTab tab() const { return tab_; }
    int modPick() const { return modPick_; }
    const char* modDetailId() const { return modDetailId_; }
    bool moveShowAll() const { return moveShowAll_; }
    int simTier() const { return simTier_; }
    // Test seam (Game's debug hook): flip the move picker's full-roster view.
    void setMoveShowAll(bool v) { moveShowAll_ = v; }

private:
    // One picker's overwrite confirm and the id it would write.
    struct Overwrite {
        InlineConfirm confirm;
        const char* pendingId = nullptr;
        void close() { confirm.close(); pendingId = nullptr; }
        void open(const char* id) { confirm.open(); pendingId = id; }
    };
    void openTab(LoadoutHost& host, LoadoutTab tab);
    void leaveTab(LoadoutHost& host);
    void onHub(LoadoutHost& host, const ButtonEvent& ev);
    void onModSlots(LoadoutHost& host, const ButtonEvent& ev);
    void onModPicker(LoadoutHost& host, const ButtonEvent& ev);
    void onModDetail(LoadoutHost& host, const ButtonEvent& ev);
    void onMoveSlots(LoadoutHost& host, const ButtonEvent& ev);
    void onMovePicker(LoadoutHost& host, const ButtonEvent& ev);
    void onMoveDetail(LoadoutHost& host, const ButtonEvent& ev);
    void onSimTier(LoadoutHost& host, const ButtonEvent& ev);
    int modPickerRows(const LoadoutHost& host) const;
    int movePickerRows(const LoadoutHost& host) const;   // the roster plus the unequip row
    // The move a detail page reads: the slot's own when opened off the slot list, else
    // the picker's focused row. Null on the unequip row.
    const MoveDef* focusedMove(const LoadoutHost& host) const;
    void drawModPage(Framebuffer& fb, const LoadoutHost& host) const;
    void drawMovePage(Framebuffer& fb, const LoadoutHost& host) const;

    LoadoutTab tab_ = LoadoutTab::Hub;
    int hubRow_ = 0;
    // MODS: the slot list's cursor, the slot whose picker is open, the picker's row,
    // and the mod a detail page reads (open while non-null).
    int modRow_ = 0;
    int modSlot_ = 0;
    int modPick_ = 0;
    const char* modDetailId_ = nullptr;
    Overwrite mods_;
    // MOVES: the slot list's cursor (always an unlocked slot), the slot whose picker is
    // open, the picker's row (0 unequips), and the detail page's first prose line.
    int moveRow_ = 0;
    int moveSlot_ = 0;
    int movePick_ = 0;
    MovePage movePage_ = MovePage::Picker;
    int moveProseScroll_ = 0;
    // The picker lists only what fits the slot now until a held B shows every move.
    bool moveShowAll_ = false;
    // The detail was opened by holding B on the slot list, so it reads the slot's move
    // and C returns to the slot list.
    bool moveDetailEquipped_ = false;
    Overwrite moves_;
    int simTier_ = 0;
};

}  // namespace mal
