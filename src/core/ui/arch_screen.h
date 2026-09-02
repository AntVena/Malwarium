// arch_screen.h — ARCH submenu: the server rack. Cold storage for the device's pets, the
// per-pet actions (Deploy / Store / Sell / Release), and the NEW EGG row that lays one.
//
// THREE SCREENS, not one list. The shelf reaches 64 slots (game_rig_shop.h's
// kRackSlotUpgradeMax) — one of every species on the roster with room over — and the
// RETIRED/CORRUPTED record tail only ever grows, so a single flat list is a walk rather
// than a menu. So: an L2 GROUP PICKER (NEW EGG · ACTIVE · one row per creature family ·
// RECORDS), the rows behind whichever group it opened, and the L3 that acts on one of
// them. Grouping by FAMILY is the axis because it is the one a player keeping a set
// thinks in, and because it is the only grouping the content already declares
// (CreatureDef::line).
//
// Sell stays Daemon-only and stubbed until its system lands.
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
// RECORDS — so a family added to kCreatureLines shows up here without an edit.
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
};

// The picker's rows. `active` may be null (the two-room state between a Store and the
// egg that follows it), in which case the ACTIVE row is drawn as empty rather than
// dropped — a missing row would move every row under it between one draw and the next.
std::vector<ArchPickRow> buildArchPickerRows(const ContentRegistry& reg,
                                             const CreatureDef* active,
                                             const std::vector<SaveStoredPet>& rack,
                                             const std::vector<SaveRecord>& records);

// The rows behind one group. Empty for Kind::NewEgg, which is an action rather than a
// list, and for a group whose shelf happens to be empty.
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
void drawArchList(Framebuffer& fb, const std::vector<ArchRow>& rows, const char* title,
                  int cursor, int used, int maxSlots);

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
// is shown in the record; `rackFull` gates Store; `sellEnabled` is Daemon-only.
// When `confirmOpen`, the confirm prompt is drawn (`confirmChoice` 0=Cancel/1=OK).
void drawArchRecord(Framebuffer& fb, const CreatureDef* pet, bool isActive,
                    int generation, ArchAction action, bool sellEnabled,
                    bool rackFull, bool confirmOpen, int confirmChoice);

} // namespace mal
