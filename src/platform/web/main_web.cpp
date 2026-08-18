// main_web.cpp — the browser target: the firmware engine, a 2D canvas, and the
// three buttons.
//
// The engine renders in software into an RGB565 Framebuffer and hands it over the
// IDisplay boundary (platform/platform.h), so a target only has to get those bytes
// onto a screen. Here that is one putImageData per repaint, which is why this build
// pulls in NO graphics library at all — the desktop preview's SDL2 (platform/host)
// buys a window, a texture and a key handler, and a browser already has all three.
// The wasm is the engine and nothing else.
//
// Input is the page's job for the same reason: a touchscreen has no keys, so the
// shell drives presses through the mal_button entry point below and binds the
// keyboard itself. One input path, not two that can disagree.
//
// The panel is composed exactly as the device composes it — the 224 active canvas
// centred in the 240 panel with the 8px bezel — so a visitor sees the screen the
// hardware would show.
#include <emscripten.h>

#include <cstdint>
#include <vector>

#include "core/app/game.h"
#include "core/render/canvas.h"
#include "core/render/color.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "platform/web/save_store_web.h"

using namespace mal;

namespace {

// How often an idle session is written back. The device autosaves on the same
// cadence (kSaveAutosaveMs) to capture passive decay without thrashing flash; here
// it is what makes a closed tab cost seconds rather than a session.
constexpr uint32_t kWebAutosaveMs = 5000;

struct App {
    Game* game = nullptr;
    WebSaveStore* store = nullptr;
    Framebuffer* fb = nullptr;
    std::vector<Rgb565> panel;      // the composed 240x240 panel
    std::vector<uint8_t> rgba;      // that panel widened for putImageData
    bool aDown = false, cDown = false;
    bool dirty = true;
    uint32_t lastSaveMs = 0;
};

App g;

uint32_t nowMs() { return static_cast<uint32_t>(emscripten_get_now()); }

// Hand the finished panel to the page. ImageData is built over the wasm heap in
// place — the pixels are not copied into JS, only viewed — so a repaint costs one
// putImageData and no allocation.
EM_JS(void, malWebPresent, (const uint8_t* rgba, int w, int h), {
    const canvas = document.getElementById('screen');
    if (!canvas) return;
    if (canvas.width !== w || canvas.height !== h) { canvas.width = w; canvas.height = h; }
    const ctx = canvas.getContext('2d');
    const view = new Uint8ClampedArray(HEAPU8.buffer, rgba, w * h * 4);
    ctx.putImageData(new ImageData(view, w, h), 0, 0);
});

// Compose the active (224) canvas into the full 240x240 panel, clearing the bezel to
// the paper field — the device's own composition step — then widen RGB565 to the
// RGBA8888 a canvas wants. r8/g8/b8 (core/render/color.h) are the same expansion the
// host preview's PPM dump uses, so the two tiers cannot disagree about a colour.
void composeAndPresent() {
    g.game->render(*g.fb);          // the engine draws the whole active canvas
    const Framebuffer& active = *g.fb;
    g.panel.assign(static_cast<size_t>(kPanelW) * kPanelH, palColor(Pal::PAPER));
    for (int y = 0; y < kActiveH; ++y) {
        const Rgb565* src = active.data() + static_cast<size_t>(y) * kActiveW;
        Rgb565* dst = g.panel.data() + static_cast<size_t>(y + kBezel) * kPanelW + kBezel;
        for (int x = 0; x < kActiveW; ++x) dst[x] = src[x];
    }
    for (size_t i = 0; i < g.panel.size(); ++i) {
        const Rgb565 c = g.panel[i];
        uint8_t* p = &g.rgba[i * 4];
        p[0] = r8(c); p[1] = g8(c); p[2] = b8(c); p[3] = 0xff;
    }
    malWebPresent(g.rgba.data(), kPanelW, kPanelH);
}

// A press edge carries whether A and C are held together, which is how the engine
// tells the Exploit chord from two separate taps (platform.h's ButtonEvent).
void pressEdge(Button b, bool pressed) {
    if (b == Button::A) g.aDown = pressed;
    if (b == Button::C) g.cDown = pressed;
    g.game->onButton({b, pressed, pressed && g.aDown && g.cDown});
    g.dirty = true;
}

// Repaint stays event-driven: the loop wakes with the display but paints only when
// something moved, which is the contract the device's ~4fps repaint keeps.
void frame() {
    const uint32_t t = nowMs();
    if (g.game->tick(t)) g.dirty = true;

    // Travel sleep has no browser equivalent — there is no SoC to park — so the
    // request is landed as a save and cleared, which is the half of it that matters
    // to the visitor's progress.
    if (g.game->travelSleepRequested() && g.game->saveNow())
        g.game->clearTravelSleep();

    if (t - g.lastSaveMs >= kWebAutosaveMs) {
        g.lastSaveMs = t;
        g.game->saveNow();
    }

    if (g.dirty) { composeAndPresent(); g.dirty = false; }
}

} // namespace

// --- The shell page's entry points ------------------------------------------
extern "C" {

// `button` is Button's ordinal: 0=A(NEXT) 1=B(ACCEPT) 2=C(CANCEL). Both edges must
// be sent: a tap/hold settles on the RELEASE edge (CONTRIBUTING.md's button
// contract), so a control that only reported presses would make every hold — the
// ITEMS filter, the list back-out, the CFG holds — impossible.
EMSCRIPTEN_KEEPALIVE void mal_button(int button, int pressed) {
    if (!g.game || button < 0 || button > 2) return;
    pressEdge(static_cast<Button>(button), pressed != 0);
}

// The icons / icons+label / text-only cycle. It lives in CFG on the device, and the
// demo locks CFG (include/demo_config.h), so the shell offers it as a page control
// rather than losing an accessibility setting to the lockdown.
EMSCRIPTEN_KEEPALIVE void mal_cycle_ui_mode() {
    if (!g.game) return;
    g.game->cycleUiMode();
    g.dirty = true;
}

// START OVER: drop this visitor's save so the next boot falls back to the baked
// seed. The page reloads afterwards, which is what re-runs the constructor.
EMSCRIPTEN_KEEPALIVE void mal_reset_demo() {
    if (g.store) g.store->clear();
}

// Written back on page-hide as well as on the autosave cadence, so closing the tab
// keeps the session rather than losing up to kWebAutosaveMs of it.
EMSCRIPTEN_KEEPALIVE void mal_flush_save() {
    if (g.game) g.game->saveNow();
}

} // extern "C"

int main(int, char**) {
    g.store = new WebSaveStore();
    // FreshHatch is a fallback the store never reaches: load() returns either the
    // visitor's own save or the baked seed, and a non-empty blob boots the pet it
    // names regardless of the mode passed here.
    g.game = new Game(StartMode::FreshHatch, "paypup", g.store);
    g.fb = new Framebuffer(kActiveW, kActiveH);
    g.rgba.assign(static_cast<size_t>(kPanelW) * kPanelH * 4, 0xff);
    g.lastSaveMs = nowMs();

    emscripten_set_main_loop(frame, 0, 1);   // 0 = match the display's refresh
    return 0;
}
