// stat_screen.h — STAT: a 5-page status viewer (read-only).
//   page 0  VITALS    the pet: gauges + care + level/XP + time-to-evolve (the landing)
//   page 1  LOADOUT   the pet's equipped moves + mods, WITH their effect text
//   page 2  BUFFS     currently-armed item buffs (Restore Point/Ambig-USB/Backup
//                     Drive), each with its effect text and remaining time if timed
//   page 3  SPECIES   the pet's own lore — line, one-line snarky hint, infosec ref
//   page 4  AUDIT LOG the rolling event history
// A cycles the pages; C backs out. Each page dual-codes (grayscale-legible).
// STAT is pet-only. The device/account stats (Hacker Rank, Bits, lifetime breadth)
// live on the Hacker PROFILE slot (game_hacker.cpp), not here.
#pragma once

#include <cstdint>
#include <vector>

#include "core/content/defs.h"
#include "core/content/effect_text.h"
#include "core/model/event_log.h"
#include "core/model/pet_model.h"

namespace mal {

class Framebuffer;
class ContentRegistry;
class MoveLoadout;
class Loadout;

// LOADOUT page row model (mirrors InvRow/the items-screen pattern for
// testability): a non-selectable section header (MOVES/MODS), an equipped
// move/mod with its effect text, or the page's own empty-state line.
struct LoadoutRow {
    bool header;        // section header row (no effect text, drawn dim)
    bool isDefault;      // true only for the innate default-move row (adds a tag)
    const char* label;   // header text / move-or-mod displayName / empty-state text
    // The move/mod's description with its own magnitudes substituted in
    // (effect_text.h). OWNED, not borrowed: the text is built from the row rather
    // than living in flash beside it. Empty on header/empty-state rows.
    EffectText effect;
};

// Build the LOADOUT page's rows: a MOVES section (the innate default move,
// tagged, followed by each equipped — unlocked-slot — move) then a MODS section
// (each equipped mod, or a single "- NONE -" row when nothing's equipped).
// EMPTY slots are skipped entirely — nothing to describe. `isEgg` collapses the
// whole page to a single "- NO LOADOUT -" row (TRAIN/MODS are inert for an egg,
// `Game::eggSlotLocked`), since there is no combat loadout to show.
std::vector<LoadoutRow> buildLoadoutRows(const ContentRegistry& reg,
                                         const MoveLoadout& moveLoad,
                                         const Loadout& modLoad,
                                         Stage stage, bool isEgg);

// Visible row window (mirrors drawMovePicker's kMovePickerVisibleRows,
// train_screen.cpp) — exported so the engine can compute wrap/clamp bounds for
// the B-scroll gesture without re-deriving the layout constant.
constexpr int kLoadoutVisibleRows = 5;

// STAT page 1 — LOADOUT: the windowed row list from buildLoadoutRows, each row's
// name + wrapped effect text, a scrollbar + "B - SCROLL" hint band when the list
// overflows the visible window (mirrors drawMovePicker's windowing).
// `scrollTop` is engine-owned — this page has no cursor to drive scrolling off
// (it's read-only), so STAT's B press advances it instead (game_core.cpp).
void drawLoadoutScreen(Framebuffer& fb, const std::vector<LoadoutRow>& rows,
                       int scrollTop, int beat);

// STAT page 0 — VITALS: name + generation + stage indicator, the three vitals
// gauges (Hunger / Fragmentation / Happiness) with tabular numerics, the creature
// level + an XP bar toward the next level, the care-pip budget, and the
// time-to-next-evolution readout. `generation` reads in the name row.
// `beat` supplies the Critical-pulse phase. `level` is the per-pet creature level
// `combatXp`/`xpToNext` fill the XP bar (banked XP / next-level cost).
// `hasNextEvo` false = a terminus (shows MAX); `evoRemainMs` = ms to the next
// boundary (the hatch clock for an egg — labelled HATCH there, EVOLVE otherwise).
void drawStatScreen(Framebuffer& fb, const PetModel& m, const char* name,
                    Stage stage, int generation, int level, int combatXp,
                    int xpToNext, int beat, bool hasNextEvo, uint32_t evoRemainMs);

// STAT page 2 — BUFFS row model: one armed item buff, its effect text (rendered
// from the item's own ItemDef row — single source of truth, no duplicated
// description), and a countdown for the timed ones (Backup Drive).
struct BuffRow {
    const char* label;     // the item's displayName
    EffectText effect;     // the item's description, its magnitudes substituted in
    bool hasTimer;         // true only for a time-limited buff (Backup Drive)
    uint32_t remainingMs;  // valid only when hasTimer
};

// Build the BUFFS page's rows from the armed-buff state Game tracks as plain
// per-pet flags (game.h) — Restore Point's mistake shield, Ambig-USB's forced
// Trojan divert, Backup Drive's timed combat shield, and the two DeepWeb Dive
// depth buffs (Deep-Learning Module/Core's win multiplier, a Bell's armed start
// depth). Each armed flag becomes one row naming the ITEM that armed it —
// resolved by scanning the registry for the item whose effect matches the raw
// state, so the display and the item's own 'Pedia/detail text never drift
// apart even if the item roster is renamed or rebalanced later.
// depthMultiplier: Game::deepWebDepthMultiplier_ (1 = none armed).
// startDepthArmed/startDepthUsesBest/startDepthValue: resolved from
// Game::pendingDeepWebStartDepth_ (armed = != -1; usesBest = the
// kDeepWebStartDepthUseBest sentinel, in which case startDepthValue is unused).
std::vector<BuffRow> buildBuffRows(const ContentRegistry& reg,
                                    bool restorePointArmed,
                                    bool trojanDivertArmed,
                                    bool backupDriveArmed,
                                    uint32_t backupDriveRemainMs,
                                    int depthMultiplier,
                                    bool startDepthArmed,
                                    bool startDepthUsesBest,
                                    int startDepthValue);

// STAT page 2 — BUFFS: the armed-buff list from buildBuffRows, each with its
// effect text wrapped below the name (and a remaining-time readout for the
// timed ones). Empty list shows a plain "no active buffs" line.
void drawBuffsScreen(Framebuffer& fb, const std::vector<BuffRow>& rows, int beat);

// STAT page 3 — SPECIES: the pet's own lore, straight off its CreatureDef row
// (`hint`/`context`, defs.h) — the game owns this copy, same as an item's
// `effect` text. `line` is the raw line id (e.g. "ransomware"), upper-cased
// for display; `hint` is the one-line snarky read, `context` the real
// infosec reference behind the pun. Either may be null (no lore authored).
void drawSpeciesScreen(Framebuffer& fb, const char* name, const char* line,
                       const char* hint, const char* context, int beat);

// STAT page 4 — AUDIT LOG: the rolling event history newest-first, each
// a type glyph + terse text. The lifetime/uptime footer lives on the Hacker
// PROFILE slot, so this page is the log alone and shows more rows.
void drawAuditLog(Framebuffer& fb, const EventLog& log, int beat);

} // namespace mal
