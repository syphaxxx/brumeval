// Combat au tour par tour actif (jauges ATB), jusqu'à 3 alliés contre 1 à 3 ennemis.
// Quand la jauge ATB d'un allié est pleine, le temps s'arrête et on choisit son action,
// sauf si le mode auto est actif (touche Tab) et qu'une de ses tactiques s'applique.
// Les ennemis peuvent avoir des renforts qui entrent quand l'un d'eux tombe.
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "data.hpp"
#include "ui.hpp"
#include "world.hpp"

class Game;
enum class BattleResult;

// Chance de capturer f avec une lanterne de multiplicateur mult (règles : capture)
float captureChance(const Fighter& f, float mult);
// Description courte d'une technique : genre, type, puissance, précision, effets
std::string moveDetails(const Move& m);

// Préparation d'un combat
struct BattleSetup {
  std::vector<FighterP> foes;     // 1 à 3 ennemis en première ligne
  std::vector<FighterP> reserve;  // renforts : entrent un par un quand un ennemi tombe
  std::string foeName;            // nom de l'adversaire (« Garo le braconnier »), vide pour des créatures sauvages
  bool boss = false, canFlee = true, canCapture = true;
  int theme = -1;                 // décor (Theme) ; -1 : celui de la carte actuelle
};

class Battle {
 public:
  Battle(Game& game, BattleSetup setup, Theme bg);
  void update(float dt);
  void draw();
  bool finished() const { return finished_; }
  BattleResult result() const { return result_; }
  bool autoPlay = false;    // les alliés jouent tout seuls (mode test, simulations de l'Arène)
  bool simTactics = false;  // avec autoPlay : les alliés suivent d'abord leurs tactiques
  std::vector<std::string> log;  // journal détaillé des actions (mode test, Arène)

 private:
  struct Pop {
    float x, y;
    std::string text;
    Color col;
    float t;
  };
  // Action choisie par l'ordinateur (ennemis, ou alliés en mode automatique) ou par une tactique
  struct Plan {
    std::string move;
    std::vector<FighterP> targets;
    bool limit = false;
    std::string item;  // objet du sac à utiliser à la place d'une technique
    int tactic = -1;   // numéro de la tactique qui a choisi cette action
  };
  Game& G;
  Script sc;
  std::vector<FighterP> allies, foes, reserve;
  std::string foeName;
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
  void pop(const FighterP& f, const std::string& t, Color c);

  void tickATB(float dt);
  bool skipTurn(FighterP f);  // sommeil ou paralysie : le tour est perdu
  void command(FighterP a);
  void autoCommand(FighterP a);
  void enemyTurn(FighterP e);
  Plan think(FighterP a);
  float attackScore(const FighterP& a, const Move& m, const FighterP& t) const;  // intérêt d'une attaque (IA et tactiques)
  // Tactiques (tactics.hpp) : première règle possible de l'allié a, puis son exécution
  bool tacticsActive() const;
  bool tacticPlan(FighterP a, Plan& out);
  bool tacticTurn(FighterP a);
  float toggledT_ = -10;  // moment où le mode auto a été changé (touche Tab)
  void useMove(FighterP a, const std::string& mv, std::vector<FighterP> targets, bool isLimit = false);
  void useItem(FighterP a, const std::string& it, FighterP target);
  void swapIn(FighterP a, FighterP r);
  void tryFlee(FighterP a);
  void afterAction(FighterP a, float gauge = 0);  // gauge : jauge ATB de départ (technique rapide ou lourde)
  void checkEnd();
  void victory();
  void finish(BattleResult r);
  int applyHit(const FighterP& a, const FighterP& d, const Move& m);  // -1 si raté
  void applyEffect(const FighterP& t, const Effect& e);
  void knockOut(const FighterP& f);

  // Menus de commande
  void pickFoe(const std::string& title, std::function<std::string(const Fighter&)> info, std::function<void(FighterP)> done,
               bool creaturesOnly = false);
  void pickAlly(const std::string& title, bool ko, std::function<void(FighterP)> done);
  void moveTarget(FighterP a, const std::string& mv);
  void drawStatusTag(const Fighter& f, float x, float y);
  friend class Game;
};
