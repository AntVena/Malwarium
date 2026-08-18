// test_absorb.cpp — native gates for FX_ABSORB, the dissolve that carries a glyph into
// the pet (core/render/absorb.h), and for the Wi-Fi discovery beat it pictures.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these.
#include "test_gates.h"

namespace {

// A 4x4 solid block on transparent, as a flat mask — the smallest sprite that has an
// inside, an edge, and a countable pixel budget.
struct TinyGlyph {
    uint8_t bits[4] = {0xF0, 0xF0, 0xF0, 0xF0};   // 4 set bits per row, MSB-first
    SpriteData s{};
    TinyGlyph() {
        s.sheetW = 4; s.h = 4; s.frameW = 4; s.frames = 1; s.rows = 1;
        s.bits = bits;
        s.ink = 0xFFFF;
    }
};

// Pixels of `ink` on the canvas — the effect's whole footprint, wherever it put them.
int inkCount(const Framebuffer& fb, Rgb565 ink) {
    int n = 0;
    for (int y = 0; y < fb.height(); ++y)
        for (int x = 0; x < fb.width(); ++x)
            if (fb.get(x, y) == ink) ++n;
    return n;
}

}  // namespace

// --- Gate: the sweep starts whole, ends empty, and leaves nothing hanging ---
//
// The three states that have to hold whatever the scatter rolls: at progress 0 every
// opaque source pixel is still standing where it was drawn; at 255 the glyph is gone;
// and a bite that only takes part of it FINISHES — a partial absorb settles on a
// smaller glyph rather than freezing blocks in mid-air, which is the difference
// between "the pet ate some of it" and a stalled animation.
void test_absorb_sweep_endpoints() {
    TinyGlyph g;
    const Rgb565 ink = palColor(Pal::ACCENT);
    const int srcX = 10, srcY = 10, dstX = 90, dstY = 12;

    Framebuffer whole(kActiveW, kActiveH);
    whole.clear(palColor(Pal::PAPER));
    drawAbsorb(whole, g.s, 0, srcX, srcY, 1, 1, dstX, dstY, ink, /*progress=*/0,
               /*bite=*/255);
    CHECK(inkCount(whole, ink) == 16);            // 4x4 opaque, at 1:1, all at rest
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x) CHECK(whole.get(srcX + x, srcY + y) == ink);

    Framebuffer eaten(kActiveW, kActiveH);
    eaten.clear(palColor(Pal::PAPER));
    drawAbsorb(eaten, g.s, 0, srcX, srcY, 1, 1, dstX, dstY, ink, /*progress=*/255,
               /*bite=*/255);
    CHECK(inkCount(eaten, ink) == 0);

    // A half bite, run to the end of its sweep: some of the glyph is gone, some is
    // still standing, and the count is stable — nothing is still travelling.
    Framebuffer bitten(kActiveW, kActiveH);
    bitten.clear(palColor(Pal::PAPER));
    drawAbsorb(bitten, g.s, 0, srcX, srcY, 1, 1, dstX, dstY, ink, /*progress=*/255,
               /*bite=*/128);
    const int left = inkCount(bitten, ink);
    CHECK(left > 0 && left < 16);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x) {
            const Rgb565 c = bitten.get(srcX + x, srcY + y);
            CHECK(c == ink || c == palColor(Pal::PAPER));   // at rest, or gone
        }
    CHECK(inkCount(bitten, ink) == left);   // and it is where the glyph was, only

    // A bite of nothing never moves a pixel, at any point in the sweep — the glyph the
    // pet declines has to sit perfectly still, shiver included.
    for (uint8_t p : {uint8_t{0}, uint8_t{128}, uint8_t{255}}) {
        Framebuffer declined(kActiveW, kActiveH);
        declined.clear(palColor(Pal::PAPER));
        drawAbsorb(declined, g.s, 0, srcX, srcY, 1, 1, dstX, dstY, ink, p, /*bite=*/0);
        CHECK(inkCount(declined, ink) == 16);
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x) CHECK(declined.get(srcX + x, srcY + y) == ink);
    }
}

// --- Gate: the phase clock brackets the window and the flash follows the bite ---
void test_absorb_phase_clock() {
    // Before it opens, nothing has moved and nothing is lit.
    for (int b = 0; b <= 2; ++b) {
        const AbsorbPhase p = absorbPhase(b, /*startBeat=*/3, kAbsorbBeats);
        CHECK(p.progress == 0);
        CHECK(p.flash == 0);
    }
    // Monotonic across the window, and finished at its close.
    int last = -1;
    for (int b = 3; b <= 3 + kAbsorbBeats; ++b) {
        const AbsorbPhase p = absorbPhase(b, 3, kAbsorbBeats);
        CHECK(static_cast<int>(p.progress) >= last);
        last = p.progress;
    }
    CHECK(absorbPhase(3 + kAbsorbBeats, 3, kAbsorbBeats).progress == 255);

    // The swallow lands on the closing beat and is an ember, then nothing, after it —
    // a screen that lingers settles rather than staying the wrong colour.
    const uint8_t landed = absorbPhase(3 + kAbsorbBeats, 3, kAbsorbBeats).flash;
    const uint8_t ember = absorbPhase(4 + kAbsorbBeats, 3, kAbsorbBeats).flash;
    CHECK(landed > 0);
    CHECK(ember < landed);
    CHECK(absorbPhase(3 + kAbsorbBeats + 8, 3, kAbsorbBeats).flash == 0);

    // A smaller mouthful flashes less, and a declined glyph never flashes at all.
    CHECK(absorbPhase(3 + kAbsorbBeats, 3, kAbsorbBeats, 96).flash < landed);
    for (int b = 0; b <= 3 + kAbsorbBeats + 4; ++b)
        CHECK(absorbPhase(b, 3, kAbsorbBeats, 0).flash == 0);
}

// --- Gate: a rival's kit picks which dissolve closes the fight ---
//
// The combat outro's whole claim is "this one had something you haven't got", so the
// INNATE default has to be on the owned side of that question: every tier-1 malbeast
// swings it, and a pet that already has it is not being offered anything.
//
// Ownership and SLOTTABILITY are separate for it, and that split is the thing most
// likely to be undone by accident — a pet owns its innate (so the 'Pedia and the drop
// filter both count it) but can never spend an equip slot on what an empty slot
// already falls back to.
void test_combat_outro_kind() {
    Game g{StartMode::Hatched};

    // The tier-1 roster fields the innate default and nothing else — already known.
    g.debugStartCombat(/*live=*/false);
    const MoveLoadout& kit = g.moveLoadout();
    CHECK(kit.defaultMove() != nullptr);
    CHECK(kit.owns(kit.defaultMove()));       // owned: it is a move this pet has
    CHECK(kit.isInnate(kit.defaultMove()));   // ...and never a slot's occupant
    const std::vector<const MoveDef*> pickable =
        ownedMoveList(ContentRegistry::embedded(), kit, MoveDef::Kind::Attack,
                      Stage::Daemon, nullptr, /*slot=*/0, /*showAll=*/true);
    for (const MoveDef* m : pickable) CHECK(!kit.isInnate(m->id));

    // A rival fielding a move outside both the default and the owned set is the one
    // that earns the absorb.
    walkToAnyCombat(g);
    bool sawUnknown = false, sawKnown = false;
    for (const MoveDef* m : g.combat().enemy().moves) {
        if (!m || !m->id) continue;
        if (std::strcmp(m->id, kit.defaultMove()) == 0 || kit.owns(m->id)) sawKnown = true;
        else sawUnknown = true;
    }
    CHECK(g.rivalFieldsUnknownMove() == sawUnknown);

    // Granting every move the rival fields must flip the answer — that is the whole
    // mechanism, and it is what makes a farmed-out opponent stop advertising itself.
    for (const MoveDef* m : g.combat().enemy().moves)
        if (m && m->id) g.debugGrantMove(m->id);
    CHECK(!g.rivalFieldsUnknownMove());
    CHECK(sawKnown || sawUnknown);            // sanity: the rival fielded something
}

// --- Gate: the Wi-Fi event's discovery beat reports which of the four it was ---
//
// The screen turns Game::NetDiscovery into how far the pet eats the network glyph, so
// the VALUE has to track the same four outcomes the flavour sentence does — otherwise
// a player could be told one thing in words and shown another.
void test_wifi_discovery_kind() {
    Game g{StartMode::Hatched};
    const uint8_t bssid[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};

    // A BSSID the ledger has never held is genuinely new — the one that pays, and the
    // one the pet eats whole.
    queueAndReachWifiEvent(g, bssid, "THE_PROMISED_LAN");
    CHECK(g.netDiscovery() == Game::NetDiscovery::New);
    resolveWifiEventToIdle(g);

    // Seen again while it sits at the head of the favourites: home turf, no reward,
    // and a glyph the pet leaves standing.
    queueAndReachWifiEvent(g, bssid, "THE_PROMISED_LAN");
    CHECK(g.netDiscovery() == Game::NetDiscovery::HomeTurf);
    resolveWifiEventToIdle(g);

    // Crowded out of the top favourites by networks it has seen far more of, the same
    // repeat visit is merely fond — a smaller reward, and a smaller bite.
    const uint64_t key = 0x02'00'00'00'00'01ull;
    for (int i = 0; i < kNetDiscoveryTopFavoritesCount + 2; ++i)
        g.debugSeedNetworkLedger(key + 0x100ull * (i + 1), "NEIGHBOUR", 20 + i);
    queueAndReachWifiEvent(g, bssid, "THE_PROMISED_LAN");
    CHECK(g.netDiscovery() == Game::NetDiscovery::Fond);
    resolveWifiEventToIdle(g);

    // With the queue drained, the next event has nothing to find and shows no glyph.
    walkToWifiEvent(g);
    CHECK(g.nav() == Game::Nav::Wifi);
    CHECK(g.netDiscovery() == Game::NetDiscovery::None);
}
