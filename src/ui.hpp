// Interface : clavier, files de messages (dialogues) et menus à curseur.
#pragma once
#include <deque>
#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

#include "gfx.hpp"

enum Dir { UP = 0, DOWN = 1, LEFT = 2, RIGHT = 3 };

struct Input {
  bool hold[4] = {};   // direction maintenue
  bool press[4] = {};  // direction appuyée cette image (avec répétition)
  bool confirm = false, cancel = false, menu = false;
  // Souris (coordonnées en pixels du jeu, 320x240)
  int mx = -1, my = -1;
  bool mouseOn = false;    // la souris a servi récemment (sinon on l'ignore)
  bool mdown[3] = {};      // boutons enfoncés : 0 gauche, 1 milieu, 2 droit
  bool mclick[3] = {};     // boutons appuyés cette image
  bool moved = false;
  int wheel = 0;           // molette : +1 vers le haut, -1 vers le bas
  bool ctrl = false, shift = false;
  bool runPad = false;  // bouton X de la manette maintenu (courir, 3.3) ; au clavier : Maj
  bool run() const { return shift || runPad; }
  // Raccourcis des éditeurs
  bool tab = false, undo = false, redo = false, saveKey = false, prev = false, next = false, del = false;
  void endFrame() {
    for (bool& p : press) p = false;
    confirm = cancel = menu = false;
    tab = undo = redo = saveKey = prev = next = del = false;
    for (bool& c : mclick) c = false;
    moved = false;
    wheel = 0;
  }
};

// File d'étapes : messages, appels de fonctions et pauses, exécutés dans l'ordre.
// C'est ce qui permet d'écrire les scènes et les combats comme une suite d'actions.
class Script {
 public:
  void say(const std::string& text, float autoSec = 0);  // autoSec = 0 : attendre une touche
  void call(std::function<void()> fn);
  void wait(float sec);
  // Exécute fn tout de suite ; les étapes qu'il ajoute passent avant celles déjà en attente
  // (utile après un choix Oui/Non ou à la fin d'un combat).
  void runNow(const std::function<void()>& fn);
  void halt() { halted_ = true; }    // en pause jusqu'à resume() (ex. un menu de choix)
  void resume() { halted_ = false; }
  bool busy() const { return !q_.empty() || halted_; }
  bool showingMessage() const { return !q_.empty() && q_.front().kind == Msg && started_; }
  void update(float dt, Input& in);
  void clear() { q_.clear(); halted_ = false; started_ = false; }
  // Dernier message affiché (reste visible en combat)
  const std::string& text() const { return text_; }
  int visibleChars() const { return (int)typed_; }
  bool messageDone() const;

 private:
  enum Kind { Msg, Call, Wait };
  struct Step {
    Kind kind;
    std::string text;
    std::function<void()> fn;
    float dur = 0;
  };
  void push(Step s);
  std::deque<Step> q_;
  std::vector<Step> pending_;
  bool collecting_ = false, halted_ = false, started_ = false;
  float t_ = 0, typed_ = 0;
  std::string text_;
};

struct MenuItem {
  std::string label, right, help;
  bool enabled = true;
  std::function<void()> act;
  std::function<void()> hover;
  // Valeur réglable : gauche/droite appellent adjust(-1 ou +1) au lieu de changer de page
  std::function<void(int)> adjust;
  std::function<std::string()> rightFn;  // texte de droite recalculé à chaque image
  bool header = false;                   // titre de section : le curseur le saute
  uint32_t color = 0;                    // couleur du libellé (0 : blanc)
  bool shrink = false;                   // libellé coupé avec « … » s'il manque de place (sinon le mode test le signale)
  std::string icon;                      // icône avant le libellé : un sprite (« ic_sac ») ou un dessin de menuGlyph (ui.cpp)
  bool sep = false;                      // un fin filet au-dessus : début d'un nouveau groupe d'entrées
  std::function<int()> notches;          // réglage de 0 à 10 dessiné en crans à droite (à la place du texte de droite)
  float bar = -1;                        // de 0 à 1 : une fine barre d'avancement sous la ligne (-1 : aucune)
};
// Titre de section dans un menu
inline MenuItem menuHeader(const std::string& label) {
  MenuItem it{label, "", "", false};
  it.header = true;
  return it;
}

struct Menu;
// Donne une icône (MenuItem::icon) aux entrées de ce libellé (déjà traduit)
void setIcons(Menu& m, std::initializer_list<std::pair<std::string, const char*>> icons);

struct Menu {
  std::string title;
  std::vector<MenuItem> items;
  int sel = 0, top = 0;
  int x = 8, y = 8, w = 120, rows = 6;
  bool cancelable = true;
  std::function<void()> onCancel;
  bool hidden = false;  // pas dessiné, ni cliquable : le choix se fait ailleurs (damier du combat) ; aide et touches restent
};

class MenuStack {
 public:
  void push(Menu m);
  void pop();
  void clear() { st_.clear(); }
  bool active() const { return !st_.empty(); }
  size_t depth() const { return st_.size(); }
  size_t shownDepth() const;  // menus dessinés (sans les menus cachés, Menu::hidden)
  Menu& top() { return st_.back(); }
  int maxRight() const;  // bord droit le plus à droite des menus ouverts (0 si aucun)
  void update(Input& in);
  void draw(Gfx& g, float t) const;
  std::string help() const;

 private:
  void hover();
  std::vector<Menu> st_;
};

// Les n premiers caractères d'un texte UTF-8 (effet machine à écrire)
std::string utf8Prefix(const std::string& s, int n);

// (2026-10-25) Option « Types lisibles sans couleur » (daltonisme) : le motif du type (5x5 pixels sombres sur un carré
// de sa couleur) centré en (cx, cy), à poser sur chaque pastille de type ; faux (rien de dessiné) si l'option est coupée
bool typeMark(Gfx& g, float cx, float cy, int type, uint8_t alpha = 255);
// Petit dessin d'une entrée de menu sans sprite (« piece », « etoile », « sablier »…) dans la case de 11x11 en (x, y)
void menuGlyph(Gfx& g, const std::string& n, float x, float y, uint8_t alpha = 255);
