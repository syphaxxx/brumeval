#include "world.hpp"

#include <algorithm>
#include <deque>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include "data.hpp"
#include "i18n.hpp"

bool tileWalkable(char c) {
  if (c == 'O' || c == 'P' || c == 'D') return true;  // rochers, plaques et grilles de l'énigme : gérés par Game::blockedOn
  return c == '.' || c == ',' || c == '=' || c == 'F' || c == 'B' || c == 'a' || c == 'g' || c == 'b' || c == 'k' || c == 'c' || c == 'i' ||
         c == 'n' || c == 'z' || c == 'p' || c == 'q';
}
std::vector<std::pair<int, int>> puzzleTiles(const MapDef& m, char c) {
  std::vector<std::pair<int, int>> v;
  for (int y = 0; y < m.h(); y++)
    for (int x = 0; x < m.w(); x++)
      if (m.rows[y][x] == c) v.push_back({x, y});
  return v;
}

std::vector<int> solvePuzzle(const MapDef& m, int sx, int sy, std::vector<std::pair<int, int>> boulders,
                             const std::function<bool(int, int)>& blocked, bool* solved) {
  // Recherche en largeur sur les poussées (comme pour le Sokoban) : un état = les rochers et la zone où le
  // joueur peut marcher sans rien pousser (repérée par sa plus petite case) ; on ne compte que les poussées
  auto plates = puzzleTiles(m, 'P');
  std::sort(boulders.begin(), boulders.end());
  auto done = [&](const std::vector<std::pair<int, int>>& b) {
    for (auto& p : plates)
      if (!std::binary_search(b.begin(), b.end(), p)) return false;
    return true;
  };
  if (solved) *solved = done(boulders);
  if (plates.empty() || done(boulders)) return {};
  const int W = m.w(), H = m.h();
  const int* z = m.puzzleZone;
  std::vector<char> wallAt((size_t)W * H);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      bool out = z[2] > 0 && (x < z[0] || y < z[1] || x >= z[0] + z[2] || y >= z[1] + z[3]);
      wallAt[(size_t)y * W + x] = out || blocked(x, y) || m.rows[y][x] == 'D';
    }
  auto wall = [&](int x, int y) { return x < 0 || y < 0 || x >= W || y >= H || wallAt[(size_t)y * W + x]; };
  static const int DX[4] = {0, 0, -1, 1}, DY[4] = {-1, 1, 0, 0};  // UP, DOWN, LEFT, RIGHT (ordre de Dir)
  // Cases accessibles depuis (x, y) avec ces rochers : distance et direction d'arrivée (pour refaire le chemin)
  auto flood = [&](int x0, int y0, const std::vector<std::pair<int, int>>& b, std::vector<int>& from) {
    from.assign((size_t)W * H, -2);
    std::vector<char> rock((size_t)W * H, 0);
    for (auto& [bx, by] : b) rock[(size_t)by * W + bx] = 1;
    std::deque<int> q = {y0 * W + x0};
    from[(size_t)(y0 * W + x0)] = -1;
    int best = y0 * W + x0;
    while (!q.empty()) {
      int c = q.front();
      q.pop_front();
      best = std::min(best, c);
      for (int d = 0; d < 4; d++) {
        int nx = c % W + DX[d], ny = c / W + DY[d];
        if (wall(nx, ny) || rock[(size_t)ny * W + nx] || from[(size_t)(ny * W + nx)] != -2) continue;
        from[(size_t)(ny * W + nx)] = d;
        q.push_back(ny * W + nx);
      }
    }
    return best;  // case la plus petite de la zone : identifie l'état
  };
  auto walkTo = [&](const std::vector<int>& from, int target, std::vector<int>& out) {  // directions jusqu'à target
    std::vector<int> rev;
    for (int c = target; from[(size_t)c] >= 0;) {
      int d = from[(size_t)c];
      rev.push_back(d);
      c = (c / W - DY[d]) * W + (c % W - DX[d]);
    }
    out.insert(out.end(), rev.rbegin(), rev.rend());
  };
  struct State {
    int px, py;  // où se tient le joueur (après la dernière poussée)
    std::vector<std::pair<int, int>> b;
    int parent, standX, standY, dir;  // la poussée qui y mène
  };
  auto key = [&](int region, const std::vector<std::pair<int, int>>& b) {
    std::string k = std::to_string(region);
    for (auto& [bx, by] : b) k += "," + std::to_string(by * W + bx);
    return k;
  };
  std::vector<State> states = {{sx, sy, boulders, -1, 0, 0, 0}};
  std::unordered_set<std::string> seen;
  std::vector<int> from;
  seen.insert(key(flood(sx, sy, boulders, from), boulders));
  for (size_t i = 0; i < states.size() && states.size() < 400000; i++) {
    State cur = states[i];
    flood(cur.px, cur.py, cur.b, from);
    for (size_t k = 0; k < cur.b.size(); k++)
      for (int d = 0; d < 4; d++) {
        int bx = cur.b[k].first, by = cur.b[k].second;
        int stX = bx - DX[d], stY = by - DY[d], toX = bx + DX[d], toY = by + DY[d];
        if (wall(stX, stY) || from[(size_t)(stY * W + stX)] == -2) continue;  // le joueur ne peut pas se placer derrière
        if (wall(toX, toY) || std::find(cur.b.begin(), cur.b.end(), std::make_pair(toX, toY)) != cur.b.end()) continue;
        State n{bx, by, cur.b, (int)i, stX, stY, d};
        n.b[k] = {toX, toY};
        std::sort(n.b.begin(), n.b.end());
        std::vector<int> f2;
        if (!seen.insert(key(flood(n.px, n.py, n.b, f2), n.b)).second) continue;
        states.push_back(n);
        if (done(n.b)) {  // refaire le chemin : marcher jusqu'à chaque poussée, puis pousser
          std::vector<size_t> chain;
          for (int s = (int)states.size() - 1; s > 0; s = states[(size_t)s].parent) chain.push_back((size_t)s);
          std::reverse(chain.begin(), chain.end());
          std::vector<int> path;
          for (size_t s : chain) {
            const State& st = states[s];
            const State& pr = states[(size_t)st.parent];
            std::vector<int> f;
            flood(pr.px, pr.py, pr.b, f);
            walkTo(f, st.standY * W + st.standX, path);
            path.push_back(st.dir);
          }
          return path;
        }
      }
  }
  return {};
}

bool tileEncounter(char c) { return c == ',' || c == 'g' || c == 'c' || c == 'n' || c == 'z'; }

static const char* THEMES[N_THEMES] = {"vallee", "cendres", "grotte", "foret", "neige", "interieur"};
Theme themeOf(const std::string& s) {
  for (int i = 0; i < N_THEMES; i++)
    if (s == THEMES[i]) return Theme(i);
  throw std::runtime_error("Thème inconnu : « " + s + " » (possibles : vallee, cendres, grotte, foret, neige, interieur)");
}
const char* themeName(Theme t) { return THEMES[(int)t]; }

static std::vector<MapDef> MAPS;
std::vector<MapDef>& maps() { return MAPS; }
int mapIndex(const std::string& id) {
  for (size_t i = 0; i < MAPS.size(); i++)
    if (MAPS[i].id == id) return (int)i;
  return -1;
}

// ---------------------------------------------------------------------------
// Lecture et écriture JSON
// ---------------------------------------------------------------------------
MapDef mapFromJson(const Json& j) {
  MapDef m;
  m.id = j.at("id").get<std::string>();
  m.name = tr(j.at("nom").get<std::string>());
  m.theme = themeOf(j.at("theme").get<std::string>());
  m.encounterRate = jget(j, "taux_rencontre", 0.f);
  if (j.contains("ambiance")) {
    m.ambiance = jget<std::string>(j["ambiance"], "effet", "");
    m.ambianceUntil = jget<std::string>(j["ambiance"], "jusqua", "");
  }
  m.rows = j.at("tuiles").get<std::vector<std::string>>();
  for (auto& r : m.rows)
    if (r.size() != m.rows[0].size()) throw std::runtime_error("toutes les lignes de « tuiles » doivent avoir la même longueur");
  if (j.contains("carte_monde") && j["carte_monde"].is_array() && j["carte_monde"].size() == 2)
    m.worldX = j["carte_monde"][0].get<float>(), m.worldY = j["carte_monde"][1].get<float>();
  for (auto& o : j.value("batiments", Json::array())) {
    Building b{o.at("x").get<int>(), o.at("y").get<int>(), o.at("l").get<int>(), o.at("h").get<int>(),
               o.at("genre").get<std::string>(), tr(jget<std::string>(o, "nom", "")), parseColor(o.value("toit", Json("#7a8a4a")))};
    b.event = jget<std::string>(o, "evenement", b.kind);
    b.interior = jget<std::string>(o, "interieur", "");
    b.look = jget<std::string>(o, "dessin", "");
    m.buildings.push_back(b);
  }
  for (auto& o : j.value("habitants", Json::array())) {
    Npc n;
    n.x = o.at("x").get<int>();
    n.y = o.at("y").get<int>();
    std::string lk = jget<std::string>(o, "apparence", "villageois");
    n.look = lookIndex(lk);
    if (n.look < 0) throw std::runtime_error("apparence inconnue : " + lk);
    n.dir = dirOf(jget<std::string>(o, "direction", "bas"));
    n.event = jget<std::string>(o, "evenement", "");
    for (auto& l : jget(o, "dialogue", std::vector<std::string>{})) n.lines.push_back(tr(l));
    n.hideIf = jget<std::string>(o, "cache_si", "");
    n.sight = jget(o, "vue", 0);
    n.sightUntil = jget<std::string>(o, "vue_jusqua", "");
    m.npcs.push_back(n);
  }
  for (auto& o : j.value("coffres", Json::array())) {
    Chest c{o.at("x").get<int>(), o.at("y").get<int>(), jget<std::string>(o, "objet", ""), 0};
    c.qty = c.item.empty() ? jget(o, "or", 0) : jget(o, "quantite", 1);
    m.chests.push_back(c);
  }
  for (auto& o : j.value("panneaux", Json::array())) m.signs.push_back({o.at("x").get<int>(), o.at("y").get<int>(), tr(o.at("texte").get<std::string>())});
  for (auto& o : j.value("passages", Json::array()))
    m.warps.push_back({o.at("x").get<int>(), o.at("y").get<int>(), o.at("vers").get<std::string>(), o.at("tx").get<int>(),
                       o.at("ty").get<int>(), dirOf(jget<std::string>(o, "direction", "bas")), o.value("condition", Json()),
                       tr(jget<std::string>(o, "message", ""))});
  for (auto& o : j.value("declencheurs", Json::array()))
    m.triggers.push_back({o.at("x").get<int>(), o.at("y").get<int>(), jget(o, "l", 1), jget(o, "h", 1), o.at("evenement").get<std::string>(),
                          jget<std::string>(o, "jusqua", "")});
  for (auto& o : j.value("zones", Json::array()))
    m.zones.push_back({o.at("x").get<int>(), o.at("y").get<int>(), o.at("l").get<int>(), o.at("h").get<int>(), o.at("niveau_min").get<int>(),
                       o.at("niveau_max").get<int>(), jget(o, "max_ennemis", 3), o.at("creatures").get<std::vector<std::string>>()});
  if (j.contains("enigme")) {
    const Json& e = j["enigme"];
    m.puzzleFlag = jget<std::string>(e, "drapeau", "");
    m.puzzleMsg = tr(jget<std::string>(e, "message", ""));
    m.puzzleX = jget(e, "x", -1), m.puzzleY = jget(e, "y", -1);
    if (e.contains("zone")) {
      const Json& z = e["zone"];
      m.puzzleZone[0] = jget(z, "x", 0), m.puzzleZone[1] = jget(z, "y", 0), m.puzzleZone[2] = jget(z, "l", 0), m.puzzleZone[3] = jget(z, "h", 0);
    }
  }
  for (auto& o : j.value("boss", Json::array())) {
    BossSpot b{o.at("x").get<int>(), o.at("y").get<int>(), o.at("espece").get<std::string>(), o.at("drapeau").get<std::string>(), ""};
    b.event = jget<std::string>(o, "evenement", b.id);
    m.bosses.push_back(b);
  }
  return m;
}

Json mapToJson(const MapDef& m) {
  Json o = {{"id", m.id}, {"nom", m.name}, {"theme", themeName(m.theme)}};
  if (!m.ambiance.empty()) {
    o["ambiance"] = {{"effet", m.ambiance}};
    if (!m.ambianceUntil.empty()) o["ambiance"]["jusqua"] = m.ambianceUntil;
  }
  if (m.encounterRate > 0) o["taux_rencontre"] = (double)m.encounterRate;
  o["tuiles"] = m.rows;
  Json b = Json::array();
  for (auto& x : m.buildings) {
    Json p = {{"x", x.x}, {"y", x.y}, {"l", x.w}, {"h", x.h}, {"genre", x.kind}, {"nom", x.name}, {"toit", colorStr(x.roof)}};
    if (x.event != x.kind) p["evenement"] = x.event;
    if (!x.interior.empty()) p["interieur"] = x.interior;
    if (!x.look.empty()) p["dessin"] = x.look;
    b.push_back(p);
  }
  o["batiments"] = b;
  if (m.worldX >= 0) o["carte_monde"] = Json::array({(int)std::lround(m.worldX), (int)std::lround(m.worldY)});
  Json n = Json::array();
  for (auto& x : m.npcs) {
    Json p = {{"x", x.x}, {"y", x.y}, {"apparence", look(x.look).id}, {"direction", dirName(x.dir)}};
    if (!x.event.empty()) p["evenement"] = x.event;
    if (!x.hideIf.empty()) p["cache_si"] = x.hideIf;
    if (x.sight > 0) p["vue"] = x.sight;
    if (!x.sightUntil.empty()) p["vue_jusqua"] = x.sightUntil;
    if (!x.lines.empty()) p["dialogue"] = x.lines;
    n.push_back(p);
  }
  o["habitants"] = n;
  Json c = Json::array();
  for (auto& x : m.chests) {
    Json p = {{"x", x.x}, {"y", x.y}};
    if (x.item.empty()) p["or"] = x.qty;
    else p["objet"] = x.item, p["quantite"] = x.qty;
    c.push_back(p);
  }
  o["coffres"] = c;
  Json s = Json::array();
  for (auto& x : m.signs) s.push_back({{"x", x.x}, {"y", x.y}, {"texte", x.text}});
  o["panneaux"] = s;
  Json w = Json::array();
  for (auto& x : m.warps) {
    Json p = {{"x", x.x}, {"y", x.y}, {"vers", x.map}, {"tx", x.tx}, {"ty", x.ty}, {"direction", dirName(x.dir)}};
    if (!x.condition.is_null()) p["condition"] = x.condition;
    if (!x.message.empty()) p["message"] = x.message;
    w.push_back(p);
  }
  o["passages"] = w;
  if (!m.puzzleFlag.empty()) {
    o["enigme"] = {{"drapeau", m.puzzleFlag}, {"x", m.puzzleX}, {"y", m.puzzleY}};
    if (m.puzzleZone[2] > 0)
      o["enigme"]["zone"] = {{"x", m.puzzleZone[0]}, {"y", m.puzzleZone[1]}, {"l", m.puzzleZone[2]}, {"h", m.puzzleZone[3]}};
    if (!m.puzzleMsg.empty()) o["enigme"]["message"] = m.puzzleMsg;
  }
  Json z = Json::array();
  for (auto& x : m.zones)
    z.push_back({{"x", x.x}, {"y", x.y}, {"l", x.w}, {"h", x.h}, {"niveau_min", x.lo}, {"niveau_max", x.hi}, {"max_ennemis", x.maxN}, {"creatures", x.pool}});
  o["zones"] = z;
  Json bo = Json::array();
  for (auto& x : m.bosses) {
    Json p = {{"x", x.x}, {"y", x.y}, {"espece", x.id}, {"drapeau", x.flag}};
    if (x.event != x.id) p["evenement"] = x.event;
    bo.push_back(p);
  }
  o["boss"] = bo;
  if (!m.triggers.empty()) {
    Json t = Json::array();
    for (auto& x : m.triggers) {
      Json p = {{"x", x.x}, {"y", x.y}, {"l", x.w}, {"h", x.h}, {"evenement", x.event}};
      if (!x.until.empty()) p["jusqua"] = x.until;
      t.push_back(p);
    }
    o["declencheurs"] = t;
  }
  return o;
}

void loadMaps() {
  MAPS.clear();
  for (auto& f : listJson("cartes")) {
    try {
      MAPS.push_back(mapFromJson(readJson("cartes/" + f + ".json")));
    } catch (const std::runtime_error& e) {
      std::string msg = e.what();
      if (msg.rfind("Erreur dans", 0) == 0 || msg.rfind("Fichier", 0) == 0) throw;
      throw std::runtime_error("data/cartes/" + f + ".json : " + msg);
    } catch (const std::exception& e) {
      throw std::runtime_error("data/cartes/" + f + ".json : " + e.what());
    }
  }
  if (MAPS.empty()) throw std::runtime_error("Aucune carte dans data/cartes/");
}

void saveMap(const MapDef& m) { writeJson("cartes/" + m.id + ".json", mapToJson(m)); }

// ---------------------------------------------------------------------------
// Vérifications
// ---------------------------------------------------------------------------
bool interiorEntry(const MapDef& in, const std::string& outside, int& x, int& y) {
  for (auto& w : in.warps)
    if (w.map == outside && w.y > 0) {
      x = w.x, y = w.y - 1;
      return true;
    }
  return false;
}

std::vector<std::string> checkMaps() {
  std::vector<std::string> err;
  const Rules& r = rules();
  if (mapIndex(r.startMap) < 0) err.push_back("Carte de départ inconnue : " + r.startMap);
  if (mapIndex(r.respawnMap) < 0) err.push_back("Carte de réveil inconnue : " + r.respawnMap);
  for (auto& m : MAPS) {
    auto at = [&](int x, int y) { return "(" + std::to_string(x) + ", " + std::to_string(y) + ")"; };
    auto E = [&](const std::string& s) { err.push_back(m.name + " : " + s); };
    auto inside = [&](int x, int y) { return x >= 0 && y >= 0 && x < m.w() && y < m.h(); };
    for (auto& z : m.zones)
      for (auto& p : z.pool)
        if (!hasSpecies(p)) E("créature inconnue dans une zone : " + p);
    for (auto& c : m.chests)
      if (!c.item.empty() && !hasItem(c.item)) E("objet inconnu dans un coffre " + at(c.x, c.y) + " : " + c.item);
    for (auto& b : m.bosses)
      if (!hasSpecies(b.id)) E("boss inconnu : " + b.id);
    for (auto& b : m.buildings) {
      int t = mapIndex(b.interior);
      int ex, ey;
      if (b.interior.empty()) continue;
      if (t < 0) E("bâtiment « " + b.name + " » : intérieur inconnu : " + b.interior);
      else if (!interiorEntry(MAPS[t], m.id, ex, ey)) E("bâtiment « " + b.name + " » : l'intérieur " + b.interior + " n'a pas de sortie vers cette carte");
    }
    for (auto& w : m.warps) {
      int t = mapIndex(w.map);
      if (t < 0) E("passage " + at(w.x, w.y) + " vers une carte inconnue : " + w.map);
      else if (w.tx < 0 || w.ty < 0 || w.tx >= MAPS[t].w() || w.ty >= MAPS[t].h() || !tileWalkable(MAPS[t].rows[w.ty][w.tx]))
        E("passage " + at(w.x, w.y) + " : l'arrivée " + at(w.tx, w.ty) + " sur " + MAPS[t].name + " n'est pas praticable");
    }

    // Énigme (3.0) : autant de rochers que de plaques, et une solution depuis sa case de départ
    if (!m.puzzleFlag.empty()) {
      auto rocks = puzzleTiles(m, 'O'), plates = puzzleTiles(m, 'P');
      if (rocks.size() != plates.size() || plates.empty() || rocks.size() > 3) E("énigme : il faut autant de rochers 'O' que de plaques 'P' (3 au plus)");
      else if (!inside(m.puzzleX, m.puzzleY) || !tileWalkable(m.rows[m.puzzleY][m.puzzleX])) E("énigme : case de départ " + at(m.puzzleX, m.puzzleY) + " impossible");
      else {
        auto wall = [&](int x, int y) {
          if (!tileWalkable(m.rows[y][x])) return true;
          for (auto& n : m.npcs)
            if (n.x == x && n.y == y) return true;
          for (auto& c : m.chests)
            if (c.x == x && c.y == y) return true;
          for (auto& sg : m.signs)
            if (sg.x == x && sg.y == y) return true;
          for (auto& w : m.warps)
            if (w.x == x && w.y == y) return true;  // un rocher ne va pas sur un passage
          return false;
        };
        if (solvePuzzle(m, m.puzzleX, m.puzzleY, rocks, wall).empty()) E("énigme : aucune solution depuis " + at(m.puzzleX, m.puzzleY));
      }
    }

    // Accessibilité à pied depuis les points d'arrivée de la carte
    std::set<std::pair<int, int>> blocked, boss;
    for (auto& b : m.buildings)
      for (int y = b.y; y < b.y + b.h; y++)
        for (int x = b.x; x < b.x + b.w; x++) blocked.insert({x, y});
    for (auto& n : m.npcs) blocked.insert({n.x, n.y});
    for (auto& c : m.chests) blocked.insert({c.x, c.y});
    for (auto& s : m.signs) blocked.insert({s.x, s.y});
    for (auto& b : m.bosses)
      for (int dy = 0; dy < 2; dy++)
        for (int dx = 0; dx < 2; dx++) boss.insert({b.x + dx, b.y + dy});
    std::vector<std::pair<int, int>> starts;
    if (m.id == r.startMap) starts.push_back({r.startX, r.startY});
    if (m.id == r.respawnMap) starts.push_back({r.respawnX, r.respawnY});
    for (auto& o : MAPS) {
      for (auto& w : o.warps)
        if (w.map == m.id) starts.push_back({w.tx, w.ty});
      int ex, ey;
      for (auto& b : o.buildings)  // intérieur : on y entre par la porte d'un bâtiment
        if (b.interior == m.id && interiorEntry(m, o.id, ex, ey)) starts.push_back({ex, ey});
    }
    if (starts.empty()) continue;  // carte sans entrée : rien à vérifier
    auto bfs = [&](bool passBoss) {
      std::set<std::pair<int, int>> seen;
      std::deque<std::pair<int, int>> q;
      for (auto& s : starts)
        if (inside(s.first, s.second) && seen.insert(s).second) q.push_back(s);
      while (!q.empty()) {
        auto [x, y] = q.front();
        q.pop_front();
        for (auto [dx, dy] : {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
          std::pair<int, int> p{x + dx, y + dy};
          if (!inside(p.first, p.second) || seen.count(p) || blocked.count(p) || (!passBoss && boss.count(p))) continue;
          if (!tileWalkable(m.rows[p.second][p.first])) continue;
          seen.insert(p);
          q.push_back(p);
        }
      }
      return seen;
    };
    auto r0 = bfs(false), r1 = bfs(true);
    auto adj = [](int x, int y, const std::set<std::pair<int, int>>& R) {
      return R.count({x + 1, y}) || R.count({x - 1, y}) || R.count({x, y + 1}) || R.count({x, y - 1});
    };
    for (auto& b : m.buildings)
      if (!b.decor() && !r0.count({b.doorX(), b.doorY() + 1})) E("porte inaccessible : " + b.name + " " + at(b.doorX(), b.doorY()));
    for (auto& n : m.npcs)
      if (!adj(n.x, n.y, r1)) E("habitant inaccessible " + at(n.x, n.y));
    for (auto& c : m.chests)
      if (!adj(c.x, c.y, r1)) E("coffre inaccessible " + at(c.x, c.y));
    for (auto& s : m.signs)
      if (!adj(s.x, s.y, r1)) E("panneau inaccessible " + at(s.x, s.y));
    for (auto& b : m.bosses) {
      bool ok = false;
      for (int dy = 0; dy < 2; dy++)  // les autres boss sont battus avant (r1 : on les traverse)
        for (int dx = 0; dx < 2; dx++) ok = ok || adj(b.x + dx, b.y + dy, r1);
      if (!ok) E("boss inaccessible " + at(b.x, b.y));
    }
    for (auto& w : m.warps)
      if (!r1.count({w.x, w.y}) && !adj(w.x, w.y, r1)) E("passage inaccessible " + at(w.x, w.y));
  }
  return err;
}
