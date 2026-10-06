// Arène de combat (écran titre > Outils) : composer deux équipes, lancer un combat
// à la main ou simuler des dizaines de combats automatiques, lire le journal.
// Les « modèles » reprennent tous les combats de data/evenements.json et les
// zones de rencontres des cartes.
#pragma once
#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "battle.hpp"

class Game;
enum class BattleResult;

// Fiche d'un combattant : statistiques, faiblesses et résistances, techniques
// (utilisée par l'Arène et les Réglages)
void drawFighterCard(Gfx& g, const Fighter& f, int x, int y, int w, int h, float t);

struct ArenaSlot {
  std::string sp;     // espèce ; vide : emplacement libre
  int lvl = 10;
  float hpMult = 1;   // multiplicateur de PV (comme « pv » dans les événements)
  bool boss = false;
};

class Arena {
 public:
  explicit Arena(Game& g);
  void open();
  void update(float dt);
  void draw();
  void onBattleEnd(BattleResult r, const std::vector<std::string>& log);

  // Composition : 0-2 en première ligne, 3-5 remplaçants (alliés) ou renforts (ennemis)
  std::array<ArenaSlot, 6> allies, foes;
  int theme = 0, simN = 20;
  bool withItems = true;
  struct Results {
    int total = 0, wins = 0, losses = 0, timeouts = 0;
    double time = 0, hpLeft = 0;
    std::array<int, 6> ko{};
  } results;
  std::string lastResult;
  std::vector<std::string> lastLog;

  // Utilisé par le mode test
  int presetCount() const { return (int)presets_.size(); }
  int findPreset(const std::string& name) const;
  void applyPreset(int i);
  void startSim(int n);
  bool simulating() const { return simRunning_; }
  void startManual();

 private:
  struct Preset {
    std::string name, info;
    std::vector<ArenaSlot> foes, reserve;
    int theme = -1;
  };
  Game& G;
  int hover_ = -1;              // emplacement survolé : 0-5 alliés, 6-11 ennemis
  std::string previewSp_;       // espèce survolée dans la liste de choix
  int previewLvl_ = 1;
  bool simRunning_ = false;
  int simTarget_ = 0;
  float simTime_ = 0, t0_ = 0;
  std::unique_ptr<Battle> sim_;
  std::vector<int> teamSlot_;   // pour chaque membre de G.team : son emplacement d'origine
  std::vector<Preset> presets_;
  // État du joueur mis de côté pendant les essais
  std::vector<FighterP> savedTeam_;
  std::map<std::string, int> savedItems_;
  int savedGold_ = 0;

  ArenaSlot& slot(bool foe, int i) { return foe ? foes[i] : allies[i]; }
  void menuMain(int sel = 1);
  void menuSlot(bool foe, int i, int sel = 0);
  void pickSpecies(bool foe, int i);
  void menuPresets();
  void menuLog();
  void loadPresets();
  FighterP make(const ArenaSlot& s) const;
  BattleSetup makeSetup() const;
  bool ready() const;
  void prepareTeam();
  void restore();
  void nextSim();
  void stepSim();
  void drawSummary(int x, int y, int w);
};
