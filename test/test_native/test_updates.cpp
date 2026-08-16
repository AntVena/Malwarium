// test_updates.cpp — native gates for over-the-air updates, the manifest/tar readers and the radio consents.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the
// feature whose field it migrates, not in a migrations pile of its own.
#include "test_gates.h"

// save v18: the per-spare equip gate round-trips.
void test_save_v18_mod_reqlevel() {
    SaveData a;
    std::strcpy(a.activeId, "paypup");
    a.ownedMods.resize(2);
    std::strcpy(a.ownedMods[0].id, "crypto_coprocessor");
    std::strcpy(a.ownedMods[1].id, "overclock_chip");
    a.ownedModReqLevels = {3, 22};
    auto full = serializeSave(a);

    SaveData out;
    CHECK(deserializeSave(full, out));
    CHECK(out.hasModEquipLevelData);
    CHECK(out.ownedModReqLevels == a.ownedModReqLevels);
}

// The pool has a ceiling now, and the Rig Shop row is what moves it. Both halves matter:
// a cap with no way to raise it is a nerf, and a shop row that doesn't actually change
// what a grant does is a Bits sink that sells nothing.
void test_mod_storage_cap_bounds_the_pool_and_the_shop_raises_it() {
    // The tier table has to stay inside what a nibble can hold, or a bought tier would
    // sell room the save cannot describe.
    CHECK(kModCopyCapBase >= 1 && kModCopyCapBase <= kModCopyCapMax);
    for (int t = 0; t <= kModStorageMaxTier; ++t) {
        CHECK(modCopyCap(t) >= kModCopyCapBase);
        CHECK(modCopyCap(t) <= kModCopyCapMax);
    }
    CHECK(modCopyCap(0) == kModCopyCapBase);
    CHECK(modCopyCap(kModStorageMaxTier) > kModCopyCapBase);       // the ladder climbs
    CHECK(modCopyCap(kModStorageMaxTier + 9) == modCopyCap(kModStorageMaxTier));  // clamps

    Loadout l;
    CHECK(l.grant("heat_sink", kModCopyCapBase));
    CHECK(l.grant("heat_sink", kModCopyCapBase));
    CHECK(!l.grant("heat_sink", kModCopyCapBase));   // full: reported, not silently dropped
    CHECK(l.countOf("heat_sink") == kModCopyCapBase);
    // Equipping frees a copy, so the cap bounds what is HELD, not what may ever be earned.
    l.equip(0, "heat_sink");
    CHECK(l.countOf("heat_sink") == kModCopyCapBase - 1);
    CHECK(l.grant("heat_sink", kModCopyCapBase));

    // A drop into a full pool leaves the pool alone, and the count doesn't creep.
    Game g{StartMode::Hatched};
    const int cap = g.modStorageCap();
    CHECK(cap == kModCopyCapBase);                  // nothing bought yet
    for (int i = 0; i < cap + 5; ++i) g.debugGrantMod("heat_sink");
    CHECK(g.loadout().countOf("heat_sink") == cap);

    // Buying MOD STORAGE raises what the same call will accept.
    g.debugSetBits(kModStorageStart);
    g.debugBuyModStorage();                         // through the real till
    CHECK(g.modStorageCap() == modCopyCap(1));
    CHECK(g.modStorageCap() > cap);
    g.debugGrantMod("heat_sink");
    CHECK(g.loadout().countOf("heat_sink") == cap + 1);
}

// The v45 pool is a nibble of COUNT per mod wire, two mods to a byte. The packing is
// where this could go wrong invisibly — a count bleeding into the neighbouring nibble
// hands the player copies of a mod they never earned — so the odd/even split, the 0..15
// range and the clamp are asserted directly rather than through a save.
void test_save_mod_count_nibbles_pack_two_mods_per_byte() {
    std::vector<uint8_t> packed;
    CHECK(saveModCount(packed, 0) == 0);            // nothing written reads as none held
    CHECK(saveModCount(packed, 999) == 0);          // ...and so does out of range
    CHECK(saveModCount(packed, -1) == 0);

    saveSetModCount(packed, 4, 3);                  // even wire -> low nibble
    saveSetModCount(packed, 5, 12);                 // odd wire  -> high nibble, same byte
    CHECK(packed.size() == 3);                      // wires 0..5 -> 3 bytes
    CHECK(saveModCount(packed, 4) == 3);
    CHECK(saveModCount(packed, 5) == 12);
    CHECK(saveModCount(packed, 3) == 0);            // neighbours untouched
    CHECK(saveModCount(packed, 6) == 0);

    // Rewriting one nibble must leave its partner alone — the bug that would silently
    // move copies between two unrelated mods.
    saveSetModCount(packed, 4, 1);
    CHECK(saveModCount(packed, 4) == 1 && saveModCount(packed, 5) == 12);

    // 15 is what 4 bits can say, and kModCopyCapMax is set to exactly that. A count past
    // it clamps rather than wrapping into the next mod.
    CHECK(kModCopyCapMax == 15);
    saveSetModCount(packed, 5, 99);
    CHECK(saveModCount(packed, 5) == kModCopyCapMax);
    CHECK(saveModCount(packed, 4) == 1);
}

// A count stored against a wire the running content no longer defines is dropped rather
// than resurrected under some other mod's id — the failure a positional array would have
// and a wire-keyed one must not.
void test_save_mod_count_for_a_retired_wire_is_dropped() {
    MemSaveStore store;
    {
        Game g(StartMode::Hatched, "paypup", &store);
        CHECK(g.saveNow());
    }
    SaveData d;
    CHECK(deserializeSave(store.bytes(), d));
    // kModWireCap - 1 is addressable by the format and belongs to no shipped row.
    CHECK(ContentRegistry::embedded().modByWire(kModWireCap - 1) == nullptr);
    saveSetModCount(d.ownedModCounts, kModWireCap - 1, 5);
    store.save(serializeSave(d));

    Game g2(StartMode::Hatched, "paypup", &store);
    int copies = 0;
    for (const OwnedMod& m : g2.loadout().owned()) copies += m.count;
    CHECK(copies == 2);                          // just the two the seed grants
}

// v37 round-trip: the pet-to-pet LINK opt-in. Only the consent bit is in the blob —
// the met operators themselves ride the SD-backed PeerLedger.
void test_save_v37_link_roundtrip() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.linkEnabled = true;
    auto blob = serializeSave(a);
    SaveData out;
    CHECK(deserializeSave(blob, out));
    CHECK(out.linkEnabled);
}

// The v38 byte is RESERVED: it held a standing internet opt-in and no longer means
// anything, so a blob written with it set must come back with this device off the
// 'net. The byte itself has to stay — v39..v42 sit behind it and the tail is read
// positionally — which is what the second half checks.
void test_save_v38_net_slot_is_reserved() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.netReserved = true;                       // whatever a caller puts here...
    a.linkEnabled = true;
    auto blob = serializeSave(a);
    SaveData out;
    CHECK(deserializeSave(blob, out));
    CHECK(!out.netReserved);                    // ...never survives the round trip
    CHECK(out.linkEnabled);                     // the v37 field ahead of it still does

    // The slot still occupies its byte, so everything appended behind it lands where
    // the reader expects. A v39 field is the nearest one to prove that with.
    SaveId sp; std::strcpy(sp.id, "malbear");
    a.raisedCreatures.push_back(sp);
    blob = serializeSave(a);
    SaveData out2;
    CHECK(deserializeSave(blob, out2));
    CHECK(out2.raisedCreatures.size() == 1);
    CHECK(std::strcmp(out2.raisedCreatures[0].id, "malbear") == 0);
}

// A save is the engine's biggest allocation and it fires off a timer, so it can land
// while something else holds the heap. On the device that is not a failed save — the
// Arduino build has exceptions off, so a bad_alloc resets the board and loses the very
// state being written. The guard has to refuse the attempt, and it has to still say
// yes where nothing reports heap at all, or the host build would never save.
void test_save_defers_when_the_heap_is_too_low() {
    Game g{StartMode::Hatched};
    CHECK(g.heapHeadroom() == 0);          // no probe wired
    CHECK(g.saveHasHeadroom());            // ...so nothing is ever held back

    // A Game that has never written still owes the blob buffer its one allocation, so
    // until that is paid every save is measured against the higher of the two bars.
    CHECK(g.saveHeapFloor() == kSaveGrowHeapFloorBytes);
    g.setHeapProbe([]() -> uint32_t { return kSaveGrowHeapFloorBytes - 1; });
    CHECK(!g.saveHasHeadroom());           // one byte under is under

    g.setHeapProbe([]() -> uint32_t { return kSaveGrowHeapFloorBytes; });
    CHECK(g.saveHasHeadroom());            // the floor itself is enough

    // That bar has to clear the reservation with room to spare, because building the
    // SaveData allocates BEFORE the buffer is sized — a floor that only covered the
    // reservation leaves nothing for the SaveData standing next to it.
    CHECK(kSaveGrowHeapFloorBytes > kSaveReserveBytes);
    // The everyday bar sits below it, which is the whole point of there being two: a
    // save writing into a buffer the engine already owns has no large allocation left
    // to clear, so it must not be turned away on a heap that could not spare a fresh one.
    CHECK(kSaveHeapFloorBytes < kSaveGrowHeapFloorBytes);

    // A refusal is COUNTED, because it is otherwise unobservable: the allocation that
    // triggered it is freed on the way out, so anything sampling the heap afterwards
    // sees a healthy number. Without the count there is no way to tell a guard that
    // worked from a device that happened to have room.
    MemSaveStore store;
    Game low(StartMode::Hatched, "paypup", &store);
    low.setHeapProbe([]() -> uint32_t { return 1024; });
    CHECK(low.savesDeferred() == 0);
    low.setApEnabled(true);                            // a change worth persisting
    for (int i = 1; i <= 40; ++i) low.tick(i * 1000);  // past debounce and autosave
    CHECK(low.savesDeferred() > 0);                    // turned away, not attempted
    CHECK(low.savesDeferredLowMark() == 1024);         // and it recorded how bad it got

    // ...and it is a DEFERRAL, not a drop: the moment there is room, the write that
    // was held back lands, carrying the change that was waiting on it.
    low.setHeapProbe([]() -> uint32_t { return kSaveGrowHeapFloorBytes; });
    low.tick(50000);
    CHECK(low.savesDeferred() == 0);
    SaveData back;
    CHECK(deserializeSave(store.bytes(), back));
    CHECK(back.apEnabled);

    // That write bought the blob buffer, so every save after it writes into memory the
    // engine already holds and is judged on the everyday floor instead. This is what
    // lets a save land while the radio owns the heap: the SAME reading that turned the
    // first one away is now enough — and the write still has to actually LAND, because
    // a guard that quietly stopped saving would pass every did-it-crash check there is.
    CHECK(low.saveHeapFloor() == kSaveHeapFloorBytes);
    low.setHeapProbe([]() -> uint32_t { return kSaveHeapFloorBytes; });
    CHECK(low.saveHasHeadroom());
    low.setLinkEnabled(true);
    for (int i = 51; i <= 90; ++i) low.tick(i * 1000);
    CHECK(low.savesDeferred() == 0);
    SaveData after;
    CHECK(deserializeSave(store.bytes(), after));
    CHECK(after.linkEnabled);
}

// A write the STORE refused is not a write. Counting it as one is what makes the
// failure silent: the flag clears, nothing retries, and a reboot later the blob on
// flash is older than the state that produced it — which is the one symptom that has no
// other explanation, a pet replaced by the one before it with nothing in between having
// looked wrong. So the refusal has to survive as an obligation.
void test_a_refused_write_is_not_a_save() {
    RefusingSaveStore store;
    Game g(StartMode::Hatched, "paypup", &store);
    uint32_t t = 0;
    g.setApEnabled(true);
    for (int i = 1; i <= 5; ++i) g.tick(t += 1000);
    SaveData landed;
    CHECK(deserializeSave(store.bytes(), landed));       // a baseline reached "flash"
    CHECK(landed.apEnabled);
    CHECK(g.saveWritesFailed() == 0);

    // Now the medium starts refusing, and the state moves on without it.
    store.refuse = true;
    g.setLinkEnabled(true);
    CHECK(!g.saveNow());                                 // reported, not swallowed
    CHECK(g.saveWritesFailed() > 0);
    SaveData stale;
    CHECK(deserializeSave(store.bytes(), stale));
    CHECK(!stale.linkEnabled);                           // flash is behind RAM: the revert

    // It keeps trying rather than giving up — and NOT once per beat. A store that just
    // said no is not one to hammer, so the retries are paced by the same debounce every
    // other write is.
    const int before = store.attempts;
    g.tick(t += 10);
    CHECK(store.attempts == before);                     // inside the debounce window
    for (int i = 1; i <= 5; ++i) g.tick(t += kSaveDebounceMs);
    CHECK(store.attempts > before);

    // ...and the moment it accepts, the state stranded in RAM lands, which is the whole
    // point of having kept the obligation.
    store.refuse = false;
    for (int i = 1; i <= 3; ++i) g.tick(t += kSaveDebounceMs);
    CHECK(g.saveWritesFailed() == 0);
    CHECK(g.saveNow());
    SaveData caught;
    CHECK(deserializeSave(store.bytes(), caught));
    CHECK(caught.linkEnabled);
}

// The reported shape of it, end to end. A hatch persists IMMEDIATELY, and that one call
// is all that stands between a new pet and a reboot: nothing in completeHatch marks the
// save dirty on its own — installPet doesn't, and the first-hatch achievement has
// already fired by the second egg — so a write turned away there used to leave the pet
// in RAM only. It is persistSave itself that now owes the retry, which is what makes
// this true of every "persist immediately" call site rather than the ones that
// remembered.
void test_a_hatch_the_store_refused_still_reaches_flash() {
    RefusingSaveStore store;
    Game g(StartMode::FreshHatch, "paypup", &store);
    pickFirstEggLine(g);
    uint32_t t = 0;
    g.tick(t += 1000);
    // The egg's own persist has landed by now, so "flash" holds the pet the device
    // would come back as — the previous one, in the report.
    SaveData onFlash;
    CHECK(deserializeSave(store.bytes(), onFlash));
    const std::string previous = onFlash.activeId;
    CHECK(!previous.empty());

    store.refuse = true;                                 // the medium starts saying no
    g.tick(t += kBootHatchMs + kHeartbeatMs);            // the clock runs out -> hatch
    CHECK(g.pet() != nullptr && g.pet()->stage == Stage::Process);
    const std::string hatched = g.pet()->id;
    CHECK(hatched != previous);                          // RAM has moved on
    CHECK(g.saveWritesFailed() > 0);                     // the hatch's own write bounced
    SaveData still;
    CHECK(deserializeSave(store.bytes(), still));
    CHECK(previous == still.activeId);                   // a reboot here = the old pet

    // The pet is owed a write, so the next store that says yes gets it — no further
    // player action, and no waiting out the 30s autosave.
    store.refuse = false;
    g.tick(t += kSaveDebounceMs);
    SaveData back;
    CHECK(deserializeSave(store.bytes(), back));
    CHECK(hatched == back.activeId);
    CHECK(g.saveWritesFailed() == 0);
}

// The three radio consents are independent: none of them may imply another, so
// setting one must never move the others. Reaching the 'net is deliberately NOT
// among them — it has no standing consent to be independent of.
void test_radio_consents_are_independent() {
    Game g{StartMode::Hatched};
    CHECK(!g.linkEnabled() && !g.apEnabled() && !g.netScanEnabled());  // default OFF

    g.setLinkEnabled(true);
    CHECK(g.linkEnabled());
    CHECK(!g.apEnabled());        // announcing never implies hosting
    CHECK(!g.netScanEnabled());   // nor listening

    g.setApEnabled(true);
    CHECK(g.linkEnabled());       // and hosting never revokes announcing
}

// Nothing persisted may leave this device able to reach the 'net. The association
// is raised by a running job and by nothing else, so it cannot outlive the job —
// and there is no saved bit left that could smuggle one across a reboot.
void test_net_access_needs_a_live_job() {
    Game g{StartMode::Hatched};
    g.setNetProvisioned(true);            // somewhere to go...
    g.setUpdateSourceKnown(true);
    CHECK(!g.netConnectWanted());         // ...and still nothing wants the radio

    g.requestUpdateCheck();
    CHECK(g.netConnectWanted());          // only the job raises it

    UpdateStatus done; done.state = UpdateState::UpToDate;
    g.setUpdateStatus(done);
    CHECK(!g.netConnectWanted());         // and it goes back the moment that ends

    // A fresh device carrying a save written when the standing opt-in still existed
    // reads that slot as reserved, so it lands off the 'net like any other.
    SaveData d; std::strcpy(d.activeId, "paypup"); d.generation = 1;
    d.netReserved = true;
    SaveData out;
    CHECK(deserializeSave(serializeSave(d), out));
    CHECK(!out.netReserved);
}

// Walk the CFG list to the UPDATES row and open it.
static void openUpdatesScreen(Game& g) {
    enterCfgTarget(g, CfgScreen::Update);
    CHECK(g.nav() == Game::Nav::Detail && g.cfgScreen() == CfgScreen::Update);
}

// A check needs a network to reach through and somewhere to look — setup, not
// permission. Missing either, the ask is refused outright rather than starting a
// job that would sit holding the radio with nothing to fetch.
void test_update_check_requires_network_and_source() {
    Game g{StartMode::Hatched};
    openUpdatesScreen(g);

    CHECK(!g.updateCheckReady());
    g.onButton(press(Button::B));            // CHECK NOW, with nothing set up
    CHECK(!g.updateJobLive());
    CHECK(g.updateStatus().state == UpdateState::Idle);

    g.setNetProvisioned(true);               // somewhere to go, nowhere to look
    CHECK(!g.updateCheckReady());
    g.requestUpdateCheck();
    CHECK(!g.updateJobLive());

    g.setUpdateSourceKnown(true);
    CHECK(g.updateCheckReady());
    g.requestUpdateCheck();
    CHECK(g.updateJobLive());
    CHECK(g.updateStatus().state == UpdateState::Checking);
}

// THE seam. A check is held by the JOB, not by a screen — a transfer that died the
// moment the menu collapsed could never finish — so it keeps the radio until it
// reaches a terminal outcome, and then hands it straight back.
void test_update_job_holds_the_radio_past_the_screen() {
    Game g{StartMode::Hatched};
    g.setNetProvisioned(true);
    g.setUpdateSourceKnown(true);

    openUpdatesScreen(g);
    g.onButton(press(Button::B));            // CHECK NOW
    CHECK(g.updateJobLive());
    CHECK(g.netConnectWanted());             // the job is what raises the radio

    // Leave the screen. Nothing nav-derived is holding the radio, so if the job
    // weren't what raises it, the fetch would be cut off mid-transfer.
    tapC(g);
    CHECK(g.cfgScreen() != CfgScreen::Update || g.nav() != Game::Nav::Detail);
    CHECK(g.updateJobLive());
    CHECK(g.netConnectWanted());
    CHECK(g.radioScreenOpen());              // ...so nothing may sleep the panel either

    // A terminal outcome ends the job and releases the radio — the only thing that
    // does, since no screen is watching it.
    UpdateStatus done;
    done.state = UpdateState::Available;
    done.firmwareNewer = true;
    std::strcpy(done.firmwareVersion, "0.4.2");
    g.setUpdateStatus(done);
    CHECK(!g.updateJobLive());
    CHECK(!g.netConnectWanted());

    // The verdict survives being walked away from: it is waiting on the way back.
    CHECK(g.updateStatus().state == UpdateState::Available);
    CHECK(g.updateStatus().firmwareNewer && !g.updateStatus().webNewer);

    // "Still working" is not terminal — that one keeps the radio.
    UpdateStatus busy;
    busy.state = UpdateState::Checking;
    g.requestUpdateCheck();
    g.setUpdateStatus(busy);
    CHECK(g.updateJobLive() && g.netConnectWanted());
}

// UPDATES is a screen you WAIT in front of, so the global 5s menu collapse must not
// run there. Both halves of the wait are longer than that on their own: a check is an
// association plus a fetch, and the verdict it leaves is then read and acted on. The
// job holds the radio by itself (above) — this is about the screen still being there
// when the answer arrives, and staying long enough to press INSTALL on it.
void test_updates_screen_outlives_the_menu_idle_timer() {
    Game g{StartMode::Hatched};
    g.setNetProvisioned(true);
    g.setUpdateSourceKnown(true);
    uint32_t t = 0;

    openUpdatesScreen(g);
    g.onButton(press(Button::B));                 // CHECK NOW
    CHECK(g.updateJobLive());

    // The device tier is still associating/fetching, and nobody is touching buttons.
    for (int i = 0; i < 4; ++i) g.tick(t += kAutoDefocusMs);
    CHECK(g.updateScreenOpen());                  // the screen is still there to report to

    UpdateStatus done;
    done.state = UpdateState::Available;
    done.firmwareNewer = true;
    std::strcpy(done.firmwareVersion, "0.4.2");
    g.setUpdateStatus(done);
    CHECK(!g.updateJobLive());

    // ...and the verdict survives being read: nothing is holding the radio now, so
    // this is the window the standard budget used to close in five seconds.
    for (int i = 0; i < 4; ++i) g.tick(t += kAutoDefocusMs);
    CHECK(g.updateScreenOpen());
    CHECK(g.updateStatus().state == UpdateState::Available);

    // Not forever, though — an abandoned screen still collapses on the long budget.
    g.tick(t += kRadioScreenDefocusMs);
    CHECK(!g.updateScreenOpen());
    CHECK(g.nav() == Game::Nav::Idle);
}

// A latched job must always reach a terminal outcome, because nothing else will
// ever take the radio off it. The association never coming up is the way that most
// plausibly happens — a passphrase changed, the router out of range — so a failed
// join has to end the job and say so, not leave it claiming to check forever.
void test_update_job_dies_when_the_join_fails() {
    Game g{StartMode::Hatched};
    g.setNetProvisioned(true);
    g.setUpdateSourceKnown(true);
    g.requestUpdateCheck();
    CHECK(g.updateJobLive());
    CHECK(g.netStatus().state == NetState::Connecting);  // joining is step one

    NetStatus failed; failed.state = NetState::Failed;
    g.setNetStatus(failed);
    CHECK(!g.updateJobLive());
    CHECK(!g.netConnectWanted());
    CHECK(g.updateStatus().state == UpdateState::Failed);
    CHECK(g.updateStatus().fail == UpdateFail::NoNetwork);  // named, not blank

    // A second ask while one is already in flight doesn't restart it or wipe the
    // Checking state out from under the device tier.
    g.requestUpdateCheck();
    UpdateStatus busy;
    busy.state = UpdateState::Checking;
    g.setUpdateStatus(busy);
    g.requestUpdateCheck();
    CHECK(g.updateJobLive());
    CHECK(g.updateStatus().state == UpdateState::Checking);
}

// Set the game up as if a check had just reported both artifacts newer.
static void seedAvailableUpdate(Game& g) {
    g.setNetProvisioned(true);
    g.setUpdateSourceKnown(true);
    UpdateStatus found;
    found.state = UpdateState::Available;
    found.firmwareNewer = true;
    std::strcpy(found.firmwareVersion, "0.4.2");
    found.webNewer = true;
    std::strcpy(found.webVersion, "0.4.0");
    g.requestUpdateCheck();
    g.setUpdateStatus(found);
}

// An install writes something, so it may only ever run against what a check
// actually confirmed. A target the last check didn't report as newer is refused
// outright — otherwise a stale screen, or a row that moved under the cursor, could
// flash an artifact the device never verified exists.
void test_update_install_needs_a_confirmed_finding() {
    Game g{StartMode::Hatched};
    g.setNetProvisioned(true);
    g.setUpdateSourceKnown(true);

    // Nothing has been checked: every target is refused.
    g.requestUpdateInstall(UpdateTarget::Firmware);
    CHECK(!g.updateJobLive());
    g.requestUpdateInstall(UpdateTarget::Web);
    CHECK(!g.updateJobLive());
    g.requestUpdateInstall(UpdateTarget::None);   // "install the check" is not a thing
    CHECK(!g.updateJobLive());

    // A check that found only the firmware doesn't authorise the web bundle.
    UpdateStatus fwOnly;
    fwOnly.state = UpdateState::Available;
    fwOnly.firmwareNewer = true;
    std::strcpy(fwOnly.firmwareVersion, "0.4.2");
    g.requestUpdateCheck();
    g.setUpdateStatus(fwOnly);
    // CHECK NOW + the one it found + FLASH OVER USB, which is always last.
    CHECK(updateCheckRows(fwOnly) == 3);
    CHECK(updateCheckRowTarget(fwOnly, 1) == UpdateTarget::Firmware);
    CHECK(updateCheckRowTarget(fwOnly, 2) == UpdateTarget::None);
    CHECK(updateCheckRowKind(fwOnly, 2) == UpdateRowKind::FlashQr);

    g.requestUpdateInstall(UpdateTarget::Web);
    CHECK(!g.updateJobLive());

    g.requestUpdateInstall(UpdateTarget::Firmware);
    CHECK(g.updateJobLive());
    CHECK(g.updateJobTarget() == UpdateTarget::Firmware);
    CHECK(g.installStatus().state == InstallState::Downloading);

    // One job at a time: a second ask can't start a competing download over the
    // one already writing to the flash slot.
    g.requestUpdateInstall(UpdateTarget::Firmware);
    CHECK(g.updateJobTarget() == UpdateTarget::Firmware);
    g.requestUpdateCheck();
    CHECK(g.updateJobTarget() == UpdateTarget::Firmware);
}

// Walking the actual buttons: an install is always two deliberate yeses, and the
// confirm starts on NO. Pressing B twice — the reflex — must install nothing.
void test_update_install_takes_two_yeses() {
    Game g{StartMode::Hatched};
    seedAvailableUpdate(g);
    openUpdatesScreen(g);

    UpdateStatus found = g.updateStatus();
    CHECK(updateCheckRows(found) == 4);           // check + firmware + web + flasher

    g.onButton(press(Button::A));                 // row 0 -> the firmware row
    g.onButton(press(Button::B));                 // opens the confirm, installs nothing
    CHECK(!g.updateJobLive());

    g.onButton(press(Button::B));                 // B again lands on NO — still nothing
    CHECK(!g.updateJobLive());

    // And C out of the confirm is equally inert.
    g.onButton(press(Button::B));
    tapC(g);
    CHECK(!g.updateJobLive());
    CHECK(g.nav() == Game::Nav::Detail);          // C left the confirm, not the screen

    // A to YES, then B: only now does anything start.
    g.onButton(press(Button::B));
    g.onButton(press(Button::A));
    g.onButton(press(Button::B));
    CHECK(g.updateJobLive());
    CHECK(g.updateJobTarget() == UpdateTarget::Firmware);

    // ...and like a check, it holds the radio past the screen it was started from.
    tapC(g);
    CHECK(g.updateJobLive() && g.netConnectWanted());

    InstallStatus done;
    done.state = InstallState::Done;
    done.target = UpdateTarget::Firmware;
    done.restartNeeded = true;
    g.setInstallStatus(done);
    CHECK(!g.updateJobLive());
    CHECK(!g.netConnectWanted());
    CHECK(g.installStatus().state == InstallState::Done);
}

// The USB flasher's address is DERIVED from the manifest one, so a device pointed
// at a fork or a laptop offers that host's flasher without being told twice.
void test_flasher_url_follows_the_publish_host() {
    char out[192];

    CHECK(updateFlasherUrl("https://antvena.github.io/Malwarium/manifest.json",
                           out, sizeof(out)));
    CHECK(std::strcmp(out, "https://antvena.github.io/Malwarium/flash/") == 0);

    // A laptop publish: same rule, different host, no scheme assumption.
    CHECK(updateFlasherUrl("http://192.168.1.50:8000/manifest.json", out, sizeof(out)));
    CHECK(std::strcmp(out, "http://192.168.1.50:8000/flash/") == 0);

    // Only the FILENAME is replaced — a nested publish keeps its whole path.
    CHECK(updateFlasherUrl("https://box.local/a/b/manifest.json", out, sizeof(out)));
    CHECK(std::strcmp(out, "https://box.local/a/b/flash/") == 0);

    // A bare host has no filename to replace, and earns the slash it lacks. The
    // "//" of the scheme is not a path separator, which is what this pins.
    CHECK(updateFlasherUrl("https://box.local", out, sizeof(out)));
    CHECK(std::strcmp(out, "https://box.local/flash/") == 0);

    // Nowhere to look, or nowhere to put it: no address, and `out` says so rather
    // than holding a half-built one.
    CHECK(!updateFlasherUrl("", out, sizeof(out)));
    CHECK(out[0] == '\0');
    CHECK(!updateFlasherUrl(nullptr, out, sizeof(out)));
    char tiny[8];
    CHECK(!updateFlasherUrl("https://box.local/manifest.json", tiny, sizeof(tiny)));
    CHECK(tiny[0] == '\0');
}

// The flasher row rides at the BOTTOM of a list whose middle changes, and it is
// the one row that works without a network — so B opens the code even when a
// check can't run, which is exactly when someone needs it.
void test_flasher_row_is_last_and_needs_no_network() {
    Game g{StartMode::Hatched};
    // A source but no stored network: a check is impossible, the flasher is not.
    g.setUpdateManifestUrl("https://antvena.github.io/Malwarium/manifest.json");
    CHECK(!g.updateCheckReady());
    openUpdatesScreen(g);

    UpdateStatus idle;
    CHECK(updateCheckRows(idle) == 2);                        // check + flasher
    CHECK(updateCheckRowKind(idle, 0) == UpdateRowKind::Check);
    CHECK(updateCheckRowKind(idle, 1) == UpdateRowKind::FlashQr);

    g.onButton(press(Button::A));                             // onto the flasher row
    g.onButton(press(Button::B));
    CHECK(g.cfgScreen() == CfgScreen::UpdateQr);
    CHECK(g.qrScreenActive());                                // holds the panel awake
    CHECK(!g.updateJobLive());                                // and asks for no radio

    tapC(g);                             // back to UPDATES, not out
    CHECK(g.cfgScreen() == CfgScreen::Update);
    CHECK(g.nav() == Game::Nav::Detail);
    CHECK(!g.qrScreenActive());

    // With nowhere to look there is nothing to encode, so the row is inert rather
    // than opening a screen that would have to apologise.
    Game blank{StartMode::Hatched};
    openUpdatesScreen(blank);
    blank.onButton(press(Button::A));
    blank.onButton(press(Button::B));
    CHECK(blank.cfgScreen() == CfgScreen::Update);
}

// A download in progress is not terminal; only Done and Failed hand the radio back.
void test_update_install_progress_is_not_terminal() {
    Game g{StartMode::Hatched};
    seedAvailableUpdate(g);
    g.requestUpdateInstall(UpdateTarget::Web);
    CHECK(g.updateJobLive());

    for (InstallState s : {InstallState::Downloading, InstallState::Verifying,
                           InstallState::Installing}) {
        InstallStatus st;
        st.state = s;
        st.target = UpdateTarget::Web;
        st.received = 1024;
        st.total = 4096;
        g.setInstallStatus(st);
        CHECK(g.updateJobLive());
        CHECK(g.netConnectWanted());
    }

    InstallStatus bad;
    bad.state = InstallState::Failed;
    bad.fail = InstallFail::Corrupt;
    g.setInstallStatus(bad);
    CHECK(!g.updateJobLive());
    CHECK(!g.netConnectWanted());
    CHECK(g.installStatus().fail == InstallFail::Corrupt);

    // An install raises its own association, so it dies the same way a check does
    // when that join never comes up — nothing else would take the radio off it.
    g.requestUpdateInstall(UpdateTarget::Web);
    CHECK(g.updateJobLive());
    CHECK(g.netStatus().state == NetState::Connecting);
    NetStatus noJoin; noJoin.state = NetState::Failed;
    g.setNetStatus(noJoin);
    CHECK(!g.updateJobLive());
    CHECK(g.installStatus().state == InstallState::Failed);
    CHECK(g.installStatus().fail == InstallFail::NoNetwork);
}

// ---- Tar reader -----------------------------------------------------------

namespace {
// Build one tar header block. `size` is written as tar's NUL-terminated octal.
void tarHeader(uint8_t* block, const char* name, uint32_t size, char type) {
    std::memset(block, 0, kTarBlock);
    std::strncpy(reinterpret_cast<char*>(block), name, 99);
    std::snprintf(reinterpret_cast<char*>(block) + 124, 12, "%011o",
                  static_cast<unsigned>(size));
    block[156] = static_cast<uint8_t>(type);
}
}  // namespace

// An archive names its own destinations, so the name check is the whole security
// story of unpacking one. Anything that could climb out of the extraction root is
// rejected — never rewritten, because a name we had to fix is not one we understood.
void test_tar_rejects_escaping_names() {
    for (const char* bad : {"../evil", "web/../../evil", "/etc/passwd", "a/../../b",
                            "..", "web\\..\\evil", ""}) {
        CHECK(!tarPathSafe(bad));
    }
    for (const char* good : {"index.html", "assets/ICON_STAT.png", "data/pedia_data.js",
                             "a..b/c", "...", "deep/nested/path/file.txt"}) {
        CHECK(tarPathSafe(good));
    }

    // ...and the header parser enforces it, so a caller can't reach a file handle
    // with a name it never validated.
    uint8_t block[kTarBlock];
    TarEntry e;
    tarHeader(block, "../../boot/app.bin", 64, '0');
    CHECK(parseTarHeader(block, sizeof(block), &e));
    CHECK(e.kind == TarEntryKind::Invalid);
}

// The rest of the header: types, octal sizes, the terminating zero block, and the
// padding maths that keeps the stream cursor aligned.
void test_tar_reads_entries() {
    uint8_t block[kTarBlock];
    TarEntry e;

    tarHeader(block, "index.html", 1234, '0');
    CHECK(parseTarHeader(block, sizeof(block), &e));
    CHECK(e.kind == TarEntryKind::File);
    CHECK(std::strcmp(e.name, "index.html") == 0);
    CHECK(e.size == 1234);

    tarHeader(block, "assets/", 0, '5');          // a directory carries no data
    CHECK(parseTarHeader(block, sizeof(block), &e));
    CHECK(e.kind == TarEntryKind::Directory && e.size == 0);

    tarHeader(block, "link", 0, '2');             // a symlink: nothing the bundle uses
    CHECK(parseTarHeader(block, sizeof(block), &e));
    CHECK(e.kind == TarEntryKind::Skip);

    // A type byte of 0 is an old-style regular file, not a broken header.
    tarHeader(block, "style.css", 512, '\0');
    CHECK(parseTarHeader(block, sizeof(block), &e));
    CHECK(e.kind == TarEntryKind::File && e.size == 512);

    // A size field that isn't octal is a broken header — reporting it as a
    // zero-length file would leave the stream cursor pointing at nothing real.
    tarHeader(block, "bad.bin", 0, '0');
    std::memcpy(block + 124, "99999999999", 11);  // 8 and 9 are not octal digits
    CHECK(parseTarHeader(block, sizeof(block), &e));
    CHECK(e.kind == TarEntryKind::Invalid);

    // Real tar output is not always the tidy form `make pedia-tar` produces — a
    // bundle built with `tar -C dir .` prefixes every name with "./", and that has
    // to extract to the same place rather than being rejected as suspicious.
    tarHeader(block, "./assets/ICON_STAT.png", 300, '0');
    CHECK(parseTarHeader(block, sizeof(block), &e));
    CHECK(e.kind == TarEntryKind::File);

    std::memset(block, 0, sizeof(block));
    CHECK(parseTarHeader(block, sizeof(block), &e));
    CHECK(e.kind == TarEntryKind::End);

    // Data is padded up to a whole block; an exact multiple gains nothing.
    CHECK(tarDataSpan(0) == 0);
    CHECK(tarDataSpan(1) == kTarBlock);
    CHECK(tarDataSpan(kTarBlock) == kTarBlock);
    CHECK(tarDataSpan(kTarBlock + 1) == 2 * kTarBlock);

    // A short read is a truncated archive, not a short entry.
    CHECK(!parseTarHeader(block, kTarBlock - 1, &e));
}

// ---- Update manifest ------------------------------------------------------

namespace {
// A well-formed two-artifact manifest, in the published shape. `parse` takes a
// length, so these helpers pass strlen rather than assuming NUL-termination —
// the real input arrives as an HTTP body.
const char* kGoodManifest =
    "{\"artifacts\":["
    "{\"id\":\"firmware\",\"version\":\"0.4.2\",\"code\":402,"
    "\"url\":\"https://example.invalid/mal-0.4.2.bin\",\"size\":1758361,"
    "\"sha256\":\"" "0123456789abcdef0123456789abcdef"
                    "0123456789abcdef0123456789abcdef" "\"},"
    "{\"id\":\"web\",\"version\":\"0.4.0\",\"code\":400,"
    "\"url\":\"https://example.invalid/web-0.4.0.tar\",\"size\":291840,"
    "\"sha256\":\"" "fedcba9876543210fedcba9876543210"
                    "fedcba9876543210fedcba9876543210" "\"}"
    "]}";

ManifestParse parseOf(UpdateManifest& m, const char* s) {
    return m.parse(s, std::strlen(s));
}
}  // namespace

// The published shape round-trips, and rows are reachable by what they ARE rather
// than where they sit — a later publish reordering the list must not change which
// artifact the firmware check reads.
void test_manifest_parses_published_shape() {
    UpdateManifest m;
    CHECK(parseOf(m, kGoodManifest) == ManifestParse::Ok);
    CHECK(m.count() == 2);

    const UpdateArtifact* fw = m.byId("firmware");
    CHECK(fw != nullptr);
    CHECK(std::strcmp(fw->version, "0.4.2") == 0);
    CHECK(fw->code == 402);
    CHECK(fw->size == 1758361);
    CHECK(std::strcmp(fw->url, "https://example.invalid/mal-0.4.2.bin") == 0);
    CHECK(fw->sha256[0] == 0x01 && fw->sha256[31] == 0xef);

    const UpdateArtifact* web = m.byId("web");
    CHECK(web != nullptr && web->code == 400);
    CHECK(web->sha256[0] == 0xfe && web->sha256[31] == 0x10);

    CHECK(m.byId("bootloader") == nullptr);   // absent, not row 0 by accident

    // Strictly newer, both directions, and never equal — an update prompt that
    // re-offers the build you're already on is one the operator learns to dismiss.
    CHECK(artifactIsNewer(*fw, 401));
    CHECK(!artifactIsNewer(*fw, 402));
    CHECK(!artifactIsNewer(*fw, 500));
}

// What actually arrives when the fetch goes wrong. A captive portal answering
// with a login page is the ordinary case, not the exotic one, so every shape here
// must be rejected outright rather than half-read into a plausible artifact.
void test_manifest_rejects_junk_and_unusable_rows() {
    UpdateManifest m;

    // Not JSON at all — the portal login page.
    CHECK(parseOf(m, "<!DOCTYPE html><html><body>Sign in</body></html>") ==
          ManifestParse::Malformed);
    CHECK(m.count() == 0);

    // Valid JSON, wrong document.
    CHECK(parseOf(m, "{\"error\":\"not found\"}") == ManifestParse::Malformed);

    // Truncated mid-download: cutting the good manifest anywhere must never yield
    // a usable row, whatever the cut lands in the middle of.
    const size_t full = std::strlen(kGoodManifest);
    for (size_t cut = 1; cut < full; ++cut) {
        UpdateManifest t;
        if (t.parse(kGoodManifest, cut) == ManifestParse::Ok) {
            CHECK(false);   // a prefix of the manifest parsed as complete
            break;
        }
    }

    // An empty publish is its own answer, distinct from a broken one.
    CHECK(parseOf(m, "{\"artifacts\":[]}") == ManifestParse::NoArtifacts);

    // Rows that cannot be VERIFIED are not installable: no hash, a short hash, a
    // non-hex hash, and a zero size each sink the parse rather than yielding a row
    // the downloader would happily flash without checking.
    CHECK(parseOf(m, "{\"artifacts\":[{\"id\":\"firmware\",\"version\":\"1\","
                     "\"code\":1,\"url\":\"https://x/y\",\"size\":10}]}") ==
          ManifestParse::Malformed);
    CHECK(parseOf(m, "{\"artifacts\":[{\"id\":\"firmware\",\"version\":\"1\","
                     "\"code\":1,\"url\":\"https://x/y\",\"size\":10,"
                     "\"sha256\":\"abcd\"}]}") == ManifestParse::Malformed);
    CHECK(parseOf(m, "{\"artifacts\":[{\"id\":\"firmware\",\"version\":\"1\","
                     "\"code\":1,\"url\":\"https://x/y\",\"size\":0,"
                     "\"sha256\":\"0123456789abcdef0123456789abcdef"
                     "0123456789abcdef0123456789abcdef\"}]}") ==
          ManifestParse::Malformed);

    // A URL too long for the row's buffer is REJECTED, never truncated — a URL cut
    // at the buffer edge still looks like a good row and would fetch the wrong thing.
    std::string longUrl = "{\"artifacts\":[{\"id\":\"firmware\",\"version\":\"1\","
                          "\"code\":1,\"size\":10,\"sha256\":\"0123456789abcdef"
                          "0123456789abcdef0123456789abcdef0123456789abcdef\","
                          "\"url\":\"https://example.invalid/";
    longUrl.append(200, 'a');
    longUrl += "\"}]}";
    CHECK(parseOf(m, longUrl.c_str()) == ManifestParse::Malformed);
}

// A device must survive meeting a manifest published by a LATER firmware: unknown
// fields, unknown keys of any length, and more rows than it can hold. Failing any
// of these would strand every device already in the field the first time the
// manifest grows.
void test_manifest_tolerates_a_newer_publish() {
    UpdateManifest m;
    CHECK(parseOf(m,
        "{\"schema\":3,\"generated\":\"2026-07-26T00:00:00Z\","
        "\"artifacts\":[{"
        "\"id\":\"firmware\",\"version\":\"0.5.0\",\"code\":500,"
        "\"url\":\"https://x/y.bin\",\"size\":42,"
        "\"sha256\":\"0123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdef\","
        "\"notes\":\"fixes the thing\",\"channels\":[\"beta\",\"stable\"],"
        "\"a_field_name_far_longer_than_any_key_this_build_knows\":{\"x\":1}"
        "}]}") == ManifestParse::Ok);
    CHECK(m.count() == 1);
    const UpdateArtifact* fw = m.byId("firmware");
    CHECK(fw != nullptr && fw->code == 500 && fw->size == 42);

    // More rows than fit: the ones that do are usable, and the caller is told.
    std::string many = "{\"artifacts\":[";
    for (int i = 0; i < UpdateManifest::kMaxArtifacts + 2; ++i) {
        if (i) many += ',';
        many += "{\"id\":\"a";
        many += static_cast<char>('0' + i);
        many += "\",\"version\":\"1\",\"code\":1,\"url\":\"https://x/y\",\"size\":1,"
                "\"sha256\":\"0123456789abcdef0123456789abcdef"
                "0123456789abcdef0123456789abcdef\"}";
    }
    many += "]}";
    CHECK(parseOf(m, many.c_str()) == ManifestParse::TooMany);
    CHECK(m.count() == UpdateManifest::kMaxArtifacts);
    CHECK(m.byId("a0") != nullptr);
}

// ---- Web-bundle version marker --------------------------------------------

namespace {
bool markerOf(VersionMarker& v, const char* s) {
    return parseVersionMarker(s, std::strlen(s), &v);
}
}  // namespace

// The stamp an installer writes must be the stamp the next check reads — these are
// the two halves of the same fact, and a device that writes one shape and reads
// another would re-offer a bundle it just installed, forever.
void test_version_marker_round_trips() {
    VersionMarker w;
    w.code = 402;
    std::strcpy(w.version, "0.4.2");

    char buf[kVersionMarkerCap];
    const size_t n = formatVersionMarker(w, buf, sizeof(buf));
    CHECK(n > 0 && n < sizeof(buf));

    VersionMarker r;
    CHECK(parseVersionMarker(buf, n, &r));
    CHECK(r.code == 402);
    CHECK(std::strcmp(r.version, "0.4.2") == 0);

    // The code is what gates, so it has to feed artifactIsNewer directly.
    UpdateArtifact a{};
    a.code = 500;
    CHECK(artifactIsNewer(a, r.code));
    a.code = 402;
    CHECK(!artifactIsNewer(a, r.code));

    // A buffer too small yields nothing rather than a truncated stamp: half of
    // "code=402" is still a parseable code, and a plausible wrong one.
    char tiny[6];
    CHECK(formatVersionMarker(w, tiny, sizeof(tiny)) == 0);
    CHECK(tiny[0] == '\0');
}

// A marker is read off a card that may have been written by a later publish, a
// different OS's line endings, or a person with a text editor. Everything that
// isn't a key this build knows is decoration.
void test_version_marker_tolerates_real_files() {
    VersionMarker v;

    // The shape web/VERSION ships: comments, CRLF, blank lines, loose spacing.
    CHECK(markerOf(v,
        "# Which web 'Pedia bundle this is.\r\n"
        "\r\n"
        "  code = 100  \r\n"
        "version=0.1.0\r\n"));
    CHECK(v.code == 100 && std::strcmp(v.version, "0.1.0") == 0);

    // A key from a publish this build predates is skipped, not fatal — the whole
    // point of a key=value file over a bare number.
    CHECK(markerOf(v, "code=7\nchannel=beta\nnotes=whatever=this=is\nversion=0.0.7\n"));
    CHECK(v.code == 7 && std::strcmp(v.version, "0.0.7") == 0);

    // A final line with no newline is still a line.
    CHECK(markerOf(v, "version=9.9.9\ncode=90909"));
    CHECK(v.code == 90909 && std::strcmp(v.version, "9.9.9") == 0);

    // A version too long to hold is dropped rather than clipped, so the prompt
    // says nothing instead of naming a version nobody published. The code — the
    // only field that decides anything — survives.
    CHECK(markerOf(v, "code=3\nversion=0.1.0-rc1-build-20260101\n"));
    CHECK(v.code == 3 && v.version[0] == '\0');
}

// Unlike the manifest, a marker that won't parse is not a failure to report — it
// reads as "unknown", which makes any published bundle newer and offers a
// re-install. Redundant, never wrong; the install then replaces the unknown.
void test_version_marker_absent_reads_as_unknown() {
    VersionMarker v;

    CHECK(!parseVersionMarker(nullptr, 0, &v));
    CHECK(v.code == 0);
    CHECK(!parseVersionMarker("", 0, &v));
    CHECK(v.code == 0);

    // An HTML page, a half-written file, a version with no code: all the same
    // answer, because none of them says what is installed.
    CHECK(!markerOf(v, "<!doctype html><title>404</title>"));
    CHECK(v.code == 0);
    CHECK(!markerOf(v, "version=0.4.2\n"));
    CHECK(v.code == 0 && v.version[0] == '\0');   // cleared, not half-filled
    CHECK(!markerOf(v, "code=\ncode=twelve\ncode=99999999999999\n"));
    CHECK(v.code == 0);

    // A garbled line must not clobber a good value read from another one.
    CHECK(markerOf(v, "code=12\ncode=NaN\n"));
    CHECK(v.code == 12);

    UpdateArtifact a{};
    a.code = 1;
    CHECK(artifactIsNewer(a, v.code) == false);   // 1 is not newer than 12
    VersionMarker unknown;
    CHECK(artifactIsNewer(a, unknown.code));      // ...but it is newer than nothing
}

// The QR carousel carries the setup step only while there is nothing stored, and A
// always walks past it — it must never stand between the operator and the 'Pedia.
void test_qr_setup_step_appears_only_until_provisioned() {
    Game g{StartMode::Hatched};
    CHECK(pediaQrPages(/*provisioned=*/false) == 3);   // join, set up, 'Pedia
    CHECK(pediaQrPages(/*provisioned=*/true) == 2);    // join, 'Pedia

    enterCfgTarget(g, CfgScreen::PediaAp);             // CFG -> RADIO -> PEDIA AP (OFF)
    g.onButton(press(Button::A));                      // OFF -> ON
    g.onButton(press(Button::B));                      // apply -> the QR screen
    CHECK(g.cfgScreen() == CfgScreen::PediaQr && g.pediaQrPage() == 0);

    // Unprovisioned: three steps, wrapping back to the join code.
    g.onButton(press(Button::A)); CHECK(g.pediaQrPage() == 1);   // set up
    g.onButton(press(Button::A)); CHECK(g.pediaQrPage() == 2);   // 'Pedia
    g.onButton(press(Button::A)); CHECK(g.pediaQrPage() == 0);

    // Once a network is stored the setup step drops out, and the cursor can't be
    // left pointing past the end of the shorter carousel.
    g.setNetProvisioned(true);
    g.onButton(press(Button::A)); CHECK(g.pediaQrPage() == 1);   // 'Pedia
    g.onButton(press(Button::A)); CHECK(g.pediaQrPage() == 0);
}

// How an attempt ends decides whether the radio goes back. There is no screen
// holding it — the JOB is, so the outcomes are read through the job's lifetime.
void test_net_attempt_outcomes() {
    Game g{StartMode::Hatched};
    g.setNetProvisioned(true);
    g.setUpdateSourceKnown(true);
    g.requestUpdateCheck();
    CHECK(g.netConnectWanted());

    NetStatus failed; failed.state = NetState::Failed;
    g.setNetStatus(failed);
    CHECK(!g.netConnectWanted());                     // radio handed back
    CHECK(g.netStatus().state == NetState::Failed);   // ...but the screen says why

    // Online is NOT terminal: the job holds the link it just raised, and only its
    // own outcome gives the radio back.
    g.requestUpdateCheck();
    NetStatus online; online.state = NetState::Online;
    std::strcpy(online.ip, "192.168.1.9");
    g.setNetStatus(online);
    CHECK(g.netConnectWanted());                      // an online link is held
    CHECK(std::strcmp(g.netStatus().ip, "192.168.1.9") == 0);

    UpdateStatus done; done.state = UpdateState::UpToDate;
    g.setUpdateStatus(done);
    CHECK(!g.netConnectWanted());                     // the job ended, so the link does
}

// v21 round-trip: the per-pet care-mistake shield + once-per-lifetime item gates,
// and fragAmountTier. (The v21 tail's dedup-mask field was removed as of v33 —
// see test_save_v33_drops_network_dedup below for that boundary.)
void test_save_v21_shield_roundtrip() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.mistakeShieldActive = 1;
    a.shieldItemConsumed = 1;
    a.yubiConsumed = 1;
    a.fragAmountTier = 3;
    auto blob = serializeSave(a);
    SaveData out;
    CHECK(deserializeSave(blob, out));
    CHECK(out.mistakeShieldActive == 1);
    CHECK(out.shieldItemConsumed == 1);
    CHECK(out.yubiConsumed == 1);
    CHECK(out.fragAmountTier == 3);
}

// v22 field round-trip: fragTriggerTier survives a full serialize/deserialize
// alongside the v21 fields it sits behind.
void test_save_v22_frag_trigger_roundtrip() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.mistakeShieldActive = 1;
    a.fragAmountTier = 3;
    a.fragTriggerTier = 4;
    SaveData rt;
    CHECK(deserializeSave(serializeSave(a), rt));
    CHECK(rt.mistakeShieldActive == 1);
    CHECK(rt.fragAmountTier == 3);
    CHECK(rt.fragTriggerTier == 4);
}

// v23 field round-trip: both one-time shop unlocks survive a full
// serialize/deserialize alongside the v22 field ahead of them.
void test_save_v23_shop_unlocks_roundtrip() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.fragTriggerTier = 5;
    a.itemTabsUnlocked = 1;
    a.bulkOpenUnlocked = 1;
    SaveData rt;
    CHECK(deserializeSave(serializeSave(a), rt));
    CHECK(rt.fragTriggerTier == 5);
    CHECK(rt.itemTabsUnlocked == 1);
    CHECK(rt.bulkOpenUnlocked == 1);
}

// The 'Pedia AP runtime toggle + QR integration (default OFF): turning it ON
// through the RADIO group's "PEDIA AP" row leads to the QR screen (connect prompt),
// turning it OFF backs out to RADIO, and the opt-in survives a reboot (save v20).
void test_ap_toggle_via_cfg_and_persist() {
    MemSaveStore store;
    {
        Game g(StartMode::Hatched, "paypup", &store);
        CHECK(!g.apEnabled());                    // default OFF (radio silent at boot)

        // Open PEDIA AP once; every exit below lands back on RADIO with the cursor
        // still on this row, so B re-opens it without re-navigating.
        enterCfgTarget(g, CfgScreen::PediaAp);    // CFG -> RADIO -> PEDIA AP (OFF)
        CHECK(g.nav() == Game::Nav::Detail);
        g.onButton(press(Button::A));             // OFF -> ON
        g.onButton(press(Button::B));             // apply ON -> QR
        CHECK(g.apEnabled());
        CHECK(g.nav() == Game::Nav::Detail && g.cfgScreen() == CfgScreen::PediaQr);
        tapC(g);             // QR -> back to RADIO
        CHECK(g.cfgScreen() == CfgScreen::Radio);

        // "See the QR" while already ON: re-open the row, apply ON (no change) -> QR.
        g.onButton(press(Button::B));             // re-open (pick starts on ON)
        g.onButton(press(Button::B));             // apply ON -> QR again
        CHECK(g.cfgScreen() == CfgScreen::PediaQr);
        tapC(g);             // back to RADIO

        // Turning it OFF has no QR — it backs straight to RADIO.
        g.onButton(press(Button::B));             // re-open (pick starts on ON)
        g.onButton(press(Button::A));             // ON -> OFF
        g.onButton(press(Button::B));             // apply OFF -> RADIO (no QR)
        CHECK(!g.apEnabled());
        CHECK(g.cfgScreen() == CfgScreen::Radio);
        tapC(g);             // RADIO -> the CFG list
        CHECK(g.nav() == Game::Nav::Submenu);

        g.setApEnabled(true);                     // leave ON for the reboot check
        g.tick(kSaveAutosaveMs + kHeartbeatMs);
    }
    {
        // Reboot over the same store: the AP opt-in is restored.
        Game g(StartMode::FreshHatch, "paypup", &store);
        pickFirstEggLine(g);
        CHECK(g.apEnabled());
    }
}

// PEDIA QR: two pages (join-network / 'Pedia-URL) cycled with A, exited with C.
// The engine 5s auto-defocus is SUSPENDED on the QR page (scanning + joining takes
// longer than the idle window) and resumes the moment the player exits with C.
void test_pedia_qr_two_pages_no_timeout() {
    Game g{StartMode::Hatched};
    // A settled device: with a home network already stored the carousel is its
    // two-step form (join, 'Pedia), which keeps this test about the TIMEOUT rather
    // than about how many setup steps happen to be in the list. The step list
    // itself is test_qr_setup_step_appears_only_until_provisioned's subject.
    g.setNetProvisioned(true);

    // Turn the AP on -> the QR page (starts on page 0, the join-network QR).
    enterCfgTarget(g, CfgScreen::PediaAp);        // CFG -> RADIO -> PEDIA AP (OFF)
    g.onButton(press(Button::A));                 // -> ON
    g.onButton(press(Button::B));                 // apply ON -> QR
    CHECK(g.qrScreenActive() && g.pediaQrPage() == 0);

    // A cycles the two pages without leaving the QR screen.
    g.onButton(press(Button::A));
    CHECK(g.qrScreenActive() && g.pediaQrPage() == 1);
    g.onButton(press(Button::A));
    CHECK(g.qrScreenActive() && g.pediaQrPage() == 0);

    // No timeout on the QR page: ticking well past the auto-defocus window keeps
    // the page up (the menu does NOT collapse to idle). tick() takes an absolute
    // clock, so run a monotonic `t`.
    uint32_t t = kAutoDefocusMs * 3 + kHeartbeatMs;
    g.tick(t);
    CHECK(g.qrScreenActive());                    // still on the QR page
    CHECK(g.nav() != Game::Nav::Idle);

    // Exit with C -> back to the RADIO group; the QR-page hold releases (this press
    // stamps lastInput at the current clock).
    tapC(g);
    CHECK(!g.qrScreenActive());
    CHECK(g.cfgScreen() == CfgScreen::Radio);

    // CFG screens never auto-defocus at all (the operator is mid-setup) — a long
    // idle tick leaves the RADIO group right where it was.
    t += kAutoDefocusMs * 2 + kHeartbeatMs;
    g.tick(t);
    CHECK(g.nav() == Game::Nav::Detail);
    CHECK(g.cfgScreen() == CfgScreen::Radio);
}
