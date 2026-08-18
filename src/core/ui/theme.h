// theme.h — where a game concept becomes a colour.
//
// The palette has two layers under this one: `Pal` names the ROLES a colour can
// play (core/render/palette.h), and the generated table gives each role a value
// per theme. This file is the third and last: it maps a DOMAIN value — an item's
// rarity, a crew's side — onto the role that carries it.
//
// It exists so that mapping lives exactly once. A screen that needs "what colour
// is Epic" asks here; it never repeats the switch, and it never reaches for a
// `Pal::RARITY_*` token directly, because then adding a fifth rarity would mean
// finding every screen that had an opinion about the fourth.
//
// So the whole colour story is three edits deep, and each is a different kind of
// change: retint a role -> assets/PAL_CORE.json · add a whole theme -> a themes
// block in that same file · change what a concept MEANS -> this file. None of
// them is a sweep through the screens.
//
// THE TINTING RULE. Drawing something in a colour is decoration, never
// information: a tint may only ever repeat a meaning the same screen already
// carries some other way — a word, a count, a shape. It must never be the only
// channel, or the screen fails the grayscale gate. The ITEMS row tints its icon
// by rarity AND prints the rarity word beside it; the Sealed Cache tiers are
// countable chevrons before they are colours. Both survive being desaturated,
// which is the test.
#pragma once

#include "core/content/defs.h"
#include "core/render/palette.h"
#include "core/ui/combat_screen.h"

namespace mal {

inline Rgb565 rarityColor(ItemDef::Rarity r) {
    switch (r) {
        case ItemDef::Rarity::Common: return palColor(Pal::RARITY_COMMON);
        case ItemDef::Rarity::Uncommon: return palColor(Pal::RARITY_UNCOMMON);
        case ItemDef::Rarity::Rare: return palColor(Pal::RARITY_RARE);
        case ItemDef::Rarity::Epic: return palColor(Pal::RARITY_EPIC);
    }
    return palColor(Pal::RARITY_COMMON);
}

// The Red/Blue axis: Operators vs. Guardians. Every surface that shows a side —
// a crew name, a duel banner, a team glyph — comes through here.
inline Rgb565 teamColor(bool red) {
    return palColor(red ? Pal::TEAM_RED : Pal::TEAM_BLUE);
}

// The combat VS grid's rows. Every one of these is a token that ALREADY carries this
// meaning somewhere on this screen or in the palette's own notes, which is why the set
// adds no colour of its own: the team pair is authored as offense/defense and dual-coded
// with a sword and a shield (PAL_CORE's `team` block), the shield-pool bar on the combat
// screen already draws in TEAM_BLUE and the ransom strip in CALM.
//
// Hues repeat across the set on purpose — speed and a stun are both WARN, a wall and a
// bubble are both TEAM_BLUE. The glyph is what separates them, per the tinting rule
// above: colour may only repeat a meaning the shape already carries.
inline Rgb565 combatVsColor(CombatVsKind k) {
    switch (k) {
        case CombatVsKind::Health:  return palColor(Pal::CALM);
        case CombatVsKind::Power:   return palColor(Pal::TEAM_RED);
        case CombatVsKind::Defense:
        case CombatVsKind::Guard:
        case CombatVsKind::Shield:  return palColor(Pal::TEAM_BLUE);
        case CombatVsKind::Speed:
        case CombatVsKind::Stun:    return palColor(Pal::WARN);
        case CombatVsKind::Dot:     return palColor(Pal::HOT);
        case CombatVsKind::Ransom:  return palColor(Pal::CALM);
        case CombatVsKind::Backup:
        case CombatVsKind::Trap:
        case CombatVsKind::Copy:    return palColor(Pal::INK_DIM);
    }
    return palColor(Pal::INK_DIM);
}

} // namespace mal
