// train_screen.h — TRAIN submenu. The
// combat-prep menu: TRAIN opens straight to the LOADOUT (L2) — the innate default
// move (read-only) + the evolution-gated equip slots + a Sim-Battle launch row.
// Selecting a slot opens the move picker (L3, mirrors MODS); Sim-Battle opens
// a dummy-tier pick (L3) then launches the combat activity (safe stakes).
#pragma once

#include <vector>

#include "core/content/defs.h"

namespace mal {

class Framebuffer;
class ContentRegistry;
class MoveLoadout;

// Owned moves of `requiredKind` in registry order (excludes the innate default —
// it isn't slottable). Move-slot rework #12: every slot is type-locked to one
// MoveDef::Kind, so the picker/pool never even lists a wrong-kind move. Unless
// `showAll`, this also drops every move the pet can't actually equip into `slot`
// right now: not yet unlocked at `petStage`, wrong-line (defensive — owned() is
// already line-scoped), or already occupying a DIFFERENT slot on this pet. `showAll`
// (the picker's hold-A toggle) restores the full list so a move can still be moved
// between slots or previewed ahead of its unlock.
std::vector<const MoveDef*> ownedMoveList(const ContentRegistry& reg,
                                          const MoveLoadout& load,
                                          MoveDef::Kind requiredKind,
                                          Stage petStage, const char* petLine,
                                          int slot, bool showAll);

// Selectable loadout rows in cursor order: one per UNLOCKED slot, then Sim-Battle.
// (The default row, the LOADOUT divider, and locked slots are shown but skipped.)
// Returns how many selectable rows there are for the pet at `stage`.
int loadoutSelectableCount(Stage stage);

// L2 loadout list. `cursor` is the selectable-row index (0..unlocked-1 = a slot;
// == unlocked = the Sim-Battle row). `stage` gates which slots are unlocked.
// `slotKinds` (#12) is a kMaxMoveSlots array of each slot's stamped required kind
// (only entries < the unlocked count are drawn) — each unlocked slot row shows its
// ATK/DEF tag alongside the slot number.
void drawLoadout(Framebuffer& fb, const ContentRegistry& reg,
                 const MoveLoadout& load, Stage stage, int cursor,
                 const MoveDef::Kind* slotKinds);

// L3 move picker for `slot` (mirrors drawModPicker): "— empty (unequip) —" + the
// moves the pet can equip here right now (ownedMoveList), the focused move's
// spec panel (spec_sheet.h), and the inline overwrite confirm. `showAll` (hold A to
// toggle) reveals the rest too: a move above `petStage` dims with an EVO-{stage} tag,
// and a move already equipped in a different slot dims with a SLOT n tag — neither is
// selectable, mirroring the MODS picker's IN SLOT n gate.
//
// The list is ADAPTIVE: it draws only the rows it has (capped + scrolled at
// kMovePickerMaxRows), so a Process pet's 2-move roster hands its slack to the spec
// panel rather than reserving it empty. B on a move row drills into drawMoveDetail
// below rather than equipping from the list.
void drawMovePicker(Framebuffer& fb, const ContentRegistry& reg,
                    const MoveLoadout& load, int slot, int pick, bool confirmActive,
                    int confirmChoice, const char* pendingId, Stage petStage,
                    const char* petLine, MoveDef::Kind requiredKind, bool showAll);

// L4 move detail — the whole screen for one move, drilled into with B from the picker
// (the same list -> detail -> act shape as ITEMS). The full readout plus prose that
// pages on A instead of being truncated, and the EQUIP action itself — so the picker
// never has to be both a chooser and a reader. `proseScroll` is the first prose line
// to show; the caller advances it (Game::moveProseScroll_) and wraps at
// moveProseLines() below.
void drawMoveDetail(Framebuffer& fb, const ContentRegistry& reg, const MoveDef& m,
                    Stage petStage, bool equippedHere, int proseScroll);

// How many prose lines the detail page shows at once, and how many the move needs —
// the caller's scroll bounds. Returns 0 extra when it all fits (no scroll offered).
int moveDetailProseLines();
int moveProseLines(const MoveDef& m);

// L3 Sim-Battle tier pick: cycle the dummy tier, Start launches combat.
void drawSimTier(Framebuffer& fb, int tier, int tierCount);

} // namespace mal
