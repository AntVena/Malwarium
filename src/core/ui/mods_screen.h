// mods_screen.h — MODS submenu: the LOADOUT hub (L2) that fronts it, the
// equip-slot list, and the mod picker. Slot-based hardware equips; mods move freely
// between slots. Mod EFFECTS apply in combat — the equip/inspect UI + the Loadout
// data model live here.
//
// MODS is the pet's whole combat loadout, so the hub's other two rows open screens
// that live elsewhere: MOVES is train_screen.h's slot list and PRACTISE is its
// Sim-Battle tier pick. Only the hub itself is drawn here.
#pragma once

#include <vector>

#include "core/content/defs.h"

namespace mal {

class Framebuffer;
class ContentRegistry;
class Loadout;
class MoveLoadout;

// Hub rows, in cursor order — MODS, MOVES, PRACTISE (LoadoutTab's non-Hub values,
// offset by one). The Game wraps its cursor on this.
inline constexpr int kLoadoutHubRows = 3;

// L2 LOADOUT hub: MODS / MOVES / PRACTISE, one row each with its own filled-vs-total
// readout (PRACTISE has nothing to count and states its stakes instead). `cursor` is
// the focused row in that order; `stage` is what decides how many move slots the
// MOVES row is counting against.
void drawLoadoutHub(Framebuffer& fb, const Loadout& load, const MoveLoadout& moves,
                    Stage stage, int cursor, int beat);

// Owned mods in picker order: the ones that can go in `slot` RIGHT NOW first, then the
// ones that can't, each half in registry order. Nothing is hidden — a mod the pet has
// not levelled into is the reason to keep levelling, and dropping it from the list
// would drop the only place that reason is ever stated — but it stops sitting between
// the player and the mods they can actually fit, which on a grown pool is most of them.
// `petLevel` / `petLine` / `slot` are the same three gates the rows draw with
// (drawModPicker), so the order can never disagree with the LOCKED tag beside it.
//
// The Game's picker cursor indexes THIS list (Game::onModPicker), so both callers must
// pass the same three arguments — an order the cursor and the rows disagree about is a
// mod equipped by pointing at a different one.
std::vector<const ModDef*> ownedModList(const ContentRegistry& reg, const Loadout& load,
                                        int petLevel, const char* petLine, int slot);

// True if `m` is blocked from `slot` on this pet right now — its rolled equip level is
// above `petLevel`, it wants a line the pet doesn't carry, or the pet already has it in
// a different slot. The one answer the ordering above, the row's dim + reason tag, and
// the picker's verdict line all read, so a mod can never sort as available and then
// draw as LOCKED.
bool modLockedFor(const ModDef& m, int petLevel, const char* petLine,
                  const Loadout& load, int slot);

// L2 equip-slot list: one row per slot, showing the equipped mod + effect tag
// (or "— empty —"). `cursor` is the focused slot; `beat` scrolls the focused row's name
// when it is too long to sit beside its tag.
void drawModsList(Framebuffer& fb, const ContentRegistry& reg,
                  const Loadout& load, int cursor, int beat);

// L3 mod picker for `slot`: the available (un-equipped) mods, the focused mod's
// effect text below, and an inline overwrite confirm when the slot
// already holds a different mod — the "discards {current} — permanent" warning,
// since mods are permanent (no unequip; equipping consumes the mod). `pick` is the
// focused picker row (0-based into the available mods). The list is windowed —
// capped to a fixed visible-row count and scrolled to follow `pick` (mirrors
// drawMovePicker, train_screen.cpp) — so a large held-mod pool never overruns the
// description zone below. `confirmActive` overlays the Cancel/Confirm prompt;
// `confirmChoice` (0 Cancel, 1 Confirm) is the focused choice; `pendingId` is the
// mod being equipped. `petLevel` gates the rolled equip-LEVEL requirement
// a mod under the pet's level shows LOCKED — NEEDS LVL n (a number
// channel, grayscale-safe), and equip is blocked in the Game. `petLine` is the active
// pet's CreatureDef::line (or nullptr) — gates a mod carrying ModDef::requiresLine
// (niche-flavour pass), shown as LOCKED — WRONG LINE. Rows are ordered fittable-first
// (ownedModList), so the mods that can go in this slot are the ones the cursor reaches
// first. `beat` drives the focused row's name marquee — a mod name too long to sit
// beside its right-hand tag scrolls within the room it has instead of printing through
// it, and only the focused row travels.
void drawModPicker(Framebuffer& fb, const ContentRegistry& reg,
                   const Loadout& load, int slot, int pick, bool confirmActive,
                   int confirmChoice, const char* pendingId, int petLevel,
                   const char* petLine, int beat);

// L4 mod detail: the read-then-act inspector reached by selecting a mod
// in the picker — mirrors drawItemDetail. Shows the icon + name, the held spare
// count against its storage cap ("HAVE 2/2"), the effect TAG (the stat-delta shorthand,
// in ACCENT), the full wrapped effect text, a ONE-SHOT flag when the mod is consumed on
// trigger, the REQUIRES LVL n gate, and the EQUIP action (EQUIPPED if it's already in
// this `slot`; LOCKED if under-level, wrong-line, or already installed in a
// different slot on this pet — a mod holds one slot per pet). `equippedHere`
// drives the action label; `reqLevel` is the mod's own gate (modEquipLevel — every copy
// shares it) and `petLevel` the pet's current level. `petLine` gates a mod carrying
// ModDef::requiresLine the same way (niche-flavour pass). `load` supplies the held
// count and the elsewhere-equipped check; `storageCap` is what that count is measured
// against (Game::modStorageCap), so a full pool reads as full rather than as a number
// with no ceiling.
// Prose lines the MODS detail panel actually holds. Its floor grows with the readout,
// so this is a constant whatever the grid costs — but it is the reserve MINUS the gap
// the grid leaves behind it, which is not the reserve. Published so the content tests
// hold a mod's description to what the screen will really draw.
int modDetailProseLines();

void drawModDetail(Framebuffer& fb, const ContentRegistry& reg, const Loadout& load,
                   const ModDef& mod, bool equippedHere, int slot,
                   int reqLevel, int petLevel, const char* petLine, int storageCap,
                   int beat);

} // namespace mal
