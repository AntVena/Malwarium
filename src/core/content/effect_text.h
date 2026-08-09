// effect_text.h — a content row's description, rendered from that row's own numbers.
//
// A description that spells out "reflects 10 damage" has to be re-edited by hand every
// time the magnitude next to it moves, and nothing catches it when nobody does. So the
// `effect` string on ItemDef/ModDef/MoveDef is a TEMPLATE over its own row: `{token}`
// names one of that row's fields, and effectText() substitutes the live value.
//
//     {"tripwire", "Tripwire", "THORNS",
//      "Below {mag2}% Health, reflects {mag} damage to any attacker.", ...
//      ModEffect::ConditionalThorns, /*magnitude=*/10, /*magnitude2=*/40, ...},
//
// Retune the magnitude and the sentence follows. `{|token|}` renders the absolute
// value, for prose that carries the sign itself ("Drains {|hunger|} Hunger").
//
// specRows() is the other half, and needs no authoring at all: it derives a row's
// structured effects (ItemEffect kinds / ModEffect / a move's riders) into a compact
// label+value readout, so a magnitude reaches the player even when the prose never
// mentions it. The detail/picker screens lay it out as an aligned grid above the prose
// (ui/spec_sheet.h); the 'Pedia takes statLine()'s one-line join as its own field.
//
// Consumers: ui/spec_sheet.h (the shared name/readout/prose panel every detail and
// picker screen draws), stat_screen (LOADOUT/BUFFS rows), game_explore (shop rows),
// tools/dump_content.cpp (the 'Pedia's data feed).
//
// Adding a token: extend the per-type table in effect_text.cpp — and for an item,
// give the new ItemEffect::Kind a name in itemEffectToken() alongside its applier
// case in Game::applyItemEffects. A row naming a token that doesn't exist leaves the
// braces in the output, which the native gate asserts against.
#pragma once

#include "core/content/content_achievements.h"
#include "core/content/content_crews.h"
#include "core/content/defs.h"

namespace mal {

// One rendered description. A value type (not a borrowed pointer) because the text is
// built on demand from the row rather than living in flash next to it — the row model
// a screen builds owns its string outright.
//
// `buf` is also the de-facto AUTHORING CAP on a description: expand() fills it and
// stops, so prose past kMaxProse characters is silently lost. The native gate
// (test_effect_text_fits_its_screen_budget) fails on a row that reaches the cap rather
// than letting the tail disappear — so this is a real limit to write to, not a
// buffer that happens to be big enough today.
struct EffectText {
    static constexpr int kMaxProse = 191;   // sizeof(buf) - 1
    char buf[kMaxProse + 1] = {0};
    const char* c_str() const { return buf; }
    bool empty() const { return buf[0] == '\0'; }
    bool atCap() const { return buf[kMaxProse - 1] != '\0'; }
};

// The row's `effect` prose with every {token} replaced by that row's own value.
EffectText effectText(const ItemDef& d);
EffectText effectText(const ModDef& d);
EffectText effectText(const MoveDef& d);
// A crew Exploit's `desc` prose (content_crews.h). One token, `{mag}` — the Exploit's
// whole vocabulary is one magnitude, and a kind that meters nothing simply never
// writes it.
EffectText effectText(const CrewExploitDef& d);
// An achievement's `trigger` prose. `effectiveGoal` is the threshold the row is actually
// held to — for a kGoalAll row that is the size of the set it counts over, which only the
// app layer can resolve (app/game_achievements.h's achievementGoal). Pass it in rather
// than letting this file reach up for it.
EffectText effectText(const AchievementDef& d, int effectiveGoal);

// One entry of the derived readout, kept as label + value rather than one baked
// string so a screen can lay the set out as an ALIGNED GRID (drawSpecGrid) — labels
// left, magnitudes right-aligned in their column, which is what makes two moves
// comparable at a glance. `value` empty marks a FLAG (a fixed behaviour with no
// magnitude, e.g. "TROJAN DIVERT"), which the grid gives a full-width row.
//
// Labels are deliberately terse: a grid column is half of 208px, so label + value
// has to clear ~17 characters to pair up with a sibling instead of taking a whole
// row to itself.
struct SpecRow {
    char label[24] = {0};   // fits the widest flag ("SURVIVES 1 FATAL HIT")
    // Wide enough for a rig row's "NOW -> NEXT" pair at its deepest tier
    // ("+1020% -> +1024%"), which is the longest value any producer builds.
    char value[24] = {0};
    bool flag() const { return value[0] == '\0'; }
};

// Room for the widest row in the tables (a Trojan trap and a Phishing siphon both
// report 5) with headroom for a new rider or two.
struct SpecRows {
    SpecRow rows[12];
    int count = 0;
};

// Every magnitude the row hands the engine, in display order. The single source of
// the readout vocabulary — statLine() below is just this joined with " / ", so the
// grid and the one-line form can never disagree about what a row reports.
SpecRows specRows(const ItemDef& d);
SpecRows specRows(const ModDef& d);
SpecRows specRows(const MoveDef& d);

// Assembles a SpecRows. Public because the content tables aren't the only producer of
// a readout — the Rig Shop's account upgrades build theirs the same way
// (app/game_rig_shop.h's rigSpec), so they render through the same grid instead of
// hand-formatting into a fixed buffer. `add` takes a terse label plus a printf'd
// value; `flag` takes a label alone, for a behaviour with no magnitude to report.
// Anything past SpecRows' capacity is ignored rather than written off the end.
struct SpecBuilder {
    SpecRows out;
    void add(const char* label, const char* fmt, ...)
        __attribute__((format(printf, 3, 4)));
    void flag(const char* label);
};

// specRows() joined into one " / "-separated line ("HUNGER +40 / HEAL 30"), for the
// places that want a string rather than a laid-out grid — the 'Pedia's data feed and
// the STAT LOADOUT/BUFFS list rows. Empty when a row has no structured effect to
// report (a pure-flavour quest item).
EffectText statLine(const ItemDef& d);
EffectText statLine(const ModDef& d);
EffectText statLine(const MoveDef& d);

// The token an item's effect magnitude answers to in a description template, e.g.
// Kind::Hunger -> "hunger" for "{hunger}". nullptr for Kind::None.
const char* itemEffectToken(ItemEffect::Kind k);

}  // namespace mal
