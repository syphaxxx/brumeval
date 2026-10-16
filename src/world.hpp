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
// Villes (sprites) : q  pavés (praticable)   H  clôture   L  réverbère   Y  puits   C  caisses   U  buisson
//   Q  banc   K  charrette   M  étal de marché   X  feu de camp   G  tableau d'annonces   J  bannière
// Intérieurs : j  chaise
// Intérieurs (thème « interieur ») :
//   p  tapis (praticable)   t  table   h  lit   e  étagère   o  comptoir   v  tonneau
// Le dessin de certaines tuiles dépend du thème de la carte (arbres, chemins, parois).
#pragma once
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "store.hpp"

enum class Theme { Vallee, Cendres, Grotte, Foret, Neige, Interieur };
constexpr int N_THEMES = 6;
Theme themeOf(const std::string& s);
const char* themeName(Theme t);

struct Building {
  int x, y, w, h;
  std::string kind;   // apparence : "soin", "boutique1", "boutique2", "forge", "chapelle", "maison", "auberge"
  std::string name;
  unsigned roof;
  std::string event;     // événement lancé à la porte (par défaut : le genre)
  std::string interior;  // carte de l'intérieur : la porte y fait entrer (l'événement est alors celui d'un habitant)
  std::string look;      // dessin (sprite) à la place de celui du genre : « maison », « taverne », « boutique », « eglise », « entrepot »
  bool decor() const { return kind == "decor"; }  // décor sans porte (ruines, moulin, château : « dessin »)
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
  int roam = 0;                    // promeneur (3.3) : marche jusqu'à cette distance de sa place (0 : reste sur place)
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
  // Énigme (3.0, « enigme ») : pousser les rochers 'O' sur toutes les plaques 'P' pose le drapeau, qui ouvre
  // les grilles 'D'. puzzleX/Y : la case d'où l'on commence (recommencer, joueur automatique)
  std::string puzzleFlag, puzzleMsg;
  int puzzleX = -1, puzzleY = -1;
  int puzzleZone[4] = {0, 0, 0, 0};
  // Carte du monde (3.3, champ « carte_monde ») : position du lieu, de 0 à 100 (-1 : absent de la carte du monde)
  float worldX = -1, worldY = -1;  // salle de l'énigme (x, y, largeur, hauteur ; 0 : toute la carte) : la recherche y reste
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
// Case où l'on arrive en entrant dans l'intérieur « in » depuis la carte « outside » : juste
// au-dessus de son passage de sortie vers cette carte. Faux si l'intérieur n'a pas de sortie.
bool interiorEntry(const MapDef& in, const std::string& outside, int& x, int& y);

bool tileWalkable(char c);
// Énigme (3.0) : suite de directions (UP, DOWN, LEFT, RIGHT) qui pousse les rochers sur toutes les plaques, en
// partant de (sx, sy) ; blocked dit si une case est fermée (hors rochers et grilles). Vide si impossible (ou
// déjà résolue : solved = vrai)
std::vector<int> solvePuzzle(const MapDef& m, int sx, int sy, std::vector<std::pair<int, int>> boulders,
                             const std::function<bool(int, int)>& blocked, bool* solved = nullptr);
std::vector<std::pair<int, int>> puzzleTiles(const MapDef& m, char c);  // cases 'O' (rochers au départ) ou 'P' (plaques)
bool tileEncounter(char c);
