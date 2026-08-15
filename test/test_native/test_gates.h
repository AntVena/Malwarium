// test_gates.h — what every native gate unit shares: the includes, the CHECK
// macro in both its harness flavours, and the fixtures more than one unit drives.
//
// The gates themselves live in test_<subject>.cpp beside this file; test_main.cpp
// holds the roster and main(). A helper belongs HERE only once a second unit needs
// it — one that drives a single subject stays private to that unit, where a reader
// looking at the test can see what it does without leaving the file.
#pragma once
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "core/app/game.h"
#include "core/app/game_internal.h"   // kMergeRecipes — item earn-path coverage guard
#include "core/app/pedia_state.h"
#include "core/content/areas/deepweb_dive/area.h"
#include "core/content/content_passives.h"
#include "core/content/content_tables.h"
#include "core/content/effect_text.h"
#include "core/content/registry.h"
#include "core/model/event_log.h"
#include "core/model/hacker_rank.h"
#include "core/model/inventory.h"
#include "core/model/combat.h"
#include "core/model/loadout.h"
#include "core/model/move_loadout.h"
#include "core/model/pet_model.h"
#include "core/model/save.h"
#include "core/model/stacker.h"
#include "core/net/audit_capture.h"
#include "core/net/eapol.h"
#include "core/net/network_ledger.h"
#include "core/net/pcap.h"
#include "core/net/pcap_naming.h"
#include "core/net/peer_ledger.h"
#include "core/net/peer_link.h"
#include "core/net/pvp_link.h"
#include "core/net/tar_reader.h"
#include "core/net/update_manifest.h"
#include "core/net/version_marker.h"
#include "core/model/pvp_battle.h"
#include "core/render/canvas.h"
#include "core/render/color.h"
#include "core/render/font.h"      // textWidth — mirroring a screen's own layout maths
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/render/upscaler.h"
#include "core/ui/arch_screen.h"
#include "core/ui/carousel.h"
#include "core/ui/cfg_screen.h"
#include "core/ui/combat_screen.h"
#include "core/ui/expl_screen.h"
#include "core/ui/modals.h"
#include "core/ui/train_screen.h"
#include "core/app/game_rig_shop.h"  // rigSpec — the Rig Shop shares the readout rails
#include "core/app/game_internal.h"  // kRigRowPitch — the SHOP row geometry
#include "core/ui/widgets.h"      // gridLines — the readout grid's own packing rule
#include "core/ui/items_screen.h"
#include "core/ui/maint_screen.h"
#include "core/ui/mods_screen.h"
#include "core/ui/stat_screen.h"
#include "generated/assets.h"
#include "tunables.h"
#include "version.h"

// Mirrors items_screen.cpp's content-area top (kRowTop) for pixel assertions.
constexpr int kItemsRowTop = 26;

// Button press edge (the common case in these gates).
inline mal::ButtonEvent press(mal::Button b) { return {b, true, false}; }
inline mal::ButtonEvent lift(mal::Button b) { return {b, false, false}; }

// A COMPLETE C press — down and up. On a list C is a tap/hold pair (a tap cancels, a
// hold walks the cursor backward, Game::listBackStep), so the press edge alone is only
// half of one and settles nothing. Off a list the release is inert and this is just the
// press. Every gate that means "back out of here" wants this rather than press(C).
inline void tapC(mal::Game& g) {
    g.onButton(press(mal::Button::C));
    g.onButton(lift(mal::Button::C));
}

// A COMPLETE B press. Four screens read B as a tap/hold pair — the ITEMS list once
// Type-Tabs is owned, the MOVES picker, the Hacker VAULT once Bulk-Open is owned, and
// the CFG Factory Reset — and on those the press edge only ARMS. Everywhere else the
// release is inert. Use this wherever a gate means "accept this row"; use a bare
// press(B) followed by a tick only when the gate is deliberately driving a hold.
inline void tapB(mal::Game& g) {
    g.onButton(press(mal::Button::B));
    g.onButton(lift(mal::Button::B));
}

using namespace mal;

// In-memory ISaveStore for the persistence gates (the test tier of the seam).
class MemSaveStore : public ISaveStore {
public:
    std::vector<uint8_t> load() override { return blob_; }
    bool save(const std::vector<uint8_t>& d) override { blob_ = d; return true; }
    void clear() override { blob_.clear(); }
    const std::vector<uint8_t>& bytes() const { return blob_; }
private:
    std::vector<uint8_t> blob_;
};

// A store that can be told to refuse a write. The heap guard's refusal is already
// drivable (setHeapProbe); this is the other half — the save was built and handed over,
// and the MEDIUM said no, which on device is a short NVS write.
class RefusingSaveStore : public ISaveStore {
public:
    std::vector<uint8_t> load() override { return blob_; }
    bool save(const std::vector<uint8_t>& d) override {
        ++attempts;
        if (refuse) return false;          // whatever was on "flash" stays there
        blob_ = d;
        return true;
    }
    void clear() override { blob_.clear(); }
    const std::vector<uint8_t>& bytes() const { return blob_; }
    bool refuse = false;
    int attempts = 0;
private:
    std::vector<uint8_t> blob_;
};

// The Bits an achievement pays on unlock, read off its own row. Gates that check a
// wallet after an action that also EARNS something use this instead of restating the
// number, so retuning a reward doesn't need a test edit to go with it.
inline int achBitsReward(const char* id) {
    const AchievementDef* d = achievementById(id);
    if (!d) return 0;
    int n = 0;
    for (const AchievementReward& r : d->rewards)
        if (r.kind == AchievementReward::Kind::Bits) n += r.magnitude;
    return n;
}

// Dual-mode gate runner. The gate bodies in the units beside this header are the
// single source of truth; they run under two harnesses depending on how the file is
// compiled:
//   * ctest (CMake `mal_tests`)      — the counting CHECK + hand-rolled main().
//   * `pio test -e native` (Unity)   — CHECK maps to a Unity assertion and each
//                                       gate is a RUN_TEST (PlatformIO defines
//                                       PIO_UNIT_TESTING for test builds).
// The gate roster lives once in test_main.cpp's MAL_RUN_ALL_GATES so the two
// harnesses can never drift out of sync.
#ifdef PIO_UNIT_TESTING
#include <unity.h>
#define CHECK(cond) TEST_ASSERT_TRUE(cond)
#else
extern int g_failures;   // defined in test_main.cpp
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
            ++g_failures;                                                    \
        }                                                                    \
    } while (0)
#endif

// --- Shared fixtures -------------------------------------------------------
//
// Drivers more than one gate unit needs. Each walks the real input/tick path
// rather than reaching past it, so a gate that says "get to a wild encounter"
// exercises the same code the device does on the way there.

// A region has a grayscale-bright pixel (the white glyph reads against the dark
// #14171c paper without colour) — the non-colour channel these icons live on.
inline bool anyLitGray(const Framebuffer& fb, int x0, int y0, int x1, int y1) {
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            if (luminance(fb.get(x, y)) > 0.12f) return true;
    return false;
}

inline bool anyNonPaper(const Framebuffer& fb, int x0, int y0, int x1, int y1) {
    const Rgb565 paper = palColor(Pal::PAPER);
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            if (fb.get(x, y) != paper) return true;
    return false;
}

// Sample each gauge cell centre, desaturate, and confirm the lit-cell COUNT
// (the non-colour fill-level channel) matches the value at every zone colour.
inline int litCellsGray(const Framebuffer& fb, int gx, int gw, int rowY) {
    const int cellW = gw / 10;
    int lit = 0;
    for (int i = 0; i < 10; ++i) {
        int cx = gx + i * cellW + cellW / 2;
        int cy = rowY + 5;  // gauge is 10px tall
        if (luminance(fb.get(cx, cy)) > 0.12f) ++lit;  // grayscale threshold
    }
    return lit;
}

// Walk the HACKER carousel to `slot` and enter it — the A+C chord flips the face
// first, so a caller mid-submenu is backed out to idle before the chord is spent.
inline void enterHackerSlot(Game& g, HackerSlotId slot) {
    while (g.nav() != Game::Nav::Idle) tapC(g);
    g.onButton({Button::A, true, true});                  // A+C -> hacker face
    g.onButton(press(Button::A));
    while (hackerCarouselSlots()[g.cursor()].id != slot) g.onButton(press(Button::A));
    g.onButton(press(Button::B));
}

// Walk the carousel cursor from idle to the slot routing to `id`, then enter it.
inline void enterSlot(Game& g, SubmenuId id) {
    g.onButton(press(Button::A));                     // idle A -> cursor @ slot 0
    while (carouselSlots()[g.cursor()].id != id)
        g.onButton(press(Button::A));
    g.onButton(press(Button::B));                     // B -> enter the submenu
}

// Fresh boot now opens the line-select modal when >1 egg line is unlocked
// (Ransomware + Phishing). These gates predate line-select, so pick the first line
// (Ransomware) to lay its egg and match the historical straight-to-egg behaviour. A
// no-op when the modal isn't up (e.g. a save-store boot that loaded a live pet).
// Play out a DISK DECRYPTION board without cracking it, and dismiss the verdict — the
// egg keeps its full clock and the run costs nothing. Deterministic by construction:
// B alone never cycles a colour, so every row is locked in as GGG, and the hatch board
// bars duplicate colours in the key, so GGG is a code it cannot have drawn.
inline void settleDecryption(Game& g) {
    if (!g.inDecryption()) return;
    for (int i = 0; i < kDecryptionAttempts * kDecryptionSlots; ++i)
        g.onButton(press(Button::B));
    g.onButton(press(Button::B));   // the verdict -> idle
}

// Play a DISK DECRYPTION board out on the FIRST guess, by reading the key straight off
// the model — the one thing a player can't do, and the only way a test gets a
// deterministic crack out of a seeded code.
inline void crackDecryption(Game& g) {
    if (!g.inDecryption()) return;
    for (int s = 0; s < kDecryptionSlots; ++s) {
        const int want = g.decryption().codeAt(s);
        for (int i = 0; i < kDecryptionColours && g.decryption().guess(s) != want; ++i)
            g.onButton(press(Button::A));
        g.onButton(press(Button::B));
    }
    g.onButton(press(Button::B));   // the verdict -> idle
}

// Commit line-select onto the first unlocked line and settle whatever hatch minigame
// that line opens, leaving the egg incubating at idle. Every line plays one at
// lay-time now, so getting to "an egg is sitting there" always costs this.
inline void pickFirstEggLine(Game& g) {
    if (g.inLineSelect()) g.onButton(press(Button::B));
    settleDecryption(g);
}

// Helper: advance the cinematic past hold + flash into the reveal.
inline void advanceToReveal(Game& g, uint32_t& t) {
    for (int i = 0; i < kEvoHoldBeats + kEvoFlashBeats + 1; ++i)
        g.tick(t += kHeartbeatMs);
}

// A chord press edge (both A+C held) — the pet-side Exploit gesture.
inline mal::ButtonEvent chordAC() { return {Button::C, true, true}; }

// Walk the cursor from idle to a given submenu and enter it (mirrors the
// dump_frame nav helper) so a screen can be driven for a render assertion.
inline void enterSubmenuId(Game& g, SubmenuId id) {
    g.onButton(press(Button::A));                 // idle A -> carousel @ slot 1
    while (carouselSlots()[g.cursor()].id != id)
        g.onButton(press(Button::A));
    g.onButton(press(Button::B));                 // enter the submenu
}

// Walk from the carousel to a CFG L3 screen by TARGET, descending through a group
// screen (DEVICE / RADIO) when the target lives in one. Scanning the tables rather
// than counting presses keeps every CFG gate independent of the row order.
inline void enterCfgTarget(Game& g, CfgScreen target) {
    const CfgScreen group = cfgParentGroup(target);
    const CfgRow* rows = nullptr;
    int n = cfgRows(rows);
    int row = -1;
    for (int i = 0; i < n; ++i) if (rows[i].target == group) row = i;
    CHECK(row >= 0);
    enterSubmenuId(g, SubmenuId::Cfg);
    for (int i = 0; i < row; ++i) g.onButton(press(Button::A));
    g.onButton(press(Button::B));                 // open the row (a group, or the target)
    if (group == target) return;
    n = cfgGroupRows(group, rows);
    int sub = -1;
    for (int i = 0; i < n; ++i) if (rows[i].target == target) sub = i;
    CHECK(sub >= 0);
    for (int i = 0; i < sub; ++i) g.onButton(press(Button::A));
    g.onButton(press(Button::B));                 // open the setting inside the group
}

// True if any pixel in [x0,x1)×[y0,y1) differs between two framebuffers.
inline bool regionDiffers(const Framebuffer& a, const Framebuffer& b,
                          int x0, int y0, int x1, int y1) {
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            if (a.get(x, y) != b.get(x, y)) return true;
    return false;
}

inline bool fbEqual(const Framebuffer& a, const Framebuffer& b) {
    return !regionDiffers(a, b, 0, 0, kActiveW, kActiveH);
}

// True if any pixel in the region survives desaturation as ink (dark on paper)
// — the non-colour channel every screen must carry (release gate 1).
inline bool hasDarkInk(const Framebuffer& fb, int x0, int y0, int x1, int y1) {
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            if (luminance(fb.get(x, y)) < 0.4f) return true;
    return false;
}

// Forges a blob byte-shaped like a pre-v33 writer would have produced for a
// SaveData otherwise identical to `proto`. v33 REMOVES three wire fields
// (the v4 dedup-mask byte, the v7 seenBssids vector, the v21 full-mask u64)
// instead of only appending — the first removal in this format's history — so
// the usual "serialize CURRENT, drop trailing bytes, restamp the version word"
// trick every migration test below relies on is invalid for a restamped version
// in [4,32]: those bytes sit mid-stream, not at the tail, and a v33+ writer no
// longer emits them at all, so restamping alone would silently misalign every
// field after the removal point. This reconstructs them: builds a byte-
// identical "shape twin" of `proto` with distinctive sentinel values stamped
// onto the handful of FIXED-WIDTH scalar fields immediately preceding each
// splice point (overwriting a fixed-width scalar can't shift any other field's
// position, so the twin's byte offsets exactly match the real blob's), locates
// those sentinels, and inserts the removed bytes at the same offsets into the
// real, caller-supplied blob — independent of whatever actual values `proto`
// holds (so a test's own meaningful field values never collide with the search).
inline std::vector<uint8_t> forgeLegacyNetworkBytes(const SaveData& proto, int targetVersion) {
    SaveData shape = proto;
    shape.networksSeen = 0x1A2B3C4D;
    shape.handshakesSeen = 0x5E6F7081;
    shape.mistakeShieldActive = 0xAA;
    shape.shieldItemConsumed = 0xBB;
    shape.yubiConsumed = 0xCC;
    const auto shapeBlob = serializeSave(shape);
    auto blob = serializeSave(proto);

    auto le32 = [](int32_t v) {
        std::vector<uint8_t> b(4);
        for (int i = 0; i < 4; ++i) b[i] = static_cast<uint8_t>(static_cast<uint32_t>(v) >> (8 * i));
        return b;
    };
    auto findOnce = [](const std::vector<uint8_t>& hay, const std::vector<uint8_t>& pat) -> long {
        long found = -1;
        for (size_t i = 0; i + pat.size() <= hay.size(); ++i) {
            if (std::equal(pat.begin(), pat.end(), hay.begin() + i)) {
                CHECK(found < 0);   // must be a unique match, else the offset is ambiguous
                found = static_cast<long>(i);
            }
        }
        CHECK(found >= 0);
        return found;
    };

    // v4: 1 zero byte right after networksSeen(4B)+hackerRank(4B). No prior
    // insertion yet, so shapeBlob's offset applies to `blob` unshifted.
    long at = findOnce(shapeBlob, le32(0x1A2B3C4D)) + 8;
    blob.insert(blob.begin() + at, uint8_t{0});

    if (targetVersion >= 7) {
        // v7: an empty seenBssids vector (u16 count = 0) right after
        // handshakesSeen(4B). +1 corrects for the v4 byte already inserted above
        // (shapeBlob's own measurement is unaffected — it's never mutated).
        long hsAt = findOnce(shapeBlob, le32(0x5E6F7081)) + 4 + 1;
        blob.insert(blob.begin() + hsAt, {0, 0});
    }
    if (targetVersion >= 21) {
        // v21: the full 64-bit mask (8 bytes) right after yubiConsumed, before
        // fragAmountTier. +1 (v4) +2 (v7, always already inserted — v21 implies
        // v7) corrects for both prior insertions.
        const std::vector<uint8_t> shieldPat = {0xAA, 0xBB, 0xCC};
        long shAt = findOnce(shapeBlob, shieldPat) + 3 + 1 + 2;
        const std::vector<uint8_t> zeros8(8, 0);
        blob.insert(blob.begin() + shAt, zeros8.begin(), zeros8.end());
    }
    return blob;
}

inline Combatant mkCombatant(const ContentRegistry& r, const char* name, int hp,
                             int spd, std::vector<const char*> ids) {
    Combatant c;
    c.name = name; c.maxHealth = hp; c.health = hp; c.speed = spd;
    for (const char* id : ids)
        if (const MoveDef* m = r.move(id)) c.moves.push_back(m);
    return c;
}

// Play a whole Stacker board through the REAL button path, locking each row at
// `col(row)`. Leaves the run parked on its result; the caller presses once more to
// finish it. False if a column turned out unreachable, which would make the test a
// no-op rather than a failure. Shared by the Defrag gates and the arcade's.
template <typename ColFn>
bool playStackerBoard(Game& g, ColFn col) {
    for (int r = 0; r < kStackerRows && g.stacker().running(); ++r) {
        const int want = col(r);
        bool aligned = false;
        for (int guard = 0; guard < 4 * kStackerCols; ++guard) {
            if (g.stacker().left() == want) { aligned = true; break; }
            g.debugStepStacker();
        }
        if (!aligned) return false;
        g.onButton(press(Button::B));
    }
    return true;
}

// Open the GAMES cabinet on roster `row` and cycle its dial to `difficulty`. Leaves the
// player on the cabinet page (L3), one B away from starting the run.
inline void enterArcadeCabinet(Game& g, int row, ArcadeDifficulty difficulty) {
    // Back out of wherever the last run left the menu — enterSubmenuId starts from the
    // carousel, and a second cabinet in one session starts from the list it just used.
    for (int i = 0; i < 4 && (g.nav() == Game::Nav::Detail ||
                              g.nav() == Game::Nav::Submenu); ++i)
        tapC(g);
    enterSubmenuId(g, SubmenuId::Games);
    for (int i = 0; i < row; ++i) g.onButton(press(Button::A));
    g.onButton(press(Button::B));
    while (g.arcadeDifficulty() != difficulty) g.onButton(press(Button::A));
}

// Open the LOADOUT hub (MODS) on `row` — 0 MODS · 1 MOVES · 2 PRACTISE — and enter it.
inline void enterLoadoutTab(Game& g, int row) {
    enterSubmenuId(g, SubmenuId::Mods);
    for (int i = 0; i < row; ++i) g.onButton(press(Button::A));
    g.onButton(press(Button::B));
}

// Walk the hub to PRACTISE and launch the practice fight.
inline void enterSimBattle(Game& g) {
    enterLoadoutTab(g, 2);                        // -> tier pick (Detail)
    g.onButton(press(Button::B));                 // Start -> combat
}

// Arm explore-mode on the open starter sector: EXPL -> B drops back to the
// IDLE habitat with explore-mode running (there is no walk screen).
inline void enterWalk(Game& g) {
    // Two-level EXPL nav: entering parks on the area-0 header at the TOP
    // level; B drills into that area, a second B arms its first open sub-area.
    enterSubmenuId(g, SubmenuId::Expl);
    g.onButton(press(Button::B));                    // drill into the focused area
    g.onButton(press(Button::B));                    // arm its first open sub-area
    // Auto-progress now defaults ON (production), but this is the shared entry point
    // for tests that drive the walk fight-by-fight by hand — a streak/clear crossing
    // its threshold mid-test would otherwise auto-launch a boss (or re-arm the next
    // sub-area, resetting exploreSteps_) out from under whatever the test is mid-
    // asserting. Tests exercising auto-progress itself opt back in explicitly
    // (debugSetAutoProgress(true)), same as before this default flipped.
    g.debugSetAutoProgress(false);
}

// Fire the next GUARANTEED explore step NOW via the A+C control chord's Network Ping
// the chord opens the control overlay, A fires doExploreStep. Only valid
// from the idle habitat while exploring.
inline void pingExplore(Game& g) {
    g.onButton(chordAC());                        // A+C -> control overlay, on PING
    g.onButton(press(Button::B));                 // B -> do it (the next event)
}

// Cancel explore-mode through the real UI. STOP EXPLORE is the control overlay's LAST
// row, so this is: chord in (landing on PING), A to the bottom, B. A shared helper
// because a running walk claims the A+C chord — any test that needs the Hacker face
// has to put the walk down first, and would otherwise open the overlay instead.
inline void stopExplore(Game& g) {
    g.onButton(chordAC());
    for (int i = 0; i < kExploreControlRows - 1; ++i) g.onButton(press(Button::A));
    g.onButton(press(Button::B));
}

// Steps explore-mode until a guaranteed step types a WILD encounter. In the
// hands-off auto mode a wild encounter AUTO-STARTS the fight (no Fight/Flee intro,
// ) — so this lands the game IN live wild combat, bounded-searching past any other
// typed event (Wi-Fi / shop) along the way. (An awakened-guardian Wi-Fi outcome also
// enters live wild combat, — either is a valid stop for callers.)
inline void walkToEncounter(Game& g) {
    enterWalk(g);
    for (int i = 0; i < 400 && g.nav() != Game::Nav::Combat; ++i) {
        switch (g.nav()) {
            case Game::Nav::Idle: pingExplore(g); break;
            case Game::Nav::Wifi: g.onButton(press(Button::B)); break;  // may enter combat
            case Game::Nav::Shop: tapC(g); break;
            case Game::Nav::ModShop: tapC(g); break;
            default: g.onButton(press(Button::B)); break;
        }
    }
}

// Walks with a stack of Sinkhole Traps in hand (so any Wild encounter rolled
// along the way bypasses for free, no rng/steps cost) until the periodic roll
// types a Wi-Fi network event. A bounded search rather than hand-tracing the
// fixed-seed roll sequence — robust to any later rebalance of the event-type
// weights — mirroring dump_frame's own "wifi" flag.
inline void walkToWifiEvent(Game& g) {
    g.inventory().add("sinkhole_trap", 20);
    enterWalk(g);
    uint32_t t = 0;
    for (int i = 0; i < 400 && g.nav() != Game::Nav::Wifi; ++i) {
        if (g.nav() == Game::Nav::Encounter) {
            g.onButton(press(Button::A));   // Fight -> Flee
            g.onButton(press(Button::A));   // Flee -> Sinkhole
            g.onButton(press(Button::B));   // confirm -> back to idle
        } else if (g.nav() == Game::Nav::Shop || g.nav() == Game::Nav::ModShop) {
            tapC(g);   // leave the shop -> back to idle
        } else if (g.nav() == Game::Nav::Combat) {
            // An awakened-guardian fight — ride it out + dismiss so the Wi-Fi
            // search keeps going.
            for (int j = 0; j < 800 &&
                            g.combat().outcome() == Combat::Outcome::Ongoing; ++j)
                g.tick(t += kHeartbeatMs);
            g.onButton(press(Button::B));
        } else if (g.nav() == Game::Nav::PostEncounter) {
            g.onButton(press(Button::B));   // that fight's status readout -> dismiss
        } else if (g.nav() == Game::Nav::Idle) {
            if (g.exploreActive()) pingExplore(g); else enterWalk(g);
        }
    }
}

// Queues `bssid` and walks to the next Wi-Fi EXPL event (asserting it's
// reached), leaving the game AT Nav::Wifi with resolveNetworkDiscovery's own
// effects (credit / XP / Happiness / reward) ALREADY applied but the
// UNRELATED guardian/cache/friendly roll (startWifiEvent's other half, see
// game.h's comment on it) still unresolved. A test that cares about isolating
// resolveNetworkDiscovery's effects from that roll's own possible Bits/combat-
// XP side effects (an awakened-guardian win grants combat XP; a loot outcome
// can grant Bits) should snapshot state here, BEFORE calling
// resolveWifiEventToIdle below.
inline void queueAndReachWifiEvent(Game& g, const uint8_t* bssid, const char* ssid) {
    CHECK(g.registerNetwork(bssid, ssid));
    walkToWifiEvent(g);
    CHECK(g.nav() == Game::Nav::Wifi);
}

// Resolves whatever guardian/cache/friendly outcome the current Wi-Fi event
// rolled (riding out combat if awakened) back to Idle.
inline void resolveWifiEventToIdle(Game& g) {
    g.onButton(press(Button::B));
    if (g.nav() == Game::Nav::Combat) {
        uint32_t t = 0;
        for (int j = 0; j < 400 && g.combat().outcome() == Combat::Outcome::Ongoing; ++j)
            g.tick(t += kHeartbeatMs);
        g.onButton(press(Button::B));
    }
}

// Queues + walks + resolves in one step — for tests that don't need to isolate
// the moment between resolveNetworkDiscovery's own effects and the guardian/
// cache/friendly roll's (see queueAndReachWifiEvent's comment for why that
// moment sometimes matters).
inline void walkAndCreditNetwork(Game& g, const uint8_t* bssid, const char* ssid) {
    queueAndReachWifiEvent(g, bssid, ssid);
    resolveWifiEventToIdle(g);
}

// Loops the Walk, auto-resolving whatever the periodic roll types (Fight on
// any Wild encounter, continue through any Wi-Fi event), until it lands in
// real combat — either path (a Fight or an awakened Wi-Fi guardian) goes
// through buildPlayerCombatant, so this is the shared way to reach a REAL
// battle entry (as opposed to the debugStartCombat dev/test hook, which
// deliberately stays raw/unbuffed).
inline void walkToAnyCombat(Game& g) {
    uint32_t t = 0;
    for (int i = 0; i < 2000 && g.nav() != Game::Nav::Combat; ++i) {
        if (g.nav() == Game::Nav::Idle) {
            if (g.exploreActive()) pingExplore(g); else enterWalk(g);
        } else if (g.nav() == Game::Nav::Encounter) {
            g.onButton(press(Button::B));   // default choice 0 = Fight
        } else if (g.nav() == Game::Nav::Wifi) {
            g.onButton(press(Button::B));   // resolve whatever it is
        } else if (g.nav() == Game::Nav::Shop || g.nav() == Game::Nav::ModShop) {
            tapC(g);   // leave the shop -> back to idle
        } else if (g.nav() == Game::Nav::PostEncounter) {
            g.onButton(press(Button::B));   // a prior fight's status readout -> dismiss
        }
    }
}
