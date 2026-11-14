// game_screens.cpp: the per-screen table. For each Nav, in Nav's order, what a press does
// on that screen, what render draws, what the heartbeat steps, what the A+C chord means
// there, and what the tick runs, holds or waits for while it is up: the dissolve clock,
// the egg's incubation, the idle timer and a Lockout crisis (Game::ScreenRow).
//
// onButton, render and stepScreenBeat each look their row up here, so a screen joins the
// game by gaining one row, and they can never disagree about which screen is up.
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
    // The played boards use the chord for nothing else, so it toggles their RULES page.
    // The egg-hatch entrants (Decrypt/Clutch/Isolation/Chroma land on their Nav straight
    // out of layEgg with no menu stop) have no other route to it at all, and the
    // CHROMATOPHORE needs it most: its three buttons are all spoken for, so the chord is
    // the only press left that can explain them.
    static constexpr auto rulesChord = [](G& g, const Ev&) { g.toggleGameBrief(); };
    static constexpr auto homeChord = [](G& g, const Ev&) { g.onHomeChord(); };
    static constexpr ScreenRow kScreens[] = {
        {Nav::Idle, [](G& g, const Ev& e) { g.onIdle(e); },
         [](const G& g, Fb& fb) { g.drawHome(fb, -1); },
         [](G& g) { g.stepHomeBeat(); }, homeChord},
        // Every other screen holds the idle timer (MenuIdle's default): it is read, played,
        // decided on or waited in front of. The cursor, the pickers and the short notices
        // collapse back to the habitat after kAutoDefocusMs, and the menu tree's two
        // levels let the entered slot answer (Game::slotIdle).
        {Nav::Cursor, [](G& g, const Ev& e) { g.onCursor(e); },
         [](const G& g, Fb& fb) { g.drawHome(fb, g.cursor_); }, nullptr, homeChord,
         FxSweep::None, Incubation::Runs, MenuIdle::Collapses},
        {Nav::Submenu, [](G& g, const Ev& e) { g.onSubmenu(e); },
         [](const G& g, Fb& fb) { g.drawSubmenu(fb); }, nullptr, nullptr,
         FxSweep::None, Incubation::Runs, MenuIdle::BySlot},
        {Nav::Detail, [](G& g, const Ev& e) { g.onDetail(e); },
         [](const G& g, Fb& fb) { g.drawDetail(fb); }, nullptr, nullptr,
         FxSweep::None, Incubation::Runs, MenuIdle::BySlot},
        {Nav::Process, [](G& g, const Ev& e) { g.maintScreen_.onProcess(g, e); },
         [](const G& g, Fb& fb) { g.maintScreen_.drawProcess(fb, g); },
         [](G& g) { g.stepProcessBeat(); }, nullptr},
        {Nav::ModalFeeding, pressScreen<&G::feedingScreen_>, drawScreen<&G::feedingScreen_>,
         [](G& g) { g.stepFeedingBeat(); }, nullptr, FxSweep::Dissolve},
        {Nav::ModalLockout, pressScreen<&G::lockoutScreen_>, drawScreen<&G::lockoutScreen_>,
         nullptr, nullptr, FxSweep::None, Incubation::Runs, MenuIdle::Collapses},
        {Nav::ModalLineSelect, pressScreen<&G::lineSelect_>, drawScreen<&G::lineSelect_>,
         nullptr, nullptr},
        {Nav::ModalEggPick, pressScreen<&G::eggPickScreen_>, drawScreen<&G::eggPickScreen_>,
         nullptr, rulesChord},
        // Non-interactive: the shell's one-shot walks a frame per heartbeat, holds on the
        // last one, then hatches itself. It can be opened with seconds left on the egg, so
        // the egg's clock holds, or running out underneath would hatch mid-animation.
        {Nav::ModalHatchReveal, pressScreen<&G::hatchReveal_>, drawScreen<&G::hatchReveal_>,
         [](G& g) { g.hatchReveal_.tick(g); }, nullptr, FxSweep::None, Incubation::Holds},
        // Hold, flash, reveal on the heartbeat, then it parks at the reveal and waits for B
        // (completeEvolution). C does nothing: the evolution already happened. A Lockout
        // waits for it, as for the crash FX below, which outranks it.
        {Nav::ModalEvolve, pressScreen<&G::evolve_>, drawScreen<&G::evolve_>,
         [](G& g) { g.evolve_.tick(); }, nullptr, FxSweep::None, Incubation::Runs, MenuIdle::Holds,
         LockoutCrisis::Waits},
        // The crash FX holds a few beats, then parks and waits for B. Death is not
        // cancellable, so C does nothing.
        {Nav::ModalCSF, pressScreen<&G::csf_>, drawScreen<&G::csf_>,
         [](G& g) { g.csf_.tick(); }, nullptr, FxSweep::None, Incubation::Runs, MenuIdle::Holds,
         LockoutCrisis::Waits},
        // The chord opens the fight's Exploit override picker.
        {Nav::Combat, pressScreen<&G::fightScreen_>, drawScreen<&G::fightScreen_>,
         [](G& g) { g.stepCombatBeat(); }, pressScreen<&G::fightScreen_>, FxSweep::FightOutro},
        {Nav::ExploreControl, pressScreen<&G::exploreControl_>,
         drawScreen<&G::exploreControl_>, nullptr, nullptr},
        {Nav::Encounter, pressScreen<&G::encounter_>, drawScreen<&G::encounter_>, nullptr,
         nullptr},
        // Hands-off REVEAL: the Wi-Fi outcome is a rolled reveal, not a choice. Held ~3s
        // so it reads, then played out (a guardian awakened enters a fight; the rest
        // resolve to the habitat).
        {Nav::Wifi, pressScreen<&G::wifiScreen_>, drawScreen<&G::wifiScreen_>,
         [](G& g) { g.stepEventHold(kExploreRevealHoldBeats, &G::resolveWifiOutcome); }, nullptr,
         FxSweep::Dissolve},
        // Hands-off DECISION: a shop (item or mod) is a real buy/leave choice, so it holds
        // ~10s for a watching player to act, then leaves and keeps exploring.
        {Nav::Shop, pressScreen<&G::shop_>, drawScreen<&G::shop_>,
         [](G& g) { g.stepEventHold(kExploreDecisionHoldBeats, &G::leaveShop); }, nullptr},
        {Nav::ModShop, pressScreen<&G::shop_>, drawScreen<&G::shop_>,
         [](G& g) { g.stepEventHold(kExploreDecisionHoldBeats, &G::leaveShop); }, nullptr},
        {Nav::WarpPicker, pressScreen<&G::warpPicker_>, drawScreen<&G::warpPicker_>, nullptr,
         nullptr, FxSweep::None, Incubation::Runs, MenuIdle::Collapses},
        {Nav::RollbackPicker, [](G& g, const Ev& e) { g.statPicker_.onRollback(g, e); },
         [](const G& g, Fb& fb) { g.statPicker_.drawRollback(fb, g); }, nullptr, nullptr,
         FxSweep::None, Incubation::Runs, MenuIdle::Collapses},
        {Nav::RepartitionPicker, [](G& g, const Ev& e) { g.statPicker_.onRepartition(g, e); },
         [](const G& g, Fb& fb) { g.statPicker_.drawRepartition(fb, g); }, nullptr, nullptr,
         FxSweep::None, Incubation::Runs, MenuIdle::Collapses},
        {Nav::CacheYield, [](G& g, const Ev& e) { g.vault_.onReveal(g, e); },
         [](const G& g, Fb& fb) { g.vault_.drawReveal(fb, g); }, nullptr, nullptr},
        {Nav::BulkYield, [](G& g, const Ev& e) { g.vault_.onBulkReveal(g, e); },
         [](const G& g, Fb& fb) { g.vault_.drawBulkReveal(fb, g); }, nullptr, nullptr},
        {Nav::PostEncounter, pressScreen<&G::postEncounter_>, drawScreen<&G::postEncounter_>,
         nullptr, nullptr},
        // The played boards step on their own faster cadences (tickAnimClocks).
        {Nav::Stacker, pressScreen<&G::stackerScreen_>, drawScreen<&G::stackerScreen_>,
         nullptr, rulesChord},
        {Nav::Fragger, pressScreen<&G::fraggerScreen_>, drawScreen<&G::fraggerScreen_>,
         nullptr, rulesChord},
        // The Isolation Protocol priced its goal off the egg's clock as it stood when the
        // run began, so draining it underneath would quietly discount the run.
        {Nav::Isolation, pressScreen<&G::isolationScreen_>, drawScreen<&G::isolationScreen_>,
         nullptr, rulesChord, FxSweep::None, Incubation::Holds},
        // The CHROMATOPHORE spends its passes against the egg's clock, so it holds too.
        {Nav::Chroma, pressScreen<&G::chromaScreen_>, drawScreen<&G::chromaScreen_>, nullptr,
         rulesChord, FxSweep::None, Incubation::Holds},
        // The decrypt runs its own countdown in place of the egg's. A Lockout waits for it,
        // as for the cinematics and the DECRYPTOGRAM below: the hatch has no pet to starve.
        {Nav::Decryption, pressScreen<&G::decryptionScreen_>,
         drawScreen<&G::decryptionScreen_>, nullptr, rulesChord,
         FxSweep::None, Incubation::Holds, MenuIdle::Holds, LockoutCrisis::Waits},
        // A Lockout waits for the DECRYPTOGRAM to be left: the board cannot be left and its
        // ticket is already spent, so firing across it would destroy a found item.
        // THE DECRYPTOGRAM: A and C are the cursor's two directions, so the chord hands a
        // taken letter back to the pool. The screen sorts its two meanings out itself (a
        // letter held drops it; nothing held opens the RULES page), since only it knows
        // which stage the board is in.
        {Nav::Cryptogram, pressScreen<&G::cryptogramScreen_>,
         drawScreen<&G::cryptogramScreen_>, nullptr, pressScreen<&G::cryptogramScreen_>,
         FxSweep::None, Incubation::Runs, MenuIdle::Holds, LockoutCrisis::Waits},
        // The arcade payout is informational, so ANY button dismisses it (the
        // PostEncounter contract, not the standard A/B/C one).
        {Nav::ArcadeResult, [](G& g, const Ev& e) { g.arcadeScreen_.onResult(g, e); },
         [](const G& g, Fb& fb) { g.arcadeScreen_.drawResult(fb, g); }, nullptr, nullptr},
        // The bracket holds the idle timer: eight entrants, their levels and the next
        // opponent's species and Exploit are what a loadout is chosen against, and five
        // seconds is shorter than reading them. Collapsing it would also drop the operator
        // onto the habitat mid-decision with a run still in play.
        // ROCK THE DOCK: the chord is the device's one spare gesture and the bracket has
        // already spent A, B and C, so the BRIEFING (the only thing that tells an operator
        // what an arena bout even is) rides the "chord opens a reader, then plain A/B/C
        // drive it" shape the explore control overlay uses.
        {Nav::Tourney, pressScreen<&G::arena_>, drawScreen<&G::arena_>, nullptr,
         pressScreen<&G::arena_>},
        // Hands-off REVEAL: the guardian arriving and speaking is to watch, so it holds
        // ~5s, then the meeting goes on to whatever the fluency roll decided.
        {Nav::ShiboleetHail, [](G& g, const Ev& e) { g.shiboleet_.onHail(g, e); },
         [](const G& g, Fb& fb) { g.shiboleet_.drawHail(fb, g); },
         [](G& g) { g.stepEventHold(kShiboleetHailHoldBeats, &G::openShiboleetWelcome); },
         nullptr, FxSweep::Swarm},
        // Hands-off DECISION: silence past the hold is an answer, the wrong one. A restarts
        // the hold (ShiboleetScreen::onRiddle), so a player still reading is not timed out.
        {Nav::Shiboleet, [](G& g, const Ev& e) { g.shiboleet_.onRiddle(g, e); },
         [](const G& g, Fb& fb) { g.shiboleet_.drawRiddle(fb, g); },
         [](G& g) { g.stepEventHold(kShiboleetReplyHoldBeats, &G::leaveShiboleetUnanswered); },
         nullptr},
        // Hands-off REVEAL: what the guardian made of the answer, held ~6s, then played
        // out. The consequence is already paid; the hold only sets how long it is read.
        {Nav::ShiboleetVerdict, [](G& g, const Ev& e) { g.shiboleet_.onVerdict(g, e); },
         [](const G& g, Fb& fb) { g.shiboleet_.drawVerdict(fb, g); },
         [](G& g) { g.stepEventHold(kShiboleetVerdictHoldBeats, &G::finishShiboleet); }, nullptr,
         FxSweep::Swarm},
        // A STORY chapter and the CHAPTERS list hold the idle timer: fifteen seconds is
        // shorter than a page of prose, and a reader dropped onto the habitat mid-sentence
        // reads as a crash. The chapter has a deadline of its own (kStoryPanelMs), and the
        // archive is a menu the operator opened on purpose.
        {Nav::Story, pressScreen<&G::storyReader_>, drawScreen<&G::storyReader_>, nullptr,
         nullptr},
        {Nav::StoryArchive, pressScreen<&G::chapters_>, drawScreen<&G::chapters_>, nullptr,
         nullptr},
        {Nav::TipCard, pressScreen<&G::tipCardScreen_>, drawScreen<&G::tipCardScreen_>,
         nullptr, nullptr, FxSweep::None, Incubation::Runs, MenuIdle::Collapses},
        {Nav::AwayDigest, pressScreen<&G::awayDigest_>, drawScreen<&G::awayDigest_>, nullptr,
         nullptr, FxSweep::None, Incubation::Runs, MenuIdle::Collapses},
        {Nav::ItemOffer, pressScreen<&G::itemOffer_>, drawScreen<&G::itemOffer_>, nullptr,
         nullptr, FxSweep::None, Incubation::Runs, MenuIdle::Collapses},
    };
    static_assert(std::size(kScreens) == static_cast<size_t>(Nav::ItemOffer) + 1,
                  "one row per Nav");
    static_assert(inNavOrder(kScreens), "rows in Nav's order");
    return kScreens[static_cast<size_t>(n)];
}

}  // namespace mal
