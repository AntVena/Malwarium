#include "core/app/game.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "core/app/game_internal.h"   // levelStatName()
#include "tunables.h"
#include "core/content/areas/area_defs.h"   // area() — the place a walk is happening in
#include "core/content/content_homes.h"     // sceneForCreature — where a pet lives
#include "core/content/content_tables.h"
#include "core/render/camo.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/palette.h"
#include "core/render/scenes.h"
#include "core/render/sprite.h"
#include "core/ui/arch_screen.h"
#include "core/ui/carousel.h"
#include "core/ui/cfg_screen.h"
#include "core/ui/combat_screen.h"
#include "core/ui/expl_screen.h"
#include "core/ui/shibboleth_screen.h"
#include "core/ui/items_screen.h"
#include "core/ui/maint_screen.h"
#include "core/ui/modals.h"
#include "core/ui/mods_screen.h"
#include "core/ui/stat_screen.h"
#include "core/ui/train_screen.h"
#include "core/ui/widgets.h"
#include "core/ui/worm_replicas.h"
#include "generated/assets.h"

namespace mal {

// --- STAT's flowed prose pages ---------------------------------------------

std::vector<ProseRow> Game::statTierRows() const {
    // TOTAL points, not earned ones: an Epic dish's off-level grant counts toward a rung
    // exactly as an earned point does (applyLevelStatPoints resolves the tiers from the
    // same sum), so the page has to read the stat the way the fight does.
    int points[kLevelStatCount];
    for (int i = 0; i < kLevelStatCount; ++i) points[i] = totalStatPoint(i);
    return buildTierRows(points);
}

std::vector<ProseRow> Game::statLoadoutRows() const {
    return buildLoadoutRows(registry_, moveLoadout_, loadout_,
                            pet_ ? pet_->stage : Stage::BootSector, inEggPhase());
}

std::vector<BuffRow> Game::statBuffRows() const {
    const uint32_t backupRemainMs =
        backupShieldArmed() ? (backupShieldUntilMs_ - lifetimeUptimeMs()) : 0;
    return buildBuffRows(registry_, mistakeShieldActive_, forceTrojanDivert_,
                         backupShieldArmed(), backupRemainMs, deepWebDepthMultiplier_,
                         pendingDeepWebStartDepth_ != -1,
                         pendingDeepWebStartDepth_ == kDeepWebStartDepthUseBest,
                         pendingDeepWebStartDepth_, evolveBranchOverride_,
                         evolveSoakFactor_, evolveHold_, upgrades_);
}

Game::StatScrollSpan Game::statScrollSpan() const {
    if (!pet_) return {0, 0};
    if (statPage_ == 1) {
        const std::vector<ProseRow> rows = statTierRows();
        return {tierRowsFitting(rows, statScroll_), static_cast<int>(rows.size())};
    }
    if (statPage_ == 2) {
        const std::vector<ProseRow> rows = statLoadoutRows();
        return {loadoutRowsFitting(rows, statScroll_), static_cast<int>(rows.size())};
    }
    if (statPage_ == 3) {
        const std::vector<BuffRow> rows = statBuffRows();
        return {buffRowsFitting(rows, statScroll_), static_cast<int>(rows.size())};
    }
    return {0, 0};
}

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
        case Nav::ArcadeResult: drawArcadeOutcome(fb); break;
        case Nav::Combat: drawCombatScreen(fb); break;
        case Nav::Tourney: drawTourney(fb); break;
        case Nav::ExploreControl:
            // The A+C control overlay floats over the idle habitat.
            drawHabitat(fb, -1);
            drawExploreControl(fb, exploreCtlRow_, !heldWarpKeys().empty(),
                               autoProgress_);
            break;
        case Nav::Encounter: drawEncounterScreen(fb); break;
        case Nav::Wifi: drawWifiScreen(fb); break;
        case Nav::ShibbolethHail: drawShibbolethHailScreen(fb); break;
        case Nav::Shibboleth: drawShibbolethScreen(fb); break;
        case Nav::ShibbolethVerdict: drawShibbolethVerdictScreen(fb); break;
        case Nav::Shop: drawShopScreen(fb); break;
        case Nav::ModShop: drawShopScreen(fb); break;
        case Nav::WarpPicker: drawWarpPickerScreen(fb); break;
        case Nav::RollbackPicker: drawRollbackPickerScreen(fb); break;
        case Nav::CacheYield: drawCacheYieldScreen(fb); break;
        case Nav::BulkYield: drawBulkYieldScreen(fb); break;
        case Nav::PostEncounter: drawPostEncounterScreen(fb); break;
        case Nav::ModalLineSelect: drawLineSelect(fb); break;
        case Nav::Decryption: drawDecryption(fb); break;
        case Nav::Cryptogram: drawCryptogram(fb); break;
        case Nav::ModalEggPick: drawEggPick(fb); break;
        case Nav::Isolation: drawIsolation(fb); break;
        case Nav::Chroma: drawChroma(fb); break;
        case Nav::ModalHatchReveal: drawHatchReveal(fb); break;
        case Nav::ModalFeeding:
            drawFeedingModal(fb, pet,
                             feedItem_ ? itemIcon(registry_, feedItem_->id) : nullptr,
                             feedItem_, model_, feedBefore_, feedBeat_, fxBeat_);
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

SceneId Game::habitatScene() const {
    // The operator's own choice first — this is their pet's home and they are allowed an
    // opinion about it. AUTO (SceneId::None) hands the question back to the creature:
    // which place a pet belongs in follows from its line and how it gets around
    // (content/content_homes.h), so an evolution walks into a new one with nothing
    // written down.
    if (backgroundPick_ != SceneId::None) return backgroundPick_;
    return pet_ ? sceneForCreature(*pet_) : SceneId::None;
}

SceneId Game::stageScene() const {
    // A fight inside an area happens THERE — the walk is the reason the operator is
    // looking at that place at all, and the stage agreeing with the EXPL row above it
    // is most of what makes an area feel like somewhere rather than a difficulty.
    if (exploreActive_ && exploreSector_ >= 0 && exploreSector_ < kAreaCount) {
        const SceneId s = area(exploreSector_).scene;
        if (s != SceneId::None) return s;
    }
    // Everything else — a duel, an arcade bout, the endless dive, an area whose place
    // is not authored yet — is fought where the pet lives, chosen background and all. A
    // fight always has a floor and it may as well have a horizon.
    //
    // The area WINS over the operator's pick on purpose. A background is an opinion
    // about home; an area is a fact about where the walk is, and a screen that let a
    // prize overwrite it would be one that could no longer tell you where you are.
    return habitatScene();
}

namespace {

// The explore status stack's text column: the left inset every line in it shares, and
// the width the flavor line wraps inside. One pair of numbers, because the height the
// stack reports (exploreStatusLines) and the width it is drawn to have to be the same
// answer — a line measured against one width and drawn to another is a block whose
// reported height is wrong.
constexpr int kExploreStatusX = 8;
constexpr int kExploreStatusW = kActiveW - 2 * kExploreStatusX;

// A flavor line gets two lines at most. A third would run into the achievement plate,
// which starts at kLivingTop + 42 (drawAchievementBanner) — and every line the
// resolvers compose fits two, which test_explore_flavor_lines_fit holds them to.
constexpr int kExploreFlavorLines = 2;

}  // namespace

int Game::exploreStatusLines() const {
    if (!exploreActive_) return 0;
    if (!exploreFlavor_[0]) return 2;             // the badge + the Bandwidth readout
    const int wrapped = textWrapLines(exploreFlavor_, kExploreStatusW);
    return 2 + (wrapped < 1 ? 1
                            : (wrapped > kExploreFlavorLines ? kExploreFlavorLines
                                                             : wrapped));
}

void Game::drawHabitat(Framebuffer& fb, int cursor) const {
    // The BACKGROUND pass. Composed against the shelf a resting pet's feet sit on, which
    // the bottom carousel track then covers — so a habitat backdrop's identity has to
    // live in its horizon band and its silhouette, and never in its floor.
    if (!drawScene(fb, habitatScene(), beat_, sceneGround(kLivingBottom)))
        fb.clear(palColor(Pal::PAPER));

    const SpriteData* pet = pet_ ? registry_.creatureSprite(*pet_) : nullptr;
    if (pet) {
        // An authored clip plays its full row in order; otherwise fall back to the
        // breathe/blink heuristic on row 0. A creature that authored a "walk" takes it
        // for exactly the beats the wander below is actually moving the anchor, so the
        // legs and the drift are one motion rather than two that happen to overlap;
        // one that authored none simply keeps breathing while it slides, which is what
        // every single-row sheet does.
        const AnimClip* clip = nullptr;
        if (pet_) {
            if (petWander_.travelling()) clip = pet_->clip("walk");
            if (!clip) clip = pet_->clip("idle");
        }
        const int row = clip ? clip->row : 0;
        const int frame = clip ? clip->frameAt(beat_) : idleFrame(*pet, beat_);
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
        // Turned to the way it is actually going. A sheet with no declared facing is
        // never mirrored, so this only moves the creatures the wander was already
        // walking backwards — the ones drawn in profile rather than in §2's
        // turned-to-the-viewer pose (assets/CREATURE_VISUAL_RULES.md). The habitat has
        // no seat and no opponent, so unlike the combat stage the direction comes off
        // the trip itself.
        const bool mirror = spriteMirrorToFace(*pet, petWander_.headingRight());
        // Resting colour: a line that wears borrowed colours spends a few seconds now
        // and then in another family's palette (core/model/idle_camo.h). The level and
        // the art are the tick's; this ranks that art into the ladder the creature is
        // repainted through, exactly as the combat screen does. Every other pet, and
        // this one between drifts, takes the plain blit and pays nothing.
        const CamoRamp drift =
            idleCamoWorn_ ? camoRampFrom(*idleCamoWorn_) : CamoRamp{};
        if (petCamo_.level() > 0 && !drift.empty())
            drawSpriteCamo(fb, *pet, frame, petX, petY, kScaleNum, kScaleDen, drift,
                           petCamo_.level(), /*flashColor=*/0, /*flashAmt=*/0, row,
                           /*from=*/nullptr, mirror);
        else
            drawSpriteUpscaled(fb, *pet, frame, petX, petY, kScaleNum, kScaleDen, row,
                               mirror);
    }

    // The Worm line's copies, at home. In a fight the board is Combatant::wormReplicas
    // and it stands in RANK between its parent and the enemy; here there is no enemy
    // and no combat state at all, so the habitat asks the model how many copies the
    // family currently has (idleCompanionCount) and simply lets them mill about.
    //
    // Each takes a seat one slot-width out from the parent, alternating sides so the
    // family stays balanced around it, and then wanders off its OWN stream from there
    // — the seat is where a copy belongs, not where it stands. The seat plus a full
    // drift can reach past the bezel, so the centre is clamped to whatever keeps the
    // whole glyph on canvas: a copy pressed against the edge still reads as a copy,
    // and half a copy does not. The glyphs alternate attack/defend for the same
    // reason the line's slots do — a board of nothing but teeth is not what a worm is.
    for (int i = 0; i < idleCompanionCount(); ++i) {
        const SpriteData& glyph = (i & 1) ? ASSET_SPR_WORM_REPLICA_DEFEND
                                          : ASSET_SPR_WORM_REPLICA_ATTACK;
        const int w = glyph.frameW * kScaleNum / kScaleDen;
        const int seat = kReplicaSlotW * (i / 2 + 1) * ((i & 1) ? -1 : 1);
        int cx = kActiveW / 2 + seat + logicalToActive(companionWander(i).offsetX());
        cx = std::max(w / 2, std::min(kActiveW - w / 2, cx));
        drawReplica(fb, glyph, kReplicaIdleFrame + (beat_ & 1), cx, kLivingBottom);
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
            // egg open into the hatch cinematic. A distinct glyph + "A+C" chord label
            // carry the meaning with no colour channel. It only ever appears in the
            // home stretch of the clock, so an egg is never advertised a crack it
            // can't take.
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
        // The rest of the USB port. The branch override is ONE slot, so it draws one
        // marker either way; the soak names the weaker of its two devices, since which
        // one armed it isn't tracked (the factor is) and both share the family glyph.
        if (evolveBranchOverride_ == BranchOverride::Bad) marker("bad_usb", "[BAD]");
        else if (evolveBranchOverride_ == BranchOverride::Good) marker("signed_usb", "[SGND]");
        if (evolveSoakFactor_ > 1) marker("sandbox_usb", "[SOAK]");
        // The hold gets a badge over any of them: it is why STAT's EVOLVE row reads MAX
        // on a pet that plainly has a successor, so the habitat has to say it is there.
        if (evolveHold_) marker("halt_usb", "[HELD]");
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
                drawText(fb, kExploreStatusX, bwLineY, bw, palColor(Pal::CALM));
            } else {
                std::snprintf(bw, sizeof(bw), "BW 0/%d LOW", bandwidthMax());
                drawText(fb, kExploreStatusX, bwLineY, bw, palColor(Pal::WARN));
            }
            // Wrapped, not clipped: a flavor line names the item it found, and the
            // longest of those is wider than the column. Drawn with plain text it lost
            // its tail with no ellipsis — including the half that says where the thing
            // it just gave you has gone.
            if (exploreFlavor_[0])
                drawTextWrapped(fb, kExploreStatusX, bwLineY + kFontH + 2,
                                kExploreStatusW, exploreFlavor_, palColor(Pal::INK_DIM),
                                kFontH + 2, kExploreFlavorLines);
        }
    }

    // The achievement announcement, over everything else in the living area (it is
    // transient, and the pet is still there underneath when it clears).
    drawAchievementBanner(fb);

    // Grey out the care/combat slots while the pet is still an egg. EXPL's globe turns
    // while AUTO-PROGRESS is armed — the walk is running itself in the background, and
    // the shelf is the only screen that's always up to say so.
    unsigned lockMask = 0, spinMask = 0;
    for (int i = 0; i < kCarouselSlots; ++i) {
        const SubmenuId id = carouselSlots()[i].id;
        if (slotLocked(id)) lockMask |= (1u << i);
        if (id == SubmenuId::Expl && autoProgress_ && exploreActive_)
            spinMask |= (1u << i);
    }
    drawCarousel(fb, cursor, uiMode_, beat_, lockMask, spinMask);
}

void Game::drawAchievementBanner(Framebuffer& fb) const {
    // The whole feedback channel for an unlock: three centred lines on a filled band,
    // shown on the home screen for kAchBannerMs. Modelled on the combat verdict banner —
    // a solid TRACK plate under INK text carries in grayscale on shape alone, no colour
    // doing any of the work.
    const AchievementDef* d = achBanner();
    if (!d) return;
    // The one announcement that is not just news: an unlock that put a NEW KIND OF EGG
    // in the hatch menu. It gets the held plate — a different colour, its own copy, and
    // no deadline — because it is the only banner with an instruction in it, and a
    // three-second window is no way to deliver one.
    const EggLineDef* eggLine = achBannerCount_ == 1 ? achEggLineUnlocked(*d) : nullptr;
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
    // The held plate inverts: a light NOTICE_HOLD ground with PAPER text on it, against
    // the ordinary banner's dark TRACK with INK text. That is the part that survives
    // desaturation — the two plates differ in VALUE, not only in hue, so a held one is
    // recognisable as a different kind of thing before a word of it is read.
    const bool held = eggLine != nullptr;
    const Rgb565 plate = held ? palColor(Pal::NOTICE_HOLD) : palColor(Pal::TRACK);
    const Rgb565 body = held ? palColor(Pal::PAPER) : palColor(Pal::INK);
    const Rgb565 quiet = held ? palColor(Pal::PAPER) : palColor(Pal::INK_DIM);
    fb.fillRect(0, bandY, kActiveW, bandH, plate);
    // A 1px lid and sill: the band has to read as a plate laid over the habitat rather
    // than a hole in it, and an edge is the only cue that survives desaturation.
    fb.fillRect(0, bandY, kActiveW, 1, quiet);
    fb.fillRect(0, bandY + bandH - 1, kActiveW, 1, quiet);

    // Every line on this plate is content — an achievement name, an egg line, an item
    // — so none of them can be centred and left to run: a name that outgrows the canvas
    // would be cut at BOTH ends, which is the one failure that reads as a rendering
    // fault rather than as long copy. Centred while it fits, scrolled in the margins
    // once it doesn't.
    // The plate is full-bleed, so a centred line may use the whole canvas — the held
    // plate's own instruction already spans it. Only what does not fit gets pulled in
    // to the margins to scroll.
    auto plateLine = [&](int ly, const char* s, Rgb565 col) {
        const int w = textWidth(s);
        if (w <= kActiveW) { drawText(fb, (kActiveW - w) / 2, ly, s, col); return; }
        drawTextMarquee(fb, kMargin, ly, kActiveW - 2 * kMargin, s, col, beat_, true);
    };

    const char* kicker = held ? "NEW EGG LINE" : "ACHIEVEMENT";
    plateLine(bandY + 5, kicker, quiet);

    // A burst says how many first and names one of them, so a long back-catalogue drop
    // still tells the player something specific about what they got.
    char line[40];
    if (achBannerCount_ > 1)
        std::snprintf(line, sizeof(line), "%d UNLOCKED", achBannerCount_);
    else if (held)
        // The LINE, not the achievement that earned it: "Hash Collision" is the thing
        // they just did, and "Metamorphic" is the thing they can now do about it. Only
        // one of those belongs on a banner whose whole job is to point at the hatch.
        std::snprintf(line, sizeof(line), "%s", eggLine->displayName);
    else
        std::snprintf(line, sizeof(line), "%s", d->displayName);
    plateLine(bandY + 15, line, body);

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
    if (held)
        // Where to go, and how to make this go away — the two things a held plate has to
        // say. The rewards are still paid; they are simply not what this banner is for.
        std::snprintf(reward, sizeof(reward), "LAY ONE NEXT HATCH - ANY KEY");
    else if (achBannerCount_ > 1)
        std::snprintf(reward, sizeof(reward), "%s +MORE", d->displayName);
    else if (rewardItem && rewardBits > 0) {
        // Two rewards on one line is the only combination that outgrows the plate —
        // "+400 BITS + COMMENDATION CACHE" is 30 characters against a 28-character
        // canvas. The currency abbreviates to the B the shop prices already use, which
        // buys back the three characters the pair needs.
        std::snprintf(reward, sizeof(reward), "+%d BITS + %s", rewardBits, rewardItem);
        if (textWidth(reward) > kActiveW)
            std::snprintf(reward, sizeof(reward), "+%d B + %s", rewardBits, rewardItem);
    } else if (rewardItem)
        std::snprintf(reward, sizeof(reward), "+%s", rewardItem);
    else if (rewardBits > 0)
        std::snprintf(reward, sizeof(reward), "+%d BITS", rewardBits);
    if (reward[0])
        plateLine(bandY + 25, reward, held ? body : palColor(Pal::CALM));
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
                // 6 pages: 0 pet vitals (the landing) · 1 the investment ladder —
                // which stat tiers this pet holds and what the next costs · 2 the
                // equipped loadout (moves + mods, WITH their effect text) ·
                // 3 currently-armed item buffs · 4 the pet's own species lore ·
                // 5 audit log. A cycles; C backs out.
                if (statPage_ == 0)
                    drawStatScreen(fb, model_, pet_->displayName, pet_->stage,
                                   generation_, combatLevel_, combatXp_,
                                   xpToNextLevel(), beat_, hasNextEvolution(),
                                   evolveRemainMs());
                else if (statPage_ == 1)
                    drawTiersScreen(fb, statTierRows(), statScroll_, beat_);
                else if (statPage_ == 2)
                    drawLoadoutScreen(fb, statLoadoutRows(), statScroll_, beat_);
                else if (statPage_ == 3)
                    drawBuffsScreen(fb, statBuffRows(), statScroll_, beat_);
                else if (statPage_ == 4)
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
            const PetLifetimeGates gates = petLifetimeGates();
            drawItemsList(fb, rows, listRow_, lockoutItemsContext_, beat_, itemFilter_,
                         itemTabsUnlocked(), itemPickerUnlocked(), &gates);
            break;
        }
        case SubmenuId::Maint:
            drawMaintList(fb, model_, listRow_, beat_);
            break;
        case SubmenuId::Cfg:
            drawCfgList(fb, listRow_, hackerTag_, equippedTitleName(), radioOwner_);
            break;
        case SubmenuId::Arch:
            if (archScreen_ == ArchScreen::Picker) {
                drawArchPicker(fb, buildArchPickerRows(registry_, pet_, rack_, records_),
                               archPickRow_, static_cast<int>(rack_.size()), rackSlots());
            } else {
                // The group's own name is the list's title, which is the whole reason
                // the picker exists: the header says which shelf this is.
                const auto tiles = buildArchPickerRows(registry_, pet_, rack_, records_);
                const char* title = "ARCH";
                for (const ArchPickRow& t : tiles)
                    if (t.group == archGroup_) { title = t.label; break; }
                drawArchList(fb, archRows(), title, listRow_,
                             static_cast<int>(rack_.size()), rackSlots());
            }
            break;
        case SubmenuId::Mods: {
            const Stage st = pet_ ? pet_->stage : Stage::BootSector;
            if (loadoutTab_ == LoadoutTab::Mods) {
                drawModsList(fb, registry_, loadout_, listRow_, beat_);
            } else if (loadoutTab_ == LoadoutTab::Moves) {
                // Each slot's stamped required kind, for the row tags.
                MoveDef::Kind sk[kMaxMoveSlots];
                for (int i = 0; i < kMaxMoveSlots; ++i) sk[i] = slotRequiredKind(i);
                drawLoadout(fb, registry_, moveLoadout_, st, trainRow_, sk, beat_);
            } else {
                drawLoadoutHub(fb, loadout_, moveLoadout_, st, loadoutHubRow_, beat_);
            }
            break;
        }
        case SubmenuId::Games:
            drawArcade(fb);
            break;
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
            v.tourneyRunning = tourneyRunning();
            v.tourneyAlive = tourneyAliveCount(tourneyAlive_);
            v.tourneyRound = tourneyRound_;
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
                               beat_, lifetimeMark(*detailItem_, petLifetimeGates()));
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
            const bool rackFull = static_cast<int>(rack_.size()) >= rackSlots();
            // NEW EGG is an action rather than a row, so its L3 is its own screen: the
            // one place that says what laying an egg costs before it is laid.
            if (archOnNewEgg()) {
                drawArchNewEgg(fb, pet_, pet_ && rackFull, archConfirm_,
                               archConfirmChoice_);
                break;
            }
            const ArchRow row = archFocusedRow();
            if (row.kind == ArchRow::Kind::Record) {
                if (row.index >= 0 && row.index < static_cast<int>(records_.size()))
                    drawArchRecordDetail(fb, registry_, records_[row.index]);
                break;
            }
            const bool sellEnabled = row.def && row.def->stage == Stage::Daemon;
            drawArchRecord(fb, row.def, row.kind == ArchRow::Kind::Active, row.generation,
                           archAction_, sellEnabled, rackFull, archConfirm_,
                           archConfirmChoice_);
            break;
        }
        case SubmenuId::Mods:
            if (loadoutTab_ == LoadoutTab::Practise) {
                drawSimTier(fb, simTier_, kSimDummyTiers);
                break;
            }
            if (loadoutTab_ == LoadoutTab::Moves) {
                drawTrain(fb);
                break;
            }
            if (modDetail_ && modDetailId_) {
                const ModDef* md = registry_.mod(modDetailId_);
                if (md) {
                    const char* cur = loadout_.equipped(modSlot_);
                    drawModDetail(fb, registry_, loadout_, *md,
                                  cur && std::strcmp(cur, modDetailId_) == 0,
                                  modSlot_, modEquipLevel(*md),
                                  combatLevel_, pet_ ? pet_->line : nullptr,
                                  modStorageCap(), beat_);
                    break;
                }
            }
            drawModPicker(fb, registry_, loadout_, modSlot_, modPick_,
                          modConfirm_, modConfirmChoice_, modPendingId_, combatLevel_,
                          pet_ ? pet_->line : nullptr, beat_);
            break;
        case SubmenuId::Games:
            drawArcadeDetail(fb);
            break;
        default: break;
    }
}

void Game::drawTrain(Framebuffer& fb) const {
    if (trainScreen_ == TrainScreen::MoveDetail) {
        // The reader half of the picker (hold-B). A cursor that has somehow lost its
        // move falls back to the picker rather than drawing an empty page.
        if (const MoveDef* m = focusedPickerMove()) {
            const char* eq = moveLoadout_.equipped(moveSlot_);
            drawMoveDetail(fb, registry_, *m,
                           pet_ ? pet_->stage : Stage::BootSector,
                           eq && std::strcmp(eq, m->id) == 0, moveProseScroll_, beat_);
            return;
        }
    }
    drawMovePicker(fb, registry_, moveLoadout_, moveSlot_, movePick_,
                   moveConfirm_, moveConfirmChoice_, movePendingId_,
                   pet_ ? pet_->stage : Stage::BootSector,
                   pet_ ? pet_->line : nullptr,
                   slotRequiredKind(moveSlot_), moveShowAll_, beat_);
}

// Which art a CamoTarget names — the contract is on the declaration (game.h).
const SpriteData* Game::camoSpriteForTarget(const CamoTarget& t, const char* rivalSprite,
                                            Stage wearer) const {
    switch (t.source) {
        case CamoTarget::Source::Own:
            return nullptr;
        case CamoTarget::Source::Rival:
            return rivalSprite ? registry_.sprite(rivalSprite) : nullptr;
        case CamoTarget::Source::Line:
            break;
    }
    const CreatureLine* cl = t.line ? registry_.creatureLine(t.line) : nullptr;
    if (!cl || cl->count <= 0) return nullptr;
    // The line AT THE WEARER'S OWN TIER: the highest row that has not passed the wearer's
    // stage, falling back to the lowest row for a line whose ladder starts above it. One
    // pass, and the first row of a tie wins, so a branching line answers with the same
    // creature every time it is asked.
    const CreatureDef* pick = &cl->rows[0];
    for (int i = 1; i < cl->count; ++i) {
        const CreatureDef& r = cl->rows[i];
        if (r.stage > wearer) continue;
        if (pick->stage > wearer || r.stage > pick->stage) pick = &r;
    }
    return registry_.creatureSprite(*pick);
}

// The rotation rule is on the declaration (game.h).
const SpriteData* Game::idleCamoSprite(int slot) const {
    if (!pet_) return nullptr;
    const std::vector<const CreatureLine*> lines = registry_.allCreatureLines();
    int others = 0;
    for (const CreatureLine* l : lines)
        if (!pet_->line || std::strcmp(l->id, pet_->line) != 0) ++others;
    if (others <= 0) return nullptr;         // a roster of one family has nobody to be
    const int want = ((slot % others) + others) % others;
    int i = 0;
    for (const CreatureLine* l : lines) {
        if (pet_->line && std::strcmp(l->id, pet_->line) == 0) continue;
        if (i++ != want) continue;
        // Through the same resolver the fight uses, so a drifting pet at home and a
        // channelling one in a fight reach for the same creature: that family's row at
        // the drifter's OWN tier, never its hatchling.
        return camoSpriteForTarget({CamoTarget::Source::Line, l->id},
                                   /*rivalSprite=*/nullptr, pet_->stage);
    }
    return nullptr;
}

namespace {
// The flock as the DRAW sees it (SwarmView, core/render/swarm.h). The copy is the layering
// rule rather than an oversight: core/render never reaches into core/model, so a renderer
// is handed a value that owns its marks — the same call CamoRamp makes. It is a couple of
// hundred bytes on a repaint that is already composing a whole panel.
SwarmView swarmViewOf(const Flock& f) {
    SwarmView v;
    v.n = f.count() < kSwarmMarksMax ? f.count() : kSwarmMarksMax;
    for (int i = 0; i < v.n; ++i) {
        v.marks[i].x = static_cast<int16_t>(f.x(i));
        v.marks[i].y = static_cast<int16_t>(f.y(i));
        v.marks[i].vx = static_cast<int16_t>(f.vx(i));
        v.marks[i].vy = static_cast<int16_t>(f.vy(i));
    }
    v.cx = f.centreX();
    v.cy = f.centreY();
    v.spread = f.spread();
    return v;
}
}  // namespace

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
        sides.localIsEnemySide = combatLocalIsEnemySide();
    }
    // An arena bout is the other pet-vs-pet fight, and the other one with no way out —
    // so it borrows the duel's caption for the opposite seat and drops the RUN hint.
    if (combatCaller_ == CombatCaller::Tourney) sides.rivalLabel = "RIVAL";
    sides.canRun = !duel && combatCaller_ != CombatCaller::Tourney;

    // The beaten rival's outro, over the beats finishCombat() holds the result for.
    // WHICH fights get one is combatOutroEligible() — shared with the tick, which has
    // to hold the auto-dismiss back for the same fights. This side only picks which of
    // the two dissolves plays.
    CombatOutro outro;
    if (combatOutroEligible()) {
        outro.kind = rivalFieldsUnknownMove() ? CombatOutro::Kind::Absorb
                                              : CombatOutro::Kind::Shred;
        outro.beat = fxBeat_;
    }
    // What the KIT page marks as worth winning — the same mask the outro's dissolve is
    // the yes/no of, so the panel and the last beat of the fight cannot disagree.
    RivalPrizes prizes;
    prizes.mask = rivalTeachableMoveMask();
    // What the pet is currently wearing (FX_CAMO). The level and both sprites are standing
    // state the tick maintains; this draw only ranks each one's colours into a ladder.
    CombatCamo camo;
    camo.level = combatCamoLevel_;
    camo.ramp = combatCamoWorn_ ? camoRampFrom(*combatCamoWorn_) : CamoRamp{};
    camo.leaving = combatCamoLeaving_ ? camoRampFrom(*combatCamoLeaving_) : CamoRamp{};
    // A rival with no sheet that is nonetheless a creature: the area guardian, drawn as
    // its flock into the seat the stage reserved (FX_SWARM). The same body the pet was
    // talking to on the hail — re-homed into the fighter's cell when the fight opened.
    SwarmView rivalSwarm;
    const bool swarmRival = combat_.enemy().isSwarm && !sides.localIsEnemySide;
    if (swarmRival) rivalSwarm = swarmViewOf(guardianFlock_);
    drawCombat(fb, combat_, ps, es, beat_, combatAnimBeat_, combatHitBeat_,
               combatStatsPage_, sides, outro, prizes, camo, stageScene(),
               swarmRival ? &rivalSwarm : nullptr);
}

void Game::drawEncounterScreen(Framebuffer& fb) const {
    const SpriteData* es = registry_.sprite(encounterEnemy_.spriteName);
    drawEncounterIntro(fb, encounterEnemy_.name, encounterEnemy_.diffPips,
                       encounterEnemy_.level, es, encounterSinkhole_,
                       encounterChoice_, beat_);
}

void Game::drawWifiScreen(Framebuffer& fb) const {
    // The discovery beat's reward, as the thing the pet does to the network glyph.
    // Untouched is not "nothing happened" — it is the pet declining its own home turf,
    // which is what the TIRED OF line says.
    //
    // The BANNER is the same fact stated in words, which is why it is derived here from
    // netDiscovery_ rather than being a constant in the screen: a fixed "NEW WI-FI
    // NETWORK" over a repeat sighting — or over a dry queue with nothing found at all —
    // is the screen contradicting its own two flavor lines.
    WifiAbsorb absorb = WifiAbsorb::None;
    const char* banner = "QUIET AIR";
    switch (netDiscovery_) {
        case NetDiscovery::New:
            absorb = WifiAbsorb::Whole; banner = "NEW WI-FI NETWORK"; break;
        case NetDiscovery::Fond:
            absorb = WifiAbsorb::Nibble; banner = "A KNOWN NETWORK"; break;
        case NetDiscovery::HomeTurf:
            absorb = WifiAbsorb::Untouched; banner = "HOME TURF"; break;
        case NetDiscovery::None: break;
    }
    drawWifiEvent(fb, explSectorName(exploreSector_), banner, wifiFlavor_,
                  netDiscoveryFlavor_,
                  pet_ ? registry_.creatureSprite(*pet_) : nullptr, absorb,
                  beat_, fxBeat_);
}

void Game::drawShibbolethScreen(Framebuffer& fb) const {
    char riddle[kRiddleBodyLines * kRiddleBodyCols + 16];
    shibbolethRiddleText(riddle, sizeof(riddle));
    char reply[kRiddleReplies][40];
    const char* rows[kRiddleReplies];
    for (int i = 0; i < kRiddleReplies; ++i) {
        shibbolethReplyText(i, reply[i], sizeof(reply[i]));
        rows[i] = reply[i];
    }
    char greeting[kRiddleBodyCols * 2 + 8];
    shibbolethGreeting(greeting, sizeof(greeting));
    const float held = static_cast<float>(exploreEventBeat_) / kShibbolethReplyHoldBeats;
    drawShibboleth(fb, guardianName(), guardianDemeanour(), greeting, riddle, rows,
                   shibRow_, cantSigils_, held);
}

void Game::drawShibbolethHailScreen(Framebuffer& fb) const {
    char greeting[kRiddleBodyCols * 2 + 8];
    shibbolethGreeting(greeting, sizeof(greeting));
    drawShibbolethHail(fb, guardianName(), guardianDemeanour(), greeting,
                       swarmViewOf(guardianFlock_), cantSigils_, shakesUnspent());
}

void Game::drawShibbolethVerdictScreen(Framebuffer& fb) const {
    // The content layer's outcome mapped onto the screen's own four. The two enums are
    // kept apart on purpose — an area row is authored against GuardianOutcome, and the
    // renderer only ever needs the banner word and whether it is bad news.
    ShibbolethVerdictKind kind = ShibbolethVerdictKind::Pleased;
    switch (shibbolethOutcome()) {
        case GuardianOutcome::Pleased:    kind = ShibbolethVerdictKind::Pleased; break;
        case GuardianOutcome::Displeased: kind = ShibbolethVerdictKind::Displeased; break;
        case GuardianOutcome::Affront:    kind = ShibbolethVerdictKind::Refused; break;
        case GuardianOutcome::Boon:       kind = ShibbolethVerdictKind::Boon; break;
    }
    char speech[kRiddleBodyCols * 2 + 8];
    shibbolethOutcomeSpeech(speech, sizeof(speech));
    drawShibbolethVerdict(fb, guardianName(), kind, shibbolethOutcomeSeen(), speech,
                          swarmViewOf(guardianFlock_), shibbolethVerdictLine(),
                          shibbolethFlavor(), cantSigils_, shibbolethVerdictFights());
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
        rows[i].held = shopListingHeld(i);
        rows[i].heldCap = shopListingHeldCap(i);
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
                  bulkYieldRow_, beat_);
}

void Game::drawStacker(Framebuffer& fb) const {
    if (gameBriefOpen_) { drawGameBrief(fb); return; }
    drawStackerBoard(fb, stacker_, model_.fragmentation(), arcadeRun_);
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
