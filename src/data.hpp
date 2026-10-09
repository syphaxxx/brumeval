// Données du jeu : types, techniques, sorts, espèces, personnages, objets et règles.
// Tout est lu au démarrage depuis le dossier data/ (voir store.hpp) :
//   types.json, techniques.json, especes.json, objets.json, apparences.json, regles.json
#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "store.hpp"

// Les types sont définis dans data/types.json ; on les désigne par leur numéro.
using Type = int;
struct TypeDef {
  std::string id, name;
  uint32_t color;
  std::vector<std::string> immune;  // états auxquels ce type est insensible
};
const std::vector<TypeDef>& types();
Type typeOf(const std::string& id);  // erreur si le type n'existe pas
const char* typeName(Type t);
float typeEff(Type attaque, Type defense);  // 2 = super efficace, 0.5 = peu efficace

enum class Target { Enemy, AllEnemies, Ally, AllAllies, AllyKO };
// Physique : Attaque contre Défense. Magique : Magie contre Résistance.
// Statut : pas de dégâts, seulement ses effets (bonus, malus, états).
enum class Kind { Physical, Magic, Heal, Revive, Status };

// États (altérations) : un seul à la fois par combattant
enum class Status { None, Poison, Burn, Paralysis, Sleep };
Status statusOf(const std::string& s);  // "poison", "brulure", "paralysie", "sommeil"
const char* statusId(Status s);
const char* statusName(Status s);  // « Poison »…
const char* statusTag(Status s);   // « PSN »…
uint32_t statusColor(Status s);

// Statistiques modifiables en combat par des bonus/malus (-3 à +3 niveaux)
enum Stage { S_ATK, S_DEF, S_MAG, S_RES, S_SPD, N_STAGES };
int stageOf(const std::string& s);  // "attaque", "defense", "magie", "resistance", "vitesse"
const char* stageName(int st);      // « Attaque »…

// Effet secondaire d'une technique
struct Effect {
  Status status = Status::None;  // état infligé
  int stat = -1, stages = 0;     // bonus (stages > 0) ou malus d'une statistique
  bool self = false;             // s'applique au lanceur plutôt qu'à la cible
  bool cure = false;             // guérit les états
  int chance = 100;              // en %
};

// Rythme d'une technique : où repart la jauge ATB du lanceur après l'avoir utilisée
enum class Pace { Normal, Quick, Heavy };

struct Move {
  std::string id, name;
  Type type;
  int power;
  Target target;
  Kind kind;
  int cost;  // coût en PM (0 = technique gratuite)
  std::string desc;
  int acc = 100;       // précision en %
  int critBonus = 0;   // chance de critique en plus (%)
  Pace pace = Pace::Normal;  // rapide : la jauge repart plus haut ; lourde : plus bas que zéro
  std::vector<Effect> effects;
  bool damaging() const { return kind == Kind::Physical || kind == Kind::Magic; }
};

struct Learn { int lvl; std::string move; };

enum class Shape { Fox, Drop, Bud, Bird, Mouse, Bug, Frog, Mush, Rock, Wisp, Boss, Lizard, Golem, Snake, Bat, Wolf, Turtle, Ghost, Crystal, Magma, Human };

// Apparence d'un humain (héros ou habitant)
struct Look {
  std::string id, name;
  uint32_t hair, skin, top, bottom;
  int hat;     // 0 aucune, 1 capuche, 2 chapeau pointu, 3 bandeau, 4 casque, 5 foulard
  int weapon;  // 0 aucune, 1 épée, 2 bâton, 3 hache, 4 dague, 5 arc, 6 lance
};

// Tactique (comme les « gambits » de Final Fantasy XII) : « si condition, alors action ».
// Les conditions et actions automatiques sont décrites dans tactics.hpp.
struct Tactic {
  bool on = true;
  std::string cond;  // identifiant de la condition (« allie_pv_moins »…)
  int value = 0;     // seuil de la condition, si elle en a un (PV < valeur %)
  enum class Act { Auto, Move, Item } kind = Act::Auto;
  std::string act;   // action automatique (« soin »…), technique ou objet
};

// Statistiques de base d'une espèce (multipliées par le niveau)
enum Base { B_HP, B_MP, B_ATK, B_DEF, B_MAG, B_RES, B_SPD, N_BASE };

struct Species {
  std::string id, name;
  Type type;                 // type principal
  Type type2 = -1;           // second type éventuel (-1 : aucun)
  std::array<int, N_BASE> base;  // PV, PM, Attaque, Défense, Magie, Résistance, Vitesse
  int acc = 100, eva = 0, crit = 5;  // précision, esquive, critique (en %, ne dépendent pas du niveau)
  std::map<std::string, float> resist;  // multiplicateurs propres : "physique", "magique" ou un type
  std::vector<std::string> immune;      // états auxquels l'espèce est insensible
  Shape shape;
  uint32_t c1, c2;  // couleurs (créatures)
  std::vector<Learn> learn;
  std::string limit;  // identifiant de la technique Limite (vide : aucune)
  bool human = false;
  int look = 0;       // index dans allLooks() pour les humains
  std::string role;   // courte description
  std::vector<Tactic> tactics;  // tactiques de départ propres (vide : celles des règles)
};

struct ItemDef {
  std::string id, name, desc;
  int price;
  bool battle;  // utilisable en combat
  bool field;   // utilisable depuis le menu
  bool key;     // objet de quête
  int healHp = 0, healMp = 0;  // soins
  int revive = 0;              // relève un K.O. avec ce pourcentage de PV
  float capture = 0;           // > 0 : lanterne de capture (multiplicateur)
  bool cure = false;           // guérit les états
};

// Règles du jeu (data/regles.json). Les valeurs numériques sont décrites dans
// ruleFields() pour pouvoir être lues, écrites et réglées au même endroit.
struct Rules {
  // Départ
  std::string startMap, hero, respawnMap, startEvent;
  int startX = 0, startY = 0, startDir = 1, startLevel = 5, startGold = 100, respawnX = 0, respawnY = 0;
  std::vector<std::string> starters;
  std::vector<std::pair<std::string, int>> startItems;
  // Équipe
  int maxTeam = 8, frontSize = 3;
  // Rencontres
  double encounterRate = .11, second = .55, third = .35;
  int minSteps = 3;
  // Combat
  double atbBase = 20, atbSpeed = 1.1, flee = .7, limitGain = 110, stab = 1.25, spreadMin = .85, dmgDivisor = 40;
  double critMult = 1.5, stageStep = .25;
  int accBase = 100, evaBase = 2, critBase = 5;
  int quickGauge = 25, heavyDelay = 25;  // jauge après une technique rapide (%), retard après une lourde (%)
  // États
  double poisonDmg = .1, burnDmg = .0625, burnAtk = .75, paraSpeed = .5, paraSkip = .25;
  int sleepMin = 1, sleepMax = 3;
  // Récompenses
  int xpPerLevel = 9, xpBoss = 3, goldPerLevel = 4, goldBoss = 8;
  double xpFront = .7, xpReserve = .3;
  std::vector<std::pair<std::string, double>> drops;  // objet, chance
  // Capture
  double capBase = .15, capHp = .75, capMax = .95;
  // Formules des statistiques
  int hpDiv = 20, hpPerLvl = 1, hpBase = 10, mpDiv = 30, mpBase = 5, statDiv = 25, statBase = 5, xpBase = 10;
  double xpSquare = 1.2;
  // Tactiques : lignes disponibles (départ, une de plus tous les n niveaux, maximum) et règles de départ
  int tacticStart = 4, tacticPerLvl = 6, tacticMax = 10;
  std::vector<Tactic> tactics;
};
struct RuleField {
  const char* group;  // section de regles.json
  const char* key;
  const char* label;  // nom affiché dans les réglages
  double* d = nullptr;
  int* i = nullptr;
  double min, max, step;
};
const Rules& rules();
Rules& editRules();
std::vector<RuleField> ruleFields(Rules& r);

const Move& moveInfo(const std::string& id);
const char* paceId(Pace p);         // "normale", "rapide", "lourde"
const char* paceName(Pace p);       // « Normale »…
float paceGauge(const Move& m);     // jauge ATB du lanceur après la technique (négative si lourde)
float paceTime(const Move& m);      // temps avant le tour suivant (1 = normal, moins si rapide)
const Species& species(const std::string& id);
const ItemDef& item(const std::string& id);
bool hasMove(const std::string& id);
bool hasSpecies(const std::string& id);
bool hasItem(const std::string& id);
const std::vector<ItemDef>& allItems();
const std::vector<Move>& allMoves();
const std::vector<Species>& allSpecies();
const std::vector<Look>& allLooks();
const Look& look(int i);
int lookIndex(const std::string& id);  // -1 si inconnue

// Lecture de tout data/ (sauf cartes et événements, voir world.hpp et events.hpp).
// Lève une erreur lisible en cas de problème.
void loadData();
// Les fichiers sont gardés en mémoire sous forme de documents JSON : les réglages
// les modifient, puis rebuildData() reconstruit les données du jeu.
enum DataFile { DF_TYPES, DF_MOVES, DF_SPECIES, DF_ITEMS, DF_LOOKS, DF_RULES, N_DATAFILES };
const char* dataFileName(DataFile f);
Json& dataDoc(DataFile f);
void rebuildData();
void saveDataDoc(DataFile f);
// Vérifie que toutes les références (techniques apprises, Limites…) existent.
std::vector<std::string> checkData();

// Noms utilisés dans les fichiers JSON
Shape shapeOf(const std::string& s);
const char* shapeName(Shape s);
int dirOf(const std::string& s);  // "haut", "bas", "gauche", "droite"
const char* dirName(int d);

// Efficacité d'une technique sur une espèce : types (et second type) et résistances propres
float moveEff(const Move& m, const Species& s);
std::string typesName(const Species& s);  // « Eau » ou « Eau/Glace »
bool hasType(const Species& s, Type t);
bool immuneTo(const Species& s, Status st);

// Un combattant (allié ou ennemi)
struct Fighter {
  std::string sp;
  int lvl = 1, xp = 0, hp = 0, mp = 0;
  int mhp = 1, mmp = 0, atk = 1, def = 1, mag = 1, res = 1, spd = 1;
  int acc = 100, eva = 0, crit = 5;
  float lim = 0;       // jauge Limite (0..100)
  float atb = 0;       // jauge ATB, uniquement en combat
  std::string tag;     // "A", "B"… quand plusieurs ennemis identiques
  bool boss = false;
  // État et bonus/malus : seulement pendant un combat
  Status status = Status::None;
  int statusTurns = 0;
  std::array<int, N_STAGES> stage{};
  // Tactiques (alliés) : jouées en combat quand le mode auto est actif
  std::vector<Tactic> tactics;
  bool tacticsOn = true;

  const Species& S() const { return species(sp); }
  std::string name() const;
  void recalc();
  void clearBattle();             // efface états et bonus/malus
  float stageMult(int st) const;  // 1.25 pour +1, 0.8 pour -1…
  float eAtk() const;             // statistiques avec bonus/malus (et brûlure)
  float eDef() const;
  float eMag() const;
  float eRes() const;
  float eSpd() const;
  int need() const;  // expérience nécessaire pour le niveau suivant
  std::vector<std::string> techs() const;   // techniques gratuites (4 dernières)
  std::vector<std::string> spells() const;  // sorts (coûtent des PM)
  bool alive() const { return hp > 0; }
};
using FighterP = std::shared_ptr<Fighter>;
// Hasard
float frand();              // nombre entre 0 et 1
int irand(int lo, int hi);  // entier entre lo et hi inclus

FighterP makeFighter(const std::string& id, int lvl);
// Gagne de l'expérience ; renvoie les messages de montée de niveau.
std::vector<std::string> gainXp(Fighter& f, int xp);
