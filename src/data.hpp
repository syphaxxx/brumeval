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
  std::string nameFr;  // nom français (procgen : le monde généré est le même dans toutes les langues)
  uint32_t color;
  std::vector<std::string> immune;  // états auxquels ce type est insensible
};
const std::vector<TypeDef>& types();
Type typeOf(const std::string& id);  // erreur si le type n'existe pas
const char* typeName(Type t);
std::string typeDot(Type t);  // icône de menu : une pastille de la couleur du type (« #rrggbb », menuGlyph de ui.cpp)
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

// Damier de combat (2.0 : 4x4, demandé le 2026-10-12) : 16 cases par camp, numérotées 0 à 15.
// Colonne = case % GRID (0 avant … BACK_COL arrière : l'avant est du côté de l'adversaire) ;
// ligne = case / GRID (0 haut … GRID - 1 bas).
constexpr int GRID = 4;
constexpr int N_CELLS = GRID * GRID;
constexpr int BACK_COL = GRID - 1;
inline int cellCol(int c) { return c % GRID; }
inline int cellRow(int c) { return c / GRID; }
inline int cellAt(int row, int col) { return row * GRID + col; }
std::string cellName(int c);  // « Avant · haut »
// Échiquier (3.4, demandé le 2026-10-18) : en combat, les deux camps partagent un seul plateau de BOARD_W × GRID
// cases ; chacun peut passer de l'autre côté. Colonne x = case % BOARD_W : 0 à GRID - 1 pour le camp de gauche (les
// alliés ; 0 = leur bord, GRID - 1 = leur avant), GRID à BOARD_W - 1 pour celui de droite (GRID = son avant).
// Fighter::cell (la formation, sauvegardée) reste une case de son camp (0 à N_CELLS - 1) : toBoard la place.
constexpr int BOARD_W = 2 * GRID;
constexpr int BOARD_CELLS = BOARD_W * GRID;
inline int boardX(int b) { return b % BOARD_W; }
inline int boardY(int b) { return b / BOARD_W; }
inline int boardAt(int y, int x) { return y * BOARD_W + x; }
inline int toBoard(bool leftSide, int c) { return boardAt(cellRow(c), leftSide ? BACK_COL - cellCol(c) : GRID + cellCol(c)); }
inline bool inHalf(bool leftSide, int b) { return leftSide ? boardX(b) < GRID : boardX(b) >= GRID; }
// Distance « du roi » (diagonales comprises) et « à pied » (sans diagonale) entre deux cases du plateau
int boardDist(int a, int b);
int boardSteps(int a, int b);
std::string boardName(int b);  // « Notre camp, avant · haut », « Camp adverse, arrière · bas »
// Zone d'effet d'une technique autour de sa cible : la cible seule, sa ligne (en profondeur),
// sa colonne (tout un rang), elle et ses voisines (croix), ou tout le camp
enum class Area { Single, Line, Column, Cross, All };
const char* zoneId(Area z);    // "case", "ligne", "colonne", "croix", "tout"
const char* zoneName(Area z);  // « Une cible »…
bool inZone(Area z, int center, int cell);  // la case est-elle touchée quand la zone vise center ?
// La même chose sur le plateau de l'échiquier : une ligne traverse les deux camps (BOARD_W cases)
bool inBoardZone(Area z, int center, int cell);

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
  Area zone = Area::Single;  // zone d'effet (par défaut : selon la cible, « tous » = tout le camp)
  bool melee = false;        // corps à corps : ne vise que la colonne ennemie la plus avancée (défaut : physique)
  int shove = 0;             // 2.6 (Into the Breach) : +1 pousse la cible d'une case vers l'arrière, -1 la tire vers l'avant
  // 3.4 (échiquier, champ « mouvement ») : "bond" = après le coup, le lanceur saute sur la case derrière la cible ;
  // "echange" = le lanceur et la cible échangent leurs cases (pas un boss)
  std::string motion;
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
  std::vector<std::string> aliases;  // autres noms de celui qui parle (« aussi ») : son portrait dans les dialogues
  std::string sprite;  // préfixe des sprites de l'atlas (<sprite>_bas_0…, <sprite>_combat) ; vide : dessiné en code
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
  std::string desc;          // « description » : quelques mots pour le Bestiaire (montrés une fois l'espèce obtenue)
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
  // Évolution (2.2) : au niveau evolveLvl, la créature peut devenir l'espèce evolveTo (vide : aucune)
  std::string evolveTo;
  int evolveLvl = 0;
  float size = 1;  // taille du dessin (formes évoluées : un peu plus grandes)
  std::string sprite;  // sprite de l'atlas (créatures ; vide : dessinée en code)
  int shield = 0;  // Brisure (2.5) : points de bouclier de l'espèce en ennemi (0 : valeur des règles)
  std::string combatRole;  // Rôle en combat (2.7, « role_combat ») : protecteur, soigneur ou chef (vide : aucun)
  std::string talent;  // Talent passif (2.5, inspiré de Pokémon) : identifiant dans les règles (« talents »)
};

// Équipement : une arme, une armure et un accessoire par combattant. Armes et armures sont
// pour les héros (humains) ; un accessoire va à tout le monde.
enum GearSlot { G_WEAPON, G_ARMOR, G_ACCESSORY, N_GEAR };
// Bonus d'un équipement : PV, PM, Attaque, Défense, Magie, Résistance, Vitesse, Précision, Esquive, Critique
enum GearStat { GS_HP, GS_MP, GS_ATK, GS_DEF, GS_MAG, GS_RES, GS_SPD, GS_ACC, GS_EVA, GS_CRIT, N_GEAR_STATS };
const char* gearSlotName(int s);  // « Arme »…
const char* gearStatName(int s);  // « Attaque »…

struct ItemDef {
  std::string id, name, desc;
  std::string icon;  // icône dans les menus (sprite de l'atlas, champ « icone » ; vide : aucune)
  int price;
  bool battle;  // utilisable en combat
  bool field;   // utilisable depuis le menu
  bool key;     // objet de quête
  int healHp = 0, healMp = 0;  // soins
  int revive = 0;              // relève un K.O. avec ce pourcentage de PV
  float capture = 0;           // > 0 : lanterne de capture (multiplicateur)
  bool cure = false;           // guérit les états
  int slot = -1;               // équipement : emplacement (GearSlot) ; -1 : pas un équipement
  std::array<int, N_GEAR_STATS> bonus{};
  bool trophy = false;         // trophée d'un boss (champ "trophee") : sert à la forge
  int moveBonus = 0;           // échiquier (3.4, champ « mouvement ») : cases de déplacement en plus quand on le porte
  std::string owner;           // champ « pour » (2026-10-20, armes des quêtes des héros) : seul ce héros peut le porter
  // Forge (2.1) : chaque arme et armure existe aussi forgée, « epee_fer+2 », objet généré au
  // chargement (pas dans objets.json) : forged = son niveau, base = l'objet d'origine
  int forged = 0;
  std::string base;
  // Gemme (2.7, comme les matérias de Final Fantasy VII) : se sertit dans un chaton d'un équipement
  // porté. gemMove : technique qu'elle donne ; gemBonus : statistiques en pourcentage (0.1 = +10 %) ;
  // gemSupport / gemValue : effet de soutien (vol_vie, contre, eveil, briseur)
  bool gem = false;
  std::string gemMove, gemSupport;
  double gemValue = 0;
  std::array<double, N_GEAR_STATS> gemBonus{};
  std::string bonusText() const;  // « +4 Attaque, +10 PV »
};
std::string forgedId(const std::string& base, int level);  // "epee_fer" et 2 : "epee_fer+2"
bool forgeable(const ItemDef& d);                           // arme ou armure qu'on peut encore forger

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
  // Meutes : loin des villes et des entrées (profondeur, Game::wildDepth), rencontres plus
  // fréquentes (jusqu'à +packDense) et créatures en plus, jusqu'à packMax ; pleine profondeur
  // à packDistance cases
  int packMax = 6;
  double packDistance = 30, packDense = .5;
  double chroma = 0;  // chance qu'une créature sauvage soit chromatique (couleurs rares, sprite « _chroma »)
  double alpha = 0;   // chance qu'une meute (2 créatures ou plus) soit menée par un alpha (comme un boss)
  int alphaLevels = 3;     // niveaux en plus de l'alpha
  double nightShare = .5;  // part des rencontres de nuit tirées de la liste « nuit » d'une zone
  // Combat
  double atbBase = 20, atbSpeed = 1.1, flee = .7, limitGain = 110, stab = 1.25, spreadMin = .85, dmgDivisor = 40;
  double critMult = 1.5, stageStep = .25;
  int accBase = 100, evaBase = 2, critBase = 5;
  int quickGauge = 25, heavyDelay = 25;  // jauge après une technique rapide (%), retard après une lourde (%)
  double zoneSide = .75;                 // zone d'effet : dégâts sur les cases autour de la cible
  double bossWarn = .35;                 // chance qu'un boss annonce sa plus grosse attaque (un tour à l'avance)
  double bossBurst = 1.6;                // puissance de l'attaque annoncée (sur ceux restés dans la zone)
  double companionAtb = .5;              // compagnons : vitesse de leur jauge ATB (ils agissent moins souvent que les héros)
  // Brisure (2.5, inspirée d'Octopath Traveler) : bouclier des ennemis (créature, boss), dégâts sur un ennemi brisé
  int breakShield = 2, breakShieldBoss = 8;
  double breakDamage = 1.5;
  // Pousser / tirer (2.6) : choc quand la case d'arrivée est prise (part des dégâts du coup, aux deux)
  double shoveClash = .5;
  // Rôles des ennemis (2.7) : protection des voisins du protecteur (part des dégâts retirée),
  // vitesse ATB en plus des alliés d'un chef
  double roleGuard = .3, roleChief = .2;
  // Gemmes (2.7) : chatons d'une arme, d'une armure, d'un accessoire ; niveau de forge qui en ajoute un
  int socketWeapon = 1, socketArmor = 1, socketAccessory = 1, socketForge = 2;
  // Terrain (2.6) : nombre de cases spéciales sur chaque damier
  int terrainMin = 0, terrainMax = 2;
  // Échiquier (3.4) : cases parcourues par tour (« Se déplacer » ne finit plus le tour), portée du corps à corps
  // d'un boss, coup dans le dos (depuis le côté du camp de la cible), dégâts reçus en plus par adversaire au contact
  // au-delà du premier (encerclé), attaque à distance au contact d'un ennemi, bonus de la Percée (atteindre le bord
  // adverse, une fois par combat) ; types qui passent par-dessus les combattants (vol) ou vont plus loin (rapides)
  int moveRange = 2, bossReach = 2, breakthrough = 1;
  double backStab = 1.3, surround = .1, contact = .75;
  std::vector<std::string> flyTypes, fastTypes;
  // Obstacles (3.4) : entre obstacleMin et obstacleMax cases du milieu du plateau sont prises par un décor
  // (sprite tiré dans obstacleSprites[décor]) : personne ne s'y arrête ni ne les traverse, sauf en volant
  int obstacleMin = 0, obstacleMax = 2;
  std::map<std::string, std::vector<std::string>> obstacleSprites;
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
  // Multijoueur : gardien à plusieurs, PV en plus par joueur au-delà du premier (0.2 : +20 %)
  double coopGuardian = .15;
  // Tactiques : lignes disponibles (départ, une de plus tous les n niveaux, maximum) et règles de départ
  int tacticStart = 4, tacticPerLvl = 6, tacticMax = 10;
  std::vector<Tactic> tactics;
  // Butin selon le type d'une créature sauvage vaincue (« butin_type ») : matériaux de la forge
  struct TypeDrop {
    std::string type, item;
    double chance;
  };
  std::vector<TypeDrop> typeDrops;
  // Talents (2.5) : effet passif de chaque espèce ; « effet » choisit ce que fait le code, « valeur » le règle
  struct Talent {
    std::string id, name, desc, kind;
    double value = 0;
  };
  std::vector<Talent> talents;
  // Météo du combat (2.6) : tirée selon le décor ; multiplie les dégâts de certains types
  struct Weather {
    std::string id, name, desc, fx;  // fx : effet visuel (pluie, chaleur, neige, orage, brume, soleil)
    std::vector<std::string> themes;
    double chance = 0;
    std::map<std::string, double> typeMult;  // identifiant du type -> multiplicateur des dégâts
    int acc = 0;                     // précision des coups physiques en plus (négatif : en moins)
  };
  std::vector<Weather> weathers;
  // Cases de terrain (2.6) : posées au hasard sur les damiers selon le décor
  struct Terrain {
    std::string id, name, desc, kind;  // kind : degats, poison, soin, lent, esquive, magie
    double value = 0;
    std::vector<std::string> immune;  // types insensibles (identifiants)
    std::vector<std::string> themes;
    uint32_t color = 0xffffff;
  };
  std::vector<Terrain> terrains;
  // Techniques en duo (2.6, inspirées de Chrono Trigger) : deux alliés qui vont ensemble. Un membre
  // s'écrit avec l'identifiant d'une espèce, « humain » (un héros) ou « type:feu » (une créature de ce type).
  // Le partenaire doit avoir sa jauge ATB à duoGauge % au moins ; il la dépense.
  struct Duo {
    std::vector<std::string> members;
    std::string move;
  };
  std::vector<Duo> duos;
  // Astuces (2.8) : expliquées la première fois qu'on rencontre la règle ; aide = page d'Aide (F1)
  struct Tip {
    std::string id, help, text;
  };
  std::vector<Tip> tips;
  // Conseils avant les boss (2.8), dans l'ordre de l'histoire : le premier dont le drapeau manque est le prochain
  struct Advice {
    std::string flag, boss, place, gem, text;
  };
  std::vector<Advice> advice;
  int duoGauge = 50;
  // Forge de Forgeroc (« forge ») : un niveau par entrée (+1, +2, +3). bonus : part du bonus de
  // l'objet ajoutée (0.3 : +30 %), or et matériaux (« trophee » : n'importe quel trophée de boss)
  struct ForgeLevel {
    double bonus = 0;
    int gold = 0;
    std::vector<std::pair<std::string, int>> items;
  };
  std::vector<ForgeLevel> forge;
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
// Talent de l'espèce (nullptr : aucun) ; talentValue : sa valeur si son effet est « kind », sinon 0
const Rules::Talent* talentOf(const Species& s);
double talentValue(const Species& s, const char* kind);
// Duo : l'espèce correspond-elle à ce membre (« lior », « humain », « type:feu ») ?
bool duoMember(const std::string& spec, const Species& s);

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
  std::array<std::string, N_GEAR> gear;  // équipement porté (objets de data/objets.json, vide : rien)
  std::array<std::vector<std::string>, N_GEAR> gems;  // gemmes serties dans chaque équipement (2.7)
  int cell = -1;                         // formation : case choisie sur le damier (-1 : automatique)
  bool evolveLater = false;              // évolution refusée : proposée de nouveau seulement par Échap > Évolution
  bool chroma = false;                   // créature chromatique (2026-10-20) : couleurs rares (sprite « <sprite>_chroma »)
  // Brisure (2.5), seulement pendant un combat : bouclier d'un ennemi ; un coup sur une faiblesse
  // retire un point ; à zéro, il est brisé jusqu'à son prochain tour (perdu) et subit plus de dégâts
  int shield = 0, shieldMax = 0;
  bool broken = false;
  // Phases d'un boss (2.7) : la liste « phases » de l'ennemi dans son combat (evenements.json) ; phase :
  // nombre de phases déjà passées. bonusMoves : techniques en plus (phase d'un boss)
  Json phases;
  int phase = 0;
  std::vector<std::string> bonusMoves;
  // Héros : la créature compagnon choisie par le joueur (vide : la première créature libre de l'équipe)
  std::weak_ptr<Fighter> companion;

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
void seedRandom(unsigned s);  // mode test : rejouer la même suite de tirages
int irand(int lo, int hi);  // entier entre lo et hi inclus

FighterP makeFighter(const std::string& id, int lvl);
// Un ennemi décrit dans un combat (« espece », « niveau », « pv » multiplicateur, « boss », « bouclier », « phases »)
FighterP makeFoe(const Json& e, int defaultLevel);
// Nouvelle partie + (2026-10-20) : niveaux ajoutés à tous les adversaires (combats des événements, renforts, créatures
// sauvages). Posé par Game (Game::applyNewGamePlus) d'après les drapeaux « ngp<k> » ; 0 partout ailleurs (Arène, tests).
extern int foeLevelBonus;
// Échiquier (3.4) : cases parcourues par tour (règle, types rapides, bottes) ; l'espèce vole par-dessus les combattants
int fighterMoveRange(const Fighter& f);
bool speciesFlies(const Species& s);
// Qui combat : les « heroes » premiers héros valides de l'équipe, chacun suivi de son compagnon
// (sa créature choisie, sinon la première créature libre, dans l'ordre de l'équipe). Une équipe
// sans héros (mode test, Arène) combat comme avant : ses premiers membres valides.
std::vector<FighterP> frontOf(const std::vector<FighterP>& team, int heroes);
// Cases du damier pour ces combattants, dans le même ordre. keep : garde les cases choisies
// (Fighter::cell) quand elles sont libres ; les autres sont placés automatiquement (les
// combattants physiques devant, les lanceurs de sorts derrière, en commençant par le centre).
std::vector<int> formation(const std::vector<FighterP>& v, bool keep);
// Le compagnon d'un héros dans cette équipe (celui qu'il aurait en combat ; nullptr : aucun)
FighterP companionOf(const std::vector<FighterP>& team, const FighterP& hero);
// Multiplicateur des PV du gardien et de ses acolytes à plusieurs (coop.cpp) : ×alliés/3 au-delà de
// trois alliés, puis +coopGuardian par joueur en plus
float guardianScale(int allies, int players);
// Équipement dans un message réseau : ["epee_fer", "", "amulette_vie"] ; lu en vérifiant chaque objet
Json gearJson(const Fighter& f);
void applyGear(Fighter& f, const Json& gear);
bool canEquip(const Fighter& f, const ItemDef& d);  // emplacement permis pour ce combattant
// Gemmes (2.7) : chatons d'un équipement (règles « gemmes », + 1 forgé à partir de +2) ; valeur d'un
// effet de soutien serti (somme) ; retire les gemmes en trop (rendues au sac par l'appelant)
int gemSockets(const ItemDef& d);
double gemSupport(const Fighter& f, const char* kind);
std::vector<std::string> trimGems(Fighter& f);
// Évolution : la créature a atteint le niveau de son évolution ; evolve la fait évoluer (même niveau,
// équipement, tactiques et case ; PV et PM gardent la même proportion)
bool canEvolve(const Fighter& f);
void evolve(Fighter& f);
// Gagne de l'expérience ; renvoie les messages de montée de niveau.
std::vector<std::string> gainXp(Fighter& f, int xp);
