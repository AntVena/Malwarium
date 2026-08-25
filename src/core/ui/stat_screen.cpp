#include "core/ui/stat_screen.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

#include "core/content/effect_text.h"
#include "core/content/registry.h"
#include "core/model/loadout.h"
#include "core/model/move_loadout.h"
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
constexpr int kPageCount = 5;

// STAT header: title at left, a dot pager at right (one dot per page, kPageCount
// of them) with the active page filled. The pager is the non-colour channel for
// "which page" (dot position), so the status pages stay distinguishable in grayscale.
void statHeader(Framebuffer& fb, const char* title, int page) {
    drawHeaderBand(fb, title);
    for (int i = 0; i < kPageCount; ++i) {
        const int x = kActiveW - 8 - (kPageCount - i) * 8;
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

// BUFFS keeps its own heights because a buff row is a name plus a COUNTDOWN rather
// than a name plus a tag — the timer is live state, re-read every repaint, where a
// ProseRow's tag is set once when the row is built.
std::vector<int> buffRowHeights(const std::vector<BuffRow>& rows) {
    std::vector<int> h;
    h.reserve(rows.size());
    for (const BuffRow& r : rows)
        h.push_back(kFontH + (r.effect.empty()
                                  ? 0
                                  : kProseNameGap +
                                        textWrapLines(r.effect.c_str(), kProseW) *
                                            kProseLineH) +
                    kProseRowGap);
    return h;
}

// BUFFS' own fit count, over its own heights — the shared proseRowsFitting takes
// ProseRows, and this page's rows are not those.
int buffFitCount(const std::vector<int>& heights, int top) {
    int y = kBuffRowTop;
    int n = 0;
    for (int i = top; i < static_cast<int>(heights.size()); ++i) {
        if (n > 0 && y + heights[i] > kProseBottom) break;
        y += heights[i];
        ++n;
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
    drawHintBand(fb, "B SCROLL");
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

int loadoutRowsFitting(const std::vector<ProseRow>& rows, int top) {
    return proseRowsFitting(rows, top, kLoadoutRowTop);
}

void drawLoadoutScreen(Framebuffer& fb, const std::vector<ProseRow>& rows,
                       int scrollTop, int beat) {
    statHeader(fb, "LOADOUT", 1);
    drawProseRows(fb, rows, scrollTop, kLoadoutRowTop, beat);
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
                                    const PetUpgrades& upgrades) {
    std::vector<BuffRow> out;
    auto add = [&](const char* itemId, bool armed, bool timed, uint32_t remain) {
        if (!armed) return;
        const ItemDef* d = reg.item(itemId);
        out.push_back({d ? d->displayName : itemId, d ? effectText(*d) : EffectText{},
                       timed, remain});
    };
    add("restore_point", restorePointArmed, false, 0);
    add("ambig_usb", trojanDivertArmed, false, 0);
    add("backup_drive", backupDriveArmed, true, backupDriveRemainMs);
    // The two DeepWeb Dive depth buffs: which ITEM armed them isn't tracked
    // directly (Game only keeps the raw multiplier/depth), so find the item whose
    // effect matches the current state instead of hardcoding an id here.
    auto addByEffect = [&](ItemEffect::Kind kind, int magnitude, bool matchAnyMagnitude) {
        for (const ItemDef* d : reg.allItems())
            for (const ItemEffect& e : d->effects)
                if (e.kind == kind && (matchAnyMagnitude || e.magnitude == magnitude)) {
                    out.push_back({d->displayName, effectText(*d), false, 0});
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
    // Last, because these are the rows that never lapse: everything an Epic dish has
    // permanently given this pet. Found by effect like the dive buffs above rather than
    // by id, so a dish that grants one can be renamed without touching this file — and
    // in stat order, so a pet carrying several reads as one block rather than in
    // whichever order it happened to be fed.
    if (upgrades.bandwidthRegenMin > 0)
        addByEffect(ItemEffect::Kind::BandwidthRegenBonusMin, 0, true);
    static const ItemEffect::Kind kStatGrants[kLevelStatCount] = {
        ItemEffect::Kind::StatPointPower, ItemEffect::Kind::StatPointDefense,
        ItemEffect::Kind::StatPointSpeed, ItemEffect::Kind::StatPointHealth};
    for (int i = 0; i < kLevelStatCount; ++i)
        if (upgrades.statBonus[i] > 0) addByEffect(kStatGrants[i], 0, true);
    if (upgrades.xpRatePct > 0)
        addByEffect(ItemEffect::Kind::XpRateBonusPct, 0, true);
    return out;
}

int buffRowsFitting(const std::vector<BuffRow>& rows, int top) {
    return buffFitCount(buffRowHeights(rows), top);
}

void drawBuffsScreen(Framebuffer& fb, const std::vector<BuffRow>& rows, int scrollTop,
                     int /*beat*/) {
    statHeader(fb, "BUFFS", 2);

    if (rows.empty()) {
        drawText(fb, kMargin, 60, "- NO ACTIVE BUFFS -", palColor(Pal::INK_DIM));
        return;
    }

    const std::vector<int> heights = buffRowHeights(rows);
    const int total = static_cast<int>(rows.size());
    const bool overflow = buffFitCount(heights, 0) < total;
    const int top = overflow ? std::max(0, std::min(scrollTop, total - 1)) : 0;
    const int shown = buffFitCount(heights, top);

    int y = kBuffRowTop;
    for (int v = 0; v < shown; ++v) {
        const BuffRow& r = rows[top + v];
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
}

void drawSpeciesScreen(Framebuffer& fb, const char* name, const char* line,
                       const char* hint, const char* context, int /*beat*/) {
    statHeader(fb, "SPECIES", 3);

    drawText(fb, kMargin, 30, name ? name : "", palColor(Pal::INK));
    if (line && line[0]) {
        char tag[24];
        std::snprintf(tag, sizeof(tag), "%s LINE", line);
        for (char* p = tag; *p; ++p)
            *p = static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
        drawText(fb, kActiveW - kMargin - textWidth(tag), 30, tag, palColor(Pal::INK_DIM));
    }

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
    const int hintRoom = kActiveH - kMargin - kSpeciesTop - kBlockGap -
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
}

void drawStatScreen(Framebuffer& fb, const PetModel& m, const char* name,
                    Stage stage, int generation, int level, int combatXp,
                    int xpToNext, int beat, bool hasNextEvo, uint32_t evoRemainMs) {
    const bool pulseOn = ((beat / 2) & 1) == 0;   // ~1Hz
    statHeader(fb, "STAT", 0);

    // Name · generation · stage.
    drawText(fb, kMargin, kRowTop, name, palColor(Pal::INK));
    const char* st = stageName(stage);
    const int stX = kActiveW - kMargin - textWidth(st);
    drawText(fb, stX, kRowTop, st, palColor(Pal::ACCENT));
    char gen[12];
    std::snprintf(gen, sizeof(gen), "GEN %d", generation);
    drawText(fb, stX - 6 - textWidth(gen), kRowTop, gen, palColor(Pal::INK_DIM));

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
    statHeader(fb, "AUDIT LOG", 4);

    // A full page to itself, so it shows a real slice rather than a teaser: the last
    // 6 events, newest-first (the ring keeps 8).
    constexpr int kShown = 6;
    const int shown = log.size() < kShown ? log.size() : kShown;
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
        return;
    }
    for (int i = 0; i < shown; ++i) {            // newest first
        const LogEntry& e = log.at(i);
        const int y = 30 + i * 24;
        drawSprite(fb, *logGlyph(e.type), 0, kMargin, y);
        drawText(fb, kMargin + 16, y + 2, e.text, palColor(Pal::INK));
    }
}

} // namespace mal
