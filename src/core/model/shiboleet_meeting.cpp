#include "core/model/shiboleet_meeting.h"

#include "tunables.h"

namespace mal {

namespace {
// A guardian's own seed. Mixed from the walk's position, so the encounter re-renders
// identically while it is on screen: the cipher is built once, in begin, and
// everything after reads it.
uint32_t cipherSeed(uint32_t rng, int area, int steps) {
    return rng ^ (static_cast<uint32_t>(area) << 24) ^ static_cast<uint32_t>(steps);
}

// The guardian's line pair for this meeting, bounds-safe. A row that authored fewer than
// kGuardianLines leaves null cells, and a null `cant`/`seen` would be a blank line rather
// than a crash, but the content gate rejects an under-filled row, so this is only a
// defensive floor.
const GuardianLine& guardianLine(const AreaDef& a, int i) {
    static const GuardianLine kNone{"", ""};
    if (i < 0 || i >= kGuardianLines) return kNone;
    const GuardianLine& l = a.guardian.lines[i];
    return (l.cant && l.seen) ? l : kNone;
}

// The same pair for how the meeting ENDED (GuardianOutcome, area_defs.h), and bounds-safe
// for the same reason: the content gate rejects an under-filled row, so an empty cell here
// draws a blank verdict rather than reading off the end of an area that shipped short.
const GuardianLine& guardianOutcomeLine(const AreaDef& a, GuardianOutcome o) {
    static const GuardianLine kNone{"", ""};
    const int i = static_cast<int>(o);
    if (i < 0 || i >= kGuardianOutcomes) return kNone;
    const GuardianLine& l = a.guardian.outcomes[i];
    return (l.cant && l.seen) ? l : kNone;
}

uint32_t step(uint32_t& rng) { return rng = rng * 1664525u + 1013904223u; }
}  // namespace

void ShiboleetMeeting::begin(uint32_t& rng, SigilSet sigils, int sector, int steps) {
    // The guardian of wherever the pet is standing. The DeepWeb has no AreaDef and so no
    // guardian of its own; nothing routes a dive here, and the clamp is what makes that
    // a dull fallback rather than a read off the end of the ladder.
    area = (sector >= 0 && sector < kAreaCount) ? sector : 0;

    // Grade the welcome first. All three bands start with the same hail, because the
    // pet cannot tell which band it is in until the guardian asks, refuses, or talks.
    //
    // A ROLL against fluency, not a threshold on it. The two edge bands are chances that
    // move in opposite directions as the Cant is learned (refusal thins out, a quiet
    // word thickens), so a pet with no sigils is still mostly asked, which gives the
    // ladder its first rung. See tunables.h.
    const int fluency = cantFluencyPct(sigils);
    const int affront = kShiboleetAffrontBasePct * (100 - fluency) / 100;
    const int boon = kShiboleetBoonMaxPct * fluency / 100;
    const int roll = static_cast<int>((step(rng) >> 16) % 100);
    welcome = (roll < affront)       ? ShiboleetWelcome::Affront
            : (roll >= 100 - boon)   ? ShiboleetWelcome::Boon
                                     : ShiboleetWelcome::Riddle;
    reply = ShiboleetReply::Pending;
    flavor[0] = '\0';
    verdictLine[0] = '\0';

    // Which of this guardian's greeting/demeanour pairs it meets the pet with.
    line = static_cast<int>((step(rng) >> 16) % kGuardianLines);

    if (welcome == ShiboleetWelcome::Riddle) {
        const int n = riddleCount();
        if (n <= 0) {
            // An empty pool has nothing to ask, so the meeting becomes a refusal,
            // which needs no content beyond the guardian.
            welcome = ShiboleetWelcome::Affront;
        } else {
            riddle = static_cast<int>((step(rng) >> 16) % static_cast<unsigned>(n));

            // Shuffle which authored reply sits on which shown row. The pool always
            // authors the true reply first (content_riddles.h), so without this the
            // answer would be row 0 every time, and with it, authoring position carries
            // no information at all.
            for (int i = 0; i < kRiddleReplies; ++i) order[i] = static_cast<uint8_t>(i);
            for (int i = kRiddleReplies - 1; i > 0; --i) {
                const int j = static_cast<int>((step(rng) >> 16) % static_cast<unsigned>(i + 1));
                const uint8_t t = order[i]; order[i] = order[j]; order[j] = t;
            }
        }
    }

    // One cipher for the whole meeting: the greeting on the hail, the riddle and its
    // replies, and whatever the guardian says about the answer at the end are all drawn
    // in it, so a letter the pet cannot read is the same letter on every screen. Built
    // for every band and not just the asking one: a refusal is still spoken in the Cant.
    const uint32_t seed = cipherSeed(rng, sector, steps);
    cipher.build(sigils, seed);

    // The BODY, seeded off the same walk position the cipher is, so a guardian
    // re-renders identically while it is on screen and takes a different shape the next
    // time it is met.
    flock.seed(seed | 1u);
}

GuardianOutcome ShiboleetMeeting::outcome() const {
    if (welcome == ShiboleetWelcome::Affront) return GuardianOutcome::Affront;
    if (welcome == ShiboleetWelcome::Boon) return GuardianOutcome::Boon;
    return reply == ShiboleetReply::Answered ? GuardianOutcome::Pleased
                                             : GuardianOutcome::Displeased;
}

bool ShiboleetMeeting::verdictFights() const {
    const GuardianOutcome o = outcome();
    return o == GuardianOutcome::Displeased || o == GuardianOutcome::Affront;
}

int ShiboleetMeeting::trueRow() const {
    // replies[0] is the true one; find where the shuffle put it.
    for (int i = 0; i < kRiddleReplies; ++i)
        if (order[i] == 0) return i;
    return 0;
}

const char* ShiboleetMeeting::guardianName() const { return mal::area(area).guardian.name; }

void ShiboleetMeeting::greeting(char* out, int cap) const {
    if (!out || cap <= 0) return;
    out[0] = '\0';
    // The SAME cipher the riddle is drawn in, deliberately: a letter the pet cannot read
    // has to be unreadable everywhere on the screen, or the greeting would quietly leak
    // the mapping the riddle is asking it to work without.
    cipher.applyTo(guardianLine(mal::area(area), line).cant, out, cap);
}

const char* ShiboleetMeeting::demeanour() const {
    return guardianLine(mal::area(area), line).seen;
}

void ShiboleetMeeting::riddleText(char* out, int cap) const {
    if (!out || cap <= 0) return;
    out[0] = '\0';
    if (riddle < 0 || riddle >= riddleCount()) return;
    cipher.applyTo(riddles()[riddle].text, out, cap);
}

void ShiboleetMeeting::replyText(int row, char* out, int cap) const {
    if (!out || cap <= 0) return;
    out[0] = '\0';
    if (row < 0 || row >= kRiddleReplies) return;
    if (riddle < 0 || riddle >= riddleCount()) return;
    cipher.applyTo(riddles()[riddle].replies[order[row]], out, cap);
}

void ShiboleetMeeting::outcomeSpeech(char* out, int cap) const {
    if (!out || cap <= 0) return;
    out[0] = '\0';
    // The SAME cipher as the greeting and the riddle. The verdict stays in the Cant, so
    // it is exactly as legible as the question was.
    cipher.applyTo(guardianOutcomeLine(mal::area(area), outcome()).cant, out, cap);
}

const char* ShiboleetMeeting::outcomeSeen() const {
    return guardianOutcomeLine(mal::area(area), outcome()).seen;
}

}  // namespace mal
