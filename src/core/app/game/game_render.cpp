#include "core/app/game.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/content/areas/area_defs.h"   // area(): the place a walk is happening in
#include "core/content/content_homes.h"     // sceneForCreature, where a pet lives
#include "core/content/content_tables.h"
#include "core/render/camo.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/glitch.h"
#include "core/render/cloak.h"
#include "core/render/rain.h"
#include "core/render/palette.h"
#include "core/render/scenes.h"
#include "core/render/sprite.h"
#include "core/ui/carousel.h"
#include "core/ui/cfg_screen.h"
#include "core/ui/combat_screen.h"
#include "core/ui/expl_screen.h"
#include "core/ui/shiboleet_screen.h"
#include "core/ui/items_screen.h"
#include "core/ui/maint_screen.h"
#include "core/ui/modals.h"
#include "core/ui/mods_screen.h"
#include "core/ui/stat_pages.h"    // StatSources: what the STAT reader draws from
#include "core/ui/train_screen.h"
#include "core/ui/widgets.h"
#include "generated/assets.h"

namespace mal {

// --- Render ----------------------------------------------------------------

void Game::render(Framebuffer& fb) const {
    const SpriteData* pet = pet_.creature ? registry_.creatureSprite(*pet_.creature) : nullptr;
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
        case Nav::Fragger: drawFragger(fb); break;
        case Nav::ArcadeResult: arcadeScreen_.drawResult(fb, *this); break;
        case Nav::Combat: fightScreen_.draw(fb, *this); break;
        case Nav::Tourney: arena_.draw(fb, *this); break;
        case Nav::Story: storyReader_.draw(fb, *this); break;
        case Nav::TipCard: tipCardScreen_.draw(fb, *this); break;
        case Nav::AwayDigest: awayDigest_.draw(fb, *this); break;
        case Nav::StoryArchive: chapters_.draw(fb, *this); break;
        case Nav::ExploreControl: exploreControl_.draw(fb, *this); break;
        case Nav::Encounter: encounter_.draw(fb, *this); break;
        case Nav::Wifi: wifiScreen_.draw(fb, *this); break;
        case Nav::ShiboleetHail: shiboleet_.drawHail(fb, *this); break;
        case Nav::Shiboleet: shiboleet_.drawRiddle(fb, *this); break;
        case Nav::ShiboleetVerdict: shiboleet_.drawVerdict(fb, *this); break;
        case Nav::Shop:
        case Nav::ModShop: shop_.draw(fb, *this); break;
        case Nav::WarpPicker: warpPicker_.draw(fb, *this); break;
        case Nav::ItemOffer: drawItemOfferScreen(fb); break;
        case Nav::RollbackPicker: statPicker_.drawRollback(fb, *this); break;
        case Nav::RepartitionPicker: statPicker_.drawRepartition(fb, *this); break;
        case Nav::CacheYield: vault_.drawReveal(fb, *this); break;
        case Nav::BulkYield: vault_.drawBulkReveal(fb, *this); break;
        case Nav::PostEncounter: postEncounter_.draw(fb, *this); break;
        case Nav::ModalLineSelect: drawLineSelect(fb); break;
        case Nav::Decryption: drawDecryption(fb); break;
        case Nav::Cryptogram: drawCryptogram(fb); break;
        case Nav::ModalEggPick: drawEggPick(fb); break;
        case Nav::Isolation: drawIsolation(fb); break;
        case Nav::Chroma: drawChroma(fb); break;
        case Nav::ModalHatchReveal: drawHatchReveal(fb); break;
        case Nav::ModalFeeding: {
            FeedingView v;
            v.pet = pet;
            if (feeding_) {
                v.food = feeding_->item;
                v.foodIcon = itemIcon(registry_, feeding_->item->id);
                v.before = feeding_->before;
                v.beat = feeding_->beat;
            }
            v.fxBeat = fxBeat_;
            drawFeedingModal(fb, pet_.model, v);
            break;
        }
        case Nav::ModalEvolve: drawEvolve(fb); break;
        case Nav::ModalCSF: drawCSF(fb); break;
        case Nav::ModalLockout: {
            LockoutView v;
            v.pet = pet;
            if (pet_.armed.lockoutDeadlineMs > nowMs_) {
                v.secondsLeft = static_cast<int>((pet_.armed.lockoutDeadlineMs - nowMs_ + 999) / 1000);
                v.remainFrac =
                    static_cast<float>(pet_.armed.lockoutDeadlineMs - nowMs_) / kLockoutDurationMs;
            }
            v.focus = lockoutPayOption_ ? LockoutPath::Pay : LockoutPath::Feed;
            v.bitsCost = kLockoutBitsCost;
            v.canPay = op_.bits >= kLockoutBitsCost;
            v.hasFood = lockoutFoodHeld();
            v.failingLeftMs = failingLeftMs();
            v.yubiReady = yubiReady();
            v.beat = beat_;
            drawLockoutModal(fb, v);
            break;
        }
    }
}

void Game::drawLineSelect(Framebuffer& fb) const {
    // Line-select modal: full 224x224, no track/header chrome. Grayscale-
    // safe: the highlighted line carries BOTH a ">" cursor glyph (shape) and INK vs
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

    const char* hint = "A NEXT   B SELECT";
    drawText(fb, (kActiveW - textWidth(hint)) / 2, kActiveH - 26, hint,
             palColor(Pal::INK_DIM));
}

SceneId Game::habitatScene() const {
    // The operator's own choice comes first, because this is their pet's home. AUTO
    // (SceneId::None) hands the question back to the creature: which place a pet
    // belongs in follows from its line and how it gets around
    // (content/content_homes.h), so an evolution walks into a new one with nothing
    // written down.
    if (op_.backgroundPick != SceneId::None) return op_.backgroundPick;
    return pet_.creature ? sceneForCreature(*pet_.creature) : SceneId::None;
}

SceneId Game::stageScene() const {
    // A fight inside an area is staged in that area, so the stage matches the EXPL
    // row the walk started from.
    if (walk_.active && walk_.sector >= 0 && walk_.sector < kAreaCount) {
        const SceneId s = area(walk_.sector).scene;
        if (s != SceneId::None) return s;
    }
    // The dive is not an area and has no AreaDef to name its place on, but it is still
    // somewhere (the shaft), and the walk is as much a fact there as anywhere.
    if (inDeepWebDive()) return SceneId::DeepWebDive;
    // Everything else (a duel, an arcade bout, the crawl, an area whose place is not
    // authored yet) is fought where the pet lives, chosen background and all. A fight
    // always has a floor and it may as well have a horizon.
    //
    // The area WINS over the operator's pick on purpose. A background is an opinion
    // about home; an area is a fact about where the walk is, and a screen that let a
    // prize overwrite it would be one that could no longer tell you where you are.
    return habitatScene();
}

namespace {

// The explore status stack's text column: the left inset every line in it shares, and
// the width the flavor line wraps inside. One pair of numbers, because the height the
// stack reports (exploreStatusLines) and the width it is drawn to must match. A line
// measured at one width and drawn at another reports the wrong height.
constexpr int kExploreStatusX = 8;
constexpr int kExploreStatusW = kActiveW - 2 * kExploreStatusX;

// A flavor line gets two lines at most. A third would run into the achievement plate,
// which starts at kLivingTop + 42 (drawAchievementBanner), and every line the
// resolvers compose fits two, which test_explore_flavor_lines_fit holds them to.
constexpr int kExploreFlavorLines = 2;

}  // namespace

int Game::exploreStatusLines() const {
    if (!walk_.active) return 0;
    if (!walk_.flavor[0]) return 2;             // the badge + the Bandwidth readout
    const int wrapped = textWrapLines(walk_.flavor, kExploreStatusW);
    return 2 + (wrapped < 1 ? 1
                            : (wrapped > kExploreFlavorLines ? kExploreFlavorLines
                                                             : wrapped));
}

void Game::drawHabitat(Framebuffer& fb, int cursor) const {
    // The BACKGROUND pass. Composed against the shelf a resting pet's feet sit on, which
    // the bottom carousel track then covers, so a habitat backdrop's identity has to
    // live in its horizon band and its silhouette, and never in its floor.
    if (!drawScene(fb, habitatScene(), beat_, sceneGround(kLivingBottom)))
        fb.clear(palColor(Pal::PAPER));

    // Drawn before the pet so a fork crawls out from behind its body.
    drawIdleReplicas(fb, kActiveW / 2 + logicalToActive(petWander_.offsetX()));

    const SpriteData* pet = pet_.creature ? registry_.creatureSprite(*pet_.creature) : nullptr;
    if (pet) drawHabitatPet(fb, *pet);
    drawFeedingMail(fb);
    // The squat writes on the caption row, so only while that row is free: no menu
    // summoned, no nudge, and no explore readout hanging off the corner beside it.
    if (cursor < 0 && tipSeen(Tip::MenuOpened) && !walk_.active) drawSquatLogin(fb);

    drawHabitatPrompt(fb, pet);
    drawFailingBanner(fb);
    // The one-time menu nudge (game_onboard.cpp): until the carousel has been summoned
    // once on this device, the habitat says which button does it. It sits on the row a
    // summoned menu's name would take, which is free at idle by definition, and goes
    // for good the first time A or C is pressed here.
    if (nav_ == Nav::Idle && !tipSeen(Tip::MenuOpened)) {
        const char* nudge = "PRESS A FOR MENU";
        drawText(fb, (kActiveW - textWidth(nudge)) / 2, kLivingTop + 4, nudge,
                 palColor(Pal::INK));
    }

    drawHabitatStatusColumn(fb);
    drawAuditBadge(fb);
    if (walk_.active) drawExploreStatus(fb);

    // The achievement announcement, over everything else in the living area (it is
    // transient, and the pet is still there underneath when it clears).
    drawAchievementBanner(fb);

    drawHabitatCarousel(fb, cursor);
}

void Game::drawHabitatPet(Framebuffer& fb, const SpriteData& pet) const {
    // An authored clip plays its full row in order; otherwise fall back to the
    // breathe/blink heuristic on row 0. A creature that authored a "walk" takes it
    // for exactly the beats the wander below is actually moving the anchor, so the
    // legs and the drift are one motion rather than two that happen to overlap;
    // one that authored none simply keeps breathing while it slides, which is what
    // every single-row sheet does.
    const AnimClip* clip = nullptr;
    if (pet_.creature) {
        if (petFeeding_.biting() || petLure_.biting()) clip = pet_.creature->clip("attack");
        if (!clip && petWander_.travelling()) clip = pet_.creature->clip("walk");
        if (!clip) clip = pet_.creature->clip("idle");
    }
    const int row = clip ? clip->row : 0;
    const int frame = clip ? clip->frameAt(beat_) : idleFrame(pet, beat_);
    // Two motions, and the creature is the only thing on this screen that takes
    // either. The bob is the pose's own 2px lift on alternate beats, which
    // animates a single-frame creature. A swimmer skips it, because a bob on top
    // of a continuous drift reads as jitter. The WANDER moves the whole anchor
    // around the living box, in logical px (core/model/idle_wander.h): sideways
    // for every mover, and off the shelf for the ones that don't need the floor.
    const int bob = (IdleWander::bobs(pet_.creature->locomotion) && (beat_ & 1) == 0) ? 2 : 0;
    const int petW = pet.frameW * kScaleNum / kScaleDen;
    const int petH = pet.h * kScaleNum / kScaleDen;
    const int petX = (kActiveW - petW) / 2 + logicalToActive(petWander_.offsetX());
    const int petY =
        kLivingBottom - petH - bob - logicalToActive(petWander_.offsetY());
    // Turned to the way it is actually going. A sheet with no declared facing is
    // never mirrored, so this only turns the creatures the wander was walking
    // backwards: the ones drawn in profile instead of §2's turned-to-the-viewer
    // pose (assets/CREATURE_VISUAL_RULES.md). The habitat has no seat and no
    // opponent, so unlike the combat stage the direction comes off the trip
    // itself.
    const bool mirror = spriteMirrorToFace(pet, petWander_.headingRight());
    // Resting colour: a line that wears borrowed colours spends a few seconds now
    // and then in another family's palette (core/model/idle_camo.h). The level and
    // the art are the tick's; this ranks that art into the ladder the creature is
    // repainted through, exactly as the combat screen does. Every other pet, and
    // this one between drifts, takes the plain blit and pays nothing.
    const CamoRamp drift =
        idleCamoWorn_ ? camoRampFrom(*idleCamoWorn_) : CamoRamp{};
    if (petDisguise_.showsProp())
        drawDisguiseProp(fb, petX + petW / 2);
    else if (petHarvest_.stage() == HarvestStage::Hold)
        drawSpriteRain(fb, pet, frame, petX, petY, kScaleNum, kScaleDen,
                       palColor(Pal::PAPER), palColor(Pal::DECRYPTION_GREEN), beat_, row,
                       mirror);
    else if (petVanish_.active())
        drawSpriteCloak(fb, pet, frame, petX, petY, kScaleNum, kScaleDen,
                        petVanish_.amount(), palColor(Pal::INK), beat_, row, mirror);
    else if (petMimic_.active() && idleMimicWorn_)
        drawMimic(fb, pet, frame, row, petX, petY, mirror);
    else if (petDisguise_.torn())
        drawSpriteGlitch(fb, pet, frame, petX, petY, kScaleNum, kScaleDen,
                         palColor(Pal::INK), palColor(Pal::HOT),
                         static_cast<uint8_t>(beat_ * 32), row, mirror);
    else if (petCamo_.level() > 0 && !drift.empty())
        drawSpriteCamo(fb, pet, frame, petX, petY, kScaleNum, kScaleDen, drift,
                       petCamo_.level(), /*flashColor=*/0, /*flashAmt=*/0, row,
                       /*from=*/nullptr, mirror);
    else
        drawSpriteUpscaled(fb, pet, frame, petX, petY, kScaleNum, kScaleDen, row,
                           mirror);
    drawHarvestStreams(fb, petX + petW / 2, petY + petH / 2);
    drawLure(fb, petX + petW / 2, petY + petH / 4);
}

void Game::drawHabitatPrompt(Framebuffer& fb, const SpriteData* pet) const {
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
            // The ⚡ override pip flashes, inviting a crack of the egg into the hatch
            // cinematic. It names B, the one-button way in (game_nav.cpp's idle case);
            // the A+C chord does the same and stays the shortcut for those who know
            // it. A distinct glyph + the label carry the meaning with no colour
            // channel. It only ever appears in the home stretch of the clock, so an
            // egg is never advertised a crack it can't take.
            const bool on = ((beat_ / 2) & 1) == 0;   // flash to draw the eye
            if (on) {
                const SpriteData& glyph = ASSET_ICON_OVERRIDE_PIP;
                const int blockH = glyph.h + 2 + kFontH;
                int gy = petTop - 6 - blockH;
                if (gy < minY) gy = minY;
                drawSprite(fb, glyph, 0, (kActiveW - glyph.frameW) / 2, gy);
                const char* hint = "B HATCH";
                drawText(fb, (kActiveW - textWidth(hint)) / 2, gy + glyph.h + 2,
                         hint, palColor(Pal::INK));
            }
        } else {
            // M:SS, counting down. Seconds are the point: a minutes-only readout
            // sits on the same number for a minute at a time, which reads as a
            // stalled egg rather than a waiting one. The label is for the player who
            // has never seen an egg: bare digits over one could as well be a timer
            // running OUT. Repaint comes free from the heartbeat (game_core.cpp),
            // which marks every beat dirty regardless of whether the model moved.
            char prompt[24];
            const unsigned totalSecs = pet_.bootHatchRemainMs / 1000u;
            std::snprintf(prompt, sizeof prompt, "HATCHES IN %u:%02u",
                          totalSecs / 60u, totalSecs % 60u);
            int py = petTop - 6 - kFontH;
            if (py < minY) py = minY;
            drawText(fb, (kActiveW - textWidth(prompt)) / 2, py, prompt,
                     palColor(Pal::INK));
        }
    } else if (pet_.model.isHungry()) {
        const bool on = ((beat_ / 2) & 1) == 0;
        if (on) {
            const SpriteData& icon = ASSET_UI_ALERT_HUNGER;
            drawSprite(fb, icon, 0, kActiveW - icon.frameW - 4, kLivingTop + 4);
        }
    }
}

void Game::drawFailingBanner(Framebuffer& fb) const {
    // FAILING: a pet at 5/5 errors is lost when kCsfDyingGraceMs of powered-on time
    // runs out (tickLifecycle), and the ways back are a Yubi-Cookie or a stored Daemon
    // sold for a HOTFIX. A banner that counts it down and names the rescue is the least
    // a player walking up to the device is owed. It sits below the hunger alert's 20px
    // slot on a plate of paper, so it reads over whatever the habitat has behind it.
    if (pet_.creature && !inEggPhase() && pet_.model.careBranch() == CareBranch::Dying) {
        const uint32_t left = static_cast<uint32_t>(failingLeftMs());
        char head[24];
        std::snprintf(head, sizeof head, "FAILING - %u:%02u LEFT",
                      static_cast<unsigned>(left / 60000u),
                      static_cast<unsigned>((left / 1000u) % 60u));
        const char* rescue = yubiReady()      ? "B: FEED THE YUBI-COOKIE"
                           : daemonOnRack()   ? "ARCH: SELL FOR A HOTFIX"
                           : pet_.grants.yubiSpent    ? "NOTHING CAN SAVE IT NOW"
                                              : "ONLY A YUBI-COOKIE SAVES IT";
        const int by = kLivingTop + 26;
        fb.fillRect(0, by - 3, kActiveW, 2 * (kFontH + 3) + 3, palColor(Pal::PAPER));
        drawText(fb, (kActiveW - textWidth(head)) / 2, by, head, palColor(Pal::HOT));
        drawText(fb, (kActiveW - textWidth(rescue)) / 2, by + kFontH + 3, rescue,
                 palColor(Pal::INK));
    }
}

void Game::drawHabitatStatusColumn(Framebuffer& fb) const {
    // Where the top-LEFT status column starts. Three unrelated things want this
    // corner. The SD flash, the buff row, and (while the mode runs) the explore
    // status stack drawn further down, so the origin is computed once here and
    // all of them hang off it. Explore wins the corner outright because its lines
    // are the live readout; the rest slide below the block it occupies.
    int statusY = kLivingTop + 4;
    if (walk_.active) statusY += exploreStatusLines() * (kFontH + 2) + 2;

    // SD-present flash: clear of the hunger alert (top-right) and the carousel
    // tracks (top/bottom bands). A distinct icon shape + slot so it reads in
    // grayscale.
    if (sdIconVisible()) {
        const SpriteData& icon = ASSET_ICON_SYS_SD;
        drawSprite(fb, icon, 0, 4, statusY);
    }

    // Active item-buff row: stacked below the SD icon slot (or at its spot if SD
    // is absent). The STAT BUFFS page (page 2) is the full description, this is
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
        if (pet_.grants.shieldActive) marker("restore_point", "[SHLD]");
        if (pet_.armed.trojanDivert) marker("ambig_usb", "[USB]");
        // The rest of the USB port. The branch override is ONE slot, so it draws one
        // marker either way; the soak names the weaker of its two devices, since which
        // one armed it isn't tracked (the factor is) and both share the family glyph.
        if (pet_.usb.branch == BranchOverride::Bad) marker("bad_usb", "[BAD]");
        else if (pet_.usb.branch == BranchOverride::Good) marker("signed_usb", "[SGND]");
        if (pet_.usb.soakFactor > 1) marker("sandbox_usb", "[SOAK]");
        // The hold gets a badge over any of them: it is why STAT's EVOLVE row reads MAX
        // on a pet that plainly has a successor, so the habitat has to say it is there.
        if (pet_.usb.hold) marker("halt_usb", "[HELD]");
        if (backupShieldArmed()) marker("backup_drive", "[BKUP]");
        // The two DeepWeb Dive depth buffs record only that they are armed, not
        // which of the several items did it, so both show the shared Buff-item
        // stand-in itemIcon() falls back to rather than guessing an id.
        if (pet_.armed.deepWebDepthMultiplier > 1) marker("deep_learning_module", "[DEEP]");
        if (pet_.armed.deepWebStartDepth != -1) marker("backdoor_bell", "[BELL]");
    }
}

void Game::drawAuditBadge(Framebuffer& fb) const {
    // Audit capture-phase badge: top-RIGHT, just below the hunger alert. Surfaces
    // the AuditCapture SM phase (ARM / HOT <s>s / COOL <m>m), so the ~2-min hot
    // window AND the ~30-min re-arm cooldown (radio off; a handshake can't be
    // captured then) are visible on screen. The Hot phase is the one worth
    // catching from across the room, so it takes a glyph as well; the word stays
    // either way, which is what keeps the badge grayscale-safe.
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

void Game::drawExploreStatus(Framebuffer& fb) const {
    // Explore-mode badge: a status line under the top track while the mode
    // runs. The armed sub-area + WINS n/10 (or BOSS READY). Dual-coded, grayscale-safe.
    // A brief event-flavor tick sits just below it. The badge label is the area's short
    // name + the 1-based sub number (e.g. "CITRUS 3"): compact + collision-free.
    // Pick the badge's right-field mode: the endless DeepWeb Dive shows DEPTH; a
    // re-armed CLEARED sub counts down to the next rung while auto-progress is armed
    // (NEXT IN n) and otherwise shows FARMING (its boss is done); an unlocked
    // sub-boss shows BOSS READY; otherwise WINS n/10 progress.
    ExploreBadgeMode mode;
    if (inDeepWebDive()) mode = ExploreBadgeMode::DeepDive;
    else if (subCleared(walk_.sector, walk_.sub))
        mode = autoProgress_ ? ExploreBadgeMode::AutoNext : ExploreBadgeMode::Farming;
    else if (subBossUnlocked(walk_.sector, walk_.sub)) mode = ExploreBadgeMode::BossReady;
    else mode = ExploreBadgeMode::Wins;
    char label[20];
    exploreBadgeLabel(label, sizeof(label));
    // the FARMING badge shows Bandwidth (the farming pool) in place of the streak:
    // "FARM n/max" while it lasts (full loot), "FARM LOW" once depleted (decayed).
    // AUTO-NEXT takes the same slot when the walk is stepping itself, and takes the
    // streak instead: the pool is spelled out on the status line right below, and
    // the question a hands-off run leaves open is when it moves on, not what is left
    // in a shield the operator is not spending by hand.
    int badgeCount = walk_.streak, badgeMax = kExploreStreakToBoss;
    if (mode == ExploreBadgeMode::Farming) {
        badgeCount = bandwidth_; badgeMax = bandwidthMax();
    }
    drawExploreBadge(fb, label, badgeCount, badgeMax, mode);
    // Bandwidth is the per-fight fragmentation shield, so it drains on every
    // explore fight. It shows all the time, beyond the Farming badge, so the player
    // can watch it and decide when to defrag or come home. CALM while it lasts,
    // WARN + "LOW" once dry (dual-coded).
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
        // longest of those is wider than the column. Plain text would cut the tail
        // with no ellipsis, and the tail says where the reward went.
        if (walk_.flavor[0])
            drawTextWrapped(fb, kExploreStatusX, bwLineY + kFontH + 2,
                            kExploreStatusW, walk_.flavor, palColor(Pal::INK_DIM),
                            kFontH + 2, kExploreFlavorLines);
    }
}

void Game::drawHabitatCarousel(Framebuffer& fb, int cursor) const {
    // Grey out the care/combat slots while the pet is still an egg. EXPL's globe turns
    // while AUTO-PROGRESS is armed: the walk runs in the background, and the shelf is
    // the screen that is always up.
    CarouselMarks marks;
    for (int i = 0; i < kCarouselSlots; ++i) {
        const SubmenuId id = carouselSlots()[i].id;
        if (slotLocked(id)) marks.locked |= (1u << i);
        if (id == SubmenuId::Expl && autoProgress_ && walk_.active)
            marks.spin |= (1u << i);
    }
    // The slots the pet needs visited (game_onboard.cpp) stay lit with a "!" beside
    // them: the shelf is always up at idle, so it is where a need is seen first.
    // CFG's CARE ALERTS switch turns the marks off; the needs behind them stand.
    if (settings_.careAlerts) careAttention(marks.attention, marks.urgent);
    drawCarousel(fb, cursor, settings_.uiMode, beat_, marks);
}

Game::AchBannerCopy Game::achBannerCopy() const {
    AchBannerCopy c;
    const AchievementDef* d = achBanner();
    if (!d) return c;
    // The one announcement that is not just news: an unlock that put a NEW KIND OF EGG
    // in the hatch menu. It gets the held plate (a different colour, its own copy, and
    // no deadline), because it is the only banner with an instruction in it, and a
    // three-second window is no way to deliver one.
    const EggLineDef* eggLine = achBannerCount_ == 1 ? achEggLineUnlocked(*d) : nullptr;
    c.held = eggLine != nullptr;
    c.kicker = c.held ? "NEW EGG LINE" : "ACHIEVEMENT";

    // A burst says how many first and names one of them, so a long back-catalogue drop
    // still tells the player something specific about what they got.
    if (achBannerCount_ > 1)
        std::snprintf(c.name, sizeof(c.name), "%d UNLOCKED", achBannerCount_);
    else if (c.held)
        // The LINE, not the achievement that earned it: "Hash Collision" is the thing
        // they just did, and "Metamorphic" is the thing they can now do about it. Only
        // one of those belongs on a banner whose whole job is to point at the hatch.
        std::snprintf(c.name, sizeof(c.name), "%s", eggLine->displayName);
    else
        std::snprintf(c.name, sizeof(c.name), "%s", d->displayName);

    // The third line is the reward, so the player knows a Commendation Cache is
    // waiting in the VAULT.
    int rewardBits = 0;
    const char* rewardItem = nullptr;
    for (const AchievementReward& r : d->rewards) {
        if (r.kind == AchievementReward::Kind::Bits) rewardBits += r.magnitude;
        else if (r.kind == AchievementReward::Kind::Item && r.id)
            if (const ItemDef* it = registry_.item(r.id)) rewardItem = it->displayName;
    }
    if (c.held)
        // A held plate says where to go and how to dismiss it. The rewards are still
        // paid; they are simply not what this banner is for.
        std::snprintf(c.reward, sizeof(c.reward), "ARCH > NEW EGG - ANY KEY");
    else if (achBannerCount_ > 1)
        std::snprintf(c.reward, sizeof(c.reward), "%s +MORE", d->displayName);
    else if (rewardItem && rewardBits > 0) {
        // Two rewards on one line is the only combination too wide for the plate:
        // "+400 BITS + COMMENDATION CACHE" is 30 characters against a 28-character
        // canvas. The currency abbreviates to the B the shop prices already use, which
        // buys back the three characters the pair needs.
        std::snprintf(c.reward, sizeof(c.reward), "+%d BITS + %s", rewardBits, rewardItem);
        if (textWidth(c.reward) > kActiveW)
            std::snprintf(c.reward, sizeof(c.reward), "+%d B + %s", rewardBits, rewardItem);
    } else if (rewardItem)
        std::snprintf(c.reward, sizeof(c.reward), "+%s", rewardItem);
    else if (rewardBits > 0)
        std::snprintf(c.reward, sizeof(c.reward), "+%d BITS", rewardBits);
    return c;
}

uint32_t Game::achBannerDwellMs() const {
    const AchBannerCopy c = achBannerCopy();
    // The window the plate scrolls in, which plateLine hands the marquee.
    const int room = kActiveW - 2 * kMargin;
    int beats = marqueeCycleBeats(c.name, room);
    const int rewardBeats = marqueeCycleBeats(c.reward, room);
    if (rewardBeats > beats) beats = rewardBeats;
    // ONE FULL MARQUEE CYCLE of the longest line, floored at kAchBannerMs. A line that
    // fits is two holds (read the head, read the tail, at the marquee's own rate), so
    // the ordinary banner sits 3.0s, above the 2.6s floor, and one that travels is
    // those two holds plus the travel between them. That is the whole point: the time
    // comes from the COPY, so a long name cannot leave mid-word, and nobody has to
    // guess a constant that suits every name in the table.
    const uint32_t travelled = static_cast<uint32_t>(beats) * kHeartbeatMs;
    return travelled > kAchBannerMs ? travelled : kAchBannerMs;
}

void Game::drawAchievementBanner(Framebuffer& fb) const {
    // The whole feedback channel for an unlock: three centred lines on a filled band,
    // shown on the home screen for achBannerDwellMs(). Modelled on the combat verdict
    // banner: a solid TRACK plate under INK text reads in grayscale on shape alone.
    const AchievementDef* d = achBanner();
    if (!d) return;
    const AchBannerCopy copy = achBannerCopy();
    // The band sits in the gap between two things it must not cover: the idle status
    // slots along the top of the living area (SD top-left, hunger top-right, the capture
    // badge under it. Reserved out to kLivingTop+40, and gated as such), and the pet
    // itself, which is anchored down on the shelf. Centring it would put the plate over
    // the pet's face for the whole announcement.
    // Three lines at font pitch, no more: the band has to fit the gap between the
    // reserved status slots above and the pet's crown below, and a taller plate would
    // have to cover one of them.
    const int bandY = kLivingTop + 42;
    const int bandH = 38;
    // The held plate inverts: a light NOTICE_HOLD ground with PAPER text on it, against
    // the ordinary banner's dark TRACK with INK text. The two plates differ in value as
    // well as hue, so a held one is recognisable in grayscale before a word of it is
    // read.
    const bool held = copy.held;
    const Rgb565 plate = held ? palColor(Pal::NOTICE_HOLD) : palColor(Pal::TRACK);
    const Rgb565 body = held ? palColor(Pal::PAPER) : palColor(Pal::INK);
    const Rgb565 quiet = held ? palColor(Pal::PAPER) : palColor(Pal::INK_DIM);

    // ARRIVAL. The plate opens from its own centre line over the first kOpenBeats, so
    // the announcement ENTERS rather than replacing a frame of habitat between two
    // repaints. Shape only, which is the same reason the plate is a plate: a wipe reads
    // at a glance in grayscale, where a fade is only a value the eye has nothing to
    // compare against. Held plates open too: the instruction can wait three beats.
    constexpr int kOpenBeats = 3;
    const int age = beat_ - achBannerOpenBeat_;
    const int open = (age >= kOpenBeats || age < 0) ? bandH
                                                    : bandH * (age + 1) / (kOpenBeats + 1);
    const int drawH = open < 2 ? 2 : open;            // never thinner than its own rules
    const int drawY = bandY + (bandH - drawH) / 2;
    fb.fillRect(0, drawY, kActiveW, drawH, plate);
    // A 1px lid and sill: the band has to read as a plate laid over the habitat rather
    // than a hole in it, and an edge is the only cue that survives desaturation.
    fb.fillRect(0, drawY, kActiveW, 1, quiet);
    fb.fillRect(0, drawY + drawH - 1, kActiveW, 1, quiet);
    if (drawH < bandH) return;                        // still opening: the plate alone

    // Every line on this plate is content (an achievement name, an egg line, an item),
    // so none can be centred and left to run: a name wider than the canvas would be
    // cut at both ends and look like a rendering fault. Centred while it fits,
    // scrolled in the margins once it doesn't. The plate is full-bleed, so a centred
    // line may use the whole canvas: the held plate's own instruction already spans
    // it. Only what does not fit gets pulled in to the margins to scroll. The marquee
    // is driven from the banner's OWN age rather than the free-running beat, so every
    // announcement starts its travel at the same place: on the shared clock a long
    // name could arrive already halfway past.
    const int age0 = age - kOpenBeats;
    auto plateLine = [&](int ly, const char* s, Rgb565 col) {
        const int w = textWidth(s);
        if (w <= kActiveW) { drawText(fb, (kActiveW - w) / 2, ly, s, col); return; }
        drawTextMarquee(fb, kMargin, ly, kActiveW - 2 * kMargin, s, col, age0, Overflow::Scroll);
    };

    // The kicker pulses between its two inks for its first beats to draw the eye. The
    // step is in value, so it survives the grayscale gate.
    const bool flash = age0 < 4 && ((age0 / 2) % 2 == 0);
    plateLine(bandY + 5, copy.kicker, flash ? body : quiet);
    plateLine(bandY + 15, copy.name, body);
    if (copy.reward[0])
        plateLine(bandY + 25, copy.reward, held ? body : palColor(Pal::CALM));
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
            // Minutes remaining until re-arm, which explains why nothing is
            // capturing (the radio is off for the whole window).
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
        // STAT's own two screens and the six pages behind them are game_stat.cpp's.
        case SubmenuId::Stat: stat_.draw(fb, statSources(), beat_); break;
        case SubmenuId::Items: items_.draw(fb, *this, beat_); break;
        case SubmenuId::Maint: maintScreen_.draw(fb, *this); break;
        case SubmenuId::Cfg: cfg_.draw(fb, *this); break;
        case SubmenuId::Arch: arch_.draw(fb, *this); break;
        case SubmenuId::Mods: loadout_.draw(fb, *this); break;
        case SubmenuId::Games:
            arcadeScreen_.drawList(fb, *this);
            break;
        case SubmenuId::Expl: expl_.draw(fb, *this); break;
        default:
            drawPlaceholderSubmenu(fb, carouselSlots()[cursor_].label);
            break;
    }
}

void Game::drawDetail(Framebuffer& fb) const {
    switch (enteredId()) {
        case SubmenuId::Items:
            items_.drawDetail(fb, *this, beat_);
            break;
        case SubmenuId::Maint:
            maintScreen_.drawDetail(fb, *this);
            break;
        case SubmenuId::Cfg: cfg_.drawDetail(fb, *this); break;
        case SubmenuId::Arch: arch_.drawDetail(fb, *this); break;
        case SubmenuId::Mods: loadout_.drawDetail(fb, *this); break;
        case SubmenuId::Games:
            arcadeScreen_.drawCabinet(fb, *this);
            break;
        default: break;
    }
}

// Which art a CamoTarget names: the contract is on the declaration (game.h).
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
    if (!pet_.creature) return nullptr;
    const std::vector<const CreatureLine*> lines = registry_.allCreatureLines();
    int others = 0;
    for (const CreatureLine* l : lines)
        if (!pet_.creature->line || std::strcmp(l->id, pet_.creature->line) != 0) ++others;
    if (others <= 0) return nullptr;         // a roster of one family has nobody to be
    const int want = ((slot % others) + others) % others;
    int i = 0;
    for (const CreatureLine* l : lines) {
        if (pet_.creature->line && std::strcmp(l->id, pet_.creature->line) == 0) continue;
        if (i++ != want) continue;
        // Through the same resolver the fight uses, so a drifting pet at home and a
        // channelling one in a fight reach for the same creature: that family's row at
        // the drifter's OWN tier, never its hatchling.
        return camoSpriteForTarget({CamoTarget::Source::Line, l->id},
                                   /*rivalSprite=*/nullptr, pet_.creature->stage);
    }
    return nullptr;
}

namespace {
// The flock as the DRAW sees it (SwarmView, core/render/swarm.h). The copy is deliberate:
// core/render never reaches into core/model, so a renderer is handed a value that owns its
// marks. The same call CamoRamp makes. It is a couple of hundred bytes on a repaint that
// is already composing a whole panel.
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

SwarmView Game::guardianSwarm() const { return swarmViewOf(guardianFlock_); }

void Game::drawStacker(Framebuffer& fb) const {
    if (gameBriefOpen_) { drawGameBrief(fb); return; }
    PlayOutcome sofar;
    sofar.units = stacker_.blocks();
    sofar.score = stacker_.score();
    sofar.scoreMax = kStackerMaxScore;
    drawStackerBoard(fb, stacker_, playHud(sofar));
}

void Game::drawProcess(Framebuffer& fb) const {
    if (!maint_) return;
    const MaintRun& run = *maint_;
    if (!run.resolved) {
        drawMaintProcess(fb, run.kind, static_cast<float>(run.beat) / kProcessBeats);
        return;
    }
    // A PLAYED run reports what its own board was worth and can come up short without
    // failing; a rolled or bought one reports the fixed reduction it either got or missed.
    if (run.played)
        drawMaintOutcome(fb, run.kind,
                         run.success ? MaintOutcome::Cleaned : MaintOutcome::Partial,
                         run.fragRemoved, run.bitsEarned);
    else
        drawMaintOutcome(fb, run.kind,
                         run.success ? MaintOutcome::Cleaned : MaintOutcome::Failed,
                         run.kind == MaintKind::Defrag ? kDefragReduction : kAvReduction);
}

void Game::drawEvolve(Framebuffer& fb) const {
    EvolveView v;
    // The cinematic phase from the beat counter.
    v.phase = evolveBeat_ < kEvoHoldBeats                    ? EvolvePhase::Hold
              : evolveBeat_ < kEvoHoldBeats + kEvoFlashBeats ? EvolvePhase::Flash
                                                             : EvolvePhase::Reveal;
    v.from = pet_.creature ? registry_.creatureSprite(*pet_.creature) : nullptr;
    v.to = evolveTo_ ? registry_.creatureSprite(*evolveTo_) : nullptr;
    // What changed, in words: the path the errors so far put it on (the thing that
    // decides a Daemon's form), and any move slot the new stage opens.
    char path[28];
    const int errors = pet_.model.careMistakes();
    std::snprintf(path, sizeof path, "%s PATH - %d ERROR%s",
                  effectiveCareBranch() == CareBranch::Bad ? "BAD" : "GOOD", errors,
                  errors == 1 ? "" : "S");
    char slot[40];
    if (pet_.creature && evolveTo_) {
        const int gained = MoveLoadout::slotsForStage(evolveTo_->stage) -
                           MoveLoadout::slotsForStage(pet_.creature->stage);
        if (gained > 0) {
            std::snprintf(slot, sizeof slot, "+%d MOVE SLOT%s - SEE MODS", gained,
                          gained == 1 ? "" : "S");
            v.slotLine = slot;
        }
    }
    v.toName = evolveTo_ ? evolveTo_->displayName : "";
    v.toStage = evolveTo_ ? evolveTo_->stage : Stage::Process;
    v.pathLine = path;
    v.beat = beat_;
    drawEvolveModal(fb, v);
}

void Game::drawCSF(Framebuffer& fb) const {
    // The heaviest modal in the game: the dying pet corrupts out under a
    // FX_CRITICAL_FAIL crash overlay. B is gated behind the crash hold (csfRevealed).
    const SpriteData* pet = pet_.creature ? registry_.creatureSprite(*pet_.creature) : nullptr;
    drawCSFModal(fb, pet, csfRevealed() ? CsfPhase::Held : CsfPhase::Crashing, beat_);
}

}  // namespace mal
