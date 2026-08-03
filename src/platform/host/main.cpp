// host/main.cpp — desktop (SDL2) preview of the Malwarium engine.
//
// Renders the logical 128 canvas, upscales x1.75 to the 224 active area,
// composes it into the 240 panel with the 8px bezel, and shows it in a
// zoomed window. Keyboard maps to the three hardware buttons:
//   Z = A (NEXT) · X = B (ACCEPT) · C = C (CANCEL)   (A+C chord = Z+C)
// U cycles the carousel UI Mode; H starves the pet (fires the Lockout crisis);
// E resets to the Decryption Hatch; V forces the Evolution boundary; P dumps the
// current panel to a .ppm.
#include <SDL.h>

#include <chrono>
#include <cstdio>
#include <vector>

#include "core/app/game.h"
#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "dev_config.h"
#include "platform/host/save_store_host.h"

using namespace mal;

namespace {

constexpr int kZoom = 3;  // desktop window magnification of the 240 panel

uint32_t nowMs() {
    using namespace std::chrono;
    static const auto t0 = steady_clock::now();
    return static_cast<uint32_t>(
        duration_cast<milliseconds>(steady_clock::now() - t0).count());
}

// Compose the active (224) canvas into a full 240x240 panel buffer, clearing
// the bezel to the paper field.
void composePanel(const Framebuffer& active, std::vector<Rgb565>& panel) {
    panel.assign(static_cast<size_t>(kPanelW) * kPanelH, palColor(Pal::PAPER));
    for (int y = 0; y < kActiveH; ++y) {
        const Rgb565* src = active.data() + static_cast<size_t>(y) * kActiveW;
        Rgb565* dst = panel.data() + static_cast<size_t>(y + kBezel) * kPanelW + kBezel;
        for (int x = 0; x < kActiveW; ++x) dst[x] = src[x];
    }
}

void dumpPpm(const std::vector<Rgb565>& panel) {
    FILE* f = std::fopen("malwarium_panel.ppm", "wb");
    if (!f) return;
    std::fprintf(f, "P6\n%d %d\n255\n", kPanelW, kPanelH);
    for (Rgb565 c : panel) {
        unsigned char rgb[3] = {r8(c), g8(c), b8(c)};
        std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
    std::puts("dumped malwarium_panel.ppm");
}

bool mapKey(SDL_Keycode k, Button& out) {
    switch (k) {
        case SDLK_z: out = Button::A; return true;
        case SDLK_x: out = Button::B; return true;
        case SDLK_c: out = Button::C; return true;
        default: return false;
    }
}

} // namespace

int main(int, char**) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window* win = SDL_CreateWindow(
        "Malwarium (host preview)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        kPanelW * kZoom, kPanelH * kZoom, SDL_WINDOW_SHOWN);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565,
                                         SDL_TEXTUREACCESS_STREAMING,
                                         kPanelW, kPanelH);

    // Persist to a file in the cwd so the preview survives a relaunch (the save
    // is loaded at boot; an empty save falls back to the start mode below).
    FileSaveStore store;
#ifdef DEV_START_CREATURE
    Game game(StartMode::Hatched, DEV_START_CREATURE, &store);  // dev: skip the egg
#else
    Game game(StartMode::FreshHatch, "paypup", &store);         // real first boot -> Hatch
#endif
    Framebuffer fb(kActiveW, kActiveH);   // active canvas is the compositor
    std::vector<Rgb565> panel;
    bool aDown = false, cDown = false;

    auto repaint = [&]() {
        game.render(fb);
        composePanel(fb, panel);
        SDL_UpdateTexture(tex, nullptr, panel.data(), kPanelW * sizeof(Rgb565));
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, nullptr, nullptr);
        SDL_RenderPresent(ren);
    };
    repaint();  // first paint

    bool running = true;
    while (running) {
        bool dirty = false;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            else if (e.type == SDL_WINDOWEVENT &&
                     e.window.event == SDL_WINDOWEVENT_EXPOSED) dirty = true;
            else if (e.type == SDL_KEYDOWN && !e.key.repeat) {
                if (e.key.keysym.sym == SDLK_p) { dumpPpm(panel); continue; }
                if (e.key.keysym.sym == SDLK_u) { game.cycleUiMode(); dirty = true; continue; }
                if (e.key.keysym.sym == SDLK_ESCAPE) { running = false; continue; }
                if (e.key.keysym.sym == SDLK_h) {     // debug: starve -> fires Lockout
                    game.model().setHunger(0); dirty = true; continue;
                }
                if (e.key.keysym.sym == SDLK_e) {     // dev: reset to the Decryption Hatch
                    game.resetToHatch(); dirty = true; continue;
                }
                if (e.key.keysym.sym == SDLK_v) {     // dev: force the Evolution boundary
                    game.debugTriggerEvolution(); dirty = true; continue;
                }
                Button b;
                if (mapKey(e.key.keysym.sym, b)) {
                    if (b == Button::A) aDown = true;
                    if (b == Button::C) cDown = true;
                    game.onButton({b, true, aDown && cDown});
                    dirty = true;
                }
            } else if (e.type == SDL_KEYUP) {
                Button b;
                if (mapKey(e.key.keysym.sym, b)) {
                    if (b == Button::A) aDown = false;
                    if (b == Button::C) cDown = false;
                    game.onButton({b, false, false});
                }
            }
        }
        // Travel sleep has no host equivalent — there is no SoC to park and no
        // battery to save — so the request is landed as a save and cleared, which
        // leaves the window sitting on the screen the device would have gone dark
        // from. That is the honest host behaviour: the seam is exercised, and the
        // one thing the device tier does that matters to the SAVE still happens.
        if (game.travelSleepRequested() && game.saveNow()) {
            std::puts("[travel] would deep-sleep here; save landed");
            game.clearTravelSleep();
        }
        if (game.tick(nowMs())) dirty = true;
        if (dirty) repaint();
        SDL_Delay(8);  // keep the event loop responsive; repaint stays event-driven
    }

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
