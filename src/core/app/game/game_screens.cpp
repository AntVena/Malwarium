// game_screens.cpp: the per-screen table. For each Nav, in Nav's order, what a press does
// on that screen, what render draws, and what the heartbeat steps (Game::ScreenRow).
//
// onButton, render and stepScreenBeat each look their row up here, so a screen joins the
// game by gaining one row, and the three can never disagree about which screen is up.
// The menu tree's two levels keep their per-slot dispatch (onSubmenu / onDetail,
// drawSubmenu / drawDetail), since several slots share one Nav.
#include "core/app/game.h"

#include <iterator>

namespace mal {

template <auto Screen>
void Game::pressScreen(Game& g, const ButtonEvent& ev) {
    (g.*Screen).onButton(g, ev);
}

template <auto Screen>
void Game::drawScreen(const Game& g, Framebuffer& fb) {
    (g.*Screen).draw(fb, g);
}

namespace {

// Rows in Nav's order, so the row for a Nav is found by its value. Checked below.
template <typename Row, size_t N>
constexpr bool inNavOrder(const Row (&rows)[N]) {
    for (size_t i = 0; i < N; ++i)
        if (static_cast<size_t>(rows[i].nav) != i) return false;
    return true;
}

}  // namespace

const Game::ScreenRow& Game::screenRow(Nav n) {
    using G = Game;
    using Ev = ButtonEvent;
    using Fb = Framebuffer;
    static constexpr ScreenRow kScreens[] = {
        {Nav::Idle, [](G& g, const Ev& e) { g.onIdle(e); },
         [](const G& g, Fb& fb) { g.drawHome(fb, -1); },
         [](G& g) { g.stepHomeBeat(); }},
        {Nav::Cursor, [](G& g, const Ev& e) { g.onCursor(e); },
         [](const G& g, Fb& fb) { g.drawHome(fb, g.cursor_); }, nullptr},
        {Nav::Submenu, [](G& g, const Ev& e) { g.onSubmenu(e); },
         [](const G& g, Fb& fb) { g.drawSubmenu(fb); }, nullptr},
        {Nav::Detail, [](G& g, const Ev& e) { g.onDetail(e); },
         [](const G& g, Fb& fb) { g.drawDetail(fb); }, nullptr},
        {Nav::Process, [](G& g, const Ev& e) { g.maintScreen_.onProcess(g, e); },
         [](const G& g, Fb& fb) { g.maintScreen_.drawProcess(fb, g); },
         [](G& g) { g.stepProcessBeat(); }},
        {Nav::ModalFeeding, pressScreen<&G::feedingScreen_>, drawScreen<&G::feedingScreen_>,
         [](G& g) { g.stepFeedingBeat(); }},
        {Nav::ModalLockout, pressScreen<&G::lockoutScreen_>, drawScreen<&G::lockoutScreen_>,
         nullptr},
        {Nav::ModalLineSelect, pressScreen<&G::lineSelect_>, drawScreen<&G::lineSelect_>,
         nullptr},
        {Nav::ModalEggPick, pressScreen<&G::eggPickScreen_>, drawScreen<&G::eggPickScreen_>,
         nullptr},
        // Non-interactive: the shell's one-shot walks a frame per heartbeat, holds on the
        // last one, then hatches itself.
        {Nav::ModalHatchReveal, pressScreen<&G::hatchReveal_>, drawScreen<&G::hatchReveal_>,
         [](G& g) { g.hatchReveal_.tick(g); }},
        // Hold, flash, reveal on the heartbeat, then it parks at the reveal and waits for B
        // (completeEvolution). C does nothing: the evolution already happened.
        {Nav::ModalEvolve, pressScreen<&G::evolve_>, drawScreen<&G::evolve_>,
         [](G& g) { g.evolve_.tick(); }},
        // The crash FX holds a few beats, then parks and waits for B. Death is not
        // cancellable, so C does nothing.
        {Nav::ModalCSF, pressScreen<&G::csf_>, drawScreen<&G::csf_>,
         [](G& g) { g.csf_.tick(); }},
        {Nav::Combat, pressScreen<&G::fightScreen_>, drawScreen<&G::fightScreen_>,
         [](G& g) { g.stepCombatBeat(); }},
        {Nav::ExploreControl, pressScreen<&G::exploreControl_>,
         drawScreen<&G::exploreControl_>, nullptr},
        {Nav::Encounter, pressScreen<&G::encounter_>, drawScreen<&G::encounter_>, nullptr},
        // Hands-off REVEAL: the Wi-Fi outcome is a rolled reveal, not a choice. Held ~3s
        // so it reads, then played out (a guardian awakened enters a fight; the rest
        // resolve to the habitat).
        {Nav::Wifi, pressScreen<&G::wifiScreen_>, drawScreen<&G::wifiScreen_>,
         [](G& g) { g.stepEventHold(kExploreRevealHoldBeats, &G::resolveWifiOutcome); }},
        // Hands-off DECISION: a shop (item or mod) is a real buy/leave choice, so it holds
        // ~10s for a watching player to act, then leaves and keeps exploring.
        {Nav::Shop, pressScreen<&G::shop_>, drawScreen<&G::shop_>,
         [](G& g) { g.stepEventHold(kExploreDecisionHoldBeats, &G::leaveShop); }},
        {Nav::ModShop, pressScreen<&G::shop_>, drawScreen<&G::shop_>,
         [](G& g) { g.stepEventHold(kExploreDecisionHoldBeats, &G::leaveShop); }},
        {Nav::WarpPicker, pressScreen<&G::warpPicker_>, drawScreen<&G::warpPicker_>, nullptr},
        {Nav::RollbackPicker, [](G& g, const Ev& e) { g.statPicker_.onRollback(g, e); },
         [](const G& g, Fb& fb) { g.statPicker_.drawRollback(fb, g); }, nullptr},
        {Nav::RepartitionPicker, [](G& g, const Ev& e) { g.statPicker_.onRepartition(g, e); },
         [](const G& g, Fb& fb) { g.statPicker_.drawRepartition(fb, g); }, nullptr},
        {Nav::CacheYield, [](G& g, const Ev& e) { g.vault_.onReveal(g, e); },
         [](const G& g, Fb& fb) { g.vault_.drawReveal(fb, g); }, nullptr},
        {Nav::BulkYield, [](G& g, const Ev& e) { g.vault_.onBulkReveal(g, e); },
         [](const G& g, Fb& fb) { g.vault_.drawBulkReveal(fb, g); }, nullptr},
        {Nav::PostEncounter, pressScreen<&G::postEncounter_>, drawScreen<&G::postEncounter_>,
         nullptr},
        // The played boards step on their own faster cadences (tickAnimClocks).
        {Nav::Stacker, pressScreen<&G::stackerScreen_>, drawScreen<&G::stackerScreen_>,
         nullptr},
        {Nav::Fragger, pressScreen<&G::fraggerScreen_>, drawScreen<&G::fraggerScreen_>,
         nullptr},
        {Nav::Isolation, pressScreen<&G::isolationScreen_>, drawScreen<&G::isolationScreen_>,
         nullptr},
        {Nav::Chroma, pressScreen<&G::chromaScreen_>, drawScreen<&G::chromaScreen_>, nullptr},
        {Nav::Decryption, pressScreen<&G::decryptionScreen_>,
         drawScreen<&G::decryptionScreen_>, nullptr},
        {Nav::Cryptogram, pressScreen<&G::cryptogramScreen_>,
         drawScreen<&G::cryptogramScreen_>, nullptr},
        // The arcade payout is informational, so ANY button dismisses it (the
        // PostEncounter contract, not the standard A/B/C one).
        {Nav::ArcadeResult, [](G& g, const Ev& e) { g.arcadeScreen_.onResult(g, e); },
         [](const G& g, Fb& fb) { g.arcadeScreen_.drawResult(fb, g); }, nullptr},
        {Nav::Tourney, pressScreen<&G::arena_>, drawScreen<&G::arena_>, nullptr},
        // Hands-off REVEAL: the guardian arriving and speaking is to watch, so it holds
        // ~5s, then the meeting goes on to whatever the fluency roll decided.
        {Nav::ShiboleetHail, [](G& g, const Ev& e) { g.shiboleet_.onHail(g, e); },
         [](const G& g, Fb& fb) { g.shiboleet_.drawHail(fb, g); },
         [](G& g) { g.stepEventHold(kShiboleetHailHoldBeats, &G::openShiboleetWelcome); }},
        // Hands-off DECISION: silence past the hold is an answer, the wrong one. A restarts
        // the hold (ShiboleetScreen::onRiddle), so a player still reading is not timed out.
        {Nav::Shiboleet, [](G& g, const Ev& e) { g.shiboleet_.onRiddle(g, e); },
         [](const G& g, Fb& fb) { g.shiboleet_.drawRiddle(fb, g); },
         [](G& g) { g.stepEventHold(kShiboleetReplyHoldBeats, &G::leaveShiboleetUnanswered); }},
        // Hands-off REVEAL: what the guardian made of the answer, held ~6s, then played
        // out. The consequence is already paid; the hold only sets how long it is read.
        {Nav::ShiboleetVerdict, [](G& g, const Ev& e) { g.shiboleet_.onVerdict(g, e); },
         [](const G& g, Fb& fb) { g.shiboleet_.drawVerdict(fb, g); },
         [](G& g) { g.stepEventHold(kShiboleetVerdictHoldBeats, &G::finishShiboleet); }},
        {Nav::Story, pressScreen<&G::storyReader_>, drawScreen<&G::storyReader_>, nullptr},
        {Nav::StoryArchive, pressScreen<&G::chapters_>, drawScreen<&G::chapters_>, nullptr},
        {Nav::TipCard, pressScreen<&G::tipCardScreen_>, drawScreen<&G::tipCardScreen_>,
         nullptr},
        {Nav::AwayDigest, pressScreen<&G::awayDigest_>, drawScreen<&G::awayDigest_>, nullptr},
        {Nav::ItemOffer, pressScreen<&G::itemOffer_>, drawScreen<&G::itemOffer_>, nullptr},
    };
    static_assert(std::size(kScreens) == static_cast<size_t>(Nav::ItemOffer) + 1,
                  "one row per Nav");
    static_assert(inNavOrder(kScreens), "rows in Nav's order");
    return kScreens[static_cast<size_t>(n)];
}

}  // namespace mal
