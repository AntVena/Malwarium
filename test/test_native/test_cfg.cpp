// test_cfg.cpp — native gates for the CFG tree.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the
// feature whose field it migrates, not in a migrations pile of its own.
#include "test_gates.h"

#include "core/ui/layout.h"  // listScrollTop — the shared list-window offset

// CFG UI Mode toggle is wired to the live Game::setUiMode: cycle the
// option, B applies and changes the carousel presentation in-menu.
void test_cfg_uimode_toggle() {
    Game g{StartMode::Hatched};
    CHECK(g.uiMode() == UiMode::IconsLabel);
    enterCfgTarget(g, CfgScreen::UiMode);         // CFG -> DEVICE -> UI MODE
    CHECK(g.nav() == Game::Nav::Detail);
    CHECK(g.cfgScreen() == CfgScreen::UiMode);
    g.onButton(press(Button::A));                 // cycle IconsLabel -> IconsOnly
    g.onButton(press(Button::B));                 // apply -> back to DEVICE
    CHECK(g.nav() == Game::Nav::Detail);
    CHECK(g.cfgScreen() == CfgScreen::Device);
    CHECK(g.uiMode() == UiMode::IconsOnly);       // the live mode changed
    tapC(g);                 // and DEVICE backs out to the list
    CHECK(g.nav() == Game::Nav::Submenu);
}

// CFG -> DEVICE -> TRAVEL MODE opens on NO and needs a second, deliberate yes, like
// every other screen that commits something the row offering it cannot take back.
void test_cfg_travel_confirm_asks_twice() {
    {   // B on the NO it opened on asks for nothing, and backs out to the group.
        Game g{StartMode::Hatched};
        enterCfgTarget(g, CfgScreen::Travel);
        CHECK(g.cfgScreen() == CfgScreen::Travel);
        CHECK(!g.travelSleepRequested());
        g.onButton(press(Button::B));
        CHECK(!g.travelSleepRequested());
        CHECK(g.cfgScreen() == CfgScreen::Device);
    }
    {   // C is the other way out, and equally silent.
        Game g{StartMode::Hatched};
        enterCfgTarget(g, CfgScreen::Travel);
        tapC(g);
        CHECK(!g.travelSleepRequested());
        CHECK(g.cfgScreen() == CfgScreen::Device);
    }
    {   // A moves onto YES; only then does B latch the request.
        Game g{StartMode::Hatched};
        enterCfgTarget(g, CfgScreen::Travel);
        g.onButton(press(Button::A));
        g.onButton(press(Button::B));
        CHECK(g.travelSleepRequested());
        // Latched, not a pulse: the device tier is already landing a save and
        // powering the panel down, so nothing here half-cancels it.
        tapC(g);
        g.onButton(press(Button::A));
        CHECK(g.travelSleepRequested());
        CHECK(g.cfgScreen() == CfgScreen::Travel);
    }
}

// CFG -> DEVICE -> FORMAT SD opens on NO, needs a deliberate yes, holds every button
// while the device tier works, and lets any button leave once a result is on screen.
void test_cfg_format_sd_confirm() {
    {   // B on the NO it opened on asks for nothing, and backs out to the group.
        Game g{StartMode::Hatched};
        enterCfgTarget(g, CfgScreen::FormatSd);
        CHECK(g.cfgScreen() == CfgScreen::FormatSd);
        g.onButton(press(Button::B));
        CHECK(!g.sdFormatRequested());
        CHECK(g.cfgScreen() == CfgScreen::Device);
    }
    {   // C is the other way out, and equally silent.
        Game g{StartMode::Hatched};
        enterCfgTarget(g, CfgScreen::FormatSd);
        tapC(g);
        CHECK(!g.sdFormatRequested());
        CHECK(g.cfgScreen() == CfgScreen::Device);
    }
    {   // A moves onto YES; only then does B latch the job, which nothing cancels.
        Game g{StartMode::Hatched};
        enterCfgTarget(g, CfgScreen::FormatSd);
        g.onButton(press(Button::A));
        g.onButton(press(Button::B));
        CHECK(g.sdFormatRequested());
        CHECK(g.sdFormatState() == SdFormatState::Working);
        tapC(g);
        g.onButton(press(Button::A));
        CHECK(g.sdFormatRequested());
        CHECK(g.cfgScreen() == CfgScreen::FormatSd);

        // The device tier's half: a fresh reading, then the outcome.
        g.setSdStatus({true, 30000});
        g.finishSdFormat(true);
        CHECK(!g.sdFormatRequested());
        CHECK(g.sdFormatState() == SdFormatState::Done);
        CHECK(g.cfgScreen() == CfgScreen::FormatSd);  // the result stays to be read
        g.onButton(press(Button::A));                 // any button leaves it
        CHECK(g.cfgScreen() == CfgScreen::Device);
        CHECK(g.sdFormatState() == SdFormatState::Idle);
    }
    {   // A failure is reported the same way, and re-opening asks the question again.
        Game g{StartMode::Hatched};
        enterCfgTarget(g, CfgScreen::FormatSd);
        g.onButton(press(Button::A));
        g.onButton(press(Button::B));
        g.finishSdFormat(false);
        CHECK(g.sdFormatState() == SdFormatState::Failed);
        tapC(g);
        CHECK(g.cfgScreen() == CfgScreen::Device);
        enterCfgTarget(g, CfgScreen::FormatSd);
        CHECK(g.sdFormatState() == SdFormatState::Idle);
    }
}

// An update job writes the 'Pedia onto the card, so FORMAT SD refuses its yes while
// one runs rather than erasing the card out from under the install.
void test_cfg_format_sd_refused_during_update() {
    Game g{StartMode::Hatched};
    g.setNetProvisioned(true);
    g.setUpdateSourceKnown(true);
    g.requestUpdateCheck();                       // the job outlives any screen
    CHECK(g.updateJobLive());
    enterCfgTarget(g, CfgScreen::FormatSd);
    g.onButton(press(Button::A));
    g.onButton(press(Button::B));
    CHECK(!g.sdFormatRequested());
    CHECK(g.cfgScreen() == CfgScreen::FormatSd);  // stays, saying why
}

// The travel-sleep contract, and the reason the mode needs no clock-freeze machinery
// of its own: the device tier lands a save and then DEEP-sleeps, which on this board
// is a reset, so a wake re-enters through that save. Whatever the gap was, the state
// it comes back to must be the state it left.
//
// Manufactured at the seam — the save the sleep writes, reloaded the way a wake
// reloads it — rather than by trying to sleep, which the host tier cannot do.
void test_travel_sleep_credits_nothing_to_the_gap() {
    MemSaveStore store;
    Game before(StartMode::Hatched, "paypup", &store);
    for (uint32_t t = 1000; t <= 120000; t += 1000) before.tick(t);
    const int hunger = before.model().hunger();
    const int happy = before.model().happiness();
    const uint32_t evolveRemain = before.evolveRemainMs();
    const uint32_t uptime = before.lifetimeUptimeMs();
    CHECK(evolveRemain > 0);                    // there IS a growth clock to strand
    CHECK(before.saveNow());                    // what the device tier does last

    // The wake. A fresh Game over the same store is exactly what the reset produces.
    Game after(StartMode::Hatched, "paypup", &store);
    CHECK(after.model().hunger() == hunger);    // no decay credited to the gap
    CHECK(after.model().happiness() == happy);
    CHECK(after.evolveRemainMs() == evolveRemain);   // nor any growth
    // Lifetime uptime counts time AWAKE, so it resumes rather than jumping: the
    // Backup Drive shield and the CSF grace window both anchor on it.
    CHECK(after.lifetimeUptimeMs() == uptime);

    // And the one outcome the device tier must not sleep over. persistSave defers
    // when the heap is too tight to serialize, and a deferral before a reset is a
    // lost session, so saveNow reports it instead of returning a bare void.
    Game tight(StartMode::Hatched, "paypup", &store);
    tight.setHeapProbe([]() -> uint32_t { return 1024; });
    CHECK(!tight.saveNow());
}

// A setting inside a group returns to that group, not to the top of CFG — and the
// group's cursor lands on the row just left, so a second visit resumes there.
void test_cfg_group_back_resumes_row() {
    Game g{StartMode::Hatched};
    enterCfgTarget(g, CfgScreen::PediaAp);        // CFG -> RADIO -> PEDIA AP
    CHECK(g.cfgScreen() == CfgScreen::PediaAp);
    tapC(g);                 // no change -> back to RADIO
    CHECK(g.nav() == Game::Nav::Detail);
    CHECK(g.cfgScreen() == CfgScreen::Radio);
    g.onButton(press(Button::B));                 // B re-opens the SAME row
    CHECK(g.cfgScreen() == CfgScreen::PediaAp);
    tapC(g);
    tapC(g);                 // RADIO -> the CFG list
    CHECK(g.nav() == Game::Nav::Submenu);
}

// The RADIO group reports the arbiter's resolved owner, not each toggle's intent:
// two consents can read ON while exactly one holds the radio.
void test_cfg_radio_reports_owner() {
    Game g{StartMode::Hatched};
    CHECK(g.radioOwner() == RadioOwner::None);    // host: nothing owns the radio
    g.setNetScanEnabled(true);
    g.setApEnabled(true);                          // both consents on...
    g.setRadioOwner(RadioOwner::Ap);               // ...the arbiter granted ONE
    CHECK(std::string(radioOwnerName(g.radioOwner())) == "PEDIA AP");
    enterCfgTarget(g, CfgScreen::Radio);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
}

// The RADIO group tells the operator "nearest the top wins", so the row order has
// to BE the arbiter's priority order — otherwise the screen's one explanation of
// why a switched-on row isn't running is a lie. RadioOwner is declared lowest
// priority first (radio_status.h), so the table read top-down must be strictly
// descending in that enum. AUDIT maps to two owners; it is compared at the higher
// of them, which is the most the row can ever resolve to.
void test_cfg_radio_rows_follow_arbiter_priority() {
    const CfgRow* rows = nullptr;
    const int n = cfgGroupRows(CfgScreen::Radio, rows);
    CHECK(n == 3);

    auto rank = [](CfgScreen s) {
        switch (s) {
            case CfgScreen::PediaAp: return RadioOwner::Ap;
            case CfgScreen::Link: return RadioOwner::Link;
            case CfgScreen::Audit: return RadioOwner::Capture;
            default: return RadioOwner::None;
        }
    };
    for (int i = 0; i < n; ++i) CHECK(rank(rows[i].target) != RadioOwner::None);
    for (int i = 1; i < n; ++i)
        CHECK(static_cast<int>(rank(rows[i].target)) <
              static_cast<int>(rank(rows[i - 1].target)));
}

// the CFG list scrolls once it overflows the viewport. The pure offset
// helper keeps the cursor on-screen and clamps to a valid window.
void test_cfg_list_scroll_offset() {
    // Fits entirely: never scrolls.
    CHECK(listScrollTop(0, 6, 6) == 0);
    CHECK(listScrollTop(5, 6, 6) == 0);
    // Overflows (10 rows, 6 visible): top rows show window 0; the cursor stays
    // pinned to the bottom visible slot as it descends; clamps at n-visible.
    CHECK(listScrollTop(0, 10, 6) == 0);
    CHECK(listScrollTop(5, 10, 6) == 0);      // last fully-visible row at window 0
    CHECK(listScrollTop(6, 10, 6) == 1);      // one past -> scroll by 1
    CHECK(listScrollTop(9, 10, 6) == 4);      // last row -> max window (10-6)
    // The live table, whatever its current length, always keeps the last row visible.
    const CfgRow* rows = nullptr;
    const int n = cfgRows(rows);
    const int top = listScrollTop(n - 1, n, 6);
    CHECK(top >= 0);
    CHECK(n - 1 >= top);                       // cursor at or after the window start
    CHECK(n - 1 < top + 6);                    // ...and within the visible window
}

// The DEV "Reset to Hatch" row (dev_config.h) wipes to the Hatch in one B press.
void test_cfg_dev_reset_row() {
    const CfgRow* rows = nullptr;
    const int n = cfgRows(rows);
    int resetRow = -1;
    for (int i = 0; i < n; ++i) if (rows[i].target == CfgScreen::ResetHatch) resetRow = i;
    if (resetRow < 0) return;                     // row compiled out (release build)
    Game g{StartMode::Hatched};
    enterSubmenuId(g, SubmenuId::Cfg);
    for (int i = 0; i < resetRow; ++i) g.onButton(press(Button::A));
    g.onButton(press(Button::B));                 // one press -> resetToHatch (line-select)
    pickFirstEggLine(g);                           // pick Ransomware -> egg at idle
    CHECK(g.nav() == Game::Nav::Idle);            // egg laid at idle
    CHECK(g.pet() != nullptr && g.inEggPhase());
}

// The hidden Factory Reset: hold-B on System Info reveals it, hold-B on
// the reset screen commits the wipe. Releasing B aborts the hold.
void test_cfg_factory_reset_hold() {
    Game g{StartMode::Hatched};
    enterSubmenuId(g, SubmenuId::Cfg);            // listRow 0 == System Info
    g.onButton(press(Button::B));                 // open System Info (L3)
    CHECK(g.nav() == Game::Nav::Detail);
    uint32_t t = 0;

    // A short hold, then release, must NOT reveal (abort).
    g.onButton(press(Button::B));                 // arm hold
    g.tick(t += kFactoryRevealMs / 2);
    g.onButton({Button::B, false, false});        // release -> abort
    g.tick(t += kHeartbeatMs);                     // (a longer wait would auto-defocus)
    CHECK(g.nav() == Game::Nav::Detail);          // still System Info, not revealed

    // A full hold reveals the Factory Reset screen.
    g.onButton(press(Button::B));                 // arm hold
    g.tick(t += kFactoryRevealMs + kHeartbeatMs); // elapses -> reveal
    CHECK(g.nav() == Game::Nav::Detail);          // FactoryReset is an L3 (Detail)
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));

    // Hold-B again commits the wipe -> line-select -> a freshly laid egg at idle.
    g.onButton(press(Button::B));                 // arm hold-to-commit
    g.tick(t += kFactoryCommitMs + kHeartbeatMs);
    pickFirstEggLine(g);                           // commit re-enters line-select; pick Ransomware
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.pet() != nullptr && g.inEggPhase());
}

// The Factory Reset's two scopes differ, and each does exactly what its screen
// says: RESET PET lays a fresh egg and leaves the operator's history standing,
// WIPE EVERYTHING clears that history too (and returns the radio consents to their
// out-of-box OFF). A picker whose options behaved alike would be a lie.
void test_cfg_factory_reset_scopes_differ() {
    // Three different storage shapes, so the wipe is shown to reach all of them:
    // a bitmask (Titles), a vector of registry pointers ('Pedia reveals), and the
    // persisted consent bools.
    auto seedProgress = [](Game& g) {
        g.debugUnlockTitle(0);                     // a Title (auto-equips)
        g.markCreatureSeen("berserkernel");     // a 'Pedia reveal
        g.setNetScanEnabled(true);
        g.setApEnabled(true);                      // two radio consents
    };
    // Commit the reveal + the hold that follows it, exactly as the screen does.
    auto commitWipe = [](Game& g, int scope) {
        enterCfgTarget(g, CfgScreen::SysInfo);
        uint32_t t = 0;
        g.onButton(press(Button::B));
        g.tick(t += kFactoryRevealMs + kHeartbeatMs);   // hold-B -> the reset screen
        CHECK(g.cfgScreen() == CfgScreen::FactoryReset);
        for (int i = 0; i < scope; ++i) g.onButton(press(Button::A));   // cycle scope
        g.onButton(press(Button::B));
        g.tick(t += kFactoryCommitMs + kHeartbeatMs);   // hold-B -> commit
        pickFirstEggLine(g);
    };

    // RESET PET keeps every one of them.
    { Game g{StartMode::Hatched};
      seedProgress(g);
      CHECK(g.equippedTitle() == 0 && g.creatureSeen("berserkernel"));
      commitWipe(g, /*scope=*/0);
      CHECK(g.pet() != nullptr && g.inEggPhase()); // ...a fresh egg all the same
      CHECK(g.equippedTitle() == 0);
      CHECK(g.creatureSeen("berserkernel"));
      CHECK(g.netScanEnabled() && g.apEnabled()); }

    // WIPE EVERYTHING clears them.
    { Game g{StartMode::Hatched};
      seedProgress(g);
      commitWipe(g, /*scope=*/1);
      CHECK(g.pet() != nullptr && g.inEggPhase());
      CHECK(g.equippedTitle() == -1);
      CHECK(!g.creatureSeen("berserkernel"));
      CHECK(!g.netScanEnabled());                  // consents back to out-of-box OFF
      CHECK(!g.apEnabled() && !g.linkEnabled());
      // ...but the pet it just laid is still tallied as this device's own.
      CHECK(g.speciesRaised() >= 1); }
}

// Grayscale gate: every CFG screen reads with colour stripped (ink + structure) —
// the list, both group screens, and every setting inside them.
void test_cfg_screens_grayscale() {
    Framebuffer fb(kActiveW, kActiveH);
    const int W = kActiveW, H = kActiveH;

    // List.
    { Game g{StartMode::Hatched}; enterSubmenuId(g, SubmenuId::Cfg);
      g.render(fb); CHECK(hasDarkInk(fb, 0, 0, W, H)); }

    const CfgRow* rows = nullptr;
    const int n = cfgRows(rows);
    for (int r = 0; r < n; ++r) {
        // The DEV reset row has no L3 viewer (it acts + stays on the list).
        if (rows[r].target == CfgScreen::ResetHatch) continue;
        Game g{StartMode::Hatched};
        enterCfgTarget(g, rows[r].target);
        CHECK(g.nav() == Game::Nav::Detail);
        g.render(fb);
        CHECK(hasDarkInk(fb, 0, 0, W, H));

        // ...and, for a group row, each setting it holds.
        const CfgRow* sub = nullptr;
        const int m = cfgGroupRows(rows[r].target, sub);
        for (int s = 0; s < m; ++s) {
            // CARE ALERTS is a switch, not a screen: it flips in place and stays on
            // the group (test_cfg_care_alerts_switch drives it).
            if (sub[s].target == CfgScreen::CareAlerts) continue;
            Game gs{StartMode::Hatched};
            enterCfgTarget(gs, sub[s].target);
            CHECK(gs.nav() == Game::Nav::Detail);
            CHECK(gs.cfgScreen() == sub[s].target);
            gs.render(fb);
            CHECK(hasDarkInk(fb, 0, 0, W, H));
        }
    }
}

// CFG > DEVICE > CARE ALERTS turns the carousel's "!" marks off and on in place, and
// both it and UI MODE beside it are DEVICE preferences: they survive a reboot (the save),
// and the save's own tail carries them, so an update that reads the blob keeps them too.
// A blob from before they were stored reads as what every device drew: ICONS+LABEL, ON.
void test_cfg_care_alerts_switch() {
    MemSaveStore store;
    const int W = kActiveW, H = kActiveH;
    int maint = 0;
    while (carouselSlots()[maint].id != SubmenuId::Maint) ++maint;
    const int x0 = (maint % kSlotCols) * kSlotW + kSlotW - 9;
    const int y0 = maint < kSlotCols ? 0 : kLivingBottom;
    {
        Game g{StartMode::Hatched, "paypup", &store};
        CHECK(g.careAlerts());                               // on out of the box
        g.model().setFragmentation(kFragCautionMin);         // MAINT wants a "!"
        Framebuffer on(W, H), off(W, H);
        g.render(on);

        enterCfgTarget(g, CfgScreen::Device);
        CHECK(g.cfgScreen() == CfgScreen::Device);
        const CfgRow* rows = nullptr;
        const int n = cfgGroupRows(CfgScreen::Device, rows);
        int row = -1;
        for (int i = 0; i < n; ++i) if (rows[i].target == CfgScreen::CareAlerts) row = i;
        CHECK(row >= 0);
        for (int i = 0; i < row; ++i) g.onButton(press(Button::A));
        g.onButton(press(Button::B));                        // flip it
        CHECK(!g.careAlerts());
        CHECK(g.cfgScreen() == CfgScreen::Device);           // ...in place
        Framebuffer dev(W, H);
        g.render(dev);                                       // the scrolled list draws
        CHECK(hasDarkInk(dev, 0, 0, W, H));

        while (g.nav() != Game::Nav::Idle && g.nav() != Game::Nav::Cursor) tapC(g);
        g.tick(kAutoDefocusMs + 1);
        CHECK(g.nav() == Game::Nav::Idle);
        g.render(off);
        bool lit = false;                                    // nothing in MAINT's gutter
        const float track = luminance(palColor(Pal::TRACK));
        for (int y = y0; y < y0 + kTrackH; ++y)
            for (int x = x0; x < x0 + 9; ++x)
                if (luminance(off.get(x, y)) > track + 0.15f) lit = true;
        CHECK(!lit);

        g.setUiMode(UiMode::TextOnly);
        g.tick(kAutoDefocusMs + 1 + kSaveAutosaveMs + kHeartbeatMs);   // autosave
    }
    Game again{StartMode::Hatched, "paypup", &store};        // a reboot onto the save
    CHECK(!again.careAlerts());
    CHECK(again.uiMode() == UiMode::TextOnly);

    SaveData d;                                              // the codec, both ways
    d.uiMode = static_cast<uint8_t>(UiMode::IconsOnly);
    d.careAlerts = 0;
    SaveData back;
    CHECK(deserializeSave(serializeSave(d), back));
    CHECK(back.uiMode == static_cast<uint8_t>(UiMode::IconsOnly) && back.careAlerts == 0);
    CHECK(SaveData{}.careAlerts == 1 && SaveData{}.uiMode == 0);   // the pre-v67 reading
}

// CFG > DEVICE > TIPS lists every card this device has shown and reads one again on
// B, landing back on the list. A card not yet shown is not listed.
void test_cfg_tips_replay() {
    Game g{StartMode::Hatched};                        // the seam counts them all as seen
    enterCfgTarget(g, CfgScreen::Tips);
    CHECK(g.nav() == Game::Nav::Detail && g.cfgScreen() == CfgScreen::Tips);
    const std::vector<Game::Tip> tips = g.replayableTips();
    CHECK(tips.size() == 7);
    for (Game::Tip t : tips) CHECK(textWidth(Game::tipTitle(t)) <= kActiveW - 30);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));

    g.onButton(press(Button::A));                      // second row
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::TipCard && g.tipCard() == tips[1]);
    for (int i = 0; i < 8 && g.nav() == Game::Nav::TipCard; ++i)
        g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Detail && g.cfgScreen() == CfgScreen::Tips);
    g.onButton(press(Button::C));
    CHECK(g.cfgScreen() == CfgScreen::Device);         // back to its group

    Game fresh{StartMode::Hatched};
    fresh.debugClearTips();
    CHECK(fresh.replayableTips().empty());
    fresh.markTipSeen(Game::Tip::FirstWalk);
    CHECK(fresh.replayableTips().size() == 1);
}

// SD RECHECK is the A press on System Info, not a list row — it acts on the SD
// line that screen already reports through.
void test_cfg_sysinfo_sd_recheck() {
    Game g{StartMode::Hatched};
    enterCfgTarget(g, CfgScreen::SysInfo);
    CHECK(g.cfgScreen() == CfgScreen::SysInfo);
    CHECK(!g.sdRecheckRequested());
    g.onButton(press(Button::A));                 // ask the device tier to re-mount
    CHECK(g.sdRecheckRequested());
    CHECK(g.nav() == Game::Nav::Detail);          // ...and stay to watch the SD line
    g.clearSdRecheck();                            // (the device tier's half)
    tapC(g);
    CHECK(g.nav() == Game::Nav::Submenu);
}
