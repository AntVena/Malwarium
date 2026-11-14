#include "core/app/game/hacker_screen.h"

#include <cstdio>

#include "tunables.h"
#include "core/app/operator.h"
#include "core/app/settings.h"
#include "core/content/content_crews.h"
#include "core/device/power_status.h"
#include "core/model/hacker_rank.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/carousel.h"
#include "core/ui/layout.h"
#include "core/ui/theme.h"
#include "core/ui/widgets.h"
#include "generated/assets.h"

namespace mal {

void HackerScreen::onProfile(HackerHost& host, const ButtonEvent& ev) {
    // A flat read-only viewer (the STAT analog): nothing to select, C backs out to the
    // hacker carousel. A/B are no-ops.
    if (ev.button == Button::C) host.leaveToCarousel();
}

void HackerScreen::drawHome(Framebuffer& fb, const HackerHost& host, int cursor) const {
    const Operator& op = host.operatorRecord();
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
    std::snprintf(tag, sizeof(tag), "// %s //", op.hackerTag);
    drawText(fb, kMargin, y, tag, palColor(Pal::INK));
    y += kFontH + 6;

    // Crew line: the operator's allegiance, or the prompt toward the CREW slot.
    if (const CrewDef* c = host.activeCrew())
        drawText(fb, kMargin, y, c->displayName, palColor(Pal::INK));
    else
        drawText(fb, kMargin, y, "NO CREW - SEE CREW SLOT", palColor(Pal::INK_DIM));
    y += kFontH + 4;

    // Equipped Title: its real home is the Hacker face. NONE → greyed.
    if (op.world.equippedTitle >= 0) {
        char t[28];
        std::snprintf(t, sizeof(t), "* %s", host.equippedTitleName());
        drawText(fb, kMargin, y, t, palColor(Pal::ACCENT));
    } else {
        drawText(fb, kMargin, y, "* NO TITLE", palColor(Pal::INK_DIM));
    }
    y += kFontH + 6;

    fb.fillRect(kMargin, y, kActiveW - 2 * kMargin, 1, palColor(Pal::TRACK));
    y += 6;

    // Rank + NETS/SHAKES: the brag line the PROFILE viewer expands.
    char rank[28];
    std::snprintf(rank, sizeof(rank), "RANK %d %s", op.hackerRank,
                  hackerRankTitle(op.hackerRank));
    drawText(fb, kMargin, y, rank, palColor(Pal::INK));
    y += kFontH + 4;
    char nets[20];
    std::snprintf(nets, sizeof(nets), "NETS %d", op.networksSeen);
    drawText(fb, kMargin, y, nets, palColor(Pal::INK_DIM));
    char shakes[20];
    std::snprintf(shakes, sizeof(shakes), "SHAKES %d", op.cant.handshakesSeen);
    drawText(fb, kActiveW - kMargin - textWidth(shakes), y, shakes,
             palColor(Pal::INK_DIM));
    y += kFontH + 4;
    // Queued sightings not yet flushed to SD, which tells the player to walk soon.
    char queued[24];
    std::snprintf(queued, sizeof(queued), "QUEUED %d/%d", host.pendingNetworks(),
                  kPendingNetworkQueueCap);
    const Rgb565 queuedColor = host.pendingNetworks() >= kPendingNetworkQueueCap
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
    if (host.batteryLevel() >= 0) {
        char pct[12];
        std::snprintf(pct, sizeof(pct), host.powerStatus().charging ? "CHG %d%%" : "%d%%",
                      host.powerStatus().percent);
        const Rgb565 battColor = !host.powerStatus().charging && host.powerStatus().percent <= kBatteryLowPct
                                     ? palColor(Pal::HOT)
                                     : palColor(Pal::INK);
        const SpriteData& glyph = ASSET_ICON_SYS_BATTERY;
        drawSpriteTinted(fb, glyph, host.batteryLevel(), kMargin, y, battColor);
        // Centred against the glyph rather than sitting on its top edge, so the two
        // read as one readout: an 8px face inside a 16px cell.
        drawText(fb, kMargin + glyph.frameW + 4, y + (glyph.h - kFontH) / 2, pct,
                 battColor);
        y += glyph.h + 6;
    }

    // Blinking console cursor: the "idle terminal" motion.
    const bool on = ((host.screenBeat() / 2) & 1) == 0;
    drawText(fb, kMargin, y, on ? "> _" : ">", palColor(Pal::ACCENT));

    // Build the mask from the same predicate B-dispatch asks, so a slot can never look
    // live and refuse to open (or the reverse).
    unsigned accessibleMask = 0;
    for (int i = 0; i < kCarouselSlots; ++i)
        if (host.hackerSlotAccessible(i)) accessibleMask |= (1u << i);
    drawHackerCarousel(fb, cursor, host.settingsRecord().uiMode, accessibleMask);
}

void HackerScreen::drawProfile(Framebuffer& fb, const HackerHost& host) const {
    const Operator& op = host.operatorRecord();
    const int beat = host.screenBeat();
    drawHeaderBand(fb, "PROFILE");
    char tag[kHackerTagMax + 8];
    std::snprintf(tag, sizeof(tag), "// %s //", op.hackerTag);
    drawText(fb, kMargin, 28, tag, palColor(Pal::INK));

    // Crew, then Title, a line each (unaffiliated → greyed). Both are content that
    // grows to 20 characters against a 26-character line, so side by side the widest
    // pair leaves the crew a five-character window to scroll through, which is no
    // way to read the one word that says who you run with.
    const CrewDef* c = host.activeCrew();
    drawTextMarquee(fb, kMargin, 44, kActiveW - 2 * kMargin,
                    c ? c->displayName : "NO CREW",
                    c ? palColor(Pal::INK) : palColor(Pal::INK_DIM), beat, Overflow::Scroll);
    char titleTag[24];
    std::snprintf(titleTag, sizeof(titleTag), "* %s",
                  op.world.equippedTitle >= 0 ? host.equippedTitleName() : "NO TITLE");
    drawTextMarquee(fb, kMargin, 58, kActiveW - 2 * kMargin, titleTag,
                    palColor(op.world.equippedTitle >= 0 ? Pal::ACCENT : Pal::INK_DIM), beat,
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
                       beat, Overflow::Scroll);
    };
    // "RANK", not "HACKER RANK": the label shares its line with a title of up to 16
    // characters, and the short form is what leaves room for the longest of them. It
    // is also what SYSTEM INFO calls the same readout.
    char rankVal[24];
    std::snprintf(rankVal, sizeof(rankVal), "R%d %s", op.hackerRank,
                  hackerRankTitle(op.hackerRank));
    statRow(76, "RANK", rankVal, palColor(Pal::ACCENT));
    char nets[12];
    std::snprintf(nets, sizeof(nets), "%d", op.networksSeen);
    statRow(92, "NETS", nets, palColor(Pal::INK));
    // SHAKES is UNSPENT/LIFETIME, the same n/N shape QUEUED uses below. A shake is what
    // buys a sigil of the Cant (game_shiboleet.cpp), so the number that matters when
    // standing in front of a guardian is what is left to spend, and the lifetime tally
    // is still the brag it always was, right beside it.
    char shakes[16];
    std::snprintf(shakes, sizeof(shakes), "%d/%d", host.shakesUnspent(), op.cant.handshakesSeen);
    statRow(108, "SHAKES", shakes, palColor(Pal::INK));
    // SIGILS: how much of the guardians' language this device can read. Dim at zero,
    // because none of it is a real state rather than a missing one, and ACCENT once the
    // whole Cant is known: at that point no guardian will ever refuse the pet again.
    char sigils[16];
    std::snprintf(sigils, sizeof(sigils), "%d/%d", host.sigilsKnown(), kCantSigils);
    statRow(124, "SIGILS", sigils,
            palColor(host.sigilsKnown() == 0 ? Pal::INK_DIM
                     : host.sigilsKnown() >= kCantSigils ? Pal::ACCENT : Pal::INK));

    // SPECIES: distinct creatures this device has ever raised (Game::speciesRaised,
    // save v39). A collection stat rather than a radio one, but it belongs on the
    // operator face for the same reason the others do: it outlives every pet.
    char species[12];
    std::snprintf(species, sizeof(species), "%d", host.speciesRaised());
    statRow(140, "SPECIES", species, palColor(Pal::INK));

    // STEPS: explore steps walked across every pet this device has raised, the only
    // explore stat counted directly instead of through a result (a boss, a sub-area, a
    // network). The STEPS ladder is scored off it (AchSeries::StepsWalked), so this is
    // also where a player reads how far they are from the next rung.
    char steps[16];
    std::snprintf(steps, sizeof(steps), "%lu",
                  static_cast<unsigned long>(op.lifetimeSteps));
    statRow(156, "STEPS", steps, palColor(Pal::INK));

    // QUEUED: sightings waiting in RAM for a walk (EXPL Wi-Fi event) to flush
    // them into the SD-backed ledger; near kPendingNetworkQueueCap new
    // sightings start dropping, so this tells the player to walk soon.
    char queued[16];
    std::snprintf(queued, sizeof(queued), "%d/%d", host.pendingNetworks(),
                  kPendingNetworkQueueCap);
    const Rgb565 queuedColor = host.pendingNetworks() >= kPendingNetworkQueueCap
                                    ? palColor(Pal::WARN)
                                    : palColor(Pal::INK);
    statRow(172, "QUEUED", queued, queuedColor);

    // BANDWIDTH: the farming pool, shown n/N with its fill bar so the player
    // reads their farm budget on the operator face. The bar level is the grayscale channel.
    char bw[16];
    std::snprintf(bw, sizeof(bw), "%d/%d", host.bandwidth(), host.bandwidthMax());
    statRow(188, "BANDWIDTH", bw, palColor(Pal::INK));
    const float t = host.bandwidthMax() > 0
                        ? static_cast<float>(host.bandwidth()) / host.bandwidthMax() : 0.f;
    drawProgressBar(fb, kMargin, 198, kActiveW - 2 * kMargin, 7, t,
                    palColor(Pal::ACCENT));

    // HOME NET: the network this operator defends (set in CREW). Sits with the
    // identity stats because it's a property of the player, not the pet.
    statRow(210, "HOME NET", host.hasHomeNetwork() ? op.homeNetworkName : "NOT SET",
            host.hasHomeNetwork() ? palColor(Pal::INK) : palColor(Pal::INK_DIM));
}

}  // namespace mal
