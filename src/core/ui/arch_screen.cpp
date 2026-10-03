#include "core/ui/arch_screen.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/content/registry.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/layout.h"
#include "core/ui/widgets.h"
#include "generated/assets.h"

namespace mal {

namespace {

// One rack row: slot glyph + name + stage caption + a right-aligned status.
// `slotIcon` distinguishes a live slot from a record's spent one, so the row's
// meaning survives in grayscale even where the status word is clipped.
void rackRow(Framebuffer& fb, int y, bool focused, const SpriteData& slotIcon,
             const char* name, const char* stage, const char* status,
             Rgb565 statusColor) {
    if (focused) {
        fb.fillRect(4, y + 2, kActiveW - 8, kRowH - 4, palColor(Pal::TRACK));
        drawRowCursor(fb, 8, y + (kRowH - 7) / 2, palColor(Pal::ACCENT));
    }
    drawSprite(fb, slotIcon, 0, 16, y + (kRowH - kRowIcon) / 2);
    drawText(fb, 40, y + (kRowH - kFontH) / 2, name, palColor(Pal::INK));
    drawText(fb, 40, y + kRowH - kFontH, stage, palColor(Pal::INK_DIM));
    drawText(fb, kActiveW - kMargin - textWidth(status), y + (kRowH - kFontH) / 2,
             status, statusColor);
}

} // namespace

const char* recordStatusTag(const SaveRecord& rec) {
    return static_cast<RecordStatus>(rec.status) == RecordStatus::Corrupted
               ? "CORRUPTED" : "RETIRED";
}

namespace {

// A creature line's display form is its id in capitals — the same thing STAT's SPECIES
// page does with it, and for the same reason: CreatureLine carries no display name and
// does not need one while the id reads as a word.
void lineLabel(const CreatureLine& l, char* out, std::size_t n) {
    std::snprintf(out, n, "%s", l.id ? l.id : "?");
    for (char* p = out; *p; ++p)
        *p = static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
}

// A picker row with a fixed label, spelled once so the three that have one read the
// same as the families that derive theirs.
ArchPickRow pickRow(ArchGroup group, const char* label, int count) {
    ArchPickRow r{group, {}, count};
    std::snprintf(r.label, sizeof(r.label), "%s", label);
    return r;
}

} // namespace

std::vector<ArchPickRow> buildArchPickerRows(const ContentRegistry& reg,
                                             const CreatureDef* active,
                                             const std::vector<SaveStoredPet>& rack,
                                             const std::vector<SaveRecord>& records) {
    std::vector<ArchPickRow> out;
    // NEW EGG leads, because it is the thing people come to ARCH to do — a first row
    // here, rather than two screens down inside the active pet's own record.
    out.push_back(pickRow({ArchGroup::Kind::NewEgg, -1}, "NEW EGG", 0));
    out.push_back(pickRow({ArchGroup::Kind::Active, -1}, "ACTIVE", active ? 1 : 0));

    // One row per family, in kCreatureLines order, counting what is on the shelf. Empty
    // families keep their row (dimmed by the draw, like an empty ITEMS category): a
    // player keeping one of everything reads the gaps as the work left to do.
    const auto lines = reg.allCreatureLines();
    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        int n = 0;
        for (const SaveStoredPet& p : rack) {
            const CreatureDef* c = reg.creature(p.id);
            if (c && c->line && lines[i]->id && std::strcmp(c->line, lines[i]->id) == 0) ++n;
        }
        ArchPickRow row{{ArchGroup::Kind::Line, i}, {}, n};
        lineLabel(*lines[i], row.label, sizeof(row.label));
        out.push_back(row);
    }
    int daemons = 0;
    for (const SaveStoredPet& p : rack) {
        const CreatureDef* c = reg.creature(p.id);
        if (c && c->stage == Stage::Daemon) ++daemons;
    }
    out.push_back(pickRow({ArchGroup::Kind::Sell, -1}, "SELL", daemons));
    out.push_back(pickRow({ArchGroup::Kind::Records, -1}, "RECORDS",
                          static_cast<int>(records.size())));
    return out;
}

std::vector<ArchRow> buildArchRows(const ContentRegistry& reg, ArchGroup group,
                                   const CreatureDef* active, int generation,
                                   const std::vector<SaveStoredPet>& rack,
                                   const std::vector<SaveRecord>& records) {
    std::vector<ArchRow> out;
    switch (group.kind) {
        case ArchGroup::Kind::NewEgg:
            break;                       // an action, not a list
        case ArchGroup::Kind::Active:
            if (active) out.push_back({ArchRow::Kind::Active, -1, active, generation, 0});
            break;
        case ArchGroup::Kind::Line: {
            const auto lines = reg.allCreatureLines();
            if (group.lineIndex < 0 || group.lineIndex >= static_cast<int>(lines.size()))
                break;
            const char* lineId = lines[group.lineIndex]->id;
            for (int i = 0; i < static_cast<int>(rack.size()); ++i) {
                const CreatureDef* c = reg.creature(rack[i].id);
                if (!c || !c->line || !lineId || std::strcmp(c->line, lineId) != 0) continue;
                out.push_back({ArchRow::Kind::Stored, i, c, rack[i].generation, 0,
                               rack[i].combatLevel});
            }
            break;
        }
        case ArchGroup::Kind::Sell:
            for (int i = 0; i < static_cast<int>(rack.size()); ++i) {
                const CreatureDef* c = reg.creature(rack[i].id);
                if (!c || c->stage != Stage::Daemon) continue;
                out.push_back({ArchRow::Kind::Stored, i, c, rack[i].generation, 0,
                               rack[i].combatLevel});
            }
            break;
        case ArchGroup::Kind::Records:
            for (int i = 0; i < static_cast<int>(records.size()); ++i)
                out.push_back({ArchRow::Kind::Record, i, reg.creature(records[i].id),
                               records[i].generation, records[i].status});
            break;
    }
    return out;
}

void drawArchPicker(Framebuffer& fb, const std::vector<ArchPickRow>& tiles, int cursor,
                    int used, int maxSlots) {
    char slots[16];
    std::snprintf(slots, sizeof(slots), "SLOTS %d/%d", used, maxSlots);
    drawHeaderBand(fb, "ARCH", slots);

    // Pitch is set by the tile COUNT against the hint band, the same arithmetic the
    // ITEMS type-picker does: the picker never scrolls, so every row has to fit between
    // kRowTop and the band. The rows are three groups — this pet (NEW EGG, ACTIVE), the
    // shelf (one per family) and the way out of it (SELL, RECORDS) — and a gap opens
    // where the group changes, so SELL doesn't read as a sixth family and the shelf
    // doesn't read as ACTIVE's children, which is what indenting it would say. The pitch
    // gives way to the count rather than the gaps: nine rows land at 18.
    constexpr int kPickRowMaxH = 20;
    constexpr int kGroupGap = 8;
    const int n = static_cast<int>(tiles.size());
    const auto band = [](ArchGroup::Kind k) {
        return k == ArchGroup::Kind::Line ? 1
             : (k == ArchGroup::Kind::Sell || k == ArchGroup::Kind::Records) ? 2 : 0;
    };
    int gaps = 0;
    for (int i = 1; i < n; ++i)
        if (band(tiles[i].group.kind) != band(tiles[i - 1].group.kind)) ++gaps;
    const int room = kActiveH - kHintBandH - kRowTop - gaps * kGroupGap;
    const int pitch = n > 0 ? std::min(kPickRowMaxH, room / n) : kPickRowMaxH;
    int y = kRowTop - pitch;
    for (int i = 0; i < n; ++i) {
        const ArchPickRow& t = tiles[i];
        y += pitch;
        if (i > 0 && band(t.group.kind) != band(tiles[i - 1].group.kind)) y += kGroupGap;
        if (i == cursor) {
            fb.fillRect(4, y + 2, kActiveW - 8, pitch - 4, palColor(Pal::TRACK));
            drawRowCursor(fb, 8, y + (pitch - 7) / 2, palColor(Pal::ACCENT));
        }
        // NEW EGG is always live — it is an action, and "zero of it" means nothing. Every
        // other row dims when its shelf is empty, name and count together, so the row
        // still reads as empty with the colour channel gone.
        const bool live = t.group.kind == ArchGroup::Kind::NewEgg || t.count > 0;
        const Rgb565 ink = live ? palColor(Pal::INK) : palColor(Pal::INK_DIM);
        drawText(fb, 24, y + (pitch - kFontH) / 2, t.label, ink);
        if (t.group.kind != ArchGroup::Kind::NewEgg) {
            char qty[8];
            std::snprintf(qty, sizeof(qty), "%d", t.count);
            drawText(fb, kActiveW - kMargin - textWidth(qty),
                     y + (pitch - kFontH) / 2, qty, ink);
        }
    }

    drawHintBand(fb, "A NEXT  B OPEN  C BACK");
}

void drawArchList(Framebuffer& fb, const std::vector<ArchRow>& rows, const char* title,
                  int cursor, int used, int maxSlots, bool showLevels) {
    char slots[16];
    std::snprintf(slots, sizeof(slots), "SLOTS %d/%d", used, maxSlots);
    drawHeaderBand(fb, title ? title : "ARCH", slots);

    const int n = static_cast<int>(rows.size());
    if (n == 0) {
        drawText(fb, kMargin, kRowTop + 8, "- NOTHING HERE -", palColor(Pal::INK_DIM));
        drawHintBand(fb, "C BACK");
        return;
    }

    const int scrollTop = listScrollTop(cursor, n, kVisibleRows);
    for (int v = 0; v < kVisibleRows && scrollTop + v < n; ++v) {
        const int i = scrollTop + v;
        const int y = kRowTop + v * kRowH;
        const ArchRow& r = rows[i];
        const char* name = r.def ? r.def->displayName : "?";
        const char* stage = r.def ? stageName(r.def->stage) : "-";
        switch (r.kind) {
            case ArchRow::Kind::Active:
                rackRow(fb, y, i == cursor, ASSET_ICON_ARCH_SLOT, name, stage, "ACTIVE",
                        palColor(Pal::ACCENT));
                break;
            case ArchRow::Kind::Stored: {
                char lv[12];
                std::snprintf(lv, sizeof(lv), "LV %d", r.level);
                rackRow(fb, y, i == cursor, ASSET_ICON_ARCH_SLOT, name, stage,
                        showLevels ? lv : "FROZEN",
                        palColor(showLevels ? Pal::INK : Pal::INK_DIM));
                break;
            }
            case ArchRow::Kind::Record: {
                // RETIRED/CORRUPTED records: greyed, no slot, read-only.
                SaveRecord rec;
                rec.status = r.status;
                rackRow(fb, y, i == cursor, ASSET_ICON_ARCH_SLOT_RETIRED, name, stage,
                        recordStatusTag(rec), palColor(Pal::INK_DIM));
                break;
            }
        }
    }

    if (n > kVisibleRows) {  // slim scrollbar (UI_SCROLLBAR)
        const int barX = kActiveW - 3;
        const int trackH = kVisibleRows * kRowH;
        fb.fillRect(barX, kRowTop, 2, trackH, palColor(Pal::TRACK));
        const int thumbH = std::max(8, trackH * kVisibleRows / n);
        const int thumbY = kRowTop + trackH * scrollTop / n;
        fb.fillRect(barX, thumbY, 2, thumbH, palColor(Pal::INK_DIM));
    }
    drawHintBand(fb, "A NEXT  B OPEN  C BACK");
}

void drawArchNewEgg(Framebuffer& fb, const CreatureDef* active, bool rackFull,
                    bool confirmOpen, int confirmChoice) {
    drawHeaderBand(fb, "NEW EGG", rackFull ? "RACK FULL" : "READY");

    // The whole point of this screen is that laying an egg has a COST, and the cost is
    // the pet you are raising: it goes to the rack first, which is why a full rack is
    // what stops you. Said plainly here rather than discovered by pressing Store on a
    // pet record and reading the prompt.
    if (active) {
        char sub[28];
        std::snprintf(sub, sizeof(sub), "%s IS STORED FIRST", active->displayName);
        drawText(fb, kMargin, 34, sub, palColor(Pal::INK));
        drawText(fb, kMargin, 52, "IT KEEPS EVERY STAT IT HAS.", palColor(Pal::INK_DIM));
    } else {
        drawText(fb, kMargin, 34, "NO ACTIVE PET TO SET ASIDE.", palColor(Pal::INK));
        drawText(fb, kMargin, 52, "THE EGG IS LAID OUTRIGHT.", palColor(Pal::INK_DIM));
    }
    drawText(fb, kMargin, 150,
             rackFull ? "- NO FREE RACK SLOT -" : "HATCH A FRESH EGG.",
             rackFull ? palColor(Pal::INK_DIM) : palColor(Pal::INK));

    if (confirmOpen) {
        const int by = 80, bh = 56;
        fb.fillRect(4, by, kActiveW - 8, bh, palColor(Pal::TRACK));
        drawText(fb, kMargin, by + 8, "HATCH A NEW EGG?", palColor(Pal::INK));
        const int cy = by + 32;
        if (confirmChoice == 0) drawRowCursor(fb, kMargin, cy, palColor(Pal::ACCENT));
        drawText(fb, kMargin + 12, cy, "CANCEL", palColor(Pal::INK));
        const char* ok = "CONFIRM";
        const int okX = kActiveW - kMargin - textWidth(ok);
        if (confirmChoice == 1) drawRowCursor(fb, okX - 12, cy, palColor(Pal::ACCENT));
        drawText(fb, okX, cy, ok, palColor(Pal::INK));
    }
    drawHintBand(fb, confirmOpen ? "A TOGGLE  B COMMIT  C CANCEL" : "B HATCH  C BACK");
}

void drawArchRecordDetail(Framebuffer& fb, const ContentRegistry& reg,
                          const SaveRecord& rec) {
    const CreatureDef* c = reg.creature(rec.id);
    const char* tag = recordStatusTag(rec);
    drawHeaderBand(fb, c ? c->displayName : "?", tag);

    char sub[28];
    std::snprintf(sub, sizeof(sub), "%s  GEN %d",
                  c ? stageName(c->stage) : "-", rec.generation);
    drawText(fb, kMargin, 34, sub, palColor(Pal::INK_DIM));
    drawText(fb, kMargin, 52,
             static_cast<RecordStatus>(rec.status) == RecordStatus::Corrupted
                 ? "LOST TO CRITICAL SYSTEM FAILURE."
                 : "RETIRED - PERMANENT RECORD.",
             palColor(Pal::INK_DIM));
    drawText(fb, kMargin, 150, "- NO ACTIONS (RECORD) -", palColor(Pal::INK_DIM));
    drawHintBand(fb, "C BACK");
}

void drawArchRecord(Framebuffer& fb, const CreatureDef* pet, bool isActive,
                    int generation, ArchAction action, bool rackFull, bool confirmOpen,
                    int confirmChoice) {
    if (!pet) { drawHeaderBand(fb, "ARCH"); return; }
    char title[28];
    std::snprintf(title, sizeof(title), "%s", pet->displayName);
    drawHeaderBand(fb, title, isActive ? "ACTIVE" : "STORED");

    char sub[28];
    std::snprintf(sub, sizeof(sub), "%s  GEN %d", stageName(pet->stage), generation);
    drawText(fb, kMargin, 34, sub, palColor(Pal::INK));
    drawText(fb, kMargin, 52,
             isActive ? "THE PET YOU'RE RAISING." : "FROZEN - NO DECAY IN STORAGE.",
             palColor(Pal::INK_DIM));

    // The action set: active → Store · stored → Deploy/Release (the no-reward valve).
    const ArchAction primary = isActive ? ArchAction::Store : ArchAction::Deploy;
    const int primY = 150, releaseY = 170;

    if (action == primary) drawRowCursor(fb, kMargin, primY, palColor(Pal::ACCENT));
    if (isActive) {
        drawLabelValue(fb, kMargin + 12, primY, "STORE", palColor(Pal::INK),
                       rackFull ? "RACK FULL" : "SET ASIDE", palColor(Pal::INK_DIM),
                       0, false);
    } else {
        drawLabelValue(fb, kMargin + 12, primY, "DEPLOY", palColor(Pal::INK),
                       "MAKE ACTIVE", palColor(Pal::INK_DIM), 0, false);
    }

    // Release: stored pets only — a no-reward drop that frees a rack slot.
    if (!isActive) {
        if (action == ArchAction::Release)
            drawRowCursor(fb, kMargin, releaseY, palColor(Pal::ACCENT));
        drawLabelValue(fb, kMargin + 12, releaseY, "RELEASE", palColor(Pal::INK),
                       "NO REWARD", palColor(Pal::INK_DIM), 0, false);
    }

    // Inline confirm: a light prompt with Cancel/Confirm (default Cancel).
    if (confirmOpen) {
        const int by = 80, bh = 56;
        fb.fillRect(4, by, kActiveW - 8, bh, palColor(Pal::TRACK));
        const char* prompt = (action == ArchAction::Store)
                                 ? "STORE & HATCH A NEW EGG?"
                             : (action == ArchAction::Release)
                                 ? "RELEASE? NO REWARD."
                                 : "DEPLOY THIS PET?";
        drawText(fb, kMargin, by + 8, prompt, palColor(Pal::INK));
        const char* cancel = "CANCEL";
        const char* ok = "CONFIRM";
        const int cy = by + 32;
        if (confirmChoice == 0) drawRowCursor(fb, kMargin, cy, palColor(Pal::ACCENT));
        drawText(fb, kMargin + 12, cy, cancel, palColor(Pal::INK));
        const int okX = kActiveW - kMargin - textWidth(ok);
        if (confirmChoice == 1) drawRowCursor(fb, okX - 12, cy, palColor(Pal::ACCENT));
        drawText(fb, okX, cy, ok, palColor(Pal::INK));
    }
    drawHintBand(fb, confirmOpen ? "A TOGGLE  B COMMIT  C CANCEL"
                                 : "A CYCLE  B SELECT  C BACK");
}

void drawArchSale(Framebuffer& fb, const ArchSaleSheet& sheet) {
    const CreatureDef* d = sheet.daemon;
    if (!d) { drawHeaderBand(fb, "SELL"); return; }
    char lv[12];
    std::snprintf(lv, sizeof(lv), "LV %d", sheet.level);
    // The page is a TRADE, not this pet's record, so its title says so and the Daemon is
    // named as the thing given up. Under its own name as a title, "REIMAGE" read as
    // something done TO the Daemon rather than bought WITH it.
    drawHeaderBand(fb, "TRADE IN", lv);

    char give[28];
    std::snprintf(give, sizeof(give), "GIVE UP %s", d->displayName);
    drawText(fb, kMargin, 34, give, palColor(Pal::INK));
    drawText(fb, kMargin, 52, "GONE FOR GOOD. GET ONE:", palColor(Pal::INK_DIM));

    // The focused offer's whole trade, spelled out above the rows. The two patches land
    // on the ACTIVE pet, never the Daemon being sold, so the copy names who gets them.
    static const char* const kLabel[kSaleOfferCount] = {"BITS", "REIMAGE", "HOTFIX"};
    static const char* const kWhat[kSaleOfferCount][2] = {
        {"BITS FOR ITS LEVEL,", nullptr},   // the par level is a tunable, so built below
        {"YOUR ACTIVE TWIN GOES BACK", nullptr},   // names its Script, built below
        {"YOUR ACTIVE PET HAS ALL", "ITS ERRORS CLEARED."},
    };
    const int f = static_cast<int>(sheet.focus);
    drawText(fb, kMargin, 82, kWhat[f][0], palColor(Pal::INK));
    char second[28];
    if (sheet.focus == SaleOffer::Bits)
        std::snprintf(second, sizeof(second), "CLIMBING PAST LV %d.", kDaemonSaleParLevel);
    else if (sheet.reimageTo[0])
        std::snprintf(second, sizeof(second), "TO %s, 0 ERRORS", sheet.reimageTo);
    else
        std::snprintf(second, sizeof(second), "TO ITS SCRIPT, 0 ERRORS");
    drawText(fb, kMargin, 96, kWhat[f][1] ? kWhat[f][1] : second, palColor(Pal::INK));

    for (int i = 0; i < kSaleOfferCount; ++i) {
        const int y = 148 + i * 20;          // clear of the confirm box below 136
        const ArchSaleSheet::Offer& o = sheet.offers[i];
        if (i == f) drawRowCursor(fb, kMargin, y, palColor(Pal::ACCENT));
        drawLabelValue(fb, kMargin + 12, y, kLabel[i],
                       palColor(o.available ? Pal::INK : Pal::INK_DIM), o.value,
                       palColor(Pal::INK_DIM), 0, false);
    }

    if (sheet.confirmOpen) {
        const int by = 80, bh = 56;
        fb.fillRect(4, by, kActiveW - 8, bh, palColor(Pal::TRACK));
        static const char* const kFor[kSaleOfferCount] = {
            "FOR BITS?", "FOR A REIMAGE?", "FOR A HOTFIX?"};
        drawText(fb, kMargin, by + 6, give, palColor(Pal::INK));
        drawText(fb, kMargin, by + 18, kFor[f], palColor(Pal::INK));
        const int cy = by + 38;
        if (sheet.confirmChoice == 0) drawRowCursor(fb, kMargin, cy, palColor(Pal::ACCENT));
        drawText(fb, kMargin + 12, cy, "CANCEL", palColor(Pal::INK));
        const char* ok = "CONFIRM";
        const int okX = kActiveW - kMargin - textWidth(ok);
        if (sheet.confirmChoice == 1) drawRowCursor(fb, okX - 12, cy, palColor(Pal::ACCENT));
        drawText(fb, okX, cy, ok, palColor(Pal::INK));
    }
    drawHintBand(fb, sheet.confirmOpen ? "A TOGGLE  B COMMIT  C CANCEL"
                                       : "A CYCLE  B TRADE  C BACK");
}

} // namespace mal
