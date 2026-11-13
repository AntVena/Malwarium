#include "core/app/game/arch_screen.h"

#include <cstdio>
#include <vector>

#include "tunables.h"
#include "core/app/operator.h"
#include "core/model/pet.h"
#include "core/ui/arch_screen.h"
#include "core/ui/layout.h"

namespace mal {

namespace {

std::vector<ArchPickRow> pickerRows(const ArchHost& host) {
    const Operator& op = host.operatorRecord();
    return buildArchPickerRows(host.content(), host.petRecord().creature, op.arch.rack,
                               op.arch.records);
}

// Rebuilt on demand rather than cached: the rack changes under this screen (a Store, a
// Deploy, a Release), and a cached list would describe a shelf that no longer exists.
std::vector<ArchRow> groupRows(const ArchHost& host, ArchGroup group) {
    const Operator& op = host.operatorRecord();
    const Pet& pet = host.petRecord();
    return buildArchRows(host.content(), group, pet.creature, pet.generation, op.arch.rack,
                         op.arch.records);
}

// The focused row, or a default one (kind Active, index -1, no def) on an empty group:
// every caller checks `def`, so an empty group reads as nothing to act on.
ArchRow focusedRow(const ArchHost& host, ArchGroup group, int row) {
    const auto rows = groupRows(host, group);
    if (row < 0 || row >= static_cast<int>(rows.size())) return ArchRow{};
    return rows[row];
}

bool rackFull(const ArchHost& host) {
    return static_cast<int>(host.operatorRecord().arch.rack.size()) >= host.rackSlots();
}

}  // namespace

int ArchScreen::pickRowCount(const ArchHost& host) const {
    return static_cast<int>(pickerRows(host).size());
}

int ArchScreen::rowCount(const ArchHost& host) const {
    return static_cast<int>(groupRows(host, group_).size());
}

const char* ArchScreen::focusedPetId(const ArchHost& host) const {
    const ArchRow r = focusedRow(host, group_, row_);
    return r.def ? r.def->id : nullptr;
}

void ArchScreen::stepList(const ArchHost& host, int dir) {
    if (view_ == ArchView::Picker) pickRow_ = stepRow(pickRow_, pickRowCount(host), dir);
    else row_ = stepRow(row_, rowCount(host), dir);
}

void ArchScreen::onButton(ArchHost& host, const ButtonEvent& ev) {
    if (view_ == ArchView::Picker) onPicker(host, ev);
    else onList(host, ev);
}

void ArchScreen::onPicker(ArchHost& host, const ButtonEvent& ev) {
    // A walks, B opens, C backs out: the ITEMS type-picker's gestures, because this is
    // the same kind of screen one level up.
    const auto tiles = pickerRows(host);
    const int n = static_cast<int>(tiles.size());
    if (n <= 0) { if (ev.button == Button::C) host.leaveToCarousel(); return; }
    if (pickRow_ < 0 || pickRow_ >= n) pickRow_ = 0;

    if (ev.button == Button::A) {
        pickRow_ = stepRow(pickRow_, n, +1);
    } else if (ev.button == Button::B) {
        group_ = tiles[pickRow_].group;
        confirm_.close();
        // NEW EGG is an action, so it opens its own L3 confirm rather than a list. An
        // empty group still opens, showing "- NOTHING HERE -", so B never does nothing
        // on a row the screen drew.
        if (group_.kind == ArchGroup::Kind::NewEgg) {
            host.openDetail();
        } else {
            view_ = ArchView::List;
            row_ = 0;
            action_ = group_.kind == ArchGroup::Kind::Active ? ArchAction::Store
                                                             : ArchAction::Deploy;
        }
    } else if (ev.button == Button::C) {
        // With no active pet there is no carousel: the player is between a Store and the
        // next egg, and C returns to line-select rather than out to an empty habitat.
        // Line-select can only have been backed out of with two or more lines, so this
        // cannot lay an egg by surprise.
        if (!host.petRecord().creature && !host.operatorRecord().arch.rack.empty())
            host.reopenLineSelect();
        else
            host.leaveToCarousel();
    }
}

void ArchScreen::onList(ArchHost& host, const ButtonEvent& ev) {
    // Which list a row came from and where it sits in it is on the row itself (ArchRow),
    // so nothing here re-derives a section from a bare cursor.
    const auto rows = groupRows(host, group_);
    const int n = static_cast<int>(rows.size());
    if (ev.button == Button::A) {
        row_ = stepRow(row_, n, +1);
    } else if (ev.button == Button::B) {
        if (n <= 0) return;                       // an empty group has nothing to open
        const ArchRow& r = rows[row_ % n];
        // Records open a read-only page; live pets open the action set, or the SELL sheet
        // when it was the SELL counter that listed them.
        if (r.kind != ArchRow::Kind::Record)
            action_ = r.kind == ArchRow::Kind::Active ? ArchAction::Store : ArchAction::Deploy;
        saleOffer_ = SaleOffer::Bits;
        confirm_.close();
        host.openDetail();
    } else if (ev.button == Button::C) {
        view_ = ArchView::Picker;
    }
}

void ArchScreen::onDetail(ArchHost& host, const ButtonEvent& ev) {
    switch (group_.kind) {
        case ArchGroup::Kind::NewEgg: onNewEgg(host, ev); return;
        case ArchGroup::Kind::Sell:   onSale(host, ev); return;
        default:                      onRecord(host, ev); return;
    }
}

void ArchScreen::onNewEgg(ArchHost& host, const ButtonEvent& ev) {
    if (confirm_.isOpen()) {
        if (confirm_.onButton(ev) == InlineConfirm::Answer::Yes) {
            view_ = ArchView::Picker;
            row_ = 0;
            host.archHatchNewEgg();
        }
        return;
    }
    if (ev.button == Button::B) {
        // Laying an egg sets the active pet aside first, so a full rack blocks it (the
        // page shows the gate).
        if (host.petRecord().creature && rackFull(host)) return;
        confirm_.open();
    } else if (ev.button == Button::C) {
        host.closeDetail();
    }
}

void ArchScreen::onRecord(ArchHost& host, const ButtonEvent& ev) {
    const ArchRow row = focusedRow(host, group_, row_);
    // A RETIRED/CORRUPTED record is read-only, and an empty group has no row to act on.
    if (row.kind == ArchRow::Kind::Record || !row.def) {
        if (ev.button == Button::C) host.closeDetail();
        return;
    }
    // Every answer but YES stays on the record.
    if (confirm_.isOpen()) {
        if (confirm_.onButton(ev) != InlineConfirm::Answer::Yes) return;
        row_ = 0;
        if (action_ == ArchAction::Store) host.archStoreActive();
        else if (action_ == ArchAction::Deploy) host.archDeployStored(row.index);
        else host.archReleaseStored(row.index);
        return;
    }

    if (ev.button == Button::A) {
        // Active has Store alone; a stored pet adds the no-reward Release beside Deploy.
        if (row.kind != ArchRow::Kind::Active)
            action_ = action_ == ArchAction::Deploy ? ArchAction::Release : ArchAction::Deploy;
    } else if (ev.button == Button::B) {
        if (action_ == ArchAction::Store && rackFull(host)) return;   // the page shows the gate
        confirm_.open();
    } else if (ev.button == Button::C) {
        host.closeDetail();
    }
}

void ArchScreen::onSale(ArchHost& host, const ButtonEvent& ev) {
    const ArchRow row = focusedRow(host, group_, row_);
    if (!row.def || row.kind != ArchRow::Kind::Stored) {
        if (ev.button == Button::C) host.closeDetail();
        return;
    }
    if (confirm_.isOpen()) {
        if (confirm_.onButton(ev) == InlineConfirm::Answer::Yes) {
            const SaleOffer offer = saleOffer_;
            saleOffer_ = SaleOffer::Bits;
            row_ = 0;
            host.archSellStored(row.index, offer);
        }
        return;
    }
    if (ev.button == Button::A) {
        // Every offer is on the cycle, open or not: a closed one still says what it would
        // take, which is how a player learns a twin is worth deploying first.
        saleOffer_ = static_cast<SaleOffer>(
            stepRow(static_cast<int>(saleOffer_), kSaleOfferCount, +1));
    } else if (ev.button == Button::B) {
        if (host.archSaleBlocker(saleOffer_, host.operatorRecord().arch.rack[row.index]))
            return;                                                   // the row says why
        confirm_.open();
    } else if (ev.button == Button::C) {
        host.closeDetail();
    }
}

ArchSaleSheet ArchScreen::saleSheet(const ArchHost& host) const {
    ArchSaleSheet sh;
    const auto& rack = host.operatorRecord().arch.rack;
    const ArchRow row = focusedRow(host, group_, row_);
    if (!row.def || row.kind != ArchRow::Kind::Stored || row.index < 0 ||
        row.index >= static_cast<int>(rack.size()))
        return sh;
    const SaveStoredPet& sold = rack[row.index];
    const CreatureDef* active = host.petRecord().creature;
    sh.daemon = row.def;
    sh.level = sold.combatLevel;
    sh.generation = sold.generation;
    for (int i = 0; i < kSaleOfferCount; ++i) {
        const SaleOffer o = static_cast<SaleOffer>(i);
        ArchSaleSheet::Offer& out = sh.offers[i];
        const char* block = host.archSaleBlocker(o, sold);
        out.available = block == nullptr;
        if (block) {
            std::snprintf(out.value, sizeof(out.value), "%s", block);
        } else if (o == SaleOffer::Bits) {
            std::snprintf(out.value, sizeof(out.value), "+%d B", daemonSaleBits(sold.combatLevel));
        } else {
            // A patch's value names the pet that receives it, which is the active pet and
            // never the Daemon being sold.
            std::snprintf(out.value, sizeof(out.value), "FOR %s", active->displayName);
        }
    }
    if (sh.offers[static_cast<int>(SaleOffer::Reimage)].available)
        std::snprintf(sh.reimageTo, sizeof(sh.reimageTo), "%s",
                      host.reimageScript(active)->displayName);
    sh.focus = saleOffer_;
    sh.confirmOpen = confirm_.isOpen();
    sh.confirmChoice = confirm_.choice();
    return sh;
}

void ArchScreen::draw(Framebuffer& fb, const ArchHost& host) const {
    const Operator& op = host.operatorRecord();
    const int used = static_cast<int>(op.arch.rack.size());
    const auto tiles = pickerRows(host);
    if (view_ == ArchView::Picker) {
        drawArchPicker(fb, tiles, pickRow_, used, host.rackSlots());
        return;
    }
    // The group's own name is the list's title, which is the whole reason the picker
    // exists: the header says which shelf this is.
    ArchListView v;
    for (const ArchPickRow& t : tiles)
        if (t.group == group_) { v.title = t.label; break; }
    v.cursor = row_;
    v.used = used;
    v.maxSlots = host.rackSlots();
    v.beat = host.screenBeat();
    if (group_.kind == ArchGroup::Kind::Sell) v.status = ArchStatusColumn::Level;
    drawArchList(fb, groupRows(host, group_), v);
}

void ArchScreen::drawDetail(Framebuffer& fb, const ArchHost& host) const {
    const Operator& op = host.operatorRecord();
    const CreatureDef* active = host.petRecord().creature;
    ArchConfirmView v;
    v.confirmOpen = confirm_.isOpen();
    v.confirmChoice = confirm_.choice();
    // NEW EGG is an action rather than a row, so its L3 is its own screen: the one place
    // that says what laying an egg costs before it is laid.
    if (group_.kind == ArchGroup::Kind::NewEgg) {
        v.pet = active;
        v.rackFull = active && rackFull(host);
        drawArchNewEgg(fb, v);
        return;
    }
    if (group_.kind == ArchGroup::Kind::Sell) {
        drawArchSale(fb, saleSheet(host));
        return;
    }
    const ArchRow row = focusedRow(host, group_, row_);
    if (row.kind == ArchRow::Kind::Record) {
        if (row.index >= 0 && row.index < static_cast<int>(op.arch.records.size()))
            drawArchRecordDetail(fb, host.content(), op.arch.records[row.index]);
        return;
    }
    v.pet = row.def;
    v.isActive = row.kind == ArchRow::Kind::Active;
    v.generation = row.generation;
    v.rackFull = rackFull(host);
    drawArchRecord(fb, action_, v);
}

}  // namespace mal
