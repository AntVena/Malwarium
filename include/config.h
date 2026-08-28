// =============================================================================
//  config.example.h  —  Malwarium hardware configuration template
// -----------------------------------------------------------------------------
//  Copy this file to  include/config.h  and edit the values for YOUR board.
//  The game engine and web layer must NEVER hardcode a pin — every board-
//  specific value lives here. To port to a new board, add a new BOARD_* block
//  below and select it; you should not need to touch any other source file.
//
//  HOW TO USE
//    1. Pick your board by setting BOARD_TARGET (or pass -D BOARD_* via
//       platformio.ini build_flags — see the bottom of this file).
//    2. Fill in any pin marked  // TODO: confirm  for your board.
//    3. Build.
//
//  Pins marked CONFIRMED were verified against Waveshare's official demo code.
//  Pins marked TODO must be read from your board's schematic, or discovered
//  with the little pin-finder sketch in  tools/pin_finder.ino .
// =============================================================================

#pragma once

// -----------------------------------------------------------------------------
//  0.  BOARD SELECTION
//  Set exactly one of these to 1 (or define it in platformio.ini build_flags).
// -----------------------------------------------------------------------------
//  Porting = adding a BOARD_* here, a block below, and a term to the sum.
#define BOARD_WAVESHARE_S3_154  1   // Primary / reference board

#if (BOARD_WAVESHARE_S3_154) != 1
#  error "Select exactly ONE board in config.h (set one BOARD_* to 1, the rest to 0)."
#endif


// =============================================================================
//  BOARD A:  Waveshare ESP32-S3-LCD-1.54  (primary / reference)
//  Source: Waveshare official Arduino + ESP-IDF demos for this board.
// =============================================================================
#if BOARD_WAVESHARE_S3_154

#define BOARD_NAME              "Waveshare ESP32-S3-LCD-1.54"

// --- Display ---------------------------------------------------------------
//  ST7789, 240x240 IPS, 4-wire SPI. Pins VERIFIED against the official Waveshare
//  ESP32-S3-Touch-LCD-1.54 demo (examples/04_gfx_helloworld): Arduino_ESP32SPI
//  (DC 45, CS 21, SCK 38, MOSI 39, MISO -1) + Arduino_ST7789(RST 40, rot 0, IPS).
//  NOTE: the earlier template values (1/2/3/4/5/6) were for a different board.
#define DISPLAY_DRIVER_ST7789   1
#define DISPLAY_WIDTH           240
#define DISPLAY_HEIGHT          240
#define DISPLAY_ROTATION        0

#define PIN_LCD_SCK             38   // VERIFIED (demo)
#define PIN_LCD_MOSI            39   // VERIFIED (demo)
#define PIN_LCD_MISO           -1    // write-only panel
#define PIN_LCD_DC              45   // VERIFIED (demo)
#define PIN_LCD_RST            40    // VERIFIED (demo)
#define PIN_LCD_CS             21    // VERIFIED (demo)
#define PIN_LCD_BL             46    // VERIFIED (demo) — plain GPIO, drive HIGH = on (no PWM)

// Backlight brightness PWM ------------------------------------
//  The CFG "BRIGHTNESS" setting is fully wired engine-side (persisted, save v14):
//  Game::setBrightness persists the level and main.cpp's applyBrightness() maps
//  it to a 0..255 duty. With HAS_BACKLIGHT_PWM 0 that duty only degraded to plain
//  on/off GPIO (digitalWrite HIGH for any non-zero duty) — every level rendered
//  full brightness, no dimming. The LEDC PWM path in display_esp32.h (init,
//  setBacklight, sleep, wake) is consistent and lgfx_config.h doesn't manage the
//  backlight pin, so there's no conflict — flipped on as the shipped default.
#define HAS_BACKLIGHT_PWM       1    // 1 = LEDC dimming (shipped) · 0 = on/off GPIO fallback
#define BACKLIGHT_LEDC_CHANNEL  7    // an LEDC channel unlikely to collide with audio PWM
#define BACKLIGHT_LEDC_FREQ_HZ  5000
#define BACKLIGHT_LEDC_BITS     8    // 0..255 duty

// Logical canvas / scaling ----------------------
#define CANVAS_SCALE_NUM        7    // 1.75x expressed as 7/4 (whole-number safe)
#define CANVAS_SCALE_DEN        4
#define CANVAS_ACTIVE_W         224
#define CANVAS_ACTIVE_H         224
#define CANVAS_EDGE_BUFFER      8    // px guard band against bezel clipping

// --- SD card  (NOTE: SD_MMC / 4-bit SDIO on this board, NOT SPI) ------------
#define SD_ENABLED              1
#define SD_USE_SDMMC            1    // 1 = SD_MMC (SDIO), 0 = SPI SD.h
#define SD_BUS_WIDTH            4
#define PIN_SD_CLK              16   // CONFIRMED
#define PIN_SD_CMD              15   // CONFIRMED
#define PIN_SD_D0               17   // CONFIRMED
#define PIN_SD_D1               18   // CONFIRMED
#define PIN_SD_D2               13   // CONFIRMED
#define PIN_SD_D3               14   // CONFIRMED

// --- I2C bus  (shared: QMI8658 IMU, CST816 touch, ES8311 codec) ------------
#define PIN_I2C_SDA             42   // CONFIRMED
#define PIN_I2C_SCL             41   // CONFIRMED

// --- Audio  (ES8311 codec + NS4150B amp over I2S) --------------------------
#define AUDIO_ENABLED           1    // set 0 to compile out all audio
#define AUDIO_USE_I2S           1    // 1 = real I2S codec, 0 = PWM buzzer fallback
#define PIN_AUDIO_PA_CTRL       7    // CONFIRMED  (amp enable, HIGH = on)
#define PIN_I2S_MCLK            8    // CONFIRMED
#define PIN_I2S_BCLK            9    // CONFIRMED
#define PIN_I2S_LRCLK           10   // CONFIRMED  (WS)
#define PIN_I2S_DOUT            12   // CONFIRMED

// --- IMU -------------------------------------------------------------------
#define IMU_ENABLED             1    // QMI8658 on the I2C bus above (stretch use)

// --- Touch (Touch variant only; SKU 33868/33869) ---------------------------
#define TOUCH_ENABLED           0    // set 1 if you have the capacitive-touch board (CST816)

// --- Buttons  (3 physical: PLUS, BOOT, PWR — all readable) -----------------
//  Active level: these are pulled HIGH and read LOW when pressed.
#define BUTTON_ACTIVE_LOW       1
#define BUTTON_COUNT            3

//  Logical game roles (engine uses these names; physical pins below):
//    A = INPUT_NEXT      (advance / cycle)
//    B = INPUT_ACCEPT    (confirm / enter)
//    C = INPUT_CANCEL    (back / exit)
//    A+C chord = INPUT_META_EXPLOIT
//  VERIFIED against the demo + on-device + the official schematic GPIO map:
//  the 3 buttons are GPIO 0, 5, 4 (active-low), physically left->middle->right.
//  Schematic net names: GPIO0 = KEY_MINUS (also the BOOT / download-mode strap),
//  GPIO5 = KEY_PWR (the middle *power* button — it is wired into the battery
//  power latch; see PIN_POWER_HOLD below), GPIO4 = KEY_PLUS. We map the physical
//  left->middle->right order onto the natural A/B/C.
#define PIN_BTN_A              0    // leftmost (KEY_MINUS/BOOT) -> NEXT
#define PIN_BTN_B              5    // middle   (KEY_PWR)        -> ACCEPT
#define PIN_BTN_C              4    // rightmost(KEY_PLUS)       -> CANCEL

// --- Power latch (soft power-on hold) --------------------------------------
//  On BATTERY this board powers itself through a P-MOSFET latch (schematic
//  "Power"/"BAT" blocks: KEY_PWR + BAT_EN -> 8050 NPN -> AO3401 P-FETs gating
//  VBAT->VSYS). Pressing PWR (GPIO5) momentarily closes the latch so the CPU
//  boots; firmware must then drive BAT_EN HIGH to *hold* it closed. If it never
//  does, the rail collapses the instant PWR is released — the panel flashes for
//  <0.1s then dies (USB is immune: VBUS feeds VSYS directly, no latch needed).
//  Assert HIGH as the very first thing in setup() (main.cpp). BAT_EN = GPIO2
//  (not a strapping pin, so driving it at boot is safe).
#define PIN_POWER_HOLD          2    // BAT_EN — HIGH = stay powered on battery
#define POWER_HOLD_ACTIVE_HIGH  1

// --- Battery monitoring ----------------------------------------------------
#define BATTERY_MONITOR_ENABLED 1
#define PIN_BATTERY_ADC         1    // BAT_ADC (GPIO1) — CONFIRMED from schematic
#define PIN_CHARGE_STAT         3    // CHG_STAT (GPIO3) — ETA6098 charge status
//  Voltage = (3.3 / 4096) * 3.0 * raw_adc   (R27 200K / R32 100K = 3:1 divider)
#define BATTERY_ADC_DIVIDER     3.0f

// --- Real-time clock -------------------------------------------------------
#define HAS_HARDWARE_RTC        0    // none on board; clock pauses while powered off

// --- Power management (device tier) ----------------------------------------
//  Idle screen sleep: after this many ms with no button press, blank the panel
//  (backlight off + ST7789 sleep). Any button press wakes it and is swallowed
//  (not delivered to the game). The engine keeps ticking while the screen is
//  off. Set 0 to disable.
#define SCREEN_SLEEP_MS         30000   // 30s
//  Once the screen is asleep AND the radio has no owner (no Audit Scan/
//  Capture, no 'Pedia AP) — i.e. nothing external can change game state — the
//  loop light-sleeps the SoC instead of spinning. A button press wakes it
//  instantly (GPIO wake, independent of this value); this timer wake is only
//  a fallback in case that GPIO wake is ever somehow missed, so it's sized by
//  "how long is acceptable to be stuck dark if that happens" rather than by
//  responsiveness. The engine keeps ticking correctly across the gap (its
//  timers are anchor-based against real elapsed ms, not per-loop counters),
//  so nothing else needs to know sleep happened.
#define IDLE_SLEEP_FALLBACK_MS  120000  // 2 min
//  CPU clock, and the one window it is throttled in. The loop light-sleeps only when
//  the radio ALSO has no owner, so the state this covers is the dark screen with a
//  consumer running — an armed audit capture, a live 'Pedia AP — which is exactly the
//  "arm it and pocket the device" use, and which otherwise spins at the full clock for
//  as long as the session lasts.
//
//  Both values stay PLL-sourced (80/160/240), which is what makes this safe to do
//  under a running peripheral: on the S3 the APB stays at 80MHz for any of the three,
//  so the LCD's SPI divider, the backlight's LEDC timer and the UART baud are all
//  untouched by the switch. Wi-Fi needs >= 80MHz, so a capture keeps running. Set the
//  two equal to disable the throttle.
//
//  The panel is asleep whenever this applies, so nothing on screen can stutter for it.
//  Sized as a first cut alongside the audit RF bounds in tunables.h, and wants the
//  same bench pass: measure armed-idle draw at both clocks before moving either.
#define CPU_ACTIVE_FREQ_MHZ     240
#define CPU_IDLE_FREQ_MHZ       80
//  Travel mode (CFG -> DEVICE -> TRAVEL MODE): a deliberate indefinite pause, and
//  a DEEP sleep rather than the idle light sleep above — the digital core goes down
//  and comes back through a reset, which is what makes the pause a real one. The
//  wake gesture is PIN_BTN_B + PIN_BTN_C held together for this long. Deliberately
//  NOT the A+C Exploit chord: PIN_BTN_A is GPIO0, the download-mode strap, and a
//  deep-sleep wake re-runs the reset that strap is sampled at.
//
//  ext1 wakes on EITHER pin, so the second button and this hold are both checked in
//  firmware on the way up (main.cpp's travelWakeGate). Sized so a press through a
//  bag falls short while a deliberate one doesn't feel like a wait.
#define TRAVEL_WAKE_HOLD_MS     1200    // 1.2s

// Heap trace ----------------------------------------
//  A periodic free/largest-block line on serial, plus whether a save would be
//  allowed to run right now (Game::saveHasHeadroom). LARGEST is the number that
//  matters: the save wants its blob in one piece, so a fragmented heap refuses it
//  long before free memory runs out — which is exactly how a save landing during a
//  radio hand-off used to reset the device. Chatty by design, so it is its own flag
//  rather than something a release build carries. The one-off heap figures on the
//  [radio]/[ap]/[cap] lines are NOT gated by this: those ride traces that already
//  exist, cost two numbers, and are the only record of what a transition did.
//
//  `held`/`low` on that line are the guard's own counters, and they are the reason to
//  turn this back on: the heap state that makes a save defer is FREED before anything
//  can sample it, so the periodic figures read healthy at the exact moment headroom
//  was measured at 19KB. Only the counters see it. `floor` is which of the two bars
//  LARGEST is being judged against (tunables.h): the high one until the engine owns
//  its serialize buffer, the low one once every save writes into it.
#define HEAP_TRACE_ENABLED      0       // 1 = periodic [heap] line on serial
#define HEAP_TRACE_MS           3000    // how often it prints
//  Tethered-awake: while a USB HOST is attached, hold off both the screen sleep
//  and the SoC light sleep. Sleeping exists to save battery, and a tethered device
//  isn't on battery — but the sharper reason is that light sleep drops the
//  USB-Serial-JTAG link, so a sleeping device disappears from `pio device monitor`
//  mid-session and the panel goes dark while you're reading it.
//
//  The signal is HWCDC::isPlugged(), which tracks USB Start-of-Frame packets from
//  a real host. A dumb wall charger sends no SOF, so charging overnight from a
//  plug still sleeps normally — only a computer keeps the device up.
//
//  NO WAKE-ON-PLUG. This SDK has no USB light-sleep wake source
//  (esp_sleep_enable_usb_serial_jtag_wakeup doesn't exist here), so plugging into
//  a sleeping device cannot wake it directly. Recovery is the existing
//  IDLE_SLEEP_FALLBACK_MS timer wake: worst case, the device notices the cable one
//  fallback period after it went in. USB_SETTLE_MS is what makes that check work
//  at all — see its note.
#define USB_KEEPS_AWAKE         1       // 1 = a tethered USB host suppresses both sleeps
//  Grace period after a light-sleep wake before sleep is allowed again. Light
//  sleep gates the clocks the USB peripheral and the FreeRTOS tick hook both need,
//  so isPlugged() is stale the instant we wake and the link has to re-enumerate
//  before it reads true again. Without this window the device would sleep again
//  before ever noticing the cable, and the fallback check above could never fire.
#define USB_SETTLE_MS           400

// --- OTA trial window ------------------------------------------------------
//  How long a freshly-installed image must stay up, rendering, before it is
//  committed and the bootloader's rollback is cancelled (main.cpp). Sized to
//  outlast the failures worth catching — a panic in setup(), a wedged display
//  bring-up, an early crash-loop — while staying short enough that a user is
//  unlikely to power-cycle inside it. Doing so is harmless anyway: they get a
//  rollback they didn't need, which is the safe direction to be wrong in.
#define OTA_PROVE_MS            20000   // 20s

// Audit mode / passive network-discovery scan -----------
//  AUTHORIZED USE ONLY. Feeds the "NETS" breadth count (Game::networksSeen_)
//  from the REAL radio via a periodic PASSIVE Wi-Fi scan (net_sniff.h ->
//  WiFi.scanNetworks(async), dedup by BSSID in Game::registerNetwork). PASSIVE
// ONLY: listen/scan, never deauth or inject (the.pcap handshake slice is
//  separate and NOT built here).
//  NET_SNIFF_ENABLED is now only a BOARD-CAPABILITY gate — "is a Wi-Fi radio
//  present / compile the audit scan path in." The actual on/off is a RUNTIME
//  toggle the player flips in CFG ("AUDIT SCAN"), persisted in the save blob
//  (Game::netScanEnabled(), save v5, default OFF). With the toggle OFF the radio
//  is powered down (no activity); ON, it rescans every NET_SNIFF_SCAN_INTERVAL_MS
//  and each newly-heard BSSID bumps NETS. (Set NET_SNIFF_ENABLED 0 on a radio-
//  less board to compile the whole path out.)
#define NET_SNIFF_ENABLED           1      // 1 = compile the device-tier passive scan path
#define NET_SNIFF_SCAN_INTERVAL_MS  30000  // rescan cadence while the runtime toggle is ON
//  A sweep's results arrive in TWO steps: the driver stops sweeping, and only then is
//  the scan-done event dispatched from the WiFi task carrying the count.
//  WiFi.scanComplete() reports WIFI_SCAN_FAILED in the gap between the two, so a
//  negative result is only believed once a sweep has run well past any legitimate
//  duration (~6s to cover 14 channels) — see net_sniff.h for why believing it early is
//  self-perpetuating.
#define NET_SNIFF_SCAN_TIMEOUT_MS   15000  // give up on a sweep that never reports
#define NET_SNIFF_RETRY_MS          2000   // backoff before retrying after that

// Pet-to-pet LINK (ESP-NOW) ---------------------------
//  Broadcast discovery of other Malwarium devices: each device announces itself
//  (core/net/peer_link.h) and remembers whoever answers (core/net/peer_ledger.h).
//  Connectionless — no pairing, no association, no peer-table entry for the people
//  we merely hear, so the met-list is bounded by storage rather than by ESP-NOW's
//  20-peer limit (that limit only ever binds a UNICAST conversation).
//  LINK_ENABLED is a BOARD-CAPABILITY gate — "is a Wi-Fi radio present / compile
//  the pet-to-pet path in." The runtime on/off is Game::linkWanted(): the player's
//  persisted CFG "LINK" toggle (Game::linkEnabled(), save v37, default OFF), plus
//  the stretch the PEERS screen raises it itself. OFF ⇒ nothing is transmitted.
//
//  ANNOUNCING IS ITS OWN CONSENT. This transmits the operator's HackerTag, pet and
//  crew to anyone in range, which is a different act from the audit scan's passive
//  listening — so it is a separate toggle, never implied by AUDIT SCAN being on.
//
//  Both devices must be parked on the SAME channel to hear each other; there is no
//  channel-hopping rendezvous. PEER_LINK_CHANNEL is that fixed meeting point.
#define LINK_ENABLED                1      // 1 = compile the device-tier ESP-NOW path
#define PEER_LINK_CHANNEL           1      // fixed rendezvous channel (must match peers)
#define PEER_BEACON_INTERVAL_MS     1000   // HELLO cadence while the radio is ours
//  The piggyback window. An unassociated ESP-NOW listener cannot use Wi-Fi modem
//  sleep (that needs an AP's DTIM beacons), so its receiver is on continuously —
//  there is no cheap "always listening" mode, and ambient discovery has to be duty
//  cycled. Rather than power the radio up on its own schedule, LINK borrows the
//  audit scan's existing radio-up window: after a sweep completes, the radio is
//  already hot, so it costs only this window rather than a full power cycle.
//  Discovery probability is bought by the window LENGTH, though, and that part is
//  not free — two devices whose windows are unsynchronised overlap roughly
//  2*W/NET_SNIFF_SCAN_INTERVAL_MS of the time, so at 2s/30s expect a few minutes
//  of co-presence before two ambient devices find each other. Holding the PEERS
//  screen open skips all of this and discovers in under a second.
#define PEER_BURST_MS               2000   // listen/announce window per scan cycle

// 'Pedia local Access Point --------------------------
//  A standalone 2.4GHz SoftAP + tiny HTTP server that serves the 'Pedia landing
//  page (no internet needed). AP_ENABLED is only a BOARD-CAPABILITY gate — "is a
//  Wi-Fi radio present / compile the AP path in." The actual on/off is a RUNTIME
//  toggle the player flips in CFG ("PEDIA AP"), persisted in the save blob
//  (Game::apEnabled(), save v20, default OFF). While ON the AP is the radio's TOP
//  owner (the RadioArbiter stands the audit scan/capture down) and hands the radio
//  back to whichever of those is still toggled ON the moment the AP is switched
//  off. Open network (no password) so the QR-scan-then-connect flow is one step;
//  set AP_PASSWORD to a >=8-char string to make it WPA2. (Set AP_ENABLED 0 on a
//  radio-less board to compile the whole path out.)
#define AP_ENABLED                  1      // 1 = compile the device-tier SoftAP + 'Pedia server
#define AP_SSID                     "Malwarium"
#define AP_PASSWORD                 ""     // "" = open AP; >=8 chars = WPA2
#define AP_CHANNEL                  1      // 2.4GHz channel for the SoftAP

// Home-network association (STA) ---------------------
//  A BOARD-CAPABILITY gate — "is there a radio that can join someone else's
//  network." There is no runtime opt-in beside it: an update job raises the
//  association it needs and drops it when it ends, so the radio is claimed on the
//  spot, per job, never standing (platform/esp32/net_sta.h). Credentials come from
//  the phone-driven setup page the SoftAP serves, never from three-button entry.
#define STA_ENABLED                 1      // 1 = compile the device-tier STA + setup portal
//  How long one association attempt may take before it reports failure. Covers
//  association AND DHCP; generous because a busy home AP can take several seconds.
#define STA_CONNECT_TIMEOUT_MS      20000

// Over-the-air updates ------------------------------
//  BOARD-CAPABILITY gate for the update path: needs a radio that can reach the
//  wider 'net (STA_ENABLED) AND a partition table with two app slots to flash
//  between (partitions_malwarium.csv). There is no runtime toggle at all: pressing
//  CHECK NOW is what puts the device on the network, the job hands the radio back
//  when it ends, and nothing installs without a second explicit yes.
#define UPDATE_ENABLED              1      // 1 = compile the device-tier update path
//  Where the published artifact list lives: this project's GitHub Pages host, which
//  CI deploys to on a `v*` tag (.github/workflows/publish.yml, docs/ORIENTATION.md
//  "Releasing"). Compiled in so a freshly flashed device can check for updates
//  without anyone typing a URL into it — which is the whole point for an operator
//  who was handed a device rather than building one.
//
//  A stored NVS override still WINS over this (platform/esp32/update_source.h reads
//  the override first, this second). That ordering is what keeps the default from
//  being a trap: a firmware image can only be replaced through the mechanism this
//  URL points at, so a device whose publish host moved would otherwise have no way
//  to be told where the new one is. The override is the way out of that circle, and
//  it is reachable from the device's own setup page.
//
//  A FORK SHOULD CHANGE THIS. It names where *this* project publishes; a fork that
//  leaves it pointed here hands its devices somebody else's firmware. Empty is a
//  legitimate value — it means "no source compiled in", and the UPDATES screen says
//  so honestly rather than failing against a placeholder.
//
//  Overridable from the build system, the same way version.h takes its numbers, so
//  pointing a board at a laptop serving `make manifest` needs no edit here (the
//  escaped quotes are load-bearing — the macro has to expand to a string literal):
//
//    PLATFORMIO_BUILD_FLAGS='-DUPDATE_MANIFEST_URL=\"http://192.168.1.50:8000/manifest.json\"' \
//      pio run -e waveshare_s3_154 -t upload
#ifndef UPDATE_MANIFEST_URL
#  define UPDATE_MANIFEST_URL       "https://antvena.github.io/Malwarium/manifest.json"
#endif

// --- Persistence (NVS / Preferences) ---------------------------------------
//  The save blob lives under this namespace/key (ESP32 SaveStore). NVS
//  namespaces are <=15 chars. It lives in NVS_SAVE_PARTITION — a dedicated
//  256KB partition (partitions_malwarium.csv, board_build.partitions), not
//  the stock 20KB `nvs` partition (which stays default-sized for Wi-Fi/BT
//  calibration data) — the save blob outgrew that 20KB as save fields were
//  added across versions. Preferences::begin's partition_label parameter
//  (save_store_esp32.h) is what actually routes to it.
#define NVS_NAMESPACE           "malwarium"
#define NVS_SAVE_KEY            "save"
#define NVS_SAVE_PARTITION      "malsave"
//  Mirrors partitions_malwarium.csv's malsave size — kept as its own constant
//  (not derived at runtime; Preferences has no "partition capacity" query) so
//  the boot-line print (main.cpp) can show blob-size-vs-capacity. Update this
//  alongside the CSV if that partition is ever resized.
#define NVS_SAVE_PARTITION_BYTES (256u * 1024u)

//  Home-network credentials (platform/esp32/net_credentials.h) live in their own
//  NVS namespace, NOT in the save blob: the save is portable — it round-trips
//  through the ARCH rack and is described to the web 'Pedia — and src/core is
//  platform-agnostic, so a passphrase has no business in either. Same partition,
//  separate namespace, so a Factory Reset of the save leaves the network alone.
//  Stored PLAINTEXT (stock NVS is not encrypted); anyone with the flash can read
//  it back.
#define NVS_WIFI_NAMESPACE      "malwifi"
#define NVS_WIFI_SSID_KEY       "ssid"
#define NVS_WIFI_PASS_KEY       "pass"

//  The update source (platform/esp32/update_source.h) gets its own namespace for
//  the same reason: it is device provisioning, not game state, and must survive a
//  Factory Reset of the save.
#define NVS_UPDATE_NAMESPACE    "malupd"
#define NVS_UPDATE_URL_KEY      "manifest"

#endif // BOARD_WAVESHARE_S3_154



// --- Cross-board fallbacks (a board block may omit optional features) --------
#ifndef HAS_BACKLIGHT_PWM
#  define HAS_BACKLIGHT_PWM      0    // default: plain on/off backlight
#endif
#ifndef AP_ENABLED
#  define AP_ENABLED             0    // default: no 'Pedia AP (a board with no radio path)
#endif
#ifndef NET_SNIFF_ENABLED
#  define NET_SNIFF_ENABLED      0    // default: no audit-scan path (radio-less board)
#endif
#ifndef LINK_ENABLED
#  define LINK_ENABLED           0    // default: no pet-to-pet path (radio-less board)
#endif
#ifndef STA_ENABLED
#  define STA_ENABLED            0    // default: no home-network path (radio-less board)
#endif
#if STA_ENABLED
#  ifndef STA_CONNECT_TIMEOUT_MS
#    define STA_CONNECT_TIMEOUT_MS 20000
#  endif
#endif
#ifndef UPDATE_ENABLED
#  define UPDATE_ENABLED         0    // default: no update path (no radio, or one app slot)
#endif
#if UPDATE_ENABLED && !STA_ENABLED
#  error "UPDATE_ENABLED needs STA_ENABLED — there is nothing to fetch a manifest over."
#endif
#if UPDATE_ENABLED
#  ifndef UPDATE_MANIFEST_URL
#    define UPDATE_MANIFEST_URL  ""   // no compiled default; a stored override is the only source
#  endif
#endif
#ifndef USB_KEEPS_AWAKE
#  define USB_KEEPS_AWAKE        0    // default: no USB-presence signal on this board
#endif
#ifndef CPU_ACTIVE_FREQ_MHZ
#  define CPU_ACTIVE_FREQ_MHZ    240
#endif
#ifndef CPU_IDLE_FREQ_MHZ
#  define CPU_IDLE_FREQ_MHZ      CPU_ACTIVE_FREQ_MHZ   // default: no throttle
#endif
#ifndef USB_SETTLE_MS
#  define USB_SETTLE_MS          400
#endif
#if LINK_ENABLED
#  ifndef PEER_LINK_CHANNEL
#    define PEER_LINK_CHANNEL    1
#  endif
#  ifndef PEER_BEACON_INTERVAL_MS
#    define PEER_BEACON_INTERVAL_MS 1000
#  endif
#  ifndef PEER_BURST_MS
#    define PEER_BURST_MS        2000
#  endif
#endif
#if AP_ENABLED
#  ifndef AP_SSID
#    define AP_SSID              "Malwarium"
#  endif
#  ifndef AP_PASSWORD
#    define AP_PASSWORD          ""
#  endif
#  ifndef AP_CHANNEL
#    define AP_CHANNEL           1
#  endif
#endif


// =============================================================================
//  COMPILE-TIME SANITY CHECKS (catch a half-filled config early)
// =============================================================================
#if defined(BUTTON_ACTIVE_LOW)
#  if (PIN_BTN_A < 0) || (PIN_BTN_B < 0) || (PIN_BTN_C < 0)
#    warning "One or more button pins are still unset (-1). Fill them in config.h before relying on input. See tools/pin_finder.ino."
#  endif
#endif


// =============================================================================
//  platformio.ini  (reference — keep in repo root, not here)
//  -------------------------------------------------------------------------
//  [env:waveshare_s3_154]
//  platform     = espressif32
//  board        = esp32-s3-devkitc-1
//  framework    = arduino
//  build_flags  = -DBOARD_WAVESHARE_S3_154=1 -std=gnu++17
//  lib_deps     = moononournation/GFX Library for Arduino
//  -------------------------------------------------------------------------
//  (If you set the BOARD_* values directly in this file, the -D flags are
//   optional — but selecting the board from build_flags lets a second board's
//   env coexist without editing config.h each time.)
// =============================================================================
