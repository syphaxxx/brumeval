#include "options.hpp"

#include "i18n.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "audio.hpp"
#include "store.hpp"

std::string optionsFile = "options.txt";

// Par défaut : ZQSD en AZERTY (WASD en QWERTY : SDL désigne la place de la touche), Espace, Retour arrière, Tab
std::array<SDL_Scancode, N_KEYS> Options::defaultKeys() {
  return {SDL_SCANCODE_W, SDL_SCANCODE_S, SDL_SCANCODE_A, SDL_SCANCODE_D, SDL_SCANCODE_SPACE, SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_TAB};
}

Options& options() {
  static Options o;
  return o;
}

static const char* KEY_IDS[N_KEYS] = {"haut", "bas", "gauche", "droite", "valider", "retour", "auto"};
static const char* KEY_NAMES[N_KEYS] = {"Haut", "Bas", "Gauche", "Droite", "Valider", "Retour, menu", "Mode auto (combat)"};
const char* keyActionName(int a) { return T(KEY_NAMES[std::clamp(a, 0, N_KEYS - 1)]); }

std::string keyName(SDL_Scancode k) {
  switch (k) {
    case SDL_SCANCODE_SPACE: return T("Espace");
    case SDL_SCANCODE_BACKSPACE: return T("Retour arrière");
    case SDL_SCANCODE_RETURN: return T("Entrée");
    case SDL_SCANCODE_ESCAPE: return T("Échap");
    case SDL_SCANCODE_TAB: return T("Tab");
    case SDL_SCANCODE_LSHIFT: return T("Maj gauche");
    case SDL_SCANCODE_RSHIFT: return T("Maj droite");
    case SDL_SCANCODE_LCTRL: return T("Ctrl gauche");
    case SDL_SCANCODE_RCTRL: return T("Ctrl droit");
    case SDL_SCANCODE_UP: return T("Flèche haut");
    case SDL_SCANCODE_DOWN: return T("Flèche bas");
    case SDL_SCANCODE_LEFT: return T("Flèche gauche");
    case SDL_SCANCODE_RIGHT: return T("Flèche droite");
    default: break;
  }
  const char* n = SDL_GetKeyName(SDL_GetKeyFromScancode(k));
  if (!n || !*n) n = SDL_GetScancodeName(k);  // clavier pas encore ouvert (mode test) : nom de la place de la touche
  return n && *n ? n : "?";
}

static std::string path() { return userFile(optionsFile); }

void loadOptions() {
  Options& o = options();
  o = Options{};
  std::ifstream f(path());
  if (!f) {  // premier lancement : la langue de l'ordinateur (français, sinon anglais)
    if (SDL_Locale* loc = SDL_GetPreferredLocales()) {
      if (loc[0].language) o.english = std::string(loc[0].language) != "fr";
      SDL_free(loc);
    }
  }
  std::string line;
  while (std::getline(f, line)) {
    std::istringstream s(line);
    std::string k;
    int v = 0;
    if (!(s >> k >> v)) continue;
    if (k == "plein_ecran") o.fullscreen = v != 0;
    else if (k == "musique") o.music = std::clamp(v, 0, 10);
    else if (k == "effets") o.effects = std::clamp(v, 0, 10);
    else if (k == "vitesse") o.speed = std::clamp(v, 1, 3);
    else if (k == "astuces") o.tips = v != 0;
    else if (k == "anglais") o.english = v != 0;
    else
      for (int a = 0; a < N_KEYS; a++)
        if (k == std::string("touche_") + KEY_IDS[a] && v > 0 && v < SDL_NUM_SCANCODES) o.keys[a] = SDL_Scancode(v);
  }
  applyVolumes();
}

void saveOptions() {
  const Options& o = options();
  std::ostringstream f;
  f << "plein_ecran " << (o.fullscreen ? 1 : 0) << "\nmusique " << o.music << "\neffets " << o.effects << "\nvitesse " << o.speed << "\nastuces " << (o.tips ? 1 : 0) << "\nanglais " << (o.english ? 1 : 0) << '\n';
  for (int a = 0; a < N_KEYS; a++) f << "touche_" << KEY_IDS[a] << ' ' << (int)o.keys[a] << '\n';
  writeUserFile(optionsFile, f.str());
}

void applyVolumes() { audio::setVolumes(options().music, options().effects); }
