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

// The block reads as a script: the stage direction first, then what the guardian says,
// then what it asks. Every one of these tops is FIXED rather than flowed — a shorter
// riddle leaves air instead of sliding the choices up, which is what lets a player
// answer without re-reading where the rows went between encounters.
//
// The demeanour gets one line and the greeting two, which is the budget the content gate
// holds them to: a stage direction that runs long stops being a glance and starts
// competing with the riddle, which is the thing being read.
constexpr int kDemeanourY = 32;
constexpr int kGreetingY = 48;
constexpr int kGreetingLines = 2;
constexpr int kRiddleTop = 76;
constexpr int kRiddleLineH = kFontH + 3;
constexpr int kReplyTop = 126;

// The content pool's cell counts (content_riddles.h) resolved against the face. This is
// the only place the two meet: the pool is authored in columns and drawn in pixels.
constexpr int kRiddleBodyW = kRiddleBodyCols * kFontAdvance;
constexpr int kRiddleReplyIndent = kRiddleReplyIndentCols * kFontAdvance;
constexpr int kRiddleReplyW = kRiddleReplyCols * kFontAdvance;
constexpr int kReplyPitch = 19;

// The Cant strip along the foot: 26 cells, one per letter, on the same 8px grid the
// text uses so a lit cell lines up with the glyph it stands for.
constexpr int kStripCellW = kFontAdvance;
constexpr int kStripW = kCantSigils * kStripCellW;
constexpr int kStripX = (kActiveW - kStripW) / 2;
constexpr int kStripY = kActiveH - kHintBandH - 14;
constexpr int kStripH = 9;

// The answer clock's bar, immediately over the strip.
constexpr int kClockY = kStripY - 6;

// The HAIL and the VERDICT share one shape, because they are the same beat twice: a
// banner naming what is happening, the guardian ITSELF, then its gesture, its words, a
// rule, and two plain lines saying what all that means to the pet. Fixed tops for the
// same reason the riddle board's are — a player who has met a guardian before should find
// every part of the next meeting where they left it.
//
// The body gets the top of the panel and the widest single band on it, which is the whole
// argument for these two screens existing: the riddle board has no room for a creature,
// so before these there was nothing anywhere in the game that showed what a guardian
// looks like.
constexpr int kMeetBannerY = 28;
constexpr int kMeetCellY = 42;      // the swarm, kGuardianCellW x kGuardianCellH
constexpr int kMeetCellX = (kActiveW - kGuardianCellW) / 2;
constexpr int kMeetSeenY = 104;     // the gesture: plain, dim, one line
constexpr int kMeetSaidY = 116;     // what it says: the Cant, up to two lines
constexpr int kMeetLines = 2;
// The fence between the MEETING and what the meeting did to the pet. Everything above it
// is the guardian; everything below it is the ledger, in plain words the pet's owner can
// always read.
constexpr int kMeetRuleY = 144;
constexpr int kMeetNoteY = 152;
constexpr int kMeetNote2Y = 170;
static_assert(kMeetCellY + kGuardianCellH < kMeetSeenY,
              "the swarm's cell must clear the gesture line under it");

void drawCentered(Framebuffer& fb, int y, const char* s, Rgb565 color) {
    if (!s || !s[0]) return;
    drawText(fb, (kActiveW - textWidth(s)) / 2, y, s, color);
}

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

void drawShibboleth(Framebuffer& fb, const char* guardian, const char* demeanour,
                    const char* greeting, const char* riddle,
                    const char* const replies[kRiddleReplies], int cursor,
                    uint32_t sigils, float holdFrac) {
    // The guardian's own name is the title, and it is ACCENT rather than the wild
    // encounter's WARN: this is not an alarm. Something is talking to the pet.
    char count[12];
    std::snprintf(count, sizeof(count), "%d/%d", sigilCount(sigils), kCantSigils);
    drawHeaderBand(fb, guardian ? guardian : "", count, palColor(Pal::INK_DIM),
                   palColor(Pal::ACCENT));

    // What the pet can SEE. The only plain-language thing on the screen, and drawn dim
    // and unquoted so it never reads as speech — it is what the guardian is doing, not
    // what it is saying. Clipped rather than wrapped: the gate holds it to one line.
    if (demeanour && demeanour[0])
        drawTextMarquee(fb, kMargin, kDemeanourY, kRiddleBodyW, demeanour,
                        palColor(Pal::INK_DIM), /*beat=*/0, /*scroll=*/false);

    // What it SAYS, then what it ASKS. Both in the Cant and both in INK, because to the
    // pet they are one utterance — the greeting is not a friendlier register it could
    // have chosen, it is the same language doing the same thing.
    if (greeting && greeting[0])
        drawTextWrapped(fb, kMargin, kGreetingY, kRiddleBodyW, greeting,
                        palColor(Pal::INK), kRiddleLineH, kGreetingLines);

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

namespace {

// The chrome both bracketing screens wear: the guardian's name over its sigil count, the
// banner, the gesture, the speech, and the rule under them. Drawn once here so the hail
// and the verdict cannot drift apart — they are the two halves of one conversation, and
// a player reading the second should recognise the first.
void drawMeetingChrome(Framebuffer& fb, const char* guardian, const char* banner,
                       Rgb565 bannerColor, const char* demeanour, const char* speech,
                       const SwarmView& swarm, Rgb565 swarmCore, uint32_t sigils) {
    char count[12];
    std::snprintf(count, sizeof(count), "%d/%d", sigilCount(sigils), kCantSigils);
    drawHeaderBand(fb, guardian ? guardian : "", count, palColor(Pal::INK_DIM),
                   palColor(Pal::ACCENT));

    drawCentered(fb, kMeetBannerY, banner, bannerColor);

    // THE GUARDIAN. Not a sprite and never going to be one — a flock of marks steering by
    // boids rules (core/render/swarm.h), so the thing across from the pet is visibly not
    // the same kind of thing the pet is. It is drawn before the words because it is what
    // the words are coming out of.
    drawSwarm(fb, swarm, kMeetCellX, kMeetCellY, kGuardianCellW, kGuardianCellH,
              swarmCore, palColor(Pal::INK_DIM));

    // What the pet can SEE, and the only plain-language thing above the rule. Dim and
    // unquoted, so it never reads as speech — it is what the guardian is doing.
    if (demeanour && demeanour[0])
        drawTextWrapped(fb, kMargin, kMeetSeenY, kRiddleBodyW, demeanour,
                        palColor(Pal::INK_DIM), kRiddleLineH, kMeetLines);

    // What it SAYS, in the Cant. Gibberish at no sigils and plain speech at a full set,
    // which is the whole ladder stated on the screen a player sees most often.
    if (speech && speech[0])
        drawTextWrapped(fb, kMargin, kMeetSaidY, kRiddleBodyW, speech,
                        palColor(Pal::INK), kRiddleLineH, kMeetLines);

    fb.fillRect(kMargin, kMeetRuleY, kActiveW - 2 * kMargin, 1, palColor(Pal::TRACK));
}

// The banner word per outcome. A WORD and not a colour, because the colour is the second
// channel here and not the first: a grayscale shot still has to say whether the pet was
// believed, and "IT IS SATISFIED" against "IT IS DISPLEASED" is what says it.
const char* verdictBanner(ShibbolethVerdictKind kind) {
    switch (kind) {
        case ShibbolethVerdictKind::Pleased:    return "IT IS SATISFIED";
        case ShibbolethVerdictKind::Displeased: return "IT IS DISPLEASED";
        case ShibbolethVerdictKind::Refused:    return "IT REFUSES TO ASK";
        case ShibbolethVerdictKind::Boon:       return "IT SPEAKS FREELY";
    }
    return "";
}

}  // namespace

void drawShibbolethHail(Framebuffer& fb, const char* guardian, const char* demeanour,
                        const char* greeting, const SwarmView& swarm, uint32_t sigils,
                        int shakes) {
    // The banner states the one fact the whole system rests on and nothing else on the
    // device says out loud: this thing is talking, and it is not talking the 'net's
    // alphabet. Everything the player is about to fail to read follows from that.
    drawMeetingChrome(fb, guardian, "IT SPEAKS THE CANT", palColor(Pal::ACCENT),
                      demeanour, greeting, swarm, palColor(Pal::ACCENT), sigils);

    // Below the rule: how much of that the pet can read, and what buying more costs. The
    // count is the strip in words — the same fact in the screen's other channel — and the
    // purse line is the only place the shake-for-a-sigil trade is ever explained.
    // Sized for the widest either line can format rather than for the values that
    // actually reach it: the purse is a device-lifetime count, and a buffer picked from
    // today's plausible one is how a readout ends up cut in half on somebody's save.
    char line[48];
    std::snprintf(line, sizeof(line), "YOU READ %d OF %d SIGILS", sigilCount(sigils),
                  kCantSigils);
    drawCentered(fb, kMeetNoteY, line, palColor(Pal::INK));

    if (sigilCount(sigils) >= kCantSigils) {
        drawCentered(fb, kMeetNote2Y, "NOTHING LEFT TO LEARN", palColor(Pal::INK_DIM));
    } else if (shakes > 0) {
        std::snprintf(line, sizeof(line), "SHAKES %d - ONE PER SIGIL", shakes);
        drawCentered(fb, kMeetNote2Y, line, palColor(Pal::ACCENT));
    } else {
        // Says what is missing rather than merely that something is: a shake is captured
        // on the walk, so this is the one line here a player can go and act on.
        drawCentered(fb, kMeetNote2Y, "NO SHAKE TO TRADE", palColor(Pal::INK_DIM));
    }

    drawCantStrip(fb, sigils);
    drawHintBand(fb, "B LISTEN");
}

void drawShibbolethVerdict(Framebuffer& fb, const char* guardian,
                           ShibbolethVerdictKind kind, const char* demeanour,
                           const char* speech, const SwarmView& swarm, const char* ledger,
                           const char* flavor, uint32_t sigils, bool nextIsFight) {
    const bool bad = kind == ShibbolethVerdictKind::Displeased ||
                     kind == ShibbolethVerdictKind::Refused;
    // The body takes the verdict's colour too. A guardian that did not like the answer is
    // flying apart in WARN, which is honest rather than decorative — it IS an alarm now,
    // and the fight is one button away. The scatter says it in grayscale either way.
    drawMeetingChrome(fb, guardian, verdictBanner(kind),
                      palColor(bad ? Pal::WARN : Pal::ACCENT), demeanour, speech, swarm,
                      palColor(bad ? Pal::WARN : Pal::ACCENT), sigils);

    // Below the rule: what it cost or paid, then what that leaves the pet with. The
    // ledger is the numbers and the flavor is the sentence — kept apart because a player
    // who only reads one of the two should still come away with the outcome.
    drawCentered(fb, kMeetNoteY, ledger, palColor(bad ? Pal::WARN : Pal::ACCENT));
    drawCentered(fb, kMeetNote2Y, flavor, palColor(Pal::INK));

    // The strip last, because a sigil bought by this answer is lit in it — the payout and
    // the picture of the payout on the same screen.
    drawCantStrip(fb, sigils);

    // Named for what the button actually does. A displeased guardian answers for itself,
    // and "B CONTINUE" over a boss would be a lie about where the press leads.
    drawHintBand(fb, nextIsFight ? "B FACE IT" : "B CONTINUE");
}

}  // namespace mal
