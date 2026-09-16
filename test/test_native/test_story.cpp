// test_story.cpp — the STORY chapters: what they say, when they fire, and where they
// go afterwards.
//
// Four things are worth holding. That an authored chapter FITS the page it is drawn on
// (prose past the cap is silently lost, and a panel taller than the flow can never be
// scrolled past). That a beat fires ONCE, in front of the thing it introduces, and
// hands that thing back when it is done. That a panel turns itself over on the clock,
// because a hands-off walk has no thumb to press B. And that everything fired lands in
// the ARCHIVE, which is the only way back to a chapter an auto-progress walk paged
// through while nobody was watching.
#include "test_gates.h"

#include "core/content/story.h"
#include "core/ui/story_screen.h"

namespace {

// Step the walk until the zone's ARRIVAL chapter opens, dismissing whatever else the
// event table rolls on the way. A wild encounter is the majority slot and the chapter
// sits at the top of it, so this lands on Nav::Story rather than on Nav::Encounter —
// which is the thing being asserted.
bool walkToStory(Game& g) {
    for (int i = 0; i < 400 && g.nav() != Game::Nav::Story; ++i) {
        switch (g.nav()) {
            case Game::Nav::Idle: pingExplore(g); break;
            case Game::Nav::Wifi: g.onButton(press(Button::B)); break;
            case Game::Nav::Shop:
            case Game::Nav::ModShop: tapC(g); break;
            case Game::Nav::CacheYield:
            case Game::Nav::BulkYield: g.onButton(press(Button::B)); break;
            default: return false;         // anything else got in front of the chapter
        }
    }
    return g.nav() == Game::Nav::Story;
}

// Page a chapter to its end with B, however many windows it turns out to hold.
void readChapterOut(Game& g) {
    for (int i = 0; i < 32 && g.nav() == Game::Nav::Story; ++i)
        g.onButton(press(Button::B));
}

}  // namespace

// Every authored chapter has to fit the page it is drawn on. Prose past EffectText's
// cap is silently truncated, a heading wider than the flow is cut mid-word, and a panel
// taller than the whole body would draw clipped and never scroll past. The wires are
// the other half: they are persisted bits, so two chapters sharing one would have the
// walk marking a chapter nobody has read.
void test_story_chapters_fit_their_page() {
    bool seenWire[kStoryWireCap] = {};
    int authored = 0;
    for (int i = 0, n = storyEntryCount(); i < n; ++i) {
        const StoryEntry e = storyEntryAt(i);
        if (!e.chapter) continue;
        ++authored;
        const StoryChapterDef& c = *e.chapter;
        // A wire is 1-based, inside this build's set, and its own.
        CHECK(c.wire != kStoryWireNone && c.wire < kStoryWireCap);
        CHECK(!seenWire[c.wire]);
        seenWire[c.wire] = true;
        // The title is the archive row AND the reader's own header line.
        CHECK(c.title[0]);
        CHECK(textWidth(c.title) <= kActiveW - 2 * kMargin);
        for (int p = 0; p < c.panelCount; ++p) {
            const StoryPanelDef& panel = c.panels[p];
            CHECK(panel.heading && panel.heading[0]);
            CHECK(panel.text && panel.text[0]);
            CHECK(textWidth(panel.heading) <= kProseW);
            CHECK(static_cast<int>(std::strlen(panel.text)) <= EffectText::kMaxProse);
        }
    }
    CHECK(authored > 0);                    // something is written, or this gate is inert

    // ...and the built rows are the chapter, in order, with nothing clipped on the way
    // in — plus every panel fits a window of its own, so the flow can always advance.
    Game g{StartMode::Hatched, "bruinforce"};
    const StoryChapterDef* first = storyChapter(0, StoryBeat::AreaIntro);
    CHECK(first != nullptr);
    g.debugOpenStory(first);
    const auto rows = g.storyRows();
    CHECK(static_cast<int>(rows.size()) == first->panelCount);
    for (int i = 0; i < first->panelCount; ++i) {
        CHECK(std::strcmp(rows[i].label, first->panels[i].heading) == 0);
        CHECK(std::strcmp(rows[i].body.c_str(), first->panels[i].text) == 0);
        CHECK(!rows[i].body.atCap());
    }
    for (int top = 0; top < static_cast<int>(rows.size()); ++top)
        CHECK(proseRowsFitting(rows, top, kStoryBodyTop) >= 1);
}

// THE ARRIVAL BEAT fires in front of the first wild encounter in a zone, hands that
// encounter back when the chapter is done, and never fires again — a chapter is spent
// on the walk that opened it whether it was read or skipped, which is what the archive
// exists to recover.
void test_story_arrival_fires_once_and_hands_back() {
    Game g{StartMode::Hatched, "bruinforce"};
    g.inventory().add("sinkhole_trap", 60);          // a cheap way out of the fight
    const StoryChapterDef* ch = storyChapter(0, StoryBeat::AreaIntro);
    CHECK(ch && !g.storyRead(ch));
    g.debugArmExplore(0, 0);
    g.debugSetAutoProgress(false);
    CHECK(walkToStory(g));
    CHECK(g.currentStoryChapter() == ch);
    CHECK(g.storyRead(ch));                          // spent on OPEN, not on finish
    CHECK(g.storyWindow() == 0 && g.storyWindows() >= 1);

    // B pages it, and the last B hands back the encounter the chapter stood in front of.
    // An encounter met ON A WALK resolves itself (there is nobody at the intro to press
    // Fight), and a held Sinkhole Trap is what it resolves WITH — so the trap going
    // missing is the evidence that the deferred encounter actually ran.
    const int traps = g.inventory().count("sinkhole_trap");
    readChapterOut(g);
    CHECK(g.currentStoryChapter() == nullptr);
    CHECK(g.inventory().count("sinkhole_trap") == traps - 1);
    CHECK(g.nav() == Game::Nav::Idle);               // ...and the walk has it again

    // Walk on: the same zone never opens the chapter a second time.
    for (int i = 0; i < 40; ++i) {
        if (g.nav() == Game::Nav::Idle) pingExplore(g);
        else if (g.nav() == Game::Nav::Story) { CHECK(false); break; }
        else g.onButton(press(Button::B));
    }
    CHECK(g.nav() != Game::Nav::Story);
}

// A panel TURNS ITSELF OVER after kStoryPanelMs, which is what lets a chapter fire on a
// walk nobody is watching. A press re-arms the clock, so a reader who is reading is
// never overtaken by it.
void test_story_panel_turns_on_its_own_clock() {
    Game g{StartMode::Hatched, "bruinforce"};
    const StoryChapterDef* ch = storyChapter(0, StoryBeat::AreaIntro);
    CHECK(ch != nullptr);
    g.debugOpenStory(ch);
    CHECK(g.nav() == Game::Nav::Story);
    const int windows = g.storyWindows();
    CHECK(windows >= 2);                             // or there is no turn to observe
    uint32_t t = 0;
    // Just short of the deadline, nothing moves.
    g.tick(t += kStoryPanelMs - kHeartbeatMs);
    CHECK(g.storyWindow() == 0);
    // Crossing it turns the page by itself.
    g.tick(t += 2 * kHeartbeatMs);
    CHECK(g.storyWindow() == 1);
    // A press re-arms: the clock that was about to fire is reset by the reader's own B.
    const uint32_t nearly = t + kStoryPanelMs - kHeartbeatMs;
    g.tick(nearly - kHeartbeatMs);
    g.onButton(press(Button::B));                    // window 1 -> 2 (or the chapter ends)
    const int after = g.nav() == Game::Nav::Story ? g.storyWindow() : -1;
    g.tick(nearly);                                  // the OLD deadline, now spent
    if (after >= 0) CHECK(g.storyWindow() == after);

    // And the reading screens are held open against the menu idle collapse — fifteen
    // seconds is shorter than a page of prose.
    Game h{StartMode::Hatched, "bruinforce"};
    h.debugOpenStory(ch);
    uint32_t ht = 0;
    for (int i = 0; i < 3; ++i) h.tick(ht += kAutoDefocusMs / 4);
    CHECK(h.nav() == Game::Nav::Story);
}

// C SKIPS — the whole chapter, not one panel — and the thing the chapter stood in front
// of still happens. A chapter is not a menu, so "cancel" on one means "I am done being
// told", and the walk has to carry on either way.
void test_story_skip_still_runs_what_it_interrupted() {
    Game g{StartMode::Hatched, "bruinforce"};
    g.inventory().add("sinkhole_trap", 60);
    g.debugArmExplore(0, 0);
    g.debugSetAutoProgress(false);
    CHECK(walkToStory(g));
    CHECK(g.storyWindows() >= 2);                    // there is something left to skip
    const int traps = g.inventory().count("sinkhole_trap");
    tapC(g);
    CHECK(g.currentStoryChapter() == nullptr);
    // Skipped from the FIRST window, and the encounter behind it still resolved — the
    // trap it spent is what says so.
    CHECK(g.inventory().count("sinkhole_trap") == traps - 1);
    CHECK(g.nav() == Game::Nav::Idle);
}

// THE ARCHIVE is the way back to a chapter an auto-progress walk paged through. It
// lists what has been fired and nothing else — so it can spoil nothing — and the
// CHAPTERS category it lives behind is locked until there is something in it.
void test_story_archive_collects_what_the_walk_fired() {
    MemSaveStore store;
    {
        Game g{StartMode::Hatched, "bruinforce", &store};
        CHECK(g.storyChapterCount() == 0);
        // Nothing written yet -> the category is a "??????" the cursor skips, so the
        // picker's first landable row is still STORY.
        enterSubmenuId(g, SubmenuId::Expl);
        explPickCategory(g, ExplCat::Chapters);
        CHECK(g.listRow() != explCatRow(ExplCat::Chapters));
        tapC(g);

        g.inventory().add("sinkhole_trap", 60);
        g.debugArmExplore(0, 0);
        g.debugSetAutoProgress(false);
        CHECK(walkToStory(g));
        tapC(g);                                     // skipped — and still archived
        CHECK(g.storyChapterCount() == 1);

        // The category is a place to go now, and B on it opens the list. The walk is put
        // down first: EXPL RESUMES a running one inside its area, which is the level
        // above the picker this half of the gate is about.
        while (g.nav() != Game::Nav::Idle && g.nav() != Game::Nav::Cursor) tapC(g);
        stopExplore(g);
        enterSubmenuId(g, SubmenuId::Expl);
        explPickCategory(g, ExplCat::Chapters);
        CHECK(g.listRow() == explCatRow(ExplCat::Chapters));
        g.onButton(press(Button::B));
        CHECK(g.nav() == Game::Nav::StoryArchive);

        // B reads the focused row, and finishing it comes back to the archive rather
        // than to the walk — nothing on the ladder was interrupted to get here.
        g.onButton(press(Button::B));
        CHECK(g.nav() == Game::Nav::Story);
        CHECK(g.currentStoryChapter() == storyChapter(0, StoryBeat::AreaIntro));
        readChapterOut(g);
        CHECK(g.nav() == Game::Nav::StoryArchive);
        // The archive is a ROW LIST and joins the shared cursor (game_listnav.cpp), so
        // a held C walks it backward rather than backing out — the same gesture every
        // other list on the device answers to.
        uint32_t ct = 0;
        g.onButton(press(Button::C));
        g.tick(ct += kListRepeatDelayMs + kHeartbeatMs);
        g.onButton(lift(Button::C));
        CHECK(g.nav() == Game::Nav::StoryArchive);

        // ...and a TAP of C lands back on the CHAPTERS row the archive was opened from.
        tapC(g);
        CHECK(g.nav() == Game::Nav::Submenu);
        CHECK(g.listRow() == explCatRow(ExplCat::Chapters));

        Framebuffer fb(kActiveW, kActiveH);
        g.render(fb);
        CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
        g.tick(kSaveAutosaveMs + kHeartbeatMs);      // autosave the read-set
    }
    // The read-set is PLAYER-level and persisted: a reboot still knows the chapter was
    // shown, so it is not told again and the archive still holds it.
    Game g2{StartMode::Hatched, "bruinforce", &store};
    CHECK(g2.storyRead(storyChapter(0, StoryBeat::AreaIntro)));
    CHECK(g2.storyChapterCount() == 1);
}

// EXPL's ACTIVITY PICKER: four rows, one per kind of thing to do, with STORY the one
// that can never be locked. The cursor skips what is locked, B opens a category with
// rows onto its own level and acts outright on one without, and C walks back out a
// level at a time.
void test_expl_categories_are_the_top_level() {
    // The row's own copy first: a name and a blurb that overrun their column travel
    // instead of standing still, which is a cost paid on the one screen whose whole job
    // is to be read at a glance.
    for (int row = 0; row < kExplCatRows; ++row) {
        const ExplCat c = explCatAt(row);
        CHECK(explCatName(c)[0] && explCatBlurb(c)[0]);
        CHECK(textWidth(explCatBlurb(c)) <= kExplCatTextW);
    }

    Game g{StartMode::Hatched, "bruinforce"};
    g.debugMarkStoryRead();                          // the archive's own gate covers that
    enterSubmenuId(g, SubmenuId::Expl);
    // Fresh device: only STORY is open, so A round-trips back to it.
    CHECK(g.listRow() == explCatRow(ExplCat::Story));
    for (int i = 0; i < kExplCatRows; ++i) {
        g.onButton(press(Button::A));
        g.onButton(lift(Button::A));     // A arms a repeat on a list; let go of it
    }
    CHECK(g.listRow() == explCatRow(ExplCat::Story));
    // ...and so does the device-wide backward walk, which is a HOLD on C: the picker is
    // a list like any other, so it joins the shared cursor rather than ignoring the
    // gesture (game_listnav.cpp). Stepping back from the first row wraps to the LAST
    // open one, which with the chapters read is CHAPTERS.
    uint32_t t = 0;
    g.onButton(press(Button::C));
    g.tick(t += kListRepeatDelayMs + kHeartbeatMs);
    g.onButton(lift(Button::C));
    CHECK(g.nav() == Game::Nav::Submenu);            // a HOLD walks, it does not cancel
    CHECK(g.listRow() == explCatRow(ExplCat::Chapters));
    explPickCategory(g, ExplCat::Story);

    // B opens STORY onto the AREA list — area 0's header, not its sub-areas.
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Submenu);
    CHECK(explRowArea(g.listRow()) == 0 && explRowSub(g.listRow()) < 0);
    tapC(g);
    CHECK(g.listRow() == explCatRow(ExplCat::Story));   // ...and C comes back to it

    // Reaching The Pirate Bayou opens the ARENA, which has no rows of its own: B on it
    // draws a bracket outright rather than opening a list of one.
    g.debugSetSectorCleared(0, true);
    g.debugAddCombatXp(600000);                        // a pet that can stand in a bout
    explPickCategory(g, ExplCat::Arena);
    CHECK(g.listRow() == explCatRow(ExplCat::Arena));
    tapB(g);
    CHECK(g.nav() == Game::Nav::Tourney);
    CHECK(g.tourneyRunning());

    // And the two ENDLESS zones live behind one row of their own.
    Game h{StartMode::Hatched, "bruinforce"};
    h.debugMarkStoryRead();
    for (int a = 0; a < kExplSectors; ++a) h.debugSetSectorCleared(a, true);
    enterSubmenuId(h, SubmenuId::Expl);
    explPickCategory(h, ExplCat::Endless);
    h.onButton(press(Button::B));
    CHECK(explRowIsDeepWeb(h.listRow()));
    h.onButton(press(Button::B));
    CHECK(h.exploreActive() && h.exploreSector() == kDeepWebSector);
}
