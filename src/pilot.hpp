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
#include <deque>
#include <map>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "data.hpp"

class Game;

class Pilot {
 public:
  Pilot(Game& g, bool demo);
  // Dossier de sa mémoire et de son historique (--partie <dossier>) ; vide : le dossier de l'utilisateur (démo du jeu).
  // Deux parties lancées en même temps dans deux dossiers ne s'écrasent plus leurs apprentissages (audit du 2026-10-24)
  static inline std::string memoryDir;
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
  int battles = 0, defeats = 0, captures = 0, purchases = 0, flights = 0;
  std::string summary() const;  // une ligne : temps, niveau, équipe, progression
  std::map<std::string, float> timeBy;  // temps de jeu par activité (combat, entraînement, trajet, menus, dialogue)
  std::string timeReport() const;
  std::string humanEstimate() const;  // durée pour un vrai joueur (très approximative)
  std::string lastRead_;              // dernier message lu (estimation de la lecture)
  long long readChars_ = 0;

 private:
  // Case où se tenir, et direction à regarder avant d'appuyer sur Entrée (-1 : y arriver suffit)
  struct Spot {
    int node, face;
  };
  enum Kind { Heal, Shop, Normal, Danger, Grind };
  bool bountyWork() const;  // primes à prendre ou à toucher
  void bountyStep();        // menu du tableau des primes
  struct Goal {
    std::string key, what;  // identifiant (« pnj:vallee:3 »), description pour le journal
    Kind kind = Normal;
    std::vector<Spot> spots;
    std::string sig;  // réponse attendue de l'événement (voir Info)
    int need = 0;     // combat pas encore prêt (Grind) : niveau moyen à atteindre
    Json fight;       // combat difficile (Danger) : l'action « combat », pour le répéter avec l'état actuel
  };
  // Ce que ferait un événement maintenant : page choisie et résultat de ses conditions
  // (sig), combat (niveau le plus haut, boss), boutique, soins, fin de l'histoire
  struct Info {
    std::string sig;
    bool combat = false, boss = false, heal = false;
    int lvl = 0;
    std::vector<std::string> shop;
    bool forge = false;  // ouvre la forge (Forgeroc)
    Json fight;          // l'action « combat » (pour la répéter avant d'y aller)
    bool effect = false; // la page fait quelque chose (objet, drapeau, quête, combat…), pas seulement parler
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
  std::vector<int> distFrom(int from) const;  // distances à pied depuis cette case (sans chemin)
  // Parcours (2.4) : les prochains objectifs ordinaires (les 10 plus proches) sont ordonnés en un
  // circuit (le plus proche d'abord, puis amélioré en inversant des morceaux : 2-opt) avec les vraies
  // distances à pied entre eux ; il suit ce circuit tant que la liste d'objectifs ne change pas
  std::vector<std::string> tour_;  // clés des objectifs, dans l'ordre du circuit
  std::string tourSet_;            // les objectifs dont il a été calculé

  // Objectifs
  std::map<std::string, std::string> doneSig_;  // dernière réponse vue de chaque habitant, porte, boss
  std::map<std::string, int> tries_, fails_;    // nombre de visites ; défaites contre ce combat
  // Mémoire des parties précédentes (pilote_memoire.txt, dossier de l'utilisateur) : pour chaque
  // combat difficile, les niveaux de marge en plus qu'il lui a fallu. Il s'en sert dès le début
  // de la partie suivante : il s'améliore au fil des parties.
  std::map<std::string, int> learned_;
  void loadMemory();
  void saveMemory() const;
  int margin(const std::string& key) const;  // niveaux en plus à avoir avant ce combat
  // Profondeur de chaque case (comme Game::wildDepth : distance aux bâtiments et passages) et
  // crainte de la nature profonde de chaque carte (« nature:<carte> » : meutes qui l'ont battu)
  std::vector<int> depth_;
  std::vector<int> zoneLo_;  // niveau le plus bas des créatures de la zone de chaque case d'herbe (0 : aucune)
  int fear(int m) const;
  int battleMap_ = -1;
  std::string battleFoes_;  // adversaires du combat en cours (pour le journal d'une défaite)
  bool battleWild_ = false;
  std::set<int> visited_;                       // cartes déjà vues
  std::vector<Goal> goals();
  Info inspect(const std::string& event) const;
  void inspectList(const Json& list, Info& in, int depth) const;
  bool ready(const Info& in, const std::string& key);
  float wantChance(const std::string& key) const;  // part de victoires demandée pour ce combat
  static float prudent(float chance, int n);       // part de victoires moins sa marge d'erreur
  std::map<std::string, float> nowChances_;        // combat + état (PV, PM) → part de victoires avec l'état actuel
  // Répétition (2.4) : avant un combat difficile, il le simule plusieurs fois avec une copie de son
  // équipe (niveaux, équipement, techniques) sans toucher à la partie, et n'y va que s'il gagne
  // assez souvent. Résultat gardé tant que l'équipe ne change pas.
  float rehearse(const Json& fight, int n, bool fullHealth);  // fullHealth : il se soignera avant d'y aller
  std::string teamSignature() const;
  std::map<std::string, std::pair<std::string, float>> chances_;  // combat → (signature de l'équipe, part de victoires)
  int rehearsals = 0;
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

  int menuMoves_ = 0;
  bool shouldFlee() const;  // combat sauvage qui tourne mal : mieux vaut fuir
  bool fleeNow_ = false;    // meute jugée trop forte dès le début du combat (répétition) : fuir
  int pathRisk(int at) const;  // niveau le plus haut des créatures des herbes sur le chemin jusqu'à cette case
  // Cases dans le champ de vision d'un dresseur (Npc::sight) qu'il n'est pas encore prêt à battre :
  // presque infranchissables (sinon le dresseur l'arrête et le bat)
  std::set<int> watched_;
  void watchSight();
  // Historique des parties (pilote_parties.txt : temps, défaites, niveau, compagnon) : pour mesurer ses progrès
  struct Run {
    int seconds, defeats, level;
    std::string starter;
  };
  std::vector<Run> history_;
  void loadHistory();
  void recordRun();
  void battleStep();
  void menuStep();
  // Amène le curseur sur ce libellé (une touche par image), puis Entrée : 0 absent, 1 en route, 2 validé
  int pickItem(const std::string& label);
  std::vector<std::string> buyList_;
  bool shopPlanned_ = false;
  void shopping();
  void equipBest(bool show = false);  // show (démo) : par les menus, comme un joueur
  // Démo (2026-10-23) : des « visites » de menus, comme un joueur : un chemin de libellés choisis l'un après l'autre
  // depuis le menu de pause (Échap), puis un temps pour regarder ; l'équipement et les gemmes passent par là en démo,
  // et il ouvre aussi les tactiques, la formation, le Journal et le Bestiaire aux bons moments
  struct Visit {
    std::vector<std::string> path;
    float look = 2.5f;
    bool gear = false;  // un changement d'équipement : à la fin, equipBest() finit ce que les menus n'ont pas fait
    std::function<void()> after;  // à la fin : ce que les menus n'auraient pas fait (compagnons, réserve)
  };
  std::deque<Visit> visits_;
  size_t visitStep_ = 0;
  bool visitOpen_ = false, visitClosing_ = false;
  float lastLook_ = -1e9f;  // dernière visite « pour voir » (pas plus d'une par minute de jeu)
  size_t seenTeam_ = 0;
  void visit(std::vector<std::string> path, float look, bool gear = false);
  void lookAt(std::vector<std::string> path, float look = 3);  // visite pour voir, si la dernière date d'une minute
  bool visitStep();  // vrai tant qu'une visite occupe le pilote
  std::vector<int> puzzleMoves_;  // énigme en cours : les directions à suivre (3.0)
  void socketGems(bool show = false);  // gemmes du sac dans les chatons libres (2.7)
  // Tactiques (2026-10-23) : un profil pour chacun selon ce qu'il sait faire (soigneur, protecteur, briseur, combattant)
  std::map<const Fighter*, std::string> profileOf_;
  void setupTactics(bool show);
  void adaptTeam(bool show);  // compagnons selon les créatures du lieu (2026-10-24)
  std::string adaptKey_;      // lieu et équipe de la dernière adaptation
  void forging();            // forge : renforce l'équipement porté tant que l'or et les matériaux le permettent
  bool forgeWanted() const;  // une arme ou armure portée peut être forgée maintenant
  float gearScore(const Fighter& f, const ItemDef& d) const;
  std::vector<std::string> wishes(const std::vector<std::string>& stock) const;  // achats utiles dans ce stock
  // Vaut-il le détour ? Un meilleur équipement, presque plus de soins, ou des lanternes qui manquent.
  // Sinon il refait seulement le plein quand il passe devant (sans quoi il ferait des allers-retours)
  bool worthTrip(const std::vector<std::string>& stock) const;
  int creaturesWanted() const;  // créatures qui manquent pour accompagner les héros

  // Progrès (pour repérer un blocage)
  long progress_ = -1;
  float lastProgressT_ = 0;
  std::set<std::string> knownFlags_;
  size_t knownTeam_ = 0;
  std::vector<const Fighter*> knownMembers_;
  std::map<const Fighter*, std::string> knownSpecies_;  // pour noter les évolutions
  int knownDefeats_ = 0, shots_ = 0;
  int endings_ = 0;  // écrans de fin vus
  bool endingNow_ = false;
  void note(const std::string& s, const std::string& capture = "");
  void watch();
  std::string hms() const;
};