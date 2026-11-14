// shiboleet_meeting.h: one guardian meeting (THE SHIBOLEET), as a run.
//
// A meeting is rolled when a guardian speaks and holds everything about that one
// encounter: the band the fluency roll landed in, the riddle and the order its replies
// are shown in, the cipher every line of it is drawn in, how the pet replied, what that
// paid or cost in words, and the flock that is the guardian's body. The next guardian
// rolls it all again, which is why none of it is persisted: a cipher that survived a
// reboot would be a mapping worth memorising instead of learning.
//
// The rules of the tongue are core/model/cant.h and the words are content
// (content_riddles.h, the area's GuardianDef). The payout lands on the pet and the
// operator, so it is the game's (game_shiboleet.cpp), which writes what it paid here
// for the verdict to read.
#pragma once

#include <cstdint>

#include "core/content/areas/area_defs.h"   // GuardianOutcome
#include "core/content/content_riddles.h"   // kRiddleReplies
#include "core/model/cant.h"
#include "core/model/flock.h"

namespace mal {

// How a guardian receives the pet, graded on how much of the CANT it can read
// (cantFluencyPct, core/model/cant.h).
//
//   Affront: it will not hear an illiterate pet out, and attacks.
//   Riddle:  the full SHIBOLEET, a riddle drawn in the Cant with three replies.
//   Boon:    fluent enough that the two simply talk; no riddle at all.
enum class ShiboleetWelcome : uint8_t { Affront, Riddle, Boon };

// How a riddle ENDED. Answered is the only one that can buy a sigil; the other two
// both hand the pet to the guardian's fight, and are distinguished only so the
// flavor line can tell the truth about which happened.
enum class ShiboleetReply : uint8_t { Pending, Answered, Wrong, Unanswered };

struct ShiboleetMeeting {
    ShiboleetWelcome welcome = ShiboleetWelcome::Riddle;
    ShiboleetReply reply = ShiboleetReply::Pending;
    int area = 0;     // the AreaDef whose guardian this is
    int riddle = 0;   // index into riddles() (content_riddles.h)
    // Which authored reply sits on each shown row. The pool authors the TRUE reply first
    // (content_riddles.h), so the order shown has to be shuffled somewhere; doing it here
    // rather than in the pool is what keeps "add a riddle" a one-line edit.
    uint8_t order[kRiddleReplies] = {0, 1, 2};
    // Which of the guardian's greeting/demeanour pairs this meeting used
    // (GuardianLine, area_defs.h). Rolled per encounter, so a guardian met twice is not
    // word-for-word the same meeting.
    int line = 0;
    CantCipher cipher;
    // What the meeting came to, in words, for the VERDICT screen and for the walk's line
    // after it. `flavor` is the consequence ("LEARNED A SIGIL - 3/26", "IS NOT
    // ANSWERED"); `verdictLine` is the stat ledger beside it ("+6 HAPPY  -4 FRAG").
    // Both are written where the payout happens, so the screen only ever reads them.
    char flavor[40] = "";
    char verdictLine[40] = "";
    // The swarm the guardian IS. A flock is reset per encounter, so nothing about the
    // body a player watched is persisted. It is the same creature next time and never
    // the same shape. It stays this meeting's into the guardian's fight, where the
    // fight re-homes it into the fighter's seat.
    Flock flock;

    // Roll the meeting a guardian at walk position (`sector`, `steps`) opens with a pet
    // that reads `sigils`. `rng` is the game's shared draw, stepped here. The flock is
    // seeded but not sized: its box is whichever screen draws it.
    void begin(uint32_t& rng, SigilSet sigils, int sector, int steps);

    // The fluency band and the reply folded onto the ONE axis the content is authored
    // against. A riddle that was never reached keeps its band's outcome, which is what
    // makes an affront and a boon expressible as things the guardian DID.
    GuardianOutcome outcome() const;
    // The two outcomes the guardian answers itself, with its fight.
    bool verdictFights() const;
    int trueRow() const;   // which shown row carries the true reply

    // The meeting's words. What it SAYS is drawn in the cipher; what the pet can SEE it
    // doing is always plain, so a pet with no sigils still learns something.
    const char* guardianName() const;
    void greeting(char* out, int cap) const;
    const char* demeanour() const;
    void riddleText(char* out, int cap) const;
    void replyText(int row, char* out, int cap) const;
    void outcomeSpeech(char* out, int cap) const;
    const char* outcomeSeen() const;
};

}  // namespace mal
