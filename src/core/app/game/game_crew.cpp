#include "core/app/game.h"

#include <algorithm>
#include <cstring>

#include "core/content/content_crews.h"

// game_crew.cpp: crew membership and the HOME NETWORK it hangs off (content_crews.h),
// and the in-range rows the CREW screen's picker offers. The screen itself is
// CrewScreen (game/crew_screen.h).
//
// Membership's payoff in play is the crew's signature Exploit: crewExploitOption()
// hands it to Combat as the A+C picker's last row (Game::openExploitPicker -> openOverride).

namespace mal {


// --- Membership + home network --------------------------------------------

void Game::setHomeNetwork(uint64_t key, const char* name) {
    op_.homeNetworkKey = key;
    std::strncpy(op_.homeNetworkName, name ? name : "", sizeof(op_.homeNetworkName) - 1);
    op_.homeNetworkName[sizeof(op_.homeNetworkName) - 1] = '\0';
    if (key == 0) {
        op_.homeNetworkName[0] = '\0';
        op_.crewIndex = -1;   // membership can't outlive the network it defends
    }
    markSaveDirty();
    dirty_ = true;
}

bool Game::joinCrew(int index) {
    if (!crew(index)) return false;
    if (!hasHomeNetwork()) return false;   // a crew member is always somebody's defender
    if (op_.crewIndex == index) return true;
    op_.crewIndex = index;
    // Taking a side at all, once. Fired on the first join. A player belongs to at most
    // one crew and can switch freely, so there is nothing to count.
    unlockAchievement(ach::kCrewEnlisted);
    log_.push(LogEventType::ItemGained, kCrews[index].displayName);
    markSaveDirty();
    dirty_ = true;
    return true;
}

void Game::leaveCrew() {
    if (op_.crewIndex < 0) return;
    op_.crewIndex = -1;
    markSaveDirty();
    dirty_ = true;
}

CrewExploit Game::crewExploitOption() const {
    const CrewDef* c = activeCrew();
    if (!c || c->exploit.kind == CrewExploitKind::None) return {};
    return {c->exploit.name, c->exploit.kind, c->exploit.magnitude};
}

void Game::rememberNetwork(uint64_t key, const char* name, int count) {
    networkLedger_.recordNew(key, name);
    for (int i = 1; i < count; ++i) networkLedger_.recordSeenAgain(key, name);
    dirty_ = true;
}

int Game::crewNetworkRows(CrewNetRow* rows, int max) const {
    int n = 0;
    // Exactly one source of rows: what the radio can hear right now. The reward queue
    // (pendingNetworks_) is not consulted. It says nothing about which networks are
    // in range, and a full backlog would hide the network the operator is standing
    // next to.
    for (int i = 0; i < visibleNetworkCount_ && n < max; ++i) {
        const VisibleNetwork& v = visibleNetworks_[i];
        if (!networkVisible(v)) continue;
        // The ledger contributes evidence, never an offer: how many times this pet has
        // walked this network in, which is what tells the familiar one apart from the
        // six neighbours' APs sitting next to it in the list.
        NetworkLedger::Entry known;
        const uint32_t count = networkLedger_.lookup(v.key, &known) ? known.count : 0;
        rows[n++] = {v.key, v.name, count};
    }
    // Most-walked first, as the best guess at which network is yours. The operator still
    // chooses. Stable within a tie, so the order stays steady between repaints instead of
    // shuffling under the cursor.
    std::stable_sort(rows, rows + n, [](const CrewNetRow& a, const CrewNetRow& b) {
        return a.count > b.count;
    });
    return n;
}

}  // namespace mal
