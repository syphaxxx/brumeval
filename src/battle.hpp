// Combat au tour par tour actif (jauges ATB), jusqu'à 3 alliés (4 contre un gardien à
// plusieurs) contre 1 à 3 ennemis.
// Quand la jauge ATB d'un allié est pleine, le temps s'arrête et on choisit son action,
// sauf si le mode auto est actif (touche Tab) et qu'une de ses tactiques s'applique.
// Les ennemis peuvent avoir des renforts qui entrent quand l'un d'eux tombe.
#pragma once
#include <functional>
#include <map>
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
  std::vector<FighterP> allies;   // vide : la première ligne de l'équipe (combat du gardien à plusieurs : tout le groupe)
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
  enum class Fx { Hit, Magic, Heal, Status, Capture };  // effet d'une action sur sa cible (animation)

  // Duel en ligne (online.hpp) : l'hôte calcule tout le combat ; l'invité affiche l'état
  // reçu et envoie ses ordres. Les « ennemis » de l'hôte sont l'équipe de l'invité.
  // Gardien à plusieurs (coop.cpp) : les alliés sont les combattants de tous les joueurs ;
  // le chef (Lead) calcule le combat, les autres (Follow) commandent leurs combattants.
  enum class Net { None, Host, Guest, Lead, Follow };
  void goOnline(Net mode, std::function<void(const Json&)> send, const std::string& me, const std::string& other);
  // owners : le joueur de chaque allié ; names : nom de chaque joueur ; send(à qui, message), -1 : tous les autres
  void goCoop(bool lead, int me, int leader, std::vector<int> owners, std::map<int, std::string> names,
              std::function<void(int, const Json&)> send);
  void netMessage(const Json& m);  // message reçu de l'autre joueur
  Json netState() const;           // hôte ou chef : état du combat, envoyé régulièrement aux autres
  void netEnd(bool won);           // fin décidée par l'hôte, ou départ de l'autre joueur
  void dropPlayer(int id);         // gardien : un joueur est parti (l'ordinateur joue ses combattants)
  Net online() const { return net_; }

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
  // Bruitage (audio.hpp), seulement pour le combat affiché (pas les simulations) ;
  // share : l'hôte ou le chef le fait aussi entendre aux autres joueurs
  void sound(const std::string& id, bool share = true);
  bool shown() const;  // c'est le combat affiché à l'écran
  // Compagnons : quand des héros combattent, les créatures alliées sont leurs compagnons et
  // agissent seules (tactiques, sinon l'ordinateur) ; le joueur ne commande que les héros
  bool hasHero_ = false;
  bool companion(const FighterP& f) const { return hasHero_ && isAlly(f) && !f->S().human; }
  void companionTurn(FighterP f);
  // Animation d'une action : élan du lanceur vers ses cibles, puis effet sur elles selon le
  // genre de l'action (Fx, plus haut) et la couleur de son type
  struct Anim {
    FighterP actor;
    std::vector<FighterP> targets;
    Fx fx = Fx::Hit;
    Color col;
    bool limit = false;
    float t0 = -10;
  };
  Anim anim_;
  void startAnim(const FighterP& a, const std::vector<FighterP>& targets, Fx fx, Color col, bool limit = false);
  Pt offset(const FighterP& f) const;  // décalage du dessin : élan, ou secousse quand il est touché
  void drawAnim();

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
  // Duel en ligne et gardien à plusieurs
  Net net_ = Net::None;
  std::function<void(int, const Json&)> send_;  // à qui (-1 : tous les autres), message
  std::string names_[2];          // duel : ce joueur, puis son adversaire
  FighterP remoteTurn_;           // hôte ou chef : combattant d'un autre joueur qui attend son ordre
  std::string netMsg_, netWait_;  // invité : message reçu de l'hôte, et qui choisit en ce moment
  float netMsgT_ = 0;
  int me_ = 0, leader_ = 0;                // gardien : ce joueur et le chef
  std::vector<int> owners_;                // gardien : le joueur de chaque allié (-1 : joué par l'ordinateur)
  std::map<int, std::string> players_;     // gardien : nom de chaque joueur
  std::vector<FighterP> roster_;           // tous les ennemis du combat (récompenses de ceux qui suivent le chef)
  bool duel() const { return net_ == Net::Host || net_ == Net::Guest; }
  bool coop() const { return net_ == Net::Lead || net_ == Net::Follow; }
  bool follower() const { return net_ == Net::Guest || net_ == Net::Follow; }  // affiche l'état reçu
  int owner(const FighterP& f) const;  // gardien : joueur de cet allié (me_ hors du gardien)
  bool mineToCommand(const FighterP& f) const { return isAlly(f) && (!coop() || owner(f) == me_); }
  std::string choosing() const;        // nom du joueur qui choisit en ce moment
  // Un combattant dans un message : [0, rang] pour l'équipe de celui qui l'envoie, [1, rang] pour l'autre
  // (au gardien, tous voient les mêmes alliés : [0, rang] désigne les alliés, [1, rang] les ennemis)
  Json mine(const FighterP& f) const;
  FighterP other(const Json& s) const;  // le même, vu par celui qui le reçoit
  void setBlink(const FighterP& f);
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
  friend class Pilot;
};
