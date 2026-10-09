// Mode Expédition (écran titre) : des régions générées sans fin (procgen.hpp),
// de plus en plus fortes. Façon roguelite : une défaite termine l'expédition,
// mais elle rapporte des éclats de brume qui achètent au Camp des améliorations
// gardées d'une expédition à l'autre (plus de héros proposés, or de départ…).
//
// Pendant une expédition, le contenu généré remplace en mémoire les données du
// jeu (dataDoc, maps(), events()) ; elles sont remises en place au retour à
// l'écran titre (leave). La sauvegarde de l'expédition est à part
// (expedition.txt) : la partie principale n'est jamais touchée.
//
// Expédition à plusieurs (online.hpp, coop.cpp) : chaque joueur a ici sa propre
// expédition dans le monde de la même graine, avec sa sauvegarde
// (expedition_groupe_<pseudo>.txt). Une défaite en solo le ramène au village (knockedOut) ;
// seule une défaite contre le gardien, à plusieurs, arrête l'expédition.
#pragma once
#include <array>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

#include "data.hpp"
#include "procgen.hpp"
#include "world.hpp"

class Game;
class Gfx;

// Améliorations du Camp
enum Upgrade { U_HEROES, U_STARTERS, U_GOLD, U_BAG, U_LEVEL, U_CAPTURE, U_XP, N_UPGRADES };

// Progression gardée d'une expédition à l'autre (expedition_progres.txt)
struct Progress {
  int record = 0;  // région la plus lointaine atteinte
  int runs = 0;    // expéditions commencées
  int shards = 0;  // éclats de brume à dépenser
  int total = 0;   // éclats gagnés en tout
  std::array<int, N_UPGRADES> up{};
};

class Expedition {
  friend class Game;  // le mode test (test.cpp) vérifie le monde généré
 public:
  explicit Expedition(Game& g);
  bool active() const { return active_; }
  bool onScreen() const { return screen_; }  // les menus de l'expédition remplacent l'écran titre
  void menu(int sel = 0);                    // menu « Expédition » de l'écran titre
  void draw(Gfx& g);                         // fond, fiches et menus de ces écrans

  // Appelés par le jeu pendant une expédition
  bool atExit(int x, int y) const;  // sortie de la région, derrière le gardien
  void nextRegion();
  void onDefeat(bool gaveUp = false);  // l'expédition s'arrête : bilan, éclats, retour au titre (au salon à plusieurs)
  void leave();                        // remet les données du jeu en place
  void writeSave(std::ostream& f) const;
  bool readSave(const std::string& key, const std::string& rest);  // lignes « expedition » et « graine »
  std::string status() const;  // « Région 3 · graine 123456 »
  int region() const { return region_; }
  const procgen::Region& current() const { return current_; }
  uint64_t seed() const { return seed_; }
  const std::string& seedText() const { return seedText_; }
  bool beaten() const;  // le gardien de la région est vaincu

  // Expédition à plusieurs : reprend la sauvegarde de cette graine, sinon nouveau héros dans cette région
  bool group() const { return group_; }
  void startGroup(uint64_t seed, const std::string& text, int region);
  void knockedOut();  // défaite en solo pendant une expédition à plusieurs : réveil au village
  // Empreinte du monde d'une graine (régions 1 à n) : la même sur tous les ordinateurs
  static std::string worldHash(uint64_t seed, int regions);

  // Fichiers dans le dossier de sauvegarde (le mode test utilise d'autres noms)
  std::string saveName = "expedition.txt", groupName = "expedition_groupe.txt", progressName = "expedition_progres.txt";
  std::string groupTag;  // pseudo du joueur : une sauvegarde à plusieurs par pseudo (deux fenêtres sur le même ordinateur)
  std::string groupFile() const;  // expedition_groupe_<pseudo>.txt
  std::string saveFile() const { return group_ ? groupFile() : saveName; }
  Progress progress;
  void loadProgress();
  void saveProgress() const;
  bool saveExists() const;
  void removeSave(bool group = false) const;
  int savedRegion(bool group = false, uint64_t* seed = nullptr) const;  // région de l'expédition sauvegardée (0 : aucune)

  // Démarre directement une expédition (mode test) : graine, héros et créature par leur rang
  void quickStart(uint64_t seed, int hero, int starter);
  static int upgradeMax(int u);
  int upgradeCost(int u) const;

 private:
  Game& G;
  bool active_ = false, screen_ = false, group_ = false;
  uint64_t seed_ = 0;
  std::string seedText_;
  int region_ = 1;
  procgen::Content content_;
  procgen::Region current_;
  std::string preview_;  // héros ou créature montré(e) à droite pendant le choix
  // Données du jeu mises de côté pendant l'expédition
  bool backedUp_ = false;
  std::array<Json, N_DATAFILES> docs_;
  std::vector<MapDef> maps_;
  Json events_;

  std::string path(const std::string& name) const;
  void install();  // met le contenu généré à la place des données du jeu
  void restore();
  void generate(uint64_t seed, int k);  // monde de la graine jusqu'à la région k
  void confirmNew(uint64_t seed, const std::string& text);
  void start(uint64_t seed, const std::string& text);
  void chooseHero(int sel = 0);
  void chooseStarter(const std::string& hero, int sel = 0);
  void begin(const std::string& hero, const std::string& starter);
  void resume();
  void camp(int sel = 0);
  void enterRegion();
  void cancelSetup();
};
