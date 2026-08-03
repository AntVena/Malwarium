#include "core/ui/arch_screen.h"

#include <cstdio>

#include "core/content/registry.h"
#include "core/render/canvas.h"
#include "core/render/font5x7.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/widgets.h"
#include "generated/assets.h"

namespace mal {

namespace {

constexpr int kMargin = 8;
constexpr int kHeaderRule = 22;
constexpr int kRowTop = 26;
constexpr int kRowH = 28;
constexpr int kIcon = 20;

void header(Framebuffer& fb, const char* title, const char* right) {
    fb.clear(palColor(Pal::PAPER));
    drawText(fb, kMargin, 6, title, palColor(Pal::INK));
    if (right)
        drawText(fb, kActiveW - kMargin - textWidth(right), 6, right,
                 palColor(Pal::INK_DIM));
    fb.fillRect(0, kHeaderRule, kActiveW, 1, palColor(Pal::TRACK));
}

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
    drawSprite(fb, slotIcon, 0, 16, y + (kRowH - kIcon) / 2);
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

void drawArchList(Framebuffer& fb, const ContentRegistry& reg,
                  const CreatureDef* active, const std::vector<SaveStoredPet>& rack,
                  const std::vector<SaveRecord>& records, int cursor, int maxSlots) {
    // Rack-slot usage surfaces the storage cost up front; stored pets fill
    // the slots, the active pet has its own save and uses none. Records do NOT
    // consume slots — they're greyed history below the live rows.
    // maxSlots is the purchasable capacity (kRackSlots + upgrades), not the
    // bare tunable, so the header stays honest once the player's bought slots.
    char slots[16];
    std::snprintf(slots, sizeof(slots), "SLOTS %d/%d",
                  static_cast<int>(rack.size()), maxSlots);
    header(fb, "ARCH", slots);

    if (!active && rack.empty() && records.empty()) {
        drawText(fb, kMargin, kRowTop + 8, "- NO PETS -", palColor(Pal::INK_DIM));
        return;
    }

    int y = kRowTop;
    int row = 0;
    if (active) {
        rackRow(fb, y, cursor == row, ASSET_ICON_ARCH_SLOT, active->displayName,
                stageName(active->stage), "ACTIVE", palColor(Pal::ACCENT));
        ++row;
    }
    for (int i = 0; i < static_cast<int>(rack.size()); ++i, ++row) {
        y += kRowH;
        const CreatureDef* c = reg.creature(rack[i].id);
        rackRow(fb, y, cursor == row, ASSET_ICON_ARCH_SLOT, c ? c->displayName : "?",
                c ? stageName(c->stage) : "-", "FROZEN", palColor(Pal::INK_DIM));
    }
    // RETIRED/CORRUPTED records: greyed, no slot, read-only.
    for (int i = 0; i < static_cast<int>(records.size()); ++i, ++row) {
        y += kRowH;
        const CreatureDef* c = reg.creature(records[i].id);
        rackRow(fb, y, cursor == row, ASSET_ICON_ARCH_SLOT_RETIRED,
                c ? c->displayName : "?", c ? stageName(c->stage) : "-",
                recordStatusTag(records[i]), palColor(Pal::INK_DIM));
    }
}

void drawArchRecordDetail(Framebuffer& fb, const ContentRegistry& reg,
                          const SaveRecord& rec) {
    const CreatureDef* c = reg.creature(rec.id);
    const char* tag = recordStatusTag(rec);
    header(fb, c ? c->displayName : "?", tag);

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
    drawText(fb, kMargin, 200, "C BACK", palColor(Pal::INK_DIM));
}

void drawArchRecord(Framebuffer& fb, const CreatureDef* pet, bool isActive,
                    int generation, ArchAction action, bool sellEnabled,
                    bool rackFull, bool confirmOpen, int confirmChoice) {
    if (!pet) { header(fb, "ARCH", nullptr); return; }
    char title[28];
    std::snprintf(title, sizeof(title), "%s", pet->displayName);
    header(fb, title, isActive ? "ACTIVE" : "STORED");

    char sub[28];
    std::snprintf(sub, sizeof(sub), "%s  GEN %d", stageName(pet->stage), generation);
    drawText(fb, kMargin, 34, sub, palColor(Pal::INK));
    drawText(fb, kMargin, 52,
             isActive ? "THE PET YOU'RE RAISING." : "FROZEN - NO DECAY IN STORAGE.",
             palColor(Pal::INK_DIM));

    // The action set: active → Store/Sell · stored → Deploy/Sell/Release (the no-reward
    // valve). The primary row sits higher when a stored pet adds Release.
    const ArchAction primary = isActive ? ArchAction::Store : ArchAction::Deploy;
    const int primY = isActive ? 150 : 130, sellY = isActive ? 170 : 150;
    const int releaseY = 170;

    if (action == primary) drawRowCursor(fb, kMargin, primY, palColor(Pal::ACCENT));
    if (isActive) {
        drawText(fb, kMargin + 12, primY, "STORE", palColor(Pal::INK));
        drawText(fb, kActiveW - kMargin - textWidth("(SET ASIDE -> NEW EGG)"), primY,
                 rackFull ? "- RACK FULL -" : "(SET ASIDE -> NEW EGG)",
                 palColor(Pal::INK_DIM));
    } else {
        drawText(fb, kMargin + 12, primY, "DEPLOY", palColor(Pal::INK));
        drawText(fb, kActiveW - kMargin - textWidth("(MAKE ACTIVE)"), primY,
                 "(MAKE ACTIVE)", palColor(Pal::INK_DIM));
    }

    if (action == ArchAction::Sell) drawRowCursor(fb, kMargin, sellY, palColor(Pal::ACCENT));
    drawText(fb, kMargin + 12, sellY, "SELL",
             sellEnabled ? palColor(Pal::INK) : palColor(Pal::INK_DIM));
    drawText(fb, kActiveW - kMargin - textWidth("- DAEMONS ONLY -"), sellY,
             sellEnabled ? "PERMANENT" : "- DAEMONS ONLY -", palColor(Pal::INK_DIM));

    // Release: stored pets only — a no-reward drop that frees a rack slot.
    if (!isActive) {
        if (action == ArchAction::Release)
            drawRowCursor(fb, kMargin, releaseY, palColor(Pal::ACCENT));
        drawText(fb, kMargin + 12, releaseY, "RELEASE", palColor(Pal::INK));
        drawText(fb, kActiveW - kMargin - textWidth("(FREE SLOT - NO REWARD)"), releaseY,
                 "(FREE SLOT - NO REWARD)", palColor(Pal::INK_DIM));
    }

    // Inline confirm: a light prompt with Cancel/Confirm (default Cancel).
    if (confirmOpen) {
        const int by = 80, bh = 56;
        fb.fillRect(4, by, kActiveW - 8, bh, palColor(Pal::TRACK));
        const char* prompt = (action == ArchAction::Store)
                                 ? "STORE & HATCH A NEW EGG?"
                             : (action == ArchAction::Release)
                                 ? "RELEASE THIS PET? NO REWARD."
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
}

} // namespace mal
