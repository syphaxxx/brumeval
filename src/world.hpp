// Description des cartes : tuiles, bâtiments, habitants, coffres, panneaux,
// passages vers d'autres cartes, zones de rencontres et boss.
// Chaque carte est un fichier data/cartes/<id>.json.
//
// Légende des tuiles :
//   .  herbe            ,  hautes herbes (rencontres)   =  chemin
//   T  arbre            ~  eau                          B  pont de bois
//   R  rocher           F  fleurs                       W  fontaine
//   #  falaise / paroi  a  cendre                       g  herbes sèches (rencontres)
//   l  lave             m  montagne                     b  pont de pierre
//   k  entrée de grotte c  sol de grotte (rencontres)   x  cristal
//   d  arbre mort       i  glace                        n  neige profonde (rencontres)
//   z  marais (rencontres)
// Le dessin de certaines tuiles dépend du thème de la carte (arbres, chemins, parois).
#pragma once
#include <string>
#include <vector>

#include "store.hpp"

enum class Theme { Vallee, Cendres, Grotte, Foret, Neige };
Theme themeOf(const std::string& s);
const char* themeName(Theme t);

struct Building {
  int x, y, w, h;
  std::string kind;   // apparence : "soin", "boutique1", "boutique2", "forge", "chapelle", "maison", "auberge"
  std::string name;
  unsigned roof;
  std::string event;  // événement lancé à la porte (par défaut : le genre)
  int doorX() const { return x + w / 2; }
  int doorY() const { return y + h - 1; }
};

struct Npc {
  int x, y;
  int look;                        // apparence (data/apparences.json)
  int dir;                         // 0 haut, 1 bas, 2 gauche, 3 droite
  std::string event;               // vide = simple dialogue ; sinon événement (data/evenements.json)
  std::vector<std::string> lines;  // dialogue simple
  std::string hideIf;              // caché quand ce drapeau est posé
  int sight = 0;                   // dresseur : repère le joueur à cette distance devant lui
  std::string sightUntil;          // … tant que ce drapeau n'est pas posé
};

struct Chest {
  int x, y;
  std::string item;  // vide si le coffre contient de l'or
  int qty;
};

struct Sign {
  int x, y;
  std::string text;
};

struct Warp {
  int x, y;
  std::string map;  // carte d'arrivée
  int tx, ty, dir;
  Json condition;   // facultatif : conditions pour passer (comme « si » des événements)
  std::string message;  // affiché si le passage est fermé
};

// Zone déclencheuse : lance un événement quand le joueur y entre
struct Trigger {
  int x, y, w, h;
  std::string event;
  std::string until;  // ne se déclenche plus quand ce drapeau est posé
};

struct Zone {
  int x, y, w, h;
  int lo, hi, maxN;
  std::vector<std::string> pool;
};

struct BossSpot {
  int x, y;           // coin haut-gauche d'un bloc de 2x2 tuiles
  std::string id;     // espèce affichée sur la carte
  std::string flag;   // drapeau posé quand il est vaincu (le boss disparaît)
  std::string event;  // événement lancé quand on lui parle
};

struct MapDef {
  std::string id, name;
  Theme theme;
  float encounterRate = 0;  // 0 = valeur des règles
  std::string ambiance;     // "", "brume", "cendres", "obscurite"
  std::string ambianceUntil;  // l'ambiance disparaît quand ce drapeau est posé
  std::vector<std::string> rows;
  std::vector<Building> buildings;
  std::vector<Npc> npcs;
  std::vector<Chest> chests;
  std::vector<Sign> signs;
  std::vector<Warp> warps;
  std::vector<Zone> zones;
  std::vector<BossSpot> bosses;
  std::vector<Trigger> triggers;
  int w() const { return rows.empty() ? 0 : (int)rows[0].size(); }
  int h() const { return (int)rows.size(); }
};

std::vector<MapDef>& maps();
int mapIndex(const std::string& id);  // -1 si inconnue
void loadMaps();
Json mapToJson(const MapDef& m);
MapDef mapFromJson(const Json& j);
void saveMap(const MapDef& m);  // écrit data/cartes/<id>.json
// Vérifie les références (espèces, objets, cartes) et que tout est accessible à pied.
std::vector<std::string> checkMaps();

bool tileWalkable(char c);
bool tileEncounter(char c);
