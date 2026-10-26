// Le jeu : écran titre, exploration, dialogues, menus, sauvegarde.
// Le combat est dans battle.hpp / battle.cpp.
#pragma once
#include <SDL.h>

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "battle.hpp"
#include "data.hpp"
#include "events.hpp"
#include "gfx.hpp"
#include "ui.hpp"
#include "world.hpp"

class Battle;
class Arena;
class Settings;
class MapEditor;
class StoryEditor;
class Expedition;
class Online;
class Pilot;
enum class Mode { Title, Map, Battle, Ending, Arena, Settings, Editor, Story };
enum class BattleResult { Win, Lose, Fled };

// Lit une ligne d'un membre de l'équipe dans une sauvegarde (voir game.cpp) ; faux si la ligne
// est d'un autre genre. Sert à Game::loadGame et au multijoueur (équipe du duel).
// links (facultatif) : reçoit les lignes « compagnon » (rang du héros, rang de sa créature)
bool readMemberLine(const std::string& key, std::istream& s, std::vector<FighterP>& team,
                    std::vector<std::pair<size_t, int>>* links = nullptr);

class Game {
 public:
  explicit Game(SDL_Renderer* r);
  ~Game();
  void onKey(SDL_Scancode sc, bool down, bool repeat);
  void onText(const char* utf8);  // caractères tapés (saisie de texte des éditeurs)
  void onMouse(int x, int y, int button, bool down);  // button -1 : simple déplacement
  void onWheel(int dy);
  // Manette (SDL_GameController) : bouton appuyé ou relâché ; stick gauche (axe 0 : horizontal, 1 : vertical)
  void onPad(int button, bool down);
  void onStick(int axis, int value);
  // Saisie de texte : affiche une fenêtre ; Entrée valide (done), Échap annule
  void editText(const std::string& title, const std::string& initial, int maxChars, std::function<void(const std::string&)> done);
  bool editingText() const { return textOn_; }
  void update(float dt);
  void draw();
  bool quit = false;
  // Vitesse du jeu (Options, touche F2) : main.cpp appelle update ce nombre de fois par image.
  // Toujours ×1 en multijoueur (duel, expédition à plusieurs : tout le monde au même rythme).
  int speed() const;
  void cycleSpeed();  // F2 : ×1 → ×2 → ×3 → ×1

  // Mode test automatique (voir main.cpp) : rejoue des situations et enregistre des captures
  int selfTest(SDL_Surface* target, const std::string& outDir);
  // Joueur automatique (pilot.hpp) : demo = dans la fenêtre, à vitesse normale ; autoGame :
  // partie rapide sans fenêtre (brumeval --partie DIR), rapport et captures dans DIR
  void startPilot(bool demo);
  int autoGame(SDL_Surface* target, const std::string& outDir);

  // ---- État partagé avec le combat ----
  Gfx g;
  Input in;
  MenuStack menus;
  Script sc;
  float time = 0;
  Mode mode = Mode::Title;

  std::vector<FighterP> team;          // 8 membres au plus (rules().maxTeam) ; voir front() pour qui combat
  std::vector<FighterP> reserve;       // créatures en plus de l'équipe (captures et cadeaux quand elle est complète)
  // Bestiaire : espèces de créatures rencontrées et obtenues (lignes « vu » et « pris » de la sauvegarde)
  std::set<std::string> seen, caught;
  std::set<std::string> visited;  // lieux de la carte du monde déjà visités (voyage rapide ; lignes « lieu » de la sauvegarde)
  // Brisure (2.5) : faiblesses découvertes (« espèce:type ») en frappant un ennemi sur l'une d'elles
  std::set<std::string> weak;
  bool weakKnown(const std::string& sp, int type) const { return weak.count(sp + ":" + std::to_string(type)) > 0; }
  void see(const Fighter& f) {
    if (!f.S().human) seen.insert(f.sp);
  }
  void noteBestiary();  // les créatures de l'équipe et de la réserve comptent comme vues et obtenues
  std::map<std::string, int> items;    // inventaire
  int gold = 0;
  std::set<std::string> flags;         // progression (boss vaincus, recrues, coffres…)
  bool tacticsAuto = true;             // mode auto : les tactiques jouent en combat (touche Tab)

  void startBattle(BattleSetup setup, std::function<void(BattleResult)> after);
  void startBattle(std::vector<FighterP> foes, bool boss, std::function<void(BattleResult)> after, bool canFlee = true,
                   bool canCapture = true);
  std::vector<FighterP> front() const;  // combattants en première ligne
  int avgLevel() const;
  void recruit(const std::string& id, int minLvl);
  void healAll(bool mpToo = true);
  bool has(const std::string& f) const { return flags.count(f) > 0; }

  // ---- Événements (data/evenements.json, voir events.cpp) ----
  void runEvent(const std::string& id);
  void runActions(const Json& list);
  bool checkCond(const Json& c) const;
  int questMark(const std::string& event) const;  // bulle d'un habitant : 1 « ! » (quête à prendre), 2 « ? » (à rendre), 3 épées (combat proposé)
  std::string fillText(std::string s) const;

 private:
  // Carte
  const MapDef& M() const { return maps()[mapId]; }
  int mapId = 0, px = 0, py = 0, dir = DOWN;
  bool moving = false;
  float moveT = 0;
  int fromX = 0, fromY = 0, steps = 0;
  int respawnMap = 0, respawnX = 0, respawnY = 0;
  float banner = 0;
  std::string bannerText;

  bool blocked(int x, int y) const;
  bool blockedOn(const MapDef& m, int x, int y) const;
  // Énigme de la carte (3.0) : rochers à pousser sur les plaques. boulders_ : leurs cases sur la carte affichée
  // (remis au départ en entrant sur la carte, sauf si l'énigme est résolue : ils restent sur les plaques)
  std::vector<std::pair<int, int>> boulders_;
  int bouldersMap_ = -1;  // carte dont boulders_ décrit les rochers
  void resetBoulders();
  bool boulderAt(int x, int y) const;
  bool puzzleSolved(const MapDef& m) const { return m.puzzleFlag.empty() || has(m.puzzleFlag); }
  void pushBoulder(int x, int y, int dx, int dy);  // le joueur pousse le rocher de (x, y)  // case occupée sur cette carte
  bool npcVisible(const Npc& n) const;
  bool chestShown(const MapDef& m, const Chest& c) const;  // ouvert, ou sa condition « si » est vraie
  bool bossAlive(const BossSpot& b) const;
  void tryMove(int d);
  void arrive();
  void interact();
  void enterDoor(const Building& b);
  void talk(const Npc& n);
  void openChest(int idx);
  std::string chestFlag(int idx) const;
  bool checkSight();       // un dresseur repère le joueur ?
  int exclaimNpc_ = -1;    // habitant qui affiche « ! »
  // Habitants de la carte affichée (3.3) : leur case actuelle (les promeneurs marchent autour de leur place) et leur
  // regard (ils se tournent vers le joueur qui leur parle). Rien n'est enregistré : tout revient en place en changeant
  // de carte.
  struct NpcWalk {
    int hx = -1, hy = -1;  // place d'origine (si l'éditeur la change, on repart de là)
    int x = 0, y = 0, fx = 0, fy = 0, dir = 1, side = 2;
    float t = 1, wait = 0, faceUntil = 0, talkT = -9;  // talkT : quand on lui a parlé (petit sursaut)
  };
  std::vector<NpcWalk> walk_;
  int walkMap_ = -1;
  uint32_t walkSeed_ = 7;  // hasard à part : les promeneurs ne changent pas les combats
  int talkNpc_ = -1;       // l'habitant à qui l'on parle : il regarde le joueur jusqu'à la fin de la conversation
  void syncWalkers();
  void updateWalkers(float dt);
  std::pair<int, int> npcCell(const MapDef& m, size_t i) const;  // case actuelle d'un habitant
  float exclaimT_ = 0;
  void changeMap(int m, int x, int y, int d);
  void encounter();
  int wildDepth(int x, int y) const;  // distance à pied du plus proche bâtiment ou passage (meutes)
  void defeat();
  int defeats_ = 0;  // défaites dans la partie principale (joueur automatique)
  void showRegionBanner();
  void updateMusic();  // musique du lieu, du combat ou de l'écran titre

  // Menus
  void titleMenu();
  void starterMenu();
  void autoSave();          // sauvegarde automatique (option), en changeant de carte à pied
  float autoSaveT_ = -10;   // moment de la dernière (petit « Sauvegarde… » à l'écran)
  std::string autoSaveFile_ = "sauvegarde.txt";  // seule la vraie partie se sauvegarde seule (le mode test le change)
  void newGame(const std::string& starter);
  void pauseMenu();
  void prepMenu();  // Échap > Préparation : formation, compagnons, tactiques, réserve
  void tacticsMenu(int sel = 0);
  void teamMenu();
  void itemMenu();
  void magicMenu();
  void shopMenu(const std::vector<std::string>& stock);
  // Évolution (evolution.cpp) : proposée après chaque combat gagné, ou par Échap > Évolution
  void offerEvolutions();                    // la première créature prête : « Évoluer ? » Oui/Non (puis la suivante)
  void evolveMember(const FighterP& f);      // l'effet de lumière, puis l'évolution
  void evolutionMenu();                      // les évolutions refusées, à faire maintenant
  void drawEvolution();
  std::string evoFrom_, evoTo_;              // effet en cours (espèces avant et après), vide : aucun
  bool evoChroma_ = false;                   // la créature qui évolue est chromatique
  float evoT_ = 0;                           // début de l'effet (Game::time)
  bool offerEvolve_ = true;
  // Astuces (2.8) : celles déjà montrées (lignes « astuce » de la sauvegarde) ; le mode test les coupe sauf dans
  // leur test ; tip() la montre dans ce script si elle est nouvelle (F1 ouvre alors sa page d'Aide)
  std::set<std::string> tipsSeen;
  bool tipsOff_ = false;
  bool tip(const std::string& id, Script& s);
  std::string adviceText() const;  // conseil pour le prochain boss (2.8) ; vide : tous battus                  // le mode test les coupe (personne ne répondrait), sauf pour les tester
  // Forge de Forgeroc (forge.cpp) : renforcer une arme ou une armure contre de l'or et des matériaux
  void forgeMenu(int sel = 0);
  bool forge(const FighterP& owner, const std::string& id);  // owner nul : l'objet est dans le sac
  int forgeStock(const std::string& id) const;               // « trophee » : tous les trophées
  std::string forgeCost(const ItemDef& d, std::string* missing) const;
  void journalMenu();
  void progressMenu();  // Journal > Progression : ce qui est déjà fait, en chiffres (2026-10-20)
  // Discussions entre les héros (discussions.cpp, data/discussions.json)
  void discussionsMenu(int sel = 0);
  void playDiscussion(const std::string& id);
  bool discussionOpen(const Json& d) const;
  int newDiscussions() const;
  void checkDiscussions();
  // Exploits (exploits.cpp, data/exploits.json) : objectifs à atteindre, drapeaux « exploit:<id> »
  void checkExploits();
  bool exploitsOff_ = false;  // mode test : pas de vérification des exploits (sauf dans leur test)
  void exploitsMenu();
  int exploitsDone() const;
  int lastNewTalks_ = 0;
  float talkCheckT_ = 0;
  // Nouvelle partie + (2026-10-20) : drapeaux « ngp1 », « ngp2 »… ; chaque tour ajoute NGP_LEVELS niveaux aux ennemis
  static constexpr int NGP_LEVELS = 12;
  int newGamePlus() const;
  void applyNewGamePlus() { foeLevelBonus = newGamePlus() * NGP_LEVELS; }
  void startNewGamePlus();
  // Primes (chasse.cpp, primes.hpp) : tableau d'annonces (tuile G) ; rang de chasseur selon les primes touchées
  void bountyMenu(int sel = 0);
  void claimBounty(const std::string& id);
  int bountiesClaimed() const;
  std::string hunterTitle() const;
  void companionsMenu(int sel = 0);  // la créature compagnon de chaque héros
  void reserveMenu(int sel = 0);     // déposer, reprendre ou échanger des créatures avec la réserve
  void bestiaryMenu(int sel = 0);    // toutes les créatures : vues, obtenues, fiche
  void formationMenu(int sel = 0);   // la case de chaque combattant sur le damier
  int formationCell_ = -1;           // case surlignée sur le dessin du damier (panelMode_ 4)
  bool formationPick_ = false;
  std::vector<std::pair<int, int>> trainingHp_;  // entraînement : PV et PM de l'équipe avant le combat (remis après)       // le choix de la case est ouvert : la souris peut la désigner sur le dessin
  int formationCellAt(int mx, int my) const;  // case de la formation sous la souris (-1 : aucune)
  void drawFormationPanel(int x, int y);
  std::string bestiarySel_;          // espèce dont la fiche est affichée (panelMode_ 3)
  int bestiaryPage_ = 0;             // 0 : la fiche ; 1 : ce que l'équipe sait pour la combattre (Gauche/Droite)
  void drawBestiaryPanel(int x, int y);
  void swapReserve(FighterP out, FighterP in);  // out : de l'équipe à la réserve ; in : de la réserve à l'équipe
  void sellMenu(const std::vector<std::string>& stock, int sel);  // boutique : vendre à moitié prix
  void gearMenu(int sel = 0);
  void gemMenu(FighterP f, int who, int sel = 0);  // gemmes serties dans l'équipement de f (2.7)
  void gearSlots(FighterP f, int who, int sel = 0);
  void gearPick(FighterP f, int who, int slot);
  void optionsMenu(int sel = 0);
  // Aide (help.cpp) : liste des pages, puis une page en plein écran (F1 partout)
  void helpMenu(int sel = 0);
  void openHelp(int page);
  void updateHelp();
  void drawHelp();
  // Pêche (peche.cpp, 2026-10-20) : face à l'eau avec la canne ; 0 attente, 1 ça mord, 2 ferrage
  struct Fishing {
    bool on = false;
    int state = 0, level = 1, need = 0, hits = 0, misses = 0, bx = 0, by = 0;
    float t = 0, wait = 0, cursor = 0, dir = 1, zone = 0, zoneW = 0, speed = 0, flash = 0;
    std::string fish;
  } fishing_;
  int camX_ = 0, camY_ = 0;  // caméra de la dernière carte dessinée (dessin de la pêche)
  void startFishing(int wx, int wy);
  void updateFishing(float dt);
  void endFishing(const std::string& msg);
  void drawFishing();
  // Cartes des gardiens (cartes.cpp, 2026-10-20) : un jeu de cartes à part, contre les joueurs de data/cartes_jeu.json
  struct CardGame {
    bool on = false, over = false, abandon = false, same = false;  // same : règle « Identique »
    std::string foe, foeName;
    int level = 1, turn = 0, phase = 0, sel = 0, cursor = 4;  // phase 0 : choisir la carte, 1 : la case
    std::array<std::string, 9> cell;
    std::array<int, 9> owner{};
    std::array<float, 9> flipT{};
    std::vector<std::string> hand[2];  // 0 : le joueur, 1 : l'adversaire
    float t = 0, wait = 0, sameT = 0;
    std::function<void(int)> done;  // 1 gagné, 0 égalité, -1 perdu
  } cards_;
  // Pension de Moulinbourg (pension.cpp, 2026-10-20) : deux créatures gagnent de l'expérience à chaque pas, et parfois un œuf
  std::vector<FighterP> pension_;
  std::vector<int> pensionLvl_;                       // leur niveau à l'arrivée (prix pour les reprendre)
  int pensionSteps_ = 0;                              // pas depuis le dernier tirage d'un œuf
  bool pensionEgg_ = false;                           // un œuf attend à la pension
  std::vector<std::pair<std::string, int>> eggs_;     // œufs emportés : espèce, pas avant l'éclosion
  void pensionStep();
  void pensionMenu(int sel = 0);
  void pensionPick();
  bool pensionCanEgg() const;
  std::vector<std::string> cardHand_;  // la main choisie (ligne « main_cartes ») ; complétée par les meilleures cartes
  int cardGames_ = 0;                  // parties jouées depuis le lancement (le joueur automatique rejoue après une défaite)
  void startCards(const std::string& foe, std::function<void(int)> done);
  void updateCards(float dt);
  void endCards();
  void drawCards();
  void drawCard(float x, float y, float w, float h, const std::string& sp, int who, bool lit, float flip);
  int cardFlips(int c, const std::string& sp, int who, bool apply);
  std::pair<int, int> cardChoice(int who, int level);
  void cardsMenu(const std::string& foe, std::function<void(int)> done);
  void cardHandMenu(int sel = 0);
 public:
  std::vector<std::string> cardCollection() const;  // toutes les cartes obtenues, les plus fortes d'abord
  std::vector<std::string> cardDeck() const;        // les cinq cartes jouées
  int cardsWon() const;                             // adversaires déjà battus
  int fishCaught() const;  // espèces de poissons du carnet
 private:
  bool worldMapOn_ = false;  // carte du monde ouverte (3.3, menu pause) : le reste du jeu attend
  void drawWorldMap();
  // Voyage rapide (2026-10-20) : les villages (une maison de soin) visités et atteignables d'ici ; flèches pour choisir,
  // Entrée pour y aller (devant la maison de soin)
  std::vector<int> travelTargets() const;
  int worldSel_ = -1;  // la destination choisie (index de carte), -1 : aucune
  std::vector<bool> worldOpen() const;  // les lieux de la carte du monde atteignables d'ici par des passages ouverts
  int helpPage_ = -1, helpScroll_ = 0, lastHelp_ = 0;  // page affichée (-1 : aucune), défilement, dernière vue (F1)
  void keysMenu(int sel = 0);
  bool padDir_[4] = {}, stickDir_[4] = {};  // directions tenues à la manette (croix, stick)
  float padRepeat_[4] = {};                  // répétition d'une direction tenue (menus)
  void padDirection(int d, bool on);
  int bindKey_ = -1;  // Options > Touches : action qui attend sa nouvelle touche
  void bindKey(SDL_Scancode k);
  void drawKeyPrompt();
  void pickMember(const std::string& title, std::function<bool(const Fighter&)> ok, std::function<void(Fighter&)> use);
  void ask(const std::string& q, std::function<void()> yes, std::function<void()> no = nullptr);
  void execAction(const Json& a);

  // Sauvegarde
  std::string saveName_ = "sauvegarde.txt";  // le mode test utilise un autre fichier
  std::string savePath() const;
  bool saveGame();
  bool loadGame();
  bool saveExists() const;

  // Dessin
  void drawTitle();
  void drawMap();
  // Jour et nuit (2.6) : 0 en plein jour, 1 au cœur de la nuit ; un jour dure 12 minutes de jeu.
  // Seulement dehors (pas les intérieurs ni les grottes obscures)
  float nightAmount() const;
  bool isNight() const;  // l'heure du jeu (condition « nuit »), même dans un intérieur
  bool coastal(const MapDef& m) const;  // beaucoup d'eau : mouettes au-dessus, la mer au fond des combats
  float rainAmount() const;  // 0 à 1 : une averse sur la carte (dessin, et météo Pluie des combats)
  bool outdoors() const;
  void drawDialogue();
  void drawEnding();
  std::string endingText_;  // texte de l'écran de fin (action « fin »)
  void drawTeamPanel(int x, int y, int sel);

  bool textOn_ = false;
  std::string textTitle_, textValue_;
  int textMax_ = 0;
  std::function<void(const std::string&)> textDone_;
  void drawTextEdit();
  std::unique_ptr<Battle> battle_;
  std::unique_ptr<Arena> arena_;  // Arène de combat (écran titre > Outils)
  std::unique_ptr<Settings> settings_;  // Réglages (écran titre > Outils)
  std::unique_ptr<MapEditor> editor_;   // Éditeur de cartes (écran titre > Outils)
  bool editorTest_ = false;             // partie de test lancée depuis l'éditeur
  std::unique_ptr<StoryEditor> story_;  // Éditeur d'histoire (écran titre > Outils)
  bool storyTest_ = false;              // événement joué depuis l'éditeur d'histoire
  std::unique_ptr<Expedition> expedition_;  // mode Expédition (écran titre), voir expedition.hpp
  std::unique_ptr<Online> online_;          // multijoueur (écran titre), voir online.hpp
  bool running_ = false;  // le joueur court (Maj ou X maintenu) pendant ce pas
  // Compagnon qui suit (3.3, dessin seul) : les dernières cases quittées par le joueur ; la créature marche une case
  // derrière lui. Vidé à chaque changement de carte (elle réapparaît au premier pas).
  std::vector<std::pair<int, int>> trail_;
  // Empreintes dans la neige (3.3, dessin seul) : les dernières cases quittées sur une carte de neige, qui s'effacent
  struct Footprint {
    int x, y, dir;
    float t;
  };
  std::vector<Footprint> footprints_;
  // Petits effets de pas (2026-10-20, dessin seul) : brins d'herbe qui volent (hautes herbes), poussière (course)
  struct Puff {
    float x, y, t;  // pixels de la carte, moment
    int kind;       // 0 : herbe, 1 : poussière
  };
  std::vector<Puff> puffs_;
  struct ChestFx {  // coffre qu'on vient d'ouvrir (dessin seul) : un éclat de lumière et l'icône de l'objet qui monte
    int x = 0, y = 0;
    float t = -10;
    std::string icon;  // vide : de l'or
    int map = -1;
  } chestFx_;
  int followDir_ = DOWN;
  float mapFadeT_ = -10;  // moment du dernier changement de carte (fondu depuis le noir, 0,3 s)
  std::unique_ptr<Pilot> pilot_;            // joueur automatique (démo, partie rapide), voir pilot.hpp
  std::string saveBeforePilot_;             // fichier de sauvegarde d'avant la démo (rendu par l'écran titre)
  bool duelBattle_ = false;                 // le combat en cours est un duel en ligne
  bool groupBattle_ = false;                // le combat en cours est celui d'un gardien, à plusieurs (coop.cpp)
  void bossTouched(const BossSpot& b);      // un gardien d'expédition à plusieurs attend tout le groupe
  bool inExpedition() const;
  bool arenaBattle_ = false;      // le combat en cours a été lancé depuis l'Arène
  void toolsMenu();
  std::function<void(BattleResult)> afterBattle_;
  int panelMode_ = 0;        // 0 rien, 1 résumé de l'équipe, 2 fiche détaillée, 5 comparaison de la boutique
  std::string shopHover_;    // boutique : l'objet survolé (panneau de comparaison, drawShopPanel)
  void drawShopPanel(int x, int y);
  int teamPanelSel_ = 0;
  std::pair<int, std::string> gearHover_{-1, ""};  // (2026-10-26) Équipement : emplacement et objet survolés (« » : retirer), la fiche les montre
  int swapFrom_ = -1;
  std::string notice_;       // petit message en bas de l'écran
  float noticeT_ = 0;
  float speedT_ = 0;
  bool demoPaused_ = false;  // démo : Espace la met en pause (Game::onKey)  // affiche la vitesse en grand quelques instants après un changement
  std::string askText_;      // question affichée pendant un choix Oui/Non
  void notice(const std::string& s) { notice_ = s, noticeT_ = 2.2f; }
  void teamMenuAt(int sel);
  void itemMenuAt(int sel);
  void shopMenuAt(const std::vector<std::string>& stock, int sel);
  friend class Battle;
  friend class Arena;
  friend class Settings;
  friend class MapEditor;
  friend class StoryEditor;
  friend class Expedition;
  friend class Online;
  friend class Pilot;
};

// Pêche (peche.cpp) : vérification de data/peche.json (mode test) et nombre d'espèces de poissons
std::vector<std::string> checkFishing();
int fishSpecies();

// Discussions (discussions.cpp) : vérification de data/discussions.json (mode test) et nombre de discussions
std::vector<std::string> checkDiscussionsData();
std::vector<std::string> checkExploitsData();
std::vector<std::string> checkCardsData();  // data/cartes_jeu.json
std::array<int, 4> cardValues(const std::string& sp);  // haut, droite, bas, gauche (1 à 10)
int cardFoeCount();
int cardTotal();  // toutes les cartes qu'on peut obtenir
std::string baseForm(const std::string& sp);  // la forme de base (avant toute évolution)
bool cardFoeExists(const std::string& id);
int exploitCount();
void checkConditionData(const Json& c, const std::string& where, std::vector<std::string>& err);  // une condition « si » bien écrite
int discussionCount();
