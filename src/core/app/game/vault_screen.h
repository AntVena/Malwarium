// vault_screen.h: the Hacker face's VAULT slot, as a feature that owns its own state.
//
// The VAULT cashes in what a pet has no use for: sealed caches, opened for a reward draw,
// and Decryptograms, played as a board. Its rows are the owned stacks of either kind,
// read off the inventory each time. The screen holds only its cursor; what a row is
// spent on is the game's, reached through VaultHost.
#pragma once

#include <cstdint>

#include "core/device/platform.h"

namespace mal {

class ContentRegistry;
class Framebuffer;
class Inventory;
struct ItemDef;

// What the VAULT needs from the game. Game implements it; a test can too.
class VaultHost {
public:
    virtual const Inventory& inventory() const = 0;
    virtual const ContentRegistry& content() const = 0;
    // Bulk Open (a Rig Shop row): a held B opens every cache of the focused row's rarity.
    virtual bool bulkOpenUnlocked() const = 0;

    // A cache opens for a reward draw, a Decryptogram rolls a quote and starts a board.
    virtual void cashVaultRow(const ItemDef& d) = 0;
    virtual void openAllCachesOfRarity(const ItemDef& focused) = 0;
    virtual void leaveToCarousel() = 0;
    virtual void armHoldB() = 0;   // B pressed; the release or the hold decides

protected:
    ~VaultHost() = default;
};

class VaultScreen {
public:
    // The owned cashable stacks, tickets first and then richest cache first, capped at
    // `max`. The rows input, the hold and the draw all read. Returns the count.
    static int rows(const VaultHost& host, const ItemDef** out, int max);

    void enter() { row_ = 0; }
    void onButton(VaultHost& host, const ButtonEvent& ev);
    void stepList(const VaultHost& host, int dir);
    // With Bulk Open owned, B on a cache row is a tap/hold pair: the release cashes the
    // one row, a hold past kBulkOpenHoldMs opens every cache of its rarity.
    bool holdB(VaultHost& host, uint32_t heldMs);
    void releaseB(VaultHost& host);
    void draw(Framebuffer& fb, const VaultHost& host) const;

    int row() const { return row_; }

private:
    // The focused row, re-parked on the first when the list shrank under the cursor.
    // Null on an empty VAULT.
    const ItemDef* focused(const VaultHost& host);

    static constexpr int kMaxRows = 16;
    int row_ = 0;
};

}  // namespace mal
