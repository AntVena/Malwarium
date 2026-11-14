#include "core/app/game.h"

#include <cstdio>

#include "core/ui/carousel.h"

// game_hacker.cpp: the Hacker face (07, /HACK2/SHOP1).
//
// A+C at the top level flips PET ↔ HACKER (toggleFace). The Hacker face reuses the
// pet-side carousel + submenu contracts wholesale (the same Nav::Idle/Cursor/Submenu
// states and the same A/B/C cursor cycling), so the code specific to this face is the
// hacker slot roster, which slots open, and the dispatch to each slot's screen. The
// home canvas and the PROFILE viewer are game/hacker_screen.h. The SHOP (the Rig Shop and its SERVICES board) lives in game_rigshop.cpp;
// MERGE HUB's own L2 screen in game_merge.cpp (this face only sells the hub itself:
// the recipes cooked in it are won off a Decryptogram), and CREW's (enlist + home
// network) in game_crew.cpp. PEERS (met operators) lives in game_peers.cpp and LINK
// (1v1 duels) in game_pvp.cpp; SCAN renders the inaccessible marker and B is inert.
//

namespace mal {

// --- Face toggle + navigation ---------------------------------------------

void Game::toggleFace() {
    face_ = (face_ == Face::Pet) ? Face::Hacker : Face::Pet;
    // Land on the face's idle/centre canvas. Drop any cursor + sub-state so a
    // flip is a clean top-level context switch, never mid-submenu on the other face.
    nav_ = Nav::Idle;
    // Focus the first accessible hacker slot, so the cursor never opens on a slot
    // where B does nothing. Computed from the roster so it survives a reorder. Pet
    // face has no such gating, so it always rests on slot 0.
    cursor_ = 0;
    if (face_ == Face::Hacker) {
        const HackerCarouselSlot* slots = hackerCarouselSlots();
        for (int i = 0; i < kCarouselSlots; ++i)
            if (slots[i].accessible) { cursor_ = i; break; }
    }
    rigShop_.enter();
    dirty_ = true;
}

HackerSlotId Game::enteredHackerId() const {
    return hackerCarouselSlots()[cursor_].id;
}

bool Game::hackerSlotAccessible(int slot) const {
    if (slot < 0 || slot >= kCarouselSlots) return false;
    const HackerCarouselSlot& s = hackerCarouselSlots()[slot];
    // MERGE is the one slot whose accessibility isn't fixed at compile time: it flips
    // live the moment the f Merge Hub purchase lands.
    if (s.id == HackerSlotId::Merge) return mergeHubUnlocked();
#ifdef MALWARIUM_DEMO
    // SCAN/LINK/PEERS are the radio: a Wi-Fi sniff and a peer link, neither of which
    // a browser has. They stay on the shelf, dimmed, rather than opening onto a
    // screen that could only ever report nothing found (demo_config.h).
    if (s.id == HackerSlotId::Scan || s.id == HackerSlotId::Link ||
        s.id == HackerSlotId::Peers)
        return false;
#endif
    return s.accessible;
}

void Game::enterHackerSubmenu() {
    // B on an inaccessible slot is inert (the slot exists but isn't usable yet).
    if (!hackerSlotAccessible(cursor_)) return;
    nav_ = Nav::Submenu;
    rigShop_.enter();
    vault_.enter();
    merge_.enter();
    crew_.enter();
    peers_.enter();
    link_.enter();
}

void Game::onHackerSubmenu(const ButtonEvent& ev) {
    switch (enteredHackerId()) {
        case HackerSlotId::Crew:
            crew_.onButton(*this, ev);
            break;
        case HackerSlotId::Shop:
            rigShop_.onButton(*this, ev);
            break;
        case HackerSlotId::Vault:
            vault_.onButton(*this, ev);
            break;
        case HackerSlotId::Merge:
            merge_.onButton(*this, ev);
            break;
        case HackerSlotId::Peers:
            peers_.onButton(*this, ev);
            break;
        case HackerSlotId::Link:
            link_.onButton(*this, ev);
            break;
        // PROFILE, and the fallback for SCAN, which is never entered
        // (hackerSlotAccessible).
        case HackerSlotId::Profile:
        default:
            hacker_.onProfile(*this, ev);
            break;
    }
}

void Game::cashVaultRow(const ItemDef& d) {
    if (d.use != ItemDef::Use::PlayCryptogram) { openSealedCache(d); return; }
    // Spend the ticket FIRST: the board cannot be walked out of, so there is no path
    // where it is consumed and no board follows, and taking it up front means a reboot
    // mid-quote costs the ticket rather than duplicating it.
    if (inventory_.count(d.id) <= 0) return;
    // Every quote won already? Then no first-solve prize is left and the board replays
    // for Bits, as the arcade cabinet does.
    const int quote = pickQuote(quotesLeftToWin() ? QuotePick::Unsolved
                                                  : QuotePick::SolvedOnly);
    if (quote < 0) return;   // an empty pool: keep the ticket rather than eat it
    inventory_.remove(d.id, 1);
    char buf[28];
    std::snprintf(buf, sizeof(buf), "USED %s", d.displayName);
    log_.push(LogEventType::ItemUsed, buf);
    markSaveDirty();
    startCryptogram(quote, cryptogramExtraReveals(quoteTier(quote)));
}

// --- Rendering -------------------------------------------------------------

void Game::drawHackerSubmenu(Framebuffer& fb) const {
    switch (enteredHackerId()) {
        // Each slot's screen lives in its own unit and draws its own header band, since
        // only the screen knows its title, tint and position note.
        case HackerSlotId::Shop: rigShop_.draw(fb, *this, beat_); break;   // rig_shop_screen.cpp
        case HackerSlotId::Vault: vault_.draw(fb, *this); break;
        case HackerSlotId::Crew: crew_.draw(fb, *this); break;
        case HackerSlotId::Peers: peers_.draw(fb, *this); break;
        case HackerSlotId::Link: link_.draw(fb, *this); break;
        case HackerSlotId::Merge: merge_.draw(fb, *this); break;
        // PROFILE: the read-only operator viewer (the STAT analog). SCAN is never
        // entered (hackerSlotAccessible), so it shares the fallback onHackerSubmenu uses.
        case HackerSlotId::Profile:
        default:
            hacker_.drawProfile(fb, *this);
            break;
    }
}

}  // namespace mal
