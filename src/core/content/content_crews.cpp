// content_crews.cpp — the crew table. One row per enlistable crew; see
// content_crews.h for the field schema and how a new Exploit kind is added.

#include "core/content/content_crews.h"

#include <cstdio>
#include <cstring>

namespace mal {

const CrewDef kCrews[] = {
    // The first Blue crew, open to every operator from the start (no unlock beyond
    // designating a home network). Its Exploit is the crew's whole personality: three
    // incoming attacks bounce off outright before real damage resumes.
    {"deniers_of_service", "DENIERS OF SERVICE", "cannot touch: permission denied",
     CrewTeam::Blue,
     {"DENIAL OF SERVICE", CrewExploitKind::NegateNextHits, 3,
      "The next {mag} attacks on you are refused outright - the damage, and the stun "
      "or rot riding on it."}},
    // Red, all gas: three swings each bank their own damage as Power, so the crew's
    // fourth hit arrives carrying everything the first three cost the other side.
    {"shell_smashers", "SHELL SMASHERS", "overkill? more like over-skilled",
     CrewTeam::Red,
     {"ESCALATION", CrewExploitKind::PowerByDamageDealt, 3,
      "Your next {mag} landed hits each bank their own damage as Power for the rest "
      "of the fight. Every one pays for the next."}},
    // Blue's answer to everything that erodes a pet rather than hitting it — the
    // Phishing siphons, a Trojan trap's armour rot: be exactly the pet you brought,
    // and stay it.
    {"injection_protection", "INJECTION PROTECTION", "know thyself",
     CrewTeam::Blue,
     {"NET NEUTRALITY", CrewExploitKind::ResetStatsAndFloor, 0,
      "Snaps your Power, speed and defence back to what you walked in with - and "
      "nothing may lower them again. Cuts both ways."}},
    // Red, sitting in the middle of someone else's session: whatever the malbeast
    // braces or stacks for itself, it braces and stacks for you as well.
    {"syntax_errorist", "SYNTAX ERRORIST",
     "sso stands for 'send it straight over', right?",
     CrewTeam::Red,
     {"MALBEAST IN THE MIDDLE", CrewExploitKind::MirrorEnemyBuffs, 0,
      "Every brace, shield and stack the malbeast puts on itself is copied onto you "
      "as it lands, for the rest of the fight."}},
    // Blue, played before the trouble rather than during it: armed early it is three
    // turns of insurance, and the blow that would have ended the fight is the one that
    // pays for the rest of it.
    {"last_responders", "THE LAST RESPONDERS", "fail to plan; plan to fail",
     CrewTeam::Blue,
     {"BACKUP PLAN B", CrewExploitKind::DeathSaveRally, 3,
      "For {mag} of the malbeast's turns, a killing blow instead leaves you on half "
      "Health and pays the overkill back as Power."}},
    // Blue, and the crew that turned up with a spare in the bag: the next casts run
    // twice, so whatever the pet's line does off a cast simply happens again.
    {"hot_spares", "HOT SPARES", "we brought two",
     CrewTeam::Blue,
     {"FAILOVER", CrewExploitKind::SpareFailover, 2,
      "Your next {mag} casts each resolve a second time, on the spot - traps, copies "
      "and re-rolls included."}},
    // Red, charging rent on every exchange: each hit that lands hands part of itself
    // straight back, for as long as the arrangement holds.
    {"shakedown_artists", "SHAKEDOWN ARTISTS", "nice uptime you've got there",
     CrewTeam::Red,
     {"PROTECTION RACKET", CrewExploitKind::LeechOnHit, 3,
      "For {mag} of your turns, every hit you land takes a cut of its own damage back "
      "as Health."}},
};
const int kCrewCount = static_cast<int>(sizeof(kCrews) / sizeof(kCrews[0]));

const char* crewTeamName(CrewTeam team) {
    return team == CrewTeam::Red ? "RED" : "BLUE";
}

const char* crewTeamDoctrine(CrewTeam team) {
    return team == CrewTeam::Red ? "OPERATORS" : "GUARDIANS";
}

int crewTeamCount(CrewTeam team) {
    int n = 0;
    for (int i = 0; i < kCrewCount; ++i)
        if (kCrews[i].team == team) ++n;
    return n;
}

int crewByTeam(CrewTeam team, int nth) {
    if (nth < 0) return -1;
    for (int i = 0; i < kCrewCount; ++i)
        if (kCrews[i].team == team && nth-- == 0) return i;
    return -1;
}

const char* crewExploitTag(CrewExploitKind kind) {
    switch (kind) {
        case CrewExploitKind::NegateNextHits:     return "NEGATE";
        case CrewExploitKind::PowerByDamageDealt: return "ESCALATE";
        case CrewExploitKind::ResetStatsAndFloor: return "STAT LOCK";
        // Not "MIRROR": that word already belongs to the RAID Mirror's full negation
        // (Combatant::mirrorFired), and two mechanics answering to it on the same
        // screen would be worse than a longer tag.
        case CrewExploitKind::MirrorEnemyBuffs:   return "BUFF COPY";
        case CrewExploitKind::DeathSaveRally:     return "RALLY";
        // A TAKING word, not a paying one: the tag names what the holder does, and the
        // house rakes a share of every pot the way this rakes a share of every hit.
        case CrewExploitKind::LeechOnHit:         return "RAKE";
        case CrewExploitKind::SpareFailover:      return "SPARE";
        case CrewExploitKind::None:               break;
    }
    return "";
}

bool crewExploitIsSticky(CrewExploitKind kind) {
    switch (kind) {
        case CrewExploitKind::ResetStatsAndFloor:
        case CrewExploitKind::MirrorEnemyBuffs:
            return true;
        case CrewExploitKind::NegateNextHits:
        case CrewExploitKind::PowerByDamageDealt:
        case CrewExploitKind::DeathSaveRally:
        case CrewExploitKind::LeechOnHit:
        case CrewExploitKind::SpareFailover:
        case CrewExploitKind::None:
            break;
    }
    return false;
}

void crewExploitLabel(char* out, int cap, CrewExploitKind kind, int count) {
    if (!out || cap <= 0) return;
    const char* tag = crewExploitTag(kind);
    if (count > 0) std::snprintf(out, static_cast<size_t>(cap), "%s x%d", tag, count);
    else           std::snprintf(out, static_cast<size_t>(cap), "%s", tag);
}

const CrewDef* crew(int index) {
    return (index >= 0 && index < kCrewCount) ? &kCrews[index] : nullptr;
}

int crewIndexById(const char* id) {
    if (!id || id[0] == '\0') return -1;
    for (int i = 0; i < kCrewCount; ++i)
        if (std::strcmp(kCrews[i].id, id) == 0) return i;
    return -1;
}

}  // namespace mal
