#include "core/app/game/feeding_screen.h"

#include "core/model/feeding.h"
#include "core/ui/modals.h"

namespace mal {

void FeedingScreen::draw(Framebuffer& fb, const FeedingHost& host) const {
    FeedingView v;
    v.pet = host.petSprite();
    if (const Feeding* f = host.feeding()) {
        v.food = f->item;
        v.foodIcon = host.itemSprite(f->item->id);
        v.before = f->before;
        v.beat = f->beat;
    }
    v.fxBeat = host.fxBeat();
    drawFeedingModal(fb, host.model(), v);
}

}  // namespace mal
