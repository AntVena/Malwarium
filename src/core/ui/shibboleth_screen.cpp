#include "core/ui/shibboleth_screen.h"

#include "core/model/cant.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"

#include <cstdio>
#include "core/ui/layout.h"
#include "core/ui/widgets.h"

namespace mal {

namespace {

// The riddle sits under the header with room to flow; the replies pack down from a
// fixed top so the cursor lands in the same place whatever length the question ran to.
// A shorter riddle therefore leaves air rather than sliding the choices up, which is
// what lets a player answer without re-reading where the rows went.
constexpr int kRiddleTop = 34;
constexpr int kRiddleLineH = kFontH + 3;
constexpr int kReplyTop = 108;

// The content pool's cell counts (content_riddles.h) resolved against the face. This is
// the only place the two meet: the pool is authored in columns and drawn in pixels.
constexpr int kRiddleBodyW = kRiddleBodyCols * kFontAdvance;
constexpr int kRiddleReplyIndent = kRiddleReplyIndentCols * kFontAdvance;
constexpr int kRiddleReplyW = kRiddleReplyCols * kFontAdvance;
constexpr int kReplyPitch = 20;

// The Cant strip along the foot: 26 cells, one per letter, on the same 8px grid the
// text uses so a lit cell lines up with the glyph it stands for.
constexpr int kStripCellW = kFontAdvance;
constexpr int kStripW = kCantSigils * kStripCellW;
constexpr int kStripX = (kActiveW - kStripW) / 2;
constexpr int kStripY = kActiveH - kHintBandH - 14;
constexpr int kStripH = 9;

// The answer clock's bar, immediately over the strip.
constexpr int kClockY = kStripY - 6;

void drawCantStrip(Framebuffer& fb, uint32_t sigils) {
    for (int i = 0; i < kCantSigils; ++i) {
        const int x = kStripX + i * kStripCellW;
        const bool known = (sigils & (1u << i)) != 0;
        if (known) {
            // A learned sigil is drawn as its own letter on a filled cell — the letter
            // IS the dual coding, so the strip reads at a glance in grayscale and reads
            // exactly as precisely in colour.
            fb.fillRect(x, kStripY, kStripCellW - 1, kStripH, palColor(Pal::ACCENT));
            const char s[2] = {static_cast<char>('A' + i), '\0'};
            drawText(fb, x, kStripY + 1, s, palColor(Pal::PAPER));
        } else {
            // Unlearned: a bare rule, not a letter. Drawing the letter dimmed would say
            // the pet half-knows it, and it does not know it at all.
            fb.fillRect(x, kStripY + kStripH - 1, kStripCellW - 1, 1,
                        palColor(Pal::TRACK));
        }
    }
}

}  // namespace

void drawShibboleth(Framebuffer& fb, const char* guardian, const char* riddle,
                    const char* const replies[kRiddleReplies], int cursor,
                    uint32_t sigils, float holdFrac) {
    // The guardian's own name is the title, and it is ACCENT rather than the wild
    // encounter's WARN: this is not an alarm. Something is talking to the pet.
    char count[12];
    std::snprintf(count, sizeof(count), "%d/%d", sigilCount(sigils), kCantSigils);
    drawHeaderBand(fb, guardian ? guardian : "", count, palColor(Pal::INK_DIM),
                   palColor(Pal::ACCENT));

    // The question, flowed. Enciphered upstream, so a letter the pet can read arrives
    // here already reading — nothing on this screen distinguishes the two, which is
    // exactly right: to the pet they are one sentence with holes in it.
    if (riddle && riddle[0])
        drawTextWrapped(fb, kMargin, kRiddleTop, kRiddleBodyW, riddle,
                        palColor(Pal::INK), kRiddleLineH, kRiddleBodyLines);

    for (int i = 0; i < kRiddleReplies; ++i) {
        const int y = kReplyTop + i * kReplyPitch;
        const bool on = (i == cursor);
        if (on) drawRowCursor(fb, kMargin, y, palColor(Pal::ACCENT));
        // Never scrolls: the content gate holds every reply to kRiddleReplyW on one
        // line, so a travelling row here would only ever mean a row that should have
        // failed the gate.
        drawTextMarquee(fb, kMargin + kRiddleReplyIndent, y, kRiddleReplyW,
                        replies && replies[i] ? replies[i] : "",
                        on ? palColor(Pal::ACCENT) : palColor(Pal::INK),
                        /*beat=*/0, /*scroll=*/false);
    }

    // The patience bar. It empties rather than fills — what is draining is the time the
    // guardian is willing to wait, and a bar that grew would read as progress.
    const float left = 1.0f - (holdFrac < 0.0f ? 0.0f : (holdFrac > 1.0f ? 1.0f : holdFrac));
    drawProgressBar(fb, kMargin, kClockY, kActiveW - 2 * kMargin, 3, left,
                    palColor(left < 0.25f ? Pal::WARN : Pal::INK_DIM));

    drawCantStrip(fb, sigils);

    // C is spelled out because its meaning bends here: backing away from something that
    // asked you a question is an answer, and the wrong one. Saying "BACK" would be a lie
    // about what the button does.
    drawHintBand(fb, "A SWITCH  B SPEAK  C REFUSE");
}

}  // namespace mal
