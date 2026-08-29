// shibboleth_screen.h — the guardian's riddle, drawn in the CANT.
//
// One screen: who is asking (the header band), what it asked, three replies to pick
// between, and the strip along the foot showing which letters of the Cant the pet can
// actually read. The engine hands over text that is ALREADY enciphered
// (Game::shibbolethRiddleText / shibbolethReplyText) — nothing here knows about the
// cipher, which is what keeps the model testable without a framebuffer.
//
// The strip is the point of the screen as much as the riddle is. A player looking at a
// wall of nonsense needs to see that the nonsense is FINITE and that they are eating
// into it: the lit letters are the sigils earned, and every one of them is a letter that
// now reads plainly in the riddle above. It is also the screen's second, non-colour
// channel — a grayscale shot still separates a learned letter from an unlearned one by
// its box, not its hue, which is the dual-coding gate.
#pragma once

#include <cstdint>

#include "core/content/content_riddles.h"   // kRiddleReplies — the row count is content's

namespace mal {

class Framebuffer;

// The SHIBBOLETH board.
//
// `guardian` names who is asking (the header). `demeanour` is what the pet can SEE the
// guardian doing — always plain words, drawn dim above everything else as the stage
// direction it is. `greeting` is what the guardian SAYS, and `riddle` is what it then
// asks; both arrive already enciphered, so both are gibberish to a pet with no sigils
// and plain speech to a fluent one.
//
// That split is the screen's whole argument. A player who cannot read a word of the Cant
// is not staring at nothing: they can see the thing is waiting, or blocking the way, or
// listening — and as sigils come in the words arrive underneath a gesture they already
// understood, which is how anyone picks up a language nobody sat them down to teach.
//
// `replies` are the kRiddleReplies enciphered choices in the order they are shown,
// `cursor` is the focused row. `sigils` is the learned-letter bitmask (bit i = 'A'+i
// reads plain) that the foot strip draws.
//
// `holdFrac` is how much of the answer clock has run, 0..1 — drawn as a bar, because a
// guardian that will take silence for an answer has to SHOW that it is running out of
// patience. It is the only thing on this screen that moves.
void drawShibboleth(Framebuffer& fb, const char* guardian, const char* demeanour,
                    const char* greeting, const char* riddle,
                    const char* const replies[kRiddleReplies], int cursor,
                    uint32_t sigils, float holdFrac);

}  // namespace mal
