// stat_picker_screen.h: the ROLLBACK and REPARTITION pickers (Nav::RollbackPicker,
// Nav::RepartitionPicker), opened by using either item from ITEMS.
//
// Both are the pet's four earned stat points as a table. ROLLBACK sheds one point (and
// the level it bought); REPARTITION moves one, in two steps: the stat to take it FROM,
// then the stat to move it TO. They are never open together, so they are one screen
// holding one cursor, the source step's pick and the item the commit spends. Nothing
// is spent until the commit, which is the game's (StatPickerHost), since it changes the
// pet.
#pragma once

#include "core/device/platform.h"

namespace mal {

class Framebuffer;
struct ItemDef;

// What the pickers need from the game. Game implements it; a test can too.
class StatPickerHost {
public:
    virtual int levelStatPoint(int stat) const = 0;   // earned points in that stat
    virtual bool statCanGiveUp(int stat) const = 0;   // it has an earned point to lose
    virtual void rollBackStat(const ItemDef& item, int stat) = 0;
    virtual void moveStatPoint(const ItemDef& item, int from, int to) = 0;
    virtual void openDetail() = 0;    // cancelled: back to the item's own page
    virtual void closeDetail() = 0;   // spent: back to the ITEMS list

protected:
    ~StatPickerHost() = default;
};

class StatPickerScreen {
public:
    // Either picker opens parked on the first stat with a point to give up. Using either
    // item already required one (itemUsable), so there always is one.
    void open(const StatPickerHost& host, const ItemDef& item);

    void onRollback(StatPickerHost& host, const ButtonEvent& ev);
    void onRepartition(StatPickerHost& host, const ButtonEvent& ev);
    void drawRollback(Framebuffer& fb, const StatPickerHost& host) const;
    void drawRepartition(Framebuffer& fb, const StatPickerHost& host) const;

private:
    // The next stat (wrapping) with a point to give up; `cur` if it is the only one,
    // -1 if none is.
    int nextGiver(const StatPickerHost& host, int cur) const;
    // The TO step's next row: any stat but the source, since a move onto the stat the
    // point is already on is a no-op the picker should never offer.
    int nextTarget(int cur) const;

    const ItemDef* item_ = nullptr;   // what the commit spends
    int row_ = 0;
    int from_ = -1;                   // REPARTITION's source; -1 while that is the question
};

}  // namespace mal
