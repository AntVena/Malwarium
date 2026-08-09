// main.cpp — ESP32-S3 device entry for Malwarium.
//
// The portable engine (src/core) is identical to the host build; only this
// thin tier differs. We mirror host/main.cpp's loop: render the 224 active
// canvas, present it behind the 8px bezel, and feed the three buttons into
// game.onButton(). Repaint stays event-driven (~4fps heartbeat + state change),
// matching the spec's render budget.
//
// Button roles (config.h is the authority): A = KEY_MINUS/GPIO0, which is also the
// BOOT/download strap (NEXT) · B = KEY_PWR (ACCEPT) · C = KEY_PLUS (CANCEL).
// A+C = the reserved meta chord (no-op pet-side).
//
// BRINGUP_PINSCAN (build flag): also scan the pin_finder candidate GPIOs and
// print any press to serial. Lets the first flash discover PLUS/PWR while the
// panel is already rendering, then it's compiled out for the real build.
#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_ota_ops.h>
#include <esp_sleep.h>

#include "core/app/game.h"
#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "dev_config.h"
#include "platform/esp32/ap_server.h"
#include "platform/esp32/battery_esp32.h"
#include "platform/esp32/display_esp32.h"
#include "platform/esp32/net_capture.h"
#include "platform/esp32/net_credentials.h"
#include "platform/esp32/net_update.h"
#include "platform/esp32/update_source.h"
#include "platform/esp32/web_bundle.h"
#include "version.h"
#include "platform/esp32/net_link.h"
#include "platform/esp32/net_sniff.h"
#include "platform/esp32/net_sta.h"
#include "platform/esp32/radio_arbiter.h"
#include "platform/esp32/save_store_esp32.h"
#include "platform/esp32/sd_esp32.h"

using namespace mal;

namespace {

Esp32Display display;
Game* game = nullptr;
Framebuffer* fb = nullptr;
NetSniff netSniff;      // Audit passive scan  (runtime-gated via CFG "AUDIT SCAN")
NetCapture netCapture;  // Audit promiscuous EAPOL .pcap capture ("AUDIT CAPTURE")
ApServer apServer;      // 'Pedia local SoftAP + HTTP landing page ("PEDIA AP")
NetLink netLink;        // pet-to-pet ESP-NOW discovery ("LINK")
NetSta netSta;          // home-network association, raised by an update job only
NetUpdate netUpdate;    // update CHECK — rides netSta's association, owns no radio
RadioArbiter radio;     // single radio owner: STA XOR AP XOR link XOR capture XOR scan XOR off
SdCard sdCard;          // microSD (SD_MMC 4-bit); inert on card-less / SPI-SD boards

// --- Buttons ---------------------------------------------------------------
struct Btn {
    int pin;
    Button role;
    bool down;
};
Btn buttons[] = {
    {PIN_BTN_A, Button::A, false},
    {PIN_BTN_B, Button::B, false},
    {PIN_BTN_C, Button::C, false},
};
constexpr int kBtnCount = sizeof(buttons) / sizeof(buttons[0]);

bool aDown = false, cDown = false;

// --- Idle screen sleep (device tier; engine unaware) -----------------------
bool screenAsleep = false;
uint32_t lastActivityMs = 0;

// Any input wakes the panel. The waking press is *swallowed* (returns true) so
// it doesn't also trigger a game action — a press to wake shouldn't navigate.
bool noteActivity() {
    lastActivityMs = millis();
    if (!screenAsleep) return false;
    display.wake();
    screenAsleep = false;
    return true;  // this press only woke the screen
}

void initButtons() {
    for (auto& b : buttons) {
        if (b.pin >= 0) pinMode(b.pin, INPUT_PULLUP);  // active-low
    }
}

// Returns true if any button edge was delivered to the engine.
bool pollButtons() {
    bool edge = false;
    for (auto& b : buttons) {
        if (b.pin < 0) continue;
        bool pressed = (digitalRead(b.pin) == LOW);  // BUTTON_ACTIVE_LOW
        if (pressed == b.down) continue;
        b.down = pressed;
        if (b.role == Button::A) aDown = pressed;
        if (b.role == Button::C) cDown = pressed;
        const bool woke = noteActivity();  // also swallows the press that wakes
        if (pressed && woke) continue;     // wake-only: don't forward to the game
        game->onButton({b.role, pressed, pressed && aDown && cDown});
        Serial.printf("[btn] %c %s\n", "ABC"[static_cast<int>(b.role)],
                      pressed ? "down" : "up");
        edge = true;
    }
    return edge;
}

#ifdef BRINGUP_PINSCAN
// The 3 physical buttons are GPIO 0/4/5 (verified from the Waveshare demo).
// Scanning just these lets us confirm the physical L/M/R -> GPIO mapping so the
// A/B/C role assignment in config.h can be finalised. (Never scan USB 19/20.)
const int kScanPins[] = {0, 4, 5};
bool scanLast[64];
void initPinScan() {
    Serial.println("[pinscan] press PLUS then PWR (short taps); watching:");
    for (int p : kScanPins) {
        pinMode(p, INPUT_PULLUP);
        scanLast[p] = digitalRead(p);
        Serial.printf("  GPIO %d\n", p);
    }
}
void pollPinScan() {
    for (int p : kScanPins) {
        bool now = digitalRead(p);
        if (now == LOW && scanLast[p] == HIGH) {
            Serial.printf(">> Pressed: GPIO %d\n", p);
            noteActivity();  // any candidate button wakes the screen during bring-up
        }
        if (now == HIGH && scanLast[p] == LOW) Serial.printf("   released: GPIO %d\n", p);
        scanLast[p] = now;
    }
}
#endif

void repaint() {
    game->render(*fb);
    display.present(fb->data(), kActiveW, kActiveH);
}

// Push the engine's CFG brightness level to the backlight. Maps the
// 0-based level to a 0..255 PWM duty via the shared percent helper; the display
// degrades to on/off when HAS_BACKLIGHT_PWM is 0 (config.h). Cheap enough to poll.
void applyBrightness() {
    const int duty = mal::brightnessPercent(game->brightness()) * 255 / 100;
    display.setBacklight(static_cast<uint8_t>(duty < 0 ? 0 : duty > 255 ? 255 : duty));
}

// Real hardware light sleep for the screen-asleep + radio-quiet window: parks
// the whole SoC (µA range, well below the "awake at 240MHz between polls"
// baseline) instead of just skipping loop iterations. Every button pin is
// armed as a wake source (active-low: wake on the LOW level), plus a timer
// wakeup as a safety net in case a GPIO edge is ever missed. Nothing about the
// game's own timers needs to change for this: they're all anchor-based against
// real elapsed ms (see evolveRemainMs/dyingEnteredMs/etc.), and light sleep
// keeps the system clock running and correctly adjusted across the gap — so
// there's no clock-freeze/pause bookkeeping to add here. That freeze (stopping
// the game clock so travel time isn't credited to hunger/decay/growth) is a
// distinct, not-yet-built feature from this always-on idle sleep.
// Only called with the radio fully powered down (Owner::None already forces
// WiFi.mode(WIFI_OFF) on every consumer), so there's no Wi-Fi/light-sleep
// coexistence concern to handle here.
// When the last light sleep ended. Read by the USB_SETTLE_MS grace window below:
// it is the only way a cable plugged in DURING a sleep can ever be noticed.
uint32_t lastSleepWakeMs = 0;

void enterIdleLightSleep() {
    for (auto& b : buttons) {
        if (b.pin < 0) continue;
        gpio_wakeup_enable(static_cast<gpio_num_t>(b.pin), GPIO_INTR_LOW_LEVEL);
    }
    esp_sleep_enable_gpio_wakeup();
    esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(IDLE_SLEEP_FALLBACK_MS) * 1000ULL);
    esp_light_sleep_start();
    lastSleepWakeMs = millis();
    for (auto& b : buttons) {
        if (b.pin < 0) continue;
        gpio_wakeup_disable(static_cast<gpio_num_t>(b.pin));
    }
}

// --- Travel mode: the deliberate, indefinite pause -------------------------
//
// Deep sleep, not the idle light sleep above, and the difference IS the feature.
// Light sleep parks the SoC with RAM and the system clock alive, so the game keeps
// its place and keeps counting; deep sleep powers the digital core down to the RTC
// domain (µA, well under light sleep) and comes back through a RESET. That reset is
// what freezes the game clock: the engine re-enters through the save, and every
// clock in it is already reboot-safe (Game::requestTravelSleep spells out how).
// Nothing needs pausing, because nothing survives to run.
//
// The wake source is B+C held together. Button A is GPIO0, the download-mode strap
// (HARDWARE.md), and a deep-sleep wake re-runs the reset that strap is sampled at —
// waking on A risks coming up in the ROM downloader instead of the game, which on a
// device with no cable reads as dead. B and C are ordinary GPIOs and carry the same
// two-button deliberateness.
//
// ext1 on this part wakes on ANY pin in the mask, never on all of them, so the CHORD
// and its hold are checked in firmware on the way up (travelWakeGate). A wake that
// isn't the gesture goes straight back down without bringing the panel, the card or
// the radio up, so a press through a bag costs milliseconds awake, not a woken pet.
constexpr uint64_t kTravelWakeMask =
    (1ULL << PIN_BTN_B) | (1ULL << PIN_BTN_C);

// Park the core until the wake gesture. Does not return.
void travelDeepSleep() {
    // Clear what the idle light sleep left configured — its fallback TIMER above all,
    // which would otherwise wake a travelling device every IDLE_SLEEP_FALLBACK_MS and
    // turn an indefinite pause into a slow one.
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

    // The wake pins hold their own pull-ups through the sleep. Every pad reverts to
    // its reset state in deep sleep, and an active-low button on a floating pin is a
    // wake source that fires on nothing.
    for (const uint64_t pin : {static_cast<uint64_t>(PIN_BTN_B),
                               static_cast<uint64_t>(PIN_BTN_C)}) {
        const auto num = static_cast<gpio_num_t>(pin);
        rtc_gpio_pullup_en(num);
        rtc_gpio_pulldown_dis(num);
    }
    esp_sleep_enable_ext1_wakeup(kTravelWakeMask, ESP_EXT1_WAKEUP_ANY_LOW);

#ifdef PIN_POWER_HOLD
    // Hold the power latch through the sleep, for the same reason the pull-ups are
    // held: this pad reverts too, and it is the only thing keeping the battery rail
    // up (HARDWARE.md). Dropping it turns the sleep into a power-off that only the
    // PWR button can undo — which is a different, worse feature.
    gpio_hold_en(static_cast<gpio_num_t>(PIN_POWER_HOLD));
    gpio_deep_sleep_hold_en();
#endif
    Serial.println("[travel] deep sleep — wake on B+C");
    Serial.flush();
    esp_deep_sleep_start();
}

// Run first thing in setup(), before anything slow or power-hungry is brought up.
// A cold boot passes straight through; a travel wake has to present the whole
// gesture here or it never becomes a boot at all.
void travelWakeGate() {
    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT1) return;
    // Hand the wake pads back from the RTC mux so an ordinary digitalRead sees them.
    rtc_gpio_deinit(static_cast<gpio_num_t>(PIN_BTN_B));
    rtc_gpio_deinit(static_cast<gpio_num_t>(PIN_BTN_C));
    pinMode(PIN_BTN_B, INPUT_PULLUP);
    pinMode(PIN_BTN_C, INPUT_PULLUP);
    const uint32_t until = millis() + TRAVEL_WAKE_HOLD_MS;
    while (millis() < until) {
        if (digitalRead(PIN_BTN_B) != LOW || digitalRead(PIN_BTN_C) != LOW)
            travelDeepSleep();   // not the gesture: back down, no boot
        delay(10);
    }
}

// Is a real USB HOST attached right now?
//
// HWCDC::isPlugged() watches USB Start-of-Frame packets via a FreeRTOS tick hook,
// so it means "a host is actively framing us" — a computer, not a dumb charger
// (chargers send no SOF). That distinction is deliberate: charging overnight from
// a wall plug should still sleep normally; only a dev tether keeps the device up.
//
// Two consequences of the tick-hook implementation matter here. It is free to poll
// (a volatile bool, no bus transaction), so this can run every loop. And it cannot
// update while the SoC is light-sleeping — the tick hook is not running and the USB
// link has to re-enumerate on wake — which is what USB_SETTLE_MS exists to cover.
bool usbHostAttached() {
#if USB_KEEPS_AWAKE && ARDUINO_USB_MODE && ARDUINO_USB_CDC_ON_BOOT
    return HWCDC::isPlugged();
#else
    return false;
#endif
}

// --- OTA rollback: proving a freshly-installed image actually works -----------
//
// The bootloader has rollback enabled, so an image installed over the air boots
// once as PENDING_VERIFY and is only kept if something calls
// esp_ota_mark_app_valid_cancel_rollback(); reboot while still pending and the
// bootloader reverts to the slot that was running before. That is the whole
// recovery story on a device with no second way in — nothing here is signed, and
// a brick means a USB cable, which the person this update path was built for does
// not have.
//
// The Arduino core would commit the image inside initArduino(), before setup()
// runs, which makes the test "did the C runtime start" — it catches an image too
// broken to boot and nothing else. verifyRollbackLater() below defers that
// decision to us so the test can be the one that matters: the device reached the
// main loop, put a frame on the panel, and stayed up. A build that panics in
// setup(), wedges bringing up the display, or crash-loops in its first seconds
// never commits, so the next power cycle silently puts the old firmware back.
//
// The window is deliberately survivable rather than long. A user who yanks power
// inside it gets a rollback they didn't need — annoying, and the safe direction.
uint32_t otaCommitAtMs = 0;   // 0 once committed or when nothing is pending

void armOtaCommitIfPending() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if (!running || esp_ota_get_state_partition(running, &state) != ESP_OK) return;
    if (state != ESP_OTA_IMG_PENDING_VERIFY) return;
    otaCommitAtMs = millis() + OTA_PROVE_MS;
    Serial.printf("[ota] on trial — committing in %us if we stay up\n",
                  static_cast<unsigned>(OTA_PROVE_MS / 1000));
}

// Called once a frame has been presented, so "healthy" includes the display.
void commitOtaWhenProven(uint32_t nowMs) {
    if (otaCommitAtMs == 0 || nowMs < otaCommitAtMs) return;
    otaCommitAtMs = 0;
    if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK)
        Serial.println("[ota] image committed — rollback cancelled");
    else
        Serial.println("[ota] commit failed; this image will roll back on reboot");
}

} // namespace

// Weak-symbol override in the Arduino core: true means "don't decide during
// initArduino()", handing the commit to commitOtaWhenProven above.
extern "C" bool verifyRollbackLater() { return true; }

// Phase trace: flushes so the last line before a hang/crash is the culprit.
#ifdef BRINGUP_PINSCAN
#  define BOOT_TRACE(...) do { Serial.printf(__VA_ARGS__); Serial.flush(); } while (0)
#else
#  define BOOT_TRACE(...) do {} while (0)
#endif

void setup() {
    // POWER LATCH FIRST — before anything that can delay boot (Serial, the
    // bringup 15s serial-wait, display init). On battery this board's rail is
    // held only by the physical PWR button until firmware asserts BAT_EN; miss
    // this and the panel flashes then dies the moment PWR is released. Harmless
    // on USB (VBUS feeds VSYS directly). See PIN_POWER_HOLD in config.h.
#ifdef PIN_POWER_HOLD
    pinMode(PIN_POWER_HOLD, OUTPUT);
    digitalWrite(PIN_POWER_HOLD, POWER_HOLD_ACTIVE_HIGH ? HIGH : LOW);
    // A travel wake arrives with this pad still HELD at the level it was parked on
    // (travelDeepSleep), which is why the rail survived the sleep. Release the hold
    // only now that the pad has been driven to the same level again, so the latch
    // never sees a gap. Both calls are no-ops on any other boot.
    gpio_deep_sleep_hold_dis();
    gpio_hold_dis(static_cast<gpio_num_t>(PIN_POWER_HOLD));
#endif

    // Before Serial, the display, the card or the radio: a wake that isn't the
    // travel gesture must cost nothing but the milliseconds spent proving it.
    travelWakeGate();

    Serial.begin(115200);
#ifdef BRINGUP_PINSCAN
    // Wait for the serial host so no boot trace is lost (HWCDC drops pre-attach).
    uint32_t t = millis();
    while (!Serial && millis() - t < 15000) delay(50);
    delay(200);
#else
    delay(200);
#endif
    Serial.println();
    Serial.printf("=== Malwarium boot: %s ===\n", BOARD_NAME);
    Serial.flush();

    BOOT_TRACE("[boot] phase=1 display.begin()\n");
    display.begin();
    BOOT_TRACE("[boot] phase=2 display ok; free heap=%lu\n",
               (unsigned long)ESP.getFreeHeap());

    initButtons();
#ifdef BRINGUP_PINSCAN
    initPinScan();
#endif
    BOOT_TRACE("[boot] phase=3 buttons/scan ok\n");

    // NVS-backed save: loaded at boot, so a power-cycle resumes the raised pet.
    static NvsSaveStore store;
    const bool hadSave = !store.load().empty();   // probe NVS before the ctor consumes it
    static Game g(StartMode::FreshHatch, "paypup", &store);         // real first boot -> Hatch
#ifdef DEV_EGG_TIMER_MS
    // Fast-forward a freshly laid egg so dev iteration doesn't wait out the real
    // clock; a resumed save is already past the egg and this is a no-op then.
    if (g.inEggPhase() && g.bootHatchRemainMs() > DEV_EGG_TIMER_MS)
        g.accelerateEggHatch(g.bootHatchRemainMs() - DEV_EGG_TIMER_MS);
#endif
    game = &g;
    // How the engine asks whether there is room to save before it commits to the
    // attempt. Largest free BLOCK, not total free — the save wants its blob in one
    // piece, so a fragmented heap refuses it long before it runs out. Wired here and
    // never again: it is a probe the engine calls, not a reading pushed at it, so it
    // is always answering about now (Game::setHeapProbe).
    g.setHeapProbe([]() -> uint32_t { return ESP.getMaxAllocHeap(); });
    // Persistence confirmation: a reflash wipes RAM but NOT the NVS partition, so
    // a "loaded NVS" line reporting an edited tag proves the save survived in flash
    // (independent of any battery-retained RAM). One line, same tier as the banner.
    // lvl is included because it's the one field known to have gone stale across a
    // reflash in the field — this line is the fastest way to tell whether NVS itself
    // already held the wrong value (nothing wrote the level before power was cut) vs.
    // it was loaded correctly and something downstream cleared it.
    // Blob-vs-capacity is printed every boot (not just on a write failure) so
    // headroom is visible well before NOT_ENOUGH_SPACE — the failure mode
    // that motivated NVS_SAVE_PARTITION_BYTES existing at all.
    Serial.printf("[save] %s pet=%s gen=%d lvl=%d tag=%s blob=%u/%uB (%u%%)\n",
                  hadSave ? "loaded NVS" : "empty -> seed",
                  game->pet() ? game->pet()->id : "(egg)", game->generation(),
                  game->combatLevel(), game->hackerTag(),
                  (unsigned)game->saveBlobSizeBytes(), (unsigned)NVS_SAVE_PARTITION_BYTES,
                  (unsigned)(100ull * game->saveBlobSizeBytes() / NVS_SAVE_PARTITION_BYTES));
    Serial.flush();
    BOOT_TRACE("[boot] phase=4 Game ok; free heap=%lu\n",
               (unsigned long)ESP.getFreeHeap());

    static Framebuffer f(kActiveW, kActiveH);
    fb = &f;
    BOOT_TRACE("[boot] phase=5 Framebuffer(%dx%d) ok; free heap=%lu\n",
               kActiveW, kActiveH, (unsigned long)ESP.getFreeHeap());

    lastActivityMs = millis();
    applyBrightness();   // drive the backlight to the loaded CFG level before first paint
    repaint();  // first paint
    BOOT_TRACE("[boot] phase=6 first frame presented\n");

    // microSD bring-up (SD_MMC 4-bit SDIO). Optional: a failed mount (no card /
    // wrong pins) logs and continues — boot stays clean. A successful mount runs
    // a write/read round-trip so the boot line proves the filesystem is usable,
    // not merely detected. This is the prerequisite for the Audit .pcap slice.
    if (sdCard.begin()) {
        const bool rt = sdCard.selfTest();
        Serial.printf("[sd] mounted %luMB; round-trip %s\n",
                      (unsigned long)sdCard.sizeMB(), rt ? "OK" : "FAILED");
        // Feed the CFG "SD" line: present only when the round-trip proves the
        // filesystem is actually usable (mounted-but-unwritable reads as absent).
        game->setSdStatus({rt, sdCard.sizeMB()});
        // Real-network discovery history (core/net/network_ledger.h) — a missing
        // file (first boot) just starts the ledger empty, not an error.
        game->loadNetworkLedger("/sdcard/network_ledger.bin");
        // The met-operators roster (core/net/peer_ledger.h) — same deal: a
        // missing file just starts it empty. Without a card the roster still
        // works, it just lasts only as long as this boot.
        game->loadPeerLedger("/sdcard/peer_ledger.bin");
    } else {
        // ESP_FAIL with a card seated = the card initialised but its filesystem
        // isn't FAT-mountable (exFAT / unformatted) — reformat FAT32. Any other
        // code (timeout etc.) points at no card / a hardware issue.
        Serial.printf("[sd] no card / mount failed: %s (continuing)\n",
                      sdCard.lastError());
        game->setSdStatus({false, 0});   // CFG "SD" -> ABSENT
    }
    Serial.flush();

    // Wire every radio consumer + the arbiter that owns the radio between them.
    // All stay off until their persisted CFG opt-in is on (authorized use); the
    // arbiter guarantees they never both power the radio (arbitration).
    netSniff.begin(game);
    netCapture.begin(game);
    apServer.begin(game);
    netLink.begin(game);
    netSta.begin(game);
    radio.begin(game, &netSniff, &netCapture, &apServer, &netLink, &netSta);
    // Tell the engine whether a home network is stored. The credentials themselves
    // stay device-tier (net_credentials.h) — this bool is all the UPDATES screen
    // needs to tell "no consent" apart from "consented, nowhere to go". Refreshed by
    // the setup portal the moment it writes one (ap_server.h).
    {
        static NetCredentials creds;
        const bool provisioned = creds.provisioned();
        game->setNetProvisioned(provisioned);
        Serial.printf("[sta] home network %s\n",
                      provisioned ? "provisioned" : "not set up");
    }
#if UPDATE_ENABLED
    // Where an update check would look, and whether that is a stored override or
    // the compiled default (update_source.h). Reported at boot because "no source"
    // is a legitimate state, and one that otherwise only shows up as a check that
    // quietly does nothing.
    netUpdate.begin(game);
    {
        static UpdateSource src;
        char manifestUrl[UpdateSource::kUrlCap];
        // Tell the engine whether a check is even possible. Like setNetProvisioned
        // this is a bare bool — the URL stays device-tier — and it is what the
        // UPDATES screen reads to name the missing piece instead of offering a
        // button that would do nothing.
        const bool known = src.url(manifestUrl, sizeof(manifestUrl));
        if (!known) manifestUrl[0] = '\0';
        // Carries the address too, not just "is there one" — the 'Pedia's user
        // section shows it in the field that edits it (ap_server.h).
        game->setUpdateManifestUrl(manifestUrl);
        if (known)
            Serial.printf("[upd] manifest %s (%s)\n", manifestUrl,
                          src.overridden() ? "stored" : "built-in");
        else
            Serial.printf("[upd] no manifest source — checks are unavailable\n");

        // What the check would compare against, one line for both artifacts. The
        // web bundle's version is a file on the card, so the engine gets told what
        // it says — the install-confirm prompt names it as the "now" half.
        VersionMarker web;
        readWebBundleVersion(&web);
        game->setWebBundleVersion(web.version);
        Serial.printf("[upd] installed fw=%s(%u) web=%s(%u)\n", kFirmwareVersion,
                      static_cast<unsigned>(kFirmwareVersionCode),
                      web.version[0] ? web.version : "unknown",
                      static_cast<unsigned>(web.code));
    }
#endif
    beginBattery();    // prime the charge-status pin (no-op if no battery monitor)
    game->setPowerStatus(readPowerStatus());  // first reading before any repaint
    // Last, so the trial window starts from a device that finished coming up —
    // everything above is what an OTA image has to survive to be worth keeping.
    armOtaCommitIfPending();
}

void loop() {
    const bool wasAsleep = screenAsleep;
    bool dirty = pollButtons();
#ifdef BRINGUP_PINSCAN
    pollPinScan();
#endif
    if (wasAsleep && !screenAsleep) dirty = true;  // just woke -> repaint fresh state

    // NOTE: there is deliberately no dev A+B "reset to Decryption Hatch" chord here
    // (— too easy to mis-press). The CFG "RESET TO HATCH" row
    // (DEV_CFG_RESET_ROW, dev_config.h) is the single dev reset path.

#ifndef BRINGUP_PINSCAN
    // Idle screen sleep: blank the panel after SCREEN_SLEEP_MS of no input. The
    // engine keeps ticking (model decay etc.); we just stop presenting.
    // Disabled during bring-up so the panel stays on while we debug.
    //
    // Auto-explore keeps the panel AWAKE: the run is hands-off and the
    // player is watching it, so we treat every explore frame as fresh activity —
    // this holds the sleep timer off WITHOUT a separate "is exploring" gate on the
    // sleep test below. When the mode ends by ANY route (a lost fight, a cleared
    // gauntlet, a manual Cancel, a new egg, or a crisis modal that preempts it) the
    // frames simply stop counting as activity and the normal timeout resumes,
    // counting from the last explore frame. Because it's a per-frame refresh and
    // not a one-shot transition edge, sleep re-enables cleanly on every exit path —
    // there is no edge to miss and no "still exploring" flag left stuck on.
    // Auto-explore, the PEDIA QR page and the RADIO SCREENS all hold the panel
    // awake: the first is a hands-off run the player watches, the second needs to
    // stay lit long enough to scan + join the AP, and the radio screens (CREW's
    // home-network picker, PEERS) are read hands-off while they fill in — the
    // operator walks toward the network they mean to claim, or holds the device
    // beside someone else's, which takes far longer than SCREEN_SLEEP_MS (their own
    // idle budget is kRadioScreenDefocusMs).
    //
    // Letting the panel sleep on those two would be worse than an inconvenience:
    // holding the screen open is what arms their radio, so a dark screen is also a
    // dead radio, and the operator would be staring at a device that has quietly
    // stopped looking. All of these are a per-frame refresh — no sticky flag — so
    // the normal timeout resumes cleanly the moment the state ends (explore stops,
    // or the player exits the QR / CREW / PEERS screen with C).
    //
    // A tethered USB host holds it awake for the same per-frame reason, but a
    // different one of its own: the sleeps exist to save battery, and a tethered
    // device isn't on battery. Unplugging simply lets the normal timeout resume.
    if (game->exploreActive() || game->qrScreenActive() || game->radioScreenOpen() ||
        usbHostAttached())
        lastActivityMs = millis();
    if (SCREEN_SLEEP_MS > 0 && !screenAsleep &&
        millis() - lastActivityMs >= static_cast<uint32_t>(SCREEN_SLEEP_MS)) {
        display.sleep();
        screenAsleep = true;
        Serial.println("[screen] sleep");
    }
#endif

    // Runtime microSD re-check (CFG "SD RECHECK" -> Game::requestSdRecheck). The
    // card is mounted once in setup(), so one inserted later is invisible until
    // this remounts it. DECISION: seal any live capture FIRST rather than defer —
    // sealActive() keeps the toggle intent (it re-arms after the normal cooldown),
    // and a forced arbiter poll powers the capture owner down so its .pcap sink is
    // flushed + closed before we unmount (unmounting under an open file corrupts
    // it). Then remount, push a fresh SD reading (which also flashes the idle SD
    // icon via setSdStatus), and log one [sd] line. Inert on card-less/SPI-SD boards.
    // Brightness: re-drive the backlight whenever the CFG level changes.
    // Only the user changes it (in CFG), so a cheap value-compare suffices — no
    // one-shot flag needed. Applied even while the screen is "asleep" so waking
    // restores the right level (wake() also re-pushes duty_).
    static int lastBrightness = -1;
    if (game->brightness() != lastBrightness) {
        lastBrightness = game->brightness();
        applyBrightness();
    }

    if (game->sdRecheckRequested()) {
        game->auditCapture().sealActive(millis());  // no-op unless armed/hot
        radio.poll(millis());                       // release capture -> close sink
        const bool ok = sdCard.recheck();
        const bool rt = ok && sdCard.selfTest();
        game->setSdStatus({rt, sdCard.sizeMB()});
        game->clearSdRecheck();
        Serial.printf("[sd] recheck: %s %luMB round-trip %s\n",
                      ok ? "mounted" : "absent",
                      (unsigned long)sdCard.sizeMB(), rt ? "OK" : "FAILED");
        Serial.flush();
        dirty = true;
    }

    // The radio: the arbiter picks the single owner (AP > LINK > CAPTURE > SCAN >
    // off) from the persisted CFG opt-ins + the low-battery guard, stands the
    // others down, and services the owner. The audit consumers are
    // passive/authorized-use (scan/listen only; the capture path records EAPOL
    // already on the air — no deauth/inject); LINK is the one that TRANSMITS —
    // its own HELLO beacon, plus unicast duel frames to one operator — behind its
    // own opt-in. Non-blocking so the ~4fps render loop never stalls.
    //
    // Screen-sleep interaction (deliberate): capture KEEPS RUNNING while the panel
    // is asleep — arming it and pocketing the device is the intended use. The
    // headless drain is bounded by the SM arm-window (self-seals a listen session
    // that never captures), the re-arm cooldown, and this battery guard — those,
    // not the screen, are the drain safeguards. The guard thresholds are a
    // first cut pending a real bench power-measurement pass.
    radio.poll(millis());

    // The update job rides whatever association the arbiter just serviced, so it
    // runs after the poll — it owns no radio of its own, it only waits for one.
    netUpdate.service(millis());
#if UPDATE_ENABLED
    // A committed firmware image only takes effect on the next boot. Give the
    // "RESTARTING..." frame time to reach the panel first — the operator should
    // see why the device went dark, not just watch it vanish mid-install.
    static uint32_t restartAtMs = 0;
    if (netUpdate.restartPending() && restartAtMs == 0) restartAtMs = millis() + 2000;
    if (restartAtMs != 0 && millis() >= restartAtMs) ESP.restart();
#endif

    // Battery/charge status: event-driven, not polled. SoC answers "do I need
    // to charge soon?", which only flips over *hours* — a free-running timer
    // sampling every ~2s (~1800x/hour) was pure waste. The only consumer is the
    // CFG "BATT" line, so we only need a fresh reading (a) right as the screen
    // wakes (the reading may be stale from a long sleep) and (b) right as the
    // player lands on CFG SysInfo (the one screen that displays it).
    if (wasAsleep && !screenAsleep) game->setPowerStatus(readPowerStatus());
    const bool onSysInfo = game->nav() == Game::Nav::Detail &&
                           game->cfgScreen() == CfgScreen::SysInfo;
    static bool wasOnSysInfo = false;
    if (onSysInfo && !wasOnSysInfo) game->setPowerStatus(readPowerStatus());
    wasOnSysInfo = onSysInfo;

#if HEAP_TRACE_ENABLED
    // A baseline to read the transition figures against: a LEAK shows as a monotonic
    // decline across repeated cycles, transient pressure as a dip that recovers. The
    // save field is the guard's own verdict, so a deferred write is visible rather
    // than being inferred from a save that silently didn't happen (config.h).
    {
        static uint32_t lastHeap = 0;
        if (millis() - lastHeap >= HEAP_TRACE_MS) {
            lastHeap = millis();
            Serial.printf("[heap] t=%lus free=%u largest=%u save=%s floor=%u held=%d low=%u "
                          "wfail=%d\n",
                          millis() / 1000,
                          (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap(),
                          game->saveHasHeadroom() ? "ok" : "DEFERRED",
                          (unsigned)game->saveHeapFloor(),
                          game->savesDeferred(), (unsigned)game->savesDeferredLowMark(),
                          game->saveWritesFailed());
        }
    }
#endif
    const bool beat = game->tick(millis());  // always tick (model runs while asleep)
    if (beat) dirty = true;
    if (dirty && !screenAsleep) repaint();

    // Travel mode. The request is latched rather than pulsed, so each of the three
    // preconditions below can simply decline this pass and be asked again next loop.
    //
    // The panel is left showing the notice frame for a moment before it goes off: a
    // device that darkens with no explanation is one the operator will press every
    // button on, and the wake gesture is the last thing on screen.
    if (game->travelSleepRequested()) {
        static uint32_t travelDarkenAtMs = 0;
        // An OTA image still on trial has not been committed, and a reset while it is
        // pending is exactly what the bootloader rolls back. Waiting out the trial
        // costs the operator seconds; not waiting costs them the version they just
        // installed, silently.
        const bool otaOnTrial = otaCommitAtMs != 0;
        // The save is the whole reason this is not just a power-off: a deep sleep is
        // a reset, so anything still inside the autosave debounce would be gone.
        // saveNow() reports the heap guard declining, and a decline means stay up.
        if (!otaOnTrial && game->saveNow()) {
            if (travelDarkenAtMs == 0) travelDarkenAtMs = millis() + 1500;
            if (millis() >= travelDarkenAtMs) {
                radio.standDown(millis());   // seals a live capture; closes the .pcap
                display.sleep();
                travelDeepSleep();           // does not return
            }
        }
    }

#ifdef BRINGUP_PINSCAN
    // Heartbeat so a serial reader attaching at any time sees the firmware is
    // alive (the S3 hardware-CDC drops prints emitted before a host connects).
    static uint32_t lastAlive = 0;
    if (millis() - lastAlive >= 2000) {
        lastAlive = millis();
        Serial.printf("[alive] t=%lus hunger=%d nav=%d\n", millis() / 1000,
                      game->model().hunger(), static_cast<int>(game->nav()));
    }
#endif
    // Poll cadence: while the screen is asleep and the radio has no owner (no
    // Audit Scan/Capture, no 'Pedia AP), nothing external can touch game state
    // — a button press is the only thing worth watching for, so light-sleep
    // the SoC instead of just spinning the loop. Any other state (screen lit,
    // or a radio consumer running) keeps the normal responsive poll cadence.
    // Bring-up pin scanning wants continuous digitalRead polling, not sleep.
    //
    // A USB host blocks the light sleep outright: light sleep gates the clocks the
    // USB-Serial-JTAG link runs on, so sleeping mid-session drops the device out of
    // a serial monitor.
    //
    // The settle window is what makes a cable plugged in DURING a sleep detectable
    // at all. There is no USB wake source on this SDK, so the fallback timer wake is
    // the only chance to notice — and at that instant usbHostAttached() is still
    // stale, because the tick hook that maintains it was stopped and the link is
    // re-enumerating. Staying up briefly after every wake gives that a chance to
    // land. It costs ~0.3% duty against a 2-minute fallback, and without it the
    // device would sleep again before ever seeing the cable.
    // A frame has been on the panel by now, so this is the full health test.
    commitOtaWhenProven(millis());

    const bool usbSettling = USB_KEEPS_AWAKE && lastSleepWakeMs != 0 &&
                             millis() - lastSleepWakeMs < USB_SETTLE_MS;
    const bool idleAndQuiet = screenAsleep && radio.owner() == RadioArbiter::Owner::None &&
                              !usbHostAttached() && !usbSettling;
#ifdef BRINGUP_PINSCAN
    delay(5);
#else
    if (idleAndQuiet) enterIdleLightSleep();
    else delay(5);
#endif
}
