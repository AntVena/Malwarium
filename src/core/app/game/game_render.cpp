#include "core/app/game.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "core/app/game_internal.h"   // levelStatName()
#include "tunables.h"
#include "core/content/content_tables.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/carousel.h"
#include "core/ui/combat_screen.h"
#include "core/ui/items_screen.h"
#include "core/ui/maint_screen.h"
#include "core/ui/modals.h"
#include "core/ui/stat_screen.h"
#include "core/ui/train_screen.h"
#include "generated/assets.h"

namespace mal {

// --- Render ----------------------------------------------------------------

void Game::render(Framebuffer& fb) const {
    const SpriteData* pet = pet_ ? registry_.creatureSprite(*pet_) : nullptr;
    switch (nav_) {
        case Nav::Idle:
            if (face_ == Face::Hacker) drawHackerHome(fb, -1);
            else drawHabitat(fb, -1);
            break;
        case Nav::Cursor:
            if (face_ == Face::Hacker) drawHackerHome(fb, cursor_);
            else drawHabitat(fb, cursor_);
            break;
        case Nav::Submenu:
            if (face_ == Face::Hacker) drawHackerSubmenu(fb);
            else drawSubmenu(fb);
            break;
        case Nav::Detail:  drawDetail(fb); break;
        case Nav::Process: drawProcess(fb); break;
        case Nav::Stacker: drawStacker(fb); break;
        case Nav::Combat: drawCombatScreen(fb); break;
        case Nav::ExploreControl:
            // The A+C control overlay floats over the idle habitat.
            drawHabitat(fb, -1);
            drawExploreControl(fb, exploreCtlRow_, !heldWarpKeys().empty(),
                               autoProgress_);
            break;
        case Nav::Encounter: drawEncounterScreen(fb); break;
        case Nav::Wifi: drawWifiScreen(fb); break;
        case Nav::Shop: drawShopScreen(fb); break;
        case Nav::ModShop: drawShopScreen(fb); break;
        case Nav::WarpPicker: drawWarpPickerScreen(fb); break;
        case Nav::RollbackPicker: drawRollbackPickerScreen(fb); break;
        case Nav::CacheYield: drawCacheYieldScreen(fb); break;
        case Nav::BulkYield: drawBulkYieldScreen(fb); break;
        case Nav::PostEncounter: drawPostEncounterScreen(fb); break;
        case Nav::ModalLineSelect: drawLineSelect(fb); break;
        case Nav::ModalHatch: {
            // Transient per-press shimmy: a few-px nudge for a short window.
            const bool jiggle = hatchPressMs_ != 0 &&
                                (nowMs_ - hatchPressMs_) < kHatchJiggleMs;
            const int dx = jiggle ? (((nowMs_ / 40) & 1) ? 3 : -3) : 0;
            drawHatchModal(fb, hatchEggSprite(), hatchCrackFrame(),
                           hatchProgress(), beat_, dx);
            break;
        }
        case Nav::ModalEggPick: drawEggPick(fb); break;
        case Nav::ModalHatchReveal: drawHatchReveal(fb); break;
        case Nav::ModalFeeding:
            drawFeedingModal(fb, pet, feedItem_, model_, feedBefore_, feedBeat_);
            break;
        case Nav::ModalEvolve: drawEvolve(fb); break;
        case Nav::ModalCSF: drawCSF(fb); break;
        case Nav::ModalLockout: {
            const int sl = lockoutDeadlineMs_ > nowMs_
                               ? static_cast<int>((lockoutDeadlineMs_ - nowMs_ + 999) / 1000)
                               : 0;
            const float rf = lockoutDeadlineMs_ > nowMs_
                                 ? static_cast<float>(lockoutDeadlineMs_ - nowMs_) / kLockoutDurationMs
                                 : 0.0f;
            drawLockoutModal(fb, pet, model_, sl, rf, lockoutPayOption_,
                             bits_ >= kLockoutBitsCost, kLockoutBitsCost, beat_);
            break;
        }
    }
}

void Game::drawLineSelect(Framebuffer& fb) const {
    // Line-select modal: full 224x224, no track/header chrome. Grayscale-
    // safe — the highlighted line carries BOTH a ">" cursor glyph (shape) and INK vs
    // INK_DIM (luminance), so a desaturated screenshot still reads the selection.
    fb.clear(palColor(Pal::PAPER));
    const char* title = "NEW SPECIMEN";
    drawText(fb, (kActiveW - textWidth(title)) / 2, 46, title, palColor(Pal::INK));
    const char* sub = "CHOOSE A LINE TO DECRYPT";
    drawText(fb, (kActiveW - textWidth(sub)) / 2, 46 + kFontH + 5, sub,
             palColor(Pal::INK_DIM));

    const auto lines = availableEggLines();   // only UNLOCKED lines (matches the modal)
    const int n = static_cast<int>(lines.size());
    const int sel = n > 0 ? lineSelectRow_ % n : 0;
    const int rowH = 24;
    const int top = 100;
    for (int i = 0; i < n; ++i) {
        const bool on = (i == sel);
        const int y = top + i * rowH;
        const char* name = lines[i]->displayName;
        const int nameW = textWidth(name);
        const int x = (kActiveW - nameW) / 2;
        if (on) drawText(fb, x - 16, y, ">", palColor(Pal::ACCENT));
        drawText(fb, x, y, name, on ? palColor(Pal::INK) : palColor(Pal::INK_DIM));
    }

    const char* hint = "A CYCLE   B SELECT";
    drawText(fb, (kActiveW - textWidth(hint)) / 2, kActiveH - 26, hint,
             palColor(Pal::INK_DIM));
}

void Game::drawHabitat(Framebuffer& fb, int cursor) const {
    fb.clear(palColor(Pal::PAPER));

    const SpriteData* pet = pet_ ? registry_.creatureSprite(*pet_) : nullptr;
    if (pet) {
        // An authored "idle" clip plays its full row in order; otherwise fall
        // back to the breathe/blink heuristic on row 0.
        const AnimClip* idle = pet_ ? pet_->clip("idle") : nullptr;
        const int row = idle ? idle->row : 0;
        const int frame = idle ? idle->frameAt(beat_) : idleFrame(*pet, beat_);
        // Two motions, and the creature is the only thing on this screen that takes
        // either. The BOB is the pose's own 2px lift on alternate beats — what
        // animates a single-frame creature; a swimmer skips it, since a bob on top of
        // a continuous drift reads as jitter rather than breathing. The WANDER moves
        // the whole anchor around the living box, in logical px
        // (core/model/idle_wander.h): sideways for every mover, and off the shelf for
        // the ones that don't need the floor.
        const int bob = (IdleWander::bobs(pet_->locomotion) && (beat_ & 1) == 0) ? 2 : 0;
        const int petW = pet->frameW * kScaleNum / kScaleDen;
        const int petH = pet->h * kScaleNum / kScaleDen;
        const int petX = (kActiveW - petW) / 2 + logicalToActive(petWander_.offsetX());
        const int petY =
            kLivingBottom - petH - bob - logicalToActive(petWander_.offsetY());
        drawSpriteUpscaled(fb, *pet, frame, petX, petY, kScaleNum, kScaleDen, row);
    }

    // Boot-Sector incubation prompt (redesign): an unhatched egg shows its
    // state ABOVE the egg sprite, in the gap below the carousel caption band
    // (kLivingTop+4, where a summoned menu name renders) so the two never overlap.
    // Before the second half it's the INCUBATING minutes; once decryptable it
    // flashes the hacker EXPLOIT symbol. Grayscale-safe: distinct glyph / INK text.
    if (inEggPhase()) {
        // Anchor to just above the egg; clamp below the caption band so it can never
        // ride up into a summoned menu name.
        const int petH = pet ? pet->h * kScaleNum / kScaleDen : 0;
        const int petTop = kLivingBottom - petH;
        const int minY = kLivingTop + 4 + kFontH + 3;   // one line below the caption
        if (eggCrackable()) {
            // The ⚡ override pip flashes, inviting the A+C Exploit chord to crack the
            // egg — into the decrypt minigame or the hatch cinematic, whichever this
            // line has. A distinct glyph + "A+C" chord label carry the meaning with no
            // colour channel. It only ever appears when the chord will actually do
            // something, so an egg is never advertised a hatch it can't take.
            const bool on = ((beat_ / 2) & 1) == 0;   // flash to draw the eye
            if (on) {
                const SpriteData& glyph = ASSET_ICON_OVERRIDE_PIP;
                const int blockH = glyph.h + 2 + kFontH;
                int gy = petTop - 6 - blockH;
                if (gy < minY) gy = minY;
                drawSprite(fb, glyph, 0, (kActiveW - glyph.frameW) / 2, gy);
                const char* hint = "A+C";
                drawText(fb, (kActiveW - textWidth(hint)) / 2, gy + glyph.h + 2,
                         hint, palColor(Pal::INK));
            }
        } else {
            // M:SS, counting down. Seconds are the point: a minutes-only readout
            // sits on the same number for a minute at a time, which reads as a
            // stalled egg rather than a waiting one. The egg sprite directly above
            // says what is being counted, so the digits carry no label. Repaint
            // comes free from the heartbeat (game_core.cpp), which marks every beat
            // dirty regardless of whether the model moved.
            char prompt[16];
            const unsigned totalSecs = bootHatchRemainMs_ / 1000u;
            std::snprintf(prompt, sizeof prompt, "%u:%02u",
                          totalSecs / 60u, totalSecs % 60u);
            int py = petTop - 6 - kFontH;
            if (py < minY) py = minY;
            drawText(fb, (kActiveW - textWidth(prompt)) / 2, py, prompt,
                     palColor(Pal::INK));
        }
    } else if (model_.isHungry()) {
        const bool on = ((beat_ / 2) & 1) == 0;
        if (on) {
            const SpriteData& icon = ASSET_UI_ALERT_HUNGER;
            drawSprite(fb, icon, 0, kActiveW - icon.frameW - 4, kLivingTop + 4);
        }
    }

    // Where the top-LEFT status column starts. Three unrelated things want this
    // corner — the SD flash, the buff row, and (while the mode runs) the explore
    // status stack drawn further down — so the origin is computed ONCE here and
    // every one of them hangs off it. Explore wins the corner outright because
    // its lines are the live readout; the rest slide below the block it occupies.
    int statusY = kLivingTop + 4;
    if (exploreActive_) statusY += exploreStatusLines() * (kFontH + 2) + 2;

    // SD-present flash: clear of the hunger alert (top-right) and the carousel
    // tracks (top/bottom bands). A distinct icon shape + slot so it reads in
    // grayscale.
    if (sdIconVisible()) {
        const SpriteData& icon = ASSET_ICON_SYS_SD;
        drawSprite(fb, icon, 0, 4, statusY);
    }

    // Active item-buff row: stacked below the SD icon slot (or at its spot if SD
    // is absent) — the STAT BUFFS page (page 2) is the full description, this is
    // just "something is currently armed." Each armed buff shows the icon of the
    // ITEM that armed it, resolved through the same itemIcon() the inventory uses
    // (items_screen.cpp), so a row that gains bespoke art needs no edit here; the
    // bracketed text tag is the fallback for an id that resolves to nothing, which
    // keeps the cue a shape-or-word rather than colour either way.
    {
        int bx = 4;
        const int by = statusY + (sdIconVisible() ? ASSET_ICON_SYS_SD.h + 4 : 0);
        // Draw one armed-buff marker and return the x the next one starts at.
        const auto marker = [&](const char* itemId, const char* tag) {
            if (const SpriteData* icon = itemIcon(registry_, itemId)) {
                drawSprite(fb, *icon, 0, bx, by - 2);
                bx += icon->frameW + 4;
            } else {
                bx = drawText(fb, bx, by, tag, palColor(Pal::INK)) + 4;
            }
        };
        if (mistakeShieldActive_) marker("restore_point", "[SHLD]");
        if (forceTrojanDivert_) marker("ambig_usb", "[USB]");
        if (backupShieldArmed()) marker("backup_drive", "[BKUP]");
        // The two DeepWeb Dive depth buffs record only that they are armed, not
        // which of the several items did it, so both show the shared Buff-item
        // stand-in itemIcon() falls back to rather than guessing an id.
        if (deepWebDepthMultiplier_ > 1) marker("deep_learning_module", "[DEEP]");
        if (pendingDeepWebStartDepth_ != -1) marker("backdoor_bell", "[BELL]");
    }

    // Audit capture-phase badge: top-RIGHT, just below the hunger alert. Surfaces
    // the AuditCapture SM phase — ARM / HOT <s>s / COOL <m>m — so the ~2-min hot
    // window AND the ~30-min re-arm cooldown (radio off; a handshake can't be
    // captured then) are visible instead of inferred. The Hot phase is the one
    // worth catching from across the room, so it takes a glyph as well; the word
    // stays either way, which is what keeps the badge grayscale-safe.
    {
        char badge[16];
        if (captureBadge(badge, sizeof badge)) {
            const int by = kLivingTop + 30;   // clear of the 20px hunger alert slot
            int bx = kActiveW - textWidth(badge) - 4;
            if (auditCapture_.phase() == AuditPhase::Hot) {
                const SpriteData& hot = ASSET_ICON_AUDIT_HOT;
                bx -= hot.frameW + 2;
                drawSprite(fb, hot, 0, bx, by - (hot.h - kFontH) / 2);
                bx += hot.frameW + 2;
            }
            drawText(fb, bx, by, badge, palColor(Pal::INK));
        }
    }

    // Explore-mode badge: a status line under the top track while the mode
    // runs — the armed sub-area + WINS n/10 (or BOSS READY). Dual-coded, grayscale-safe.
    // A brief event-flavor tick sits just below it. The badge label is the area's short
    // name + the 1-based sub number (e.g. "CITRUS 3") — compact + collision-free.
    if (exploreActive_) {
        // Pick the badge's right-field mode: the endless DeepWeb Dive shows DEPTH; a
        // re-armed CLEARED sub shows FARMING (its boss is done); an unlocked
        // sub-boss shows BOSS READY; otherwise WINS n/10 progress.
        ExploreBadgeMode mode;
        if (inDeepWebDive()) mode = ExploreBadgeMode::DeepDive;
        else if (subCleared(exploreSector_, exploreSub_)) mode = ExploreBadgeMode::Farming;
        else if (subBossUnlocked(exploreSector_, exploreSub_)) mode = ExploreBadgeMode::BossReady;
        else mode = ExploreBadgeMode::Wins;
        char label[20];
        exploreBadgeLabel(label, sizeof(label));
        // the FARMING badge shows Bandwidth (the farming pool), not the streak —
        // "FARM n/max" while it lasts (full loot), "FARM LOW" once depleted (decayed).
        int badgeCount = exploreStreak_, badgeMax = kExploreStreakToBoss;
        if (mode == ExploreBadgeMode::Farming) {
            badgeCount = bandwidth_; badgeMax = bandwidthMax();
        }
        drawExploreBadge(fb, label, badgeCount, badgeMax, mode);
        // Bandwidth is the per-fight fragmentation shield, so it drains on EVERY
        // explore fight — surface it persistently (not just in the
        // Farming badge) so the player can watch their safety budget and decide when to
        // defrag / come home. CALM while it lasts, WARN + "LOW" once dry (dual-coded).
        {
            char bw[20];
            // Line 2 of the block exploreStatusLines() counts; anything else that
            // wants this corner starts below the whole block (statusY, above).
            const int bwLineY = kLivingTop + 4 + kFontH + 2;
            if (bandwidth_ > 0) {
                std::snprintf(bw, sizeof(bw), "BW %d/%d", bandwidth_, bandwidthMax());
                drawText(fb, 8, bwLineY, bw, palColor(Pal::CALM));
            } else {
                std::snprintf(bw, sizeof(bw), "BW 0/%d LOW", bandwidthMax());
                drawText(fb, 8, bwLineY, bw, palColor(Pal::WARN));
            }
            if (exploreFlavor_[0])
                drawText(fb, 8, bwLineY + kFontH + 2, exploreFlavor_,
                         palColor(Pal::INK_DIM));
        }
    }

    // The achievement announcement, over everything else in the living area (it is
    // transient, and the pet is still there underneath when it clears).
    drawAchievementBanner(fb);

    // Grey out the care/combat slots while the pet is still an egg. EXPL's globe turns
    // while AUTO-PROGRESS is armed — the walk is running itself in the background, and
    // the shelf is the only screen that's always up to say so.
    unsigned eggLockMask = 0, spinMask = 0;
    for (int i = 0; i < kCarouselSlots; ++i) {
        const SubmenuId id = carouselSlots()[i].id;
        if (eggSlotLocked(id)) eggLockMask |= (1u << i);
        if (id == SubmenuId::Expl && autoProgress_ && exploreActive_)
            spinMask |= (1u << i);
    }
    drawCarousel(fb, cursor, uiMode_, beat_, eggLockMask, spinMask);
}

void Game::drawAchievementBanner(Framebuffer& fb) const {
    // The whole feedback channel for an unlock: three centred lines on a filled band,
    // shown on the home screen for kAchBannerMs. Modelled on the combat verdict banner —
    // a solid TRACK plate under INK text carries in grayscale on shape alone, no colour
    // doing any of the work.
    const AchievementDef* d = achBanner();
    if (!d) return;
    // The band sits in the gap between two things it must not cover: the idle status
    // slots along the top of the living area (SD top-left, hunger top-right, the capture
    // badge under it — reserved out to kLivingTop+40, and gated as such), and the pet
    // itself, which is anchored down on the shelf. Centring it would put the plate over
    // the pet's face for the whole announcement.
    // Three lines at font pitch, no more: the band has to fit the gap between the
    // reserved status slots above and the pet's crown below, and a taller plate would
    // have to cover one of them.
    const int bandY = kLivingTop + 42;
    const int bandH = 38;
    fb.fillRect(0, bandY, kActiveW, bandH, palColor(Pal::TRACK));
    // A 1px lid and sill: the band has to read as a plate laid over the habitat rather
    // than a hole in it, and an edge is the only cue that survives desaturation.
    fb.fillRect(0, bandY, kActiveW, 1, palColor(Pal::INK_DIM));
    fb.fillRect(0, bandY + bandH - 1, kActiveW, 1, palColor(Pal::INK_DIM));

    const char* kicker = "ACHIEVEMENT";
    drawText(fb, (kActiveW - textWidth(kicker)) / 2, bandY + 5, kicker,
             palColor(Pal::INK_DIM));

    // A burst says how many first and names one of them, so a long back-catalogue drop
    // still tells the player something specific about what they got.
    char line[40];
    if (achBannerCount_ > 1)
        std::snprintf(line, sizeof(line), "%d UNLOCKED", achBannerCount_);
    else
        std::snprintf(line, sizeof(line), "%s", d->displayName);
    drawText(fb, (kActiveW - textWidth(line)) / 2, bandY + 15, line, palColor(Pal::INK));

    // The third line is the reward, because that is the part with a consequence — a
    // Commendation Cache is sitting in the VAULT now and the player needs to know to
    // go and open it.
    char reward[40] = {0};
    int rewardBits = 0;
    const char* rewardItem = nullptr;
    for (const AchievementReward& r : d->rewards) {
        if (r.kind == AchievementReward::Kind::Bits) rewardBits += r.magnitude;
        else if (r.kind == AchievementReward::Kind::Item && r.id)
            if (const ItemDef* it = registry_.item(r.id)) rewardItem = it->displayName;
    }
    if (achBannerCount_ > 1)
        std::snprintf(reward, sizeof(reward), "%s +MORE", d->displayName);
    else if (rewardItem && rewardBits > 0)
        std::snprintf(reward, sizeof(reward), "+%d BITS + %s", rewardBits, rewardItem);
    else if (rewardItem)
        std::snprintf(reward, sizeof(reward), "+%s", rewardItem);
    else if (rewardBits > 0)
        std::snprintf(reward, sizeof(reward), "+%d BITS", rewardBits);
    if (reward[0])
        drawText(fb, (kActiveW - textWidth(reward)) / 2, bandY + 25, reward,
                 palColor(Pal::CALM));
}

bool Game::captureBadge(char* out, unsigned n) const {
    if (!out || n == 0) return false;
    switch (auditCapture_.phase()) {
        case AuditPhase::Armed:
            std::snprintf(out, n, "ARM");
            return true;
        case AuditPhase::Hot:
            std::snprintf(out, n, "HOT %us",
                          static_cast<unsigned>(auditCapture_.hotRemainingMs(nowMs_) / 1000u));
            return true;
        case AuditPhase::Cooldown:
            // Minutes remaining until re-arm — the number is the "why nothing's
            // capturing" answer (the radio is off for the whole window).
            std::snprintf(out, n, "COOL %um",
                          static_cast<unsigned>(auditCapture_.cooldownRemainingMs(nowMs_) / 60000u));
            return true;
        case AuditPhase::Disarmed:
        default:
            out[0] = '\0';
            return false;
    }
}

void Game::drawSubmenu(Framebuffer& fb) const {
    switch (enteredId()) {
        case SubmenuId::Stat:
            if (pet_) {
                // 5 pages: 0 pet vitals (the landing) · 1 the equipped loadout
                // (moves + mods, WITH their effect text) · 2 currently-armed item
                // buffs · 3 the pet's own species lore · 4 audit log. A cycles;
                // C backs out.
                if (statPage_ == 0)
                    drawStatScreen(fb, model_, pet_->displayName, pet_->stage,
                                   generation_, combatLevel_, combatXp_,
                                   xpToNextLevel(), beat_, hasNextEvolution(),
                                   evolveRemainMs());
                else if (statPage_ == 1)
                    drawLoadoutScreen(fb,
                                      buildLoadoutRows(registry_, moveLoadout_,
                                                        loadout_, pet_->stage,
                                                        inEggPhase()),
                                      loadoutScroll_, beat_);
                else if (statPage_ == 2) {
                    const uint32_t backupRemainMs =
                        backupShieldArmed() ? (backupShieldUntilMs_ - lifetimeUptimeMs())
                                            : 0;
                    drawBuffsScreen(fb,
                                    buildBuffRows(registry_, mistakeShieldActive_,
                                                  forceTrojanDivert_,
                                                  backupShieldArmed(), backupRemainMs,
                                                  deepWebDepthMultiplier_,
                                                  pendingDeepWebStartDepth_ != -1,
                                                  pendingDeepWebStartDepth_ ==
                                                      kDeepWebStartDepthUseBest,
                                                  pendingDeepWebStartDepth_),
                                    beat_);
                } else if (statPage_ == 3)
                    drawSpeciesScreen(fb, pet_->displayName, pet_->line, pet_->hint,
                                      pet_->context, beat_);
                else
                    drawAuditLog(fb, log_, beat_);
            }
            break;
        case SubmenuId::Items: {
            if (itemsScreen_ == ItemsScreen::Picker) {
                drawItemTypePicker(fb, buildItemPickerRows(registry_, inventory_),
                                   itemPickRow_);
                break;
            }
            auto rows = buildInventoryRows(registry_, inventory_, lockoutItemsContext_,
                                           itemFilter_);
            drawItemsList(fb, rows, listRow_, lockoutItemsContext_, beat_, itemFilter_,
                         itemTabsUnlocked(), itemPickerUnlocked());
            break;
        }
        case SubmenuId::Maint:
            drawMaintList(fb, model_, listRow_, beat_);
            break;
        case SubmenuId::Cfg:
            drawCfgList(fb, listRow_, hackerTag_, equippedTitleName(), radioOwner_);
            break;
        case SubmenuId::Arch:
            drawArchList(fb, registry_, pet_, rack_, records_, listRow_, rackSlots());
            break;
        case SubmenuId::Mods:
            drawModsList(fb, registry_, loadout_, listRow_);
            break;
        case SubmenuId::Train: {
            // #12: each slot's stamped required kind, for the row tags.
            MoveDef::Kind sk[kMaxMoveSlots];
            for (int i = 0; i < kMaxMoveSlots; ++i) sk[i] = slotRequiredKind(i);
            drawLoadout(fb, registry_, moveLoadout_,
                        pet_ ? pet_->stage : Stage::BootSector, trainRow_, sk, beat_);
            break;
        }
        case SubmenuId::Expl: {
            bool cleared[kExplSectors * kExplSubAreas];
            bool boss[kExplSectors * kExplSubAreas];
            flattenSubFlags(cleared, boss);
            ExplListView v;
            v.cursor = listRow_;
            v.areaCleared = sectorCleared_;
            v.subCleared = cleared;
            v.subBossUnlocked = boss;
            v.exploringSector = exploreActive_ ? exploreSector_ : -1;
            v.exploringSub = exploreActive_ ? exploreSub_ : -1;
            v.navArea = explNavArea_;
            v.streakWins = exploreStreak_;
            v.winsToBoss = kExploreStreakToBoss;
            v.bestDeepWebDepth = bestDeepWebDepth_;
            v.beat = beat_;
            drawExplList(fb, registry_, v);
            break;
        }
        default:
            drawPlaceholderSubmenu(fb, carouselSlots()[cursor_].label);
            break;
    }
}

void Game::drawDetail(Framebuffer& fb) const {
    switch (enteredId()) {
        case SubmenuId::Items:
            if (detailItem_) {
                const char* gate = "";
                const bool usable = itemUsable(*detailItem_, gate);
                const SpriteData* icon = itemIcon(registry_, detailItem_->id);
                drawItemDetail(fb, *detailItem_, icon,
                               inventory_.count(detailItem_->id), usable, gate,
                               beat_);
            }
            break;
        case SubmenuId::Maint:
            drawMaintAction(fb, maintKind_, model_,
                            maintKind_ == MaintKind::Defrag ? defragCost() : 0,
                            bits_, defragVariant_,
                            inventory_.count(kDefragToolId), defragCount_);
            break;
        case SubmenuId::Cfg:
            drawCfg(fb);
            break;
        case SubmenuId::Arch: {
            if (archRowIsRecord()) {
                const int ri = listRow_ - 1 - static_cast<int>(rack_.size());
                if (ri >= 0 && ri < static_cast<int>(records_.size()))
                    drawArchRecordDetail(fb, registry_, records_[ri]);
                break;
            }
            const bool active = archRowIsActive();
            const CreatureDef* recPet = pet_;
            int recGen = generation_;
            if (!active) {
                const int idx = listRow_ - 1;
                if (idx >= 0 && idx < static_cast<int>(rack_.size())) {
                    recPet = registry_.creature(rack_[idx].id);
                    recGen = rack_[idx].generation;
                }
            }
            const bool sellEnabled = recPet && recPet->stage == Stage::Daemon;
            const bool rackFull = static_cast<int>(rack_.size()) >= rackSlots();
            drawArchRecord(fb, recPet, active, recGen, archAction_, sellEnabled,
                           rackFull, archConfirm_, archConfirmChoice_);
            break;
        }
        case SubmenuId::Mods:
            if (modDetail_ && modDetailId_) {
                const ModDef* md = registry_.mod(modDetailId_);
                if (md) {
                    const char* cur = loadout_.equipped(modSlot_);
                    drawModDetail(fb, registry_, loadout_, *md,
                                  cur && std::strcmp(cur, modDetailId_) == 0,
                                  modSlot_, modEquipLevel(*md),
                                  combatLevel_, pet_ ? pet_->line : nullptr,
                                  modStorageCap());
                    break;
                }
            }
            drawModPicker(fb, registry_, loadout_, modSlot_, modPick_,
                          modConfirm_, modConfirmChoice_, modPendingId_, combatLevel_,
                          pet_ ? pet_->line : nullptr);
            break;
        case SubmenuId::Train:
            drawTrain(fb);
            break;
        default: break;
    }
}

void Game::drawTrain(Framebuffer& fb) const {
    if (trainScreen_ == TrainScreen::SimTier) {
        drawSimTier(fb, simTier_, kSimDummyTiers);
        return;
    }
    if (trainScreen_ == TrainScreen::MoveDetail) {
        // The reader half of the picker (hold-B). A cursor that has somehow lost its
        // move falls back to the picker rather than drawing an empty page.
        if (const MoveDef* m = focusedPickerMove()) {
            const char* eq = moveLoadout_.equipped(moveSlot_);
            drawMoveDetail(fb, registry_, *m,
                           pet_ ? pet_->stage : Stage::BootSector,
                           eq && std::strcmp(eq, m->id) == 0, moveProseScroll_);
            return;
        }
    }
    drawMovePicker(fb, registry_, moveLoadout_, moveSlot_, movePick_,
                   moveConfirm_, moveConfirmChoice_, movePendingId_,
                   pet_ ? pet_->stage : Stage::BootSector,
                   pet_ ? pet_->line : nullptr,
                   slotRequiredKind(moveSlot_), moveShowAll_);
}

void Game::drawCombatScreen(Framebuffer& fb) const {
    // In a duel BOTH combatants are remote-describable pets, so the player-side sprite
    // comes from the fight rather than from the local pet — on a guest's screen the
    // player_ slot holds the HOST's creature (core/model/pvp_battle.h).
    const bool duel = pvpFighting();
    const SpriteData* ps = duel ? registry_.sprite(combat_.player().spriteName)
                                : (pet_ ? registry_.creatureSprite(*pet_) : nullptr);
    const SpriteData* es = registry_.sprite(combat_.enemy().spriteName);

    // Both duellists read their own pet in the left seat on the zoned gauge, exactly like
    // every other fight — `localIsEnemySide` tells the screen to swap the roles for the
    // guest, whose pet occupies Combat's enemy_ slot. Display only: mirroring the fight
    // itself would desync the two devices.
    CombatSides sides;
    if (duel) {
        sides.rivalLabel = "RIVAL";
        sides.localLabel = "YOU";
        sides.localIsEnemySide = !pvpLocalIsHost();
    }
    drawCombat(fb, combat_, ps, es, beat_, combatAnimBeat_, combatHitBeat_,
               combatStatsOpen_, sides);
}

void Game::drawEncounterScreen(Framebuffer& fb) const {
    const SpriteData* es = registry_.sprite(encounterEnemy_.spriteName);
    drawEncounterIntro(fb, encounterEnemy_.name, encounterEnemy_.diffPips,
                       encounterEnemy_.level, es, encounterSinkhole_,
                       encounterChoice_, beat_);
}

void Game::drawWifiScreen(Framebuffer& fb) const {
    drawWifiEvent(fb, explSectorName(exploreSector_), wifiFlavor_, netDiscoveryFlavor_);
}

void Game::drawShopScreen(Framebuffer& fb) const {
    // Resolves every listing (id -> display name/description/costs) through the
    // registry into plain ShopRowView rows, so expl_screen.cpp's drawShop stays
    // registry-agnostic — mirrors drawHackerSubmenu building its row strings before
    // handing them to the generic windowed-list draw.
    const int n = std::min(shopListingCount(), kMaxShopListings);
    ShopRowView rows[kMaxShopListings];
    for (int i = 0; i < n; ++i) {
        rows[i].name = shopListingName(i);
        rows[i].bitsPrice = shopListingBitsPrice(i);
        rows[i].stock = shopListingStock(i);
        rows[i].costCount = std::min(shopListingCostCount(i), kMaxShopCostItems);
        for (int k = 0; k < rows[i].costCount; ++k) {
            rows[i].costName[k] = shopListingCostName(i, k);
            rows[i].costQty[k] = shopListingCostQty(i, k);
        }
    }
    drawShop(fb, shopStoreName(), bits_, rows, n, shopCursor_,
             shopListingDescription(shopCursor_).c_str(), shopStatusLine(), beat_);
}

void Game::drawPostEncounterScreen(Framebuffer& fb) const {
    // Frag state, dual-coded (a distinct WORD per state). The ACTUAL delta wins:
    // if fragmentation rose we say so honestly (a Bandwidth-shielded LOSS still takes the
    // separate loss penalty, so it can be both shielded AND up). Only a genuinely-clean
    // fight where a charge was spent reads SHIELDED; no charge + no rise reads None.
    const PostEncounterFragState st =
        (postEncFragAfter_ > postEncFragBefore_) ? PostEncounterFragState::Rose
        : (postEncShielded_ ? PostEncounterFragState::Shielded
                            : PostEncounterFragState::None);
    // Compose the level-up line (nullptr if this fight didn't level up): "LVL n" +
    // one "STAT +k" chip per stat that gained (usually one — a wild win grants a
    // single level → a single random stat). game_render owns the stat labels
    // (levelStatName), so drawPostEncounter stays label-agnostic.
    char lvlBuf[64];   // "LVL n" + up to kLevelStatCount " STAT +k" chips (multi-level lump)
    const char* lvlLine = nullptr;
    int gained = 0;
    for (int i = 0; i < kLevelStatCount; ++i) gained += postEncStatGain_[i];
    if (gained > 0) {
        int off = std::snprintf(lvlBuf, sizeof(lvlBuf), "LVL %d", postEncLevelAfter_);
        for (int i = 0; i < kLevelStatCount && off > 0 && off < (int)sizeof(lvlBuf); ++i)
            if (postEncStatGain_[i] > 0)
                off += std::snprintf(lvlBuf + off, sizeof(lvlBuf) - off, "  %s +%d",
                                     levelStatName(i), postEncStatGain_[i]);
        lvlLine = lvlBuf;
    }
    drawPostEncounter(fb, postEncBwBefore_, postEncBwAfter_, bandwidthMax(), st,
                      postEncFragAfter_ - postEncFragBefore_, lvlLine);
}

void Game::drawWarpPickerScreen(Framebuffer& fb) const {
    // Snapshot the held keys' display names into a small array the UI helper reads
    // (the picker is at most a couple of rows — one per warp-key type).
    auto keys = heldWarpKeys();
    const char* names[8];
    int n = 0;
    for (const ItemDef* d : keys) {
        if (n >= 8) break;
        names[n++] = d->displayName;
    }
    int cursor = warpRow_;
    if (n > 0 && cursor >= n) cursor = 0;
    drawWarpPicker(fb, names, n, cursor);
}

void Game::drawRollbackPickerScreen(Framebuffer& fb) const {
    const int points[4] = {statPoints_[0], statPoints_[1], statPoints_[2],
                           statPoints_[3]};
    drawRollbackPicker(fb, points, rollbackRow_);
}

void Game::drawCacheYieldScreen(Framebuffer& fb) const {
    if (!cacheYieldCache_) return;
    const SpriteData* icons[2] = {nullptr, nullptr};
    for (int i = 0; i < cacheYieldItemCount_ && i < 2; ++i)
        icons[i] = cacheYieldItems_[i]
                       ? itemIcon(registry_, cacheYieldItems_[i]->id)
                       : nullptr;
    drawCacheYield(fb, *cacheYieldCache_, cacheYieldBits_, cacheYieldItems_,
                   icons, cacheYieldItemCount_);
}

void Game::drawBulkYieldScreen(Framebuffer& fb) const {
    if (!bulkYieldCache_) return;
    BulkYieldRow rows[16];
    int n = static_cast<int>(bulkYieldTally_.size());
    if (n > 16) n = 16;
    for (int i = 0; i < n; ++i) {
        rows[i].def = bulkYieldTally_[i].def;
        rows[i].icon = bulkYieldTally_[i].def
                           ? itemIcon(registry_, bulkYieldTally_[i].def->id)
                           : nullptr;
        rows[i].count = bulkYieldTally_[i].count;
    }
    drawBulkYield(fb, *bulkYieldCache_, bulkYieldCachesOpened_, bulkYieldBits_, rows, n,
                 bulkYieldRow_);
}

void Game::drawStacker(Framebuffer& fb) const {
    drawStackerBoard(fb, stacker_, model_.fragmentation());
}

void Game::drawProcess(Framebuffer& fb) const {
    if (!processResolved_) {
        drawMaintProcess(fb, maintKind_,
                         static_cast<float>(processBeat_) / kProcessBeats);
        return;
    }
    // A PLAYED defrag reports what its own board was worth and can come up short without
    // failing; a rolled or bought one reports the fixed reduction it either got or missed.
    if (maintKind_ == MaintKind::Defrag && defragVariant_ == kDefragVariantStacker)
        drawMaintOutcome(fb, maintKind_,
                         maintSuccess_ ? MaintOutcome::Cleaned : MaintOutcome::Partial,
                         stackerFragRemoved_);
    else
        drawMaintOutcome(fb, maintKind_,
                         maintSuccess_ ? MaintOutcome::Cleaned : MaintOutcome::Failed,
                         maintKind_ == MaintKind::Defrag ? kDefragReduction : kAvReduction);
}

void Game::drawEvolve(Framebuffer& fb) const {
    // Cinematic phase from the beat counter: 0 hold (old) · 1 flash · 2 reveal.
    const int phase = evolveBeat_ < kEvoHoldBeats ? 0
                    : evolveBeat_ < kEvoHoldBeats + kEvoFlashBeats ? 1 : 2;
    const SpriteData* from = pet_ ? registry_.creatureSprite(*pet_) : nullptr;
    const SpriteData* to = evolveTo_ ? registry_.creatureSprite(*evolveTo_) : nullptr;
    drawEvolveModal(fb, from, to, evolveTo_ ? evolveTo_->displayName : "",
                    evolveTo_ ? evolveTo_->stage : Stage::Process, phase, beat_);
}

void Game::drawCSF(Framebuffer& fb) const {
    // The heaviest modal in the game: the dying pet corrupts out under a
    // FX_CRITICAL_FAIL crash overlay. B is gated behind the crash hold (csfRevealed).
    const SpriteData* pet = pet_ ? registry_.creatureSprite(*pet_) : nullptr;
    drawCSFModal(fb, pet, csfRevealed(), beat_);
}

}  // namespace mal
