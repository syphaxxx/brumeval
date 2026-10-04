// Données du jeu : types, techniques, sorts, espèces, personnages et objets.
// Pour ajouter une créature ou un sort, il suffit d'ajouter une ligne dans data.cpp.
#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

enum class Type { Normal, Feu, Eau, Plante, Foudre, Ombre, Lumiere };
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
  uint32_t hair, skin, top, bottom;
  int hat;     // 0 aucun, 1 capuche, 2 chapeau pointu, 3 bandeau
  int weapon;  // 0 aucune, 1 épée, 2 bâton, 3 hache, 4 dague
};

struct Species {
  std::string id, name;
  Type type;
  std::array<int, 6> base;  // PV, PM, Attaque, Défense, Magie, Vitesse
  Shape shape;
  uint32_t c1, c2;  // couleurs (créatures)
  std::vector<Learn> learn;
  std::string limit;  // identifiant de la technique Limite
  bool human = false;
  int look = 0;       // index dans looks() pour les humains
  std::string role;   // courte description
};

struct ItemDef {
  std::string id, name, desc;
  int price;
  bool battle;  // utilisable en combat
  bool field;   // utilisable depuis le menu
  bool key;     // objet de quête
};

const Move& moveInfo(const std::string& id);
const Species& species(const std::string& id);
const ItemDef& item(const std::string& id);
const std::vector<ItemDef>& allItems();
const Look& look(int i);

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
