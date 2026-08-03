// mods_screen.h — MODS submenu: the equip-slot list (L2) + the mod
// picker (L3). Slot-based hardware equips; mods move freely between slots. Mod
// EFFECTS apply in combat — the equip/inspect UI + the Loadout data model live
// here.
#pragma once

#include <vector>

#include "core/content/defs.h"

namespace mal {

class Framebuffer;
class ContentRegistry;
class Loadout;

// Owned mods in a stable display order (registry order, filtered to owned). The
// picker rows are this list, prefixed by the "— empty (unequip) —" option.
std::vector<const ModDef*> ownedModList(const ContentRegistry& reg,
                                        const Loadout& load);

// L2 equip-slot list: one row per slot, showing the equipped mod + effect tag
// (or "— empty —"). `cursor` is the focused slot.
void drawModsList(Framebuffer& fb, const ContentRegistry& reg,
                  const Loadout& load, int cursor);

// L3 mod picker for `slot`: the available (un-equipped) mods, the focused mod's
// effect text below, and an inline overwrite confirm (D3) when the slot
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
// (niche-flavour pass), shown as LOCKED — WRONG LINE.
void drawModPicker(Framebuffer& fb, const ContentRegistry& reg,
                   const Loadout& load, int slot, int pick, bool confirmActive,
                   int confirmChoice, const char* pendingId, int petLevel,
                   const char* petLine);

// L4 mod detail: the read-then-act inspector reached by selecting a mod
// in the picker — mirrors drawItemDetail. Shows the icon + name, the held spare
// count ("HAVE xN"), the effect TAG (the stat-delta shorthand, in ACCENT), the full
// wrapped effect text, a ONE-SHOT flag when the mod is consumed on trigger, the
// rolled REQUIRES LVL n gate, and the EQUIP action (EQUIPPED if it's already in
// this `slot`; LOCKED if under-level, wrong-line, or already installed in a
// different slot on this pet — a mod holds one slot per pet). `equippedHere`
// drives the action label; `reqLevel` is the best held copy's rolled gate and
// `petLevel` the pet's current level. `petLine` gates a mod carrying
// ModDef::requiresLine the same way (niche-flavour pass). `load` supplies the held
// count and the elsewhere-equipped check.
void drawModDetail(Framebuffer& fb, const ContentRegistry& reg, const Loadout& load,
                   const ModDef& mod, bool equippedHere, int slot,
                   int reqLevel, int petLevel, const char* petLine);

} // namespace mal
