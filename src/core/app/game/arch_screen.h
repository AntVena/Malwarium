// arch_screen.h: the ARCH slot's screens, as a feature that owns its own state.
//
// The group picker and the rows behind one group (L2), and the page that acts on one row
// (L3): a pet's record with its Store / Deploy / Release, the SELL sheet, the NEW EGG
// confirm, or a read-only headstone. The screen holds its two cursors, the open group,
// the focused action or offer and the inline confirm. The rack, the records and what a
// commit does to them are the game's, reached through ArchHost.
//
// This header names no core/ui type beyond ui_state.h, so game.h can hold an ArchScreen
// by value.
#pragma once

#include "core/device/platform.h"
#include "core/app/game/inline_confirm.h"
#include "core/ui/ui_state.h"   // ArchAction, ArchGroup, SaleOffer

namespace mal {

class ContentRegistry;
class Framebuffer;
struct ArchSaleSheet;
struct CreatureDef;
struct Operator;
struct Pet;
struct SaveStoredPet;

// Which of ARCH's two L2 screens is up.
enum class ArchView : uint8_t { Picker, List };

// What ARCH needs from the game. Game implements it; a test can too.
class ArchHost {
public:
    virtual const ContentRegistry& content() const = 0;
    virtual const Operator& operatorRecord() const = 0;   // the rack and the records
    virtual const Pet& petRecord() const = 0;             // the active pet, Store's subject
    virtual int rackSlots() const = 0;
    virtual int screenBeat() const = 0;
    // The SELL counter's rules: nullptr when `offer` can be taken for `sold`, else the
    // short reason its row shows; and the Script a Reimage sends an active twin back to.
    virtual const char* archSaleBlocker(SaleOffer offer, const SaveStoredPet& sold) const = 0;
    virtual const CreatureDef* reimageScript(const CreatureDef* daemon) const = 0;

    // Each commit leaves for wherever its result is seen.
    virtual void archStoreActive() = 0;
    virtual void archDeployStored(int storedIdx) = 0;
    virtual void archReleaseStored(int storedIdx) = 0;
    virtual void archSellStored(int storedIdx, SaleOffer offer) = 0;
    virtual void archHatchNewEgg() = 0;
    // Back to line-select without laying an egg: C off the picker with no active pet.
    virtual void reopenLineSelect() = 0;
    virtual void openDetail() = 0;
    virtual void closeDetail() = 0;
    virtual void leaveToCarousel() = 0;

protected:
    ~ArchHost() = default;
};

class ArchScreen {
public:
    // ARCH always opens on its group picker: a 64-slot shelf plus a record tail is not
    // one list, and the picker is where NEW EGG lives.
    void enter() {
        view_ = ArchView::Picker;
        pickRow_ = 0;
        group_ = ArchGroup{};
        row_ = 0;
        action_ = ArchAction::Store;
        confirm_.close();
    }
    void reset() { *this = ArchScreen{}; }

    // L2: the picker or the open group's rows, whichever is up.
    void onButton(ArchHost& host, const ButtonEvent& ev);
    void stepList(const ArchHost& host, int dir);
    void draw(Framebuffer& fb, const ArchHost& host) const;
    // L3: the page for the focused row, or the NEW EGG confirm.
    void onDetail(ArchHost& host, const ButtonEvent& ev);
    void drawDetail(Framebuffer& fb, const ArchHost& host) const;

    ArchView view() const { return view_; }
    ArchGroup group() const { return group_; }
    int pickRow() const { return pickRow_; }
    int pickRowCount(const ArchHost& host) const;
    int rowCount(const ArchHost& host) const;
    // The species id under the cursor, or nullptr on an empty group, a record whose id
    // the tables no longer answer to, or the NEW EGG row.
    const char* focusedPetId(const ArchHost& host) const;
    // The SELL sheet for the focused row: what the L3 draws, and what a gate reads.
    ArchSaleSheet saleSheet(const ArchHost& host) const;

private:
    void onPicker(ArchHost& host, const ButtonEvent& ev);
    void onList(ArchHost& host, const ButtonEvent& ev);
    void onNewEgg(ArchHost& host, const ButtonEvent& ev);
    void onRecord(ArchHost& host, const ButtonEvent& ev);
    void onSale(ArchHost& host, const ButtonEvent& ev);

    ArchView view_ = ArchView::Picker;
    int pickRow_ = 0;
    // The open group survives into L3: the page asks it whether it is the NEW EGG
    // confirm, the SELL sheet or a pet's record.
    ArchGroup group_;
    int row_ = 0;
    ArchAction action_ = ArchAction::Store;
    SaleOffer saleOffer_ = SaleOffer::Bits;
    InlineConfirm confirm_;
};

}  // namespace mal
