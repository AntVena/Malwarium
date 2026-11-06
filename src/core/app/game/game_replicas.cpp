#include "core/app/game.h"

#include <algorithm>

#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/worm_replicas.h"
#include "generated/assets.h"

// game_replicas.cpp: a replicating line's copies in the idle habitat. In a fight the
// board is Combatant::wormReplicas, standing in rank between parent and enemy; at home
// there is no combat state, so the copies mill about on their own wanders, and now and
// then a burst of extras past the line's slots crawls out and dies (core/model/idle_fork.h).
// Between bursts the copies pass a byte along (core/model/idle_relay.h).

namespace mal {

namespace {

// Slot i's glyph and seat, one slot-width further out per pair, alternating sides so the
// family stays balanced around the parent. Glyphs alternate attack/defend, as the line's
// slots do.
const SpriteData& replicaGlyph(int i) {
    return (i & 1) ? ASSET_SPR_WORM_REPLICA_DEFEND : ASSET_SPR_WORM_REPLICA_ATTACK;
}

int replicaSeatX(int i) {
    return kActiveW / 2 + kReplicaSlotW * (i / 2 + 1) * ((i & 1) ? -1 : 1);
}

// A seat plus a full drift can reach past the bezel. Half a copy does not read as a copy.
int clampOnCanvas(const SpriteData& glyph, int cx) {
    const int w = glyph.frameW * kScaleNum / kScaleDen;
    return std::max(w / 2, std::min(kActiveW - w / 2, cx));
}

// Extra k takes the seat after the family's last copy, `copies + k`.
void drawFork(Framebuffer& fb, const IdleFork& fork, int copies, int petCx, int beat) {
    for (int k = 0; k < fork.extras(); ++k) {
        const SpriteData& glyph = replicaGlyph(copies + k);
        const int seatCx = clampOnCanvas(glyph, replicaSeatX(copies + k));
        switch (fork.stage()) {
            case ForkStage::Rest:
                return;
            case ForkStage::Spawn: {
                const int age = fork.crawlBeat(k);
                const int cx = petCx + (seatCx - petCx) * age / kIdleForkSpawnBeats;
                drawReplica(fb, glyph, kReplicaIdleFrame + (beat & 1), cx, kLivingBottom);
                break;
            }
            case ForkStage::Look: {
                // Each extra turns on its own beat, so the crowd glances about.
                const bool flipped = ((fork.beatInStage() + k) / 2) & 1;
                drawReplica(fb, glyph, kReplicaIdleFrame, seatCx, kLivingBottom, kScaleNum,
                            kScaleDen, flipped ? Mirror::Flipped : Mirror::None);
                break;
            }
            case ForkStage::Die:
                drawReplica(fb, glyph, kReplicaDeathFrame + fork.beatInStage(), seatCx,
                            kLivingBottom);
                break;
        }
    }
}

// The byte in flight: a block at the head of a parabola from `fromCx` to `toCx`, with a
// dotted trail behind it over the part of the arc already flown. y is the copies' top.
constexpr int kPacketPx = 6;
constexpr int kArcRisePx = 22;
constexpr int kTrailDotsPerBeat = 3;

void drawPacket(Framebuffer& fb, int fromCx, int toCx, int topY, int beatInHop) {
    const Rgb565 ink = palColor(Pal::INK);
    const int steps = kIdleRelayHopBeats * kTrailDotsPerBeat;
    const int head = (beatInHop + 1) * kTrailDotsPerBeat;
    for (int s = 0; s <= head; ++s) {
        const int x = fromCx + (toCx - fromCx) * s / steps;
        const int y = topY - kArcRisePx * 4 * s * (steps - s) / (steps * steps);
        if (s == head)
            fb.fillRect(x - kPacketPx / 2, y - kPacketPx, kPacketPx, kPacketPx, ink);
        else if (s % 2 == 0)
            fb.fillRect(x - 1, y - kPacketPx / 2, 2, 2, ink);
    }
}

}  // namespace

void Game::drawIdleReplicas(Framebuffer& fb, int petCx) const {
    const int copies = idleCompanionCount();
    int seatedCx[kWormReplicaSlots] = {};
    for (int i = 0; i < copies; ++i) {
        const SpriteData& glyph = replicaGlyph(i);
        const int cx = replicaSeatX(i) + logicalToActive(companionWander(i).offsetX());
        seatedCx[i] = clampOnCanvas(glyph, cx);
        const int pose = petRelay_.chomping() == i ? kReplicaAttackFrame : kReplicaIdleFrame;
        drawReplica(fb, glyph, pose + (beat_ & 1), seatedCx[i], kLivingBottom);
    }
    if (const int h = petRelay_.hop(); h >= 0 && h < copies) {
        const int topY = kLivingBottom - replicaGlyph(h).h * kScaleNum / kScaleDen;
        drawPacket(fb, h == 0 ? petCx : seatedCx[h - 1], seatedCx[h], topY,
                   petRelay_.beatInHop());
    }
    if (petReplicates()) drawFork(fb, petFork_, copies, petCx, beat_);
}

}  // namespace mal
