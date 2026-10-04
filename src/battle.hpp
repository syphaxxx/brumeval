// Combat au tour par tour actif (jauges ATB), jusqu'à 3 alliés contre 1 à 3 ennemis.
// Quand la jauge ATB d'un allié est pleine, le temps s'arrête et on choisit son action.
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "data.hpp"
#include "ui.hpp"
#include "world.hpp"

class Game;
enum class BattleResult;

class Battle {
 public:
  Battle(Game& game, std::vector<FighterP> foes, bool boss, Theme bg, bool canFlee, bool canCapture);
  void update(float dt);
  void draw();
  bool finished() const { return finished_; }
  BattleResult result() const { return result_; }
  bool autoPlay = false;  // utilisé par le mode test

 private:
  struct Pop {
    float x, y;
    std::string text;
    Color col;
    float t;
  };
  Game& G;
  Script sc;
  std::vector<FighterP> allies, foes;
  bool boss, canFlee, canCapture;
  Theme bg;
  float start = 0;
  FighterP actor, cursor, blink;
  float blinkT = 0, flashT = -1;
  Color flashCol;
  std::vector<Pop> pops;
  std::vector<FighterP> defeated;
  bool finished_ = false, ending_ = false;
  BattleResult result_;

  bool isAlly(const FighterP& f) const;
  std::vector<FighterP> alive(const std::vector<FighterP>& v) const;
  Pt pos(const FighterP& f) const;
  void pop(const FighterP& f, const std::string& t, Color c, float dy = 0);

  void tickATB(float dt);
  void command(FighterP a);
  void autoCommand(FighterP a);
  void enemyTurn(FighterP e);
  void useMove(FighterP a, const std::string& mv, std::vector<FighterP> targets, bool isLimit = false);
  void useItem(FighterP a, const std::string& it, FighterP target);
  void swapIn(FighterP a, FighterP r);
  void tryFlee(FighterP a);
  void afterAction(FighterP a);
  void victory();
  void finish(BattleResult r);
  int applyHit(const FighterP& a, const FighterP& d, const Move& m);

  // Menus de commande
  void pickFoe(const std::string& title, std::function<std::string(const Fighter&)> info, std::function<void(FighterP)> done);
  void pickAlly(const std::string& title, bool ko, std::function<void(FighterP)> done);
  void moveTarget(FighterP a, const std::string& mv);
  friend class Game;
};
