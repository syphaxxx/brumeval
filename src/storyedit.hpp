// Éditeur d'histoire (écran titre > Outils > Éditeur d'histoire) : modifier les
// événements de data/evenements.json depuis le jeu.
//
// Un événement est une liste d'actions, ou plusieurs pages avec une condition.
// Les actions qui contiennent d'autres actions (question, si, combat) s'ouvrent
// comme des sous-listes. Chaque écran est désigné par son « chemin » JSON, par
// exemple /brann/actions/3/oui/0 (voir nlohmann::json_pointer) : goTo(chemin)
// ouvre le bon écran, goUp(chemin) revient au niveau du dessus.
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "store.hpp"

class Game;
struct MenuItem;

std::string actionSummary(const Json& a);    // « Dire : … », « Combat : … »
std::string conditionSummary(const Json& c);  // « drapeau boss1 et avec Herbe lunaire »

class StoryEditor {
 public:
  explicit StoryEditor(Game& g);
  void open();
  void update(float dt);
  void draw();
  void returnFromTest();

  // Utilisé aussi par le mode test
  bool change(const std::function<void(Json&)>& fn);
  void undo();
  void redo();
  bool dirty() const;
  void revert();
  void goTo(const std::string& ptr, int sel = 0);
  void play(const std::string& id);
  const std::vector<std::string>& errors() const { return errors_; }
  std::string testFlags;  // drapeaux posés avant de jouer un événement (séparés par des virgules)

 private:
  Game& G;
  Json saved_;
  std::vector<Json> undo_, redo_;
  std::vector<std::string> errors_;
  std::string message_, current_ = "", lastEvent_;
  float messageT_ = -10;
  int testLevel_ = 15;

  const Json& peek(const std::string& ptr) const;
  void say(const std::string& m);
  void save();
  void quit();
  std::vector<std::string> usage(const std::string& id) const;
  void goUp(const std::string& ptr);
  MenuItem textField(const std::string& ptr, const std::string& key, const std::string& label, const std::string& title, int maxChars,
                     int row);

  void menuEvents(const std::string& sel = "");
  void newEvent();
  void editEvent(const std::string& id, int sel = 0);
  void editPage(const std::string& id, int page, int sel = 0);
  void editList(const std::string& ptr, int sel = 0);
  void editAction(const std::string& ptr, int sel = 0);
  void addAction(const std::string& listPtr);
  void editCondition(const std::string& ptr, int sel = 0);
  void editEnemies(const std::string& ptr, int sel = 0);
  void editEnemy(const std::string& ptr, int sel = 0);
  void pickItems(const std::string& ptr);
  void organize(std::vector<MenuItem>& items, const std::string& ptr, bool keepOne);
};
