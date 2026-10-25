// Combat au tour par tour actif (jauges ATB), jusqu'à 3 alliés (4 contre un gardien à
// plusieurs) contre 1 à 3 ennemis.
// Quand la jauge ATB d'un allié est pleine, le temps s'arrête et on choisit son action,
// sauf si le mode auto est actif (touche Tab) et qu'une de ses tactiques s'applique.
// Les ennemis peuvent avoir des renforts qui entrent quand l'un d'eux tombe.
#pragma once
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "data.hpp"
#include "ui.hpp"
#include "world.hpp"

class Game;
enum class BattleResult;

// Chance de capturer f avec une lanterne de multiplicateur mult (règles : capture)
float captureChance(const Fighter& f, float mult);
// Tout ce que fait une technique : genre, type, puissance, coût, cible et zone, portée, effets,
// rythme (user : le lanceur, pour chiffrer un soin). moveHelp : sa description, puis tout cela.
std::string moveDetails(const Move& m, const Fighter* user = nullptr);
std::string moveHelp(const Move& m, const Fighter* user = nullptr);
// En combat (3.2) : la même chose en bref, sur deux lignes au plus (la zone est montrée sur le damier)
std::string moveBrief(const Move& m, const Fighter* user = nullptr);
// Ce que fait un objet, chiffré (soins, capture, bonus, où l'utiliser) ; itemHelp : puis sa description
std::string itemDetails(const ItemDef& d);
std::string itemHelp(const ItemDef& d);
// Forces et faiblesses d'une espèce (types, résistances propres, états impossibles), pour sa fiche
// known : faiblesses découvertes (Game::weak) ; seulement celles-là sont citées (créature pas encore obtenue)
std::string matchupText(const Species& s, const std::set<std::string>* known = nullptr);
// La même chose en listes (fiche tactique du Bestiaire) : known nul = tout est connu (espèce obtenue) ; sinon seules
// les faiblesses et absorptions découvertes, les résistances restent inconnues (full faux)
struct Matchup {
  std::vector<std::pair<Type, float>> weak, resist, absorb;  // type et multiplicateur (résiste : de 0 à 1)
  int hidden = 0;            // faiblesses pas encore découvertes
  float phys = 1, mag = 1;   // coups physiques, magie (résistances propres et talent)
  bool full = false;
  bool physKnown = false, magKnown = false;  // (2026-10-25) découverts en frappant (ou espèce obtenue)
};
Matchup knownMatchup(const Species& s, const std::set<std::string>* known);

// Préparation d'un combat
struct BattleSetup {
  std::vector<FighterP> foes;     // 1 à 3 ennemis en première ligne
  std::vector<FighterP> reserve;  // renforts : entrent un par un quand un ennemi tombe
  std::vector<FighterP> allies;   // vide : la première ligne de l'équipe (combat du gardien à plusieurs : tout le groupe)
  std::string foeName;            // nom de l'adversaire (« Garo le braconnier »), vide pour des créatures sauvages
  bool boss = false, canFlee = true, canCapture = true;
  int theme = -1;                 // décor (Theme) ; -1 : celui de la carte actuelle
  // Défis de l'échiquier (3.4) : plateau imposé (cases des alliés et des ennemis, obstacles), sans météo ni terrain
  bool fixedBoard = false;
  std::vector<int> allyCells, foeCells, obstacleCells;
  std::string intro;  // message d'ouverture à la place de « … surgit » (défi : la leçon)
  bool training = false;  // entraînement : ni gains, et une défaite ne coûte rien (Game::startBattle)
  std::string music;      // morceau imposé (« musique » de l'action combat), sinon combat ou boss
  bool rain = false;      // il pleut sur la carte (Game::rainAmount) : la météo d'effet « pluie », si le décor la permet
  bool ambush = false;    // embuscade (2026-10-23) : la moitié des créatures surgit au fond du camp de l'équipe, face à elle
};

// Défi de l'échiquier (data/defis.json) : alliés, ennemis, obstacles et décor imposés (faux : défi inconnu)
bool challengeSetup(const std::string& id, BattleSetup& out);

class Battle {
 public:
  Battle(Game& game, BattleSetup setup, Theme bg);
  void update(float dt);
  void draw();
  bool finished() const { return finished_; }
  bool raining() const;  // météo d'effet « pluie » (bruit de pluie pendant le combat)
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
    int cell = -1;     // se déplacer vers cette case (tactique « Se mettre à l'abri »)
    FighterP duo;      // technique en duo : le partenaire
    bool wait = false; // rien à portée : il attend (son tour finit, il rejoue plus tôt)
  };
  // Techniques en duo (2.6) : celles que a peut lancer maintenant, avec quel partenaire
  struct DuoOpt {
    std::string move;
    FighterP partner;
  };
  std::vector<DuoOpt> duoOptions(const FighterP& a, bool ready = true) const;  // ready = false : sans regarder jauges et PM
  void duoMove(FighterP a, FighterP partner, const std::string& id, std::vector<FighterP> targets);
  FighterP duoPartner_;  // pendant useMove : le partenaire du duo (message)
  // Attaques annoncées (1.8) : un boss prépare sa plus grosse attaque, concentrée sur une zone du
  // damier allié ; elle part à son tour suivant sur ceux qui y sont restés (plus forte)
  struct Warning {
    FighterP who;
    std::string move;
    Area zone;
    int center;
  };
  std::vector<Warning> warnings_;
  std::vector<std::pair<int, int>> netWarn_;  // chez l'invité : zones annoncées reçues (case, zone)
  bool threatened(const FighterP& f) const;   // sa case est dans une zone annoncée
  int safeCell(const FighterP& f) const;      // case où il peut aller ce tour, hors des zones annoncées (-1 : aucune)
  bool bossPlan(FighterP e);                  // boss : annonce ou déclenche son attaque ; vrai si le tour est joué
  Game& G;
  Script sc;
  std::vector<FighterP> allies, foes, reserve;
  std::string foeName;
  bool boss, canFlee, canCapture;
  std::string music;  // morceau imposé (BattleSetup::music)
  Theme bg;
  float start = 0;
  FighterP actor, cursor, blink;
  float blinkT = 0, flashT = -1;
  Color flashCol;
  std::vector<Pop> pops;
  float popTop_ = 54;  // un texte flottant ne commence pas plus haut (sous l'ordre des tours, d'après le dernier dessin)
  std::vector<FighterP> defeated;
  bool finished_ = false, ending_ = false;
  // Fin d'un combat gagné : tous les gains dans une fenêtre, fermée par Entrée
  std::vector<std::string> summary_;
  std::vector<const Fighter*> levelUps_;  // alliés qui ont pris un niveau (étincelles dorées pendant la fenêtre des gains)
  bool summaryOn_ = false;
  float summaryT_ = 0;  // moment où elle s'est ouverte (elle se ferme seule après 8 s)
  BattleResult result_;

  bool isAlly(const FighterP& f) const;
  std::vector<FighterP> alive(const std::vector<FighterP>& v) const;
  Pt pos(const FighterP& f) const;      // à l'écran, en glissant vers sa nouvelle case
  Pt cellPos(int cell) const;           // centre d'une case du plateau à l'écran
  // Échiquier (3.4, data.hpp) : un seul plateau pour les deux camps, les alliés à gauche. La case de chaque
  // combattant pendant ce combat ; au départ, la formation de l'équipe (Fighter::cell) pour les alliés, un
  // placement automatique pour les ennemis, chacun dans sa moitié. On peut ensuite passer de l'autre côté.
  std::map<const Fighter*, int> cells_;
  // Regard (2026-10-23) : +1 vers la droite du plateau, -1 vers la gauche ; absent = vers le camp adverse.
  // Il se tourne en se déplaçant (vers où il va), en agissant (vers sa cible) et en attendant (vers l'adversaire
  // le plus proche) ; un coup au corps à corps venu du côté opposé à son regard est « dans le dos ».
  std::map<const Fighter*, int> facing_;
  bool ambush_ = false;  // ce combat a commencé par une embuscade
  std::set<const Fighter*> guarding_;  // en garde (2026-10-24, « Attendre ») : dégâts reçus × rules().guard jusqu'à son tour
  // Intention (2026-10-24) : à rules().intentAt % de sa jauge, un ennemi annonce ce qu'il va faire (case, technique,
  // cibles) ; dessinée au-dessus de lui et sur ses cibles. À son tour, il l'exécute si elle tient toujours (cibles
  // vivantes et à portée), sinon il réfléchit à nouveau. C'est ce qui laisse le temps de réagir : écarter la cible,
  // la mettre en garde, frapper l'ennemi d'abord.
  struct Intent {
    int cell = -1;
    std::string move;
    std::vector<FighterP> targets;
    bool wait = false;
    float t0 = 0;
  };
  std::map<const Fighter*, Intent> intents_;
  // Dessin seul (2026-10-25) : une intention qui vient de quitter intents_, exécutée (l'icône pulse) ou déjouée (elle se
  // brise en éclats) ; et depuis quand chacun est en garde (vu au dessin : le bouclier se déplie en 0,2 s)
  struct IntentFx {
    Intent in;
    float t0 = -10;
    bool foiled = false;
  };
  std::map<const Fighter*, IntentFx> intentFx_;
  std::map<const Fighter*, float> guardT_;
  float aimSoundT_ = -10;  // dernier son « vise » (une meute annonce presque en même temps : un seul signal)
  void declareIntent(const FighterP& e);
  std::string intentText(const Fighter& e) const;
  float lure(const FighterP& t) const;  // attrait d'une cible pour les ennemis (soigneur, lanceur, blessé)
  bool dangerAt(const FighterP& f, int cell) const;  // terrain qui blesse, zone rouge d'un boss, intention ennemie
  bool announced(const FighterP& f) const;           // ce boss a une attaque annoncée (zone rouge) en attente
  int aimedBy(const FighterP& f) const;              // ennemis vivants qui ont annoncé une attaque sur lui  // « Prépare Charge sur Maëlle » (vide : rien d'annoncé)
  std::map<const Fighter*, float> koT_;  // moment du K.O. (un ennemi vaincu s'efface en 0,6 s avec un éclat)
  int cellOf(const FighterP& f) const;  // -1 : pas sur le plateau
  void place(const std::vector<FighterP>& v, bool keep, bool left);
  const std::vector<FighterP>& sideOf(const FighterP& f) const { return isAlly(f) ? allies : foes; }
  const std::vector<FighterP>& oppOf(const FighterP& f) const { return isAlly(f) ? foes : allies; }
  FighterP at(int cell) const;                 // combattant debout sur cette case
  int freeCellNear(bool left, int near) const;  // case libre de ce camp, la plus proche de near (-1 : aucune)
  // Déplacement : une fois par tour, jusqu'à moveRange cases (pas de diagonale) ; les alliés se traversent, pas
  // les adversaires, et on s'arrête en arrivant au contact de l'un d'eux. Les volants passent par-dessus tout.
  std::set<const Fighter*> moved_;  // ceux qui se sont déjà déplacés pendant leur tour
  int moveRange(const FighterP& f) const;
  bool flier(const FighterP& f) const;
  // cases où il peut aller ce tour → nombre de pas (parent : d'où l'on arrive sur chaque case, pour le chemin)
  std::map<int, int> walkable(const FighterP& f, std::map<int, int>* parent = nullptr) const;
  std::vector<int> pathTo(const FighterP& f, int cell) const;  // les cases du chemin, de sa case jusqu'à cell
  std::map<const Fighter*, int> moveFrom_;                   // « Revenir » : sa case avant « Se déplacer » (ce tour)
  std::map<const Fighter*, int> faceFrom_;                   // … et son regard d'alors (0 : celui par défaut)
  int reach(const FighterP& a) const;                     // portée du corps à corps (1 : les 8 cases autour ; boss : plus)
  // cell / from : la case où il se trouverait (-1 : sa case actuelle)
  bool engaged(const FighterP& f, int cell = -1) const;   // un adversaire est au contact
  int foesAround(const FighterP& d, int cell = -1) const; // adversaires de d au contact
  bool behind(const FighterP& a, const FighterP& d, int from = -1) const;  // a frappe d dans le dos (côté du camp de d)
  float posMult(const FighterP& a, const FighterP& d, const Move& m, int from = -1) const;  // dos, encerclé, distance au contact
  int approachCell(const FighterP& a, const FighterP& t) const;  // case d'où frapper t au corps à corps (-1 : aucune)
  float posValue(const FighterP& a, int cell);  // intérêt de cette case pour l'ordinateur (attaques possibles, danger)
  int planMove(const FighterP& a);              // ordinateur : où aller avant d'agir (-1 : rester)
  void step(const FighterP& a, int cell);       // le déplacer (glisse à l'écran ; Percée en atteignant le bord adverse)
  std::set<const Fighter*> breakthrough_;       // ceux qui ont déjà fait leur Percée
  float quietT_ = 0;    // temps de jauges sans que personne prenne de dégâts : au-delà de 40 s, l'ordinateur va au contact
  long long hpSum_ = -1;  // somme des PV de tous, pour voir s'ils baissent (dégâts, poison…)
  // éclat dessiné par-dessus un combattant (Percée, Brisure : images gfx_<fx>_k de Godot ; « garde » : le bouclier qui
  // encaisse, « riposte » : élan vers l'attaquant, anim_.actor ; ces deux-là dessinés en code)
  struct Burst {
    FighterP f;
    const char* fx;
    float t0;
  };
  std::vector<Burst> bursts_;
  std::map<const Fighter*, std::pair<Pt, float>> slide_;  // dessin : d'où il part et quand
  void resumeTurn(FighterP a);                  // après « Se déplacer » : le même combattant choisit son action
  void waitTurn(FighterP a);                    // « Attendre » : finir son tour sans agir (rejoue plus tôt)
  int netCell(int c) const;                     // duel : le plateau est vu en miroir chez l'invité
  bool facesRight(const FighterP& f) const;     // son regard (facing_)
  int facingOf(const FighterP& f) const;        // +1 ou -1
  FighterP backReach(const FighterP& a, int cell) const;  // un adversaire que a frapperait dans le dos depuis cette case
  void face(const FighterP& a, int cell);       // se tourne vers cette case (rien si elle est dans sa colonne)
  int nearestFoeSide(const FighterP& f) const;  // +1 ou -1 : le côté de l'adversaire le plus proche
  // Adversaires de f qui pourront le frapper au corps à corps s'il se tient sur cette case (au prochain tour :
  // déplacement + portée, sans compter les détours) : marqués en rouge quand on choisit où aller
  int meleeThreat(const FighterP& f, int cell) const;
  // Corps à corps : un adversaire au contact, ou qu'on peut rejoindre ce tour (on s'approche avant de frapper)
  bool reachable(const FighterP& a, const Move& m, const FighterP& t) const;
  // Victimes d'une technique visant center : center d'abord, puis les autres combattants de sa zone
  std::vector<FighterP> zoneTargets(const Move& m, const FighterP& center) const;
  float zoneScore(const FighterP& a, const Move& m, const FighterP& center) const;  // attackScore sur toute la zone
  const Move* aiming_ = nullptr;  // technique dont on choisit la cible : sa zone est surlignée
  // Choix sur le damier (3.2) : la cible ou la case se choisit directement sur le damier (flèches ou souris).
  // Le menu correspondant existe toujours (joueur automatique, mode test, aide de la fenêtre du haut) mais
  // n'est pas dessiné (Menu::hidden) ; cells : la case de chaque ligne de ce menu.
  struct BoardPick {
    size_t depth = 0;  // place du menu caché dans la pile (0 : aucun choix en cours)
    std::vector<int> cells;
    bool move = false;  // « Se déplacer » : le combattant apparaît en transparence sur la case
  };
  BoardPick pick_;
  void boardMenu(Menu t, std::vector<int> cells, bool move = false);
  bool boardPicking() const;
  void boardInput();
  int cellUnder(float x, float y, const std::vector<int>& among) const;  // index dans among (-1 : aucune)
  // Aperçu de la zone en survolant une technique, avant même de choisir la cible
  std::string preview_;
  size_t previewDepth_ = 0;
  void previewMove(const FighterP& a, const std::string& id);
  void drawZones(float t);
  void drawHud(float t);
  void moveTo(FighterP a, int cell);  // « Se déplacer » : change de case, puis choisit son action
  void pop(const FighterP& f, const std::string& t, Color c);
  // Bruitage (audio.hpp), seulement pour le combat affiché (pas les simulations) ;
  // share : l'hôte ou le chef le fait aussi entendre aux autres joueurs
  void sound(const std::string& id, bool share = true);
  bool shown() const;  // c'est le combat affiché à l'écran
  // Compagnons : quand des héros combattent, les créatures alliées sont leurs compagnons ; le
  // joueur les commande comme les héros (leur jauge avance moins vite : règle compagnon_atb)
  bool hasHero_ = false;
  bool companion(const FighterP& f) const { return hasHero_ && isAlly(f) && !f->S().human; }
  // Animation d'une action : élan du lanceur vers ses cibles, puis effet sur elles selon le
  // genre de l'action (Fx, plus haut) et la couleur de son type
  struct Anim {
    FighterP actor;
    std::vector<FighterP> targets;
    Fx fx = Fx::Hit;
    Color col;
    bool limit = false;
    float t0 = -10;
    int type = -1;  // type de la technique : effet propre à chaque type (flammes, gouttes, feuilles…)
  };
  Anim anim_;
  void startAnim(const FighterP& a, const std::vector<FighterP>& targets, Fx fx, Color col, bool limit = false, int type = -1);
  void drawTypeFx(const Pt& p, float k);  // particules propres au type de la technique (anim_.type)
  Pt offset(const FighterP& f) const;  // décalage du dessin : élan, ou secousse quand il est touché
  void drawAnim();

  void tickATB(float dt);
  // Ordre des tours (2.5, inspiré de Final Fantasy X) : les n prochains tours prévus d'après les
  // jauges et les vitesses actuelles (sans le hasard de la paralysie)
  std::vector<FighterP> turnOrder(int n) const;
  void drawTurnOrder(float y, float minX = 0, float maxX = 320);
  bool skipTurn(FighterP f);  // sommeil ou paralysie : le tour est perdu
  void command(FighterP a);
  void autoCommand(FighterP a);
  void enemyTurn(FighterP e);
  Plan think(FighterP a);
  float attackScore(const FighterP& a, const Move& m, const FighterP& t) const;  // intérêt d'une attaque (IA et tactiques)
  // Dégâts prévus (3.3, aide du choix de la cible) : « ≈ 18-24 PV · 95 % » (sans coup critique). L'efficacité d'un type
  // n'est comptée qu'une fois découverte (Brisure) : sinon neutre, pour ne rien dévoiler.
  std::string damageText(const FighterP& a, const Move& m, const FighterP& d) const;
  // Dégâts prévus d'une attaque (même formule qu'applyHit, sans hasard ni critique) ; eff < 0 : absorbé, 0 : sans effet ;
  // fromCell : la case d'où l'attaquant frappera (sinon celle où il s'approchera)
  struct Preview {
    int lo = 0, hi = 0;
    float hit = 0, eff = 1;
    bool back = false;
  };
  Preview preview(const FighterP& a, const Move& m, const FighterP& d, int fromCell = -1) const;
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
  // boost > 1 : attaque annoncée (cibles déjà choisies, pas de zone autour, dégâts multipliés)
  void useMove(FighterP a, const std::string& mv, std::vector<FighterP> targets, bool isLimit = false, float boost = 1);
  void useItem(FighterP a, const std::string& it, FighterP target);
  void swapIn(FighterP a, FighterP r);
  void tryFlee(FighterP a);
  void afterAction(FighterP a, float gauge = 0);  // gauge : jauge ATB de départ (technique rapide ou lourde)
  void checkEnd();
  void victory();
  void finish(BattleResult r);
  int applyHit(const FighterP& a, const FighterP& d, const Move& m, float mult = 1);  // -1 si raté ; mult : autour de la cible d'une zone
  void applyEffect(const FighterP& t, const Effect& e);
  void breakFoe(const FighterP& d);  // Brisure : bouclier à zéro
  // Astuces (2.8) : seulement dans le combat affiché, joué par le joueur ; startTips : au début du combat
  void tip(const char* id);
  void startTips();
  bool startTips_ = false;
  bool training_ = false;  // entraînement (BattleSetup::training)
  void initFoe(const FighterP& e);   // bouclier de Brisure d'un ennemi qui entre (début, renfort)
  // Phases d'un boss (2.7) : sous un seuil de PV, message, bouclier rechargé, bonus, techniques,
  // météo, renforts (jamais en ligne)
  void checkPhase(const FighterP& f);
  // Rôles (2.7) : protecteur (ses voisins prennent moins de dégâts), soigneur (l'ordinateur soigne
  // ses alliés), chef (ses alliés ont une jauge plus rapide)
  float guardMult(const FighterP& d) const;  // dégâts reçus par d (protecteur voisin vivant)
  bool roleHeal(const FighterP& e);          // ennemi soigneur : soigne un allié blessé ; vrai si joué
  // 2.6 : pousser / tirer (Into the Breach). La cible change de colonne ; case prise ou bord : choc.
  // Les combattants mis K.O. par le choc sont ajoutés à ko.
  void shove(const FighterP& a, const FighterP& t, const Move& m, int dmg, std::vector<FighterP>& ko);
  // Case d'arrivée, dans l'axe du lanceur (-1 : bord ; -2 : boss ou rien à faire, ne bouge pas)
  int shoveDest(const FighterP& a, const FighterP& t, const Move& m) const;
  // Techniques de mouvement (3.4) : bond derrière la cible, échange de cases ; case d'arrivée du bond (-1 : impossible)
  int leapDest(const FighterP& a, const FighterP& t) const;
  void motionAfterHit(const FighterP& a, const FighterP& t, const Move& m, std::vector<FighterP>& ko);
  // Météo (indice dans rules().weathers, -1 : temps calme) et cases de terrain du plateau
  int weather_ = -1;
  std::array<int, BOARD_CELLS> terrain_{};  // indice dans rules().terrains, -1 : rien
  // Obstacles (3.4) : case → sprite du décor (rocher, buisson…) ; on ne s'y arrête pas, on ne les traverse qu'en volant
  std::map<int, std::string> obstacles_;
  bool blockedCell(int c) const { return obstacles_.count(c) > 0; }
  const Rules::Terrain* terrainOf(int cell) const;
  const Rules::Terrain* terrainAt(const FighterP& f) const;  // case spéciale sous ce combattant (nullptr : aucune)
  bool terrainHits(const FighterP& f, const Rules::Terrain* t) const;  // le terrain agit sur lui (pas insensible)
  bool terrainHurts(const FighterP& f, int cell) const;                 // cette case le blesse ou le ralentit
  bool terrainTurn(const FighterP& f, bool arrived);                    // effet de sa case ; vrai s'il tombe K.O.
  float weatherMult(int type) const;
  float atbRate(const FighterP& f) const;  // vitesse de sa jauge ATB (compagnon, verglas)
  void drawWeather();
  float shakeT_ = -10;               // l'écran tremble (brisure, coup critique)
  bool countering_ = false;          // une riposte (gemme) est en cours : pas de riposte à la riposte
  void knockOut(const FighterP& f);

  // Menus de commande
  void pickFoe(const std::string& title, std::function<std::string(const Fighter&)> info, std::function<void(FighterP)> done,
               bool creaturesOnly = false);
  void pickAlly(const std::string& title, bool ko, std::function<void(FighterP)> done);
  void moveTarget(FighterP a, const std::string& mv, FighterP duo = nullptr);
  void drawStatusTag(const Fighter& f, float x, float y);
  friend class Game;
  friend class Pilot;
};
