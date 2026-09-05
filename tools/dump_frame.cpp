// dump_frame.cpp — headless single-frame renderer for visual verification and
// (later) screenshot baselines. Links only malcore (no SDL). Writes a binary
// PPM of the full 240 panel:
//   ./dump_frame [beats] [out.ppm] [flags...]
// flags: hungry · crit · stat · log · carousel · bottom · iconsonly · textonly
//        scene:<name> [floor:<row>] (a BACKDROP on its own, with no screen over it —
//             the names are core/render/scenes.h's, e.g. scene:pirate_bayou. `floor`
//             is the row the ground puts under it, which is the whole reason to look:
//             140 is where fighters stand and 184 where a resting pet does, and a
//             scene has to compose at both. Defaults to 184.)
//        items [picker [ingredients|keys|tools]] [row:<n>] [detail|feed] (row:<n> walks the list
//             cursor first, so a detail other than the first row is renderable)
//        feed:<item_id> (eat one named food through the real Use path and hold the
//             feeding modal — how to eyeball that its gauges follow that item's own
//             effects, e.g. feed:tortilla_chip, feed:null_noodles)
//        stat [tiers|loadout|buffs|species|log] [scroll:<n>] (the 6 STAT pages;
//             "scroll:<n>" takes B n times, which walks the row window of the three pages
//             that flow prose — pair it with "fullkit" and a Daemon for the longest lists.
//             TIERS is the investment ladder, and the pet the preset levels has real
//             points in it, so the page shows held rungs and not just locked ones)
//        fullkit (every unlocked move slot and every mod slot equipped)
//        pantry (one of EVERY item in the bag — the whole icon set in a list, and the
//             deepest the combat picker's ITEMS band gets)
//        shop | modshop [full] (an area's two storefronts — the ITEM shop and the MOD
//             shop, one screen either way. "full" tops the mod pool up to
//             modStorageCap() first, which is the state the row's HAVE n/cap column and
//             the STORAGE FULL buy reason exist for)
//        maint [detail] [stacker [slide|drop|stop ...]] · lockout · evolve
//        cryptogram [open:<n>] [take] [win|lose] (THE DECRYPTOGRAM's quote board, cashed
//             at the VAULT; "open:<n>" places n letters correctly so the frame shows a
//             part-solved quote, "take" leaves a letter in hand for the cell-cursor
//             control state, "win" plays it out to the attribution + prize and "lose"
//             misplaces one to hold the verdict)
//        decryption [rows|lost] (the Ransomware egg's DISK DECRYPTION board; "rows"
//             plays three attempts so the history and its corruption overlay are on
//             screen, "lost" plays all five and holds the verdict + revealed key)
//        arcade [solved] [clutch|worm|decryption|quote|chroma] [cabinet [hard]
//             [play [result]]]
//             (the GAMES list, one cabinet's page, its running board, and the payout;
//             "solved" banks the eight quote wins that reveal the DECRYPTOGRAM cabinet,
//             which "quote" then focuses)
//        clutch [aim] [round ...] | clutch win | clutch lose (the Phishing egg's Clutch
//        Pick; "aim" flips to the second half and "round" commits, interleaved to walk
//        any path — the run now resolves the instant the live egg leaves the surviving
//        span, so a "round" past that point CONTINUES the reveal instead of playing
//        another one; "win" plays it perfectly, "lose" aims wrong on purpose so it
//        resolves deterministically on round one)
//        isolation [steps:<n>] [crash] [bank] (the Worm egg's Isolation Protocol;
//             "steps:<n>" walks the worm n moves along the buffer's Hamiltonian cycle so
//             the frame shows a long coil mid-run, "crash" drives it into the wall to
//             hold the verdict, and "bank" takes the B off it to leave the Vermicell egg
//             at idle)
//        chroma [wear|half|spotted] [clean] [bank] (the Metamorphic egg's CHROMATOPHORE;
//             "wear" settles into the water for the hidden frame, "half" holds the
//             repaint mid-scatter, "spotted" wears the wrong skin into the sweep,
//             "clean" plays every round and holds the verdict, "bank" takes the B off
//             it to leave the Polystaria egg at idle)
//        ach | achburst | acheggline (the unlock announcement over the idle habitat:
//             one row by name, a collapsed burst, or the HELD new-egg-line plate that
//             waits for a button instead of timing out)
//        hatchreveal [frame:<n>] (the on-demand crack cinematic, held on frame n)
//        cfg updates [ready] [checking|nojoin|found|confirm [yes]|installing|failed|
//             flashqr] (the UPDATES screen; without "ready" it shows which setup step
//             is missing, and "flashqr" walks onto the last row and opens the USB
//             flasher's code)
//        cfg [sysinfo|tag|titles|device|uimode|brightness|background [earned]|travel [sleeping]|
//             radio [idle|all]|audit|
//             link|pediaap|qr|factory] (the settings tree; device/radio are the two
//             group screens, and radio is seeded with a live arbiter owner —
//             "idle" seeds nothing on air, "all" seeds every toggle on under a
//             running update job)
//        arch [stored] [rackfull] [group:<n>] [row:<n>] [detail] [confirm] (ARCH opens
//             on its GROUP PICKER — NEW EGG · ACTIVE · one row per creature family ·
//             RECORDS — so "group:<n>" is what opens a shelf and "row:<n>" then walks
//             it; rackfull buys slots
//             and fills them, so the list overflows kVisibleRows and scrolls;
//             row:<n> walks the cursor down it)
//        train [trainpicker] · combat [override [band:<n>]] [stats] [kit] (the raw dev hook;
//             stats opens the panel's STATE page, kit its second) ·
//        simbattle [fight [stats]] (the REAL entry — buffs carried in from outside,
//             e.g. armbuffs simbattle fight stats) · malbear
//        bruinforce (Good Daemon) · berserkernel (Bad Daemon) · csf (Critical Failure)
// expl [inside|bossready|endgame] (the nested area/sub-area ladder — one nav
//        LEVEL per frame: the top-level zone picker, "inside" for area 0's own block,
//        "bossready" for that block with its gauntlet unlocked, "rerun" for it already
//        beaten, "endgame" for the every-area-cleared picker) ·
//        explore [cachefind] (armed → the idle explore badge; "cachefind" states the longest
//             flavor line the walk composes, so the wrap under the BW readout is visible)
// dock [fight|deep|scout|brief] (ROCK THE DOCK's arena screen — the eight-operator
//        bracket; "fight" plays the operator's own first bout out so the frame shows a
//        settled round, "deep" plays the bracket as far forward as the pet can carry it
//        so the field has collapsed and the tree has narrowed, "scout" holds B into the
//        focused entrant's kit sheet, "brief" chords into the paged explainer)
// explorectl [auto] (the A+C control overlay; "auto" arms AUTO-PROGRESS with the
//        second chord) · explore auto (the armed habitat with it running — the EXPL
//        globe spins, so pass a `beats` count to land on a frame) · encounter [sinkhole] ·
//        wildcombat (arm → Network Ping to a wild intro → live combat; sinkhole seeds
//        a Sinkhole Trap so the 3rd intro option shows) · walk (alias of explore)
// wifi [new|fond|hometurf] (arm → ping to the Wi-Fi network event,; bounded search,
//        auto-Sinkholes through any Wild encounters rolled along the way. The three
//        named forms seed the ledger so the discovery beat resolves that way, which is
//        what picks how far the pet eats the network glyph — pass a `beats` count to
//        land on a frame of the absorb; bare "wifi" is the empty-queue beat)
// shibboleth [hail|verdict [wrong]|refused|boon] [fight] [sigils:<n>] (the guardian
//        encounter's screens. Bare "shibboleth" is the RIDDLE drawn in the CANT; "hail" is
//        the beat before it and "verdict" the one after — what the guardian made of the
//        answer, with "wrong" picking a reply it will not take. "refused" and "boon"
//        search for the two bands that never ask anything at all — the refusal and the
//        quiet word — and land on the verdict each resolves onto ("boon" wants a high
//        `sigils:` to be reachable). "fight" carries on into the guardian's own COMBAT,
//        where its SWARM holds the rival seat.
//        `sigils` is how many letters the pet has learned to read, 0..26 — at 0 the panel
//        is nonsense and at 26 it is plain English, with every stage between. `beats`
//        steps the swarm — on the FX clock on the meeting screens, on the stage's own
//        clock in a fight — and walks the riddle board's patience bar down)
// beats:<n> (any scene: override the positional beat count, for a scene whose subject has
//        to be animated into its shape before the frame is worth taking)
// rank (Hacker Rank rank-up celebration on the idle badge,; crosses
//        a rank via registerNetwork, then resolves one event to surface it)
//        postencounter (a wild fight ridden to its own auto-dismiss, landing the
//        frame on Nav::PostEncounter — the bandwidth/frag STATUS readout)
// outro [known] (a wild fight ridden to a WIN and held ON the combat screen, so the
//        frame lands inside the beaten rival's dissolve — pass a `beats` count to walk
//        it. Bare "outro" shows whichever the pet's kit earns; "known" grants the
//        rival's moves first, which turns the absorb back into a shred)
// hacker (A+C → the Hacker face home) · hacker profile [decorated] (the PROFILE
// viewer; "decorated" seeds the widest identity it can hold — the longest crew
// name, a Title equipped, and the rank that unlocks the longest rank title) · hacker shop [hub] [row:<n>] [buy] (the SHOP; "hub" buys the MERGE HUB
//        first, which is what reveals its two recipe rows, and row:<n> A-cycles
//        down the list to reach them) ·
// hacker merge [recipes] [stock] (the MERGE HUB craft list — the slot is itself a
//        SHOP purchase, so the scene buys it before entering) ·
// hacker vault (the VAULT cash-in list, stocked with a Decryptogram ticket and
//        every container tier) ·
// hacker crew [joined] [unset] [netpick] [red|blue [row:<n>] [detail]] (the CREW
//        screen's four views: the Hub, the home-network picker, one side's roster,
//        and a crew's own page)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "tunables.h"
#include "core/app/game.h"
#include "core/content/content_arcade.h"
#include "core/content/content_crews.h"   // kCrews — the widest name the PROFILE holds
#include "core/app/game_internal.h"   // kMergeRecipeCount — the MERGE HUB dump wins them all
#include "core/app/game_rig_shop.h"    // kRigSlotServices — the SHOP's SERVICES head slot
#include "core/model/hacker_rank.h"   // the rank ladder, for the widest title on it
#include "core/render/canvas.h"
#include "core/render/font.h"         // textWidth — picking the widest of a table
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/render/scenes.h"
#include "core/ui/carousel.h"
#include "core/ui/cfg_screen.h"
#include "core/ui/expl_screen.h"

using namespace mal;

// The CHROMATOPHORE's three buttons are three skins, in chip order — so a dump that
// wants a particular skin worn presses the button that wears it rather than a cursor.
static ButtonEvent chromaPress(int skin) {
    const Button b = skin == 0 ? Button::A : skin == 1 ? Button::B : Button::C;
    return {b, true, false};
}

// A complete C press — a list reads C as a tap/hold pair, so the press edge alone
// settles nothing (CONTRIBUTING.md's button contract).
static void tapC(mal::Game& g) {
    g.onButton({mal::Button::C, true, false});
    g.onButton({mal::Button::C, false, false});
}

static bool hasFlag(int argc, char** argv, const char* f) {
    for (int i = 3; i < argc; ++i)
        if (std::strcmp(argv[i], f) == 0) return true;
    return false;
}

// The active canvas composed into the full panel and written out as binary PPM — the
// one exit both the screen path and the bare-backdrop path below take.
static int writePanel(const Framebuffer& fb, const char* out, int beats) {
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

int main(int argc, char** argv) {
    int beats = (argc > 1) ? std::atoi(argv[1]) : 0;
    const char* out = (argc > 2) ? argv[2] : "frame.ppm";
    // A SCENE may name its own beat count, which the positional one cannot express: the
    // contact sheet runs every screen at the same beat (tools/screens.sh), and a scene
    // whose subject has to be animated INTO its shape — a guardian's swarm settling out
    // of the scatter it was reset from — is otherwise stuck being dumped mid-arrival.
    for (int i = 3; i < argc; ++i)
        if (std::strncmp(argv[i], "beats:", 6) == 0) beats = std::atoi(argv[i] + 6);

    // A backdrop on its own, with no screen composed over it. Nothing about a scene
    // needs a Game, and the whole question a look at one answers — does it still read
    // with its floor somewhere else — is a question about the ground it is handed.
    for (int i = 3; i < argc; ++i) {
        if (std::strncmp(argv[i], "scene:", 6) != 0) continue;
        const SceneId id = sceneByName(argv[i] + 6);
        if (id == SceneId::None) {
            std::fprintf(stderr, "no scene named '%s'\n", argv[i] + 6);
            return 2;
        }
        int floorY = kLivingBottom;
        for (int j = 3; j < argc; ++j)
            if (std::strncmp(argv[j], "floor:", 6) == 0) floorY = std::atoi(argv[j] + 6);
        Framebuffer fb(kActiveW, kActiveH);
        drawScene(fb, id, beats, sceneGround(floorY));
        return writePanel(fb, out, beats);
    }

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
    // ...and a much deeper grind for the TIERS page, which is about the far end of that
    // curve: at ~level 8 every rung on it reads "TO GO" and the page cannot show what a
    // held one looks like. The grant picks its stat at random, so a level in the forties
    // is what reliably puts SOME stat past the first rung and usually a second.
    if (hasFlag(argc, argv, "tiers")) game.debugAddCombatXp(400000);
    if (hasFlag(argc, argv, "maint")) {
        game.model().setFragmentation(31);   // something to defragment
        game.model().setGhost(true);         // AV has work to do
        game.inventory().add("disk_scrubber", 2);  // show the TOOL variant
    }
    if (hasFlag(argc, argv, "iconsonly")) game.setUiMode(UiMode::IconsOnly);
    if (hasFlag(argc, argv, "textonly")) game.setUiMode(UiMode::TextOnly);
    // ARCH rack: seed a frozen stored pet so the rack list / Deploy record render.
    if (hasFlag(argc, argv, "stored")) game.debugSeedRack("cryptoshell");
    // ...and the overflowing rack: buy past kRackSlots and fill every slot, which is
    // the state the list has to WINDOW rather than draw straight down the screen.
    if (hasFlag(argc, argv, "rackfull")) {
        game.debugSetBits(1 << 16);
        for (int i = 0; i < 4; ++i) game.debugBuyRackSlotUpgrade();
        static const char* const kSeed[] = {"cryptoshell", "phishlet", "tadpoll",
                                            "croaken",     "paypup",   "malbear",
                                            "clickbait",   "spamwhale"};
        for (const char* id : kSeed) game.debugSeedRack(id);
    }
    // CSF: drive the pet to the 5/5 dying state so the ageing window can expire.
    if (hasFlag(argc, argv, "csf")) game.model().setCareMistakes(kCareDying);
    // Achievement banner: earn one and let the next tick raise its announcement over the
    // idle habitat. "achburst" earns enough at once to trip the collapse-into-a-summary
    // threshold instead, which is the other shape the banner takes.
    if (hasFlag(argc, argv, "ach")) game.unlockAchievement(ach::kTrojanUnleashed);
    // "acheggline" earns the unlock that puts a new KIND of egg in the hatch menu,
    // which is the one announcement that HOLDS — its own plate, its own copy, and no
    // deadline (Game::achBannerHeld).
    if (hasFlag(argc, argv, "acheggline")) game.unlockAchievement(ach::kHashCollision);
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
        // The two DeepWeb depth buffs as well, which takes BUFFS to all five rows it
        // can hold at once — the state the page has to scroll rather than draw.
        game.inventory().add("deep_learning_core", 2);
        game.debugUseItem("deep_learning_core");
        game.inventory().add("zeroday_bell", 2);
        game.debugUseItem("zeroday_bell");
    }
    // "fullkit" equips every unlocked move slot and every mod slot — pair it with a
    // Daemon (pet:wire_heir) for the deepest LOADOUT page the game can produce.
    if (hasFlag(argc, argv, "fullkit")) game.debugFillLoadout();
    // "pantry" puts one of EVERY item in the bag: the whole ICON_ITEM_* set in ITEMS
    // list context, and the deepest the combat Exploit picker's ITEMS band can get.
    if (hasFlag(argc, argv, "pantry"))
        for (const ItemDef* d : game.content().allItems())
            game.inventory().add(d->id, 1);

    // A COMPLETE press — down and up. Four screens read B as a tap/hold pair and every
    // LIST reads C as one (Game::listBackStep), so on those a press edge alone only
    // ARMS the gesture and settles nothing: a scene that drills in with a bare press
    // silently renders the screen ABOVE the one it named. Everywhere else the release
    // is inert, so scenes can reach for this by default and only send a bare press when
    // they mean to drive a hold.
    auto tap = [&](Button b) {
        game.onButton({b, true, false});
        game.onButton({b, false, false});
    };
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
        // The modal opens at its own beat 0, after the heartbeat above has already run,
        // so `beats` is re-spent inside it to land on a frame of the bite. Stepped on
        // the DISSOLVE's clock (kFxAnimMs), which is what `beats` indexes for any scene
        // showing an FX_ABSORB — a heartbeat step would sample every fourth frame.
        uint32_t ft = static_cast<uint32_t>(beats) * kHeartbeatMs;
        for (int i = 1; i <= beats; ++i) {
            ft += kFxAnimMs;
            game.tick(ft);
            if (game.nav() != Game::Nav::ModalFeeding) break;   // it dismissed itself
        }
    } else if (hasFlag(argc, argv, "rollback")) {
        // Rollback stat picker: the leveled pet (above) has points to shed;
        // open the picker directly through the real Use path.
        game.inventory().add("rollback", 1);
        game.debugUseItem("rollback");
    } else if (hasFlag(argc, argv, "stat")) {
        // STAT is 8 paged screens: 0 vitals (landing) · 1 tiers · 2 loadout · 3 movedex ·
        // 4 foods · 5 buffs · 6 species · 7 audit log. "stat" alone shows vitals; the
        // page names step to theirs; "index" then holds B to open the jump list over
        // them. "ate:<n>" feeds the pet n dishes first, which is what puts lit cells in
        // the FOODS grid.
        game.debugSetBits(1450);
        game.debugSetNetworksSeen(27);       // R2, partway to R3
        enterSlot(SubmenuId::Stat);
        int steps = hasFlag(argc, argv, "tiers")   ? 1
                  : hasFlag(argc, argv, "loadout") ? 2
                  : hasFlag(argc, argv, "movedex") ? 3
                  : hasFlag(argc, argv, "foods")    ? 4
                  : hasFlag(argc, argv, "buffs")    ? 5
                  : hasFlag(argc, argv, "species")  ? 6
                  : hasFlag(argc, argv, "log")      ? 7
                                                     : 0;
        // "ate:<n>" puts n dishes on the pet's palate before the page draws, which is
        // the only way to see the FOODS grid half filled rather than wholly dim.
        for (int i = 3; i < argc; ++i)
            if (std::strncmp(argv[i], "ate:", 4) == 0)
                game.debugTasteFoods(std::atoi(argv[i] + 4));
        while (steps-- > 0) game.onButton({Button::A, true, false});
        // "scroll:<n>" then takes B n times, which on the three flowed pages
        // (TIERS/LOADOUT/BUFFS) advances the row window — the way to see the rows past
        // the first screenful, and that the last window lands clear of the hint band.
        // A COMPLETE press: B is a tap/hold pair on these pages and the advance settles
        // on the release.
        for (int i = 3; i < argc; ++i)
            if (std::strncmp(argv[i], "scroll:", 7) == 0)
                for (int n = std::atoi(argv[i] + 7); n > 0; --n) {
                    game.onButton({Button::B, true, false});
                    game.onButton({Button::B, false, false});
                }
        // ...and "index" holds B past the dwell instead, which opens the INDEX over
        // whichever page the flags above landed on.
        if (hasFlag(argc, argv, "index")) {
            game.onButton({Button::B, true, false});
            uint32_t t = static_cast<uint32_t>(beats) * kHeartbeatMs;
            game.tick(t + kStatIndexHoldMs + kHeartbeatMs);
        }
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
        // "picker" buys the ITEMS Type-Picker first, so ITEMS opens on the category
        // tiles; without it ITEMS opens straight on the list, as an unowned rig does.
        if (hasFlag(argc, argv, "picker")) {
            game.debugSetBits(kShopItemPickerCost);
            game.debugBuyItemPicker();
        }
        enterSlot(SubmenuId::Items);
        // With the picker up, "ingredients"/"keys"/"tools" walk to that tile and
        // drill into it, so the filtered list behind a tile is renderable too.
        const int tile = hasFlag(argc, argv, "ingredients") ? 2
                       : hasFlag(argc, argv, "keys") ? 4
                       : hasFlag(argc, argv, "tools") ? 5 : -1;
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
        else if (hasFlag(argc, argv, "background")) {
            // The picker is mostly LOCKED rows on a fresh save, which is the state it
            // ships in and the one worth looking at. "earned" plays the four grants
            // instead — a species raised, an area cleared, a bracket taken, a ladder
            // climbed — so the other half of the list can be seen without walking one.
            if (hasFlag(argc, argv, "earned")) {
                game.markCreatureRaised("cuttlefork");   // a swimmer's place
                game.markCreatureRaised("tadpoll");      // the Phishing line's
                game.debugClearSector(0);                // an area pays out itself
                game.debugAddTourneyWin();               // ...and the arena its first
                game.unlockAchievement("RECIPES_10");    // ...and one from each of the
                game.unlockAchievement("RIG_ALL");       //    four achievement families
                game.unlockAchievement("NETS_100");      //    that pay out a place
                game.unlockAchievement("STEPS_100K");
            }
            openTarget(CfgScreen::Background);
            // "row:<n>" walks the focus down, which is how to see that the line under
            // the header is the FOCUSED row's and not a fixed caption.
            for (int i = 3; i < argc; ++i)
                if (std::strncmp(argv[i], "row:", 4) == 0)
                    for (int k = std::atoi(argv[i] + 4); k > 0; --k)
                        game.onButton({Button::A, true, false});
            // "use" applies the focused row and walks back out to the habitat, which is
            // the only place the choice can actually be looked at.
            if (hasFlag(argc, argv, "use")) {
                game.onButton({Button::B, true, false});
                game.onButton({Button::B, false, false});
                for (int k = 0; k < 3; ++k) tapC(game);
            }
        }
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
        // ARCH opens on its GROUP PICKER now — NEW EGG, ACTIVE, one row per creature
        // family, RECORDS. "group:<n>" opens the nth of those, so a family shelf can be
        // looked at; without it the frame is the picker itself.
        enterSlot(SubmenuId::Arch);
        for (int i = 3; i < argc; ++i)
            if (std::strncmp(argv[i], "group:", 6) == 0) {
                for (int k = std::atoi(argv[i] + 6); k > 0; --k)
                    game.onButton({Button::A, true, false});
                game.onButton({Button::B, true, false});
            }
        // "row:<n>" walks the cursor down the open group, which is how a windowed list
        // is looked at: the rows on screen only change once the cursor leaves the window.
        for (int i = 3; i < argc; ++i)
            if (std::strncmp(argv[i], "row:", 4) == 0)
                for (int k = std::atoi(argv[i] + 4); k > 0; --k)
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
        // "solved" banks the eight quote wins that unlock the DECRYPTOGRAM cabinet, so
        // the list can be looked at both with the row absent and with it present.
        if (hasFlag(argc, argv, "solved"))
            for (int i = 0; i < kQuoteArcadeUnlockWins; ++i)
                game.debugSetQuoteTier(i, CryptogramTier::Solved);
        enterSlot(SubmenuId::Games);
        // cabinet → open the focused cabinet's page (L3); + hard → cycle the dial off
        // MEDIUM so the setting is visible; clutch/worm/... → focus that cabinet first.
        // Walked by ID rather than by a press count, because the A-cycle SKIPS a locked
        // row: counting presses lands on a different cabinet depending on whether the
        // quote board has been unlocked, which is exactly the frame a dump is trying
        // to pin down.
        const char* want = hasFlag(argc, argv, "clutch")       ? "clutch"
                           : hasFlag(argc, argv, "worm")       ? "isolation"
                           : hasFlag(argc, argv, "decryption") ? "decryption"
                           : hasFlag(argc, argv, "quote")      ? "cryptogram"
                           : hasFlag(argc, argv, "chroma")     ? "chroma"
                                                               : nullptr;
        for (int i = 0; want && i < arcadeGameCount(); ++i) {
            if (std::strcmp(arcadeGames()[game.arcadeRow()].id, want) == 0) break;
            game.onButton({Button::A, true, false});
        }
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
            game.onButton({Button::B, true, false});   // slot list -> the picker
            if (hasFlag(argc, argv, "focus") || hasFlag(argc, argv, "movedetail"))
                game.onButton({Button::A, true, false});
            // The picker's B is a tap/hold pair (hold reveals the full roster), so the
            // drill-in needs the release edge to read as a tap.
            if (hasFlag(argc, argv, "movedetail")) tap(Button::B);
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
        if (hasFlag(argc, argv, "override")) {       // open the A+C override picker
            game.onButton({Button::A, true, true});
            // The picker is two levels: it opens on the bands this fight has, and
            // "band:<n>" walks A to the n-th of them and B into its rows.
            for (int i = 3; i < argc; ++i)
                if (std::strncmp(argv[i], "band:", 5) == 0) {
                    for (int k = std::atoi(argv[i] + 5); k > 0; --k)
                        game.onButton({Button::A, true, false});
                    game.onButton({Button::B, true, false});
                }
        }
        // B CYCLES the mid-combat panel (closed -> VS -> KIT -> closed), so "stats"
        // lands on page 1 and "kit" presses through to page 2.
        if (hasFlag(argc, argv, "stats") || hasFlag(argc, argv, "kit"))
            game.onButton({Button::B, true, false});
        if (hasFlag(argc, argv, "kit")) game.onButton({Button::B, true, false});
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
    } else if (hasFlag(argc, argv, "dock")) {
        // ROCK THE DOCK — the operator bracket (game_tourney.cpp). Clearing area 0 is what
        // reaches The Pirate Bayou and so opens the arena's EXPL row, which is the LAST
        // row of the top level; A walks the cursor to it and B draws a bracket. "fight"
        // runs the operator's first match to a verdict, so the frame lands on the
        // bracket with one round already settled; "out"/"champion" force a terminal
        // banner without playing a whole tournament for it.
        game.debugSetSectorCleared(0, true);
        game.debugAddCombatXp(600000);               // a pet that can win a match
        enterSlot(SubmenuId::Expl);
        for (int i = 0; i < explRowCount() && game.listRow() != explRowCount() - 1; ++i)
            game.onButton({Button::A, true, false});
        game.onButton({Button::B, true, false});
        // B on the bracket is a TAP/HOLD pair (tap = start the bout, hold = the scout
        // sheet), so every press from here has to send both edges.
        auto tapB = [&] {
            game.onButton({Button::B, true, false});
            game.onButton({Button::B, false, false});
        };
        if (hasFlag(argc, argv, "deep")) {
            // Play the bracket forward as far as the pet can take it, so the frame
            // lands on a field that has already collapsed — which is the only way to
            // LOOK at the later rounds' tree and at the room they open up.
            for (int r = 0; r < kTourneyRounds &&
                            game.tourneyPhase() == Game::TourneyPhase::Ready; ++r) {
                tapB();
                for (int i = 1; i <= 4000 &&
                                game.combat().outcome() == Combat::Outcome::Ongoing; ++i)
                    game.tick(static_cast<uint32_t>(beats + i) * kHeartbeatMs);
                if (game.nav() != Game::Nav::Tourney) tapB();   // dismiss the verdict
            }
        } else if (hasFlag(argc, argv, "fight")) {
            tapB();
            for (int i = 1; i <= 4000 &&
                            game.combat().outcome() == Combat::Outcome::Ongoing; ++i)
                game.tick(static_cast<uint32_t>(beats + i) * kHeartbeatMs);
            tapB();                                    // dismiss the verdict
        } else if (hasFlag(argc, argv, "scout")) {
            // Park on a rival rather than on the operator's own row, so the sheet shows
            // somebody else's kit — which is what the gesture is for.
            while (game.tourneyCursor() == game.tourneySlot())
                game.onButton({Button::A, true, false});
            game.onButton({Button::B, true, false});   // arm the hold...
            game.tick(static_cast<uint32_t>(beats + 1) * kHeartbeatMs + kTourneyScoutHoldMs);
            game.onButton({Button::B, false, false});
        } else if (hasFlag(argc, argv, "brief")) {
            game.onButton({Button::A, true, true});    // the A+C chord
        }
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
        if (hasFlag(argc, argv, "decorated")) {
            // The widest identity the operator screens can hold, all at once: the
            // longest crew name on the roster, a zone Title equipped, and the rank the
            // longest title on the ladder unlocks at. The default seed is a rank-2
            // operator with no crew and no Title, which is exactly the state in which
            // none of these lines have to share a row with anything.
            int widest = 0;
            for (int i = 1; i < kCrewCount; ++i)
                if (textWidth(kCrews[i].displayName) >
                    textWidth(kCrews[widest].displayName)) widest = i;
            // Enlisting is gated on a home network, and the HOME NET row is one of the
            // lines being looked at — so seed a long SSID rather than the shortest one
            // that would let joinCrew through.
            game.setHomeNetwork(0x001122334455ull, "THE_PROMISED_LAN");
            game.joinCrew(widest);
            game.debugUnlockTitle(0);
            int longest = 0;
            for (int i = 1; i < hackerRankTierCount(); ++i)
                if (textWidth(hackerRankTitle(hackerRankTierUnlock(i))) >
                    textWidth(hackerRankTitle(hackerRankTierUnlock(longest)))) longest = i;
            game.debugSetNetworksSeen(hackerRankTierUnlock(longest) *
                                      (kHackerRankXpPerRank / kHackerRankXpPerNetwork));
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
            // "hub" buys the MERGE HUB first — the way to see the list with and
            // without it. "row:<n>" then A-cycles down to reach a given row.
            if (hasFlag(argc, argv, "hub")) { game.debugSetBits(99999); game.debugBuyMergeHub(); }
            // "services" buys the two rows that RUN on their own, which is what puts
            // the SERVICES head slot on the list; A-cycling onto it and holding B opens
            // one service's info page ("info"), the third of the SHOP's screens.
            const bool services = hasFlag(argc, argv, "services");
            if (services) {
                game.debugSetBits(999999);
                game.debugBuyAutoBackup();
                game.debugBuyRigRow(kRigRowDiskMaintenance);
            }
            enterHackerSlot(HackerSlotId::Shop);
            if (services) {
                for (int k = 0; k <= kRigUpgradeCount &&
                                game.shopSlot() != kRigSlotServices; ++k)
                    game.onButton({Button::A, true, false});
                if (hasFlag(argc, argv, "board") || hasFlag(argc, argv, "info")) {
                    game.onButton({Button::B, true, false});
                    game.onButton({Button::B, false, false});
                }
                if (hasFlag(argc, argv, "info")) {
                    // The DISK MAINTENANCE page is the one with something to read: a
                    // billed service, and the only readout with a live price on it. A
                    // is LIFTED, not left down — a held A repeats its step (the list
                    // contract), and the tick that fires the hold below would walk the
                    // cursor off the row this is trying to reach.
                    game.onButton({Button::A, true, false});
                    game.onButton({Button::A, false, false});
                    uint32_t t = 0;
                    game.tick(t);
                    game.onButton({Button::B, true, false});
                    game.tick(t += kServiceInfoHoldMs + kHeartbeatMs);
                    game.onButton({Button::B, false, false});
                }
            }
            for (int i = 3; i < argc; ++i)
                if (std::strncmp(argv[i], "row:", 4) == 0)
                    for (int k = std::atoi(argv[i] + 4); k > 0; --k)
                        game.onButton({Button::A, true, false});
            if (hasFlag(argc, argv, "buy")) game.onButton({Button::B, true, false});
        } else if (hasFlag(argc, argv, "merge")) {
            // MERGE HUB: the slot is a Rig Shop purchase, so buy it before entering
            // or the carousel gates the slot. "recipes" then wins every recipe the way
            // a solved Decryptogram does (no Bits path reaches one), so the list draws
            // unlocked rather than a column of LOCKED; "stock" fills the bag so the
            // focused row reads craftable rather than short.
            game.debugSetBits(99999);
            game.debugBuyMergeHub();
            if (hasFlag(argc, argv, "recipes"))
                for (int i = 0; i < kMergeRecipeCount; ++i) game.debugWinRecipe(i);
            if (hasFlag(argc, argv, "stock")) {
                game.inventory().add("null_noodles", 4);
                game.inventory().add("pwnzu_sauce", 4);
            }
            enterHackerSlot(HackerSlotId::Merge);
        } else if (hasFlag(argc, argv, "vault")) {
            // VAULT: stock one of every container tier, deliberately added in the
            // WRONG order — the list's own sort is what should land Epic on top.
            game.inventory().add("sealed_cache_common", 3);
            game.inventory().add("decryptogram", 2);   // the ticket row, above the caches
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
              hasFlag(argc, argv, "modshop") ||
              hasFlag(argc, argv, "warp") || hasFlag(argc, argv, "postencounter") ||
              hasFlag(argc, argv, "shibboleth") || hasFlag(argc, argv, "outro")) {
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
        // "deep" opens the later sectors and arms one of them: sector 0's wild roster
        // fields nothing but the innate Quick Jab, so anything that depends on the
        // rival's KIT (the combat outro's two dissolves) has nothing to show there.
        const bool deepSector = hasFlag(argc, argv, "deep");
        if (deepSector)
            for (int a = 0; a < 2; ++a) {
                game.debugSetSectorCleared(a, true);
                for (int s = 0; s < kExplSubAreas; ++s) game.debugSetSubCleared(a, s, true);
            }
        enterSlot(SubmenuId::Expl);
        // The ladder is nested: the first B expands the focused sector, the second arms
        // the sub-area the cursor lands on — and arming is what drops the game back to
        // the IDLE habitat with the explore badge live.
        if (deepSector)
            for (int i = 0; i < 2; ++i)
                game.onButton({Button::A, true, false});   // walk to sector[2]
        game.onButton({Button::B, true, false});     // expand the focused sector
        game.onButton({Button::B, true, false});     // arm sub-area[0] -> idle explore-mode
        if (hasFlag(argc, argv, "cachefind")) {
            // AFTER arming, which clears the line the way a fresh walk does.
            // The longest line the walk composes: a Sealed Cache find naming the widest
            // findable cache and the screen it opens on. Which event a step rolls is not
            // something a scene can ask for, so this states the line rather than walking
            // until one turns up.
            const ItemDef* widest = nullptr;
            for (const ItemDef* it : game.content().allItems()) {
                if (it->use != ItemDef::Use::OpenContainer || it->cache.findWeight <= 0)
                    continue;
                if (!widest || textWidth(it->displayName) > textWidth(widest->displayName))
                    widest = it;
            }
            if (widest) {
                char flavor[48];
                std::snprintf(flavor, sizeof(flavor), "%s - DECRYPT IN VAULT",
                              widest->displayName);
                game.debugSetExploreFlavor(flavor);
            }
        }
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
            // Every screen an explore step can land on has to be answered, because the
            // fallback is `ping`, and the A+C chord on anything but the armed habitat
            // opens the Hacker face instead of stepping — one unhandled screen and the
            // walk stops without ever reaching a fight.
            // Walk until a fight is in front of us. BOTH navs end the walk, because
            // `ping`'s second press doubles as the Encounter's own Fight key: a step
            // that rolls an encounter often arrives already IN the fight, so a loop
            // watching only for Encounter walks straight past the thing it wanted.
            //
            // Every other screen a step can land on has to be answered too, since the
            // fallback is `ping` and the A+C chord on anything but the armed habitat
            // opens the Hacker face instead of stepping.
            const bool wild = hasFlag(argc, argv, "wildcombat");
            auto atFight = [&] {
                return game.nav() == Game::Nav::Encounter ||
                       (wild && game.nav() == Game::Nav::Combat);
            };
            for (int i = 0; i < 600 && !atFight(); ++i) {
                if (game.nav() == Game::Nav::Wifi ||
                    game.nav() == Game::Nav::PostEncounter)
                    game.onButton({Button::B, true, false});
                else if (game.nav() == Game::Nav::Shop ||
                         game.nav() == Game::Nav::ModShop)
                    game.onButton({Button::C, true, false});
                else if (game.nav() == Game::Nav::Combat) {
                    for (int j = 0; j < 800 &&
                            game.combat().outcome() == Combat::Outcome::Ongoing; ++j)
                        game.tick(t += kHeartbeatMs);
                    game.onButton({Button::B, true, false});
                } else ping();
            }
            if (wild) {
                if (game.nav() == Game::Nav::Encounter)
                    game.onButton({Button::B, true, false});  // Fight -> live combat
                for (int i = 1; i <= 4; ++i)                  // a few action beats
                    game.tick(t += kHeartbeatMs);
                // B cycles the mid-fight panel, same as the `combat` dev hook — but on a
                // REAL wild, which is the only fight whose KIT page can mark a prize.
                if (hasFlag(argc, argv, "stats") || hasFlag(argc, argv, "kit"))
                    game.onButton({Button::B, true, false});
                if (hasFlag(argc, argv, "kit")) game.onButton({Button::B, true, false});
            }
        } else if (hasFlag(argc, argv, "wifi")) {
            game.inventory().add("sinkhole_trap", 20);   // bypass wild encounters, free
            // Which discovery the event resolves — the thing the pet does to the
            // network glyph (FX_ABSORB). Bare "wifi" queues nothing, which is the
            // empty-queue beat; the three named ones seed the ledger so the sighting
            // below lands as new / fond / home-turf.
            const uint64_t seen = 0x02'00'00'00'00'01ull;
            if (hasFlag(argc, argv, "fond") || hasFlag(argc, argv, "hometurf")) {
                const bool home = hasFlag(argc, argv, "hometurf");
                // inTopN is a RANK, so "fond" needs a crowd above it to be outside the
                // favourites and "hometurf" needs to sit at the head of one.
                game.debugSeedNetworkLedger(seen, "THE_PROMISED_LAN", home ? 99 : 1);
                for (int i = 0; i < kNetDiscoveryTopFavoritesCount + 2; ++i) {
                    char nm[24];
                    std::snprintf(nm, sizeof(nm), "NEIGHBOUR_%d", i);
                    game.debugSeedNetworkLedger(seen + 0x100ull * (i + 1), nm, 20 + i);
                }
            }
            if (hasFlag(argc, argv, "new") || hasFlag(argc, argv, "fond") ||
                hasFlag(argc, argv, "hometurf")) {
                const uint8_t bssid[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
                game.registerNetwork(bssid, "THE_PROMISED_LAN");
            }
            for (int i = 0; i < 400 && game.nav() != Game::Nav::Wifi; ++i) {
                if (game.nav() == Game::Nav::Encounter) {
                    game.onButton({Button::A, true, false});  // Fight -> Flee
                    game.onButton({Button::A, true, false});  // Flee -> Sinkhole
                    game.onButton({Button::B, true, false});  // confirm -> back to idle
                } else if (game.nav() == Game::Nav::Shop ||
                           game.nav() == Game::Nav::ModShop) {
                    game.onButton({Button::C, true, false});
                } else ping();
            }
            // The event opens at its own beat 0, so `beats` is re-spent inside it to
            // land on a frame of the absorb — on the dissolve's clock, and stopping if
            // the hands-off hold resolves the event off the screen underneath us.
            for (int i = 1; i <= beats && game.nav() == Game::Nav::Wifi; ++i)
                game.tick(t += kFxAnimMs);
        } else if (hasFlag(argc, argv, "shibboleth")) {
            // The guardian's riddle, drawn in the CANT. `sigils:<n>` is the whole point
            // of the scene: at 0 the panel is a wall of nonsense, and every sigil turns
            // one more letter of it into itself — the same riddle, legible in stages.
            // The reply the cursor is on is never the interesting part, so nothing here
            // steers it.
            game.inventory().add("sinkhole_trap", 20);
            for (int i = 3; i < argc; ++i)
                if (std::strncmp(argv[i], "sigils:", 7) == 0)
                    game.debugLearnSigils(std::atoi(argv[i] + 7));
            // The HAIL is where every band opens, so a scene that only wants that one
            // stops as soon as a guardian is summoned.
            const bool wantHail = hasFlag(argc, argv, "hail");
            const bool wantVerdict = hasFlag(argc, argv, "verdict");
            const bool wantRefused = hasFlag(argc, argv, "refused");
            const bool wantBoon = hasFlag(argc, argv, "boon");
            // The welcome is ROLLED (game_shibboleth.cpp), so a band is reached by asking
            // until it comes up rather than by forcing it.
            for (int i = 0; i < 200 && game.nav() != Game::Nav::Shibboleth; ++i) {
                game.debugStartShibboleth();
                if (wantHail) break;                      // the hail IS the scene
                if (wantRefused || wantBoon) {
                    // One of the two bands that never asks anything: the refusal's
                    // verdict, which has to make a boss out of nowhere legible, and the
                    // boon's, which is what fluency is FOR. Both are searched for rather
                    // than forced, since the welcome is a roll — pair `boon` with a high
                    // `sigils:` or the search will not find one.
                    const auto want = wantRefused ? Game::ShibbolethWelcome::Affront
                                                  : Game::ShibbolethWelcome::Boon;
                    if (game.shibbolethWelcome() != want) continue;
                    game.onButton({Button::B, true, false});
                    break;
                }
                game.onButton({Button::B, true, false});   // hail -> the band's screen
                if (game.nav() == Game::Nav::ShibbolethVerdict)
                    game.onButton({Button::B, true, false});
                if (game.nav() == Game::Nav::Combat) {
                    for (int j = 0; j < 400 &&
                            game.combat().outcome() == Combat::Outcome::Ongoing; ++j)
                        game.tick(t += kHeartbeatMs);
                    game.onButton({Button::B, true, false});
                }
                if (game.nav() == Game::Nav::PostEncounter)
                    game.onButton({Button::B, true, false});
                if (!game.exploreActive()) game.debugArmExplore(0, 0);
            }
            // The VERDICT is what the focused reply earns. "wrong" steps the cursor OFF
            // the true row first, which is the half of the pair worth looking at hardest:
            // a displeased guardian is drawn in WARN and leads to a fight, so it is the
            // screen the dual-coding gate has to hold.
            if (wantVerdict && game.nav() == Game::Nav::Shibboleth) {
                if (hasFlag(argc, argv, "wrong"))
                    for (int i = 0; i < kRiddleReplies &&
                            game.shibbolethRow() == game.shibbolethTrueRow(); ++i)
                        game.onButton({Button::A, true, false});
                game.onButton({Button::B, true, false});
            }
            // "fight" carries on into the guardian's own COMBAT, the other screen its
            // swarm is drawn on and the one where it used to appear as a borrowed pet
            // sheet. The stage runs on kCombatAnimMs, which is also the clock the swarm
            // steps on there — so the same `beats` count means the same thing.
            if (hasFlag(argc, argv, "fight")) {
                for (int i = 0; i < 8 && game.nav() != Game::Nav::Combat; ++i)
                    game.onButton({Button::B, true, false});
                for (int i = 1; i <= beats && game.nav() == Game::Nav::Combat; ++i)
                    game.tick(t += kCombatAnimMs);
            }
            // `beats` steps the guardian's SWARM, which is what actually moves on the
            // hail and the verdict — so this ticks the FX clock (kFxAnimMs) rather than
            // the heartbeat, exactly as the Wi-Fi dissolve's scene does. Four of these
            // per heartbeat also means a scene can run the flock well into its shape
            // before the screen's own hands-off hold resolves out from under it.
            for (int i = 1; i <= beats && (game.nav() == Game::Nav::Shibboleth ||
                                           game.nav() == Game::Nav::ShibbolethHail ||
                                           game.nav() == Game::Nav::ShibbolethVerdict); ++i)
                game.tick(t += kFxAnimMs);
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
        } else if (hasFlag(argc, argv, "shop") || hasFlag(argc, argv, "modshop")) {
            // The two storefronts share one screen, so the walk stops at whichever
            // this scene asked for and LEAVES the other kind (C) to keep stepping.
            // "full" fills the mod pool to modStorageCap() before the visit, which is
            // the state the HAVE n/cap column and the STORAGE FULL buy reason exist for.
            const bool wantMods = hasFlag(argc, argv, "modshop");
            const Game::Nav stopAt = wantMods ? Game::Nav::ModShop : Game::Nav::Shop;
            game.inventory().add("sinkhole_trap", 20);
            for (int i = 0; i < 400 && game.nav() != stopAt; ++i) {
                if (game.nav() == Game::Nav::Encounter) {
                    game.onButton({Button::A, true, false});
                    game.onButton({Button::A, true, false});
                    game.onButton({Button::B, true, false});
                } else if (game.nav() == Game::Nav::Wifi) {
                    game.onButton({Button::B, true, false});
                } else if (game.nav() == Game::Nav::Shop ||
                           game.nav() == Game::Nav::ModShop) {
                    game.onButton({Button::C, true, false});   // the other storefront
                } else if (game.nav() == Game::Nav::Combat) {
                    for (int j = 0; j < 800 &&
                            game.combat().outcome() == Combat::Outcome::Ongoing; ++j)
                        game.tick(t += kHeartbeatMs);
                    game.onButton({Button::B, true, false});
                } else ping();
            }
            if (wantMods && hasFlag(argc, argv, "full"))
                for (int i = 0; i < game.shopListingCount(); ++i)
                    for (int k = 0; k < game.modStorageCap(); ++k)
                        game.debugGrantMod(game.shopListingId(i));
        } else if (hasFlag(argc, argv, "outro")) {
            // Ride a wild fight to a WIN and hold ON the combat screen for `beats`, so
            // the frame lands inside the beaten rival's dissolve rather than past it.
            // Which dissolve plays is the pet's own kit against the rival's
            // (Game::rivalFieldsUnknownMove) — "known" grants the rival's moves first,
            // which is what turns the absorb back into a shred.
            const bool known = hasFlag(argc, argv, "known");
            for (int i = 0; i < 600; ++i) {
                if (game.nav() == Game::Nav::Wifi || game.nav() == Game::Nav::Encounter)
                    game.onButton({Button::B, true, false});
                else if (game.nav() == Game::Nav::Shop ||
                         game.nav() == Game::Nav::ModShop)
                    game.onButton({Button::C, true, false});
                else if (game.nav() == Game::Nav::PostEncounter)
                    game.onButton({Button::B, true, false});
                else if (game.nav() == Game::Nav::Combat) {
                    if (known)
                        for (const MoveDef* m : game.combat().enemy().moves)
                            if (m && m->id) game.debugGrantMove(m->id);
                    for (int j = 0; j < 800 &&
                            game.combat().outcome() == Combat::Outcome::Ongoing; ++j)
                        game.tick(t += kHeartbeatMs);
                    if (game.combat().outcome() == Combat::Outcome::Win) break;
                    // A loss has no outro — keep walking until one is won.
                    for (int k = 0; k < kExploreRevealHoldBeats + 2 &&
                            game.nav() == Game::Nav::Combat; ++k)
                        game.tick(t += kHeartbeatMs);
                } else ping();
            }
            // The dissolve's own clock, held while the result beat is still up (
            // finishCombat auto-dismisses it on the slower heartbeat).
            for (int k = 0; k < beats && game.nav() == Game::Nav::Combat; ++k)
                game.tick(t += kFxAnimMs);
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
    } else if (hasFlag(argc, argv, "cryptogram")) {
        // Cash a Decryptogram at the VAULT, which is the real door in. "open:<n>"
        // places n letters correctly (reading each off the cell the cursor is already
        // on, the way a player who has deduced it would), "win" plays the whole quote
        // out so the attribution and the prize line are on screen, and "lose" misplaces
        // one letter to hold the verdict with its highlighted cell.
        game.inventory().add("decryptogram", 1);
        game.onButton({Button::A, true, true});      // A+C chord -> the Hacker face
        game.onButton({Button::A, true, false});     // summon the hacker cursor
        while (hackerCarouselSlots()[game.cursor()].id != HackerSlotId::Vault)
            game.onButton({Button::A, true, false});
        game.onButton({Button::B, true, false});     // -> the VAULT
        game.onButton({Button::B, true, false});     // cash it -> the board
        int opens = hasFlag(argc, argv, "win") ? 64 : 0;
        for (int i = 3; i < argc; ++i)
            if (std::strncmp(argv[i], "open:", 5) == 0) opens = std::atoi(argv[i] + 5);
        for (int n = 0; n < opens && game.cryptogram().running(); ++n) {
            const Cryptogram& c = game.cryptogram();
            const char want = c.at(c.cellCursor());
            for (int i = 0; i < c.poolSize() && c.poolLetter(c.poolCursor()) != want; ++i)
                game.onButton({Button::A, true, false});
            game.onButton({Button::B, true, false});   // take it
            game.onButton({Button::B, true, false});   // place it
        }
        // "take" leaves a letter in hand, which is the OTHER control state — the
        // cell cursor and its own hint band.
        if (hasFlag(argc, argv, "take") && game.cryptogram().running())
            game.onButton({Button::B, true, false});
        if (hasFlag(argc, argv, "lose") && game.cryptogram().running()) {
            const Cryptogram& c = game.cryptogram();
            const char want = c.at(c.cellCursor());
            for (int i = 0; i < c.poolSize() && c.poolLetter(c.poolCursor()) == want; ++i)
                game.onButton({Button::A, true, false});
            game.onButton({Button::B, true, false});
            game.onButton({Button::B, true, false});
        }
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
        } else if (hasFlag(argc, argv, "lose")) {
            // Aim at whichever half does NOT hold the live egg — the mirror of "win" —
            // so the run resolves on round one every time, deterministic regardless of
            // seed. The clutch now ends the instant the target leaves the span (see
            // game_eggpick.cpp), so unlike "win" this never needs more than one commit.
            const int col = game.eggPickTargetSlot() % Game::kEggPickCols;
            const int row = game.eggPickTargetSlot() / Game::kEggPickCols;
            const bool second = col < Game::kEggPickCols / 2;   // wrong half of round 1's column cut
            (void)row;
            game.onButton({second ? Button::C : Button::A, true, false});
            game.onButton({Button::B, true, false});
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
                // Every corner on this path is a single quarter-turn (the path never
                // reverses on itself), so turn is always 1 or 3 here — 1 is a RIGHT
                // turn (C), which this used to send to B, a no-op while running (only
                // A/C steer — see onIsolation). That silently dropped every right turn,
                // so the worm took its one working left turn and then ran straight
                // until it stalled on a wall, instead of actually walking the cycle.
                if (turn == 3) game.onButton({Button::A, true, false});
                else if (turn == 1) game.onButton({Button::C, true, false});
                game.tick(it += kIsolationStepMs);
            }
        }
        // "bank" takes the verdict's B, which spends the run and leaves the Vermicell
        // egg incubating at idle — the state the whole minigame hands back to.
        if (hasFlag(argc, argv, "bank") && !game.isolation().running())
            game.onButton({Button::B, true, false});
    } else if (hasFlag(argc, argv, "chroma")) {
        // Lay a Metamorphic egg, which opens its CHROMATOPHORE on the spot.
        game.unlockAchievement(ach::kHashCollision);   // unlocks the Metamorphic line
        game.resetToHatch();
        for (int i = 0; i < 8; ++i) {
            const auto lines = game.availableEggLines();
            if (!lines.empty() &&
                std::strcmp(lines[game.lineSelectRow() % lines.size()]->id,
                            "metamorphic") == 0)
                break;
            game.onButton({Button::A, true, false});
        }
        game.onButton({Button::B, true, false});   // lay it -> Nav::Chroma
        uint32_t ct = 0;
        // "wear" wears the water and settles into it, which is the frame the board is
        // FOR: the creature standing in the same colours as what is under it.
        // "half" stops the repaint partway, holding the scatter FX_CAMO draws.
        // "spotted" wears the wrong skin and lets the sweep arrive, for the verdict.
        if (hasFlag(argc, argv, "spotted")) {
            game.onButton(chromaPress((game.chroma().plate() + 1) % kChromaSkins));
            while (game.chroma().running()) game.tick(ct += kFxAnimMs);
        } else if (hasFlag(argc, argv, "wear") || hasFlag(argc, argv, "half")) {
            const bool half = hasFlag(argc, argv, "half");
            game.onButton(chromaPress(game.chroma().plate()));
            while (game.chroma().running() &&
                   game.chroma().wearPct() < (half ? 50 : 100))
                game.tick(ct += kFxAnimMs);
        }
        // "clean" plays every round perfectly and holds the verdict; "bank" then takes
        // the B off it, leaving the Polystaria egg incubating at idle.
        if (hasFlag(argc, argv, "clean")) {
            while (game.chroma().running()) {
                game.onButton(chromaPress(game.chroma().plate()));
                game.tick(ct += kFxAnimMs);
            }
        }
        if (hasFlag(argc, argv, "bank") && !game.chroma().running())
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

    return writePanel(fb, out, beats);
}
