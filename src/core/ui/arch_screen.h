// arch_screen.h — ARCH submenu: the server rack. Cold storage for the
// device's pets + the active-pet actions (Store / Deploy / Sell).
// The list + record SHELL: multi-pet rack storage ties to persistence and Sell
// is Daemon-only, so those rows are stubbed (·soon· / gated) until their systems
// land. Only the single active pet is shown today.
#pragma once

#include <vector>

#include "core/content/defs.h"
#include "core/model/save.h"
#include "core/ui/ui_state.h"  // ArchAction

namespace mal {

class Framebuffer;
class ContentRegistry;

// L2 rack list. Row 0 is the active pet; the next rows are the frozen
// rack entries; the trailing rows are RETIRED/CORRUPTED records (greyed, no slot —
// they don't count toward `SLOTS n/N`). `cursor` is the focused row. `maxSlots` is
// the rack capacity (kRackSlots + any Containment Rack Slot purchases).
//
// The three sections share one row space, and it outgrows the screen: capacity is
// purchasable and records only accumulate. The list draws a kVisibleRows window
// that follows `cursor`, with the slim scrollbar items/mods/cfg use.
void drawArchList(Framebuffer& fb, const ContentRegistry& reg,
                  const CreatureDef* active, const std::vector<SaveStoredPet>& rack,
                  const std::vector<SaveRecord>& records, int cursor, int maxSlots);

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
