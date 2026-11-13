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

// game_hacker.cpp: the Hacker face (07, /HACK2/SHOP1).
//
// A+C at the top level flips PET ↔ HACKER (toggleFace). The Hacker face reuses the
// pet-side carousel + submenu contracts wholesale (the same Nav::Idle/Cursor/Submenu
// states and the same A/B/C cursor cycling), so the code specific to this face is the
// hacker slot roster, the PROFILE viewer and VAULT, and the terminal-reskin centre
// canvas. The SHOP (the Rig Shop and its SERVICES board) lives in game_rigshop.cpp;
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
    pvpRow_ = 0;
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

void Game::drawHackerHome(Framebuffer& fb, int cursor) const {
    fb.clear(palColor(Pal::PAPER));

    // Terminal scanline pass over the living area: the procedural "console" tell
    // (SCREEN_FX budget). Dim horizontal lines every few rows; grayscale-safe
    // (it only darkens, never re-colours, so the readout stays legible).
    for (int y = kLivingTop; y < kLivingBottom; y += 4)
        for (int x = 0; x < kActiveW; ++x)
            fb.blendPixel(x, y, palColor(Pal::TRACK), 90);

    int y = kLivingTop + 8;

    // HackerTag: the operator's handle, framed like a prompt banner.
    char tag[kHackerTagMax + 8];
    std::snprintf(tag, sizeof(tag), "// %s //", op_.hackerTag);
    drawText(fb, kMargin, y, tag, palColor(Pal::INK));
    y += kFontH + 6;

    // Crew line: the operator's allegiance, or the prompt toward the CREW slot.
    if (const CrewDef* c = activeCrew())
        drawText(fb, kMargin, y, c->displayName, palColor(Pal::INK));
    else
        drawText(fb, kMargin, y, "NO CREW - SEE CREW SLOT", palColor(Pal::INK_DIM));
    y += kFontH + 4;

    // Equipped Title: its real home is the Hacker face. NONE → greyed.
    if (op_.world.equippedTitle >= 0) {
        char t[28];
        std::snprintf(t, sizeof(t), "* %s", equippedTitleName());
        drawText(fb, kMargin, y, t, palColor(Pal::ACCENT));
    } else {
        drawText(fb, kMargin, y, "* NO TITLE", palColor(Pal::INK_DIM));
    }
    y += kFontH + 6;

    fb.fillRect(kMargin, y, kActiveW - 2 * kMargin, 1, palColor(Pal::TRACK));
    y += 6;

    // Rank + NETS/SHAKES: the brag line the PROFILE viewer expands.
    char rank[28];
    std::snprintf(rank, sizeof(rank), "RANK %d %s", op_.hackerRank,
                  hackerRankTitle(op_.hackerRank));
    drawText(fb, kMargin, y, rank, palColor(Pal::INK));
    y += kFontH + 4;
    char nets[20];
    std::snprintf(nets, sizeof(nets), "NETS %d", op_.networksSeen);
    drawText(fb, kMargin, y, nets, palColor(Pal::INK_DIM));
    char shakes[20];
    std::snprintf(shakes, sizeof(shakes), "SHAKES %d", op_.cant.handshakesSeen);
    drawText(fb, kActiveW - kMargin - textWidth(shakes), y, shakes,
             palColor(Pal::INK_DIM));
    y += kFontH + 4;
    // Queued sightings not yet flushed to SD, which tells the player to walk soon.
    char queued[24];
    std::snprintf(queued, sizeof(queued), "QUEUED %d/%d", pendingNetworks(),
                  kPendingNetworkQueueCap);
    const Rgb565 queuedColor = pendingNetworks() >= kPendingNetworkQueueCap
                                    ? palColor(Pal::WARN)
                                    : palColor(Pal::INK_DIM);
    drawText(fb, kMargin, y, queued, queuedColor);
    y += kFontH + 6;

    // Battery describes the device, so it gets its own line under the operator stats.
    // It gets the line because the glyph is a 16px cell against an 8px face: sharing a
    // row would either crop it or prise the stat rows apart.
    //
    // Dual-coded three times over (countable bars, the emptying shell, and the
    // printed number), so the HOT a flat pack takes is the only channel a grayscale
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

    // Blinking console cursor: the "idle terminal" motion.
    const bool on = ((beat_ / 2) & 1) == 0;
    drawText(fb, kMargin, y, on ? "> _" : ">", palColor(Pal::ACCENT));

    // Build the mask from the same predicate B-dispatch asks, so a slot can never look
    // live and refuse to open (or the reverse).
    unsigned accessibleMask = 0;
    for (int i = 0; i < kCarouselSlots; ++i)
        if (hackerSlotAccessible(i)) accessibleMask |= (1u << i);
    drawHackerCarousel(fb, cursor, settings_.uiMode, accessibleMask);
}

void Game::drawHackerSubmenu(Framebuffer& fb) const {
    switch (enteredHackerId()) {
        // Each slot's screen lives in its own unit and draws its own header band, since
        // only the screen knows its title, tint and position note.
        case HackerSlotId::Shop: rigShop_.draw(fb, *this, beat_); break;   // rig_shop_screen.cpp
        case HackerSlotId::Vault: vault_.draw(fb, *this); break;
        case HackerSlotId::Crew: crew_.draw(fb, *this); break;
        case HackerSlotId::Peers: peers_.draw(fb, *this); break;
        case HackerSlotId::Link: drawHackerLink(fb); break;       // game_pvp.cpp
        case HackerSlotId::Merge: merge_.draw(fb, *this); break;
        // PROFILE: the read-only operator viewer (the STAT analog). SCAN is never
        // entered (hackerSlotAccessible), so it shares the fallback onHackerSubmenu uses.
        case HackerSlotId::Profile:
        default:
            drawHackerProfile(fb);
            break;
    }
}

void Game::drawHackerProfile(Framebuffer& fb) const {
    drawHeaderBand(fb, "PROFILE");
    char tag[kHackerTagMax + 8];
    std::snprintf(tag, sizeof(tag), "// %s //", op_.hackerTag);
    drawText(fb, kMargin, 28, tag, palColor(Pal::INK));

    // Crew, then Title, a line each (unaffiliated → greyed). Both are content that
    // grows to 20 characters against a 26-character line, so side by side the widest
    // pair leaves the crew a five-character window to scroll through, which is no
    // way to read the one word that says who you run with.
    const CrewDef* c = activeCrew();
    drawTextMarquee(fb, kMargin, 44, kActiveW - 2 * kMargin,
                    c ? c->displayName : "NO CREW",
                    c ? palColor(Pal::INK) : palColor(Pal::INK_DIM), beat_, Overflow::Scroll);
    char titleTag[24];
    std::snprintf(titleTag, sizeof(titleTag), "* %s",
                  op_.world.equippedTitle >= 0 ? equippedTitleName() : "NO TITLE");
    drawTextMarquee(fb, kMargin, 58, kActiveW - 2 * kMargin, titleTag,
                    palColor(op_.world.equippedTitle >= 0 ? Pal::ACCENT : Pal::INK_DIM), beat_,
                    Overflow::Scroll);
    fb.fillRect(0, 68, kActiveW, 1, palColor(Pal::TRACK));

    // The operator's own standing, on one 16px row pitch: rank · nets · shakes ·
    // sigils · species · steps · queued · bandwidth, then the defended network last.
    // Same shape as the identity row above: the value owns the right end and the label
    // yields to it, so the widest rank title on the ladder cannot land on top of its
    // own label.
    //
    // The pitch is 16 against an 8px face, so every row keeps a full cell of air under
    // it and the block reads as one list. Nine rows plus the bandwidth bar is what the
    // foot holds at that pitch: HOME NET's text ends at 218 against a 224 canvas, so a
    // tenth row needs the pitch or the identity block above to give something up first.
    auto statRow = [&](int ry, const char* label, const char* value, Rgb565 vc) {
        drawLabelValue(fb, kMargin, ry, label, palColor(Pal::INK_DIM), value, vc,
                       beat_, Overflow::Scroll);
    };
    // "RANK", not "HACKER RANK": the label shares its line with a title of up to 16
    // characters, and the short form is what leaves room for the longest of them. It
    // is also what SYSTEM INFO calls the same readout.
    char rankVal[24];
    std::snprintf(rankVal, sizeof(rankVal), "R%d %s", op_.hackerRank,
                  hackerRankTitle(op_.hackerRank));
    statRow(76, "RANK", rankVal, palColor(Pal::ACCENT));
    char nets[12];
    std::snprintf(nets, sizeof(nets), "%d", op_.networksSeen);
    statRow(92, "NETS", nets, palColor(Pal::INK));
    // SHAKES is UNSPENT/LIFETIME, the same n/N shape QUEUED uses below. A shake is what
    // buys a sigil of the Cant (game_shiboleet.cpp), so the number that matters when
    // standing in front of a guardian is what is left to spend, and the lifetime tally
    // is still the brag it always was, right beside it.
    char shakes[16];
    std::snprintf(shakes, sizeof(shakes), "%d/%d", shakesUnspent(), op_.cant.handshakesSeen);
    statRow(108, "SHAKES", shakes, palColor(Pal::INK));
    // SIGILS: how much of the guardians' language this device can read. Dim at zero,
    // because none of it is a real state rather than a missing one, and ACCENT once the
    // whole Cant is known: at that point no guardian will ever refuse the pet again.
    char sigils[16];
    std::snprintf(sigils, sizeof(sigils), "%d/%d", sigilsKnown(), kCantSigils);
    statRow(124, "SIGILS", sigils,
            palColor(sigilsKnown() == 0 ? Pal::INK_DIM
                     : sigilsKnown() >= kCantSigils ? Pal::ACCENT : Pal::INK));

    // SPECIES: distinct creatures this device has ever raised (Game::speciesRaised,
    // save v39). A collection stat rather than a radio one, but it belongs on the
    // operator face for the same reason the others do: it outlives every pet.
    char species[12];
    std::snprintf(species, sizeof(species), "%d", speciesRaised());
    statRow(140, "SPECIES", species, palColor(Pal::INK));

    // STEPS: explore steps walked across every pet this device has raised, the only
    // explore stat counted directly instead of through a result (a boss, a sub-area, a
    // network). The STEPS ladder is scored off it (AchSeries::StepsWalked), so this is
    // also where a player reads how far they are from the next rung.
    char steps[16];
    std::snprintf(steps, sizeof(steps), "%lu",
                  static_cast<unsigned long>(op_.lifetimeSteps));
    statRow(156, "STEPS", steps, palColor(Pal::INK));

    // QUEUED: sightings waiting in RAM for a walk (EXPL Wi-Fi event) to flush
    // them into the SD-backed ledger; near kPendingNetworkQueueCap new
    // sightings start dropping, so this tells the player to walk soon.
    char queued[16];
    std::snprintf(queued, sizeof(queued), "%d/%d", pendingNetworks(),
                  kPendingNetworkQueueCap);
    const Rgb565 queuedColor = pendingNetworks() >= kPendingNetworkQueueCap
                                    ? palColor(Pal::WARN)
                                    : palColor(Pal::INK);
    statRow(172, "QUEUED", queued, queuedColor);

    // BANDWIDTH: the farming pool, shown n/N with its fill bar so the player
    // reads their farm budget on the operator face. The bar level is the grayscale channel.
    char bw[16];
    std::snprintf(bw, sizeof(bw), "%d/%d", bandwidth_, bandwidthMax());
    statRow(188, "BANDWIDTH", bw, palColor(Pal::INK));
    const float t = bandwidthMax() > 0
                        ? static_cast<float>(bandwidth_) / bandwidthMax() : 0.f;
    drawProgressBar(fb, kMargin, 198, kActiveW - 2 * kMargin, 7, t,
                    palColor(Pal::ACCENT));

    // HOME NET: the network this operator defends (set in CREW). Sits with the
    // identity stats because it's a property of the player, not the pet.
    statRow(210, "HOME NET", hasHomeNetwork() ? op_.homeNetworkName : "NOT SET",
            hasHomeNetwork() ? palColor(Pal::INK) : palColor(Pal::INK_DIM));
}

}  // namespace mal
