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
enum class Mode { Title, Map, Battle, Ending, Arena, Settings, Editor, Story };
enum class BattleResult { Win, Lose, Fled };

// Lit une ligne d'un membre de l'équipe dans une sauvegarde (voir game.cpp) ; faux si la ligne
// est d'un autre genre. Sert à Game::loadGame et au multijoueur (équipe du duel).
bool readMemberLine(const std::string& key, std::istream& s, std::vector<FighterP>& team);

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

  // Mode test automatique (voir main.cpp) : rejoue des situations et enregistre des captures
  int selfTest(SDL_Surface* target, const std::string& outDir);

  // ---- État partagé avec le combat ----
  Gfx g;
  Input in;
  MenuStack menus;
  Script sc;
  float time = 0;
  Mode mode = Mode::Title;

  std::vector<FighterP> team;          // les 3 premiers valides combattent
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
  bool npcVisible(const Npc& n) const;
  bool bossAlive(const BossSpot& b) const;
  void tryMove(int d);
  void arrive();
  void interact();
  void talk(const Npc& n);
  void openChest(int idx);
  std::string chestFlag(int idx) const;
  bool checkSight();       // un dresseur repère le joueur ?
  int exclaimNpc_ = -1;    // habitant qui affiche « ! »
  float exclaimT_ = 0;
  void changeMap(int m, int x, int y, int d);
  void encounter();
  void defeat();
  void showRegionBanner();
  void updateMusic();  // musique du lieu, du combat ou de l'écran titre

  // Menus
  void titleMenu();
  void starterMenu();
  void newGame(const std::string& starter);
  void pauseMenu();
  void tacticsMenu(int sel = 0);
  void teamMenu();
  void itemMenu();
  void magicMenu();
  void shopMenu(const std::vector<std::string>& stock);
  void journalMenu();
  void gearMenu(int sel = 0);
  void gearSlots(FighterP f, int who, int sel = 0);
  void gearPick(FighterP f, int who, int slot);
  void optionsMenu(int sel = 0);
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
  void drawDialogue();
  void drawEnding();
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
  bool duelBattle_ = false;                 // le combat en cours est un duel en ligne
  bool groupBattle_ = false;                // le combat en cours est celui d'un gardien, à plusieurs (coop.cpp)
  void bossTouched(const BossSpot& b);      // un gardien d'expédition à plusieurs attend tout le groupe
  bool inExpedition() const;
  bool arenaBattle_ = false;      // le combat en cours a été lancé depuis l'Arène
  void toolsMenu();
  std::function<void(BattleResult)> afterBattle_;
  int panelMode_ = 0;        // 0 rien, 1 résumé de l'équipe, 2 fiche détaillée
  int teamPanelSel_ = 0;
  int swapFrom_ = -1;
  std::string notice_;       // petit message en bas de l'écran
  float noticeT_ = 0;
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
};
