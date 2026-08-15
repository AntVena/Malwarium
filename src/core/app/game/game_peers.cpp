#include "core/app/game.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "core/app/game_internal.h"   // drawScrollbar — shared with game_crew.cpp
#include "core/content/content_crews.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/layout.h"
#include "core/ui/widgets.h"
#include "generated/assets.h"

// game_peers.cpp — the Hacker-face PEERS slot: the operators this device has met.
//
// A device announces itself (buildPeerHello -> the platform tier's ESP-NOW beacon,
// platform/esp32/net_link.h) and remembers whoever it hears (registerPeer). The
// announcing is gated on Game::linkWanted() — the persisted CFG opt-in, plus the
// stretch this screen raises it for — so a device with LINK off is silent and
// deaf, and opening this screen is itself the explicit act that makes it talk.
//
// Two sources feed one list, and keeping them apart is the point: livePeers_ is
// what the radio can hear this second, peerLedger_ is everyone ever met. A LIVE row
// renders from the snapshot's fresh HELLO, so it shows what someone is raising
// right now; a remembered row renders from the roster's last-meeting copy. The
// merge (peerRows) is what the screen draws, and LIVE leads it — the person
// standing in front of you is the one you're looking for.
//
// Everything but the key and the meeting tally is UNAUTHENTICATED (peer_link.h):
// it is what a device in range claimed about itself, sanitized but never verified.

namespace mal {

namespace {
// A peer block is tag / pet / crew — three of them clear the body under the header
// rule, leaving room for the hint band.
constexpr int kPeerBlockH = kLineH * 3 + 6;
constexpr int kPeerVisibleRows = 3;
}  // namespace

// --- The LINK opt-in ---------------------------------------------------------

void Game::setLinkEnabled(bool on) {
    if (linkEnabled_ == on) return;
    linkEnabled_ = on;
    markSaveDirty();   // a persisted CFG opt-in — survives a reboot (save v37)
    dirty_ = true;
}

// --- Announcing --------------------------------------------------------------

void Game::buildPeerHello(PeerHello* out) const {
    if (!out) return;
    PeerHello h;
    std::strncpy(h.tag, hackerTag_, sizeof(h.tag) - 1);

    // An operator with no pet yet (a fresh device on the Hatch screen) broadcasts an
    // empty species — the row reads as "no pet" rather than inventing one.
    if (pet_) {
        std::strncpy(h.petName, pet_->displayName, sizeof(h.petName) - 1);
        h.stage = static_cast<uint8_t>(pet_->stage);
    }

    if (const CrewDef* c = activeCrew()) {
        std::strncpy(h.crewName, c->displayName, sizeof(h.crewName) - 1);
        h.crewRed = c->team == CrewTeam::Red;
    }

    // Rank is unbounded in the model but one byte on the wire; clamping beats
    // wrapping a rank-260 operator back to 4.
    h.rank = static_cast<uint8_t>(std::min(hackerRank_, 255));
    *out = h;
}

// --- Hearing -----------------------------------------------------------------

bool Game::registerPeer(const uint8_t* mac, const uint8_t* payload, size_t len) {
    if (!mac) return false;
    PeerHello hello;
    // A frame that fails to decode is simply not a peer: wrong magic, a protocol
    // version this build can't lay out, or a truncated payload. Dropped silently —
    // anything in range can transmit, so a bad frame is background noise, not an
    // error worth surfacing.
    if (!decodePeerHello(payload, len, &hello)) return false;

    const uint64_t key = packPeerKey(mac);
    if (key == 0) return false;

    // The snapshot takes every beacon; the roster only takes the arrival edge. That
    // split is what makes timesMet a count of MEETINGS — a device parked next to
    // this one beacons once a second and must still read as one encounter.
    if (!noteLivePeer(key, hello)) return false;

    const bool firstEver = peerLedger_.record(key, hello);
    log_.push(LogEventType::ItemGained, hello.tag[0] ? hello.tag : "UNKNOWN OP");
    dirty_ = true;
    return firstEver;
}

bool Game::noteLivePeer(uint64_t key, const PeerHello& hello) {
    int stalest = -1;
    uint32_t stalestSeen = 0;
    for (int i = 0; i < livePeerCount_; ++i) {
        LivePeer& p = livePeers_[i];
        if (p.key == key) {
            const bool arrived = !peerLive(p);   // aged out and came back = a new meeting
            p.hello = hello;                     // a live row always shows the latest
            p.lastSeenMs = nowMs_;
            return arrived;
        }
        const uint32_t age = nowMs_ - p.lastSeenMs;
        if (stalest < 0 || age > stalestSeen) { stalest = i; stalestSeen = age; }
    }

    int slot;
    if (livePeerCount_ < kPeerVisibleCap) {
        slot = livePeerCount_++;
    } else {
        // Full: evict the STALEST entry rather than refusing the new one, so a room
        // that has churned through more than kPeerVisibleCap devices still tracks
        // whoever is actually present now.
        slot = stalest >= 0 ? stalest : 0;
    }
    livePeers_[slot].key = key;
    livePeers_[slot].hello = hello;
    livePeers_[slot].lastSeenMs = nowMs_;
    return true;
}

void Game::loadPeerLedger(const char* path) { peerLedger_.load(path); }

void Game::debugSeedPeer(uint64_t key, const PeerHello& hello) {
    peerLedger_.record(key, hello);
    dirty_ = true;
}

void Game::debugSeedLivePeer(uint64_t key, const PeerHello& hello) {
    // Both halves, which is what a real beacon does (registerPeer above): a peer
    // standing in front of you is in range AND has now been met.
    noteLivePeer(key, hello);
    peerLedger_.record(key, hello);
    dirty_ = true;
}

int Game::livePeerCount() const {
    int n = 0;
    for (int i = 0; i < livePeerCount_; ++i)
        if (peerLive(livePeers_[i])) ++n;
    return n;
}

// --- The merged list ---------------------------------------------------------

int Game::peerRows(PeerRow* rows, int max) const {
    if (!rows || max <= 0) return 0;
    int n = 0;

    // Live first, straight from the snapshot — these rows show what the operator is
    // raising THIS SECOND, which is the whole reason the snapshot keeps the full
    // HELLO rather than just a timestamp.
    for (int i = 0; i < livePeerCount_ && n < max; ++i) {
        const LivePeer& p = livePeers_[i];
        if (!peerLive(p)) continue;
        PeerLedger::Entry known;
        const uint32_t met = peerLedger_.lookup(p.key, &known) ? known.timesMet : 1;
        rows[n++] = {p.key,        p.hello.tag,  p.hello.petName, p.hello.crewName,
                     p.hello.stage, p.hello.rank, p.hello.crewRed, true, met};
    }

    // Then everyone else ever met, from the roster's last-meeting copy. Skipping the
    // ones already emitted above keeps a live operator to a single row rather than
    // listing them twice with stale data underneath.
    for (const auto& e : peerLedger_.entries()) {
        if (n >= max) break;
        bool alreadyLive = false;
        for (int i = 0; i < n; ++i)
            if (rows[i].key == e.key) { alreadyLive = true; break; }
        if (alreadyLive) continue;
        rows[n++] = {e.key,  e.tag,  e.petName, e.crewName,
                     e.stage, e.rank, e.crewRed, false, e.timesMet};
    }
    return n;
}

// --- Input -------------------------------------------------------------------

void Game::onHackerPeers(const ButtonEvent& ev) {
    if (ev.button == Button::C) { nav_ = Nav::Cursor; return; }

    PeerRow rows[kPeerMaxRows];
    const int n = peerRows(rows, kPeerMaxRows);
    if (n == 0) return;                        // empty state: A/B inert
    if (peerRow_ >= n) peerRow_ = 0;
    if (ev.button == Button::A) peerRow_ = (peerRow_ + 1) % n;
    // B is unbound here: this screen only reports who is around. The interactions a
    // selected peer would enable live on the LINK slot.
}

// --- Rendering ---------------------------------------------------------------

void Game::drawHackerPeers(Framebuffer& fb) const {
    PeerRow rows[kPeerMaxRows];
    const int n = peerRows(rows, kPeerMaxRows);

    drawText(fb, kMargin, kContextY, "PEERS MET", palColor(Pal::INK));

    // Name whoever actually holds the radio rather than claiming LINKING
    // unconditionally — an empty list means something entirely different when the
    // 'Pedia AP has the radio than when nobody is around. Doubles as the notice that
    // this screen announces on its own, so an operator who deliberately keeps LINK
    // off is never surprised into broadcasting.
    const bool starved = apEnabled_ || auditCapture_.capturing();
    const char* radioState = apEnabled_                  ? "AP HAS RADIO"
                             : auditCapture_.capturing() ? "CAPTURE HAS RADIO"
                                                         : "LINKING";
    drawText(fb, kActiveW - kMargin - textWidth(radioState), kContextY, radioState,
             starved ? palColor(Pal::WARN) : palColor(Pal::INK_DIM));
    fb.fillRect(0, kContextRule, kActiveW, 1, palColor(Pal::TRACK));

    if (n == 0) {
        // Empty for three very different reasons — say which. Words, never colour.
        drawText(fb, kMargin, 54,
                 starved ? "LINK CANT RUN" : "NOBODY MET YET", palColor(Pal::INK_DIM));
        drawText(fb, kMargin, 54 + kLineH,
                 apEnabled_                  ? "TURN OFF AP IN CFG"
                 : auditCapture_.capturing() ? "CAPTURE IN PROGRESS"
                                             : "LISTENING...",
                 palColor(Pal::INK_DIM));
    } else {
        int sel = peerRow_;
        if (sel >= n) sel = 0;
        int scrollTop = 0;
        if (n > kPeerVisibleRows) {
            if (sel >= kPeerVisibleRows) scrollTop = sel - kPeerVisibleRows + 1;
            scrollTop = std::min(scrollTop, n - kPeerVisibleRows);
        }
        for (int v = 0; v < kPeerVisibleRows && scrollTop + v < n; ++v) {
            const PeerRow& row = rows[scrollTop + v];
            const int rowY = 52 + v * kPeerBlockH;
            const bool s = scrollTop + v == sel;
            if (s) drawRowCursor(fb, 2, rowY + 1, palColor(Pal::ACCENT));

            drawText(fb, kMargin, rowY, row.tag[0] ? row.tag : "UNKNOWN OP",
                     s ? palColor(Pal::INK) : palColor(Pal::INK_DIM));

            // LIVE vs. how many times we've met is the one thing that must survive
            // grayscale, so it's a word and a count, never a colour.
            char state[16];
            if (row.live) std::snprintf(state, sizeof(state), "LIVE");
            else std::snprintf(state, sizeof(state), "x%u",
                               static_cast<unsigned>(row.timesMet));
            drawText(fb, kActiveW - kMargin - textWidth(state), rowY, state,
                     row.live ? palColor(Pal::ACCENT) : palColor(Pal::INK_DIM));

            // What they're raising. An operator between pets broadcasts nothing here.
            char pet[40];
            if (row.petName[0])
                std::snprintf(pet, sizeof(pet), "%s  %s", row.petName,
                              stageName(static_cast<Stage>(row.stage)));
            else
                std::snprintf(pet, sizeof(pet), "NO PET");
            drawText(fb, kMargin + 6, rowY + kLineH, pet, palColor(Pal::INK_DIM));

            // Crew + rank. Team is dual-coded by its glyph beside the name, never by
            // the colour of it.
            char crew[40];
            std::snprintf(crew, sizeof(crew), "%s  RANK %u",
                          row.crewName[0] ? row.crewName : "NO CREW",
                          static_cast<unsigned>(row.rank));
            drawText(fb, kMargin + 6, rowY + kLineH * 2, crew, palColor(Pal::INK_DIM));
            if (row.crewName[0])
                drawSprite(fb,
                           row.crewRed ? ASSET_ICON_TEAM_RED : ASSET_ICON_TEAM_BLUE,
                           0, kActiveW - kMargin - 20, rowY + kLineH * 2 - 5);
        }
        drawScrollbar(fb, 52, kPeerVisibleRows * kPeerBlockH, n, scrollTop,
                      kPeerVisibleRows);
    }

    // Ambient discovery borrows the audit scan's radio window, so with the scan off
    // this screen is the ONLY place meetings happen. Saying so here is what keeps a
    // LINK-on/AUDIT-off device from looking broken — the toggle is working, it just
    // has no ambient airtime to use.
    if (linkAmbientStarved())
        drawText(fb, kMargin, kActiveH - 34, "AMBIENT NEEDS AUDIT SCAN",
                 palColor(Pal::WARN));

    fb.fillRect(0, kActiveH - 26, kActiveW, 1, palColor(Pal::TRACK));
    drawText(fb, kMargin, kActiveH - 20, "A NEXT", palColor(Pal::INK_DIM));
    drawText(fb, kActiveW - kMargin - textWidth("C BACK"), kActiveH - 20, "C BACK",
             palColor(Pal::ACCENT));
}

}  // namespace mal
