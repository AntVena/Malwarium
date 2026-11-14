#include "core/ui/stat_screen.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

#include "core/model/event_log.h"
#include "core/model/loadout.h"
#include "core/model/move_loadout.h"
#include "core/model/pet.h"
#include "core/model/pet_upgrades.h"
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

// STAT header: title at left, a dot pager at right (one dot per page) with the active
// page filled. The pager is the non-colour channel for "which page" (dot position), so
// the status pages stay distinguishable in grayscale.
void statHeader(Framebuffer& fb, const char* title, StatPager pager) {
    drawHeaderBand(fb, title);
    for (int i = 0; i < pager.count; ++i) {
        const int x = kActiveW - 8 - (pager.count - i) * 8;
        fb.fillRect(x, 8, 4, 4, i == pager.index ? palColor(Pal::INK) : palColor(Pal::INK_DIM));
    }
}

void vitalsRow(Framebuffer& fb, int y, const char* label, int value, Zone zone,
               GaugeInk ink, PulsePhase pulse, int beat) {
    drawText(fb, kLabelX, y + 2, label, palColor(Pal::INK));
    drawGauge(fb, kGaugeX, y, kGaugeW, kGaugeH, value, zone, ink, pulse, beat);
    char num[8];
    std::snprintf(num, sizeof(num), "%3d", value);
    // Critical numerics tint hot too (dual-coded with the gauge).
    Rgb565 nc = (zone == Zone::Critical) ? palColor(Pal::HOT) : palColor(Pal::INK);
    drawText(fb, kNumX, y + 2, num, nc);
}

// The band every STAT page ends with. Two things belong on it and they are not the
// same kind of fact: what the key under the reader's thumb does right now, and that
// the INDEX exists at all. The second is the whole reason the band is drawn even on a
// page with nothing to scroll: a hold gesture nothing names is a gesture nobody finds.
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

namespace {

// The BOND page's grid. Three source rows under the total, each a name line, a bar
// and the action that fills it.
constexpr int kBondTotalY = 28;
constexpr int kBondRowTop = 68;
constexpr int kBondRowPitch = 44;
constexpr int kBondBarDy = 12;
constexpr int kBondBarH = 10;
constexpr int kBondHowDy = 27;
constexpr int kBondNotches = 10;   // one notch per +10%, the vitals gauges' cell count

void formatBondPct(char* out, size_t n, int bp) {
    std::snprintf(out, n, "+%d.%02d%%", bp / 100, bp % 100);
}

// A continuous fill notched every tenth: fine enough to show one meal's +1% move it,
// and segmented like the vitals gauges so a glance reads it as so many cells.
void drawBondBar(Framebuffer& fb, int y, int bp) {
    const int x = kMargin;
    const int w = kActiveW - 2 * kMargin;
    drawProgressBar(fb, x, y, w, kBondBarH, static_cast<float>(bp) / kBondSourceCapBp,
                    palColor(Pal::ACCENT));
    const int innerW = w - 2;
    for (int i = 1; i < kBondNotches; ++i) {
        const int nx = x + 1 + innerW * i / kBondNotches;
        const bool filled = bp * kBondNotches >= kBondSourceCapBp * i;
        fb.fillRect(nx, y + 1, 1, kBondBarH - 2,
                    palColor(filled ? Pal::PAPER : Pal::TRACK));
    }
}

void drawBondRow(Framebuffer& fb, int y, const char* name, const char* how, int bp) {
    drawText(fb, kLabelX, y, name, palColor(Pal::INK));
    char val[16];
    if (bp >= kBondSourceCapBp) std::snprintf(val, sizeof(val), "MAX");
    else formatBondPct(val, sizeof(val), bp);
    drawText(fb, kActiveW - kMargin - textWidth(val), y, val,
             palColor(bp > 0 ? Pal::ACCENT : Pal::INK_DIM));
    drawBondBar(fb, y + kBondBarDy, bp);
    drawText(fb, kLabelX, y + kBondHowDy, how, palColor(Pal::INK_DIM));
}

// STAT BOND: the XP bonus Bond adds, then one bar per source filling toward its cap,
// each with the action that fills it.
void drawBondScreen(Framebuffer& fb, const PetUpgrades& upgrades, StatPager pager) {
    statHeader(fb, "BOND", pager);

    char total[16];
    formatBondPct(total, sizeof(total), bondTotalBp(upgrades));
    drawText(fb, kLabelX, kBondTotalY + 4, "XP BONUS", palColor(Pal::INK_DIM));
    drawText(fb, kActiveW - kMargin - textWidth(total, 2), kBondTotalY, total,
             palColor(Pal::ACCENT), 2);
    drawText(fb, kLabelX, kBondTotalY + 20, "ON EVERY XP AWARD", palColor(Pal::INK_DIM));

    struct Source { BondSource src; const char* name; const char* how; };
    static const Source kRows[kBondSources] = {
        {BondSource::Food, "MEALS", "+1% A MEAL, IF NOT FULL"},
        {BondSource::Games, "GAMES", "+0.5% AN ARCADE GAME"},
        {BondSource::Adventure, "FIGHTS", "+0.01% PER XP WON"},
    };
    for (int i = 0; i < kBondSources; ++i)
        drawBondRow(fb, kBondRowTop + i * kBondRowPitch, kRows[i].name, kRows[i].how,
                    upgrades.bondBp[static_cast<int>(kRows[i].src)]);

    statHintBand(fb, 1, 1);
}

// STAT SPECIES: the pet's own lore, straight off its CreatureDef row (`hint`/`context`,
// defs.h). `hint` is the snarky read, `context` the real infosec reference behind the
// pun, set dim a paragraph below it as an unlabelled footnote. Either may be null (no
// lore authored). Both are wrapped to as many lines as they need: `context` is measured
// first and held back, and `hint` takes the room that leaves, so a Daemon's long read is
// shown whole instead of stopping mid-sentence with the foot of the screen still empty.
void drawSpeciesScreen(Framebuffer& fb, const CreatureDef& c, StatPager pager, int beat) {
    const char* name = c.displayName;
    const char* line = c.line;
    const char* hint = c.hint;
    const char* context = c.context;
    statHeader(fb, "SPECIES", pager);

    // Name and line tag are both content, and the widest pair of them wants more than
    // the line has: a twelve-letter creature beside METAMORPHIC LINE fills it exactly.
    // The tag owns the right end (it is the fact this header adds that the rest of the
    // page doesn't), and the name yields to it and scrolls.
    char tag[24];
    tag[0] = '\0';
    if (line && line[0]) {
        // The line, not "<line> LINE": on a page headed SPECIES the word adds nothing
        // and cost 40px that several creatures need. A twelve-letter name beside
        // "METAMORPHIC LINE" overran the row, and the name is what the page is about.
        std::snprintf(tag, sizeof(tag), "%s", line);
        for (char* p = tag; *p; ++p)
            *p = static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
    }
    drawLabelValue(fb, kMargin, 30, name ? name : "", palColor(Pal::INK), tag,
                   palColor(Pal::INK_DIM), beat, Overflow::Scroll);

    // Tighter than the grid's kLineH: SPECIES stacks two wrapped prose blocks in one
    // screen, and the leading is what has to give.
    constexpr int kSpeciesLineH = kFontH + 3;
    constexpr int kSpeciesTop = 50;
    // A paragraph break, not a section: the second block carries no heading of its
    // own, so the gap and the dimmer ink are the whole of what separates them,
    // which is enough, because it reads as a footnote to the block above rather than
    // as a different KIND of thing. It also leaves the foot of the page free for
    // whatever the page grows next.
    constexpr int kBlockGap = 14;
    // The two blocks are a creature's own authored lore, and the roster's are not
    // the same length: a Daemon's read runs to nine wrapped lines where a
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

// Care mistakes, labelled ERRORS, with the path they currently lead to spelled out
// beside the pips: the pips show how many, and what a first-time player cannot read off
// them is what the divider means. The word is the channel; its colour only grades it.
void drawCareRow(Framebuffer& fb, int y, const PetModel& m, PulsePhase pulse) {
    drawText(fb, kLabelX, y, "ERRORS", palColor(Pal::INK));
    drawCarePips(fb, kGaugeX, y - 2, m.careMistakes(), pulse);
    const CareBranch branch = m.careBranch();
    const char* path = branch == CareBranch::Dying ? "DYING"
                     : branch == CareBranch::Bad   ? "BAD"
                                                   : "GOOD";
    const Rgb565 pc = branch == CareBranch::Dying ? palColor(Pal::HOT)
                    : branch == CareBranch::Bad   ? palColor(Pal::WARN)
                                                  : palColor(Pal::CALM);
    drawText(fb, kActiveW - kMargin - textWidth(path), y, path, pc);
}

// Time-to-next-evolution (all stages). An egg counts down to its HATCH; a mid-chain pet
// to its next EVOLVE; a Daemon terminus reads MAX (no successor). Ink text readout,
// grayscale-safe with no colour channel needed.
void drawEvolveRow(Framebuffer& fb, int y, const StatScreenView& v) {
    const char* evoLabel = (v.stage == Stage::BootSector) ? "HATCH" : "EVOLVE";
    drawText(fb, kLabelX, y, evoLabel, palColor(Pal::INK));
    char evo[12];
    if (v.hasNextEvo) {
        const uint32_t s = v.evoRemainMs / 1000u;
        std::snprintf(evo, sizeof(evo), "%u:%02u:%02u", s / 3600u,
                      static_cast<unsigned>((s / 60u) % 60u),
                      static_cast<unsigned>(s % 60u));
    } else {
        std::snprintf(evo, sizeof(evo), "MAX");
    }
    drawText(fb, kActiveW - kMargin - textWidth(evo), y, evo, palColor(Pal::INK));
}

}  // namespace

void drawStatScreen(Framebuffer& fb, const PetModel& m, const StatScreenView& v) {
    const PulsePhase pulse = ((v.beat / 2) & 1) == 0 ? PulsePhase::Bright   // ~1Hz
                                                      : PulsePhase::Dim;
    statHeader(fb, "STAT", v.pager);

    // Name · generation. The stage is NOT here: it is spelled out under its own node on
    // the indicator below, which is where the reader is already looking to see how far
    // along the pet is. Naming it twice cost the header 94px, and a pet's name is
    // eleven or twelve characters often enough that the row would cut it (the first
    // Ransomware egg hatches as CryptoShell, into exactly that).
    char gen[12];
    std::snprintf(gen, sizeof(gen), "GEN %d", v.generation);
    const int genX = kActiveW - kMargin - textWidth(gen);
    drawText(fb, genX, kRowTop, gen, palColor(Pal::INK_DIM));
    drawTextMarquee(fb, kMargin, kRowTop, genX - kMargin - kMargin, v.name,
                    palColor(Pal::INK), v.beat, Overflow::Scroll);

    drawStageIndicator(fb, kMargin, 40, 130, v.stage);
    // Creature level: LVL n beside the stage bar, text-only, grayscale-safe.
    char lvl[12];
    std::snprintf(lvl, sizeof(lvl), "LVL %d", v.level);
    drawText(fb, kActiveW - kMargin - textWidth(lvl), 44, lvl, palColor(Pal::ACCENT));

    // XP toward the next level: a slim progress bar under the stage row,
    // banked-XP / next-level-cost. Fill level is the grayscale channel; the numeric
    // readout spells out the exact threshold.
    const float xt = v.xpToNext > 0 ? static_cast<float>(v.combatXp) / v.xpToNext : 0.f;
    drawText(fb, kLabelX, 60, "XP", palColor(Pal::INK_DIM));
    drawProgressBar(fb, 30, 59, 118, 7, xt, palColor(Pal::ACCENT));
    char xp[16];
    std::snprintf(xp, sizeof(xp), "%d/%d", v.combatXp, v.xpToNext);
    drawText(fb, kActiveW - kMargin - textWidth(xp), 60, xp, palColor(Pal::INK_DIM));

    // Three vitals gauges. The first is the Hunger stat, labelled FED because the
    // number counts how full the pet is: "HUNGER 80" reads as starving to anyone who
    // has not been told otherwise, and FED and HAPPY both read more-is-better.
    vitalsRow(fb, 74, "FED", m.hunger(), m.hungerZone(), GaugeInk::Zone, pulse, v.beat);
    vitalsRow(fb, 96, "FRAG", m.fragmentation(), m.fragZone(), GaugeInk::FragRamp, pulse,
              v.beat);
    vitalsRow(fb, 118, "HAPPY", m.happiness(), m.happyZone(), GaugeInk::Zone, pulse, v.beat);

    drawCareRow(fb, 146, m, pulse);
    drawEvolveRow(fb, 168, v);
    // The board has no real-time clock, so every timer and every drain on this page
    // runs only while the device is on. A player expecting a pet that lives through the
    // night needs telling that switched off, it waits, in both directions.
    drawText(fb, kLabelX, 186, "TIME RUNS ONLY WHILE ON", palColor(Pal::INK_DIM));

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

// STAT AUDIT LOG: the rolling event history newest-first, each a type glyph + terse
// text. The lifetime/uptime footer lives on the Hacker PROFILE slot, so this page is the
// log alone and shows the whole ring.
void drawAuditLog(Framebuffer& fb, const EventLog& log, StatPager pager) {
    statHeader(fb, "AUDIT LOG", pager);

    // A full page to itself, so it shows the WHOLE ring newest-first rather than a
    // slice of it: the log holds eight and eight is what a page of this pitch fits, so
    // nothing the device remembered is off the bottom of the one screen that reports it.
    constexpr int kLogRowH = 22;
    const int shown = log.size() < EventLog::kCapacity ? log.size() : EventLog::kCapacity;
    if (shown == 0) {
        // A full page's worth of empty space read as an unfinished screen rather
        // than an empty list: every other empty-state in the app ("- NO PETS -",
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

} // namespace

void drawStatHintBand(Framebuffer& fb, int window, int windows) {
    statHintBand(fb, window, windows);
}

// --- The pages ---------------------------------------------------------------

namespace {

Stage petStage(const Pet& pet) {
    return pet.creature ? pet.creature->stage : Stage::BootSector;
}

class VitalsPage final : public StatPage {
public:
    const char* title() const override { return "VITALS"; }
    void draw(Framebuffer& fb, const StatSources& s, StatPager pager, int /*scrollTop*/,
              int beat) const override {
        StatScreenView v;
        v.name = s.pet.creature->displayName;
        v.stage = s.pet.creature->stage;
        v.generation = s.pet.generation;
        v.level = s.pet.level;
        v.combatXp = s.pet.xp;
        v.xpToNext = s.xpToNext;
        v.hasNextEvo = s.hasNextEvo;
        v.evoRemainMs = s.evoRemainMs;
        v.beat = beat;
        v.pager = pager;
        drawStatScreen(fb, s.pet.model, v);
    }
    void indexRows(const StatSources& s, std::vector<StatIndexRow>& out) const override {
        out.push_back(pageIndexRow(*this, "LVL %d", s.pet.level));
    }
};

class BondPage final : public StatPage {
public:
    const char* title() const override { return "BOND"; }
    void draw(Framebuffer& fb, const StatSources& s, StatPager pager, int /*scrollTop*/,
              int /*beat*/) const override {
        drawBondScreen(fb, s.pet.upgrades, pager);
    }
    void indexRows(const StatSources& s, std::vector<StatIndexRow>& out) const override {
        out.push_back(pageIndexRow(*this, "+%d%%", bondTotalBp(s.pet.upgrades) / 100));
    }
};

// TIERS, LOADOUT and BUFFS: name + prose rows from `rows`, flowed down from `rowTop` (the
// flow itself is core/ui/prose_page.h), B stepping a window at a time. The draw and the
// window advance both measure the window off the same rows at the same row top, which is
// what keeps B from skipping or repeating a row. `index` lists the page's INDEX rows off
// those rows; `emptyText` stands in for an empty page (null on a page that always has
// rows).
class ProseStatPage final : public StatPage {
public:
    using Rows = std::vector<ProseRow> (*)(const StatSources&);
    using Index = void (*)(const StatPage&, const StatSources&, const std::vector<ProseRow>&,
                           std::vector<StatIndexRow>&);
    constexpr ProseStatPage(const char* title, Rows rows, int rowTop, Index index,
                            const char* emptyText)
        : title_(title), rows_(rows), rowTop_(rowTop), index_(index), emptyText_(emptyText) {}

    const char* title() const override { return title_; }
    void draw(Framebuffer& fb, const StatSources& s, StatPager pager, int scrollTop,
              int beat) const override {
        const std::vector<ProseRow> rows = rows_(s);
        statHeader(fb, title_, pager);
        if (rows.empty() && emptyText_) {
            drawText(fb, kMargin, 60, emptyText_, palColor(Pal::INK_DIM));
            statHintBand(fb, 1, 1);
            return;
        }
        // The flow draws no band of its own here (nullptr): every STAT page ends with the
        // same one, and it says more than the flow can know to say.
        drawProseRows(fb, {rows, scrollTop, rowTop_, beat, nullptr});
        statProseHint(fb, rows, scrollTop, rowTop_);
    }
    int nextScroll(const StatSources& s, int scrollTop) const override {
        const std::vector<ProseRow> rows = rows_(s);
        return nextWindowTop(scrollTop, proseRowsFitting(rows, scrollTop, rowTop_),
                             static_cast<int>(rows.size()));
    }
    void indexRows(const StatSources& s, std::vector<StatIndexRow>& out) const override {
        index_(*this, s, rows_(s), out);
    }

private:
    const char* title_;
    Rows rows_;
    int rowTop_;
    Index index_;
    const char* emptyText_;
};

// Where the section opened by the header at `from` ends (the next header, or the end).
int sectionEnd(const std::vector<ProseRow>& rows, int from) {
    for (int i = from + 1; i < static_cast<int>(rows.size()); ++i)
        if (isHeading(rows[i].kind)) return i;
    return static_cast<int>(rows.size());
}

std::vector<ProseRow> tierRows(const StatSources& s) {
    // TOTAL points, not earned ones: an Epic dish's off-level grant counts toward a rung
    // exactly as an earned point does (applyLevelStatPoints resolves the tiers from the
    // same sum), so the page has to read the stat the way the fight does.
    int points[kLevelStatCount];
    for (int i = 0; i < kLevelStatCount; ++i) points[i] = s.pet.totalStatPoint(i);
    return buildTierRows(points);
}

// Rungs held in `rows[from..to)`: the TIERS page's own HELD tag counted back, so the
// index reports exactly what the page under it shows and cannot drift from it.
int rungsHeld(const std::vector<ProseRow>& rows, int from, int to) {
    int n = 0;
    for (int i = from; i < to && i < static_cast<int>(rows.size()); ++i)
        if (!isHeading(rows[i].kind) && std::strcmp(rows[i].tag, "HELD") == 0) ++n;
    return n;
}

void tierIndex(const StatPage& page, const StatSources& /*s*/,
               const std::vector<ProseRow>& rows, std::vector<StatIndexRow>& out) {
    int rungTotal = 0;
    for (const ProseRow& r : rows)
        if (!isHeading(r.kind)) ++rungTotal;
    out.push_back(pageIndexRow(page, "%d/%d",
                               rungsHeld(rows, 0, static_cast<int>(rows.size())), rungTotal));
    // One sub-row per stat, anchored at that stat's own heading: choosing MAX HEALTH
    // opens TIERS with MAX HEALTH's rungs already on screen, which on a levelled pet is
    // several scroll steps saved and the reason this page exists.
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        if (!isHeading(rows[i].kind)) continue;
        const int end = sectionEnd(rows, i);
        out.push_back(sectionIndexRow(page, rows[i].label, i, "%d/%d",
                                      rungsHeld(rows, i, end), end - i - 1));
    }
}

std::vector<ProseRow> loadoutRows(const StatSources& s) {
    return buildLoadoutRows(s.reg, s.pet.moves, s.pet.mods, petStage(s.pet),
                            s.pet.inEggPhase());
}

void loadoutIndex(const StatPage& page, const StatSources& s,
                  const std::vector<ProseRow>& rows, std::vector<StatIndexRow>& out) {
    // Counted the way the LOADOUT hub counts them (drawLoadoutHub, core/ui/mods_screen.h):
    // equipped over the slots this stage has actually unlocked, so "4/4" on the index
    // means the same thing it means there.
    const int moveSlots = MoveLoadout::slotsForStage(petStage(s.pet));
    int movesOn = 0;
    for (int i = 0; i < moveSlots; ++i)
        if (s.pet.moves.equipped(i)) ++movesOn;
    int modsOn = 0;
    for (int i = 0; i < kModSlots; ++i)
        if (s.pet.mods.equipped(i)) ++modsOn;

    out.push_back(pageIndexRow(page, "%d/%d", movesOn + modsOn, moveSlots + kModSlots));
    // The sections come off the page's own rows, so an egg (whose page collapses to a
    // single row and carries no headings) offers none of them.
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        if (!isHeading(rows[i].kind)) continue;
        const bool moves = std::strcmp(rows[i].label, "MOVES") == 0;
        out.push_back(sectionIndexRow(page, rows[i].label, i, "%d/%d",
                                      moves ? movesOn : modsOn,
                                      moves ? moveSlots : kModSlots));
    }
}

std::vector<ProseRow> buffRows(const StatSources& s) {
    return buildBuffRows(s.reg, s.pet, s.backupRemainMs);
}

void buffIndex(const StatPage& page, const StatSources& /*s*/,
               const std::vector<ProseRow>& rows, std::vector<StatIndexRow>& out) {
    // The count is BUFFS, not its rows: the page's headings are signage and counting
    // them would tell the reader they have two more buffs than they do.
    int armed = 0;
    for (const ProseRow& b : rows)
        if (!isHeading(b.kind)) ++armed;
    out.push_back(armed > 0 ? pageIndexRow(page, "%d", armed) : pageIndexRow(page, "NONE"));
}

class SpeciesPage final : public StatPage {
public:
    const char* title() const override { return "SPECIES"; }
    void draw(Framebuffer& fb, const StatSources& s, StatPager pager, int /*scrollTop*/,
              int beat) const override {
        drawSpeciesScreen(fb, *s.pet.creature, pager, beat);
    }
    void indexRows(const StatSources& s, std::vector<StatIndexRow>& out) const override {
        // The line, upper-cased: the same tag the SPECIES page heads itself with.
        char tag[14] = "";
        const char* line = s.pet.creature ? s.pet.creature->line : nullptr;
        if (line && line[0]) {
            std::snprintf(tag, sizeof(tag), "%s", line);
            for (char* p = tag; *p; ++p)
                *p = static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
        }
        out.push_back(pageIndexRow(*this, "%s", tag));
    }
};

class LogPage final : public StatPage {
public:
    const char* title() const override { return "AUDIT LOG"; }
    void draw(Framebuffer& fb, const StatSources& s, StatPager pager, int /*scrollTop*/,
              int /*beat*/) const override {
        drawAuditLog(fb, s.log, pager);
    }
    void indexRows(const StatSources& s, std::vector<StatIndexRow>& out) const override {
        out.push_back(s.log.size() > 0 ? pageIndexRow(*this, "%d", s.log.size())
                                       : pageIndexRow(*this, "EMPTY"));
    }
};

// The flowed pages' own row tops. TIERS opens on a section heading, same as LOADOUT.
constexpr int kTierRowTop = 26;
constexpr int kLoadoutRowTop = 26;
constexpr int kBuffRowTop = 28;

const VitalsPage kVitals;
const BondPage kBond;
const ProseStatPage kTiers{"TIERS", tierRows, kTierRowTop, tierIndex, nullptr};
const ProseStatPage kLoadout{"LOADOUT", loadoutRows, kLoadoutRowTop, loadoutIndex, nullptr};
const ProseStatPage kBuffs{"BUFFS", buffRows, kBuffRowTop, buffIndex, "- NO ACTIVE BUFFS -"};
const SpeciesPage kSpecies;
const LogPage kLog;

}  // namespace

const StatPage& kStatVitalsPage = kVitals;
const StatPage& kStatBondPage = kBond;
const StatPage& kStatTiersPage = kTiers;
const StatPage& kStatLoadoutPage = kLoadout;
const StatPage& kStatBuffsPage = kBuffs;
const StatPage& kStatSpeciesPage = kSpecies;
const StatPage& kStatLogPage = kLog;

} // namespace mal
