// Joueur automatique : joue la partie principale tout seul, avec les mêmes touches qu'un
// joueur (marcher, Entrée devant un habitant, une porte, un coffre ou un boss, menus).
//   brumeval --partie DOSSIER  partie rapide sans fenêtre : rapport et captures dans DOSSIER
//   brumeval --demo            dans la fenêtre, à vitesse normale (aussi : Outils > Démo) ;
//                              une touche rend la main au joueur
// Il ne suit pas un chemin écrit d'avance : à chaque fois, il cherche la chose la plus proche
// à faire sur toutes les cartes (parler à un habitant dont la réponse a changé, ouvrir un
// coffre, entrer dans une nouvelle carte, affronter un boss quand l'équipe est prête…).
// Ainsi, il suit l'histoire même si les données changent, et signale où il reste bloqué.
#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>

#include "data.hpp"

class Game;

class Pilot {
 public:
  Pilot(Game& g, bool demo);
  ~Pilot();  // relâche les directions tenues (sinon le joueur continuerait de marcher)
  // Au début de Game::update : choisit les touches de cette image
  void update(float dt);
  bool demo() const { return demo_; }
  bool over() const { return finished || stuck; }
  bool finished = false;  // fin de l'histoire atteinte
  bool stuck = false;     // plus de progrès : voir journal
  std::string status;     // ce qu'il fait en ce moment (affiché pendant la démo)
  std::vector<std::string> journal;  // étapes marquantes, avec l'heure de jeu
  std::string shot;       // nom d'une capture à prendre maintenant (partie rapide), sinon vide
  float clock = 0;        // temps de jeu écoulé (secondes)
  int battles = 0, defeats = 0, captures = 0, purchases = 0;
  std::string summary() const;  // une ligne : temps, niveau, équipe, progression

 private:
  // Case où se tenir, et direction à regarder avant d'appuyer sur Entrée (-1 : y arriver suffit)
  struct Spot {
    int node, face;
  };
  enum Kind { Heal, Shop, Normal, Danger, Grind };
  struct Goal {
    std::string key, what;  // identifiant (« pnj:vallee:3 »), description pour le journal
    Kind kind = Normal;
    std::vector<Spot> spots;
    std::string sig;  // réponse attendue de l'événement (voir Info)
    int need = 0;     // combat pas encore prêt (Grind) : niveau moyen à atteindre
  };
  // Ce que ferait un événement maintenant : page choisie et résultat de ses conditions
  // (sig), combat (niveau le plus haut, boss), boutique, soins, fin de l'histoire
  struct Info {
    std::string sig;
    bool combat = false, boss = false, heal = false;
    int lvl = 0;
    std::vector<std::string> shop;
  };
  struct Step {
    int from, to, dir;
  };

  Game& G;
  bool demo_;
  float wait_ = 0;  // démo : pause avant la prochaine touche (laisser lire)
  bool paused_ = false;  // démo : la pause avant d'interagir est faite

  // Graphe de toutes les cartes : une case = un numéro (début de chaque carte dans offset_)
  std::vector<int> offset_;
  int node(int m, int x, int y) const { return offset_[m] + y * mapW(m) + x; }
  int mapW(int m) const;
  void unpack(int n, int& m, int& x, int& y) const;
  bool blockedAt(int m, int x, int y) const;
  int moveTo(int m, int x, int y, int d, int& cost) const;  // case d'arrivée en allant vers d (-1 : impossible)
  std::vector<int> dist_, prev_, prevDir_;
  void dijkstra(int from);

  // Objectifs
  std::map<std::string, std::string> doneSig_;  // dernière réponse vue de chaque habitant, porte, boss
  std::map<std::string, int> tries_, fails_;    // nombre de visites ; défaites contre ce combat
  std::set<int> visited_;                       // cartes déjà vues
  std::vector<Goal> goals();
  Info inspect(const std::string& event) const;
  void inspectList(const Json& list, Info& in, int depth) const;
  bool ready(const Info& in, const std::string& key) const;
  int need_ = 0;  // niveau visé pour le prochain combat difficile (entraînement)

  // Chemin en cours
  Goal goal_;
  bool hasGoal_ = false, needPlan_ = true;
  std::vector<Step> path_;
  int target_ = -1, targetFace_ = -1;
  int lastNode_ = -1;
  float sameT_ = 0;
  std::set<long long> banned_;  // pas qui n'ont pas marché (case * 4 + direction)
  std::set<int> grind_;         // entraînement : cases de hautes herbes de la zone choisie
  int paceDir_ = 0;
  void plan();
  void walk(float dt);
  void interactNow();
  void pace();  // entraînement : faire des allers-retours dans les hautes herbes

  // Combat, menus et dialogues
  std::string engaged_, engagedWhat_;  // objectif dont le combat est en cours
  int defeatsAtEngage_ = 0;
  bool engagedFought_ = false;  // son combat a commencé
  bool inBattle_ = false;
  std::string capture_, lantern_;  // capture en cours : créature visée, lanterne
  size_t captureTeam_ = 0;
  int menuMoves_ = 0;
  void battleStep();
  void menuStep();
  // Amène le curseur sur ce libellé (une touche par image), puis Entrée : 0 absent, 1 en route, 2 validé
  int pickItem(const std::string& label);
  std::vector<std::string> buyList_;
  bool shopPlanned_ = false;
  void shopping();
  void equipBest();
  float gearScore(const Fighter& f, const ItemDef& d) const;
  std::vector<std::string> wishes(const std::vector<std::string>& stock) const;  // achats utiles dans ce stock
  int creaturesWanted() const;  // créatures qui manquent pour accompagner les héros

  // Progrès (pour repérer un blocage)
  long progress_ = -1;
  float lastProgressT_ = 0;
  std::set<std::string> knownFlags_;
  size_t knownTeam_ = 0;
  std::vector<const Fighter*> knownMembers_;
  int knownDefeats_ = 0, shots_ = 0;
  int endings_ = 0;  // écrans de fin vus
  bool endingNow_ = false;
  void note(const std::string& s, const std::string& capture = "");
  void watch();
  std::string hms() const;
};