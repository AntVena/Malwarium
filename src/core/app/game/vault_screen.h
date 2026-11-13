// vault_screen.h: the Hacker face's VAULT slot, as a feature that owns its own state.
//
// The VAULT cashes in what a pet has no use for: sealed caches, opened for a reward draw,
// and Decryptograms, played as a board. Its rows are the owned stacks of either kind,
// read off the inventory each time. The screen holds its cursor and the reveal of what
// the last opening paid (one cache, or a bulk open); what a row is spent on, and the
// payout itself, are the game's, reached through VaultHost.
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "core/device/platform.h"

namespace mal {

class ContentRegistry;
class Framebuffer;
class Inventory;
struct ItemDef;

// What the cache-yield reveal (Nav::CacheYield) shows: one opened cache's result. The
// opening has already paid it by the time the reveal is up, so this is the screen's
// alone, held while it is open. The ItemDefs live in the static registry.
struct CacheReveal {
    const ItemDef* cache = nullptr;
    int bits = 0;
    const ItemDef* items[2] = {nullptr, nullptr};
    int itemCount = 0;
};

// What the bulk-open reveal (Nav::BulkYield) shows: the totals across every cache opened
// in one action, one tally row per distinct item, and the reveal's scroll cursor.
struct BulkReveal {
    struct Tally {
        const ItemDef* def = nullptr;
        int count = 0;
    };
    const ItemDef* cache = nullptr;   // the cache that was opened in bulk
    int cachesOpened = 0;
    int bits = 0;
    std::vector<Tally> tally;
    int row = 0;
};

// What the VAULT needs from the game. Game implements it; a test can too.
class VaultHost {
public:
    virtual const Inventory& inventory() const = 0;
    virtual const ContentRegistry& content() const = 0;
    virtual int screenBeat() const = 0;
    // Bulk Open (a Rig Shop row): a held B opens every cache of the focused row's rarity.
    virtual bool bulkOpenUnlocked() const = 0;

    // A cache opens for a reward draw, a Decryptogram rolls a quote and starts a board.
    virtual void cashVaultRow(const ItemDef& d) = 0;
    virtual void openAllCachesOfRarity(const ItemDef& focused) = 0;
    virtual void leaveToCarousel() = 0;
    virtual void closeDetail() = 0;   // off a reveal, back to the list
    virtual void armHoldB() = 0;      // B pressed; the release or the hold decides

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

    // The reveals (Nav::CacheYield, Nav::BulkYield). The opening has already paid by the
    // time one is up, so any confirm or back dismisses it to the list.
    void showReveal(const CacheReveal& r) { reveal_ = r; }
    void showBulkReveal(BulkReveal r) { bulk_ = std::move(r); }
    void onReveal(VaultHost& host, const ButtonEvent& ev);
    void onBulkReveal(VaultHost& host, const ButtonEvent& ev);
    void drawReveal(Framebuffer& fb, const VaultHost& host) const;
    void drawBulkReveal(Framebuffer& fb, const VaultHost& host) const;

    int row() const { return row_; }
    const CacheReveal* reveal() const { return reveal_ ? &*reveal_ : nullptr; }
    const BulkReveal* bulkReveal() const { return bulk_ ? &*bulk_ : nullptr; }

private:
    // The focused row, re-parked on the first when the list shrank under the cursor.
    // Null on an empty VAULT.
    const ItemDef* focused(const VaultHost& host);

    static constexpr int kMaxRows = 16;
    int row_ = 0;
    std::optional<CacheReveal> reveal_;
    std::optional<BulkReveal> bulk_;
};

}  // namespace mal
