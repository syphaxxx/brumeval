// Options du joueur (écran titre > Options, ou menu de pause > Options) : volumes,
// plein écran, vitesse du jeu et touches du clavier. Gardées dans options.txt, à côté de la
// sauvegarde, d'une partie à l'autre.
//
// Les flèches, Entrée et Échap marchent toujours, en plus des touches choisies :
// on ne peut pas se retrouver sans moyen de jouer.
#pragma once
#include <SDL.h>

#include <array>
#include <string>

// Actions du clavier que le joueur peut changer
enum KeyAction { K_UP, K_DOWN, K_LEFT, K_RIGHT, K_OK, K_BACK, K_AUTO, N_KEYS };

struct Options {
  bool fullscreen = false;
  int music = 6, effects = 7;  // volumes, 0 à 10
  int speed = 1;               // vitesse du jeu : ×1, ×2 ou ×3 (F2), voir Game::speed
  bool tips = true;
  bool autosave = true;        // sauvegarde automatique à chaque passage et porte (2026-10-20)
  bool english = false;        // langue (3.1) : anglais au lieu du français (data/langues/en.json)            // astuces (2.8) : chaque nouvelle règle expliquée la première fois
  std::array<SDL_Scancode, N_KEYS> keys = defaultKeys();
  static std::array<SDL_Scancode, N_KEYS> defaultKeys();
};

Options& options();
extern std::string optionsFile;  // « options.txt » ; le mode test en utilise un autre
void loadOptions();
void saveOptions();
void applyVolumes();  // transmet les volumes au son (audio.hpp)

const char* keyActionName(int a);    // « Haut », « Valider »…
std::string keyName(SDL_Scancode k);  // nom de la touche selon la disposition du clavier (« Z » en AZERTY)
