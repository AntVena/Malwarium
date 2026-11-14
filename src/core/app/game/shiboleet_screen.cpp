#include "core/app/game/shiboleet_screen.h"

#include "core/model/shiboleet_meeting.h"
#include "core/ui/shiboleet_screen.h"
#include "tunables.h"

namespace mal {

void ShiboleetScreen::onHail(ShiboleetHost& host, const ButtonEvent& ev) {
    // A REVEAL, like the Wi-Fi event's: B plays it forward. There is nothing to cancel:
    // the meeting has already happened.
    host.restartEventHold();
    if (ev.button == Button::B) host.openShiboleetWelcome();
}

void ShiboleetScreen::onRiddle(ShiboleetHost& host, const ButtonEvent& ev) {
    // A cursor list, the same contract every other picker keeps: A steps the row, B
    // commits it, C backs out. C does not leave the encounter. Backing away from a
    // question counts as a wrong answer, so C resolves the riddle as unanswered.
    if (ev.button == Button::A) {
        row_ = (row_ + 1) % kRiddleReplies;
        host.restartEventHold();   // a player who is reading is not being rushed
        return;
    }
    if (ev.button == Button::B) { host.answerShiboleet(row_); return; }
    if (ev.button == Button::C) { host.leaveShiboleetUnanswered(); return; }
}

void ShiboleetScreen::onVerdict(ShiboleetHost& host, const ButtonEvent& ev) {
    // A REVEAL again: B plays out what the guardian decided. Nothing here asks for
    // input; the reply already decided it.
    host.restartEventHold();
    if (ev.button == Button::B) host.finishShiboleet();
}

void ShiboleetScreen::drawHail(Framebuffer& fb, const ShiboleetHost& host) const {
    const ShiboleetMeeting& m = host.shiboleetMeeting();
    char greeting[kRiddleBodyCols * 2 + 8];
    m.greeting(greeting, sizeof(greeting));
    const SwarmView swarm = host.guardianSwarm();
    GuardianView g;
    g.name = m.guardianName();
    g.demeanour = m.demeanour();
    g.sigils = host.cantSigils();
    g.swarm = &swarm;
    drawShiboleetHail(fb, g, greeting, host.shakesUnspent());
}

void ShiboleetScreen::drawRiddle(Framebuffer& fb, const ShiboleetHost& host) const {
    const ShiboleetMeeting& m = host.shiboleetMeeting();
    char riddle[kRiddleBodyLines * kRiddleBodyCols + 16];
    m.riddleText(riddle, sizeof(riddle));
    char reply[kRiddleReplies][40];
    const char* rows[kRiddleReplies];
    for (int i = 0; i < kRiddleReplies; ++i) {
        m.replyText(i, reply[i], sizeof(reply[i]));
        rows[i] = reply[i];
    }
    char greeting[kRiddleBodyCols * 2 + 8];
    m.greeting(greeting, sizeof(greeting));
    RiddleView r;
    r.greeting = greeting;
    r.riddle = riddle;
    r.replies = rows;
    r.cursor = row_;
    r.holdFrac = static_cast<float>(host.eventHoldBeat()) / kShiboleetReplyHoldBeats;
    GuardianView g;
    g.name = m.guardianName();
    g.demeanour = m.demeanour();
    g.sigils = host.cantSigils();
    drawShiboleet(fb, g, r);
}

void ShiboleetScreen::drawVerdict(Framebuffer& fb, const ShiboleetHost& host) const {
    const ShiboleetMeeting& m = host.shiboleetMeeting();
    // The content layer's outcome mapped onto the screen's own four. The two enums
    // stay separate: an area row is authored against GuardianOutcome, and the renderer
    // needs only the banner word and whether it is bad news.
    ShiboleetVerdictKind kind = ShiboleetVerdictKind::Pleased;
    switch (m.outcome()) {
        case GuardianOutcome::Pleased:    kind = ShiboleetVerdictKind::Pleased; break;
        case GuardianOutcome::Displeased: kind = ShiboleetVerdictKind::Displeased; break;
        case GuardianOutcome::Affront:    kind = ShiboleetVerdictKind::Refused; break;
        case GuardianOutcome::Boon:       kind = ShiboleetVerdictKind::Boon; break;
    }
    char speech[kRiddleBodyCols * 2 + 8];
    m.outcomeSpeech(speech, sizeof(speech));
    VerdictView v;
    v.kind = kind;
    v.speech = speech;
    v.ledger = m.verdictLine;
    v.flavor = m.flavor;
    v.next = m.verdictFights() ? VerdictNext::Fight : VerdictNext::Continue;
    // The verdict shows what the guardian made of the answer, not its standing demeanour.
    const SwarmView swarm = host.guardianSwarm();
    GuardianView g;
    g.name = m.guardianName();
    g.demeanour = m.outcomeSeen();
    g.sigils = host.cantSigils();
    g.swarm = &swarm;
    drawShiboleetVerdict(fb, g, v);
}

}  // namespace mal
