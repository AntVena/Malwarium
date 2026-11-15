// nav.h: the screen layer the game is on (Game::nav_), which every input and draw routes
// by. Its own header so a screen that returns somewhere can name where.
//   Idle/Cursor: resting / a slot focused. The carousel is always drawn at these two.
//   Submenu:     L2 (STAT viewer · ITEMS/MAINT list · placeholder).
//   Detail:      L3 (item detail · MAINT action).
//   Process:     a running MAINT process (non-interruptible).
//   Modal*:      event overlays.
#pragma once

namespace mal {

enum class Nav { Idle, Cursor, Submenu, Detail, Process, ModalFeeding, ModalLockout, ModalLineSelect, ModalEggPick, ModalHatchReveal, ModalEvolve, ModalCSF, Combat, ExploreControl, Encounter, Wifi, Shop, ModShop, WarpPicker, RollbackPicker, RepartitionPicker, CacheYield, BulkYield, PostEncounter, Stacker, Fragger, Isolation, Chroma, Decryption, Cryptogram, ArcadeResult, Tourney, ShiboleetHail, Shiboleet, ShiboleetVerdict, Story, StoryArchive, TipCard, AwayDigest, ItemOffer };

}  // namespace mal
