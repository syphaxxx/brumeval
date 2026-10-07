#include "mapedit.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>

#include "data.hpp"
#include "events.hpp"
#include "game.hpp"
#include "sprites.hpp"

static const Color WHITE = rgb(0xffffff), GOLD = rgb(0xffd34d), MUTED = rgb(0xaab3d8), CYAN = rgb(0x7fe0ff), RED = rgb(0xff8a7a),
                   VIOLET = rgb(0xd08aff), ORANGE = rgb(0xffa040);
static const int TOP = 12, BOTTOM = 220, VIEW_H = BOTTOM - TOP;

const std::vector<TileInfo>& tilePalette() {
  static const std::vector<TileInfo> v = {
      {'.', "Sol"},           {',', "Hautes herbes"},  {'=', "Chemin"},        {'T', "Arbre"},         {'F', "Fleurs"},  {'R', "Rocher"},
      {'~', "Eau"},           {'B', "Pont de bois"},   {'W', "Fontaine"},      {'#', "Paroi"},         {'m', "Montagne"}, {'k', "Entrée de grotte"},
      {'a', "Cendre"},        {'g', "Herbes sèches"},  {'l', "Lave"},          {'b', "Pont de pierre"}, {'c', "Sol de grotte"}, {'x', "Cristal"},
      {'d', "Arbre mort"},    {'i', "Glace"},          {'n', "Neige profonde"}, {'z', "Marais"}};
  return v;
}
static std::string tileName(char c) {
  for (auto& t : tilePalette())
    if (t.c == c) return t.name;
  return std::string(1, c);
}
static int tileIndex(char c) {
  auto& p = tilePalette();
  for (size_t i = 0; i < p.size(); i++)
    if (p[i].c == c) return (int)i;
  return 0;
}

static const char* THEMES_FR[] = {"Vallée", "Cendres", "Grotte", "Forêt", "Neige"};
static const char* AMBIANCES[] = {"", "brume", "cendres", "obscurite", "neige", "lucioles"};
static const char* AMBIANCES_FR[] = {"aucune", "Brume", "Cendres", "Obscurité", "Neige", "Lucioles"};
static const char* KINDS[] = {"maison", "soin", "boutique1", "boutique2", "auberge", "chapelle", "forge"};
static const char* KINDS_FR[] = {"Maison", "Soin", "Boutique", "Grande boutique", "Auberge", "Chapelle", "Forge"};
static const uint32_t ROOFS[] = {0xb8483e, 0x3f6fb0, 0x8a6fb8, 0x7a8a4a, 0x9a5a2a, 0x4a4a52, 0x6a5a3a, 0x5a6a9a, 0x7a6a8a, 0x8a3a2a};
static const char* DIRS_FR[] = {"haut", "bas", "gauche", "droite"};
static const uint32_t ZONE_COLORS[] = {0xff6a6a, 0x6aff9a, 0x6aa8ff, 0xffd34d, 0xff8ae0, 0x8affff};

// Couleur simplifiée d'une tuile (vue éloignée)
static uint32_t tileColor(char c, Theme th) {
  uint32_t ground = th == Theme::Cendres ? 0x6e625c : th == Theme::Grotte ? 0x4a4252 : th == Theme::Foret ? 0x3f7d45 : th == Theme::Neige ? 0xe8eef6 : 0x67b35d;
  switch (c) {
    case ',': return th == Theme::Foret ? 0x2f6236 : 0x4a9446;
    case '=': return th == Theme::Neige ? 0xc4cedc : (th == Theme::Vallee || th == Theme::Foret) ? 0xd9c08a : 0x8a7f78;
    case 'T': return th == Theme::Neige ? 0x2e5a46 : th == Theme::Foret ? 0x1f4a2a : 0x2e6b3a;
    case '~': return 0x3b7fc4;
    case 'B': return 0xa0703f;
    case 'R': return 0x8a8a92;
    case 'F': return 0xf29bb5;
    case 'W': return 0x5ea0dc;
    case '#': return th == Theme::Grotte ? 0x241e2e : th == Theme::Neige ? 0x7fa8c8 : 0x8a7a66;
    case 'g': return 0x7a6a50;
    case 'l': return 0xc8441a;
    case 'b': return 0x9a9090;
    case 'm': return 0x5a4a44;
    case 'k': return 0x120e18;
    case 'x': return 0x7fd6ff;
    case 'd': return 0x4a3a30;
    case 'i': return 0xa8d8f0;
    case 'n': return 0xd4e0f0;
    case 'z': return 0x4a6a4a;
    default: return ground;
  }
}

// Identifiant de fichier à partir d'un nom : minuscules, sans accents ni espaces
std::string makeSlug(const std::string& name) {
  static const std::pair<const char*, char> ACC[] = {{"à", 'a'}, {"â", 'a'}, {"ä", 'a'}, {"é", 'e'}, {"è", 'e'}, {"ê", 'e'}, {"ë", 'e'},
                                                     {"î", 'i'}, {"ï", 'i'}, {"ô", 'o'}, {"ö", 'o'}, {"ù", 'u'}, {"û", 'u'}, {"ü", 'u'},
                                                     {"ç", 'c'}, {"É", 'e'}, {"È", 'e'}, {"À", 'a'}, {"Ç", 'c'}, {"œ", 'o'}};
  std::string s = name, out;
  for (auto& [a, b] : ACC)
    for (size_t p; (p = s.find(a)) != std::string::npos;) s.replace(p, std::strlen(a), std::string(1, b));
  for (char c : s) {
    if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    if (ok) out += c;
    else if (!out.empty() && out.back() != '_') out += '_';
  }
  while (!out.empty() && out.back() == '_') out.pop_back();
  return out.empty() ? "carte" : out;
}

static MenuItem numItem(const std::string& label, const std::string& help, std::function<double()> get, std::function<void(double)> set,
                        double step, double mn, double mx) {
  MenuItem it{label, "", help, true};
  it.rightFn = [get] {
    double v = get();
    char b[24];
    if (std::fabs(v - std::round(v)) < 1e-6) std::snprintf(b, sizeof b, "%d", (int)std::lround(v));
    else std::snprintf(b, sizeof b, "%.2f", v);
    std::string s = b;
    for (auto& c : s)
      if (c == '.') c = ',';
    return "< " + s + " >";
  };
  it.adjust = [get, set, step, mn, mx](int d) { set(std::clamp(std::round((get() + d * step) * 1e6) / 1e6, mn, mx)); };
  return it;
}
static MenuItem cycleItem(const std::string& label, const std::string& help, std::function<int()> get, std::function<void(int)> set, int n,
                          std::function<std::string(int)> name) {
  MenuItem it{label, "", help, true};
  it.rightFn = [get, name] { return "< " + name(get()) + " >"; };
  it.adjust = [get, set, n](int d) { set(((get() + d) % n + n) % n); };
  return it;
}

// ---------------------------------------------------------------------------
MapEditor::MapEditor(Game& g) : G(g) {
  for (auto& m : maps()) saved_[m.id] = mapToJson(m);
}

MapDef& MapEditor::M() { return maps()[map_]; }

void MapEditor::open(const std::string& mapId) {
  G.menus.clear();
  pending_ = nullptr;
  int i = mapId.empty() ? map_ : mapIndex(mapId);
  select(std::clamp(i < 0 ? 0 : i, 0, (int)maps().size() - 1));
}

void MapEditor::select(int index) {
  bool changed = index != map_;
  map_ = index;
  if (changed) undo_.clear(), redo_.clear();
  const MapDef& m = M();
  cx_ = std::clamp(cx_, 0, m.w() - 1);
  cy_ = std::clamp(cy_, 0, m.h() - 1);
  if (changed) {
    cx_ = m.w() / 2, cy_ = m.h() / 2;
    for (auto& o : maps())
      for (auto& w : o.warps)
        if (w.map == m.id) cx_ = w.tx, cy_ = w.ty;
  }
  camX_ = cx_ * zoom_ - 160.f, camY_ = cy_ * zoom_ - VIEW_H / 2.f;
  follow();
}

void MapEditor::setCursor(int x, int y) {
  cx_ = std::clamp(x, 0, M().w() - 1);
  cy_ = std::clamp(y, 0, M().h() - 1);
  follow();
}

void MapEditor::follow() {
  float px = cx_ * zoom_, py = cy_ * zoom_;
  float mx = 2.f * zoom_, L = G.g.left(), R = G.g.right();
  if (px - camX_ < L + mx) camX_ = px - mx - L;
  if (px - camX_ > R - mx - zoom_) camX_ = px - R + mx + zoom_;
  if (py - camY_ < mx) camY_ = py - mx;
  if (py - camY_ > VIEW_H - mx - zoom_) camY_ = py - VIEW_H + mx + zoom_;
  camX_ = std::clamp(camX_, -L, std::max(-L, float(M().w() * zoom_) - R));
  camY_ = std::clamp(camY_, 0.f, std::max(0.f, float(M().h() * zoom_ - VIEW_H)));
}

void MapEditor::say(const std::string& m) {
  message_ = m;
  messageT_ = G.time;
}

void MapEditor::begin() {
  undo_.push_back(M());
  if (undo_.size() > 100) undo_.erase(undo_.begin());
  redo_.clear();
}
void MapEditor::undo() {
  if (undo_.empty()) return say("Rien à annuler.");
  redo_.push_back(M());
  M() = undo_.back();
  undo_.pop_back();
  setCursor(cx_, cy_);
  say("Annulé.");
}
void MapEditor::redo() {
  if (redo_.empty()) return say("Rien à rétablir.");
  undo_.push_back(M());
  M() = redo_.back();
  redo_.pop_back();
  setCursor(cx_, cy_);
  say("Rétabli.");
}

bool MapEditor::dirty(int i) const {
  const MapDef& m = maps()[i];
  auto it = saved_.find(m.id);
  return it == saved_.end() || mapToJson(m) != it->second;
}

std::vector<std::string> MapEditor::problems() const {
  std::vector<std::string> out;
  std::string a = maps()[map_].name + " : ", b = maps()[map_].name + ", ";
  auto collect = [&](const std::vector<std::string>& list) {
    for (auto& p : list)
      if (p.rfind(a, 0) == 0 || p.rfind(b, 0) == 0) out.push_back(p.substr(a.size()));
  };
  collect(checkMaps());
  collect(checkEvents());
  return out;
}

void MapEditor::save() {
  try {
    saveMap(M());
  } catch (const std::exception& e) {
    return say(std::string("Erreur : ") + e.what());
  }
  saved_[M().id] = mapToJson(M());
  auto p = problems();
  say("Enregistré : data/cartes/" + M().id + ".json" + (p.empty() ? "" : " (" + std::to_string(p.size()) + " problème(s))"));
}

void MapEditor::reload() {
  auto it = saved_.find(M().id);
  if (it == saved_.end()) return say("Cette carte n'a jamais été enregistrée.");
  begin();
  M() = mapFromJson(it->second);
  setCursor(cx_, cy_);
  say("Carte rechargée depuis le fichier.");
}

// ---------------------------------------------------------------------------
// Peinture
// ---------------------------------------------------------------------------
void MapEditor::paintAt(int x, int y) {
  MapDef& m = M();
  if (x < 0 || y < 0 || x >= m.w() || y >= m.h()) return;
  m.rows[y][x] = tile_;
}

void MapEditor::fill(int x, int y) {
  MapDef& m = M();
  char from = m.rows[y][x];
  if (from == tile_) return;
  std::deque<std::pair<int, int>> q{{x, y}};
  m.rows[y][x] = tile_;
  while (!q.empty()) {
    auto [px, py] = q.front();
    q.pop_front();
    for (auto [dx, dy] : {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
      int nx = px + dx, ny = py + dy;
      if (nx < 0 || ny < 0 || nx >= m.w() || ny >= m.h() || m.rows[ny][nx] != from) continue;
      m.rows[ny][nx] = tile_;
      q.push_back({nx, ny});
    }
  }
}

void MapEditor::askPosition(const std::string& text, std::function<void(int, int)> done) {
  G.menus.clear();
  pending_ = std::move(done);
  pendingText_ = text;
}

void MapEditor::apply() {
  if (pending_) {
    auto f = pending_;
    pending_ = nullptr;
    pendingText_.clear();
    f(cx_, cy_);
    anchorX_ = anchorY_ = -1;
    return;
  }
  if (layer_ == Layer::Tiles) {
    if (tool_ == Tool::Brush) {
      begin();
      paintAt(cx_, cy_);
    } else if (tool_ == Tool::Fill) {
      begin();
      fill(cx_, cy_);
    } else {
      int ax = cx_, ay = cy_;
      anchorX_ = ax, anchorY_ = ay;
      askPosition("Rectangle : placez le coin opposé puis Entrée (ou clic)", [this, ax, ay](int x, int y) {
        begin();
        for (int yy = std::min(ay, y); yy <= std::max(ay, y); yy++)
          for (int xx = std::min(ax, x); xx <= std::max(ax, x); xx++) paintAt(xx, yy);
      });
    }
    return;
  }
  act(cx_, cy_);
}

// ---------------------------------------------------------------------------
// Objets et zones sous le curseur
// ---------------------------------------------------------------------------
void MapEditor::act(int x, int y) {
  MapDef& m = M();
  if (layer_ == Layer::Objects) {
    for (size_t i = 0; i < m.npcs.size(); i++)
      if (m.npcs[i].x == x && m.npcs[i].y == y) return editNpc((int)i);
    for (size_t i = 0; i < m.chests.size(); i++)
      if (m.chests[i].x == x && m.chests[i].y == y) return editChest((int)i);
    for (size_t i = 0; i < m.signs.size(); i++)
      if (m.signs[i].x == x && m.signs[i].y == y) return editSign((int)i);
    for (size_t i = 0; i < m.bosses.size(); i++)
      if (x >= m.bosses[i].x && x <= m.bosses[i].x + 1 && y >= m.bosses[i].y && y <= m.bosses[i].y + 1) return editBoss((int)i);
    for (size_t i = 0; i < m.warps.size(); i++)
      if (m.warps[i].x == x && m.warps[i].y == y) return editWarp((int)i);
    for (size_t i = 0; i < m.buildings.size(); i++) {
      auto& b = m.buildings[i];
      if (x >= b.x && y >= b.y && x < b.x + b.w && y < b.y + b.h) return editBuilding((int)i);
    }
    return menuAdd(x, y);
  }
  // Calque des zones : celles qui contiennent la case
  std::vector<std::pair<bool, int>> hits;
  for (size_t i = 0; i < m.triggers.size(); i++) {
    auto& t = m.triggers[i];
    if (x >= t.x && y >= t.y && x < t.x + t.w && y < t.y + t.h) hits.push_back({true, (int)i});
  }
  for (size_t i = 0; i < m.zones.size(); i++) {
    auto& z = m.zones[i];
    if (x >= z.x && y >= z.y && x < z.x + z.w && y < z.y + z.h) hits.push_back({false, (int)i});
  }
  if (hits.size() == 1) return hits[0].first ? editTrigger(hits[0].second) : editZone(hits[0].second);
  if (hits.empty()) return menuAdd(x, y);
  Menu mm;
  mm.title = "Zones ici";
  mm.x = 4, mm.y = 16, mm.w = 200, mm.rows = 8;
  for (auto [trig, i] : hits) {
    if (trig) mm.items.push_back({"Déclencheur : " + utf8Prefix(m.triggers[i].event, 14), "", "", true, [this, i = i] { editTrigger(i); }});
    else
      mm.items.push_back({"Zone n°" + std::to_string(i + 1) + " (N." + std::to_string(m.zones[i].lo) + "-" + std::to_string(m.zones[i].hi) + ")",
                          "", "La première zone de la liste qui contient le joueur est utilisée.", true, [this, i = i] { editZone(i); }});
  }
  mm.items.push_back({"Nouvelle zone ici…", "", "", true, [this, x, y] {
                        G.menus.clear();
                        menuAdd(x, y);
                      }});
  G.menus.push(mm);
}

void MapEditor::menuAdd(int x, int y) {
  Menu m;
  m.title = "Ajouter en (" + std::to_string(x) + ", " + std::to_string(y) + ")";
  m.x = 4, m.y = 16, m.w = 176, m.rows = 7;
  if (layer_ == Layer::Objects) {
    m.items.push_back({"Habitant", "", "Un personnage qui parle ou lance un événement.", true, [this, x, y] {
                         begin();
                         Npc n{x, y, std::max(0, lookIndex("villageois")), 1, "", {"Bonjour !"}, ""};
                         M().npcs.push_back(n);
                         editNpc((int)M().npcs.size() - 1);
                       }});
    m.items.push_back({"Coffre", "", "", true, [this, x, y] {
                         begin();
                         M().chests.push_back({x, y, allItems().empty() ? "" : allItems()[0].id, 1});
                         editChest((int)M().chests.size() - 1);
                       }});
    m.items.push_back({"Panneau", "", "", true, [this, x, y] {
                         begin();
                         M().signs.push_back({x, y, "Panneau"});
                         editSign((int)M().signs.size() - 1);
                       }});
    m.items.push_back({"Passage vers une carte", "", "Le joueur change de carte en marchant sur cette case.", true, [this, x, y] {
                         begin();
                         Warp w{x, y, M().id, x, y, 1, Json(), ""};
                         M().warps.push_back(w);
                         editWarp((int)M().warps.size() - 1);
                       }});
    m.items.push_back({"Boss", "", "Occupe 2x2 cases ; disparaît quand son drapeau est posé.", true, [this, x, y] {
                         begin();
                         std::string sp = "sylvarque";
                         for (auto& s : allSpecies())
                           if (!s.human && s.limit.empty()) {
                             sp = s.id;
                             break;
                           }
                         BossSpot b{x, y, sp, "boss_" + M().id + "_" + std::to_string(M().bosses.size() + 1), sp};
                         M().bosses.push_back(b);
                         editBoss((int)M().bosses.size() - 1);
                       }});
    m.items.push_back({"Bâtiment", "", "La case cliquée devient le coin haut-gauche.", true, [this, x, y] {
                         begin();
                         Building b{x, y, 5, 4, "maison", "Maison", ROOFS[0], "maison"};
                         M().buildings.push_back(b);
                         editBuilding((int)M().buildings.size() - 1);
                       }});
  } else {
    m.items.push_back({"Zone de rencontres", "", "Créatures sauvages dans les hautes herbes, la neige profonde, le marais…", true,
                       [this, x, y] {
                         begin();
                         std::vector<std::string> pool;
                         for (auto& s : allSpecies())
                           if (!s.human && !s.limit.empty() && pool.empty()) pool.push_back(s.id);
                         M().zones.push_back({x, y, 6, 6, 2, 4, 3, pool});
                         editZone((int)M().zones.size() - 1);
                       }});
    m.items.push_back({"Zone déclencheuse", "", "Lance un événement quand le joueur y entre.", true, [this, x, y] {
                         begin();
                         std::string ev = events().empty() ? "" : events().begin().key();
                         M().triggers.push_back({x, y, 1, 1, ev, ""});
                         editTrigger((int)M().triggers.size() - 1);
                       }});
  }
  G.menus.push(m);
}

void MapEditor::pickEvent(const std::string& current, std::function<void(const std::string&)> done) {
  Menu m;
  m.title = "Événement";
  m.x = 4, m.y = 16, m.w = 176, m.rows = 13;
  m.items.push_back({"(aucun)", "", "", true, [this, done] {
                       G.menus.pop();
                       done("");
                     }});
  std::vector<std::string> ids;
  for (auto& [id, ev] : events().items()) ids.push_back(id);
  std::sort(ids.begin(), ids.end());
  for (auto& id : ids) {
    // Aide : le premier message de l'événement
    std::string first;
    std::function<void(const Json&)> scan = [&](const Json& list) {
      for (auto& a : list) {
        if (!first.empty()) return;
        if (jget<std::string>(a, "action", "") == "dire") first = jget<std::string>(a, "texte", "");
      }
    };
    const Json& ev = *findEvent(id);
    if (ev.contains("pages"))
      for (auto& p : ev["pages"]) scan(p.value("actions", Json::array()));
    else scan(ev.value("actions", Json::array()));
    if (id == current) m.sel = (int)m.items.size();
    m.items.push_back({id, "", first.empty() ? "(pas de message)" : first, true, [this, done, id] {
                         G.menus.pop();
                         done(id);
                       }});
  }
  G.menus.push(m);
}

// --- Habitants
void MapEditor::editNpc(int i, int sel) {
  G.menus.clear();
  auto N = [this, i]() -> Npc& { return M().npcs[i]; };
  Menu m;
  m.title = "Habitant (" + std::to_string(N().x) + ", " + std::to_string(N().y) + ")";
  m.x = 4, m.y = 16, m.w = 176, m.rows = 12;
  m.sel = sel;
  int nl = (int)allLooks().size();
  m.items.push_back(cycleItem("Apparence", "", [N] { return N().look; },
                              [this, N](int v) {
                                begin();
                                N().look = v;
                              },
                              nl, [](int v) { return look(v).name; }));
  m.items.push_back(cycleItem("Regard", "Direction du regard (et de la vue d'un dresseur).", [N] { return N().dir; },
                              [this, N](int v) {
                                begin();
                                N().dir = v;
                              },
                              4, [](int v) { return std::string(DIRS_FR[v]); }));
  MenuItem dl{"Dialogue", "", "Phrases dites quand on parle à l'habitant (s'il n'a pas d'événement).", true, [this, i] { editLines(i); }};
  dl.rightFn = [N] { return std::to_string(N().lines.size()) + " phrase(s) >"; };
  m.items.push_back(dl);
  MenuItem ev{"Événement", "", "Remplace le dialogue : à créer dans l'éditeur d'histoire.", true};
  ev.rightFn = [N] { return N().event.empty() ? std::string("aucun") : utf8Prefix(N().event, 12); };
  ev.act = [this, N, i] {
    pickEvent(N().event, [this, N, i](const std::string& id) {
      begin();
      N().event = id;
      editNpc(i, 3);
    });
  };
  m.items.push_back(ev);
  auto textField = [&](const std::string& label, const std::string& title, std::function<std::string&()> field, int maxc, int row) {
    MenuItem t{label, "", "Entrée : modifier (vide = aucun).", true};
    t.rightFn = [field] { return field().empty() ? std::string("—") : utf8Prefix(field(), 12); };
    t.act = [this, title, field, maxc, i, row] {
      G.editText(title, field(), maxc, [this, field, i, row](const std::string& v) {
        begin();
        field() = v;
        editNpc(i, row);
      });
    };
    return t;
  };
  m.items.push_back(textField("Caché si", "Drapeau qui fait disparaître l'habitant", [N]() -> std::string& { return N().hideIf; }, 30, 4));
  m.items.push_back(numItem("Vue (dresseur)", "Distance à laquelle il repère le joueur et lance son événement (0 : jamais).",
                            [N] { return (double)N().sight; },
                            [this, N](double v) {
                              begin();
                              N().sight = (int)v;
                            },
                            1, 0, 10));
  m.items.push_back(textField("Vue jusqu'à", "Drapeau qui arrête la vue (ex. quand il est battu)", [N]() -> std::string& { return N().sightUntil; },
                              30, 6));
  m.items.push_back({"Déplacer", "", "", true, [this, N] {
                       askPosition("Nouvelle place de l'habitant : Entrée ou clic", [this, N](int x, int y) {
                         begin();
                         N().x = x, N().y = y;
                       });
                     }});
  m.items.push_back({"Supprimer", "", "", true, [this, i] {
                       begin();
                       M().npcs.erase(M().npcs.begin() + i);
                       G.menus.clear();
                     }});
  G.menus.push(m);
}

void MapEditor::editLines(int i, int sel) {
  G.menus.clear();
  Menu m;
  m.title = "Dialogue";
  m.x = 4, m.y = 16, m.w = 236, m.rows = 10;
  m.sel = sel;
  auto& lines = M().npcs[i].lines;
  for (size_t k = 0; k < lines.size(); k++)
    m.items.push_back({utf8Prefix(lines[k], 36), "", lines[k] + "  (Entrée : modifier ; texte vide = supprimer)", true, [this, i, k] {
                         G.editText("Phrase " + std::to_string(k + 1), M().npcs[i].lines[k], 160, [this, i, k](const std::string& v) {
                           begin();
                           auto& L = M().npcs[i].lines;
                           if (v.empty()) L.erase(L.begin() + k);
                           else L[k] = v;
                           editLines(i, (int)k);
                         });
                       }});
  m.items.push_back({"Ajouter une phrase", "", "", true, [this, i] {
                       G.editText("Nouvelle phrase", "", 160, [this, i](const std::string& v) {
                         if (!v.empty()) {
                           begin();
                           M().npcs[i].lines.push_back(v);
                         }
                         editLines(i, (int)M().npcs[i].lines.size());
                       });
                     }});
  m.onCancel = [this, i] { editNpc(i, 2); };
  G.menus.push(m);
}

// --- Coffres
void MapEditor::editChest(int i, int sel) {
  G.menus.clear();
  auto C = [this, i]() -> Chest& { return M().chests[i]; };
  Menu m;
  m.title = "Coffre (" + std::to_string(C().x) + ", " + std::to_string(C().y) + ")";
  m.x = 4, m.y = 16, m.w = 176, m.rows = 6;
  m.sel = sel;
  int n = (int)allItems().size() + 1;
  m.items.push_back(cycleItem("Contenu", "", [C] {
    if (C().item.empty()) return 0;
    for (size_t k = 0; k < allItems().size(); k++)
      if (allItems()[k].id == C().item) return (int)k + 1;
    return 0;
  }, [this, C](int v) {
    begin();
    bool wasGold = C().item.empty();
    C().item = v == 0 ? "" : allItems()[v - 1].id;
    if (C().item.empty() && !wasGold) C().qty = 100;
    if (!C().item.empty() && wasGold) C().qty = 1;
  }, n, [](int v) { return v == 0 ? std::string("Or") : allItems()[v - 1].name; }));
  m.items.push_back(numItem("Quantité", "Nombre d'objets, ou pièces d'or.", [C] { return (double)C().qty; },
                            [this, C](double v) {
                              begin();
                              C().qty = (int)v;
                            },
                            1, 1, 99999));
  m.items.push_back({"Déplacer", "", "", true, [this, C] {
                       askPosition("Nouvelle place du coffre : Entrée ou clic", [this, C](int x, int y) {
                         begin();
                         C().x = x, C().y = y;
                       });
                     }});
  m.items.push_back({"Supprimer", "", "Attention : les sauvegardes retiennent les coffres ouverts par leur position.", true, [this, i] {
                       begin();
                       M().chests.erase(M().chests.begin() + i);
                       G.menus.clear();
                     }});
  G.menus.push(m);
}

// --- Panneaux
void MapEditor::editSign(int i) {
  G.menus.clear();
  Menu m;
  m.title = "Panneau";
  m.x = 4, m.y = 16, m.w = 176, m.rows = 4;
  MenuItem t{"Texte", "", "", true};
  t.rightFn = [this, i] { return utf8Prefix(M().signs[i].text, 12) + "…"; };
  t.act = [this, i] {
    G.editText("Texte du panneau", M().signs[i].text, 160, [this, i](const std::string& v) {
      begin();
      M().signs[i].text = v.empty() ? "…" : v;
      editSign(i);
    });
  };
  m.items.push_back(t);
  m.items.push_back({"Déplacer", "", "", true, [this, i] {
                       askPosition("Nouvelle place du panneau : Entrée ou clic", [this, i](int x, int y) {
                         begin();
                         M().signs[i].x = x, M().signs[i].y = y;
                       });
                     }});
  m.items.push_back({"Supprimer", "", "", true, [this, i] {
                       begin();
                       M().signs.erase(M().signs.begin() + i);
                       G.menus.clear();
                     }});
  G.menus.push(m);
}

// --- Passages
void MapEditor::editWarp(int i, int sel) {
  G.menus.clear();
  auto W = [this, i]() -> Warp& { return M().warps[i]; };
  Menu m;
  m.title = "Passage (" + std::to_string(W().x) + ", " + std::to_string(W().y) + ")";
  m.x = 4, m.y = 16, m.w = 196, m.rows = 11;
  m.sel = sel;
  int nm = (int)maps().size();
  m.items.push_back(cycleItem("Vers", "Carte d'arrivée.", [W] { return std::max(0, mapIndex(W().map)); },
                              [this, W](int v) {
                                begin();
                                W().map = maps()[v].id;
                              },
                              nm, [](int v) { return utf8Prefix(maps()[v].name, 14); }));
  auto dest = [W]() -> const MapDef& { return maps()[std::max(0, mapIndex(W().map))]; };
  m.items.push_back(numItem("Arrivée x", "", [W] { return (double)W().tx; },
                            [this, W, dest](double v) {
                              begin();
                              W().tx = std::min((int)v, dest().w() - 1);
                            },
                            1, 0, 300));
  m.items.push_back(numItem("Arrivée y", "", [W] { return (double)W().ty; },
                            [this, W, dest](double v) {
                              begin();
                              W().ty = std::min((int)v, dest().h() - 1);
                            },
                            1, 0, 300));
  m.items.push_back(cycleItem("Regard à l'arrivée", "", [W] { return W().dir; },
                              [this, W](int v) {
                                begin();
                                W().dir = v;
                              },
                              4, [](int v) { return std::string(DIRS_FR[v]); }));
  m.items.push_back({"Choisir l'arrivée sur la carte", "", "Ouvre la carte d'arrivée : placez le curseur puis Entrée.", true, [this, i] {
                       int from = map_;
                       Warp w = M().warps[i];
                       int target = std::max(0, mapIndex(w.map));
                       select(target);
                       setCursor(w.tx, w.ty);
                       askPosition("Arrivée du passage : Entrée ou clic (Échap : annuler)", [this, from, i](int x, int y) {
                         select(from);
                         begin();
                         M().warps[i].tx = x, M().warps[i].ty = y;
                         setCursor(M().warps[i].x, M().warps[i].y);
                         editWarp(i, 4);
                       });
                     }});
  MenuItem flag{"Fermé sans le drapeau", "", "Le passage reste fermé tant que ce drapeau n'est pas posé (vide : toujours ouvert).", true};
  flag.rightFn = [W] { return W().condition.is_object() && W().condition.contains("drapeau") ? utf8Prefix(W().condition["drapeau"].get<std::string>(), 10) : std::string("—"); };
  flag.act = [this, W, i] {
    std::string cur = W().condition.is_object() ? jget<std::string>(W().condition, "drapeau", "") : "";
    G.editText("Drapeau nécessaire pour passer", cur, 30, [this, W, i](const std::string& v) {
      begin();
      if (!W().condition.is_object()) W().condition = Json::object();
      if (v.empty()) W().condition.erase("drapeau");
      else W().condition["drapeau"] = v;
      if (W().condition.empty()) W().condition = Json();
      editWarp(i, 5);
    });
  };
  m.items.push_back(flag);
  int ni = (int)allItems().size() + 1;
  m.items.push_back(cycleItem("Fermé sans l'objet", "Objet à posséder pour passer (une clé…).",
                              [W] {
                                if (!W().condition.is_object() || !W().condition.contains("objet")) return 0;
                                std::string id = W().condition["objet"].get<std::string>();
                                for (size_t k = 0; k < allItems().size(); k++)
                                  if (allItems()[k].id == id) return (int)k + 1;
                                return 0;
                              },
                              [this, W](int v) {
                                begin();
                                if (!W().condition.is_object()) W().condition = Json::object();
                                if (v == 0) W().condition.erase("objet");
                                else W().condition["objet"] = allItems()[v - 1].id;
                                if (W().condition.empty()) W().condition = Json();
                              },
                              ni, [](int v) { return v == 0 ? std::string("—") : utf8Prefix(allItems()[v - 1].name, 12); }));
  MenuItem msg{"Message si fermé", "", "", true};
  msg.rightFn = [W] { return W().message.empty() ? std::string("—") : utf8Prefix(W().message, 10) + "…"; };
  msg.act = [this, W, i] {
    G.editText("Message quand le passage est fermé", W().message, 160, [this, W, i](const std::string& v) {
      begin();
      W().message = v;
      editWarp(i, 7);
    });
  };
  m.items.push_back(msg);
  m.items.push_back({"Déplacer", "", "", true, [this, W] {
                       askPosition("Nouvelle place du passage : Entrée ou clic", [this, W](int x, int y) {
                         begin();
                         W().x = x, W().y = y;
                       });
                     }});
  m.items.push_back({"Supprimer", "", "", true, [this, i] {
                       begin();
                       M().warps.erase(M().warps.begin() + i);
                       G.menus.clear();
                     }});
  G.menus.push(m);
}

// --- Boss
void MapEditor::editBoss(int i, int sel) {
  G.menus.clear();
  auto B = [this, i]() -> BossSpot& { return M().bosses[i]; };
  std::vector<std::string> ids;
  for (auto& s : allSpecies())
    if (!s.human) ids.push_back(s.id);
  Menu m;
  m.title = "Boss (" + std::to_string(B().x) + ", " + std::to_string(B().y) + ")";
  m.x = 4, m.y = 16, m.w = 176, m.rows = 7;
  m.sel = sel;
  m.items.push_back(cycleItem("Apparence", "Créature affichée sur la carte (le combat est décrit par l'événement).",
                              [B, ids] {
                                auto it = std::find(ids.begin(), ids.end(), B().id);
                                return it == ids.end() ? 0 : int(it - ids.begin());
                              },
                              [this, B, ids](int v) {
                                begin();
                                B().id = ids[v];
                              },
                              (int)ids.size(), [ids](int v) { return species(ids[v]).name; }));
  MenuItem fl{"Drapeau (vaincu)", "", "Posé par l'événement après la victoire : le boss disparaît alors de la carte.", true};
  fl.rightFn = [B] { return utf8Prefix(B().flag, 12); };
  fl.act = [this, B, i] {
    G.editText("Drapeau du boss vaincu", B().flag, 30, [this, B, i](const std::string& v) {
      if (!v.empty()) {
        begin();
        B().flag = v;
      }
      editBoss(i, 1);
    });
  };
  m.items.push_back(fl);
  MenuItem ev{"Événement", "", "Dialogue et combat du boss (éditeur d'histoire).", true};
  ev.rightFn = [B] { return B().event.empty() ? std::string("aucun") : utf8Prefix(B().event, 12); };
  ev.act = [this, B, i] {
    pickEvent(B().event, [this, B, i](const std::string& id) {
      begin();
      B().event = id;
      editBoss(i, 2);
    });
  };
  m.items.push_back(ev);
  m.items.push_back({"Déplacer", "", "", true, [this, B] {
                       askPosition("Nouvelle place du boss (coin haut-gauche) : Entrée ou clic", [this, B](int x, int y) {
                         begin();
                         B().x = x, B().y = y;
                       });
                     }});
  m.items.push_back({"Supprimer", "", "", true, [this, i] {
                       begin();
                       M().bosses.erase(M().bosses.begin() + i);
                       G.menus.clear();
                     }});
  G.menus.push(m);
}

// --- Bâtiments
void MapEditor::editBuilding(int i, int sel) {
  G.menus.clear();
  auto Bd = [this, i]() -> Building& { return M().buildings[i]; };
  Menu m;
  m.title = "Bâtiment";
  m.x = 4, m.y = 16, m.w = 196, m.rows = 9;
  m.sel = sel;
  int nk = (int)(sizeof KINDS / sizeof *KINDS);
  m.items.push_back(cycleItem("Genre", "Apparence (enseigne) et événement par défaut de la porte.",
                              [Bd, nk] {
                                for (int k = 0; k < nk; k++)
                                  if (Bd().kind == KINDS[k]) return k;
                                return 0;
                              },
                              [this, Bd](int v) {
                                begin();
                                bool same = Bd().event == Bd().kind;
                                Bd().kind = KINDS[v];
                                if (same) Bd().event = Bd().kind;
                              },
                              nk, [](int v) { return std::string(KINDS_FR[v]); }));
  MenuItem nm{"Nom", "", "", true};
  nm.rightFn = [Bd] { return utf8Prefix(Bd().name, 12); };
  nm.act = [this, Bd, i] {
    G.editText("Nom du bâtiment", Bd().name, 30, [this, Bd, i](const std::string& v) {
      begin();
      Bd().name = v;
      editBuilding(i, 1);
    });
  };
  m.items.push_back(nm);
  m.items.push_back(numItem("Largeur", "La porte est au milieu du bas.", [Bd] { return (double)Bd().w; },
                            [this, Bd](double v) {
                              begin();
                              Bd().w = (int)v;
                            },
                            1, 3, 12));
  m.items.push_back(numItem("Hauteur", "", [Bd] { return (double)Bd().h; },
                            [this, Bd](double v) {
                              begin();
                              Bd().h = (int)v;
                            },
                            1, 3, 10));
  int nr = (int)(sizeof ROOFS / sizeof *ROOFS);
  m.items.push_back(cycleItem("Toit", "",
                              [Bd, nr] {
                                for (int k = 0; k < nr; k++)
                                  if (Bd().roof == ROOFS[k]) return k;
                                return 0;
                              },
                              [this, Bd](int v) {
                                begin();
                                Bd().roof = ROOFS[v];
                              },
                              nr, [](int v) { return "couleur " + std::to_string(v + 1); }));
  MenuItem ev{"Événement de la porte", "", "Par défaut : celui du genre (soin, boutique…).", true};
  ev.rightFn = [Bd] { return utf8Prefix(Bd().event, 12); };
  ev.act = [this, Bd, i] {
    pickEvent(Bd().event, [this, Bd, i](const std::string& id) {
      begin();
      Bd().event = id.empty() ? Bd().kind : id;
      editBuilding(i, 5);
    });
  };
  m.items.push_back(ev);
  m.items.push_back({"Déplacer", "", "", true, [this, Bd] {
                       askPosition("Nouvelle place du bâtiment (coin haut-gauche) : Entrée ou clic", [this, Bd](int x, int y) {
                         begin();
                         Bd().x = x, Bd().y = y;
                       });
                     }});
  m.items.push_back({"Supprimer", "", "", true, [this, i] {
                       begin();
                       M().buildings.erase(M().buildings.begin() + i);
                       G.menus.clear();
                     }});
  G.menus.push(m);
}

// --- Zones de rencontres
void MapEditor::editZone(int i, int sel) {
  G.menus.clear();
  auto Z = [this, i]() -> Zone& { return M().zones[i]; };
  Menu m;
  m.title = "Zone de rencontres n°" + std::to_string(i + 1);
  m.x = 4, m.y = 16, m.w = 196, m.rows = 11;
  m.sel = sel;
  m.items.push_back(numItem("Niveau minimum", "", [Z] { return (double)Z().lo; },
                            [this, Z](double v) {
                              begin();
                              Z().lo = (int)v;
                              Z().hi = std::max(Z().hi, Z().lo);
                            },
                            1, 1, 100));
  m.items.push_back(numItem("Niveau maximum", "", [Z] { return (double)Z().hi; },
                            [this, Z](double v) {
                              begin();
                              Z().hi = (int)v;
                              Z().lo = std::min(Z().hi, Z().lo);
                            },
                            1, 1, 100));
  m.items.push_back(numItem("Ennemis au plus", "", [Z] { return (double)Z().maxN; },
                            [this, Z](double v) {
                              begin();
                              Z().maxN = (int)v;
                            },
                            1, 1, 3));
  MenuItem cr{"Créatures", "", "Créatures possibles (une même créature peut apparaître plusieurs fois pour être plus fréquente).", true};
  cr.rightFn = [Z] { return std::to_string(Z().pool.size()) + " >"; };
  cr.act = [this, Z, i] {
    Menu c;
    c.title = "Créatures de la zone";
    c.x = 30, c.y = 20, c.w = 196, c.rows = 12;
    for (auto& s : allSpecies()) {
      if (s.human || s.limit.empty()) continue;
      std::string id = s.id;
      MenuItem t{s.name, "", "Gauche/droite : nombre de places dans la liste (plus = plus fréquent).", true};
      t.rightFn = [Z, id] { return "< " + std::to_string(std::count(Z().pool.begin(), Z().pool.end(), id)) + " >"; };
      t.adjust = [this, Z, id](int d) {
        begin();
        auto& p = Z().pool;
        if (d > 0) p.push_back(id);
        else {
          auto it = std::find(p.begin(), p.end(), id);
          if (it != p.end() && p.size() > 1) p.erase(it);
        }
      };
      c.items.push_back(t);
    }
    c.onCancel = [this, i] { editZone(i, 3); };
    G.menus.push(c);
  };
  m.items.push_back(cr);
  m.items.push_back({"Déplacer (coin haut-gauche)", "", "", true, [this, Z] {
                       askPosition("Nouveau coin haut-gauche de la zone : Entrée ou clic", [this, Z](int x, int y) {
                         begin();
                         Z().x = x, Z().y = y;
                       });
                     }});
  m.items.push_back({"Redimensionner (coin bas-droit)", "", "", true, [this, Z] {
                       anchorX_ = Z().x, anchorY_ = Z().y;
                       askPosition("Coin bas-droit de la zone : Entrée ou clic", [this, Z](int x, int y) {
                         begin();
                         Z().w = std::max(1, x - Z().x + 1), Z().h = std::max(1, y - Z().y + 1);
                       });
                     }});
  m.items.push_back({"Passer en premier", "", "Quand des zones se chevauchent, la première de la liste l'emporte.", i > 0, [this, i] {
                       begin();
                       std::swap(M().zones[i], M().zones[0]);
                       editZone(0, 6);
                     }});
  m.items.push_back({"Supprimer", "", "", true, [this, i] {
                       begin();
                       M().zones.erase(M().zones.begin() + i);
                       G.menus.clear();
                     }});
  G.menus.push(m);
}

// --- Zones déclencheuses
void MapEditor::editTrigger(int i, int sel) {
  G.menus.clear();
  auto T = [this, i]() -> Trigger& { return M().triggers[i]; };
  Menu m;
  m.title = "Zone déclencheuse";
  m.x = 4, m.y = 16, m.w = 196, m.rows = 6;
  m.sel = sel;
  MenuItem ev{"Événement", "", "", true};
  ev.rightFn = [T] { return T().event.empty() ? std::string("aucun") : utf8Prefix(T().event, 12); };
  ev.act = [this, T, i] {
    pickEvent(T().event, [this, T, i](const std::string& id) {
      begin();
      T().event = id;
      editTrigger(i, 0);
    });
  };
  m.items.push_back(ev);
  MenuItem un{"Jusqu'à (drapeau)", "", "Ne se déclenche plus quand ce drapeau est posé (l'événement doit le poser).", true};
  un.rightFn = [T] { return T().until.empty() ? std::string("—") : utf8Prefix(T().until, 12); };
  un.act = [this, T, i] {
    G.editText("Drapeau qui arrête la zone", T().until, 30, [this, T, i](const std::string& v) {
      begin();
      T().until = v;
      editTrigger(i, 1);
    });
  };
  m.items.push_back(un);
  m.items.push_back({"Déplacer (coin haut-gauche)", "", "", true, [this, T] {
                       askPosition("Nouveau coin haut-gauche : Entrée ou clic", [this, T](int x, int y) {
                         begin();
                         T().x = x, T().y = y;
                       });
                     }});
  m.items.push_back({"Redimensionner (coin bas-droit)", "", "", true, [this, T] {
                       anchorX_ = T().x, anchorY_ = T().y;
                       askPosition("Coin bas-droit : Entrée ou clic", [this, T](int x, int y) {
                         begin();
                         T().w = std::max(1, x - T().x + 1), T().h = std::max(1, y - T().y + 1);
                       });
                     }});
  m.items.push_back({"Supprimer", "", "", true, [this, i] {
                       begin();
                       M().triggers.erase(M().triggers.begin() + i);
                       G.menus.clear();
                     }});
  G.menus.push(m);
}

// ---------------------------------------------------------------------------
// Menu principal de l'éditeur (Échap)
// ---------------------------------------------------------------------------
void MapEditor::menuMain(int sel) {
  G.menus.clear();
  Menu m;
  m.title = "Éditeur : " + utf8Prefix(M().name, 18);
  m.x = 4, m.y = 16, m.w = 196, m.rows = 14;
  m.sel = sel;
  static const char* LAYERS[] = {"Tuiles", "Objets", "Zones"};
  static const char* TOOLS[] = {"Pinceau", "Rectangle", "Remplissage"};
  m.items.push_back(cycleItem("Calque", "Tab change aussi de calque.", [this] { return (int)layer_; }, [this](int v) { layer_ = Layer(v); }, 3,
                              [](int v) { return std::string(LAYERS[v]); }));
  MenuItem t{"Tuile", "", "Molette ou Page préc./suiv. pour changer de tuile ; clic droit sur la carte : pipette.", true,
             [this] { menuTiles(); }};
  t.rightFn = [this] { return tileName(tile_) + " >"; };
  m.items.push_back(t);
  m.items.push_back(cycleItem("Outil", "Pinceau : case par case. Rectangle : deux coins. Remplissage : toute la surface d'une même tuile.",
                              [this] { return (int)tool_; }, [this](int v) { tool_ = Tool(v); }, 3, [](int v) { return std::string(TOOLS[v]); }));
  m.items.push_back(cycleItem("Vue", "", [this] { return zoom_ == 16 ? 0 : 1; },
                              [this](int v) {
                                zoom_ = v == 0 ? 16 : 8;
                                follow();
                              },
                              2, [](int v) { return std::string(v == 0 ? "normale" : "éloignée"); }));
  m.items.push_back(menuHeader("Carte"));
  m.items.push_back({"Propriétés", ">", "Nom, thème, ambiance, taux de rencontre.", true, [this] { menuProps(); }});
  m.items.push_back({"Taille", ">", "", true, [this] { menuSize(); }});
  MenuItem chk{"Vérifier", "", "Lieux inaccessibles, références inconnues…", true, [this] { menuProblems(); }};
  chk.rightFn = [this] {
    auto p = problems();
    return p.empty() ? std::string("ok") : std::to_string(p.size()) + " problème(s)";
  };
  m.items.push_back(chk);
  m.items.push_back({"Tester ici", "", "Jouer depuis le curseur avec une équipe de test. Échap > « Fin du test ».", true, [this] { testHere(); }});
  m.items.push_back(numItem("Niveau du test", "Niveau de l'équipe utilisée par « Tester ici ».", [this] { return (double)testLevel_; },
                            [this](double v) { testLevel_ = (int)v; }, 1, 1, 100));
  m.items.push_back({"Annuler (Ctrl+Z)", "", "", !undo_.empty(), [this] {
                       undo();
                       menuMain(10);
                     }});
  m.items.push_back({"Rétablir (Ctrl+Y)", "", "", !redo_.empty(), [this] {
                       redo();
                       menuMain(11);
                     }});
  m.items.push_back(menuHeader("Fichiers"));
  m.items.push_back({"Ouvrir une carte", ">", "", true, [this] { menuOpen(); }});
  m.items.push_back({"Nouvelle carte", "", "", true, [this] { newMap(); }});
  MenuItem sv{"Enregistrer (Ctrl+S)", "", "Écrit data/cartes/<carte>.json.", true, [this] {
                save();
                menuMain(15);
              }};
  sv.rightFn = [this] { return std::string(dirty(map_) ? "modifiée" : "à jour"); };
  m.items.push_back(sv);
  m.items.push_back({"Recharger la carte", "", "Abandonne les modifications de cette carte.", true, [this] {
                       reload();
                       menuMain(16);
                     }});
  m.items.push_back({"Quitter l'éditeur", "", "", true, [this] {
                       std::vector<int> d;
                       for (int k = 0; k < (int)maps().size(); k++)
                         if (dirty(k)) d.push_back(k);
                       auto leave = [this] {
                         G.menus.clear();
                         G.titleMenu();
                       };
                       if (d.empty()) return leave();
                       Menu q;
                       q.title = std::to_string(d.size()) + " carte(s) non enregistrée(s)";
                       q.x = 50, q.y = 80, q.w = 220, q.rows = 3;
                       q.items.push_back({"Tout enregistrer et quitter", "", "", true, [this, d, leave] {
                                            for (int k : d) {
                                              saveMap(maps()[k]);
                                              saved_[maps()[k].id] = mapToJson(maps()[k]);
                                            }
                                            leave();
                                          }});
                       q.items.push_back({"Quitter sans enregistrer", "", "Les cartes reviennent au contenu des fichiers.", true, [this, d, leave] {
                                            for (int k = (int)d.size() - 1; k >= 0; k--) {
                                              auto it = saved_.find(maps()[d[k]].id);
                                              if (it != saved_.end()) maps()[d[k]] = mapFromJson(it->second);
                                              else maps().erase(maps().begin() + d[k]);  // nouvelle carte jamais enregistrée
                                            }
                                            map_ = 0;
                                            leave();
                                          }});
                       q.items.push_back({"Annuler", "", "", true, [this] { G.menus.pop(); }});
                       G.menus.push(q);
                     }});
  m.items.push_back({"Retour à la carte", "", "", true, [this] { G.menus.clear(); }});
  m.onCancel = [this] { G.menus.clear(); };
  G.menus.push(m);
}

void MapEditor::menuTiles() {
  Menu m;
  m.title = "Tuile";
  m.x = 30, m.y = 20, m.w = 170, m.rows = 12;
  auto& pal = tilePalette();
  for (size_t k = 0; k < pal.size(); k++) {
    char c = pal[k].c;
    if (c == tile_) m.sel = (int)k;
    std::string help = std::string(tileWalkable(c) ? "Praticable" : "Bloque le passage") + (tileEncounter(c) ? " · rencontres" : "");
    m.items.push_back({pal[k].name, std::string(1, c), help, true, [this, c] {
                         tile_ = c;
                         layer_ = Layer::Tiles;
                         G.menus.clear();
                       }});
  }
  G.menus.push(m);
}

void MapEditor::menuProps(int sel) {
  G.menus.clear();
  Menu m;
  m.title = "Propriétés de la carte";
  m.x = 4, m.y = 16, m.w = 236, m.rows = 7;
  m.sel = sel;
  MenuItem nm{"Nom", "", "Nom affiché dans le bandeau en entrant sur la carte.", true};
  nm.rightFn = [this] { return utf8Prefix(M().name, 16); };
  nm.act = [this] {
    G.editText("Nom de la carte", M().name, 30, [this](const std::string& v) {
      if (!v.empty()) {
        begin();
        M().name = v;
      }
      menuProps(0);
    });
  };
  m.items.push_back(nm);
  m.items.push_back({"Identifiant", M().id, "Nom du fichier data/cartes/" + M().id + ".json (ne change pas).", false});
  m.items.push_back(cycleItem("Thème", "Sol, arbres, chemins, parois et décor des combats.", [this] { return (int)M().theme; },
                              [this](int v) {
                                begin();
                                M().theme = Theme(v);
                              },
                              5, [](int v) { return std::string(THEMES_FR[v]); }));
  m.items.push_back(cycleItem("Ambiance", "",
                              [this] {
                                for (int k = 0; k < 6; k++)
                                  if (M().ambiance == AMBIANCES[k]) return k;
                                return 0;
                              },
                              [this](int v) {
                                begin();
                                M().ambiance = AMBIANCES[v];
                              },
                              6, [](int v) { return std::string(AMBIANCES_FR[v]); }));
  MenuItem un{"Ambiance jusqu'à", "", "L'ambiance disparaît quand ce drapeau est posé (ex. boss1).", true};
  un.rightFn = [this] { return M().ambianceUntil.empty() ? std::string("—") : M().ambianceUntil; };
  un.act = [this] {
    G.editText("Drapeau qui arrête l'ambiance", M().ambianceUntil, 30, [this](const std::string& v) {
      begin();
      M().ambianceUntil = v;
      menuProps(4);
    });
  };
  m.items.push_back(un);
  MenuItem rate = numItem("Taux de rencontre", "Chance de rencontre par pas sur les tuiles à rencontres (0 : valeur des règles).",
                          [this] { return (double)M().encounterRate; },
                          [this](double v) {
                            begin();
                            M().encounterRate = (float)v;
                          },
                          .01, 0, 1);
  m.items.push_back(rate);
  m.onCancel = [this] { menuMain(5); };
  G.menus.push(m);
}

void MapEditor::menuSize() {
  G.menus.clear();
  static int nw = 0, nh = 0, fillIdx = 0;
  nw = M().w(), nh = M().h();
  fillIdx = tileIndex(M().theme == Theme::Grotte || M().theme == Theme::Neige ? '#' : 'T');
  Menu m;
  m.title = "Taille de la carte";
  m.x = 4, m.y = 16, m.w = 196, m.rows = 5;
  m.items.push_back(numItem("Largeur", "", [] { return (double)nw; }, [](double v) { nw = (int)v; }, 1, 10, 200));
  m.items.push_back(numItem("Hauteur", "", [] { return (double)nh; }, [](double v) { nh = (int)v; }, 1, 8, 200));
  m.items.push_back(cycleItem("Remplir avec", "Tuile des nouvelles cases.", [] { return fillIdx; }, [](int v) { fillIdx = v; },
                              (int)tilePalette().size(), [](int v) { return std::string(tilePalette()[v].name); }));
  m.items.push_back({"Appliquer", "", "Agrandit ou rogne à droite et en bas. Les objets hors de la carte sont supprimés.", true, [this] {
                       begin();
                       MapDef& mp = M();
                       char f = tilePalette()[fillIdx].c;
                       mp.rows.resize(nh, std::string(nw, f));
                       for (auto& r : mp.rows) r.resize(nw, f);
                       auto out = [&](int x, int y) { return x < 0 || y < 0 || x >= nw || y >= nh; };
                       auto drop = [&](auto& v, auto pred) { v.erase(std::remove_if(v.begin(), v.end(), pred), v.end()); };
                       drop(mp.npcs, [&](const Npc& o) { return out(o.x, o.y); });
                       drop(mp.chests, [&](const Chest& o) { return out(o.x, o.y); });
                       drop(mp.signs, [&](const Sign& o) { return out(o.x, o.y); });
                       drop(mp.warps, [&](const Warp& o) { return out(o.x, o.y); });
                       drop(mp.bosses, [&](const BossSpot& o) { return out(o.x + 1, o.y + 1); });
                       drop(mp.buildings, [&](const Building& o) { return out(o.x + o.w - 1, o.y + o.h - 1); });
                       setCursor(cx_, cy_);
                       say("Nouvelle taille : " + std::to_string(nw) + " x " + std::to_string(nh));
                       G.menus.clear();
                     }});
  m.onCancel = [this] { menuMain(6); };
  G.menus.push(m);
}

void MapEditor::menuOpen() {
  Menu m;
  m.title = "Ouvrir une carte";
  m.x = 30, m.y = 20, m.w = 220, m.rows = 12;
  for (int k = 0; k < (int)maps().size(); k++) {
    if (k == map_) m.sel = k;
    m.items.push_back({maps()[k].name + (dirty(k) ? " *" : ""), std::to_string(maps()[k].w()) + "x" + std::to_string(maps()[k].h()),
                       "data/cartes/" + maps()[k].id + ".json", true, [this, k] {
                         G.menus.clear();
                         select(k);
                       }});
  }
  G.menus.push(m);
}

void MapEditor::newMap() {
  G.editText("Nom de la nouvelle carte", "", 30, [this](const std::string& name) {
    if (name.empty()) return;
    std::string id = makeSlug(name), base = id;
    for (int n = 2; mapIndex(id) >= 0; n++) id = base + "_" + std::to_string(n);
    MapDef m;
    m.id = id;
    m.name = name;
    m.theme = Theme::Vallee;
    m.rows.assign(30, std::string(40, '.'));
    for (int x = 0; x < 40; x++) m.rows[0][x] = m.rows[29][x] = 'T';
    for (int y = 0; y < 30; y++) m.rows[y][0] = m.rows[y][39] = 'T';
    maps().push_back(m);
    select((int)maps().size() - 1);
    say("Nouvelle carte « " + name + " » (data/cartes/" + id + ".json). Pensez à y relier un passage !");
  });
}

void MapEditor::menuProblems() {
  Menu m;
  auto p = problems();
  m.title = p.empty() ? "Aucun problème" : std::to_string(p.size()) + " problème(s)";
  m.x = 4, m.y = 16, m.w = 312, m.rows = 12;
  for (auto& s : p) m.items.push_back({utf8Prefix(s, 48), "", s, true, nullptr});
  if (p.empty()) m.items.push_back({"Tout est accessible et les références existent.", "", "", false});
  G.menus.push(m);
}

// ---------------------------------------------------------------------------
// Tester la carte
// ---------------------------------------------------------------------------
void MapEditor::testHere() {
  const MapDef& m = M();
  if (!tileWalkable(m.rows[cy_][cx_])) return say("Placez le curseur sur une case praticable pour tester.");
  G.menus.clear();
  G.sc.clear();
  G.flags.clear();
  G.team.clear();
  G.team.push_back(makeFighter(rules().hero, testLevel_));
  for (size_t k = 0; k < rules().starters.size() && k < 2; k++) G.team.push_back(makeFighter(rules().starters[k], testLevel_));
  G.items.clear();
  for (auto [id, n] : std::vector<std::pair<std::string, int>>{{"potion", 5}, {"superpotion", 2}, {"ether", 2}, {"remede", 3}, {"lanterne", 5}})
    if (hasItem(id)) G.items[id] = n;
  G.gold = 500;
  G.editorTest_ = true;
  G.respawnMap = map_, G.respawnX = cx_, G.respawnY = cy_;
  G.mapId = -1;
  G.changeMap(map_, cx_, cy_, DOWN);
  G.mode = Mode::Map;
}

void MapEditor::returnFromTest() {
  G.editorTest_ = false;
  G.menus.clear();
  G.sc.clear();
  G.mode = Mode::Editor;
  if (G.mapId >= 0 && G.mapId < (int)maps().size() && G.mapId != map_) select(G.mapId);
  setCursor(G.px, G.py);
  say("Retour à l'éditeur.");
}

// ---------------------------------------------------------------------------
// Mise à jour
// ---------------------------------------------------------------------------
void MapEditor::update(float) {
  if (G.editingText()) return;
  Input& in = G.in;
  if (G.menus.active()) {
    G.menus.update(in);
    return;
  }
  if (in.undo) undo();
  if (in.redo) redo();
  if (in.saveKey) save();
  if (in.tab) {
    layer_ = Layer(((int)layer_ + 1) % 3);
    static const char* L[] = {"Calque : tuiles", "Calque : objets", "Calque : zones"};
    say(L[(int)layer_]);
  }
  auto& pal = tilePalette();
  int ti = tileIndex(tile_);
  bool overMap = in.mouseOn && in.my >= TOP && in.my < BOTTOM;
  if (in.prev || (in.wheel > 0 && layer_ == Layer::Tiles)) tile_ = pal[(ti + (int)pal.size() - 1) % pal.size()].c;
  if (in.next || (in.wheel < 0 && layer_ == Layer::Tiles)) tile_ = pal[(ti + 1) % pal.size()].c;
  if (in.del && layer_ == Layer::Objects && !pending_) {
    MapDef& m = M();
    auto at = [&](auto& v) {
      for (size_t i = 0; i < v.size(); i++)
        if (v[i].x == cx_ && v[i].y == cy_) {
          begin();
          v.erase(v.begin() + i);
          say("Supprimé.");
          return true;
        }
      return false;
    };
    if (!at(m.npcs) && !at(m.chests) && !at(m.signs)) at(m.warps);
  }
  for (int d : {UP, DOWN, LEFT, RIGHT})
    if (in.press[d]) {
      setCursor(cx_ + (d == LEFT ? -1 : d == RIGHT ? 1 : 0), cy_ + (d == UP ? -1 : d == DOWN ? 1 : 0));
      if (stroke_ || (in.shift && layer_ == Layer::Tiles && tool_ == Tool::Brush)) paintAt(cx_, cy_);  // Maj : peindre en se déplaçant
    }
  // Souris
  if (overMap && (in.moved || in.mclick[0] || in.mclick[2])) {
    int tx = int((in.mx + camX_) / zoom_), ty = int((in.my - TOP + camY_) / zoom_);
    if (tx >= 0 && ty >= 0 && tx < M().w() && ty < M().h()) cx_ = tx, cy_ = ty;
  }
  if (in.mouseOn && in.my >= BOTTOM && in.mclick[0] && layer_ == Layer::Tiles) {  // clic dans la palette
    int first = std::clamp(ti - 8, 0, std::max(0, (int)pal.size() - 17));
    int k = first + (in.mx - 4) / 18;
    if (k >= 0 && k < (int)pal.size()) tile_ = pal[k].c;
  }
  if (overMap && in.mclick[0]) {
    if (!pending_ && layer_ == Layer::Tiles && tool_ == Tool::Brush) {
      begin();
      stroke_ = true;
      paintAt(cx_, cy_);
    } else apply();
  } else if (stroke_ && in.mdown[0] && overMap) paintAt(cx_, cy_);
  if (!in.mdown[0]) stroke_ = false;
  if (overMap && in.mclick[2]) {
    if (layer_ == Layer::Tiles) {
      tile_ = M().rows[cy_][cx_];
      say("Pipette : " + tileName(tile_));
    } else if (!pending_) act(cx_, cy_);
  }
  if (in.confirm) apply();
  if (in.cancel) {
    if (pending_) {
      pending_ = nullptr;
      pendingText_.clear();
      anchorX_ = anchorY_ = -1;
      say("Annulé.");
    } else menuMain();
  }
}

// ---------------------------------------------------------------------------
// Dessin
// ---------------------------------------------------------------------------
void MapEditor::drawMap() {
  Gfx& g = G.g;
  const MapDef& m = M();
  int z = zoom_;
  int cx = (int)camX_, cy = (int)camY_;
  int tx0 = std::max(0, (int)std::floor((cx + g.left()) / z)), tx1 = (int)((cx + g.right()) / z) + 1, ty0 = cy / z;
  auto SX = [&](int x) { return x * z - cx; };
  auto SY = [&](int y) { return TOP + y * z - cy; };
  for (int y = ty0; y <= ty0 + VIEW_H / z + 1 && y < m.h(); y++)
    for (int x = tx0; x <= tx1 && x < m.w(); x++) {
      if (z == 16) drawTile(g, m, x, y, SX(x), SY(y), G.time);
      else g.rect(SX(x), SY(y), z, z, rgb(tileColor(m.rows[y][x], m.theme)));
    }
  // Objets
  for (auto& b : m.buildings) {
    if (z == 16) drawBuilding(g, b, SX(b.x), SY(b.y));
    else g.rect(SX(b.x), SY(b.y), b.w * z, b.h * z, rgb(b.roof));
  }
  for (auto& c : m.chests) {
    if (z == 16) drawChest(g, SX(c.x), SY(c.y), false);
    else g.rect(SX(c.x) + 1, SY(c.y) + 1, z - 2, z - 2, rgb(0xa06a32));
  }
  for (auto& s : m.signs) {
    if (z == 16) drawSign(g, SX(s.x), SY(s.y));
    else g.rect(SX(s.x) + 2, SY(s.y) + 2, z - 4, z - 4, rgb(0xb88a52));
  }
  for (auto& n : m.npcs) {
    if (z == 16) drawHuman(g, look(n.look), SX(n.x), SY(n.y) - 2, 1, n.dir, 0, false);
    else g.rect(SX(n.x) + 1, SY(n.y) + 1, z - 2, z - 2, rgb(0xffd34d));
  }
  for (auto& b : m.bosses) {
    if (z == 16) drawCreature(g, b.id, SX(b.x + 1), SY(b.y + 1) - 2, .6f, false, G.time);
    else g.rect(SX(b.x), SY(b.y), 2 * z, 2 * z, rgb(0xd2493f));
  }
  // Passages
  for (auto& w : m.warps) {
    Color c = w.condition.is_null() ? CYAN : ORANGE;
    g.frame(SX(w.x), SY(w.y), z, z, c);
    if (z == 16) g.text(SX(w.x) + 5, SY(w.y) + 1, "▶", c, 0, false);
  }
  // Vue des dresseurs
  if (layer_ == Layer::Objects)
    for (auto& n : m.npcs)
      for (int k = 1; k <= n.sight; k++) {
        int x = n.x + (n.dir == LEFT ? -k : n.dir == RIGHT ? k : 0), y = n.y + (n.dir == UP ? -k : n.dir == DOWN ? k : 0);
        g.rect(SX(x), SY(y), z, z, rgb(0xffe066, 60));
      }
  // Zones
  if (layer_ == Layer::Zones) {
    for (size_t i = 0; i < m.zones.size(); i++) {
      auto& zo = m.zones[i];
      uint32_t c = ZONE_COLORS[i % 6];
      g.rect(SX(zo.x), SY(zo.y), zo.w * z, zo.h * z, rgb(c, 40));
      g.frame(SX(zo.x), SY(zo.y), zo.w * z, zo.h * z, rgb(c));
      g.text(SX(zo.x) + 2, SY(zo.y) + 1, "Z" + std::to_string(i + 1), rgb(c), 0, true);
    }
    for (auto& t : m.triggers) {
      g.rect(SX(t.x), SY(t.y), t.w * z, t.h * z, rgb(0xd08aff, 50));
      g.frame(SX(t.x), SY(t.y), t.w * z, t.h * z, VIOLET);
      g.text(SX(t.x) + 2, SY(t.y) + 1, "!", VIOLET, 0, true);
    }
  }
  // Rectangle en cours
  if (anchorX_ >= 0 && pending_) {
    int x0 = std::min(anchorX_, cx_), y0 = std::min(anchorY_, cy_), x1 = std::max(anchorX_, cx_), y1 = std::max(anchorY_, cy_);
    g.rect(SX(x0), SY(y0), (x1 - x0 + 1) * z, (y1 - y0 + 1) * z, rgb(0xffd34d, 50));
    g.frame(SX(x0), SY(y0), (x1 - x0 + 1) * z, (y1 - y0 + 1) * z, GOLD);
  }
  // Curseur
  bool blink = int(G.time * 4) % 2 == 0;
  Color cc = pending_ ? GOLD : WHITE;
  g.frame(SX(cx_), SY(cy_), z, z, blink ? cc : rgb(0x000000));
  g.frame(SX(cx_) - 1, SY(cy_) - 1, z + 2, z + 2, rgb(0x000000, 120));
}

// Description de ce qu'il y a sous le curseur
static std::string underCursor(const MapDef& m, int x, int y) {
  for (auto& n : m.npcs)
    if (n.x == x && n.y == y) return "Habitant : " + look(n.look).name + (n.event.empty() ? "" : " (" + n.event + ")");
  for (auto& c : m.chests)
    if (c.x == x && c.y == y) return "Coffre : " + (c.item.empty() ? std::to_string(c.qty) + " or" : item(c.item).name);
  for (auto& s : m.signs)
    if (s.x == x && s.y == y) return "Panneau : " + s.text;
  for (auto& w : m.warps)
    if (w.x == x && w.y == y) return "Passage vers " + (mapIndex(w.map) >= 0 ? maps()[mapIndex(w.map)].name : w.map);
  for (auto& b : m.bosses)
    if (x >= b.x && x <= b.x + 1 && y >= b.y && y <= b.y + 1) return "Boss : " + species(b.id).name;
  for (auto& b : m.buildings)
    if (x >= b.x && y >= b.y && x < b.x + b.w && y < b.y + b.h) return "Bâtiment : " + b.name;
  return "";
}

void MapEditor::drawBars() {
  Gfx& g = G.g;
  const MapDef& m = M();
  g.rect(g.left(), 0, g.fullW, TOP, rgb(0x0a0f33, 235));
  static const char* L[] = {"Tuiles", "Objets", "Zones"};
  std::string left = utf8Prefix(m.name, 16) + (dirty(map_) ? "*" : "") + " · " + L[(int)layer_];
  g.text(3, 0, left, GOLD);
  g.text(317, 0, std::to_string(cx_) + "," + std::to_string(cy_), MUTED, 2);
  // Ligne d'information sous la barre
  std::string info;
  Color ic = WHITE;
  if (!pendingText_.empty()) info = pendingText_, ic = GOLD;
  else if (G.time - messageT_ < 3) info = message_, ic = GOLD;
  else if (layer_ == Layer::Tiles) info = tileName(m.rows[cy_][cx_]);
  else info = underCursor(m, cx_, cy_);
  if (!info.empty()) {
    int w = std::min(312, Gfx::textW(utf8Prefix(info, 51)) + 8);
    g.rect(4, TOP + 2, w, 11, rgb(0x0a0f33, 200));
    g.text(8, TOP + 1, utf8Prefix(info, 51), ic);
  }
  // Barre du bas : palette ou aide
  g.rect(g.left(), BOTTOM, g.fullW, SCREEN_H - BOTTOM, rgb(0x0a0f33, 235));
  if (layer_ == Layer::Tiles) {
    static MapDef pal;
    pal.theme = m.theme;
    auto& p = tilePalette();
    if (pal.rows.empty() || (int)pal.rows[0].size() != (int)p.size()) {
      std::string row;
      for (auto& t : p) row += t.c;
      pal.rows = {row};
    }
    int ti = tileIndex(tile_);
    int first = std::clamp(ti - 8, 0, std::max(0, (int)p.size() - 17));
    for (int k = first; k < (int)p.size() && k < first + 17; k++) {
      int x = 4 + (k - first) * 18;
      drawTile(g, pal, k, 0, x, BOTTOM + 2, G.time);
      if (k == ti) g.frame(x - 1, BOTTOM + 1, 18, 18, GOLD);
    }
  } else {
    std::string h = layer_ == Layer::Objects ? "Entrée : modifier/ajouter · Suppr · Tab : calque" : "Entrée : modifier/créer une zone · Tab : calque";
    g.text(160, BOTTOM + 5, h, MUTED, 1);
  }
}

void MapEditor::draw() {
  Gfx& g = G.g;
  g.clear(rgb(0x070a1c));
  drawMap();
  drawBars();
  if (G.menus.active()) {
    G.menus.draw(g, G.time);
    std::string h = G.menus.help();
    if (!h.empty()) {
      auto lines = Gfx::wrap(h, 300);
      int hh = 8 + 11 * (int)lines.size();
      g.window(4, 238 - hh, 312, hh);
      for (size_t i = 0; i < lines.size(); i++) g.text(10, 238 - hh + 4 + i * 11, lines[i], MUTED);
    }
  }
}
