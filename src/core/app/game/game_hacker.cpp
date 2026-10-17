#include "core/app/game.h"

#include <algorithm>
#include <cstdio>

#include "tunables.h"
#include "core/app/game_internal.h"
#include "core/model/hacker_rank.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/carousel.h"
#include "core/ui/items_screen.h"
#include "core/ui/layout.h"
#include "core/ui/spec_sheet.h"
#include "core/ui/theme.h"
#include "core/ui/widgets.h"
#include "generated/assets.h"

// game_hacker.cpp — the Hacker face (07, /HACK2/SHOP1).
//
// A+C at the top level flips PET ↔ HACKER (toggleFace). The Hacker face reuses the
// pet-side carousel + submenu contracts wholesale — the same Nav::Idle/Cursor/Submenu
// states and the same A/B/C cursor cycling — so the only bespoke code here is the
// hacker slot roster, the PROFILE viewer and VAULT, and the terminal-reskin centre
// canvas. The SHOP (the Rig Shop and its SERVICES board) lives in game_rigshop.cpp;
// MERGE HUB's own L2 screen in game_merge.cpp (this face only sells the hub itself —
// the recipes cooked in it are won off a Decryptogram), and CREW's (enlist + home
// network) in game_crew.cpp.
// PEERS (met operators) lives in game_peers.cpp and LINK (1v1 duels) in game_pvp.cpp;
// SCAN renders the inaccessible marker and B is inert.
//

namespace mal {

// --- Face toggle + navigation ---------------------------------------------

void Game::toggleFace() {
    face_ = (face_ == Face::Pet) ? Face::Hacker : Face::Pet;
    // Land on the face's idle/centre canvas. Drop any cursor + sub-state so a
    // flip is a clean top-level context switch, never mid-submenu on the other face.
    nav_ = Nav::Idle;
    // Focus the first ACCESSIBLE hacker slot rather than blindly slot 0 — landing on an
    // inaccessible slot (B inert) would be a bad first impression. Computed from the
    // roster so it survives a reorder. Pet face has no such gating, so it always rests
    // on slot 0.
    cursor_ = 0;
    if (face_ == Face::Hacker) {
        const HackerCarouselSlot* slots = hackerCarouselSlots();
        for (int i = 0; i < kCarouselSlots; ++i)
            if (slots[i].accessible) { cursor_ = i; break; }
    }
    hackerShopRow_ = 0;
    listRow_ = 0;
    detailItem_ = nullptr;
    dirty_ = true;
}

HackerSlotId Game::enteredHackerId() const {
    return hackerCarouselSlots()[cursor_].id;
}

bool Game::hackerSlotAccessible(int slot) const {
    if (slot < 0 || slot >= kCarouselSlots) return false;
    const HackerCarouselSlot& s = hackerCarouselSlots()[slot];
    // MERGE is the one slot whose accessibility isn't fixed at compile time — it
    // flips live the moment the f Merge Hub purchase lands (mirrors
    // drawHackerCarousel's `mergeUnlocked` override).
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
    hackerShopRow_ = 0;      // the first PURCHASABLE row; SERVICES sits above it
    shopView_ = ShopView::Rows;
    rigServiceRow_ = 0;
    rigServiceScroll_ = 0;
    hackerVaultRow_ = 0;
    hackerMergeRow_ = 0;
    crewView_ = CrewView::Hub;   // every entry opens at the top of the CREW screen
    crewHubRow_ = 0;
    crewTeamRow_ = 0;
    crewDetail_ = -1;
    crewProseScroll_ = 0;
    crewNetRow_ = 0;
    peerRow_ = 0;
    pvpRow_ = 0;
}

void Game::onHackerSubmenu(const ButtonEvent& ev) {
    switch (enteredHackerId()) {
        case HackerSlotId::Crew:
            onHackerCrew(ev);
            break;
        case HackerSlotId::Shop:
            onHackerShop(ev);
            break;
        case HackerSlotId::Vault:
            onHackerVault(ev);
            break;
        case HackerSlotId::Merge:
            onHackerMerge(ev);
            break;
        case HackerSlotId::Peers:
            onHackerPeers(ev);
            break;
        case HackerSlotId::Link:
            onHackerLink(ev);
            break;
        case HackerSlotId::Profile:
        default:
            // PROFILE is a flat read-only viewer (the STAT analog): nothing to select,
            // C backs out to the hacker carousel. A/B are no-ops.
            if (ev.button == Button::C) nav_ = Nav::Cursor;
            break;
    }
}

namespace {
// Is this an item the VAULT cashes in? Two Use-actions qualify: a sealed cache opens
// for a reward draw, and a Decryptogram buys a board. Both are spent HERE rather than
// pet-side, because what either pays out is player-level — which is also what
// itemUsable's two gate messages say from the other side.
bool vaultCashable(const ItemDef& d) {
    return d.use == ItemDef::Use::OpenContainer || d.use == ItemDef::Use::PlayCryptogram;
}
}  // namespace

int Game::vaultOwnedRows(const ItemDef** rows, int max) const {
    int n = 0;
    for (const auto& s : inventory_.stacks()) {
        if (s.qty <= 0) continue;
        const ItemDef* d = registry_.item(s.id);
        if (d && vaultCashable(*d) && n < max) rows[n++] = d;
    }
    // Tickets above caches, then richest draw first — a cache's rarity IS its payout
    // tier (content_items.cpp grades bits/draws/pool by it), so the top cache row is
    // always the best thing to open, and a Decryptogram outranks all of them because it
    // is the only row here that is a GAME rather than a draw. Inventory order is
    // insertion order, which says nothing a player cares about.
    std::stable_sort(rows, rows + n, [](const ItemDef* a, const ItemDef* b) {
        const bool at = a->use == ItemDef::Use::PlayCryptogram;
        const bool bt = b->use == ItemDef::Use::PlayCryptogram;
        if (at != bt) return at;
        return a->rarity > b->rarity;
    });
    return n;
}

void Game::cashVaultRow(const ItemDef& d) {
    if (d.use != ItemDef::Use::PlayCryptogram) { openSealedCache(d); return; }
    // Spend the ticket FIRST: the board cannot be walked out of, so there is no path
    // where it is consumed and no board follows, and taking it up front means a reboot
    // mid-quote costs the ticket rather than duplicating it.
    if (inventory_.count(d.id) <= 0) return;
    // Every quote won already? Then there is no first-solve prize left and the board is
    // replayed for Bits — the same deal the arcade cabinet offers, minus the cabinet.
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

void Game::onHackerVault(const ButtonEvent& ev) {
    // VAULT — cash in what a pet has no use for, OFF pet ITEMS. Build the list of
    // owned cashable stacks (caches to decrypt, Decryptograms to play); A cycles, C
    // backs to the carousel. B: unowned e resolves the focused row immediately
    // (cashVaultRow); owned, B becomes a tap/hold gesture ON A CACHE ROW — arm here and
    // resolve on the button-RELEASE edge (vaultBulkReleaseB, a short tap cashes just
    // the one) or on the hold crossing kBulkOpenHoldMs (tick() bulk-opens every cache
    // sharing the row's rarity instead).
    const ItemDef* rows[16];
    const int n = vaultOwnedRows(rows, 16);
    if (ev.button == Button::C) { nav_ = Nav::Cursor; return; }
    if (n == 0) return;                                       // empty state: A/B inert
    if (hackerVaultRow_ >= n) hackerVaultRow_ = 0;
    if (ev.button == Button::A) {
        hackerVaultRow_ = (hackerVaultRow_ + 1) % n;
    } else if (ev.button == Button::B) {
        // The hold gesture is a CACHE gesture — "open every cache of this rarity" means
        // nothing on a Decryptogram, and arming it there would swallow the tap that is
        // supposed to start a board. A ticket row therefore always resolves on the press.
        if (bulkOpenUnlocked() &&
            rows[hackerVaultRow_]->use == ItemDef::Use::OpenContainer) {
            bHeld_ = true;
            bDownMs_ = nowMs_;
        } else {
            cashVaultRow(*rows[hackerVaultRow_]);   // a reward draw, or a board
        }
    }
}

void Game::vaultBulkReleaseB() {
    if (!(face_ == Face::Hacker && nav_ == Nav::Submenu &&
          enteredHackerId() == HackerSlotId::Vault && bulkOpenUnlocked()))
        return;   // bHeld_ was armed elsewhere (e.g. the unrelated CFG hold) — no-op
    const ItemDef* rows[16];
    const int n = vaultOwnedRows(rows, 16);
    if (n == 0) return;
    if (hackerVaultRow_ >= n) hackerVaultRow_ = 0;
    cashVaultRow(*rows[hackerVaultRow_]);
    dirty_ = true;
    lastInputMs_ = nowMs_;
}

// --- Rendering -------------------------------------------------------------

void Game::drawHackerHome(Framebuffer& fb, int cursor) const {
    fb.clear(palColor(Pal::PAPER));

    // Terminal scanline pass over the living area — the procedural "console" tell
    // (SCREEN_FX budget). Dim horizontal lines every few rows; grayscale-safe
    // (it only darkens, never re-colours, so the readout stays legible).
    for (int y = kLivingTop; y < kLivingBottom; y += 4)
        for (int x = 0; x < kActiveW; ++x)
            fb.blendPixel(x, y, palColor(Pal::TRACK), 90);

    int y = kLivingTop + 8;

    // HackerTag — the operator's handle, framed like a prompt banner.
    char tag[kHackerTagMax + 8];
    std::snprintf(tag, sizeof(tag), "// %s //", hackerTag_);
    drawText(fb, kMargin, y, tag, palColor(Pal::INK));
    y += kFontH + 6;

    // Crew line — the operator's allegiance, or the prompt toward the CREW slot.
    if (const CrewDef* c = activeCrew())
        drawText(fb, kMargin, y, c->displayName, palColor(Pal::INK));
    else
        drawText(fb, kMargin, y, "NO CREW - SEE CREW SLOT", palColor(Pal::INK_DIM));
    y += kFontH + 4;

    // Equipped Title — its real home is the Hacker face. NONE → greyed.
    if (equippedTitle_ >= 0) {
        char t[28];
        std::snprintf(t, sizeof(t), "* %s", equippedTitleName());
        drawText(fb, kMargin, y, t, palColor(Pal::ACCENT));
    } else {
        drawText(fb, kMargin, y, "* NO TITLE", palColor(Pal::INK_DIM));
    }
    y += kFontH + 6;

    fb.fillRect(kMargin, y, kActiveW - 2 * kMargin, 1, palColor(Pal::TRACK));
    y += 6;

    // Rank + NETS/SHAKES — the brag line the PROFILE viewer expands.
    char rank[28];
    std::snprintf(rank, sizeof(rank), "RANK %d %s", hackerRank_,
                  hackerRankTitle(hackerRank_));
    drawText(fb, kMargin, y, rank, palColor(Pal::INK));
    y += kFontH + 4;
    char nets[20];
    std::snprintf(nets, sizeof(nets), "NETS %d", networksSeen_);
    drawText(fb, kMargin, y, nets, palColor(Pal::INK_DIM));
    char shakes[20];
    std::snprintf(shakes, sizeof(shakes), "SHAKES %d", handshakesSeen_);
    drawText(fb, kActiveW - kMargin - textWidth(shakes), y, shakes,
             palColor(Pal::INK_DIM));
    y += kFontH + 4;
    // Queued sightings not yet flushed to SD — the "walk soon" pressure gauge.
    char queued[24];
    std::snprintf(queued, sizeof(queued), "QUEUED %d/%d", pendingNetworks(),
                  kPendingNetworkQueueCap);
    const Rgb565 queuedColor = pendingNetworks() >= kPendingNetworkQueueCap
                                    ? palColor(Pal::WARN)
                                    : palColor(Pal::INK_DIM);
    drawText(fb, kMargin, y, queued, queuedColor);
    y += kFontH + 6;

    // Battery — the operator's own power budget, the one readout here that is about
    // the DEVICE rather than the operator, which is why it sits under the stats with
    // a line to itself instead of pairing off with one of them. It gets the line
    // because the glyph is a 16px cell against an 8px face: sharing a row would either
    // crop it or prise the stat rows apart.
    //
    // Dual-coded three times over — countable bars, the emptying shell, and the
    // printed number — so the HOT a flat pack takes is the only channel a grayscale
    // shot loses, which is what the tinting rule asks. Absent on the host and on a
    // board with the monitor off: an empty shell there would report a flat battery
    // rather than no battery, and silence is the honest reading.
    if (batteryLevel_ >= 0) {
        char pct[12];
        std::snprintf(pct, sizeof(pct), power_.charging ? "CHG %d%%" : "%d%%",
                      power_.percent);
        const Rgb565 battColor = !power_.charging && power_.percent <= kBatteryLowPct
                                     ? palColor(Pal::HOT)
                                     : palColor(Pal::INK);
        const SpriteData& glyph = ASSET_ICON_SYS_BATTERY;
        drawSpriteTinted(fb, glyph, batteryLevel_, kMargin, y, battColor);
        // Centred against the glyph rather than sitting on its top edge, so the two
        // read as one readout: an 8px face inside a 16px cell.
        drawText(fb, kMargin + glyph.frameW + 4, y + (glyph.h - kFontH) / 2, pct,
                 battColor);
        y += glyph.h + 6;
    }

    // Blinking console cursor — the "idle terminal" motion.
    const bool on = ((beat_ / 2) & 1) == 0;
    drawText(fb, kMargin, y, on ? "> _" : ">", palColor(Pal::ACCENT));

    // Build the inaccessible mask from the same predicate B-dispatch asks, so a slot
    // can never look live and refuse to open (or the reverse).
    unsigned lockedMask = 0;
    for (int i = 0; i < kCarouselSlots; ++i)
        if (!hackerSlotAccessible(i)) lockedMask |= (1u << i);
    drawHackerCarousel(fb, cursor, uiMode_, beat_, mergeHubUnlocked(), lockedMask);
}

void Game::drawHackerSubmenu(Framebuffer& fb) const {
    switch (enteredHackerId()) {
        // Each slot's screen lives in its own unit and draws its own header band, since
        // only the screen knows its title, tint and position note.
        case HackerSlotId::Shop: drawRigShop(fb); break;          // game_rigshop.cpp
        case HackerSlotId::Vault: drawHackerVault(fb); break;
        case HackerSlotId::Crew: drawHackerCrew(fb); break;       // game_crew.cpp
        case HackerSlotId::Peers: drawHackerPeers(fb); break;     // game_peers.cpp
        case HackerSlotId::Link: drawHackerLink(fb); break;       // game_pvp.cpp
        case HackerSlotId::Merge: drawHackerMerge(fb); break;     // game_merge.cpp
        // PROFILE — the read-only operator viewer (the STAT analog). SCAN is never
        // entered (hackerSlotAccessible), so it shares the fallback onHackerSubmenu uses.
        case HackerSlotId::Profile:
        default:
            drawHackerProfile(fb);
            break;
    }
}

void Game::drawHackerVault(Framebuffer& fb) const {
    // VAULT — cash in what a pet has no use for, OFF pet ITEMS. One row per owned
    // cashable stack (name + count); A cycles, B spends it (cashVaultRow), C exits.
    drawHeaderBand(fb, "VAULT");

    const ItemDef* rows[16];
    const int n = vaultOwnedRows(rows, 16);
    if (n == 0) {
        drawText(fb, kMargin, 40, "NOTHING TO CASH IN", palColor(Pal::INK_DIM));
        return;
    }
    int sel = hackerVaultRow_;
    if (sel >= n) sel = 0;
    const int scrollTop = listScrollTop(sel, n, kHackerListRows);
    for (int v = 0; v < kHackerListRows && scrollTop + v < n; ++v) {
        const int i = scrollTop + v;
        const int rowY = kHackerListTop + v * kHackerListRowH;
        const bool s = i == sel;
        if (s) drawRowCursor(fb, 2, rowY + 1, palColor(Pal::ACCENT));
        // The cache glyph takes its rarity's colour. The tint states nothing new:
        // every container's own name carries the tier word ("Epic Cache"), and the
        // list is ordered by it — see core/ui/theme.h.
        if (const SpriteData* icon = itemIcon(registry_, rows[i]->id))
            drawSpriteTinted(fb, *icon, 0, kMargin, rowY - 7,
                             rarityColor(rows[i]->rarity));
        char line[40];
        std::snprintf(line, sizeof(line), "%s x%d", rows[i]->displayName,
                      inventory_.count(rows[i]->id));
        drawText(fb, kMargin + 26, rowY, line,
                 s ? palColor(Pal::INK) : palColor(Pal::INK_DIM));
    }
    drawScrollbar(fb, kHackerListTop, kHackerListRows * kHackerListRowH, n, scrollTop,
                  kHackerListRows);
    fb.fillRect(0, kActiveH - 26, kActiveW, 1, palColor(Pal::TRACK));
    // B does two different things here, so it says which: a cache is DECRYPTED for
    // a draw, a Decryptogram is PLAYED as a board.
    const bool ticket = rows[sel]->use == ItemDef::Use::PlayCryptogram;
    drawText(fb, kMargin, kActiveH - 20, ticket ? "B PLAY" : "B DECRYPT",
             palColor(Pal::ACCENT));
    // The bulk-open hint (exception rule, VISUAL_LANGUAGE §4.2) — only once owned, so
    // an unowned player is never shown a gesture they don't have. Never on a ticket
    // row, which has no bulk gesture to advertise.
    if (bulkOpenUnlocked() && !ticket)
        drawText(fb, kActiveW - kMargin - textWidth("HOLD - OPEN ALL"),
                 kActiveH - 20, "HOLD - OPEN ALL", palColor(Pal::INK_DIM));
}

void Game::drawHackerProfile(Framebuffer& fb) const {
    drawHeaderBand(fb, "PROFILE");
    char tag[kHackerTagMax + 8];
    std::snprintf(tag, sizeof(tag), "// %s //", hackerTag_);
    drawText(fb, kMargin, 28, tag, palColor(Pal::INK));

    // Crew, then Title — a line each, not a pair sharing one (unaffiliated → greyed).
    // Both are content that grows to 20 characters against a 26-character line, so
    // side by side the widest pair leaves the crew a five-character window to scroll
    // through, which is no way to read the one word that says who you run with.
    const CrewDef* c = activeCrew();
    drawTextMarquee(fb, kMargin, 44, kActiveW - 2 * kMargin,
                    c ? c->displayName : "NO CREW",
                    c ? palColor(Pal::INK) : palColor(Pal::INK_DIM), beat_, true);
    char titleTag[24];
    std::snprintf(titleTag, sizeof(titleTag), "* %s",
                  equippedTitle_ >= 0 ? equippedTitleName() : "NO TITLE");
    drawTextMarquee(fb, kMargin, 58, kActiveW - 2 * kMargin, titleTag,
                    palColor(equippedTitle_ >= 0 ? Pal::ACCENT : Pal::INK_DIM), beat_,
                    true);
    fb.fillRect(0, 68, kActiveW, 1, palColor(Pal::TRACK));

    // The operator's own standing, on one 16px row pitch: rank · nets · shakes ·
    // sigils · species · steps · queued · bandwidth, then the defended network last.
    // Same shape as the identity row above: the value owns the right end and the label
    // yields to it, so the widest rank title on the ladder cannot land on top of its
    // own label.
    //
    // The pitch is 16 against an 8px face, so every row keeps a full cell of air under
    // it and the block reads as one list. Nine rows plus the bandwidth bar is what the
    // foot holds at that pitch — HOME NET's text ends at 218 against a 224 canvas, so a
    // tenth row needs the pitch or the identity block above to give something up first.
    auto statRow = [&](int ry, const char* label, const char* value, Rgb565 vc) {
        drawLabelValue(fb, kMargin, ry, label, palColor(Pal::INK_DIM), value, vc,
                       beat_, true);
    };
    // "RANK", not "HACKER RANK": the label shares its line with a title of up to 16
    // characters, and the short form is what leaves room for the longest of them. It
    // is also what SYSTEM INFO calls the same readout.
    char rankVal[24];
    std::snprintf(rankVal, sizeof(rankVal), "R%d %s", hackerRank_,
                  hackerRankTitle(hackerRank_));
    statRow(76, "RANK", rankVal, palColor(Pal::ACCENT));
    char nets[12];
    std::snprintf(nets, sizeof(nets), "%d", networksSeen_);
    statRow(92, "NETS", nets, palColor(Pal::INK));
    // SHAKES is UNSPENT/LIFETIME, the same n/N shape QUEUED uses below. A shake is what
    // buys a sigil of the Cant (game_shiboleet.cpp), so the number that matters when
    // standing in front of a guardian is what is left to spend — and the lifetime tally
    // is still the brag it always was, right beside it.
    char shakes[16];
    std::snprintf(shakes, sizeof(shakes), "%d/%d", shakesUnspent(), handshakesSeen_);
    statRow(108, "SHAKES", shakes, palColor(Pal::INK));
    // SIGILS — how much of the guardians' language this device can read. Dim at zero,
    // because none of it is a real state rather than a missing one, and ACCENT once the
    // whole Cant is known: at that point no guardian will ever refuse the pet again.
    char sigils[16];
    std::snprintf(sigils, sizeof(sigils), "%d/%d", sigilsKnown(), kCantSigils);
    statRow(124, "SIGILS", sigils,
            palColor(sigilsKnown() == 0 ? Pal::INK_DIM
                     : sigilsKnown() >= kCantSigils ? Pal::ACCENT : Pal::INK));

    // SPECIES — distinct creatures this device has ever raised (Game::speciesRaised,
    // save v39). A collection stat rather than a radio one, but it belongs on the
    // operator face for the same reason the others do: it outlives every pet.
    char species[12];
    std::snprintf(species, sizeof(species), "%d", speciesRaised());
    statRow(140, "SPECIES", species, palColor(Pal::INK));

    // STEPS — explore steps walked across every pet this device has raised, which is
    // the one explore stat measured directly rather than through a result (a boss, a
    // sub-area, a network). The STEPS ladder is scored off it (AchSeries::StepsWalked),
    // so this is also where a player reads how far they are from the next rung.
    char steps[16];
    std::snprintf(steps, sizeof(steps), "%lu",
                  static_cast<unsigned long>(lifetimeSteps_));
    statRow(156, "STEPS", steps, palColor(Pal::INK));

    // QUEUED — sightings waiting in RAM for a walk (EXPL Wi-Fi event) to flush
    // them into the SD-backed ledger; nears kPendingNetworkQueueCap = new
    // sightings start dropping, so this is the "walk your pet soon" signal.
    char queued[16];
    std::snprintf(queued, sizeof(queued), "%d/%d", pendingNetworks(),
                  kPendingNetworkQueueCap);
    const Rgb565 queuedColor = pendingNetworks() >= kPendingNetworkQueueCap
                                    ? palColor(Pal::WARN)
                                    : palColor(Pal::INK);
    statRow(172, "QUEUED", queued, queuedColor);

    // BANDWIDTH — the farming pool, shown n/N with its fill bar so the player
    // reads their farm budget on the operator face. The bar level is the grayscale channel.
    char bw[16];
    std::snprintf(bw, sizeof(bw), "%d/%d", bandwidth_, bandwidthMax());
    statRow(188, "BANDWIDTH", bw, palColor(Pal::INK));
    const float t = bandwidthMax() > 0
                        ? static_cast<float>(bandwidth_) / bandwidthMax() : 0.f;
    drawProgressBar(fb, kMargin, 198, kActiveW - 2 * kMargin, 7, t,
                    palColor(Pal::ACCENT));

    // HOME NET — the network this operator defends (set in CREW). Sits with the
    // identity stats because it's a property of the player, not the pet.
    statRow(210, "HOME NET", hasHomeNetwork() ? homeNetworkName_ : "NOT SET",
            hasHomeNetwork() ? palColor(Pal::INK) : palColor(Pal::INK_DIM));
}

}  // namespace mal
