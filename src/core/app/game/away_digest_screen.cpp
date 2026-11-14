#include "core/app/game/away_digest_screen.h"

#include <cstdio>

#include "core/content/defs.h"   // CreatureDef::displayName
#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/font.h"
#include "core/render/palette.h"
#include "core/ui/layout.h"
#include "core/ui/widgets.h"

namespace mal {

std::vector<AwayRow> AwayDigestScreen::rows(const AwayTally& t, int careMistakes) {
    std::vector<AwayRow> out;
    const auto add = [&](const char* label, AwayRowTone tone, const char* fmt, auto... v) {
        AwayRow r;
        r.label = label;
        r.tone = tone;
        std::snprintf(r.value, sizeof r.value, fmt, v...);
        out.push_back(r);
    };
    const AwayRowTone plain = AwayRowTone::Plain, cost = AwayRowTone::Cost;
    if (t.hatched) add("HATCHED INTO", plain, "%s", t.hatched->displayName);
    if (t.evolved) add("EVOLVED INTO", plain, "%s", t.evolved->displayName);
    if (t.hungry) add("WENT HUNGRY", cost, "%dX", t.hungry);
    if (t.missed) add("LOCKOUTS MISSED", cost, "%d", t.missed);
    if (t.errors) add("ERRORS", cost, "+%d, NOW %d", t.errors, careMistakes);
    if (t.won) add("FIGHTS WON", plain, "%d", t.won);
    if (t.lost) add("FIGHTS LOST", plain, "%d", t.lost);
    if (t.caches) add("CACHES FOUND", plain, "%d", t.caches);
    return out;
}

void AwayDigestScreen::onButton(AwayDigestHost& host, const ButtonEvent& ev) {
    if (ev.button == Button::B || ev.button == Button::C) host.closeAwayDigest();
}

void AwayDigestScreen::draw(Framebuffer& fb, const AwayDigestHost& host) const {
    fb.clear(palColor(Pal::PAPER));
    drawHeaderBand(fb, "WHILE YOU WERE AWAY");
    char quiet[28];
    std::snprintf(quiet, sizeof quiet, "%u:%02u AWAY (DEVICE ON)",
                  static_cast<unsigned>(quietMs_ / 3600000u),
                  static_cast<unsigned>((quietMs_ / 60000u) % 60u));
    drawText(fb, kMargin, 32, quiet, palColor(Pal::INK_DIM));
    int y = 52;
    for (const AwayRow& r : rows(host.awayTally(), host.careMistakes())) {
        drawLabelValue(fb, kMargin, y, r.label, palColor(Pal::INK), r.value,
                       r.tone == AwayRowTone::Cost ? palColor(Pal::WARN) : palColor(Pal::INK_DIM),
                       0, Overflow::Clip);
        y += 18;
    }
    drawHintBand(fb, "B OK");
}

}  // namespace mal
