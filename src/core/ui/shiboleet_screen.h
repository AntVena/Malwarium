// shiboleet_screen.h — the guardian encounter, in three screens.
//
// A meeting is a HAIL, then (in the middle band) the RIDDLE, then a VERDICT — the same
// three beats a conversation has, and the reason the riddle is not the whole encounter:
// something has to arrive before it can ask, and it has to make something of the answer
// afterwards or a lost riddle is just a boss that appeared out of nothing.
//
//   drawShiboleetHail    — the thing is here, this is what it looks like, this is what
//                           it says, and this is how much of that the pet can read.
//   drawShiboleet        — what it asked and the three replies to pick between.
//   drawShiboleetVerdict — what it made of the answer, and what that paid or cost.
//
// All three share a header band, the Cant strip along the foot, and the rule that
// everything SPOKEN arrives already enciphered (Game::shiboleetGreeting /
// shiboleetRiddleText / shiboleetReplyText / shiboleetOutcomeSpeech) while everything
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
#include "core/render/swarm.h"              // SwarmView — the guardian's body is a flock

namespace mal {

class Framebuffer;

// THE GUARDIAN'S CELL: the box its swarm occupies on the two screens that draw the body,
// in active px. Public because the ENGINE resets the flock into it (Game::startShiboleet)
// — a flock is told how big its box is and never where it is, so this is the one number
// the two layers have to agree on, and it is stated here because the screen is what owns
// a layout.
//
// Creature-sized rather than panel-wide on purpose: a swarm spread over the full canvas
// has no density anywhere and reads as static, and density is the only thing making these
// marks a body (core/render/swarm.h).
constexpr int kGuardianCellW = 132;
constexpr int kGuardianCellH = 58;

// How many marks one guardian is made of. Under kSwarmMarksMax, which is the ceiling the
// O(n^2) step can afford — this is the number that fills the cell above at a density that
// still has a middle, and a bigger flock in the same box is a blob.
constexpr int kGuardianSwarmMarks = 26;

// Who is talking, on all three screens.
struct GuardianView {
    const char* name = "";           // the header
    // What the pet can SEE the guardian doing: always plain words, drawn dim as the
    // stage direction it is.
    const char* demeanour = "";
    // The learned-letter bitmask (bit i = 'A'+i reads plain) that the foot strip draws.
    uint32_t sigils = 0;
    // The guardian itself, drawn in the cell above the text on the hail and the verdict.
    const SwarmView* swarm = nullptr;
};

// The SHIBOLEET board's question. `greeting` is what the guardian SAYS, and `riddle` is
// what it then asks; both arrive already enciphered, so both are gibberish to a pet with
// no sigils and plain speech to a fluent one.
//
// That split is the screen's whole argument. A player who cannot read a word of the Cant
// is not staring at nothing: they can see the thing is waiting, or blocking the way, or
// listening, and as sigils come in the words arrive underneath a gesture they already
// understood, which is how anyone picks up a language nobody sat them down to teach.
struct RiddleView {
    const char* greeting = "";
    const char* riddle = "";
    // The kRiddleReplies enciphered choices in the order they are shown.
    const char* const* replies = nullptr;
    int cursor = 0;
    // How much of the answer clock has run, 0..1, drawn as a bar: a guardian that will
    // take silence for an answer has to SHOW that it is running out of patience. It is
    // the only thing on this screen that moves.
    float holdFrac = 0.0f;
};
void drawShiboleet(Framebuffer& fb, const GuardianView& g, const RiddleView& r);

// The HAIL, the beat before the question.
//
// Same header and the same demeanour/greeting pair the riddle board carries, but alone on
// the screen with the guardian's body: a player meeting this for the first time gets one
// beat to look at the thing that stopped the walk before being asked anything by it.
// What the riddle board has no room to say, this does: `shakes` is the unspent purse, and
// the foot states in plain words that a captured handshake is what buys a sigil, which is
// the one rule of this system nothing else on the device explains.
//
// The Cant strip is drawn here too, and that is deliberate: the first thing a new player
// should learn is that the nonsense is FINITE and that the lit cells are the way out of
// it.
void drawShiboleetHail(Framebuffer& fb, const GuardianView& g, const char* greeting,
                       int shakes);

// How a meeting came out, as the verdict screen announces it. The engine folds the
// fluency band and the reply onto this (Game::shiboleetOutcome); the screen needs only
// the WORD to put at the top and whether it is good news, so it keeps its own four rather
// than reaching into the content layer's area rows for them.
enum class ShiboleetVerdictKind { Pleased, Displeased, Refused, Boon };

// Where B leads from the verdict. A displeased guardian answers for itself, and a player
// is never told "B CONTINUE" and handed a boss.
enum class VerdictNext : uint8_t { Continue, Fight };

// The VERDICT: what the guardian made of it. The guardian's body stays on screen, now
// flying under the mood the answer put it in, which is what turns "it was displeased"
// from a line of text into something the player watched happen. The strip is drawn once
// more, which is where a sigil just earned lights its own cell.
struct VerdictView {
    // Picks the banner word (the dual-coding channel: a distinct WORD per outcome, so a
    // grayscale shot still separates being believed from being refused) and its colour.
    ShiboleetVerdictKind kind = ShiboleetVerdictKind::Pleased;
    const char* speech = "";         // what it SAYS, enciphered like every screen here
    // The engine's plain stat line ("+6 HAPPY  -4 FRAG") and the consequence in words
    // ("LEARNED A SIGIL - 3/26", "YOU ANSWERED WRONG"); either may be empty.
    const char* ledger = "";
    const char* flavor = "";
    VerdictNext next = VerdictNext::Continue;
};
void drawShiboleetVerdict(Framebuffer& fb, const GuardianView& g, const VerdictView& v);

}  // namespace mal
