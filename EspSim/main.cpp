#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "Compat.h"

#define SIM_MODE
#include "../Brickintosh.ino"

static SDL_Window* win = nullptr;
static SDL_Renderer* ren = nullptr;
static SDL_Texture* tex = nullptr;

void sdlLoop() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        bool running = true;

        if (e.type == SDL_QUIT) running = false;
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = false;

        if (!running) {
            SDL_DestroyTexture(tex);
            SDL_DestroyRenderer(ren);
            SDL_DestroyWindow(win);
            SDL_Quit();
            exit(0);
        }
    }

    // Upload + present
    if (SDL_UpdateTexture(tex, NULL, gfx.buffer, LCD_WIDTH * (int)sizeof(uint16_t)) != 0) {
        SDL_Log("SDL_UpdateTexture failed: %s", SDL_GetError());
    }
    SDL_SetRenderDrawColor(ren, 16, 16, 16, 255); // visible background even if texture is black
    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, NULL, NULL);
    SDL_RenderPresent(ren);
}

int main(int argc, char* argv[])
{
    SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP, "0");   // make sure we become a foreground GUI app
    SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    // Scale up the window so it's easy to see, but keep pixel-perfect rendering.
    const int logicalW = LCD_WIDTH;
    const int logicalH = LCD_HEIGHT;
    const int scale    = 2; // bump to taste
    const int winW     = logicalW * scale;
    const int winH     = logicalH * scale;

    SDL_Log("LCD %dx%d, window %dx%d", logicalW, logicalH, winW, winH);

    win = SDL_CreateWindow(
        "ESP Sim",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        winW, winH,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );
    if (!win) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return 1;
    }

    SDL_ShowWindow(win);
    SDL_RaiseWindow(win);

    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        return 1;
    }

    // Render at LCD resolution, scaled up by the window size.
    SDL_RenderSetLogicalSize(ren, logicalW, logicalH);
    SDL_RenderSetIntegerScale(ren, SDL_TRUE);

    tex = SDL_CreateTexture(
        ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING,
        logicalW, logicalH
    );
    if (!tex) {
        SDL_Log("SDL_CreateTexture failed: %s", SDL_GetError());
        return 1;
    }

    int outW=0, outH=0; SDL_GetWindowSize(win, &outW, &outH);
    SDL_Log("Actual window size: %dx%d", outW, outH);
    Uint32 pf=0; int acc=0, w=0, h=0;
    SDL_QueryTexture(tex, &pf, &acc, &w, &h);
    SDL_Log("Texture %dx%d format=0x%x access=%d", w, h, pf, acc);

    // Make sure your Arduino state is initialized.
    setup();

    loop(); // let the sketch run

    return 0;
}