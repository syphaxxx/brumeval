// Point d'entrée : ouvre la fenêtre et fait tourner la boucle du jeu.
//   brumeval            lance le jeu
//   brumeval --test DIR mode test : rejoue des situations, vérifie l'équilibrage
//                       et enregistre des captures d'écran dans DIR
#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "events.hpp"
#include "game.hpp"
#include "world.hpp"

// Charge tout le dossier data/. Renvoie le message d'erreur (vide si tout va bien).
static std::string loadAll() {
  try {
    loadData();
    loadMaps();
    loadEvents();
  } catch (const std::exception& e) {
    return e.what();
  }
  return "";
}
// Problèmes de cohérence (référence inconnue, lieu inaccessible…)
static std::vector<std::string> problems() {
  std::vector<std::string> p = checkData();
  for (auto& v : {checkMaps(), checkEvents()}) p.insert(p.end(), v.begin(), v.end());
  return p;
}

static int runTests(const char* outDir) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);  // affichage immédiat, même en cas de plantage
  SDL_Init(0);
  std::string err = loadAll();
  if (!err.empty()) {
    std::printf("[ÉCHEC]  chargement de data/ : %s\n", err.c_str());
    return 1;
  }
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
  std::string err = loadAll();
  if (!err.empty()) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Brumeval : erreur dans les données", err.c_str(), nullptr);
    return 1;
  }
  auto warn = problems();
  if (!warn.empty()) {
    std::string msg = "Le jeu va démarrer, mais les données contiennent des problèmes :\n";
    for (size_t i = 0; i < warn.size() && i < 12; i++) msg += "\n- " + warn[i];
    if (warn.size() > 12) msg += "\n… et " + std::to_string(warn.size() - 12) + " autres.";
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Brumeval", msg.c_str(), nullptr);
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
        if (e.type == SDL_TEXTINPUT) {
          game.onText(e.text.text);
          continue;
        }
        if (e.type == SDL_MOUSEMOTION) {
          game.onMouse(e.motion.x, e.motion.y, -1, false);
          continue;
        }
        if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) {
          int b = e.button.button == SDL_BUTTON_LEFT ? 0 : e.button.button == SDL_BUTTON_RIGHT ? 2 : 1;
          game.onMouse(e.button.x, e.button.y, b, e.type == SDL_MOUSEBUTTONDOWN);
          continue;
        }
        if (e.type == SDL_MOUSEWHEEL) {
          game.onWheel(e.wheel.y);
          continue;
        }
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
