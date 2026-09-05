#include "core/ui/stat_screen.h"

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "core/content/effect_text.h"
#include "core/content/registry.h"
#include "core/model/loadout.h"
#include "core/model/move_loadout.h"
#include "core/model/stat_tiers.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/layout.h"
#include "core/ui/widgets.h"
#include "generated/assets.h"
#include "tunables.h"

namespace mal {

namespace {

constexpr int kLabelX = kMargin;
constexpr int kGaugeX = 70;
constexpr int kGaugeW = 110;
constexpr int kGaugeH = 10;
constexpr int kNumX = 188;

// STAT header: title at left, a dot pager at right (one dot per page, kStatPages
// of them) with the active page filled. The pager is the non-colour channel for
// "which page" (dot position), so the status pages stay distinguishable in grayscale.
void statHeader(Framebuffer& fb, const char* title, int page) {
    drawHeaderBand(fb, title);
    for (int i = 0; i < kStatPages; ++i) {
        const int x = kActiveW - 8 - (kStatPages - i) * 8;
        fb.fillRect(x, 8, 4, 4,
                    i == page ? palColor(Pal::INK) : palColor(Pal::INK_DIM));
    }
}

void vitalsRow(Framebuffer& fb, int y, const char* label, int value, Zone zone,
               bool fragRamp, bool pulseOn, int beat) {
    drawText(fb, kLabelX, y + 2, label, palColor(Pal::INK));
    drawGauge(fb, kGaugeX, y, kGaugeW, kGaugeH, value, zone, fragRamp, pulseOn, beat);
    char num[8];
    std::snprintf(num, sizeof(num), "%3d", value);
    // Critical numerics tint hot too (dual-coded with the gauge).
    Rgb565 nc = (zone == Zone::Critical) ? palColor(Pal::HOT) : palColor(Pal::INK);
    drawText(fb, kNumX, y + 2, num, nc);
}

// The two pages' own row tops. The flow itself (row heights, the fit count, the
// scrollbar) is shared — core/ui/prose_page.h — because LOADOUT, BUFFS and the arena's
// opponent sheet are one page wearing three headers.
constexpr int kLoadoutRowTop = 26;
constexpr int kBuffRowTop = 28;
constexpr int kTierRowTop = 26;   // TIERS opens on a section heading, same as LOADOUT

// --- The INDEX's own grid ---------------------------------------------------
//
// Tighter than the list grid's kRowH: the index earns its keep by showing as much of
// the reader's map at once as it can, and a row here is a name and a number rather than
// the icon-plus-label a carousel row carries.
//
// It still WINDOWS rather than shrinking to fit — the roster grows with the page count
// and with each page's sections, and a pitch chased down to hold all of it would end up
// unreadable to buy an ability the list contract already provides (the cursor drives the
// window, listScrollTop, and A/hold-C walk it).
constexpr int kIndexRowTop = kRowTop;
constexpr int kIndexRowH = 15;
constexpr int kIndexLabelX = 20;   // clear of the cursor marker at the left margin
constexpr int kIndexSubX = 32;     // a section INSIDE a page, indented under it
constexpr int kIndexVisible = (kProseBottom - kIndexRowTop) / kIndexRowH;

// BUFFS keeps its own heights because a buff row is a name plus a COUNTDOWN rather
// than a name plus a tag — the timer is live state, re-read every repaint, where a
// ProseRow's tag is set once when the row is built. Its headings are measured the same
// way the shared flow measures its own (prose_page.cpp): the lead-in above one is part
// of its height, and the heading that opens a window takes none.
std::vector<int> buffRowHeights(const std::vector<BuffRow>& rows, int top) {
    std::vector<int> h;
    h.reserve(rows.size());
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        const BuffRow& r = rows[i];
        if (r.header) {
            h.push_back((i == top ? 0 : kProseGroupLead) + kProseHeaderH);
            continue;
        }
        h.push_back(kFontH + (r.effect.empty()
                                  ? 0
                                  : kProseNameGap +
                                        textWrapLines(r.effect.c_str(), kProseW) *
                                            kProseLineH) +
                    kProseRowGap);
    }
    return h;
}

// BUFFS' own fit count, over its own heights — the shared proseRowsFitting takes
// ProseRows, and this page's rows are not those. Same section rule as the shared flow,
// including the half-full threshold that keeps a section break from spending a whole
// screen on a one-row tail.
int buffFitCount(const std::vector<BuffRow>& rows, int top) {
    const std::vector<int> heights = buffRowHeights(rows, top);
    int y = kBuffRowTop;
    int n = 0;
    for (int i = top; i < static_cast<int>(heights.size()); ++i) {
        if (n > 0 && rows[i].header &&
            y - kBuffRowTop >= (kProseBottom - kBuffRowTop) / 2)
            break;
        if (n > 0 && y + heights[i] > kProseBottom) break;
        y += heights[i];
        ++n;
    }
    return n;
}

// BUFFS' own window count / position, the same pair the shared flow exports
// (proseWindowCount / proseWindowIndex) and for the same hint band.
int buffWindowCount(const std::vector<BuffRow>& rows) {
    int n = 0;
    for (int top = 0; top < static_cast<int>(rows.size()); ++n) {
        const int shown = buffFitCount(rows, top);
        if (shown <= 0) break;
        top += shown;
    }
    return n;
}

int buffWindowIndex(const std::vector<BuffRow>& rows, int scrollTop) {
    int n = 0;
    for (int top = 0; top < static_cast<int>(rows.size()); ++n) {
        if (top >= scrollTop) return n;
        const int shown = buffFitCount(rows, top);
        if (shown <= 0) break;
        top += shown;
    }
    return n;
}

void drawBuffScrollbar(Framebuffer& fb, int top, int shown, int total) {
    const int barX = kActiveW - 3;
    const int trackH = kProseBottom - kBuffRowTop;
    fb.fillRect(barX, kBuffRowTop, 2, trackH, palColor(Pal::TRACK));
    const int thumbH = std::max(8, trackH * shown / total);
    fb.fillRect(barX, kBuffRowTop + trackH * top / total, 2, thumbH,
                palColor(Pal::INK_DIM));
}

// The band every STAT page ends with. Two things belong on it and they are not the
// same kind of fact: what the key under the reader's thumb does right now, and that
// the INDEX exists at all. The second is the whole reason the band is drawn even on a
// page with nothing to scroll — a hold gesture nothing names is a gesture nobody finds.
//
// `windows`/`window` are the page's own window count and which one is open, 1-based
// for the reader: a scrollbar thumb says roughly how far down the page a reader is,
// and "2/5" says how much of it is left, which is the question they are actually
// asking before deciding to keep pressing.
void statHintBand(Framebuffer& fb, int window, int windows) {
    char hint[32];
    if (windows > 1)
        std::snprintf(hint, sizeof(hint), "B MORE %d/%d  HOLD B INDEX", window, windows);
    else
        std::snprintf(hint, sizeof(hint), "A PAGE  HOLD B INDEX");
    drawHintBand(fb, hint);
}

// The same band for a flowed page, off the page's own rows.
void statProseHint(Framebuffer& fb, const std::vector<ProseRow>& rows, int scrollTop,
                   int rowTop) {
    statHintBand(fb, proseWindowIndex(rows, scrollTop, rowTop) + 1,
                 proseWindowCount(rows, rowTop));
}

} // namespace

std::vector<ProseRow> buildLoadoutRows(const ContentRegistry& reg,
                                       const MoveLoadout& moveLoad,
                                       const Loadout& modLoad,
                                       Stage stage, bool isEgg) {
    std::vector<ProseRow> out;
    // An egg can't train or mod (Game::slotLocked) — nothing to show.
    if (isEgg) {
        out.push_back({false, "- NO LOADOUT -", {}, {}});
        return out;
    }

    out.push_back({true, "MOVES", {}, {}});
    // One row per unlocked slot: the equipped move, or — when the slot is empty — the
    // innate Quick Jab fallback marked (DEFAULT). Quick Jab is NOT a standalone row;
    // it surfaces only in a slot that has no move (mirrors the combat per-slot fallback,
    // ), so it never reads as a dedicated "default slot".
    const MoveDef* def = reg.move(moveLoad.defaultMove());
    const int unlocked = MoveLoadout::slotsForStage(stage);
    for (int i = 0; i < unlocked; ++i) {
        const char* id = moveLoad.equipped(i);
        if (const MoveDef* m = id ? reg.move(id) : nullptr) {
            out.push_back({false, m->displayName, {}, effectText(*m)});
        } else if (def) {
            out.push_back({false, def->displayName, {}, effectText(*def)});
            setProseTag(out.back(), "DEFAULT");
        }
    }

    out.push_back({true, "MODS", {}, {}});
    bool anyMod = false;
    for (int i = 0; i < kModSlots; ++i) {
        if (const char* id = modLoad.equipped(i)) {
            if (const ModDef* m = reg.mod(id)) {
                out.push_back({false, m->displayName, {}, effectText(*m)});
                anyMod = true;
            }
        }
    }
    if (!anyMod) out.push_back({false, "- NONE -", {}, {}});

    return out;
}

std::vector<ProseRow> buildTierRows(const int statPoints[kLevelStatCount]) {
    std::vector<ProseRow> out;
    if (!statPoints) return out;
    // Stat order, then rung order — the same order the shared table is written in, and
    // the same order every other stat-indexed readout on the device uses (0 power ·
    // 1 defense · 2 speed · 3 max-Health).
    static const char* kStatNames[kLevelStatCount] = {"POWER", "DEFENSE", "SPEED",
                                                      "MAX HEALTH"};
    for (int i = 0; i < kLevelStatCount; ++i) {
        const int points = statPoints[i];
        const int reached = statTiersReached(points);
        ProseRow head{};
        head.header = true;
        head.label = kStatNames[i];
        // The stat's own point total rides its heading — it is the one number on the page
        // that is about the STAT rather than about a rung, and every "N TO GO" below is
        // measured from it, so the two have to be readable together.
        setProseTag(head, "%d PTS", points);
        out.push_back(head);
        for (int t = 0; t < kStatTierCount; ++t) {
            const StatTierDef& def = statTier(static_cast<LevelStat>(i), t);
            ProseRow row{};
            row.label = def.name;
            row.body = statTierText(static_cast<LevelStat>(i), t);
            // Through the shared helper, not `def.points - points`: "how far to the next
            // rung" is the engine's answer to give, and a page that computes its own is
            // how a screen starts disagreeing with the thing it reports on.
            if (t < reached) setProseTag(row, "HELD");
            else if (t == reached) setProseTag(row, "%d TO GO", statTierPointsToNext(points));
            else setProseTag(row, "AT %d", def.points);
            out.push_back(row);
        }
    }
    return out;
}

int tierRowsFitting(const std::vector<ProseRow>& rows, int top) {
    return proseRowsFitting(rows, top, kTierRowTop);
}

void drawTiersScreen(Framebuffer& fb, const std::vector<ProseRow>& rows, int scrollTop,
                     int beat) {
    statHeader(fb, "TIERS", 1);
    // The flow draws no band of its own here (nullptr): every STAT page ends with the
    // same one, and it says more than the flow can know to say.
    drawProseRows(fb, rows, scrollTop, kTierRowTop, beat, nullptr);
    statProseHint(fb, rows, scrollTop, kTierRowTop);
}

int loadoutRowsFitting(const std::vector<ProseRow>& rows, int top) {
    return proseRowsFitting(rows, top, kLoadoutRowTop);
}

void drawLoadoutScreen(Framebuffer& fb, const std::vector<ProseRow>& rows,
                       int scrollTop, int beat) {
    statHeader(fb, "LOADOUT", 2);
    drawProseRows(fb, rows, scrollTop, kLoadoutRowTop, beat, nullptr);
    statProseHint(fb, rows, scrollTop, kLoadoutRowTop);
}

std::vector<BuffRow> buildBuffRows(const ContentRegistry& reg,
                                    bool restorePointArmed,
                                    bool trojanDivertArmed,
                                    bool backupDriveArmed,
                                    uint32_t backupDriveRemainMs,
                                    int depthMultiplier,
                                    bool startDepthArmed,
                                    bool startDepthUsesBest,
                                    int startDepthValue,
                                    BranchOverride branchOverride,
                                    int evolveSoakFactor,
                                    bool evolveHoldArmed,
                                    const PetUpgrades& upgrades) {
    std::vector<BuffRow> out;
    // The two headings are emitted LAZILY, by the first row that belongs under each:
    // an empty section is not a section, and a page that says PERMANENT over nothing
    // has told the reader something false about the pet.
    auto section = [&](const char* label) {
        for (const BuffRow& r : out)
            if (r.header && std::strcmp(r.label, label) == 0) return;
        BuffRow head{};
        head.label = label;
        head.header = true;
        out.push_back(head);
    };
    auto add = [&](const char* itemId, bool armed, bool timed, uint32_t remain) {
        if (!armed) return;
        const ItemDef* d = reg.item(itemId);
        section("ARMED");
        out.push_back({d ? d->displayName : itemId, d ? effectText(*d) : EffectText{},
                       timed, remain, false});
    };
    add("restore_point", restorePointArmed, false, 0);
    add("ambig_usb", trojanDivertArmed, false, 0);
    add("backup_drive", backupDriveArmed, true, backupDriveRemainMs);
    // The two DeepWeb Dive depth buffs: which ITEM armed them isn't tracked
    // directly (Game only keeps the raw multiplier/depth), so find the item whose
    // effect matches the current state instead of hardcoding an id here.
    // `heading` is which section the found row lands under — every arming lands under
    // ARMED, and an Epic dish's standing grant under PERMANENT.
    auto addByEffect = [&](ItemEffect::Kind kind, int magnitude, bool matchAnyMagnitude,
                           const char* heading = "ARMED") {
        for (const ItemDef* d : reg.allItems())
            for (const ItemEffect& e : d->effects)
                if (e.kind == kind && (matchAnyMagnitude || e.magnitude == magnitude)) {
                    section(heading);
                    out.push_back({d->displayName, effectText(*d), false, 0, false});
                    return;
                }
    };
    if (depthMultiplier > 1)
        addByEffect(ItemEffect::Kind::ArmDeepWebDepthMultiplier, depthMultiplier, false);
    if (startDepthArmed) {
        if (startDepthUsesBest)
            addByEffect(ItemEffect::Kind::SetDeepWebStartDepthToBest, 0, true);
        else
            addByEffect(ItemEffect::Kind::SetDeepWebStartDepth, startDepthValue, false);
    }
    // The rest of the USB port, found the same way and for the same reason: Game keeps
    // the forced DIRECTION and the raw soak FACTOR, not which device wrote them, so the
    // row names whichever item carries that effect. The soak matches on its magnitude
    // (which is what tells a Sandbox-USB from a Hypervisor-USB); a forced branch has no
    // magnitude at all, so it matches on the Kind alone.
    if (branchOverride == BranchOverride::Good)
        addByEffect(ItemEffect::Kind::ForceEvolveBranchGood, 0, true);
    else if (branchOverride == BranchOverride::Bad)
        addByEffect(ItemEffect::Kind::ForceEvolveBranchBad, 0, true);
    if (evolveSoakFactor > 1) {
        // Either soak Kind can be the one armed and Game keeps only the factor, so ask
        // for the plain one first and fall back to the late one — the magnitude is what
        // tells a Sandbox-USB from a Hypervisor-USB, and only one row carries each.
        const std::size_t before = out.size();
        addByEffect(ItemEffect::Kind::ArmEvolveSoak, evolveSoakFactor, false);
        if (out.size() == before)
            addByEffect(ItemEffect::Kind::ArmEvolveSoakLate, evolveSoakFactor, false);
    }
    if (evolveHoldArmed) addByEffect(ItemEffect::Kind::ArmEvolveHold, 0, true);
    // Last, because these are the rows that never lapse: everything an Epic dish has
    // permanently given this pet. Found by effect like the dive buffs above rather than
    // by id, so a dish that grants one can be renamed without touching this file — and
    // in stat order, so a pet carrying several reads as one block rather than in
    // whichever order it happened to be fed.
    if (upgrades.bandwidthRegenMin > 0)
        addByEffect(ItemEffect::Kind::BandwidthRegenBonusMin, 0, true, "PERMANENT");
    static const ItemEffect::Kind kStatGrants[kLevelStatCount] = {
        ItemEffect::Kind::StatPointPower, ItemEffect::Kind::StatPointDefense,
        ItemEffect::Kind::StatPointSpeed, ItemEffect::Kind::StatPointHealth};
    for (int i = 0; i < kLevelStatCount; ++i)
        if (upgrades.statBonus[i] > 0) addByEffect(kStatGrants[i], 0, true, "PERMANENT");
    if (upgrades.xpRatePct > 0)
        addByEffect(ItemEffect::Kind::XpRateBonusPct, 0, true, "PERMANENT");
    return out;
}

int buffRowsFitting(const std::vector<BuffRow>& rows, int top) {
    return buffFitCount(rows, top);
}

void drawBuffsScreen(Framebuffer& fb, const std::vector<BuffRow>& rows, int scrollTop,
                     int /*beat*/) {
    statHeader(fb, "BUFFS", 5);

    if (rows.empty()) {
        drawText(fb, kMargin, 60, "- NO ACTIVE BUFFS -", palColor(Pal::INK_DIM));
        statHintBand(fb, 1, 1);
        return;
    }

    const int total = static_cast<int>(rows.size());
    const bool overflow = buffFitCount(rows, 0) < total;
    const int top = overflow ? std::max(0, std::min(scrollTop, total - 1)) : 0;
    const std::vector<int> heights = buffRowHeights(rows, top);
    const int shown = buffFitCount(rows, top);

    int y = kBuffRowTop;
    for (int v = 0; v < shown; ++v) {
        const BuffRow& r = rows[top + v];
        if (r.header) {
            // Dim, like the flow's own headings: a fence, not a row.
            drawText(fb, kMargin, y + (top + v == top ? 0 : kProseGroupLead), r.label,
                     palColor(Pal::INK_DIM));
            y += heights[top + v];
            continue;
        }
        const int nx = drawText(fb, kMargin, y, r.label, palColor(Pal::INK));
        if (r.hasTimer) {
            char t[16];
            const uint32_t s = r.remainingMs / 1000u;
            std::snprintf(t, sizeof(t), "%u:%02u LEFT", s / 60u,
                          static_cast<unsigned>(s % 60u));
            drawText(fb, nx + kFontAdvance, y, t, palColor(Pal::ACCENT));
        }
        if (!r.effect.empty())
            drawTextWrapped(fb, kMargin, y + kFontH + kProseNameGap, kProseW,
                              r.effect.c_str(), palColor(Pal::INK_DIM), kProseLineH,
                              textWrapLines(r.effect.c_str(), kProseW));
        y += heights[top + v];
    }

    if (overflow) drawBuffScrollbar(fb, top, shown, total);
    statHintBand(fb, buffWindowIndex(rows, top) + 1, buffWindowCount(rows));
}

void drawSpeciesScreen(Framebuffer& fb, const char* name, const char* line,
                       const char* hint, const char* context, int beat) {
    statHeader(fb, "SPECIES", 6);

    // Name and line tag are both content, and the widest pair of them wants more than
    // the line has: a twelve-letter creature beside METAMORPHIC LINE fills it exactly.
    // The tag owns the right end — it is the fact this header adds that the rest of the
    // page doesn't — and the name yields to it and scrolls.
    char tag[24];
    tag[0] = '\0';
    if (line && line[0]) {
        // The line, not "<line> LINE": on a page headed SPECIES the word adds nothing
        // and cost 40px that several creatures need — a twelve-letter name beside
        // "METAMORPHIC LINE" overran the row, and the name is what the page is about.
        std::snprintf(tag, sizeof(tag), "%s", line);
        for (char* p = tag; *p; ++p)
            *p = static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
    }
    drawLabelValue(fb, kMargin, 30, name ? name : "", palColor(Pal::INK), tag,
                   palColor(Pal::INK_DIM), beat, true);

    // Tighter than the grid's kLineH: SPECIES stacks two wrapped prose blocks in one
    // screen, and the leading is what has to give.
    constexpr int kSpeciesLineH = kFontH + 3;
    constexpr int kSpeciesTop = 50;
    // A paragraph break, not a section: the second block carries no heading of its
    // own, so the gap and the dimmer ink are the whole of what separates them —
    // which is enough, because it reads as a footnote to the block above rather than
    // as a different KIND of thing. It also leaves the foot of the page free for
    // whatever the page grows next.
    constexpr int kBlockGap = 14;
    // The two blocks are a creature's own authored lore, and the roster's are not
    // the same length — a Daemon's read runs to nine wrapped lines where a
    // Boot-Sector's takes three. So the first block is given the room that is
    // actually LEFT rather than a fixed allowance: measure the second, hold that
    // back, and the read takes the rest. Still capped by what remains, but the cap
    // is the screen's own edge rather than a number set ahead of the content.
    const int ctxLines = (context && context[0]) ? textWrapLines(context, kProseW) : 0;
    const int hintRoom = kProseBottom - kSpeciesTop - kBlockGap -
                         ctxLines * kSpeciesLineH;
    const int hintMax = std::max(1, hintRoom / kSpeciesLineH);

    int y = kSpeciesTop;
    if (hint && hint[0]) {
        y = drawTextWrapped(fb, kMargin, y, kProseW, hint, palColor(Pal::INK),
                              kSpeciesLineH, hintMax);
    } else {
        drawText(fb, kMargin, y, "- NO DATA -", palColor(Pal::INK_DIM));
        y += kSpeciesLineH;
    }
    y += kBlockGap;

    if (ctxLines > 0)
        drawTextWrapped(fb, kMargin, y, kProseW, context, palColor(Pal::INK_DIM),
                          kSpeciesLineH, ctxLines);

    statHintBand(fb, 1, 1);
}

void drawStatScreen(Framebuffer& fb, const PetModel& m, const char* name,
                    Stage stage, int generation, int level, int combatXp,
                    int xpToNext, int beat, bool hasNextEvo, uint32_t evoRemainMs) {
    const bool pulseOn = ((beat / 2) & 1) == 0;   // ~1Hz
    statHeader(fb, "STAT", 0);

    // Name · generation. The stage is NOT here: it is spelled out under its own node on
    // the indicator below, which is where the reader is already looking to see how far
    // along the pet is. Naming it twice cost the header 94px, and a pet's name is
    // eleven or twelve characters often enough that the row was cutting it — the first
    // Ransomware egg hatches as CryptoShell, into exactly that.
    char gen[12];
    std::snprintf(gen, sizeof(gen), "GEN %d", generation);
    const int genX = kActiveW - kMargin - textWidth(gen);
    drawText(fb, genX, kRowTop, gen, palColor(Pal::INK_DIM));
    drawTextMarquee(fb, kMargin, kRowTop, genX - kMargin - kMargin, name,
                    palColor(Pal::INK), beat, true);

    drawStageIndicator(fb, kMargin, 40, 130, stage);
    // Creature level: LVL n beside the stage bar — text-only, grayscale-safe.
    char lvl[12];
    std::snprintf(lvl, sizeof(lvl), "LVL %d", level);
    drawText(fb, kActiveW - kMargin - textWidth(lvl), 44, lvl, palColor(Pal::ACCENT));

    // XP toward the next level: a slim progress bar under the stage row,
    // banked-XP / next-level-cost. Fill level is the grayscale channel; the numeric
    // readout spells out the exact threshold.
    const float xt = xpToNext > 0 ? static_cast<float>(combatXp) / xpToNext : 0.f;
    drawText(fb, kLabelX, 60, "XP", palColor(Pal::INK_DIM));
    drawProgressBar(fb, 30, 59, 118, 7, xt, palColor(Pal::ACCENT));
    char xp[16];
    std::snprintf(xp, sizeof(xp), "%d/%d", combatXp, xpToNext);
    drawText(fb, kActiveW - kMargin - textWidth(xp), 60, xp, palColor(Pal::INK_DIM));

    // Three vitals gauges.
    vitalsRow(fb, 74, "HUNGER", m.hunger(), m.hungerZone(), false, pulseOn, beat);
    vitalsRow(fb, 96, "FRAG", m.fragmentation(), m.fragZone(), true, pulseOn, beat);
    vitalsRow(fb, 118, "HAPPY", m.happiness(), m.happyZone(), false, pulseOn, beat);

    // Care mistakes.
    drawText(fb, kLabelX, 146, "CARE", palColor(Pal::INK));
    drawCarePips(fb, kGaugeX, 144, m.careMistakes(), pulseOn);
    char num[8];
    std::snprintf(num, sizeof(num), "%d", m.careMistakes());
    Rgb565 nc = (m.careBranch() == CareBranch::Dying) ? palColor(Pal::HOT)
                                                      : palColor(Pal::INK);
    drawText(fb, kNumX + 12, 146, num, nc);

    // Time-to-next-evolution (all stages). An egg counts down to its HATCH; a
    // mid-chain pet to its next EVOLVE; a Daemon terminus reads MAX (no successor).
    // Ink text readout — grayscale-safe, no colour channel needed.
    const char* evoLabel = (stage == Stage::BootSector) ? "HATCH" : "EVOLVE";
    drawText(fb, kLabelX, 168, evoLabel, palColor(Pal::INK));
    char evo[12];
    if (hasNextEvo) {
        const uint32_t s = evoRemainMs / 1000u;
        std::snprintf(evo, sizeof(evo), "%u:%02u:%02u", s / 3600u,
                      static_cast<unsigned>((s / 60u) % 60u),
                      static_cast<unsigned>(s % 60u));
    } else {
        std::snprintf(evo, sizeof(evo), "MAX");
    }
    drawText(fb, kActiveW - kMargin - textWidth(evo), 168, evo, palColor(Pal::INK));

    statHintBand(fb, 1, 1);
}

namespace {
// 3 glyphs cover the 5 v1 event types (glyph economy).
const SpriteData* logGlyph(LogEventType t) {
    switch (t) {
        case LogEventType::ItemGained:
        case LogEventType::ItemUsed: return &ASSET_ICON_LOG_EVENT_ITEM;
        case LogEventType::CareMistake: return &ASSET_ICON_LOG_EVENT_WARN;
        case LogEventType::CombatWon:
        case LogEventType::CombatLost: return &ASSET_ICON_LOG_EVENT_COMBAT;
    }
    return &ASSET_ICON_LOG_EVENT_ITEM;
}
} // namespace

void drawAuditLog(Framebuffer& fb, const EventLog& log, int /*beat*/) {
    statHeader(fb, "AUDIT LOG", 7);

    // A full page to itself, so it shows the WHOLE ring newest-first rather than a
    // slice of it: the log holds eight and eight is what a page of this pitch fits, so
    // nothing the device remembered is off the bottom of the one screen that reports it.
    constexpr int kLogRowH = 22;
    const int shown = log.size() < EventLog::kCapacity ? log.size() : EventLog::kCapacity;
    if (shown == 0) {
        // A full page's worth of empty space read as an unfinished screen rather
        // than an empty list — every other empty-state in the app ("- NO PETS -",
        // "- SYSTEM CLEAN -") sits inside a page with other content around it, but
        // this page has nothing else on it. Centered in the space a populated log
        // would fill, with a second line saying what fills it, instead of pinned
        // to the top of a void.
        constexpr int kCenterY = (30 + kProseBottom) / 2 - kFontH;
        drawText(fb, kMargin, kCenterY, "- NO EVENTS YET -", palColor(Pal::INK_DIM));
        drawText(fb, kMargin, kCenterY + 16, "FILLS IN AS YOU PLAY.",
                 palColor(Pal::INK_DIM));
        statHintBand(fb, 1, 1);
        return;
    }
    for (int i = 0; i < shown; ++i) {            // newest first
        const LogEntry& e = log.at(i);
        const int y = 30 + i * kLogRowH;
        drawSprite(fb, *logGlyph(e.type), 0, kMargin, y);
        drawText(fb, kMargin + 16, y + 2, e.text, palColor(Pal::INK));
    }
    statHintBand(fb, 1, 1);
}

// --- The INDEX ---------------------------------------------------------------

namespace {

// One row, formatted. `label` is borrowed; the readout is not.
StatIndexRow indexRow(const char* label, bool sub, int page, int anchor,
                      const char* fmt, ...) {
    StatIndexRow r{};
    r.label = label;
    r.sub = sub;
    r.page = page;
    r.anchor = anchor;
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(r.value, sizeof(r.value), fmt, ap);
    va_end(ap);
    return r;
}

// Rungs held in `rows[from..to)` — the TIERS page's own HELD tag counted back, so the
// index reports exactly what the page under it shows and cannot drift from it.
int rungsHeld(const std::vector<ProseRow>& rows, int from, int to) {
    int n = 0;
    for (int i = from; i < to && i < static_cast<int>(rows.size()); ++i)
        if (!rows[i].header && std::strcmp(rows[i].tag, "HELD") == 0) ++n;
    return n;
}

// Where the section opened by the header at `from` ends (the next header, or the end).
int sectionEnd(const std::vector<ProseRow>& rows, int from) {
    for (int i = from + 1; i < static_cast<int>(rows.size()); ++i)
        if (rows[i].header) return i;
    return static_cast<int>(rows.size());
}

}  // namespace

std::vector<StatIndexRow> buildStatIndexRows(const std::vector<ProseRow>& tierRows,
                                             const std::vector<ProseRow>& loadoutRows,
                                             int level, int movesOn, int moveSlots,
                                             int modsOn, int modSlots, int buffs,
                                             const char* line, int logEvents,
                                             int movesKnown, int movesLearnable,
                                             int foodsEaten, int foodsTotal) {
    std::vector<StatIndexRow> out;

    out.push_back(indexRow("VITALS", false, 0, 0, "LVL %d", level));

    int rungTotal = 0;
    for (const ProseRow& r : tierRows)
        if (!r.header) ++rungTotal;
    out.push_back(indexRow("TIERS", false, 1, 0, "%d/%d",
                           rungsHeld(tierRows, 0, static_cast<int>(tierRows.size())),
                           rungTotal));
    // One sub-row per stat, anchored at that stat's own heading: choosing MAX HEALTH
    // opens TIERS with MAX HEALTH's rungs already on screen, which on a levelled pet is
    // several scroll steps saved and the reason this page exists.
    for (int i = 0; i < static_cast<int>(tierRows.size()); ++i) {
        if (!tierRows[i].header) continue;
        const int end = sectionEnd(tierRows, i);
        out.push_back(indexRow(tierRows[i].label, true, 1, i, "%d/%d",
                               rungsHeld(tierRows, i, end), end - i - 1));
    }

    out.push_back(indexRow("LOADOUT", false, 2, 0, "%d/%d", movesOn + modsOn,
                           moveSlots + modSlots));
    // The loadout's sections come off its own rows too, so an egg — whose page
    // collapses to a single row and carries no headings — offers none of them.
    for (int i = 0; i < static_cast<int>(loadoutRows.size()); ++i) {
        if (!loadoutRows[i].header) continue;
        const bool moves = std::strcmp(loadoutRows[i].label, "MOVES") == 0;
        out.push_back(indexRow(loadoutRows[i].label, true, 2, i, "%d/%d",
                               moves ? movesOn : modsOn,
                               moves ? moveSlots : modSlots));
    }

    // The two COLLECTION pages read as scores, which is the only thing worth saying
    // about them in a list: a row that says 41/195 has already told the reader whether
    // opening it is a plan or a formality.
    out.push_back(indexRow("MOVES", false, 3, 0, "%d/%d", movesKnown, movesLearnable));
    out.push_back(indexRow("FOODS", false, 4, 0, "%d/%d", foodsEaten, foodsTotal));

    out.push_back(buffs > 0 ? indexRow("BUFFS", false, 5, 0, "%d", buffs)
                            : indexRow("BUFFS", false, 5, 0, "NONE"));

    // The line, upper-cased — the same tag the SPECIES page heads itself with.
    char tag[14] = "";
    if (line && line[0]) {
        std::snprintf(tag, sizeof(tag), "%s", line);
        for (char* c = tag; *c; ++c)
            *c = static_cast<char>(std::toupper(static_cast<unsigned char>(*c)));
    }
    out.push_back(indexRow("SPECIES", false, 6, 0, "%s", tag));

    out.push_back(logEvents > 0 ? indexRow("AUDIT LOG", false, 7, 0, "%d", logEvents)
                                : indexRow("AUDIT LOG", false, 7, 0, "EMPTY"));

    return out;
}

void drawStatIndex(Framebuffer& fb, const std::vector<StatIndexRow>& rows, int cursor,
                   int beat) {
    drawHeaderBand(fb, "STAT", "INDEX");

    const int n = static_cast<int>(rows.size());
    // One window, derived from the cursor the way every scrolling list on the device
    // derives its own (layout.h) — so the rows drawn and the thumb below can never
    // disagree about where the list is.
    const int top = listScrollTop(cursor, n, kIndexVisible);
    for (int v = 0; v < kIndexVisible && top + v < n; ++v) {
        const int i = top + v;
        const StatIndexRow& r = rows[i];
        const int y = kIndexRowTop + v * kIndexRowH;
        if (i == cursor) {
            fb.fillRect(4, y - 2, kActiveW - 8, kIndexRowH - 1, palColor(Pal::TRACK));
            drawRowCursor(fb, 6, y, palColor(Pal::ACCENT));
        }
        // A section reads as part of the page above it: dimmer, and indented under it.
        // The indent is the grayscale channel for that nesting — the dimming alone
        // would not survive the gate.
        const Rgb565 ink = r.sub ? palColor(Pal::INK_DIM) : palColor(Pal::INK);
        const int x = r.sub ? kIndexSubX : kIndexLabelX;
        drawLabelValue(fb, x, y, r.label, ink, r.value, palColor(Pal::INK_DIM), beat,
                       i == cursor);
    }

    if (n > kIndexVisible) {
        const int barX = kActiveW - 3;
        const int trackH = kIndexVisible * kIndexRowH;
        fb.fillRect(barX, kIndexRowTop, 2, trackH, palColor(Pal::TRACK));
        const int thumbH = std::max(8, trackH * kIndexVisible / n);
        fb.fillRect(barX, kIndexRowTop + trackH * top / n, 2, thumbH,
                    palColor(Pal::INK_DIM));
    }

    drawHintBand(fb, "A NEXT  B OPEN  C BACK");
}

void drawStatHintBand(Framebuffer& fb, int window, int windows) {
    statHintBand(fb, window, windows);
}

} // namespace mal
