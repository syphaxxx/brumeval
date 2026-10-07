// Réglages (écran titre > Outils > Réglages) : modifier depuis le jeu toutes les
// données de data/ (règles, espèces, techniques, types, objets), les essayer dans
// l'Arène, puis les enregistrer dans les fichiers.
//
// Chaque modification passe par change() : elle modifie le document JSON en
// mémoire (dataDoc) puis reconstruit les données du jeu (rebuildData). Si les
// données deviennent invalides, la modification est annulée.
#pragma once
#include <array>
#include <functional>
#include <string>
#include <vector>

#include "data.hpp"

class Game;
struct MenuItem;

class Settings {
 public:
  explicit Settings(Game& g);
  void open();
  void update(float dt);
  void draw();

  bool change(DataFile f, const std::function<void(Json&)>& fn);
  int dirtyCount() const;
  void revert();  // recharge les fichiers (abandonne les modifications)
  void save();
  bool inGrid() const { return grid_; }

 private:
  friend class Game;  // le mode test ouvre directement certains écrans
  enum class Panel { None, Species, Move, Item, Type };
  Game& G;
  Panel panel_ = Panel::None;
  std::string panelId_;
  int panelType_ = 0;
  int previewLvl_ = 20;
  bool grid_ = false;
  int gx_ = 0, gy_ = 0;
  std::string message_;
  float messageT_ = -10;
  std::array<Json, N_DATAFILES> saved_;  // contenu des fichiers au dernier chargement ou enregistrement
  std::vector<std::string> problems_;
  std::vector<Tactic> tactics_;          // tactiques de départ en cours de modification

  void snapshot();
  bool dirty(DataFile f) const;
  void say(const std::string& m);
  MenuItem textItem(const std::string& label, const std::string& title, std::function<std::string()> get,
                    std::function<void(const std::string&)> set, int maxChars);

  void menuMain(int sel = 0);
  void menuRules(int sel = 0);
  void menuStartItems(int sel = 0);
  void menuDrops(int sel = 0);
  void menuSpeciesList(const std::string& sel = "");
  void editSpecies(const std::string& id, int sel = 0);
  void menuResist(const std::string& id, int sel = 0);
  void menuImmune(const std::string& id, int sel = 0);
  void menuLearn(const std::string& id, int sel = 0);
  void pickMove(const std::string& title, const std::string& current, std::function<void(const std::string&)> done,
                std::function<void()> back, bool allowNone = true);
  void menuMovesList(const std::string& sel = "");
  void editMove(const std::string& id, int sel = 0);
  void menuEffect(const std::string& id, int sel = 0);
  void menuTypes(int sel = 0);
  void editType(int t, int sel = 0);
  void menuItemsList(const std::string& sel = "");
  void editItem(const std::string& id, int sel = 0);
  void quit();

  void drawGrid();
  void drawPanel(int x, int y, int w, int h);
};
