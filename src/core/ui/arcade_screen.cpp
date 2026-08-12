#include "core/ui/arcade_screen.h"

#include <cstdio>

#include "tunables.h"
#include "core/content/registry.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/layout.h"
#include "core/ui/spec_sheet.h"
#include "core/ui/widgets.h"

namespace mal {

namespace {

// The cabinet page's bands, top to bottom: the game's own copy, then the dial, then
// what a run pays, then START. Fixed rather than flowed — every cabinet carries the
// same four things, so the page reads identically whichever one is open, and a longer
// blurb wraps inside its band instead of pushing the payout around.
constexpr int kCabIconY = 28;
constexpr int kCabBlurbY = 30;
constexpr int kCabBlurbBottom = 74;
constexpr int kCabDialY = 84;
constexpr int kCabDialBlurbY = kCabDialY + kLineH;
constexpr int kCabPaysY = 128;
constexpr int kCabBonusY = kCabPaysY + kLineH;
constexpr int kCabStartY = 176;

}  // namespace

void drawArcadeList(Framebuffer& fb, const ContentRegistry& reg, const int* plays,
                    const int* rows, int n, int cursor, int beat) {
    drawHeaderBand(fb, "GAMES");
    for (int v = 0; v < n; ++v) {
        const int i = rows[v];
        const ArcadeGameDef& d = arcadeGames()[i];
        const int y = kRowTop + v * kRowH;
        if (i == cursor) {
            fb.fillRect(4, y + 2, kActiveW - 8, kRowH - 4, palColor(Pal::TRACK));
            drawRowCursor(fb, 8, y + (kRowH - 7) / 2, palColor(Pal::ACCENT));
        }
        if (const SpriteData* ic = reg.sprite(d.iconName))
            drawSprite(fb, *ic, 0, 16, y + (kRowH - kRowIcon) / 2);
        // The play tally owns the right end — it is the row's one changing fact. Kept
        // to three glyphs because the NAME is what the row is for, and an untouched
        // cabinet says so in a word rather than with a zero the eye slides off.
        char tally[16];
        if (plays[i] > 0) std::snprintf(tally, sizeof(tally), "x%d", plays[i]);
        else std::snprintf(tally, sizeof(tally), "NEW");
        const int textY = y + (kRowH - kFontH) / 2;
        const int tallyX = kActiveW - kMargin - textWidth(tally);
        drawText(fb, tallyX, textY, tally, palColor(Pal::INK_DIM));
        drawTextMarquee(fb, 40, textY, tallyX - kMargin - 40, d.displayName,
                        palColor(Pal::INK), beat, i == cursor);
    }
}

void drawArcadeCabinet(Framebuffer& fb, const ContentRegistry& reg,
                       const ArcadeGameDef& def, ArcadeDifficulty difficulty,
                       int plays, int wins) {
    drawHeaderBand(fb, def.displayName);

    // The record sits up in the header's own row, right-aligned — it belongs to the
    // cabinet's identity rather than to any of the four bands below.
    char rec[20];
    std::snprintf(rec, sizeof(rec), "%d-%d", wins, plays - wins);
    drawText(fb, kActiveW - kMargin - textWidth(rec), 6, rec, palColor(Pal::INK_DIM));

    const SpriteData* ic = reg.sprite(def.iconName);
    if (ic) drawSprite(fb, *ic, 0, kMargin, kCabIconY);
    const int blurbX = ic ? kMargin + kRowIcon + 6 : kMargin;
    SpecSheet sheet;
    sheet.prose = def.blurb;
    drawSpecSheet(fb, blurbX, kCabBlurbY, kActiveW - kMargin - blurbX,
                  kCabBlurbBottom, sheet);

    // The dial. Arrows around the word say it cycles without spending a hint-band
    // slot on it, and the word itself is the setting (grayscale-safe).
    char dial[24];
    std::snprintf(dial, sizeof(dial), "< %s >", arcadeDifficultyName(difficulty));
    drawText(fb, kMargin, kCabDialY, "DIFFICULTY:", palColor(Pal::INK_DIM));
    drawText(fb, kMargin + textWidth("DIFFICULTY:") + 8, kCabDialY, dial,
             palColor(Pal::ACCENT));
    drawText(fb, kMargin, kCabDialBlurbY, def.difficultyBlurb, palColor(Pal::INK_DIM));

    // What a run is worth, stated before it starts — the arcade's whole pitch is that
    // finishing pays whatever happens, so the flat half leads and the skill half
    // qualifies itself.
    char pays[36];
    std::snprintf(pays, sizeof(pays), "PAYS %d BITS + %d HAPPY", kArcadePlayBits,
                  kArcadePlayHappy);
    drawText(fb, kMargin, kCabPaysY, pays, palColor(Pal::INK));
    char bonus[40];
    if (def.scoring == ArcadeScoring::Incremental)
        std::snprintf(bonus, sizeof(bonus), "UP TO %d MORE ON SCORE", kArcadeScoreBits);
    else
        std::snprintf(bonus, sizeof(bonus), "%d MORE IF YOU WIN", kArcadeScoreBits);
    drawText(fb, kMargin, kCabBonusY, bonus, palColor(Pal::INK_DIM));

    drawRowCursor(fb, kMargin, kCabStartY, palColor(Pal::ACCENT));
    drawText(fb, kMargin + 12, kCabStartY, "START", palColor(Pal::INK));

    drawHintBand(fb, "A DIFF  B START  C BACK");
}

void drawArcadeResult(Framebuffer& fb, const ArcadeGameDef& def, bool won,
                      int score, int scoreMax, int bits, int happy) {
    fb.clear(palColor(Pal::PAPER));
    drawHeaderBand(fb, def.displayName);

    // The verdict is a WORD, and the payout below it is the number — so the outcome
    // and the reward are two channels rather than one, and a losing run still has
    // something to read that isn't zero.
    const char* verdict = won ? "CLEARED" : "RUN ENDED";
    drawText(fb, (kActiveW - textWidth(verdict, 2)) / 2, 48, verdict,
             won ? palColor(Pal::ACCENT) : palColor(Pal::INK), 2);

    if (scoreMax > 0) {
        char line[24];
        std::snprintf(line, sizeof(line), "SCORE %d / %d", score, scoreMax);
        drawText(fb, (kActiveW - textWidth(line)) / 2, 84, line, palColor(Pal::INK_DIM));
    }

    char bitsLine[24];
    std::snprintf(bitsLine, sizeof(bitsLine), "+%d BITS", bits);
    drawText(fb, (kActiveW - textWidth(bitsLine, 2)) / 2, 116, bitsLine,
             palColor(Pal::ACCENT), 2);
    char happyLine[24];
    std::snprintf(happyLine, sizeof(happyLine), "+%d HAPPINESS", happy);
    drawText(fb, (kActiveW - textWidth(happyLine)) / 2, 148, happyLine,
             palColor(Pal::INK));

    drawHintBand(fb, "B CONTINUE");
}

}  // namespace mal
