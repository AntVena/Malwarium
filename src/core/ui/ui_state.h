// ui_state.h — the UI's own state vocabulary: which submenu, which L3, which
// filter, which mode. What `Game` HOLDS, as opposed to what a screen DRAWS.
//
// These types live here rather than beside the renderers that consume them for one
// structural reason: `Game` stores every one of them as a member, so putting them in
// a screen header makes core/app/game.h include nine renderers to declare a handful
// of enums — and then every TU that includes game.h gets the whole UI layer whether
// it draws or not. The split is by what the type IS: an id naming where the player
// currently is is engine state, and a `draw*` signature is not.
//
// Each renderer still owns its own drawing vocabulary (expl_screen's ExplRowState,
// cfg_screen's CfgRow, items_screen's InvRow) and includes this header for the ids
// its signatures name. Anything only a screen and its caller pass between themselves
// belongs there, not here — this file grows only when `Game` gains a member.
//
// Consumers: core/app/game.h and the game_*.cpp units, plus every core/ui screen
// header whose signatures name one of these.
#pragma once

namespace mal {

// Which submenu an L1 carousel slot routes to on B. The engine dispatches on this
// id; adding a real submenu later is a new case, not a rewire (registration seam).
// The slot table that maps cursor position to one of these is carousel.h's.
enum class SubmenuId { Stat, Items, Games, Expl, Maint, Mods, Arch, Cfg };

// Which page of the MODS submenu is open. MODS is the pet's whole combat LOADOUT:
// the hub (L2) is a three-row menu, and each row hands the same L2/L3 pair to a
// different half of it — the mod slot list, the move slot list, or the Sim-Battle
// tier pick. Practise never rests at the hub level: it opens straight into its L3,
// so `Hub` is where C from any of the three returns to.
enum class LoadoutTab { Hub, Mods, Moves, Practise };

// Hacker-face slot ids. CREW/PROFILE/SHOP/VAULT/MERGE/PEERS are the designed slots;
// SCAN/LINK render the inaccessible marker until designed. PROFILE, CREW, SHOP,
// VAULT, and PEERS are live from the static table; MERGE starts statically
// inaccessible but flips accessible at runtime once mergeHubUnlocked() is bought in
// SHOP (Game::hackerSlotAccessible + drawHackerCarousel's `mergeUnlocked` override —
// the static table alone can't express "locked until purchased").
enum class HackerSlotId { Crew, Profile, Shop, Vault, Merge, Scan, Link, Peers };

// Carousel / label presentation (CFG cycles these).
enum class UiMode { IconsLabel, IconsOnly, TextOnly };

// L3 destinations a CFG row can open. Device and Radio are the two GROUP screens
// (their own children follow them here). ResetHatch is the DEV-only quick reset (an
// immediate action, no L3 screen). FactoryReset is reached only via the hidden
// hold-B gesture, never a list row.
enum class CfgScreen { SysInfo, HackerTag, Titles,
                       Device, UiMode, Brightness, Travel,
                       Radio, Audit, Link, PediaAp, PediaQr,
                       Update, UpdateQr, ResetHatch, FactoryReset };

// The ITEMS list's active tab. Two axes share the field: the base cycle walks the
// TYPE axis (ALL/FOOD/BUFFS/QUEST), and owning the type-picker upgrade widens the
// hold-A gesture to the finer CATEGORY axis (ALL/FOOD/BUFFS/KEYS/TOOLS). See
// items_screen.h's nextItemFilter for the cycle order and which axis is live.
enum class ItemFilter { All, Food, Buffs, Quest, Keys, Tools };

// The two MAINT actions.
enum class MaintKind { Defrag, Av };

// The ARCH record actions, cycled by A in the L3 record. Availability depends on the
// pet: active → Store + Sell; stored → Deploy + Sell. Sell stays Daemon-only
// (deferred until the Daemon stage exists).
enum class ArchAction { Store, Deploy, Sell, Release };

// The pet's vitals as they stood BEFORE the food was applied. The feeding modal
// reports the change that actually happened, which is the only way a pull TOWARD
// 50% Happiness, or a top-up onto an already-full stat, can state its own
// direction — the magnitude on the row doesn't know either. Game::startFeeding
// snapshots this the instant before Game::applyItemEffects.
struct FeedVitals {
    int hunger = 0;
    int frag = 0;
    int happy = 0;
};

}  // namespace mal
