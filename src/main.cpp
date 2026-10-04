// Point d'entrée : ouvre la fenêtre et fait tourner la boucle du jeu.
//   brumeval            lance le jeu
//   brumeval --test DIR mode test : rejoue des situations, vérifie l'équilibrage
//                       et enregistre des captures d'écran dans DIR
#include <SDL.h>

#include <algorithm>
#include <string>

#include "game.hpp"

static int runTests(const char* outDir) {
  SDL_Init(0);
  SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_W, SCREEN_H, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* r = SDL_CreateSoftwareRenderer(surf);
  int code = 1;
  {
    Game game(r);
    code = game.selfTest(surf, outDir);
  }
  SDL_DestroyRenderer(r);
  SDL_FreeSurface(surf);
  SDL_Quit();
  return code;
}

int main(int argc, char* argv[]) {
  if (argc > 1 && std::string(argv[1]) == "--test") return runTests(argc > 2 ? argv[2] : "captures");

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Brumeval", SDL_GetError(), nullptr);
    return 1;
  }
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");  // pixels nets
  SDL_Window* win = SDL_CreateWindow("Brumeval", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_W * 3, SCREEN_H * 3,
                                     SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  if (!win) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Brumeval", SDL_GetError(), nullptr);
    return 1;
  }
  SDL_Renderer* r = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!r) r = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
  SDL_RenderSetLogicalSize(r, SCREEN_W, SCREEN_H);
  SDL_RenderSetIntegerScale(r, SDL_TRUE);

  {
    Game game(r);
    bool fullscreen = false;
    Uint64 last = SDL_GetPerformanceCounter();
    while (!game.quit) {
      SDL_Event e;
      while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) game.quit = true;
        if (e.type != SDL_KEYDOWN && e.type != SDL_KEYUP) continue;
        SDL_Scancode k = e.key.keysym.scancode;
        bool alt = (e.key.keysym.mod & KMOD_ALT) != 0;
        if (e.type == SDL_KEYDOWN && !e.key.repeat && (k == SDL_SCANCODE_F11 || (alt && k == SDL_SCANCODE_RETURN))) {
          fullscreen = !fullscreen;  // F11 ou Alt+Entrée : plein écran
          SDL_SetWindowFullscreen(win, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
          continue;
        }
        game.onKey(k, e.type == SDL_KEYDOWN, e.key.repeat != 0);
      }
      Uint64 now = SDL_GetPerformanceCounter();
      float dt = std::min(.05f, float(now - last) / float(SDL_GetPerformanceFrequency()));
      last = now;
      game.update(dt);
      game.draw();
      SDL_RenderPresent(r);
      SDL_Delay(1);
    }
  }
  SDL_DestroyRenderer(r);
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
