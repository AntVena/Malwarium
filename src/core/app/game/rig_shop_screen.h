// rig_shop_screen.h: the Hacker face's SHOP slot, as a feature that owns its own state.
//
// The SHOP is three views behind one slot: the storefront list, the SERVICES switchboard
// its head slot opens, and one service's info page, which a HELD B opens from the board.
// RigShopScreen holds which view is up and the cursors inside each; the Rig Shop account
// it sells into (levels, service switches, the wallet) belongs to the game, and the
// screen reaches it only through RigShopHost.
#pragma once

#include <cstdint>

#include "core/app/game_rig_shop.h"
#include "core/content/effect_text.h"
#include "core/device/platform.h"

namespace mal {

class Framebuffer;

enum class ShopView : uint8_t { Rows, Services, ServiceInfo };

// What the SHOP needs from the game. Game implements it; a test can too.
class RigShopHost {
public:
    virtual int bits() const = 0;
    virtual int rigLevel(int row) const = 0;
    virtual int rigUpgradeCostFor(int row) const = 0;
    virtual bool shopRowOffered(int row) const = 0;
    virtual bool rigServicesOffered() const = 0;
    virtual int rigServiceRows(int* out, int max) const = 0;
    virtual int rigServicesRunning() const = 0;
    virtual bool rigFeatureActive(int row) const = 0;
    virtual ServiceMode rigServiceMode(int row) const = 0;
    virtual const char* rigServiceWord(int row) const = 0;
    // Disk Maintenance's per-run price is its row's multiple of the pet's defrag price.
    virtual int defragCost() const = 0;
    virtual int diskMaintenanceLevel() const = 0;

    virtual void buyRigUpgrade(int row) = 0;
    virtual void toggleRigService(int row) = 0;
    virtual void leaveToCarousel() = 0;   // C on the storefront
    virtual void armHoldB() = 0;          // B pressed; the release or the hold decides
    virtual void markDirty() = 0;         // repaint

protected:
    ~RigShopHost() = default;
};

class RigShopScreen {
public:
    // Every entry opens on the storefront, cursor on the first purchasable row.
    void enter();
    void onButton(RigShopHost& host, const ButtonEvent& ev);
    // B released on the SERVICES board before the hold landed: flip the focused service.
    void releaseB(RigShopHost& host);
    // B held on the SERVICES board: open the focused service's info page.
    void openServiceInfo(RigShopHost& host);
    // Step the storefront cursor `dir` slots over the SERVICES head slot and every
    // offered row, wrapping. The one walk the A-cycle, the hold-repeat and the
    // post-buy re-aim share.
    void stepSlot(const RigShopHost& host, int dir);
    void stepService(const RigShopHost& host, int dir);
    void draw(Framebuffer& fb, const RigShopHost& host, int beat) const;

    ShopView view() const { return view_; }
    // A RigRow, or kRigSlotServices on the head slot. A slot, which differs from a row
    // POSITION by every row the list is filtering out.
    int slot() const { return slot_; }
    int serviceRow() const { return serviceRow_; }

private:
    void onRows(RigShopHost& host, const ButtonEvent& ev);
    void onServices(RigShopHost& host, const ButtonEvent& ev);
    void onServiceInfo(RigShopHost& host, const ButtonEvent& ev);
    int nextSlot(const RigShopHost& host, int cur, int dir) const;
    // The info page's readout: the row's own rigSpec plus the live per-run price. The
    // renderer and A's paging both read it, since paging needs the readout's height.
    static SpecRows serviceSpec(const RigShopHost& host, int row);
    static int serviceProseLines(const RigShopHost& host, int row);
    void drawRows(Framebuffer& fb, const RigShopHost& host, int beat) const;
    void drawServices(Framebuffer& fb, const RigShopHost& host, int beat) const;
    void drawServiceInfo(Framebuffer& fb, const RigShopHost& host) const;

    ShopView view_ = ShopView::Rows;
    int slot_ = 0;
    int serviceRow_ = 0;
    int serviceScroll_ = 0;
};

}  // namespace mal
