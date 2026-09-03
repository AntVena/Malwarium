// shibboleth_screen.h — the guardian encounter, in three screens.
//
// A meeting is a HAIL, then (in the middle band) the RIDDLE, then a VERDICT — the same
// three beats a conversation has, and the reason the riddle is not the whole encounter:
// something has to arrive before it can ask, and it has to make something of the answer
// afterwards or a lost riddle is just a boss that appeared out of nothing.
//
//   drawShibbolethHail    — the thing is here, this is what it looks like, this is what
//                           it says, and this is how much of that the pet can read.
//   drawShibboleth        — what it asked and the three replies to pick between.
//   drawShibbolethVerdict — what it made of the answer, and what that paid or cost.
//
// All three share a header band, the Cant strip along the foot, and the rule that
// everything SPOKEN arrives already enciphered (Game::shibbolethGreeting /
// shibbolethRiddleText / shibbolethReplyText / shibbolethOutcomeSpeech) while everything
// SEEN is plain. Nothing here knows about the cipher, which is what keeps the model
// testable without a framebuffer.
//
// The strip is the point of these screens as much as the riddle is. A player looking at a
// wall of nonsense needs to see that the nonsense is FINITE and that they are eating
// into it: the lit letters are the sigils earned, and every one of them is a letter that
// now reads plainly in the riddle above. It is also the screens' second, non-colour
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

// The HAIL — the beat before the question.
//
// Same `guardian` header, same `demeanour`/`greeting` pair the riddle board carries, but
// alone on the screen: a player meeting this for the first time gets one beat to look at
// the thing that stopped the walk before being asked anything by it. What the riddle
// board has no room to say, this does — `shakes` is the unspent purse, and the foot
// states in plain words that a captured handshake is what buys a sigil, which is the one
// rule of this system nothing else on the device explains.
//
// The Cant strip is drawn here too, and that is deliberate: the first thing a new player
// should learn is that the nonsense is FINITE and that the lit cells are the way out of
// it.
void drawShibbolethHail(Framebuffer& fb, const char* guardian, const char* demeanour,
                        const char* greeting, uint32_t sigils, int shakes);

// How a meeting came out, as the verdict screen announces it. The engine folds the
// fluency band and the reply onto this (Game::shibbolethOutcome); the screen needs only
// the WORD to put at the top and whether it is good news, so it keeps its own four rather
// than reaching into the content layer's area rows for them.
enum class ShibbolethVerdictKind { Pleased, Displeased, Refused, Boon };

// The VERDICT — what the guardian made of it.
//
// `kind` picks the banner word (the dual-coding channel: a distinct WORD per outcome, so
// a grayscale shot still separates being believed from being refused) and its colour.
// `demeanour` is what the pet SEES the guardian do about it and `speech` is what it SAYS,
// the same plain/enciphered pair every screen here draws.
//
// `ledger` is the engine's plain stat line ("+6 HAPPY  -4 FRAG") and `flavor` the
// consequence in words ("LEARNED A SIGIL - 3/26", "YOU ANSWERED WRONG"); either may be
// empty. `sigils` draws the strip once more, which is where a sigil just earned lights
// its own cell — the payout and the picture of it on the same screen.
//
// `nextIsFight` names the button honestly: a displeased guardian answers for itself, and
// a player is never told "B CONTINUE" and handed a boss.
void drawShibbolethVerdict(Framebuffer& fb, const char* guardian,
                           ShibbolethVerdictKind kind, const char* demeanour,
                           const char* speech, const char* ledger, const char* flavor,
                           uint32_t sigils, bool nextIsFight);

}  // namespace mal
