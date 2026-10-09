#include "expedition.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

#include "arena.hpp"
#include "events.hpp"
#include "game.hpp"
#include "online.hpp"

static const Color WHITE = rgb(0xffffff), GOLD = rgb(0xffd34d), MUTED = rgb(0xaab3d8);

// Améliorations du Camp : nom, effet, maximum, prix de base (multiplié par le niveau visé)
struct UpgradeDef {
  const char* name;
  const char* help;
  int max, cost;
};
static const UpgradeDef UPGRADES[N_UPGRADES] = {
    {"Héros proposés", "Un héros de plus au choix au départ (3 à 6).", 3, 30},
    {"Créatures proposées", "Une créature de plus au choix au départ (3 à 5).", 2, 25},
    {"Bourse", "+60 pièces d'or au départ.", 5, 15},
    {"Sac de voyage", "+2 Potions et +1 Lanterne au départ.", 3, 20},
    {"Entraînement", "L'équipe de départ commence un niveau plus haut.", 3, 40},
    {"Lanternes bénies", "Les captures réussissent un peu plus souvent.", 3, 25},
    {"Sagesse", "+15 % d'expérience à chaque combat.", 3, 35},
};
static const char* UPGRADE_IDS[N_UPGRADES] = {"heros", "creatures", "bourse", "sac", "entrainement", "lanternes", "sagesse"};

Expedition::Expedition(Game& g) : G(g) { loadProgress(); }

int Expedition::upgradeMax(int u) { return UPGRADES[u].max; }
int Expedition::upgradeCost(int u) const { return UPGRADES[u].cost * (progress.up[u] + 1); }

// ---------------------------------------------------------------------------
// Fichiers
// ---------------------------------------------------------------------------
std::string Expedition::path(const std::string& name) const {
  char* p = SDL_GetPrefPath("Brumeval", "Brumeval");
  std::string s = p ? p : "";
  SDL_free(p);
  return s + name;
}
void Expedition::loadProgress() {
  progress = Progress{};
  std::ifstream f(path(progressName));
  std::string line;
  while (std::getline(f, line)) {
    std::istringstream s(line);
    std::string k;
    s >> k;
    if (k == "record") s >> progress.record;
    else if (k == "expeditions") s >> progress.runs;
    else if (k == "eclats") s >> progress.shards;
    else if (k == "total") s >> progress.total;
    else if (k == "amelioration") {
      std::string id;
      int n = 0;
      s >> id >> n;
      for (int u = 0; u < N_UPGRADES; u++)
        if (id == UPGRADE_IDS[u]) progress.up[u] = std::clamp(n, 0, UPGRADES[u].max);
    }
  }
}
void Expedition::saveProgress() const {
  std::ofstream f(path(progressName));
  f << "PROGRES 1\n";
  f << "record " << progress.record << "\nexpeditions " << progress.runs << "\neclats " << progress.shards << "\ntotal " << progress.total << '\n';
  for (int u = 0; u < N_UPGRADES; u++) f << "amelioration " << UPGRADE_IDS[u] << ' ' << progress.up[u] << '\n';
}
std::string Expedition::groupFile() const {
  size_t dot = groupName.rfind('.');
  if (groupTag.empty() || dot == std::string::npos) return groupName;
  return groupName.substr(0, dot) + "_" + groupTag + groupName.substr(dot);
}
bool Expedition::saveExists() const { return savedRegion() > 0; }
void Expedition::removeSave(bool group) const { std::remove(path(group ? groupFile() : saveName).c_str()); }
int Expedition::savedRegion(bool group, uint64_t* seed) const {
  std::ifstream f(path(group ? groupFile() : saveName));
  std::string line;
  while (std::getline(f, line)) {
    std::istringstream s(line);
    std::string k;
    uint64_t sd;
    int k2 = 0;
    s >> k;
    if (k == "expedition" && (s >> sd >> k2)) {
      if (seed) *seed = sd;
      return k2;
    }
  }
  return 0;
}

// ---------------------------------------------------------------------------
// Contenu généré à la place des données du jeu
// ---------------------------------------------------------------------------
void Expedition::generate(uint64_t seed, int k) {
  seed_ = seed;
  content_ = procgen::generateBase(seed);
  for (int i = 1; i <= k; i++) current_ = procgen::generateRegion(seed, i, content_);
  region_ = k;
}

void Expedition::install() {
  if (!backedUp_) {
    for (int f = 0; f < N_DATAFILES; f++) docs_[f] = dataDoc(DataFile(f));
    maps_ = maps();
    events_ = events();
    backedUp_ = true;
  }
  dataDoc(DF_MOVES) = content_.moves;
  dataDoc(DF_SPECIES) = content_.species;
  Json looks = docs_[DF_LOOKS];
  for (auto& l : content_.looks) looks.push_back(l);
  dataDoc(DF_LOOKS) = looks;
  // Améliorations du Camp qui passent par les règles
  Json r = docs_[DF_RULES];
  r["capture"]["base"] = r["capture"].value("base", .15) + .05 * progress.up[U_CAPTURE];
  r["recompenses"]["xp_par_niveau"] = int(std::lround(r["recompenses"].value("xp_par_niveau", 9) * (1 + .15 * progress.up[U_XP])));
  // Départ : la région courante (les vérifications des cartes partent de là)
  Json& d = r["depart"];
  std::string mapId = current_.map["id"].get<std::string>();
  d["carte"] = mapId, d["x"] = current_.startX, d["y"] = current_.startY, d["direction"] = "droite";
  d["heros"] = content_.heroes[0];
  d["compagnons"] = Json::array({content_.starters[0], content_.starters[1], content_.starters[2]});
  d["reveil"] = Json{{"carte", mapId}, {"x", current_.startX}, {"y", current_.startY}};
  d.erase("evenement");
  dataDoc(DF_RULES) = r;
  for (auto f : {DF_TYPES, DF_ITEMS}) dataDoc(f) = docs_[f];
  rebuildData();
  maps() = {mapFromJson(current_.map)};
  events() = current_.events;
}

void Expedition::restore() {
  if (!backedUp_) return;
  for (int f = 0; f < N_DATAFILES; f++) dataDoc(DataFile(f)) = docs_[f];
  rebuildData();
  maps() = maps_;
  events() = events_;
  backedUp_ = false;
}

void Expedition::leave() {
  restore();
  active_ = false;
  screen_ = false;
  group_ = false;
  preview_.clear();
}

std::string Expedition::status() const { return "Région " + std::to_string(region_) + " · graine " + seedText_; }

// ---------------------------------------------------------------------------
// Menus de l'écran titre
// ---------------------------------------------------------------------------
void Expedition::menu(int sel) {
  if (active_) leave();
  loadProgress();
  screen_ = true;
  G.mode = Mode::Title;
  G.menus.clear();
  Menu m;
  m.title = "Expédition";
  m.x = 8, m.y = 40, m.w = 150, m.rows = 6, m.sel = sel;
  int saved = savedRegion();
  m.items.push_back({"Continuer", saved ? "région " + std::to_string(saved) : "", "Reprendre l'expédition sauvegardée.", saved > 0, [this] { resume(); }});
  m.items.push_back({"Nouvelle", "", "Une expédition dans un monde neuf, tiré au hasard.", true,
                     [this] { confirmNew(procgen::randomSeed(), ""); }});
  m.items.push_back({"Graine…", "", "Écrire une graine (un nombre ou un mot) : la même graine redonne le même monde.", true, [this] {
                       G.editText("Graine : un nombre ou un mot", "", 20, [this](const std::string& s) {
                         if (!s.empty()) confirmNew(procgen::seedFromText(s), s);
                       });
                     }});
  m.items.push_back({"Camp", std::to_string(progress.shards) + " éclats", "Dépenser les éclats de brume en améliorations gardées pour toujours.",
                     true, [this] { camp(); }});
  m.items.push_back({"Retour", "", "", true, [this] {
                       screen_ = false;
                       G.titleMenu();
                     }});
  m.onCancel = m.items.back().act;
  if (!saved && sel == 0) m.sel = 1;
  G.menus.push(m);
}

void Expedition::confirmNew(uint64_t seed, const std::string& text) {
  if (!saveExists()) return start(seed, text);
  Menu m;
  m.title = "Expédition en cours";
  m.x = 20, m.y = 92, m.w = 200, m.rows = 2;
  m.items.push_back({"L'abandonner", "", "L'expédition sauvegardée sera perdue (sans éclats).", true, [this, seed, text] {
                       removeSave();
                       start(seed, text);
                     }});
  m.items.push_back({"Garder", "", "", true, [this] { G.menus.pop(); }});
  m.onCancel = m.items.back().act;
  G.menus.push(m);
}

void Expedition::start(uint64_t seed, const std::string& text) {
  seedText_ = text.empty() ? std::to_string(seed) : text;
  generate(seed, 1);
  active_ = true;
  install();
  chooseHero();
}

void Expedition::cancelSetup() {
  leave();
  menu(1);
}

// Choix du héros parmi ceux de la graine (3 à 6 selon le Camp)
void Expedition::chooseHero(int sel) {
  G.menus.clear();
  int n = std::min((int)content_.heroes.size(), 3 + progress.up[U_HEROES]);
  Menu m;
  m.title = "Votre héros";
  m.x = 4, m.y = 30, m.w = 152, m.rows = 6, m.sel = sel;
  for (int i = 0; i < n; i++) {
    std::string id = content_.heroes[i];
    const Species& s = species(id);
    std::string cls = s.role.substr(0, s.role.find(','));
    std::string lim = s.limit.empty() ? "aucune" : moveInfo(s.limit).name;
    m.items.push_back({s.name, cls, s.role + ". Limite : " + lim + ".", true, [this, id] { chooseStarter(id); },
                       [this, id] { preview_ = id; }});
  }
  m.onCancel = [this] { cancelSetup(); };
  if (group_) m.cancelable = false;  // à plusieurs : le groupe est déjà parti
  preview_ = content_.heroes[std::clamp(sel, 0, n - 1)];
  G.menus.push(m);
}

void Expedition::chooseStarter(const std::string& hero, int sel) {
  G.menus.clear();
  int n = std::min((int)content_.starters.size(), 3 + progress.up[U_STARTERS]);
  Menu m;
  m.title = "Votre créature";
  m.x = 4, m.y = 30, m.w = 152, m.rows = 6, m.sel = sel;
  for (int i = 0; i < n; i++) {
    std::string id = content_.starters[i];
    const Species& s = species(id);
    std::string lim = s.limit.empty() ? "aucune" : moveInfo(s.limit).name;
    m.items.push_back({s.name, typesName(s), "Type " + typesName(s) + ". Limite : " + lim + ".", true, [this, hero, id] { begin(hero, id); },
                       [this, id] { preview_ = id; }});
  }
  m.onCancel = [this, hero] {
    int i = int(std::find(content_.heroes.begin(), content_.heroes.end(), hero) - content_.heroes.begin());
    chooseHero(i);
  };
  preview_ = content_.starters[std::clamp(sel, 0, n - 1)];
  G.menus.push(m);
}

void Expedition::begin(const std::string& hero, const std::string& starter) {
  const Progress& P = progress;
  // Un joueur qui rejoint un groupe déjà loin part de la région du groupe, au niveau de ses créatures
  int lvl = procgen::regionLevel(region_) + 1 + P.up[U_LEVEL];
  G.menus.clear();
  G.sc.clear();
  G.team = {makeFighter(hero, lvl), makeFighter(starter, lvl)};
  G.items = {{"potion", 3 + 2 * P.up[U_BAG]}, {"lanterne", 4 + P.up[U_BAG]}, {"ether", 1}, {"plume", 1}};
  G.gold = 100 + 60 * P.up[U_GOLD] + 60 * (region_ - 1);
  G.flags.clear();
  screen_ = false;
  preview_.clear();
  progress.runs++;
  saveProgress();
  enterRegion();
  if (group_) {
    G.sc.say("Expédition à plusieurs — graine " + seedText_ + ". Chacun explore, combat et capture de son côté.");
    G.sc.say("Le gardien de chaque région ne s'affronte qu'ensemble : retrouvez-vous devant lui !");
  } else {
    G.sc.say("Expédition — graine " + seedText_ + ". Allez le plus loin possible, de région en région !");
    G.sc.say("Une défaite y met fin, mais les éclats gagnés restent : dépensez-les au Camp.");
  }
  G.sc.call([this] { G.saveGame(); });
}

// Place l'équipe à l'entrée de la région courante
void Expedition::enterRegion() {
  G.mode = Mode::Map;
  G.mapId = -1;
  G.changeMap(0, current_.startX, current_.startY, RIGHT);
  G.respawnMap = 0, G.respawnX = current_.startX, G.respawnY = current_.startY;
  G.bannerText = "Région " + std::to_string(region_) + " : " + current_.name;
  G.banner = 3.f;
  if (region_ > progress.record) {
    progress.record = region_;
    saveProgress();
  }
}

void Expedition::resume() {
  active_ = true;
  if (!G.loadGame()) {
    leave();
    menu();
    G.notice("Impossible de reprendre l'expédition.");
    return;
  }
  screen_ = false;
  G.menus.clear();
  G.mode = Mode::Map;
  G.bannerText = "Région " + std::to_string(region_) + " : " + current_.name;
  G.banner = 3.f;
}

// ---------------------------------------------------------------------------
// Expédition à plusieurs (online.hpp, coop.cpp)
// ---------------------------------------------------------------------------
void Expedition::startGroup(uint64_t seed, const std::string& text, int region) {
  if (active_) leave();
  group_ = true;
  loadProgress();
  uint64_t saved = 0;
  if (savedRegion(true, &saved) > 0 && saved == seed) {  // ce joueur reprend là où il en était
    active_ = true;
    if (G.loadGame()) {
      screen_ = false;
      G.menus.clear();
      G.mode = Mode::Map;
      G.bannerText = "Région " + std::to_string(region_) + " : " + current_.name;
      G.banner = 3.f;
      return;
    }
    leave();
    group_ = true;
  }
  screen_ = true;
  G.mode = Mode::Title;
  seedText_ = text.empty() ? std::to_string(seed) : text;
  generate(seed, std::max(1, region));  // le groupe est peut-être déjà plus loin : on part de sa région
  active_ = true;
  install();
  chooseHero();
}

void Expedition::knockedOut() {
  int lost = G.gold / 2;
  G.gold -= lost;
  G.healAll();
  G.changeMap(0, current_.startX, current_.startY, RIGHT);
  G.sc.say("Toute l'équipe est à terre… Vous vous réveillez au village, soignés.");
  if (lost > 0) G.sc.say("Dans la fuite, vous avez perdu " + std::to_string(lost) + " pièces d'or.");
  G.sc.say("À plusieurs, seule une défaite contre le gardien met fin à l'expédition.");
}

bool Expedition::beaten() const { return active_ && G.has(current_.bossFlag); }

std::string Expedition::worldHash(uint64_t seed, int regions) {
  procgen::Content c = procgen::generateBase(seed);
  uint64_t h = 1469598103934665603ull;
  auto mix = [&](const std::string& s) {
    for (unsigned char ch : s) h = (h ^ ch) * 1099511628211ull;
  };
  mix(c.moves.dump());
  mix(c.species.dump());
  for (auto& l : c.looks) mix(l.dump());
  for (int k = 1; k <= regions; k++) {
    procgen::Region r = procgen::generateRegion(seed, k, c);
    mix(r.map.dump());
    mix(r.events.dump());
  }
  char buf[20];
  std::snprintf(buf, sizeof buf, "%016llx", (unsigned long long)h);
  return buf;
}

void Expedition::quickStart(uint64_t seed, int hero, int starter) {
  start(seed, "");
  begin(content_.heroes.at((size_t)hero), content_.starters.at((size_t)starter));
}

void Expedition::camp(int sel) {
  G.menus.clear();
  Menu m;
  m.title = "Camp : " + std::to_string(progress.shards) + " éclats de brume";
  m.x = 8, m.y = 40, m.w = 230, m.rows = 7, m.sel = sel;
  for (int u = 0; u < N_UPGRADES; u++) {
    int lv = progress.up[u], cost = upgradeCost(u);
    bool max = lv >= UPGRADES[u].max;
    std::string help = std::string(UPGRADES[u].help) + " Niveau " + std::to_string(lv) + " sur " + std::to_string(UPGRADES[u].max) + ".";
    m.items.push_back({UPGRADES[u].name, max ? "au maximum" : std::to_string(cost) + " éclats", help, !max && progress.shards >= cost, [this, u] {
                         int c = upgradeCost(u);
                         if (progress.up[u] >= UPGRADES[u].max || progress.shards < c) return;
                         progress.shards -= c;
                         progress.up[u]++;
                         saveProgress();
                         camp(u);
                       }});
  }
  m.onCancel = [this] { menu(3); };
  G.menus.push(m);
}

// ---------------------------------------------------------------------------
// Pendant l'expédition
// ---------------------------------------------------------------------------
bool Expedition::atExit(int x, int y) const {
  return active_ && x >= current_.exitX && (y == current_.exitY || y == current_.exitY + 1);
}

void Expedition::nextRegion() {
  current_ = procgen::generateRegion(seed_, region_ + 1, content_);
  region_++;
  install();
  enterRegion();
  G.saveGame();
  G.notice("Expédition sauvegardée.");
}

void Expedition::onDefeat(bool gaveUp) {
  int trainers = 0;
  for (auto& f : G.flags) trainers += f.find("_dresseur") != std::string::npos;
  int gain = 5 + 10 * (region_ - 1) + 2 * trainers;
  bool newRecord = region_ >= progress.record;
  progress.record = std::max(progress.record, region_);
  progress.shards += gain;
  progress.total += gain;
  saveProgress();
  removeSave();
  G.menus.clear();
  G.healAll();
  // À plusieurs, seule la défaite contre le gardien (ou l'abandon) arrête l'expédition de ce joueur
  std::string where = " en région " + std::to_string(region_) + ".";
  G.sc.say(gaveUp   ? "Vous rebroussez chemin… L'expédition s'arrête" + where
           : group_ ? current_.bossName + " a vaincu le groupe… L'expédition s'achève" + where
                    : "Toute l'équipe est à terre… L'expédition s'achève" + where);
  G.sc.say("Région atteinte : " + std::to_string(region_) + (newRecord ? " (record !)" : ". Record : région " + std::to_string(progress.record) + "."));
  G.sc.say("Vous gagnez " + std::to_string(gain) + " éclats de brume (" + std::to_string(progress.shards) + " à dépenser au Camp).");
  G.sc.call([this] {
    bool group = group_;
    leave();
    if (group && G.online_) return G.online_->runEnded();  // retour au salon, toujours connecté
    G.titleMenu();
    menu(3);
  });
}

void Expedition::writeSave(std::ostream& f) const {
  f << "expedition " << seed_ << ' ' << region_ << '\n';
  f << "graine " << seedText_ << '\n';
}

bool Expedition::readSave(const std::string& key, const std::string& rest) {
  std::istringstream s(rest);
  if (key == "expedition") {
    uint64_t seed = 0;
    int k = 0;
    if (!(s >> seed >> k) || k < 1) return false;
    generate(seed, k);
    seedText_ = std::to_string(seed);
    active_ = true;
    install();
    return true;
  }
  if (key == "graine") {
    std::string t;
    std::getline(s >> std::ws, t);
    if (!t.empty()) seedText_ = t;
  }
  return true;
}

// ---------------------------------------------------------------------------
// Dessin des écrans de l'expédition (à la place de l'écran titre)
// ---------------------------------------------------------------------------
void Expedition::draw(Gfx& g) {
  float L = g.left(), W = (float)g.fullW;
  g.gradV(L, 0, W, SCREEN_H, rgb(0x10183a), rgb(0x2c3a6e));
  g.poly({{L, 170}, {40, 140}, {90, 160}, {150, 126}, {210, 150}, {270, 120}, {L + W, 146}, {L + W, 240}, {L, 240}}, rgb(0x1e2650));
  for (int i = 0; i < 6 * W / SCREEN_W; i++) g.ellipse(L + std::fmod(i * 70 + G.time * 14, W + 100) - 50, 150 + (i % 5) * 12, 90, 7, rgb(0xe6ebff, 22));
  bool choosing = active_ && !preview_.empty() && hasSpecies(preview_);
  if (choosing) drawFighterCard(g, *makeFighter(preview_, 5 + progress.up[U_LEVEL]), 160, 18, 156, 180, G.time);
  else {
    g.text(160, 10, "EXPÉDITION", GOLD, 1);
    if (G.menus.maxRight() <= 158) {
      g.window(164, 40, 150, 70);
      g.text(172, 46, "Record : région " + std::to_string(progress.record), WHITE);
      g.text(172, 59, "Expéditions : " + std::to_string(progress.runs), WHITE);
      g.text(172, 72, "Éclats : " + std::to_string(progress.shards), GOLD);
      g.text(172, 85, "Gagnés en tout : " + std::to_string(progress.total), MUTED);
    }
  }
  G.menus.draw(g, G.time);
  auto lines = Gfx::wrap(G.menus.help(), 284);
  if (!lines.empty() && !lines[0].empty()) {
    int h = 9 + 11 * (int)lines.size();
    g.window(14, 236 - h, 292, h);
    for (size_t i = 0; i < lines.size(); i++) g.text(160, 236 - h + 4 + i * 11, lines[i], WHITE, 1);
  }
}
