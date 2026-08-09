// dump_frame.cpp — headless single-frame renderer for visual verification and
// (later) screenshot baselines. Links only malcore (no SDL). Writes a binary
// PPM of the full 240 panel:
//   ./dump_frame [beats] [out.ppm] [flags...]
// flags: hungry · crit · stat · log · carousel · bottom · iconsonly · textonly
//        items [picker [keys|tools]] [row:<n>] [detail|feed] (row:<n> walks the list
//             cursor first, so a detail other than the first row is renderable)
//        feed:<item_id> (eat one named food through the real Use path and hold the
//             feeding modal — how to eyeball that its gauges follow that item's own
//             effects, e.g. feed:tortilla_chip, feed:null_noodles)
//        maint [detail] [stacker [slide|drop|stop ...]] · lockout · evolve
//        decryption [rows|lost] (the Ransomware egg's DISK DECRYPTION board; "rows"
//             plays three attempts so the history and its corruption overlay are on
//             screen, "lost" plays all five and holds the verdict + revealed key)
//        arcade [clutch|worm] [cabinet [hard] [play [result]]] (the GAMES list, one
//             cabinet's page, its running board, and the payout)
//        clutch [aim] [round ...] | clutch win (the Phishing egg's Clutch Pick; "aim"
//        flips to the second half and "round" commits, interleaved to walk any path —
//        three "round"s land on the lost reveal, "win" plays it perfectly instead)
//        isolation [steps:<n>] [crash] [bank] (the Worm egg's Isolation Protocol;
//             "steps:<n>" walks the worm n moves along the buffer's Hamiltonian cycle so
//             the frame shows a long coil mid-run, "crash" drives it into the wall to
//             hold the verdict, and "bank" takes the B off it to leave the Vermicell egg
//             at idle)
//        hatchreveal [frame:<n>] (the on-demand crack cinematic, held on frame n)
//        cfg updates [ready] [checking|nojoin|found|confirm [yes]|installing|failed|
//             flashqr] (the UPDATES screen; without "ready" it shows which setup step
//             is missing, and "flashqr" walks onto the last row and opens the USB
//             flasher's code)
//        cfg [sysinfo|tag|titles|device|uimode|brightness|travel [sleeping]|
//             radio [idle|all]|audit|
//             link|pediaap|qr|factory] (the settings tree; device/radio are the two
//             group screens, and radio is seeded with a live arbiter owner —
//             "idle" seeds nothing on air, "all" seeds every toggle on under a
//             running update job)
//        arch [stored] [detail] [confirm]
//        train [trainpicker] · combat [override] [stats] (the raw dev hook) ·
//        simbattle [fight [stats]] (the REAL entry — buffs carried in from outside,
//             e.g. armbuffs simbattle fight stats) · malbear
//        bruinforce (Good Daemon) · berserkernel (Bad Daemon) · csf (Critical Failure)
// expl [inside|bossready|endgame] (the nested area/sub-area ladder — one nav
//        LEVEL per frame: the top-level zone picker, "inside" for area 0's own block,
//        "bossready" for that block with its gauntlet unlocked, "rerun" for it already
//        beaten, "endgame" for the every-area-cleared picker) ·
//        explore (armed → the idle explore badge)
// explorectl [auto] (the A+C control overlay; "auto" arms AUTO-PROGRESS with the
//        second chord) · explore auto (the armed habitat with it running — the EXPL
//        globe spins, so pass a `beats` count to land on a frame) · encounter [sinkhole] ·
//        wildcombat (arm → Network Ping to a wild intro → live combat; sinkhole seeds
//        a Sinkhole Trap so the 3rd intro option shows) · walk (alias of explore)
// wifi (arm → ping to the Wi-Fi network event,; bounded search,
//        auto-Sinkholes through any Wild encounters rolled along the way)
// rank (Hacker Rank rank-up celebration on the idle badge,; crosses
//        a rank via registerNetwork, then resolves one event to surface it)
//        postencounter (a wild fight ridden to its own auto-dismiss, landing the
//        frame on Nav::PostEncounter — the bandwidth/frag STATUS readout)
// hacker (A+C → the Hacker face home) · hacker profile (the PROFILE
// viewer) · hacker shop [hub] [row:<n>] [buy] (the SHOP; "hub" buys the MERGE HUB
//        first, which is what reveals its two recipe rows, and row:<n> A-cycles
//        down the list to reach them) ·
// hacker merge [recipes] [stock] (the MERGE HUB craft list — the slot is itself a
//        SHOP purchase, so the scene buys it before entering) ·
// hacker vault (the VAULT cache list, stocked with every container tier) ·
// hacker crew [joined] [unset] [netpick] [red|blue [row:<n>] [detail]] (the CREW
//        screen's four views: the Hub, the home-network picker, one side's roster,
//        and a crew's own page)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "tunables.h"
#include "core/app/game.h"
#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/ui/carousel.h"
#include "core/ui/cfg_screen.h"
#include "core/ui/expl_screen.h"

using namespace mal;

static bool hasFlag(int argc, char** argv, const char* f) {
    for (int i = 3; i < argc; ++i)
        if (std::strcmp(argv[i], f) == 0) return true;
    return false;
}

int main(int argc, char** argv) {
    int beats = (argc > 1) ? std::atoi(argv[1]) : 0;
    const char* out = (argc > 2) ? argv[2] : "frame.ppm";

    // Default to an already-hatched pet so the existing nav flags work; the
    // "hatch" flag starts from an empty save instead. "evolve" starts
    // on the Boot-Sector CryptoShell so the evolution boundary has a successor to reveal.
    const bool evolveFlag = hasFlag(argc, argv, "evolve");
    // Start creature: cryptoshell for the evolve cinematic; malbear/bruinforce to
    // exercise the new Script/Daemon placeholders; paypup (Process) otherwise.
    const char* startCreature = evolveFlag ? "cryptoshell"
        : hasFlag(argc, argv, "malbear") ? "malbear"
        : hasFlag(argc, argv, "berserkernel") ? "berserkernel"
        : hasFlag(argc, argv, "bruinforce") ? "bruinforce"
        : "paypup";
    // "pet:<id>" renders any roster creature by id (e.g. pet:tadpoll) — used to
    // eyeball the cat/frog lines. Overrides the fixed flags above.
    for (int i = 3; i < argc; ++i)
        if (std::strncmp(argv[i], "pet:", 4) == 0) startCreature = argv[i] + 4;
    Game game{hasFlag(argc, argv, "hatch") ? StartMode::FreshHatch
                                           : StartMode::Hatched,
              startCreature};
    if (hasFlag(argc, argv, "hungry")) game.model().setHunger(10);  // Critical band
    if (hasFlag(argc, argv, "lockout")) game.model().setHunger(0);  // fires Lockout
    // A stat preset exercising Caution/Critical zones for the grayscale gate.
    if (hasFlag(argc, argv, "crit")) {
        game.model().setHunger(12);          // Critical (hot, pulses)
        game.model().setFragmentation(82);   // Critical frag (ramp + hot numeric)
        game.model().setHappiness(24);       // Caution (warn)
        game.model().setCareMistakes(4);     // Bad branch
    }
    if (hasFlag(argc, argv, "sinkhole")) game.inventory().add("sinkhole_trap", 1);
    // Creature levels: grind XP to ~level 8 so STAT's LVL n + the stat points
    // (and the Rollback picker) render populated.
    if (hasFlag(argc, argv, "level") || hasFlag(argc, argv, "rollback"))
        game.debugAddCombatXp(1200);
    if (hasFlag(argc, argv, "maint")) {
        game.model().setFragmentation(31);   // something to defragment
        game.model().setGhost(true);         // AV has work to do
        game.inventory().add("disk_scrubber", 2);  // show the TOOL variant
    }
    if (hasFlag(argc, argv, "iconsonly")) game.setUiMode(UiMode::IconsOnly);
    if (hasFlag(argc, argv, "textonly")) game.setUiMode(UiMode::TextOnly);
    // ARCH rack: seed a frozen stored pet so the rack list / Deploy record render.
    if (hasFlag(argc, argv, "stored")) game.debugSeedRack("cryptoshell");
    // CSF: drive the pet to the 5/5 dying state so the ageing window can expire.
    if (hasFlag(argc, argv, "csf")) game.model().setCareMistakes(kCareDying);
    // Achievement banner: earn one and let the next tick raise its announcement over the
    // idle habitat. "achburst" earns enough at once to trip the collapse-into-a-summary
    // threshold instead, which is the other shape the banner takes.
    if (hasFlag(argc, argv, "ach")) game.unlockAchievement(ach::kTrojanUnleashed);
    if (hasFlag(argc, argv, "achburst"))
        for (int i = 0; i < kAchievementCount && i <= kAchBannerBurstMax; ++i)
            game.unlockAchievement(kAchievements[i].id);
    // CFG System Info demo: present a live SD reading + populate the audit ledgers
    // so the "SD" and (with the pcap toggle on) "SHAKES" lines render populated.
    if (hasFlag(argc, argv, "sdcard")) game.setSdStatus({true, 32000});  // 32GB
    if (hasFlag(argc, argv, "shakes")) {
        game.setAuditCaptureEnabled(true);       // pcap toggle on -> SHAKES shows
        for (int i = 0; i < 4; ++i) {
            uint8_t bssid[6] = {0x02, 0, 0, 0, 0, static_cast<uint8_t>(i)};
            game.registerHandshake(bssid);
        }
    }

    // Advance the heartbeat `beats` times so we can capture any animation phase.
    for (int i = 0; i <= beats; ++i)
        game.tick(static_cast<uint32_t>(i) * kHeartbeatMs);

    // "armbuffs" arms all three item buffs (Restore Point/Ambig-USB/Backup
    // Drive) regardless of which screen flag follows — so it shows on the
    // idle habitat's top-left buff badge as well as the STAT BUFFS page.
    // Stocks 2 of each (not 1): useItem() flips nav_ to Nav::Submenu when an
    // item's count hits 0 ("item left the list"), which would confuse the
    // enterSlot() navigation below — leaving 1 in stock sidesteps that.
    if (hasFlag(argc, argv, "armbuffs")) {
        game.inventory().add("restore_point", 2);
        game.debugUseItem("restore_point");
        game.inventory().add("ambig_usb", 2);
        game.debugUseItem("ambig_usb");
        game.inventory().add("backup_drive", 2);
        game.debugUseItem("backup_drive");
    }

    // Apply navigation AFTER ticking so the 5s auto-defocus timer (which keys off
    // the last tick) doesn't collapse the menu before we render it.
    auto enterSlot = [&](SubmenuId id) {           // A to walk the cursor, then B
        game.onButton({Button::A, true, false});   // idle A → carousel @ slot 1
        while (carouselSlots()[game.cursor()].id != id)
            game.onButton({Button::A, true, false});
        game.onButton({Button::B, true, false});
    };
    // "feed:<id>" eats one named food through the real Use path and lands on the
    // feeding modal (e.g. feed:tortilla_chip, feed:r007_b33r) — the way to eyeball
    // that the modal's gauges follow the item's own effects rather than a fixed
    // Hunger row. Stocks 2 so the modal's return path still has a detail to go back to.
    const char* feedId = nullptr;
    for (int i = 3; i < argc; ++i)
        if (std::strncmp(argv[i], "feed:", 5) == 0) feedId = argv[i] + 5;
    if (feedId) {
        game.inventory().add(feedId, 2);
        game.debugUseItem(feedId);
    } else if (hasFlag(argc, argv, "rollback")) {
        // Rollback stat picker: the leveled pet (above) has points to shed;
        // open the picker directly through the real Use path.
        game.inventory().add("rollback", 1);
        game.debugUseItem("rollback");
    } else if (hasFlag(argc, argv, "stat")) {
        // STAT is 5 paged screens: 0 vitals (landing) · 1 loadout · 2 buffs ·
        // 3 species · 4 audit log. "stat" alone shows vitals; "loadout"/
        // "buffs"/"species"/"log" step to pages 1-4.
        game.debugSetBits(1450);
        game.debugSetNetworksSeen(27);       // R2, partway to R3
        enterSlot(SubmenuId::Stat);
        int steps = hasFlag(argc, argv, "loadout") ? 1
                  : hasFlag(argc, argv, "buffs")    ? 2
                  : hasFlag(argc, argv, "species")  ? 3
                  : hasFlag(argc, argv, "log")      ? 4
                                                     : 0;
        while (steps-- > 0) game.onButton({Button::A, true, false});
    } else if (hasFlag(argc, argv, "cache")) {
        // Cache yield reveal: open a rarity-tiered cache and land on the
        // Nav::CacheYield screen naming what came out. `epic` = the 2-item tier.
        const char* id = hasFlag(argc, argv, "epic")   ? "sealed_cache_epic"
                       : hasFlag(argc, argv, "rare")   ? "sealed_cache_rare"
                       : hasFlag(argc, argv, "common") ? "sealed_cache_common"
                                                       : "sealed_cache_uncommon";
        game.inventory().add(id, 1);
        game.debugUseItem(id);
    } else if (hasFlag(argc, argv, "items")) {
        // "pantry" puts one of EVERY item in the bag, which is the only way to see
        // the whole ICON_ITEM_* set in list context — the seeded inventory carries
        // four. "row:<n>" then walks down it.
        if (hasFlag(argc, argv, "pantry"))
            for (const ItemDef* d : game.content().allItems())
                game.inventory().add(d->id, 1);
        // "picker" buys the ITEMS Type-Picker first, so ITEMS opens on the category
        // tiles; without it ITEMS opens straight on the list, as an unowned rig does.
        if (hasFlag(argc, argv, "picker")) {
            game.debugSetBits(kShopItemPickerCost);
            game.debugBuyItemPicker();
        }
        enterSlot(SubmenuId::Items);
        // With the picker up, "keys"/"tools" walk to that tile and drill into it, so
        // the filtered list behind a tile is renderable too.
        const int tile = hasFlag(argc, argv, "keys") ? 3
                       : hasFlag(argc, argv, "tools") ? 4 : -1;
        if (tile >= 0 && game.itemsScreen() == Game::ItemsScreen::Picker) {
            for (int i = 0; i < tile; ++i) game.onButton({Button::A, true, false});
            game.onButton({Button::B, true, false});
        }
        // "row:<n>" walks the cursor down the list before opening anything, so a
        // detail other than the first row is renderable (e.g. row:3 items detail).
        for (int i = 3; i < argc; ++i)
            if (std::strncmp(argv[i], "row:", 4) == 0)
                for (int k = std::atoi(argv[i] + 4); k > 0; --k)
                    game.onButton({Button::A, true, false});
        if (hasFlag(argc, argv, "detail")) game.onButton({Button::B, true, false});
        if (hasFlag(argc, argv, "feed")) {        // open detail, then Use
            game.onButton({Button::B, true, false});
            game.onButton({Button::B, true, false});
        }
    } else if (hasFlag(argc, argv, "maint")) {
        enterSlot(SubmenuId::Maint);
        if (hasFlag(argc, argv, "detail")) game.onButton({Button::B, true, false});
        // "stacker" opens the DEFRAG action, cycles A onto the minigame variant and runs
        // it; each following "drop" locks the run where it currently stands, so a board
        // mid-climb (or a lost one) is renderable by repeating the word.
        if (hasFlag(argc, argv, "stacker")) {
            game.onButton({Button::B, true, false});          // open DEFRAGMENTATION
            for (int i = 0; i < kDefragVariantStacker; ++i)
                game.onButton({Button::A, true, false});      // cycle onto STACKER
            game.onButton({Button::B, true, false});          // run it
            // "drop" locks the run where it stands; "slide" advances it one beat first,
            // so interleaving the two walks the run off the stack and renders a loss.
            // "stop" ends the run there and banks it, which is also the only way to reach
            // the outcome toast for a board that was neither cleared nor stalled.
            for (int i = 1; i < argc; ++i) {
                if (std::strcmp(argv[i], "drop") == 0)
                    game.onButton({Button::B, true, false});
                else if (std::strcmp(argv[i], "slide") == 0)
                    game.debugStepStacker();
                else if (std::strcmp(argv[i], "stop") == 0)
                    game.onButton({Button::C, true, false});
            }
        }
    } else if (hasFlag(argc, argv, "cfg")) {
        enterSlot(SubmenuId::Cfg);
        // Sub-screens: walk the tables to the target row (descending through the
        // DISPLAY / RADIO group when the setting lives in one), then B to open it.
        auto openTarget = [&](CfgScreen target) {
            const CfgScreen group = cfgParentGroup(target);
            const CfgRow* rows = nullptr;
            int n = cfgRows(rows);
            for (int i = 0; i < n && rows[i].target != group; ++i)
                game.onButton({Button::A, true, false});
            game.onButton({Button::B, true, false});
            if (group == target) return;
            n = cfgGroupRows(group, rows);
            for (int i = 0; i < n && rows[i].target != target; ++i)
                game.onButton({Button::A, true, false});
            game.onButton({Button::B, true, false});
        };
        if (hasFlag(argc, argv, "sysinfo")) openTarget(CfgScreen::SysInfo);
        else if (hasFlag(argc, argv, "tag")) openTarget(CfgScreen::HackerTag);
        else if (hasFlag(argc, argv, "titles")) {   // unlock a Title so a row isn't LOCKED
            game.debugUnlockTitle(0);                // Citrus Circuit -> auto-equipped
            openTarget(CfgScreen::Titles);
        }
        else if (hasFlag(argc, argv, "device")) openTarget(CfgScreen::Device);
        else if (hasFlag(argc, argv, "travel")) {
            openTarget(CfgScreen::Travel);
            // "sleeping" dumps the notice the confirm becomes; the bare flag dumps
            // the question, which is the frame an operator actually has to read.
            if (hasFlag(argc, argv, "sleeping")) game.requestTravelSleep();
        }
        else if (hasFlag(argc, argv, "radio")) {     // the radio toggles + who has it
            // Stand in for what the arbiter pushes on a device, since a host build
            // never mounts one. The default is the case the screen exists for: two
            // rows switched on, one of them actually on the air. "idle" is the
            // nothing-running end and "all" the crowded one — every toggle on with a
            // live update job outranking the lot, which is the only way the
            // non-selectable UPDATE row appears.
            if (hasFlag(argc, argv, "all")) {
                game.setAuditMode(Game::AuditMode::ScanCapture);
                game.setLinkEnabled(true);
                game.setApEnabled(true);
                game.setNetProvisioned(true);
                game.setUpdateManifestUrl("http://malwarium.local:8000/dist/manifest.json");
                game.requestUpdateCheck();          // the job that takes the radio
                game.setRadioOwner(RadioOwner::Update);
            } else if (!hasFlag(argc, argv, "idle")) {
                game.setNetScanEnabled(true);
                game.setApEnabled(true);
                game.setRadioOwner(RadioOwner::Ap);
            }
            openTarget(CfgScreen::Radio);
        }
        else if (hasFlag(argc, argv, "uimode")) openTarget(CfgScreen::UiMode);
        else if (hasFlag(argc, argv, "brightness")) openTarget(CfgScreen::Brightness);
        else if (hasFlag(argc, argv, "audit")) openTarget(CfgScreen::Audit);
        else if (hasFlag(argc, argv, "link")) openTarget(CfgScreen::Link);
        else if (hasFlag(argc, argv, "pediaap")) openTarget(CfgScreen::PediaAp);
        else if (hasFlag(argc, argv, "qr")) {   // PEDIA QR is reached via PEDIA AP -> ON
            openTarget(CfgScreen::PediaAp);                 // open the PEDIA AP toggle
            game.onButton({Button::A, true, false});        // focus ON
            game.onButton({Button::B, true, false});        // apply ON -> QR screen (page 0)
            if (hasFlag(argc, argv, "qr2"))                 // A cycles to page 1 ('Pedia URL)
                game.onButton({Button::A, true, false});
        }
        else if (hasFlag(argc, argv, "updates")) {
            // UPDATES: "ready" fills in what the platform tier would push at boot
            // (a stored network + a manifest source), so the screen shows the verdict
            // rather than the missing-setup copy. There is no consent to seed —
            // pressing CHECK NOW is what grants the connection.
            if (hasFlag(argc, argv, "ready")) {
                game.setNetProvisioned(true);
                // Setting the address is what marks the source known, so the FROM
                // host beside the installed version has something real to show.
                game.setUpdateManifestUrl("http://malwarium.local:8000/dist/manifest.json");
            }
            // "found" seeds the verdict a check would have pushed; "confirm" then
            // walks A/B onto the firmware row's yes-no, and "installing"/"failed"
            // stand in for a job the device tier would be driving.
            if (hasFlag(argc, argv, "found") || hasFlag(argc, argv, "confirm") ||
                hasFlag(argc, argv, "installing") || hasFlag(argc, argv, "failed")) {
                UpdateStatus st;
                st.state = UpdateState::Available;
                st.firmwareNewer = true;
                std::strcpy(st.firmwareVersion, "0.4.2");
                st.webNewer = true;
                std::strcpy(st.webVersion, "0.4.0");
                game.requestUpdateCheck();
                game.setUpdateStatus(st);
            }
            openTarget(CfgScreen::Update);
            if (hasFlag(argc, argv, "checking")) game.onButton({Button::B, true, false});
            // "nojoin" is the step before the fetch giving up: the association a
            // check raises for itself never comes up, which this screen now owns
            // reporting because nothing else asks for one.
            if (hasFlag(argc, argv, "nojoin")) {
                game.onButton({Button::B, true, false});     // CHECK NOW
                NetStatus down; down.state = NetState::Failed;
                game.setNetStatus(down);
            }
            if (hasFlag(argc, argv, "confirm")) {
                game.onButton({Button::A, true, false});   // row 0 -> FIRMWARE
                game.onButton({Button::B, true, false});   // -> the yes/no confirm
                if (hasFlag(argc, argv, "yes"))
                    game.onButton({Button::A, true, false});  // NO -> YES
            }
            if (hasFlag(argc, argv, "installing") || hasFlag(argc, argv, "failed")) {
                game.requestUpdateInstall(UpdateTarget::Firmware);
                InstallStatus in;
                in.target = UpdateTarget::Firmware;
                in.total = 1758361;
                if (hasFlag(argc, argv, "failed")) {
                    in.state = InstallState::Failed;
                    in.fail = InstallFail::Corrupt;
                    in.received = 1758361;
                } else {
                    in.state = InstallState::Downloading;
                    in.received = 703344;
                }
                game.setInstallStatus(in);
            }
            // The USB-flasher QR, which is the LAST row on the screen. Seeded on
            // its own rather than on top of "found": the flasher row needs only a
            // known source, and the state worth looking at is the one an operator
            // reaches before any check has run.
            if (hasFlag(argc, argv, "flashqr")) {
                game.setUpdateManifestUrl("https://antvena.github.io/Malwarium/manifest.json");
                game.onButton({Button::A, true, false});   // CHECK NOW -> FLASH OVER USB
                game.onButton({Button::B, true, false});   // -> the code
            }
        }
        else if (hasFlag(argc, argv, "factory")) {     // hold-B reveal off Sys Info
            game.onButton({Button::B, true, false});   // open Sys Info
            game.onButton({Button::B, true, false});   // press B (arm hold)
            game.tick(kFactoryRevealMs + kHeartbeatMs); // hold elapses -> reveal
        }
    } else if (hasFlag(argc, argv, "arch")) {
        enterSlot(SubmenuId::Arch);
        if (hasFlag(argc, argv, "stored"))           // focus the stored pet row
            game.onButton({Button::A, true, false});
        if (hasFlag(argc, argv, "detail")) game.onButton({Button::B, true, false});
        if (hasFlag(argc, argv, "confirm")) {        // open the Store/Deploy confirm
            game.onButton({Button::B, true, false}); // record -> confirm
        }
    } else if (hasFlag(argc, argv, "mods")) {
        // grant a tier-3 spare (equip gate ~L15-25) so the picker shows a
        // LOCKED row for a level-0 pet (the rolled equip-level gate, grayscale-safe).
        if (hasFlag(argc, argv, "modlocked")) game.debugGrantMod("overclock_chip");
        enterSlot(SubmenuId::Mods);
        game.onButton({Button::B, true, false});     // LOADOUT hub row 0 -> the mod slots
        if (hasFlag(argc, argv, "picker")) game.onButton({Button::B, true, false});
        if (hasFlag(argc, argv, "detail")) {         // mod detail (a ONE-SHOT)
            game.onButton({Button::A, true, false}); // slot 1 -> slot 2
            game.onButton({Button::A, true, false}); // slot 2 -> slot 3 (empty)
            game.onButton({Button::B, true, false}); // open slot 3 picker
            game.onButton({Button::A, true, false}); // first spare -> RAID Mirror
            game.onButton({Button::B, true, false}); // open the mod detail
        }
        if (hasFlag(argc, argv, "confirm")) {        // overwrite an installed mod
            game.onButton({Button::B, true, false}); // picker for slot 1 (Firewall)
            game.onButton({Button::A, true, false}); // -> a spare (RAID Mirror)
            game.onButton({Button::B, true, false}); // open detail
            game.onButton({Button::B, true, false}); // EQUIP -> overwrite confirm
        }
    } else if (hasFlag(argc, argv, "arcade")) {
        enterSlot(SubmenuId::Games);
        // cabinet → open the focused cabinet's page (L3); + hard → cycle the dial off
        // MEDIUM so the setting is visible; clutch/worm → focus that cabinet first.
        const int focus = hasFlag(argc, argv, "clutch") ? 1
                        : hasFlag(argc, argv, "worm") ? 2
                        : hasFlag(argc, argv, "decryption") ? 3 : 0;
        for (int i = 0; i < focus; ++i) game.onButton({Button::A, true, false});
        if (hasFlag(argc, argv, "cabinet")) {
            game.onButton({Button::B, true, false});
            if (hasFlag(argc, argv, "hard")) game.onButton({Button::A, true, false});
            if (hasFlag(argc, argv, "easy")) {             // MEDIUM -> HARD -> EASY
                game.onButton({Button::A, true, false});
                game.onButton({Button::A, true, false});
            }
            // play → start the run and take the board a few steps in.
            if (hasFlag(argc, argv, "play")) {
                game.onButton({Button::B, true, false});
                if (game.inDecryption()) {
                    for (int r = 0; r < 3; ++r)
                        for (int s = 0; s < kDecryptionSlots; ++s) {
                            for (int c = 0; c <= (r + s) % kDecryptionColours; ++c)
                                game.onButton({Button::A, true, false});
                            game.onButton({Button::B, true, false});
                        }
                }
                for (int i = 1; i <= 6; ++i)
                    game.tick(static_cast<uint32_t>(beats + i) * kHeartbeatMs);
                // result → stop the run there and land on the payout screen.
                if (hasFlag(argc, argv, "result"))
                    game.onButton({Button::C, true, false});
            }
        }
    } else if (hasFlag(argc, argv, "loadout")) {
        enterSlot(SubmenuId::Mods);                  // the LOADOUT hub itself
    } else if (hasFlag(argc, argv, "train")) {
        enterSlot(SubmenuId::Mods);
        game.onButton({Button::A, true, false});     // hub -> MOVES
        game.onButton({Button::B, true, false});     // -> the move slot list
        // trainpicker → open the focused slot's move picker (L3); + focus → step the
        // cursor onto a real move row (row 0 is unequip); movedetail → drill into that
        // move's own entry (L4).
        if (hasFlag(argc, argv, "trainpicker") || hasFlag(argc, argv, "movedetail")) {
            game.onButton({Button::B, true, false});
            if (hasFlag(argc, argv, "focus") || hasFlag(argc, argv, "movedetail"))
                game.onButton({Button::A, true, false});
            if (hasFlag(argc, argv, "movedetail")) game.onButton({Button::B, true, false});
        }
    } else if (hasFlag(argc, argv, "simbattle")) {
        enterSlot(SubmenuId::Mods);
        // Hub -> PRACTISE (row 2), which opens straight into the dummy-tier pick.
        game.onButton({Button::A, true, false});
        game.onButton({Button::A, true, false});
        game.onButton({Button::B, true, false});
        // "fight" takes the tier and runs the real fight — unlike the `combat` flag's
        // dev hook, this enters through Game::buildPlayerCombatant, so buffs carried
        // in from outside (a Backup Drive shield, the ally buff) are on the combatant.
        if (hasFlag(argc, argv, "fight")) {
            game.onButton({Button::B, true, false});
            for (int i = 1; i <= 4; ++i)
                game.tick(static_cast<uint32_t>(beats + i) * kHeartbeatMs);
            if (hasFlag(argc, argv, "stats"))
                game.onButton({Button::B, true, false});
        }
    } else if (hasFlag(argc, argv, "combat")) {
        if (hasFlag(argc, argv, "crew")) {           // enlist so the picker grows a crew row
            game.setHomeNetwork(0x001122334455ull, "HOME_ROUTER");
            game.joinCrew(0);
        }
        game.debugStartCombat(/*live=*/false);
        for (int i = 1; i <= 4; ++i)                 // advance a few action beats
            game.tick(static_cast<uint32_t>(beats + i) * kHeartbeatMs);
        if (hasFlag(argc, argv, "override"))         // open the A+C override picker
            game.onButton({Button::A, true, true});
        if (hasFlag(argc, argv, "stats"))            // B → open the mid-combat stat panel
            game.onButton({Button::B, true, false});
    } else if (hasFlag(argc, argv, "csf")) {
        // Critical System Failure: arm the ageing window, let it expire, then
        // hold the crash FX so B is active in the render.
        uint32_t t = 1000;
        game.tick(t);                                   // arm the dying window
        game.tick(t += kCsfDyingGraceMs);               // expire -> CSF modal
        for (int i = 0; i < kCsfHoldBeats; ++i)
            game.tick(t += kHeartbeatMs);               // hold the crash overlay
    } else if (hasFlag(argc, argv, "deepweb")) {
        // DeepWeb Dive: clear every area ("beat the game") so the terminal
        // row unlocks. `list` = the EXPL ladder with "DEEPWEB DIVE  > DIVE" now at the
        // TOP (row 0); otherwise arm the dive → the idle "EXPL DEEPWEB … DEPTH 0" badge.
        for (int a = 0; a < kExplSectors; ++a) {
            game.debugSetSectorCleared(a, true);
            for (int s = 0; s < kExplSubAreas; ++s) game.debugSetSubCleared(a, s, true);
        }
        if (hasFlag(argc, argv, "list")) {
            // DeepWeb is row 0 and, once unlocked, first-selectable — entering EXPL parks
            // the cursor directly on it (no cycling needed).
            enterSlot(SubmenuId::Expl);
        } else {
            game.debugStartDeepWebDive();
        }
    } else if (hasFlag(argc, argv, "farm")) {
        // re-farm: a CLEARED sub-area stays re-armable. Clear area 0 fully,
        // then arm a cleared sub — stays at the idle habitat so the FARMING badge shows
        // (no misleading WINS/BOSS progress toward an already-beaten boss).
        for (int s = 0; s < kExplSubAreas; ++s) game.debugSetSubCleared(0, s, true);
        if (hasFlag(argc, argv, "list"))
            enterSlot(SubmenuId::Expl);                  // the all-CLEARED farmable list
        else
            game.debugArmExplore(0, 2);                  // idle badge: EXPL ... FARMING
    } else if (hasFlag(argc, argv, "expl")) {
        // Seed a mid-ladder nested state so the EXPL list shows every tag at
        // once: area 0 has subs 1-2 CLEARED, sub 3 BOSS-READY (> FIGHT BOSS), subs 4-5
        // OPEN; area 1 stays LOCKED (its subs "??????"). Grayscale-safe row tags.
        // The list draws one nav LEVEL at a time, so the two levels are two frames:
        // without a flag it's the TOP-level zone picker, "inside" drills into area 0
        // for that area's own block (its gauntlet row + the five sub-areas).
        // "endgame" is the other end of the same screen: every area CLEARED, so the
        // DeepWeb row is a live "> DIVE" and each zone shows its own glyph (or the
        // pending-art frame) — the densest the top level ever gets. "bossready" clears
        // area 0's five sub-areas WITHOUT clearing the area, which is the one state that
        // names the area gauntlet's boss on its row.
        const bool rerun = hasFlag(argc, argv, "rerun");
        const bool bossReady = rerun || hasFlag(argc, argv, "bossready");
        if (hasFlag(argc, argv, "endgame")) {
            for (int a = 0; a < kExplSectors; ++a) {
                game.debugSetSectorCleared(a, true);
                for (int s = 0; s < kExplSubAreas; ++s) game.debugSetSubCleared(a, s, true);
            }
        } else if (bossReady) {
            for (int s = 0; s < kExplSubAreas; ++s) game.debugSetSubCleared(0, s, true);
            // "rerun" beats the gauntlet too, so its row shows the CLEARED area's
            // re-runnable form ("> RERUN") rather than the first-clear "> AREA BOSS".
            if (rerun) game.debugSetSectorCleared(0, true);
        } else {
            game.debugSetSubCleared(0, 0, true);
            game.debugSetSubCleared(0, 1, true);
            game.debugSetSubBossUnlocked(0, 2, true);
        }
        enterSlot(SubmenuId::Expl);
        // B drills into the focused ZONE — area 0 here, except in "endgame", where the
        // cursor rightly parks on the DeepWeb row and B arms the dive instead.
        if (bossReady || hasFlag(argc, argv, "inside"))
            game.onButton({Button::B, true, false});
    } else if (hasFlag(argc, argv, "hacker")) {
        // Hacker face (07): A+C flips PET → HACKER. Seed identity + economy so
        // the PROFILE viewer + SHOP list read populated. `profile`/`shop` open a slot;
        // `shop buy` also purchases the bandwidth upgrade so the price/pool step shows.
        // (Checked BEFORE the explore block, which also claims a "shop" flag.)
        game.debugSetBits(600);
        game.debugSetNetworksSeen(27);              // R2 + populated NETS
        game.setAuditCaptureEnabled(true);
        for (int i = 0; i < 3; ++i) {               // a few SHAKES
            uint8_t bssid[6] = {0x02, 0, 0, 0, 0, static_cast<uint8_t>(i)};
            game.registerHandshake(bssid);
        }
        game.onButton({Button::A, true, true});     // A+C chord → hacker home (idle)
        auto enterHackerSlot = [&](HackerSlotId id) {
            game.onButton({Button::A, true, false}); // summon hacker cursor @ slot 0
            while (hackerCarouselSlots()[game.cursor()].id != id)
                game.onButton({Button::A, true, false});
            game.onButton({Button::B, true, false});
        };
        if (hasFlag(argc, argv, "profile")) enterHackerSlot(HackerSlotId::Profile);
        else if (hasFlag(argc, argv, "duel")) {
            // LINK's peer list. There is no radio on the host, so the three peers
            // come in through debugSeedPeer — one per side plus an unenlisted
            // operator, which is the set that shows all three crew presentations
            // at once. Audit capture (armed above for the other hacker slots) owns
            // the radio when it runs, and LINK refuses to draw a list it could not
            // have gathered, so it goes back off here.
            game.setAuditCaptureEnabled(false);
            const PeerHello peers[] = {
                {"GHOSTKEY", "Paypup", "DENIERS OF SERVICE", 2, 4, false},
                {"NULLRIDER", "Malbear", "EIGHT PWNS", 3, 7, true},
                {"LURKER_7", "CryptoShell", "", 1, 1, false},
            };
            for (size_t i = 0; i < sizeof(peers) / sizeof(peers[0]); ++i)
                game.debugSeedLivePeer(0xBEEF0000ull + i, peers[i]);
            enterHackerSlot(HackerSlotId::Link);
        }
        else if (hasFlag(argc, argv, "shop")) {
            // "hub" buys the MERGE HUB first, which is what makes its two recipe
            // rows appear at all (RigUpgradeDef::requiresRow) — the way to see the
            // list with and without them. "row:<n>" then A-cycles down to reach them.
            if (hasFlag(argc, argv, "hub")) { game.debugSetBits(99999); game.debugBuyMergeHub(); }
            enterHackerSlot(HackerSlotId::Shop);
            for (int i = 3; i < argc; ++i)
                if (std::strncmp(argv[i], "row:", 4) == 0)
                    for (int k = std::atoi(argv[i] + 4); k > 0; --k)
                        game.onButton({Button::A, true, false});
            if (hasFlag(argc, argv, "buy")) game.onButton({Button::B, true, false});
        } else if (hasFlag(argc, argv, "merge")) {
            // MERGE HUB: the slot is a Rig Shop purchase, so buy it before entering
            // or the carousel gates the slot. "recipes" also buys both recipe rows
            // (they gate on the hub, so they aren't even offered until it's owned),
            // and "stock" fills the bag so a row reads craftable rather than short.
            game.debugSetBits(99999);
            game.debugBuyMergeHub();
            if (hasFlag(argc, argv, "recipes")) {
                game.debugBuyRecipe(0);
                game.debugBuyRecipe(1);
            }
            if (hasFlag(argc, argv, "stock")) {
                game.inventory().add("null_noodles", 4);
                game.inventory().add("pwnzu_sauce", 4);
            }
            enterHackerSlot(HackerSlotId::Merge);
        } else if (hasFlag(argc, argv, "vault")) {
            // VAULT: stock one of every container tier, deliberately added in the
            // WRONG order — the list's own sort is what should land Epic on top.
            game.inventory().add("sealed_cache_common", 3);
            game.inventory().add("sealed_cache_epic", 1);
            game.inventory().add("sealed_cache_uncommon", 2);
            game.inventory().add("commend_cache", 1);
            game.inventory().add("sealed_cache_rare", 2);
            enterHackerSlot(HackerSlotId::Vault);
        } else if (hasFlag(argc, argv, "crew")) {
            // CREW: seed known networks (ledger history) plus a live scan sighting, so
            // the home-network picker shows both halves of its merged list. `joined`
            // enlists first (the allegiance card's filled state); `netpick` opens the
            // picker; `red`/`blue` open that side's roster and `detail` a crew's page
            // — the four views of the screen, one flag each.
            game.debugSeedNetworkLedger(0x001122334455ull, "HOME_ROUTER", 12);
            game.debugSeedNetworkLedger(0x0066778899AAull, "CAFE_GUEST", 3);
            const uint8_t heard[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
            game.registerNetwork(heard, "HOME_ROUTER");    // history + in range now
            const uint8_t fresh[6] = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
            game.registerNetwork(fresh, "NEW_NEIGHBOUR");  // live only, never walked
            game.setHomeNetwork(0x001122334455ull, "HOME_ROUTER");
            if (hasFlag(argc, argv, "joined")) game.joinCrew(0);
            if (hasFlag(argc, argv, "unset")) game.setHomeNetwork(0, "");
            enterHackerSlot(HackerSlotId::Crew);
            const bool red = hasFlag(argc, argv, "red");
            if (hasFlag(argc, argv, "netpick")) {
                game.onButton({Button::B, true, false});   // Hub row 0 -> the modal
            } else if (red || hasFlag(argc, argv, "blue")) {
                game.onButton({Button::A, true, false});   // Hub row 1 = RED...
                if (!red)
                    game.onButton({Button::A, true, false});   // ...row 2 = BLUE
                game.onButton({Button::B, true, false});
                // `row:<n>` walks the side's cursor first, so a page other than that
                // side's first crew is reachable.
                for (int i = 3; i < argc; ++i)
                    if (std::strncmp(argv[i], "row:", 4) == 0)
                        for (int k = std::atoi(argv[i] + 4); k > 0; --k)
                            game.onButton({Button::A, true, false});
                if (hasFlag(argc, argv, "detail"))
                    game.onButton({Button::B, true, false});
            }
        }
    } else if (hasFlag(argc, argv, "explore") || hasFlag(argc, argv, "explorectl") ||
              hasFlag(argc, argv, "walk") || hasFlag(argc, argv, "encounter") ||
              hasFlag(argc, argv, "wildcombat") || hasFlag(argc, argv, "wifi") ||
              hasFlag(argc, argv, "rank") || hasFlag(argc, argv, "shop") ||
              hasFlag(argc, argv, "warp") || hasFlag(argc, argv, "postencounter")) {
        // Explore-mode: arm sector 0 → the game drops back to the IDLE
        // habitat with the explore badge live. There is no walk screen; a step is
        // driven by the A+C control chord's Network Ping (A+C → A), which fires the
        // next guaranteed event NOW. `ping` is that deterministic single-step.
        if (hasFlag(argc, argv, "rank")) {
            // Cross the first rank deterministically via the DEVICE seam
            // (registerNetwork isn't limited by the 8-slot simulated dedup pool),
            // arming rankUpPending — the celebration then surfaces on the idle badge
            // the next time an event resolves through returnToExplore.
            for (int k = 0; k < kHackerRankXpPerRank / kHackerRankXpPerNetwork + 2; ++k) {
                const uint8_t bssid[6] = {0xAB, 0xCD, 0xEF, 0x00, 0x00,
                                          static_cast<uint8_t>(k)};
                game.registerNetwork(bssid, "TestNet");
            }
        }
        enterSlot(SubmenuId::Expl);
        // The ladder is nested: the first B expands sector[0], the second arms the
        // sub-area the cursor lands on — and arming is what drops the game back to
        // the IDLE habitat with the explore badge live.
        game.onButton({Button::B, true, false});     // expand sector[0]
        game.onButton({Button::B, true, false});     // arm sub-area[0] -> idle explore-mode
        auto ping = [&]{
            game.onButton({Button::A, true, true});  // A+C chord -> overlay, on PING
            game.onButton({Button::B, true, false}); // B -> do it (the next event)
        };
        // The overlay opens on PING; AUTO-PROGRESS is two rows down.
        auto armAuto = [&]{
            game.onButton({Button::A, true, true});  // A+C -> the control overlay
            game.onButton({Button::A, true, false}); // -> WARP
            game.onButton({Button::A, true, false}); // -> AUTO-PROGRESS
            game.onButton({Button::B, true, false}); // arm it
        };
        uint32_t t = static_cast<uint32_t>(beats) * kHeartbeatMs;
        if (hasFlag(argc, argv, "explorectl")) {
            game.inventory().add("access_token", 1); // so WARP shows enabled
            // "auto" walks to the AUTO-PROGRESS row and arms it, which is also what
            // sets the carousel's EXPL globe spinning ("explore auto", below); without
            // it the overlay rests on its first row.
            if (hasFlag(argc, argv, "auto")) armAuto();
            else game.onButton({Button::A, true, true});   // A+C -> the control overlay
        } else if (hasFlag(argc, argv, "auto")) {
            // The armed habitat with auto-progress running: the EXPL globe turns on the
            // shelf. Pass a `beats` count to land on a particular frame of the spin.
            armAuto();
            game.onButton({Button::C, true, false}); // C -> back to the habitat
        } else if (hasFlag(argc, argv, "warp")) {
            // Warp picker: hold both keys, open the control overlay, walk to WARP.
            game.inventory().add("access_token", 1);
            game.inventory().add("safe_mode_key", 1);
            game.onButton({Button::A, true, true});  // A+C -> overlay, on PING
            game.onButton({Button::A, true, false}); // A -> WARP
            game.onButton({Button::B, true, false}); // B -> warp-key picker
        } else if (hasFlag(argc, argv, "encounter") || hasFlag(argc, argv, "wildcombat")) {
            for (int i = 0; i < 400 && game.nav() != Game::Nav::Encounter; ++i) {
                if (game.nav() == Game::Nav::Wifi)
                    game.onButton({Button::B, true, false});
                else if (game.nav() == Game::Nav::Shop)
                    game.onButton({Button::C, true, false});
                else if (game.nav() == Game::Nav::Combat) {
                    for (int j = 0; j < 800 &&
                            game.combat().outcome() == Combat::Outcome::Ongoing; ++j)
                        game.tick(t += kHeartbeatMs);
                    game.onButton({Button::B, true, false});
                } else ping();
            }
            if (hasFlag(argc, argv, "wildcombat") && game.nav() == Game::Nav::Encounter)
                game.onButton({Button::B, true, false});  // Fight -> live combat
        } else if (hasFlag(argc, argv, "wifi")) {
            game.inventory().add("sinkhole_trap", 20);   // bypass wild encounters, free
            for (int i = 0; i < 400 && game.nav() != Game::Nav::Wifi; ++i) {
                if (game.nav() == Game::Nav::Encounter) {
                    game.onButton({Button::A, true, false});  // Fight -> Flee
                    game.onButton({Button::A, true, false});  // Flee -> Sinkhole
                    game.onButton({Button::B, true, false});  // confirm -> back to idle
                } else if (game.nav() == Game::Nav::Shop) {
                    game.onButton({Button::C, true, false});
                } else ping();
            }
        } else if (hasFlag(argc, argv, "rank")) {
            game.inventory().add("sinkhole_trap", 20);
            // Rank already crossed (device seam above). Fire steps until the first
            // FULL-SCREEN event appears and resolve it — that returnToExplore consumes
            // rankUpPending and writes the celebration onto the idle badge.
            for (int i = 0; i < 600 && game.nav() == Game::Nav::Idle; ++i) ping();
            switch (game.nav()) {
                case Game::Nav::Encounter:
                    game.onButton({Button::A, true, false});  // Fight -> Flee
                    game.onButton({Button::A, true, false});  // Flee -> Sinkhole
                    game.onButton({Button::B, true, false});  // confirm -> back to idle
                    break;
                case Game::Nav::Combat:
                    for (int j = 0; j < 400 &&
                            game.combat().outcome() == Combat::Outcome::Ongoing; ++j)
                        game.tick(t += kHeartbeatMs);
                    game.onButton({Button::B, true, false});
                    break;
                case Game::Nav::Wifi: game.onButton({Button::B, true, false}); break;
                case Game::Nav::Shop: game.onButton({Button::C, true, false}); break;
                default: break;
            }
        } else if (hasFlag(argc, argv, "shop")) {
            game.inventory().add("sinkhole_trap", 20);
            for (int i = 0; i < 400 && game.nav() != Game::Nav::Shop; ++i) {
                if (game.nav() == Game::Nav::Encounter) {
                    game.onButton({Button::A, true, false});
                    game.onButton({Button::A, true, false});
                    game.onButton({Button::B, true, false});
                } else if (game.nav() == Game::Nav::Wifi) {
                    game.onButton({Button::B, true, false});
                } else if (game.nav() == Game::Nav::Combat) {
                    for (int j = 0; j < 800 &&
                            game.combat().outcome() == Combat::Outcome::Ongoing; ++j)
                        game.tick(t += kHeartbeatMs);
                    game.onButton({Button::B, true, false});
                } else ping();
            }
        } else if (hasFlag(argc, argv, "postencounter")) {
            // Ride a wild fight to resolution, then let explore-mode's own
            // hands-off auto-dismiss land the frame on the
            // post-encounter STATUS overlay instead of
            // pressing past it — the captured frame IS the readout screen.
            for (int i = 0; i < 400 && game.nav() != Game::Nav::Combat; ++i) {
                if (game.nav() == Game::Nav::Wifi)
                    game.onButton({Button::B, true, false});
                else if (game.nav() == Game::Nav::Shop)
                    game.onButton({Button::C, true, false});
                else ping();
            }
            for (int j = 0; j < 800 &&
                    game.combat().outcome() == Combat::Outcome::Ongoing; ++j)
                game.tick(t += kHeartbeatMs);
            for (int k = 0; k < kExploreRevealHoldBeats + 4 &&
                    game.nav() == Game::Nav::Combat; ++k)
                game.tick(t += kHeartbeatMs);   // auto-dismiss -> Nav::PostEncounter
        }
        // ("explore" and "walk": no further driving — the idle badge is the frame.)
    } else if (hasFlag(argc, argv, "bottom")) {   // idle C → carousel@8
        game.onButton({Button::C, true, false});
    } else if (hasFlag(argc, argv, "carousel")) { // idle A → carousel@1
        game.onButton({Button::A, true, false});
    } else if (hasFlag(argc, argv, "decryption")) {
        // A Ransomware egg opens straight onto its DISK DECRYPTION board. "rows" plays
        // three attempts first, so the frame lands on a board with history to read;
        // "lost" plays all five, landing on the verdict + the revealed key.
        game.resetToHatch();
        if (game.inLineSelect()) game.onButton({Button::B, true, false});
        const int rows = hasFlag(argc, argv, "lost") ? kDecryptionAttempts
                       : hasFlag(argc, argv, "rows") ? 3 : 0;
        for (int r = 0; r < rows; ++r)
            for (int s = 0; s < kDecryptionSlots; ++s) {
                for (int c = 0; c <= (r + s) % kDecryptionColours; ++c)
                    game.onButton({Button::A, true, false});
                game.onButton({Button::B, true, false});
            }
    } else if (hasFlag(argc, argv, "clutch")) {
        // Lay a Phishing egg, which opens its Clutch Pick. "aim" flips to the second
        // half; each "round" commits one halving, so "clutch round round round" lands
        // the frame on the reveal.
        game.unlockAchievement(ach::kDeepWebDepth8);   // unlocks Phishing
        game.resetToHatch();
        game.onButton({Button::A, true, false});   // cycle line-select to Phishing
        game.onButton({Button::B, true, false});   // lay it -> Nav::ModalEggPick
        if (hasFlag(argc, argv, "win")) {
            // Play it perfectly to land on the won reveal: aim at whichever half holds
            // the live egg each round, following game_eggpick.cpp's alternating cut.
            const int col = game.eggPickTargetSlot() % Game::kEggPickCols;
            const int row = game.eggPickTargetSlot() / Game::kEggPickCols;
            int c0 = 0, cw = Game::kEggPickCols, r0 = 0, rh = Game::kEggPickRows;
            for (int i = 0; i < Game::kEggPickRounds; ++i) {
                bool second;
                if (i % 2 == 0) { cw /= 2; second = col >= c0 + cw; if (second) c0 += cw; }
                else            { rh /= 2; second = row >= r0 + rh; if (second) r0 += rh; }
                game.onButton({second ? Button::C : Button::A, true, false});
                game.onButton({Button::B, true, false});
            }
        } else {
            for (int i = 3; i < argc; ++i) {
                // "round" commits the current aim; "aim" flips to the second half, so
                // the two interleave to walk any path through the clutch.
                if (std::strcmp(argv[i], "aim") == 0)
                    game.onButton({Button::C, true, false});
                else if (std::strcmp(argv[i], "round") == 0)
                    game.onButton({Button::B, true, false});
            }
        }
    } else if (hasFlag(argc, argv, "isolation")) {
        // Lay a Worm egg, which opens its Isolation Protocol on the spot.
        game.unlockAchievement(ach::kSecondInstance);   // unlocks the Worm line
        game.resetToHatch();
        game.onButton({Button::A, true, false});   // cycle line-select to Worm
        game.onButton({Button::B, true, false});   // lay it -> Nav::Isolation
        uint32_t it = 0;
        if (hasFlag(argc, argv, "crash")) {
            game.onButton({Button::A, true, false});    // turn into the near wall
            for (int i = 0; i < 4 * kIsolationRows && game.isolation().running(); ++i)
                game.tick(it += kIsolationStepMs);
        } else {
            // Follow the buffer's Hamiltonian cycle (row 0 is the return corridor, every
            // column below it walked down when even and up when odd) so the worm eats
            // rather than crashes, and the frame shows a coil worth looking at.
            int steps = 0;
            for (int i = 3; i < argc; ++i)
                if (std::strncmp(argv[i], "steps:", 6) == 0) steps = std::atoi(argv[i] + 6);
            for (int i = 0; i < steps && game.isolation().running(); ++i) {
                const int cell = game.isolation().head();
                const int col = cell % kIsolationCols, row = cell / kIsolationCols;
                int next;
                if (row == 0) next = col > 0 ? cell - 1 : kIsolationCols;
                else if (col == kIsolationCols - 1 && row == 1) next = col;
                else if (col % 2 == 0)
                    next = row < kIsolationRows - 1 ? cell + kIsolationCols : cell + 1;
                else next = row > 1 ? cell - kIsolationCols : cell + 1;
                const int d = next - cell;
                const int want = d == 1 ? 0 : d == kIsolationCols ? 1 : d == -1 ? 2 : 3;
                const int turn = (want - game.isolation().dir() + 4) % 4;
                if (turn == 3) game.onButton({Button::A, true, false});
                else if (turn) game.onButton({Button::B, true, false});
                game.tick(it += kIsolationStepMs);
            }
        }
        // "bank" takes the verdict's B, which spends the run and leaves the Vermicell
        // egg incubating at idle — the state the whole minigame hands back to.
        if (hasFlag(argc, argv, "bank") && !game.isolation().running())
            game.onButton({Button::B, true, false});
    } else if (hasFlag(argc, argv, "hatchreveal")) {
        // Run a Phishing egg's incubation down into the reveal window, then crack it
        // with the chord. "frame:<n>" holds the cinematic on that frame of the one-shot.
        game.unlockAchievement(ach::kDeepWebDepth8);
        game.resetToHatch();
        game.onButton({Button::A, true, false});
        game.onButton({Button::B, true, false});
        for (int i = 0; i < Game::kEggPickRounds; ++i) {
            game.onButton({Button::A, true, false});
            game.onButton({Button::B, true, false});
        }
        game.onButton({Button::B, true, false});     // out of the pick, egg at idle
        uint32_t rt = 1000;
        game.tick(rt);
        game.tick(rt += kBootHatchMs - kHatchRevealMs / 2);
        game.onButton({Button::C, true, true});      // A+C -> the crack cinematic
        int hold = 0;
        for (int i = 3; i < argc; ++i)
            if (std::strncmp(argv[i], "frame:", 6) == 0) hold = std::atoi(argv[i] + 6);
        for (int i = 0; i < hold; ++i) game.tick(rt += kHeartbeatMs);
    } else if (evolveFlag) {                        // force + advance to the reveal
        game.debugTriggerEvolution();
        const int n = kEvoHoldBeats + kEvoFlashBeats + 1;
        for (int i = 1; i <= n; ++i)
            game.tick(static_cast<uint32_t>(beats + i) * kHeartbeatMs);
    }

    Framebuffer fb(kActiveW, kActiveH);   // active canvas is the compositor
    game.render(fb);

    std::vector<Rgb565> panel(static_cast<size_t>(kPanelW) * kPanelH,
                              palColor(Pal::PAPER));
    for (int y = 0; y < kActiveH; ++y)
        for (int x = 0; x < kActiveW; ++x)
            panel[static_cast<size_t>(y + kBezel) * kPanelW + (x + kBezel)] =
                fb.get(x, y);

    FILE* f = std::fopen(out, "wb");
    if (!f) { std::perror("fopen"); return 1; }
    std::fprintf(f, "P6\n%d %d\n255\n", kPanelW, kPanelH);
    for (Rgb565 c : panel) {
        unsigned char rgb[3] = {r8(c), g8(c), b8(c)};
        std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
    std::printf("wrote %s (beat %d)\n", out, beats);
    return 0;
}
