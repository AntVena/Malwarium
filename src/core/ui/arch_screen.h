// arch_screen.h — ARCH submenu: the server rack. Cold storage for the device's pets, the
// per-pet actions (Deploy / Store / Release), the NEW EGG row that lays one, and the SELL
// counter that trades a finished Daemon in.
//
// THREE SCREENS, not one list. The shelf reaches 64 slots (game_rig_shop.h's
// kRackSlotUpgradeMax) — one of every species on the roster with room over — and the
// RETIRED/CORRUPTED record tail only ever grows, so a single flat list is a walk rather
// than a menu. So: an L2 GROUP PICKER (NEW EGG · ACTIVE · one row per creature family ·
// SELL · RECORDS), the rows behind whichever group it opened, and the L3 that acts on one
// of them. Grouping by FAMILY is the axis because it is the one a player keeping a set
// thinks in, and because it is the only grouping the content already declares
// (CreatureDef::line).
//
// SELL is a group rather than a pet action because its L3 is a different screen: what a
// sale can pay depends on the ACTIVE pet (the two patches spend the Daemon on it), so the
// sheet is read across two pets where a record only ever describes one.
#pragma once

#include <vector>

#include "core/content/defs.h"
#include "core/model/save.h"
#include "core/ui/ui_state.h"  // ArchAction, ArchGroup

namespace mal {

class Framebuffer;
class ContentRegistry;

// One row of the ARCH picker (L2): a group, its label, and how many rows are behind it.
// A fixed set, built fresh each draw — NEW EGG, ACTIVE, one row per creature line, then
// SELL and RECORDS — so a family added to kCreatureLines shows up here without an edit.
struct ArchPickRow {
    ArchGroup group;
    // Held BY VALUE, not as a pointer: a family's label is derived from its id rather
    // than authored (CreatureLine carries no display name), so there is no string in the
    // content tables to point at and a shared scratch buffer would be one more thing
    // whose lifetime a caller has to know about. Sized to kCarouselLabelMaxChars' order
    // of magnitude — the longest label on it is METAMORPHIC.
    char label[20];
    int count;          // rows behind this group (the NEW EGG row carries 0)
};

// One row of the ARCH list (L2b), under whichever group the picker opened. `index` is
// into the rack (Stored) or the record list (Record) — the ROW knows which list it came
// from and where, so nothing downstream has to re-derive a section from a bare cursor
// the way the old flat list did.
struct ArchRow {
    enum class Kind : uint8_t { Active, Stored, Record };
    Kind kind = Kind::Active;
    int index = -1;
    const CreatureDef* def = nullptr;   // resolved species, or null for an unknown id
    int generation = 0;
    uint8_t status = 0;                 // Kind::Record only — RecordStatus
    int level = 0;                      // Kind::Stored only — what the SELL list prices on
};

// The SELL counter's L3, read off the Game in one piece (Game::archSaleSheet). Each offer
// carries whether it can be taken and the value its row shows — a payout when it can, the
// reason in a word or two when it cannot, so a dimmed row still says why in grayscale.
struct ArchSaleSheet {
    const CreatureDef* daemon = nullptr;
    int level = 0;
    int generation = 0;
    struct Offer {
        bool available = false;
        char value[20] = {0};
    } offers[kSaleOfferCount];
    // The Script an active twin goes back to; empty when Reimage is closed.
    char reimageTo[20] = {0};
    SaleOffer focus = SaleOffer::Bits;
    bool confirmOpen = false;
    int confirmChoice = 0;              // 0 Cancel · 1 Confirm
};

// The picker's rows. `active` may be null (the two-room state between a Store and the
// egg that follows it), in which case the ACTIVE row is drawn as empty rather than
// dropped — a missing row would move every row under it between one draw and the next.
std::vector<ArchPickRow> buildArchPickerRows(const ContentRegistry& reg,
                                             const CreatureDef* active,
                                             const std::vector<SaveStoredPet>& rack,
                                             const std::vector<SaveRecord>& records);

// The rows behind one group. Empty for Kind::NewEgg, which is an action rather than a
// list, and for a group whose shelf happens to be empty. Kind::Sell holds every STORED
// Daemon whatever its family — never the active pet, which has to be set aside first.
std::vector<ArchRow> buildArchRows(const ContentRegistry& reg, ArchGroup group,
                                   const CreatureDef* active, int generation,
                                   const std::vector<SaveStoredPet>& rack,
                                   const std::vector<SaveRecord>& records);

// L2 group picker. `maxSlots` is the rack capacity (kRackSlots + any Containment Rack
// Slot purchases) and `used` how much of it is spent, so the header carries the same
// SLOTS n/N the list does.
void drawArchPicker(Framebuffer& fb, const std::vector<ArchPickRow>& tiles, int cursor,
                    int used, int maxSlots);

// L2b rack list, showing one group's rows. `cursor` is the focused row; `title` names
// the group. A group can outgrow the screen on its own now that the shelf holds 64, so
// the list still draws a kVisibleRows window with the slim scrollbar items/mods/cfg use.
// `showLevels` puts each stored pet's level where FROZEN would go — the SELL list's
// status column, since level is what a sale is priced on.
void drawArchList(Framebuffer& fb, const std::vector<ArchRow>& rows, const char* title,
                  int cursor, int used, int maxSlots, bool showLevels = false);

// L3 NEW EGG confirm. The one screen that says what laying an egg COSTS: the pet you
// are raising goes to the rack first, so a full rack is what blocks it. `active` null
// means there is no pet to set aside and the egg is laid outright.
void drawArchNewEgg(Framebuffer& fb, const CreatureDef* active, bool rackFull,
                    bool confirmOpen, int confirmChoice);

// L3 record view for a RETIRED/CORRUPTED entry: read-only — final /
// last-known identity, no actions (the pet is gone). Greyed to match the list.
void drawArchRecordDetail(Framebuffer& fb, const ContentRegistry& reg,
                          const SaveRecord& rec);

// L3 pet record: details + the available action + a light inline
// confirm. `isActive` picks the Store-vs-Deploy action set; `generation`
// is shown in the record; `rackFull` gates Store.
// When `confirmOpen`, the confirm prompt is drawn (`confirmChoice` 0=Cancel/1=OK).
void drawArchRecord(Framebuffer& fb, const CreatureDef* pet, bool isActive,
                    int generation, ArchAction action, bool rackFull, bool confirmOpen,
                    int confirmChoice);

// L3 SELL sheet: one stored Daemon and the three things it can be traded for.
void drawArchSale(Framebuffer& fb, const ArchSaleSheet& sheet);

} // namespace mal
