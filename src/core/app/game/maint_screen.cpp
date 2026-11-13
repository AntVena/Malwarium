#include "core/app/game/maint_screen.h"

#include "tunables.h"
#include "core/app/operator.h"
#include "core/content/content_tables.h"   // kDefragToolId
#include "core/model/inventory.h"
#include "core/model/pet.h"
#include "core/ui/layout.h"
#include "core/ui/maint_screen.h"

namespace mal {

void MaintScreen::onButton(MaintHost& host, const ButtonEvent& ev) {
    // A cycles the payment VARIANT, and on the PLAYED row it walks the difficulty dial
    // first, EASY up to HARD, before wrapping back to Quick (and the dial back to EASY).
    // One button for both, the way the arcade cabinet page cycles its dial, so the
    // screen never needs a second gesture to set the stakes. B runs the focused one.
    if (ev.button == Button::A) {
        const int n = kind_ == MaintKind::Defrag ? kDefragVariants : kAvVariants;
        const bool played = variant_ == n - 1;
        if (played && difficulty_ != ArcadeDifficulty::Hard) {
            difficulty_ = static_cast<ArcadeDifficulty>(static_cast<int>(difficulty_) + 1);
        } else {
            if (played) difficulty_ = ArcadeDifficulty::Easy;
            variant_ = stepRow(variant_, n, +1);
        }
    } else if (ev.button == Button::B) {
        host.startMaint(kind_, variant_, difficulty_);
    } else if (ev.button == Button::C) {
        host.closeDetail();
    }
}

void MaintScreen::draw(Framebuffer& fb, const MaintHost& host) const {
    MaintActionView v;
    if (const ArcadeGameDef* g = arcadeGameByKind(host.maintPlayedGame(kind_))) {
        v.played.name = g->displayName;
        v.played.unit = g->unitName ? g->unitName : "RUN";
        v.played.bitsUnit = g->bitsUnitName;
    }
    v.played.difficulty = difficulty_;
    v.kind = kind_;
    v.variant = variant_;
    v.cost = kind_ == MaintKind::Defrag ? host.defragCost() : 0;
    v.walletBits = host.operatorRecord().bits;
    v.toolCount = host.inventory().count(kDefragToolId);
    v.defragCount = host.petRecord().defragCount;
    v.failPct = host.maintFailPct();
    drawMaintAction(fb, host.petRecord().model, v);
}

}  // namespace mal
