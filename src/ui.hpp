// Interface : clavier, files de messages (dialogues) et menus à curseur.
#pragma once
#include <deque>
#include <functional>
#include <string>
#include <vector>

#include "gfx.hpp"

enum Dir { UP = 0, DOWN = 1, LEFT = 2, RIGHT = 3 };

struct Input {
  bool hold[4] = {};   // direction maintenue
  bool press[4] = {};  // direction appuyée cette image (avec répétition)
  bool confirm = false, cancel = false, menu = false;
  void endFrame() {
    for (bool& p : press) p = false;
    confirm = cancel = menu = false;
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
};

struct Menu {
  std::string title;
  std::vector<MenuItem> items;
  int sel = 0, top = 0;
  int x = 8, y = 8, w = 120, rows = 6;
  bool cancelable = true;
  std::function<void()> onCancel;
};

class MenuStack {
 public:
  void push(Menu m);
  void pop();
  void clear() { st_.clear(); }
  bool active() const { return !st_.empty(); }
  size_t depth() const { return st_.size(); }
  Menu& top() { return st_.back(); }
  void update(Input& in);
  void draw(Gfx& g, float t) const;
  std::string help() const;

 private:
  void hover();
  std::vector<Menu> st_;
};

// Utilitaire : texte « à gauche / à droite » dans une ligne de menu
std::string fmtInt(int v);
// Les n premiers caractères d'un texte UTF-8 (effet machine à écrire)
std::string utf8Prefix(const std::string& s, int n);
