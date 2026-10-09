// Point d'entrée : ouvre la fenêtre et fait tourner la boucle du jeu.
//   brumeval            lance le jeu
//   brumeval --test DIR mode test : rejoue des situations, vérifie l'équilibrage
//                       et enregistre des captures d'écran dans DIR
#include <SDL.h>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>

#include "audio.hpp"
#include "events.hpp"
#include "game.hpp"
#include "world.hpp"

// Charge tout le dossier data/. Renvoie le message d'erreur (vide si tout va bien).
static std::string loadAll() {
  try {
    loadData();
    loadMaps();
    loadEvents();
    audio::loadSounds();
  } catch (const std::exception& e) {
    return e.what();
  }
  return "";
}
// Problèmes de cohérence (référence inconnue, lieu inaccessible…)
static std::vector<std::string> problems() {
  std::vector<std::string> p = checkData();
  for (auto& v : {checkMaps(), checkEvents(), audio::checkSounds()}) p.insert(p.end(), v.begin(), v.end());
  return p;
}

// ---------------------------------------------------------------------------
// Affichage : le jeu est dessiné dans une image de 240 pixels de haut, aussi
// large que la forme de la fenêtre le demande (320 pixels au moins ; les menus
// restent dans la zone de 320 pixels du milieu, voir Gfx::ox), puis agrandie
// pour remplir toute la fenêtre ou tout l'écran, sans bandes noires. Pour des
// pixels nets et réguliers à n'importe quelle taille, l'image est d'abord
// agrandie d'un nombre entier de fois (pixels carrés), puis ajustée en douceur
// à la taille finale.
// ---------------------------------------------------------------------------
class Screen {
 public:
  static constexpr int MAX_W = 576;  // écran encore plus large (plus de 2,4 fois sa hauteur) : bandes sur les côtés

  explicit Screen(SDL_Renderer* r) : r_(r) {}
  ~Screen() { reset(); }
  // Textures perdues (changement de carte graphique, mise en veille…) : elles seront recréées
  void reset() {
    if (scene_) SDL_DestroyTexture(scene_);
    if (big_) SDL_DestroyTexture(big_);
    scene_ = big_ = nullptr;
    bigK_ = 0;
  }
  // À appeler avant de dessiner une image du jeu : choisit sa largeur selon la fenêtre
  void begin(Gfx& g) {
    SDL_SetRenderTarget(r_, nullptr);
    int ow = 1, oh = 1;
    SDL_GetRendererOutputSize(r_, &ow, &oh);
    scale_ = std::max(.25f, oh / float(SCREEN_H));  // la hauteur remplit la fenêtre
    int w = (int)std::ceil(ow / scale_ - 1e-3f);
    if (w < SCREEN_W) {  // fenêtre plus étroite que le jeu : c'est la largeur qui remplit
      w = SCREEN_W;
      scale_ = std::max(.25f, ow / float(SCREEN_W));
    }
    w = std::min(w + (w & 1), MAX_W);  // largeur paire : la zone du milieu tombe sur un pixel entier
    if (w != w_) {
      reset();
      w_ = w;
    }
    int dw = (int)std::lround(w_ * scale_), dh = (int)std::lround(SCREEN_H * scale_);
    dst_ = {(ow - dw) / 2, (oh - dh) / 2, dw, dh};
    if (!scene_) {
      scene_ = SDL_CreateTexture(r_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, w_, SCREEN_H);
      SDL_SetTextureScaleMode(scene_, SDL_ScaleModeNearest);
      SDL_SetTextureBlendMode(scene_, SDL_BLENDMODE_NONE);
    }
    g.fullW = w_;
    g.ox = (w_ - SCREEN_W) / 2;
    SDL_SetRenderTarget(r_, scene_);
  }
  // Affiche l'image dessinée, agrandie à la taille de la fenêtre
  void present() {
    SDL_SetRenderTarget(r_, nullptr);
    SDL_SetRenderDrawColor(r_, 0, 0, 0, 255);
    SDL_RenderClear(r_);
    SDL_Texture* src = scene_;
    if (std::fabs(scale_ - std::round(scale_)) > 1e-3f) {  // taille non entière : passer par une image agrandie
      int k = std::max(2, (int)std::ceil(scale_));
      if (bigK_ != k) {
        if (big_) SDL_DestroyTexture(big_);
        big_ = SDL_CreateTexture(r_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, w_ * k, SCREEN_H * k);
        SDL_SetTextureScaleMode(big_, SDL_ScaleModeLinear);
        SDL_SetTextureBlendMode(big_, SDL_BLENDMODE_NONE);
        bigK_ = k;
      }
      if (big_) {
        SDL_SetRenderTarget(r_, big_);
        SDL_RenderCopy(r_, scene_, nullptr, nullptr);
        SDL_SetRenderTarget(r_, nullptr);
        src = big_;
      }
    }
    SDL_RenderCopy(r_, src, nullptr, &dst_);
    SDL_RenderPresent(r_);
  }
  // Position dans la fenêtre -> position dans le jeu (la zone de 320x240 du milieu commence à 0)
  void toGame(SDL_Window* w, const Gfx& g, int x, int y, int& gx, int& gy) const {
    int ww = 1, wh = 1, ow = 1, oh = 1;
    SDL_GetWindowSize(w, &ww, &wh);
    SDL_GetRendererOutputSize(r_, &ow, &oh);
    float px = x * float(ow) / std::max(1, ww), py = y * float(oh) / std::max(1, wh);
    gx = (int)std::floor((px - dst_.x) / scale_) - g.ox;
    gy = (int)std::floor((py - dst_.y) / scale_);
  }

 private:
  SDL_Renderer* r_;
  SDL_Texture *scene_ = nullptr, *big_ = nullptr;
  int w_ = 0, bigK_ = 0;
  float scale_ = 1;
  SDL_Rect dst_{0, 0, SCREEN_W, SCREEN_H};
};

// Taille de départ de la fenêtre : la forme de l'écran, aussi grande que possible
// sans la barre des tâches ni la barre de titre
static void fitWindow(SDL_Window* win) {
  int d = std::max(0, SDL_GetWindowDisplayIndex(win));
  SDL_Rect ub, full;
  if (SDL_GetDisplayUsableBounds(d, &ub) != 0 || SDL_GetDisplayBounds(d, &full) != 0) return;
  int top = 0, left = 0, bottom = 0, right = 0;
  if (SDL_GetWindowBordersSize(win, &top, &left, &bottom, &right) != 0) top = 40, left = right = bottom = 8;
  float aspect = std::clamp(full.w / float(std::max(1, full.h)), SCREEN_W / float(SCREEN_H), Screen::MAX_W / float(SCREEN_H));
  float s = std::min((ub.w - left - right) * .95f / (SCREEN_H * aspect), (ub.h - top - bottom) * .95f / SCREEN_H);
  if (s >= 2 && s - std::floor(s) < .15f) s = std::floor(s);  // un nombre entier de fois si c'est presque pareil
  s = std::max(1.f, s);
  int h = (int)std::lround(SCREEN_H * s), w = (int)std::lround(SCREEN_H * s * aspect);
  SDL_SetWindowSize(win, w, h);
  SDL_SetWindowPosition(win, ub.x + left + (ub.w - left - right - w) / 2, ub.y + top + (ub.h - top - bottom - h) / 2);
}

// Préférence plein écran, gardée d'une partie à l'autre (options.txt, à côté de la sauvegarde)
static std::string optionsPath() {
  char* p = SDL_GetPrefPath("Brumeval", "Brumeval");
  std::string s = p ? p : "";
  SDL_free(p);
  return s + "options.txt";
}
static bool loadFullscreen() {
  std::ifstream f(optionsPath());
  std::string k;
  int v = 0;
  while (f >> k >> v)
    if (k == "plein_ecran") return v != 0;
  return false;
}
static void saveFullscreen(bool on) {
  std::ofstream f(optionsPath());
  f << "plein_ecran " << (on ? 1 : 0) << '\n';
}

static int runTests(const char* outDir) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);  // affichage immédiat, même en cas de plantage
  SDL_Init(0);
  std::string err = loadAll();
  if (!err.empty()) {
    std::printf("[ÉCHEC]  chargement de data/ : %s\n", err.c_str());
    return 1;
  }
  // BRUMEVAL_LARGEUR=384 : captures au format d'un écran large (16:10), pour vérifier les décors élargis
  const char* wEnv = SDL_getenv("BRUMEVAL_LARGEUR");
  int w = std::clamp(wEnv ? std::atoi(wEnv) : SCREEN_W, SCREEN_W, Screen::MAX_W);
  w += w & 1;
  SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, w, SCREEN_H, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* r = SDL_CreateSoftwareRenderer(surf);
  int code = 1;
  {
    Game game(r);
    game.g.fullW = w;
    game.g.ox = (w - SCREEN_W) / 2;
    code = game.selfTest(surf, outDir);
  }
  SDL_DestroyRenderer(r);
  SDL_FreeSurface(surf);
  SDL_Quit();
  return code;
}

int main(int argc, char* argv[]) {
  if (argc > 1 && std::string(argv[1]) == "--test") return runTests(argc > 2 ? argv[2] : "captures");

#ifdef _WIN32
  // Lancé par un double-clic, le jeu reçoit une console noire à lui seul : on la ferme.
  // Lancé depuis un terminal (plusieurs programmes sur la console), on la laisse.
  DWORD procs[2];
  if (GetConsoleProcessList(procs, 2) == 1) FreeConsole();
#endif
  // Windows : tenir compte du zoom de l'écran (125 %, 150 %…) pour une image nette,
  // au lieu de laisser Windows agrandir (et flouter) la fenêtre
  SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
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
  SDL_Window* win = SDL_CreateWindow("Brumeval", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_W * 3, SCREEN_H * 3,
                                     SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN);
  if (!win) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Brumeval", SDL_GetError(), nullptr);
    return 1;
  }
  fitWindow(win);
  SDL_SetWindowMinimumSize(win, SCREEN_W, SCREEN_H);
  SDL_ShowWindow(win);
  bool fullscreen = loadFullscreen();
  if (fullscreen) SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN_DESKTOP);
  SDL_Renderer* r = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
  if (!r) r = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE | SDL_RENDERER_TARGETTEXTURE);
  if (!r) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Brumeval", SDL_GetError(), nullptr);
    return 1;
  }

  audio::init();
  audio::setVolumes(6, 7);
  {
    Screen screen(r);
    Game game(r);
    Uint64 last = SDL_GetPerformanceCounter();
    while (!game.quit) {
      SDL_Event e;
      while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) game.quit = true;
        if (e.type == SDL_RENDER_DEVICE_RESET) screen.reset();
        if (e.type == SDL_TEXTINPUT) {
          game.onText(e.text.text);
          continue;
        }
        if (e.type == SDL_MOUSEMOTION) {
          int x, y;
          screen.toGame(win, game.g, e.motion.x, e.motion.y, x, y);
          game.onMouse(x, y, -1, false);
          continue;
        }
        if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) {
          int b = e.button.button == SDL_BUTTON_LEFT ? 0 : e.button.button == SDL_BUTTON_RIGHT ? 2 : 1;
          int x, y;
          screen.toGame(win, game.g, e.button.x, e.button.y, x, y);
          game.onMouse(x, y, b, e.type == SDL_MOUSEBUTTONDOWN);
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
          fullscreen = !fullscreen;  // F11 ou Alt+Entrée : plein écran (gardé pour la prochaine fois)
          SDL_SetWindowFullscreen(win, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
          saveFullscreen(fullscreen);
          continue;
        }
        game.onKey(k, e.type == SDL_KEYDOWN, e.key.repeat != 0);
      }
      Uint64 now = SDL_GetPerformanceCounter();
      float dt = std::min(.05f, float(now - last) / float(SDL_GetPerformanceFrequency()));
      last = now;
      game.update(dt);
      screen.begin(game.g);
      game.draw();
      screen.present();
      SDL_Delay(1);
    }
  }
  audio::shutdown();
  SDL_DestroyRenderer(r);
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
