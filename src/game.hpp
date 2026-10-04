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

#include "data.hpp"
#include "gfx.hpp"
#include "ui.hpp"
#include "world.hpp"

class Battle;
enum class Mode { Title, Map, Battle, Ending };
enum class BattleResult { Win, Lose, Fled };

class Game {
 public:
  explicit Game(SDL_Renderer* r);
  ~Game();
  void onKey(SDL_Scancode sc, bool down, bool repeat);
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
  static constexpr int MAX_TEAM = 8;

  void startBattle(std::vector<FighterP> foes, bool boss, std::function<void(BattleResult)> after, bool canFlee = true,
                   bool canCapture = true);
  std::vector<FighterP> front() const;  // combattants en première ligne
  int avgLevel() const;
  void recruit(const std::string& id, int minLvl);
  void healAll(bool mpToo = true);
  bool has(const std::string& f) const { return flags.count(f) > 0; }

 private:
  // Carte
  const MapDef& M() const { return maps()[mapId]; }
  int mapId = 0, px = 0, py = 0, dir = DOWN;
  bool moving = false;
  float moveT = 0;
  int fromX = 0, fromY = 0, steps = 0;
  int respawnMap = 0, respawnX = 7, respawnY = 7;
  float banner = 0;
  std::string bannerText;

  bool blocked(int x, int y) const;
  bool npcVisible(const Npc& n) const;
  bool bossAlive(const BossSpot& b) const;
  void tryMove(int d);
  void arrive();
  void interact();
  void talk(const Npc& n);
  void door(const Building& b);
  void bossEvent(const BossSpot& b);
  void openChest(int idx);
  void changeMap(int m, int x, int y, int d);
  void encounter();
  void defeat();
  void showRegionBanner();

  // Menus
  void titleMenu();
  void starterMenu();
  void newGame(const std::string& starter);
  void pauseMenu();
  void teamMenu();
  void itemMenu();
  void magicMenu();
  void shopMenu(const std::vector<std::string>& stock);
  void pickMember(const std::string& title, std::function<bool(const Fighter&)> ok, std::function<void(Fighter&)> use);
  void ask(const std::string& q, std::function<void()> yes, std::function<void()> no = nullptr);
  void ending();

  // Sauvegarde
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

  std::unique_ptr<Battle> battle_;
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
};
