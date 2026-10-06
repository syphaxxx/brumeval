// Données du jeu : types, techniques, sorts, espèces, personnages, objets et règles.
// Tout est lu au démarrage depuis le dossier data/ (voir store.hpp) :
//   types.json, techniques.json, especes.json, objets.json, apparences.json, regles.json
#pragma once
#include <array>
#include <cstdint>
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
};
const std::vector<TypeDef>& types();
Type typeOf(const std::string& id);  // erreur si le type n'existe pas
const char* typeName(Type t);
float typeEff(Type attaque, Type defense);  // 2 = super efficace, 0.5 = peu efficace

enum class Target { Enemy, AllEnemies, Ally, AllAllies, AllyKO };
enum class Kind { Physical, Magic, Heal, Revive };

struct Move {
  std::string id, name;
  Type type;
  int power;
  Target target;
  Kind kind;
  int cost;  // coût en PM (0 = technique gratuite)
  std::string desc;
};

struct Learn { int lvl; std::string move; };

enum class Shape { Fox, Drop, Bud, Bird, Mouse, Bug, Frog, Mush, Rock, Wisp, Boss, Lizard, Golem, Human };

// Apparence d'un humain (héros ou habitant)
struct Look {
  std::string id, name;
  uint32_t hair, skin, top, bottom;
  int hat;     // 0 aucune, 1 capuche, 2 chapeau pointu, 3 bandeau
  int weapon;  // 0 aucune, 1 épée, 2 bâton, 3 hache, 4 dague
};

struct Species {
  std::string id, name;
  Type type;
  std::array<int, 6> base;  // PV, PM, Attaque, Défense, Magie, Vitesse
  Shape shape;
  uint32_t c1, c2;  // couleurs (créatures)
  std::vector<Learn> learn;
  std::string limit;  // identifiant de la technique Limite (vide : aucune)
  bool human = false;
  int look = 0;       // index dans allLooks() pour les humains
  std::string role;   // courte description
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
  // Récompenses
  int xpPerLevel = 9, xpBoss = 3, goldPerLevel = 4, goldBoss = 8;
  double xpFront = .7, xpReserve = .3;
  std::vector<std::pair<std::string, double>> drops;  // objet, chance
  // Capture
  double capBase = .15, capHp = .75, capMax = .95;
  // Formules des statistiques
  int hpDiv = 20, hpPerLvl = 1, hpBase = 10, mpDiv = 30, mpBase = 5, statDiv = 25, statBase = 5, xpBase = 10;
  double xpSquare = 1.2;
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
// Vérifie que toutes les références (techniques apprises, Limites…) existent.
std::vector<std::string> checkData();

// Noms utilisés dans les fichiers JSON
Shape shapeOf(const std::string& s);
const char* shapeName(Shape s);
int dirOf(const std::string& s);  // "haut", "bas", "gauche", "droite"
const char* dirName(int d);

// Un combattant (allié ou ennemi)
struct Fighter {
  std::string sp;
  int lvl = 1, xp = 0, hp = 0, mp = 0;
  int mhp = 1, mmp = 0, atk = 1, def = 1, mag = 1, spd = 1;
  float lim = 0;       // jauge Limite (0..100)
  float atb = 0;       // jauge ATB, uniquement en combat
  std::string tag;     // "A", "B"… quand plusieurs ennemis identiques
  bool boss = false;

  const Species& S() const { return species(sp); }
  std::string name() const;
  void recalc();
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
