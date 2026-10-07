// Éditeur de cartes (écran titre > Outils > Éditeur de cartes).
//
// Trois calques :
//   Tuiles  : peindre au pinceau, en rectangle ou par remplissage
//   Objets  : habitants, coffres, panneaux, passages, boss, bâtiments
//   Zones   : zones de rencontres et zones déclencheuses d'événements
// Souris : clic gauche = peindre / modifier, clic droit = pipette (tuiles),
// molette = tuile suivante. Clavier : flèches = curseur, Entrée = peindre /
// modifier, Tab = calque suivant, Échap = menu, Ctrl+Z / Ctrl+Y = annuler / rétablir.
// « Tester ici » lance le jeu depuis le curseur ; Échap > « Retour à l'éditeur ».
#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "world.hpp"

class Game;

class MapEditor {
 public:
  explicit MapEditor(Game& g);
  void open(const std::string& mapId = "");
  void update(float dt);
  void draw();
  void returnFromTest();

  // Utilisé par le mode test
  enum class Layer { Tiles, Objects, Zones };
  enum class Tool { Brush, Rect, Fill };
  int mapIndexEdited() const { return map_; }
  int cursorX() const { return cx_; }
  int cursorY() const { return cy_; }
  void setCursor(int x, int y);
  void setTile(char c) { tile_ = c; }
  void setLayer(Layer l) { layer_ = l; }
  void setTool(Tool t) { tool_ = t; }
  void apply();   // comme Entrée : peindre, ou ouvrir le menu de l'objet sous le curseur
  void undo();
  void redo();
  bool dirty(int map) const;
  std::vector<std::string> problems() const;  // problèmes de la carte éditée
  void testHere();

 private:
  Game& G;
  int map_ = 0;
  int cx_ = 0, cy_ = 0;        // curseur (en cases)
  float camX_ = 0, camY_ = 0;  // caméra (en pixels de la carte)
  int zoom_ = 16;              // 16 : normal ; 8 : vue éloignée
  Layer layer_ = Layer::Tiles;
  Tool tool_ = Tool::Brush;
  char tile_ = '.';
  int testLevel_ = 12;
  // Action en attente d'une case (déplacer, redimensionner, 2e coin d'un rectangle)
  std::function<void(int, int)> pending_;
  std::string pendingText_;
  int anchorX_ = -1, anchorY_ = -1;
  bool stroke_ = false;  // trait de pinceau à la souris en cours
  std::vector<MapDef> undo_, redo_;
  std::map<std::string, Json> saved_;  // contenu des fichiers (au chargement ou au dernier enregistrement)
  std::string message_;
  float messageT_ = -10;

  MapDef& M();
  void begin();  // à appeler avant chaque modification (annulation)
  void say(const std::string& m);
  void follow();  // la caméra suit le curseur
  bool inView(int px, int py) const;
  void paintAt(int x, int y);
  void fill(int x, int y);
  void act(int x, int y);
  void save();
  void reload();
  void select(int index);

  void menuMain(int sel = 0);
  void menuTiles();
  void menuProps(int sel = 0);
  void menuSize();
  void menuOpen();
  void newMap();
  void menuProblems();
  void menuAdd(int x, int y);
  void editNpc(int i, int sel = 0);
  void editLines(int i, int sel = 0);
  void editChest(int i, int sel = 0);
  void editSign(int i);
  void editWarp(int i, int sel = 0);
  void editBoss(int i, int sel = 0);
  void editBuilding(int i, int sel = 0);
  void editZone(int i, int sel = 0);
  void editTrigger(int i, int sel = 0);
  void pickEvent(const std::string& current, std::function<void(const std::string&)> done);
  void askPosition(const std::string& text, std::function<void(int, int)> done);

  void drawMap();
  void drawBars();
};

// Identifiant de fichier à partir d'un nom : « Forêt d'Été » donne foret_d_ete
std::string makeSlug(const std::string& name);

// Tuiles disponibles et leur nom
struct TileInfo {
  char c;
  const char* name;
};
const std::vector<TileInfo>& tilePalette();
