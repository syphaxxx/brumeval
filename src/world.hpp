// Description des cartes : tuiles, bâtiments, habitants, coffres, panneaux,
// passages vers d'autres cartes, zones de rencontres et boss.
// Les cartes elles-mêmes sont dans world_data.cpp.
//
// Légende des tuiles :
//   .  herbe            ,  hautes herbes (rencontres)   =  chemin
//   T  arbre            ~  eau                          B  pont de bois
//   R  rocher           F  fleurs                       W  fontaine
//   #  falaise / paroi  a  cendre                       g  herbes sèches (rencontres)
//   l  lave             m  montagne                     b  pont de pierre
//   k  entrée de grotte c  sol de grotte (rencontres)   x  cristal
//   d  arbre mort
#pragma once
#include <string>
#include <vector>

enum class Theme { Vallee, Cendres, Grotte };

struct Building {
  int x, y, w, h;
  std::string kind;  // "soin", "boutique1", "boutique2", "forge", "chapelle", "maison", "auberge"
  std::string name;
  unsigned roof;
  int doorX() const { return x + w / 2; }
  int doorY() const { return y + h - 1; }
};

struct Npc {
  int x, y;
  int look;            // apparence (voir data.cpp)
  int dir;             // 0 haut, 1 bas, 2 gauche, 3 droite
  std::string script;  // vide = simple dialogue ; sinon comportement spécial (game.cpp)
  std::vector<std::string> lines;
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
  int map, tx, ty, dir;
};

struct Zone {
  int x, y, w, h;
  int lo, hi, maxN;
  std::vector<std::string> pool;
};

struct BossSpot {
  int x, y;           // coin haut-gauche d'un bloc de 2x2 tuiles
  std::string id;     // "sylvarque", "golem", "ignarok"
  std::string flag;   // drapeau posé quand il est vaincu
};

struct MapDef {
  std::string name;
  Theme theme;
  std::vector<std::string> rows;
  std::vector<Building> buildings;
  std::vector<Npc> npcs;
  std::vector<Chest> chests;
  std::vector<Sign> signs;
  std::vector<Warp> warps;
  std::vector<Zone> zones;
  std::vector<BossSpot> bosses;
  int w() const { return rows.empty() ? 0 : (int)rows[0].size(); }
  int h() const { return (int)rows.size(); }
};

const std::vector<MapDef>& maps();
bool tileWalkable(char c);
bool tileEncounter(char c);
