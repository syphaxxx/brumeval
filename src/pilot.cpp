#include "pilot.hpp"

#include "i18n.hpp"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <queue>

#include "battle.hpp"
#include "events.hpp"
#include "primes.hpp"
#include "tactics.hpp"
#include "game.hpp"
#include "world.hpp"

static const int DX[4] = {0, 0, -1, 1}, DY[4] = {-1, 1, 0, 0};  // UP, DOWN, LEFT, RIGHT (ui.hpp)
static const int BACK[4] = {DOWN, UP, RIGHT, LEFT};              // direction opposée

Pilot::Pilot(Game& g, bool demo) : G(g), demo_(demo) {
  offset_.push_back(0);
  for (auto& m : maps()) offset_.push_back(offset_.back() + m.w() * m.h());
  G.tacticsAuto = true;  // les héros suivent leurs tactiques ; le pilote ne choisit que quand le menu s'ouvre
  loadMemory();
  loadHistory();
  // Profondeur de chaque case de chaque carte : distance à pied du plus proche bâtiment ou passage
  depth_.assign(offset_.back(), 0);
  for (int m = 0; m < (int)maps().size(); m++) {
    const MapDef& M = maps()[m];
    int W = M.w(), H = M.h();
    std::vector<int> d((size_t)W * H, -1), q;
    auto seed = [&](int x, int y) {
      if (x >= 0 && y >= 0 && x < W && y < H && d[(size_t)y * W + x] < 0) d[(size_t)y * W + x] = 0, q.push_back(y * W + x);
    };
    for (auto& b : M.buildings) seed(b.doorX(), b.doorY() + 1);
    for (auto& w : M.warps) seed(w.x, w.y);
    for (size_t k = 0; k < q.size(); k++) {
      int i = q[k], x = i % W, y = i / W;
      for (int s = 0; s < 4; s++) {
        int nx = x + DX[s], ny = y + DY[s];
        if (nx < 0 || ny < 0 || nx >= W || ny >= H || d[(size_t)ny * W + nx] >= 0 || !tileWalkable(M.rows[ny][nx])) continue;
        d[(size_t)ny * W + nx] = d[(size_t)i] + 1;
        q.push_back(ny * W + nx);
      }
    }
    for (int i = 0; i < W * H; i++) depth_[offset_[m] + i] = std::max(0, d[(size_t)i]);
  }
  zoneLo_.assign(offset_.back(), 0);
  for (int m = 0; m < (int)maps().size(); m++)
    for (auto& z : maps()[m].zones)
      for (int y = z.y; y < z.y + z.h && y < maps()[m].h(); y++)
        for (int x = z.x; x < z.x + z.w && x < maps()[m].w(); x++) zoneLo_[(size_t)node(m, x, y)] = std::max(zoneLo_[(size_t)node(m, x, y)], z.lo);
}

int Pilot::fear(int m) const {
  auto it = learned_.find("nature:" + maps()[m].id);
  return it == learned_.end() ? 0 : it->second;
}

// BRUMEVAL_SANS_MEMOIRE=1 : ni mémoire ni historique (banc d'essai : chaque partie part de zéro)
static bool noMemory() { return SDL_getenv("BRUMEVAL_SANS_MEMOIRE") != nullptr; }

void Pilot::loadMemory() {
  if (noMemory()) return;
  std::ifstream f(userFile("pilote_memoire.txt"));
  std::string key;
  int n;
  while (f >> key >> n) learned_[key] = std::clamp(n, 0, 20);
  if (!learned_.empty()) {
    int sum = 0;
    for (auto& [k, v] : learned_) sum += v;
    note("Mémoire des parties précédentes : " + std::to_string(learned_.size()) + " combats difficiles (marge totale " + std::to_string(sum) +
         " niveaux)");
  }
}
void Pilot::saveMemory() const {
  if (noMemory()) return;
  std::string s;
  for (auto& [k, v] : learned_) s += k + " " + std::to_string(v) + "\n";
  writeUserFile("pilote_memoire.txt", s);
}
void Pilot::loadHistory() {
  if (noMemory()) return;
  std::ifstream f(userFile("pilote_parties.txt"));
  Run r;
  while (f >> r.seconds >> r.defeats >> r.level >> r.starter) history_.push_back(r);
  if (history_.empty()) return;
  const Run* best = &history_[0];
  for (auto& h : history_)
    if (h.seconds < best->seconds) best = &h;
  char b[16];
  std::snprintf(b, sizeof b, "%d:%02d:%02d", best->seconds / 3600, best->seconds / 60 % 60, best->seconds % 60);
  note("Partie n°" + std::to_string(history_.size() + 1) + ". Record : " + b + " (" + std::to_string(best->defeats) + " défaite(s), niveau " +
       std::to_string(best->level) + ")");
}

// Partie finie : ajoutée à l'historique ; record battu ?
void Pilot::recordRun() {
  if (noMemory()) return;
  Run r{int(clock), defeats, G.avgLevel(), G.team.size() > 1 ? G.team[1]->sp : std::string("?")};
  bool record = !history_.empty();
  for (auto& h : history_) record = record && r.seconds < h.seconds;
  history_.push_back(r);
  std::string s;
  for (auto& h : history_) s += std::to_string(h.seconds) + " " + std::to_string(h.defeats) + " " + std::to_string(h.level) + " " + h.starter + "\n";
  writeUserFile("pilote_parties.txt", s);
  if (record) note("NOUVEAU RECORD : partie finie en " + hms() + " !");
}

int Pilot::margin(const std::string& key) const {
  auto f = fails_.find(key);
  auto l = learned_.find(key);
  return 2 * (f == fails_.end() ? 0 : f->second) + (l == learned_.end() ? 0 : l->second);
}

Pilot::~Pilot() {
  for (bool& h : G.in.hold) h = false;
  G.tacticsAuto = true;  // il le coupe pour capturer : ne pas le laisser coupé après une reprise en main
}
int Pilot::mapW(int m) const { return maps()[m].w(); }
void Pilot::unpack(int n, int& m, int& x, int& y) const {
  m = int(std::upper_bound(offset_.begin(), offset_.end(), n) - offset_.begin()) - 1;
  int w = mapW(m);
  x = (n - offset_[m]) % w, y = (n - offset_[m]) / w;
}
bool Pilot::blockedAt(int m, int x, int y) const { return G.blockedOn(maps()[m], x, y); }

// Case où l'on arrive en allant vers d depuis (m, x, y) : en marchant, par un passage vers une
// autre carte, ou par la porte d'une maison (en marchant dedans vers le haut)
int Pilot::moveTo(int m, int x, int y, int d, int& cost) const {
  const MapDef& M = maps()[m];
  int nx = x + DX[d], ny = y + DY[d];
  if (nx < 0 || ny < 0 || nx >= M.w() || ny >= M.h()) return -1;
  if (d == UP)
    for (auto& b : M.buildings)
      if (nx == b.doorX() && ny == b.doorY()) {
        int in = mapIndex(b.interior), ex, ey;
        if (in < 0 || !interiorEntry(maps()[in], M.id, ex, ey)) return -1;
        cost = 2;
        return node(in, ex, ey);
      }
  if (blockedAt(m, nx, ny)) return -1;
  // Les hautes herbes coûtent plus cher : on les évite, sauf pour s'entraîner ; d'autant plus loin
  // dans la nature d'une carte où des meutes l'ont déjà battu (mémoire)
  cost = 1;
  size_t here = (size_t)node(m, nx, ny);
  if (watched_.count((int)here)) cost += 500;  // sous le regard d'un dresseur trop fort pour lui
  if (tileEncounter(M.rows[ny][nx])) {
    int over = zoneLo_[here] - G.avgLevel();  // créatures plus fortes que l'équipe : bien plus cher, surtout loin des villes
    cost += 1 + fear(m) * depth_[here] / 8 + (over > 0 ? over * 3 * (10 + depth_[here]) / 10 : 0);
  }
  for (auto& w : M.warps)
    if (w.x == nx && w.y == ny) {
      int t = mapIndex(w.map);
      if (t >= 0) return node(t, w.tx, w.ty);
    }
  return node(m, nx, ny);
}

std::vector<int> Pilot::distFrom(int from) const {
  std::vector<int> d(offset_.back(), INT_MAX);
  using Q = std::pair<int, int>;
  std::priority_queue<Q, std::vector<Q>, std::greater<Q>> q;
  d[from] = 0;
  q.push({0, from});
  while (!q.empty()) {
    auto [du, u] = q.top();
    q.pop();
    if (du != d[u]) continue;
    int m, x, y;
    unpack(u, m, x, y);
    for (int k = 0; k < 4; k++) {
      if (banned_.count((long long)u * 4 + k)) continue;
      int c = 1, v = moveTo(m, x, y, k, c);
      if (v < 0 || du + c >= d[v]) continue;
      d[v] = du + c;
      q.push({d[v], v});
    }
  }
  return d;
}

void Pilot::dijkstra(int from) {
  int n = offset_.back();
  dist_.assign(n, INT_MAX);
  prev_.assign(n, -1);
  prevDir_.assign(n, -1);
  using P = std::pair<int, int>;
  std::priority_queue<P, std::vector<P>, std::greater<P>> q;
  dist_[from] = 0;
  q.push({0, from});
  while (!q.empty()) {
    auto [d, u] = q.top();
    q.pop();
    if (d != dist_[u]) continue;
    int m, x, y;
    unpack(u, m, x, y);
    for (int k = 0; k < 4; k++) {
      if (banned_.count((long long)u * 4 + k)) continue;
      int c = 1, v = moveTo(m, x, y, k, c);
      if (v < 0 || d + c >= dist_[v]) continue;
      dist_[v] = d + c, prev_[v] = u, prevDir_[v] = k;
      q.push({dist_[v], v});
    }
  }
}

// ---------------------------------------------------------------------------
// Ce que ferait un événement maintenant
// ---------------------------------------------------------------------------
static std::vector<std::string> nameList(const Json& v) {
  std::vector<std::string> o;
  if (v.is_string()) o.push_back(v.get<std::string>());
  else if (v.is_array())
    for (auto& x : v) o.push_back(x.get<std::string>());
  return o;
}

void Pilot::inspectList(const Json& list, Info& in, int depth) const {
  for (auto& a : list) {
    std::string k = jget<std::string>(a, "action", "");
    // Entraînement sans récompense (à refaire à volonté) : rien à gagner, ni objectif ni effet
    if (k == "combat" && jget(a, "entrainement", false) && a.value("victoire", Json::array()).empty()) continue;
    // Cartes des gardiens : il joue tant qu'il n'a pas battu ce joueur (et s'il a cinq cartes)
    if (k == "cartes" && (G.has("cartes:" + jget<std::string>(a, "adversaire", "")) || G.cardCollection().size() < 5)) continue;
    if (k == "cartes") in.sig += "c" + std::to_string(G.cardGames_);
    if (k == "pension") continue;  // la pension : il ne s'en sert pas  // perdu : la signature change, il pourra rejouer (6 essais)
    // Seulement parler, attendre ou choisir le point de réveil : aucun effet sur la partie
    if (k != "dire" && k != "attendre" && k != "si" && k != "question" && k != "reveil" && k != "evenement") in.effect = true;
    if (k == "si") {
      bool ok = G.checkCond(a["condition"]);
      in.sig += ok ? '1' : '0';
      inspectList(a.value(ok ? "alors" : "sinon", Json::array()), in, depth);
    } else if (k == "question") inspectList(a.value("oui", Json::array()), in, depth);  // le pilote répond toujours Oui
    else if (k == "combat") {
      if (!in.combat) in.fight = a;
      in.combat = true;
      in.boss = in.boss || jget(a, "boss", false);
      Json foes = a.value("ennemis", Json::array());  // un défi (« defi ») n'a pas de liste d'ennemis
      for (auto& e : foes) in.lvl = std::max(in.lvl, jget(e, "niveau", G.avgLevel()));
    } else if (k == "boutique") {
      for (auto& id : nameList(a["objets"])) in.shop.push_back(id);
    } else if (k == "forge") in.forge = true;
    else if (k == "soigner" && jget(a, "pv", true)) in.heal = true;  // la chapelle ne rend que les PM : pas un soin
    else if (k == "evenement" && depth < 4) {
      const Json* ev = findEvent(jget<std::string>(a, "id", ""));
      if (ev) {
        Info sub = inspect(jget<std::string>(a, "id", ""));
        in.sig += "(" + sub.sig + ")";
        in.combat = in.combat || sub.combat, in.boss = in.boss || sub.boss, in.heal = in.heal || sub.heal;
        in.lvl = std::max(in.lvl, sub.lvl);
        in.shop.insert(in.shop.end(), sub.shop.begin(), sub.shop.end());
        in.forge = in.forge || sub.forge;
        in.effect = in.effect || sub.effect;
      }
    }
  }
}

Pilot::Info Pilot::inspect(const std::string& id) const {
  Info in;
  const Json* ev = findEvent(id);
  if (!ev) return in;
  Json pages = ev->contains("pages") ? (*ev)["pages"] : Json::array({{{"actions", ev->value("actions", Json::array())}}});
  for (size_t p = 0; p < pages.size(); p++)
    if (!pages[p].contains("si") || G.checkCond(pages[p]["si"])) {
      in.sig = "p" + std::to_string(p) + ":";
      inspectList(pages[p].value("actions", Json::array()), in, 0);
      break;
    }
  return in;
}

// Prêt pour ce combat ? Nettement plus fort : oui. Trop faible : non. Entre les deux, il répète le
// combat (rehearse) et y va s'il le gagne assez souvent ; plus prudent après chaque défaite (de cette
// partie, et de la mémoire des précédentes)
bool Pilot::ready(const Info& in, const std::string& key) {
  int avg = G.avgLevel();
  // (2026-10-23) loin au-dessus : d'office, sauf un boss ou un combat déjà perdu, toujours répétés (la Braisaure
  // ancestrale, trois carapaces, se perdait 8 fois de suite avec 12 niveaux d'avance et une équipe sans magie)
  if (avg >= in.lvl + 3 + margin(key) && !in.boss && !fails_.count(key)) return true;
  if (avg < in.lvl - 8 || in.fight.is_null()) return avg >= in.lvl - (in.boss ? 2 : 3) + margin(key);
  std::string sig = teamSignature();
  auto it = chances_.find(key);
  if (it == chances_.end() || it->second.first != sig) {
    int n = in.boss ? 16 : 8;
    float c = prudent(rehearse(in.fight, n, true), n);
    chances_[key] = {sig, c};
    if (SDL_getenv("BRUMEVAL_TRACE")) std::printf("  [%s] répétition : %s, %d %% de victoires (équipe N.%d)\n", hms().c_str(), key.c_str(), int(c * 100 + .5f), avg);
  }
  return chances_[key].second >= wantChance(key);
}

// Estimation prudente : la part de victoires moins sa marge d'erreur (un écart type). Sans elle, à force de
// répéter après chaque petit changement, un tirage chanceux finissait par passer le seuil (et il perdait)
float Pilot::prudent(float c, int n) { return c - std::sqrt(c * (1 - c) / float(n)); }

float Pilot::wantChance(const std::string& key) const {
  auto f = fails_.find(key);
  auto l = learned_.find(key);
  return std::min(.97f, .85f + .05f * (f == fails_.end() ? 0 : f->second) + .03f * (l == learned_.end() ? 0 : l->second));
}

std::string Pilot::teamSignature() const {
  std::string s;
  for (auto& f : G.front()) {
    s += f->sp + ":" + std::to_string(f->lvl);
    for (auto& g : f->gear) s += "," + g;
    s += ";";
  }
  return s;
}

float Pilot::rehearse(const Json& fight, int n, bool fullHealth) {
  // La partie est mise de côté : la simulation joue avec des copies (expérience, objets, or, bestiaire)
  auto keepTeam = G.team, keepReserve = G.reserve;
  auto keepItems = G.items;
  int keepGold = G.gold;
  auto keepSeen = G.seen, keepCaught = G.caught;
  bool keepAuto = G.tacticsAuto;
  Input keepIn = G.in;
  int wins = 0;
  for (int k = 0; k < n; k++) {
    std::map<const Fighter*, FighterP> copyOf;
    std::vector<FighterP> team;
    for (auto& f : keepTeam) {
      auto c = std::make_shared<Fighter>(*f);
      if (fullHealth) c->hp = c->mhp, c->mp = c->mmp;
      copyOf[f.get()] = c;
      team.push_back(c);
    }
    for (auto& c : team) {
      auto o = c->companion.lock();
      if (o && copyOf.count(o.get())) c->companion = copyOf[o.get()];
      else c->companion.reset();
    }
    G.team = team, G.reserve.clear(), G.items = keepItems, G.gold = keepGold;
    BattleSetup s;
    auto make = [&](const Json& e) { return makeFoe(e, G.avgLevel()); };
    if (fight.contains("defi") && fight["defi"].is_string()) challengeSetup(fight["defi"].get<std::string>(), s);  // ses combattants
    for (auto& e : fight.value("ennemis", Json::array())) s.foes.push_back(make(e));
    for (auto& e : fight.value("renforts", Json::array())) s.reserve.push_back(make(e));
    s.ambush = fight.value("embuscade", false);
    if (s.foes.empty()) {  // rien à combattre : gagné d'avance (l'équipe est remise en place après la boucle)
      wins = n;
      break;
    }
    s.boss = jget(fight, "boss", false);
    s.canFlee = false, s.canCapture = false;
    Battle b(G, std::move(s), maps()[G.mapId].theme);
    b.autoPlay = true;
    b.simTactics = true;
    for (float t = 0; t < 600 && !b.finished(); t += .05f) {
      G.in.confirm = true;
      b.update(.05f);
    }
    if (b.finished() && b.result() == BattleResult::Win) wins++;
  }
  G.team = keepTeam, G.reserve = keepReserve, G.items = keepItems, G.gold = keepGold;
  G.seen = keepSeen, G.caught = keepCaught, G.tacticsAuto = keepAuto;
  G.in = keepIn;
  G.menus.clear();
  rehearsals += n;
  return float(wins) / n;
}

// ---------------------------------------------------------------------------
// Objectifs : tout ce qu'il y a à faire en ce moment, sur toutes les cartes
// ---------------------------------------------------------------------------
std::vector<Pilot::Goal> Pilot::goals() {
  std::vector<Goal> out;
  for (int m = 0; m < (int)maps().size(); m++) {
    const MapDef& M = maps()[m];
    auto spot = [&](Goal& g, int x, int y, int face) {
      if (x < 0 || y < 0 || x >= M.w() || y >= M.h() || blockedAt(m, x, y)) return;
      for (auto& w : M.warps)  // se tenir sur un passage ferait changer de carte
        if (w.x == x && w.y == y) return;
      g.spots.push_back({node(m, x, y), face});
    };
    // Les cases autour d'un bloc (habitant, coffre : 1x1 ; boss : 2x2), en le regardant
    auto around = [&](Goal& g, int x, int y, int w, int h) {
      for (int i = 0; i < w; i++) spot(g, x + i, y - 1, DOWN), spot(g, x + i, y + h, UP);
      for (int j = 0; j < h; j++) spot(g, x - 1, y + j, RIGHT), spot(g, x + w, y + j, LEFT);
    };
    // Un événement à lancer : selon ce qu'il ferait maintenant
    auto byEvent = [&](Goal g, const std::string& ev) {
      Info in = inspect(ev);
      g.sig = in.sig;
      bool seen = doneSig_.count(g.key) > 0;
      if (in.combat) {  // pas encore prêt : objectif d'entraînement (seulement s'il est accessible, voir plan)
        bool ok = ready(in, g.key);
        g.fight = in.fight;
        g.need = G.avgLevel() + 1;  // un niveau de plus, puis il répète de nouveau le combat
        g.kind = ok ? Danger : Grind;
        out.push_back(g);
        return;
      }
      if (in.heal) {
        Goal h = g;
        h.kind = Heal;
        out.push_back(h);
      }
      if ((!in.shop.empty() && worthTrip(in.shop)) || (in.forge && forgeWanted() && tries_[g.key + ":forge"] < 20)) {
        Goal s = g;
        s.kind = Shop;
        if (in.forge) tries_[g.key + ":forge"]++;
        out.push_back(s);
      }
      if (!in.effect) return;  // rien qu'une conversation : pas la peine d'y aller
      if ((in.heal || !in.shop.empty() || in.forge) ? !seen : (!seen || doneSig_[g.key] != in.sig) && tries_[g.key] < 6) {
        g.kind = Normal;
        out.push_back(g);
      }
    };
    for (size_t i = 0; i < M.npcs.size(); i++) {
      const Npc& n = M.npcs[i];
      if (!G.npcVisible(n)) continue;
      Goal g;
      g.key = "pnj:" + M.id + ":" + std::to_string(i);
      g.what = (n.event.empty() ? "parler à un habitant" : "parler : " + n.event) + " (" + M.name + ")";
      auto [nx, ny] = G.npcCell(M, i);  // un promeneur : là où il est maintenant
      around(g, nx, ny, 1, 1);
      // Les Échos des légendes (après l'acte 4) : le joueur automatique les laisse de côté pour l'instant, leur difficulté
      // est en cours de réglage (2026-10-22 : il butait sur le quatrième cercle)
      if (n.event == "legendes") continue;
      if (!n.event.empty()) byEvent(g, n.event);  // un simple dialogue (n.lines) n'a aucun effet : il n'y va pas
    }
    for (auto& c : M.chests) {
      Goal g;
      g.key = "coffre:" + M.id + ":" + std::to_string(c.x) + ":" + std::to_string(c.y);  // comme Game::chestFlag
      if (G.has(g.key) || tries_[g.key] >= 3 || !G.chestShown(M, c)) continue;
      g.what = "ouvrir un coffre (" + M.name + ")";
      around(g, c.x, c.y, 1, 1);
      out.push_back(g);
    }
    // Les panneaux ne font que donner un texte : il ne va pas les lire
    if (!G.inExpedition() && bountyWork())  // primes à prendre ou à toucher : au tableau d'annonces le plus proche
      for (int y = 0; y < M.h(); y++)
        for (int x = 0; x < M.w(); x++)
          if (M.rows[y][x] == 'G') {
            Goal g;
            g.key = "primes:" + M.id + ":" + std::to_string(x) + ":" + std::to_string(y);
            if (tries_[g.key] >= 30) continue;
            g.what = "tableau des primes (" + M.name + ")";
            around(g, x, y, 1, 1);
            out.push_back(g);
          }
    for (auto& b : M.buildings) {
      if (!b.interior.empty() || b.event.empty() || b.decor()) continue;  // une maison avec un intérieur : c'est un passage
      Goal g;
      g.key = "porte:" + M.id + ":" + b.name;
      g.what = "porte : " + b.name + " (" + M.name + ")";
      spot(g, b.doorX(), b.doorY() + 1, UP);
      byEvent(g, b.event);
    }
    if (!M.puzzleFlag.empty() && !G.puzzleSolved(M) && tries_["enigme:" + M.id] < 5) {  // énigme (3.0) : aller à son départ
      Goal g;
      g.key = "enigme:" + M.id;
      g.what = "énigme (" + M.name + ")";
      spot(g, M.puzzleX, M.puzzleY, -1);
      out.push_back(g);
    }
    for (auto& b : M.bosses) {
      if (!G.bossAlive(b)) continue;
      Goal g;
      g.key = "boss:" + b.flag;
      g.what = "boss : " + b.id + " (" + M.name + ")";
      around(g, b.x, b.y, 2, 2);
      byEvent(g, b.event);
    }
  }
  return out;
}

// ---------------------------------------------------------------------------
// Choix de l'objectif et du chemin
// ---------------------------------------------------------------------------
void Pilot::plan() {
  needPlan_ = false;
  hasGoal_ = false;
  path_.clear();
  grind_.clear();
  int cur = node(G.mapId, G.px, G.py);
  socketGems(demo_);  // une gemme reçue (boss, coffre) : sertie aussitôt (en démo, par les menus)
  setupTactics(demo_);  // un profil de tactiques pour chaque membre (en démo, par les menus)
  watchSight();
  dijkstra(cur);
  std::vector<Goal> gs = goals();

  int hp = 0, mhp = 0, mp = 0, mmp = 0;
  bool ko = false;
  for (auto& f : G.front()) hp += f->hp, mhp += f->mhp, mp += f->mp, mmp += f->mmp, ko = ko || !f->alive();
  float frac = mhp ? float(hp) / mhp : 1, mpFrac = mmp ? float(mp) / mmp : 1;
  need_ = 0;  // niveau à atteindre pour le combat difficile accessible le plus facile
  for (auto& g : gs)
    if (g.kind == Grind && std::any_of(g.spots.begin(), g.spots.end(), [&](const Spot& s) { return dist_[s.node] < INT_MAX; }))
      need_ = need_ ? std::min(need_, g.need) : g.need;
  bool hurt = ko || frac < (G.front().size() <= 3 ? .6f : .5f);  // petite équipe : plus prudente

  // Le plus proche objectif de ce genre dont le chemin ne traverse pas d'herbes trop fortes pour
  // l'équipe (créatures de plus d'un niveau au-dessus) ; sinon il s'entraînera d'abord (need_)
  int avgLvl = G.avgLevel();
  auto best = [&](Kind k, Goal& out, int& at, int& face) {
    int bd = INT_MAX;
    for (auto& g : gs) {
      if (g.kind != k) continue;
      for (auto& s : g.spots) {
        if (dist_[s.node] == INT_MAX) continue;  // pas encore accessible
        int sm, sx, sy;
        unpack(s.node, sm, sx, sy);
        int d = dist_[s.node] + (k == Normal && sm != G.mapId ? 40 : 0);  // finir la carte en cours d'abord
        if (d >= bd) continue;
        int risk = k == Heal ? 0 : pathRisk(s.node);  // se soigner passe avant tout
        if (risk > avgLvl + 1) {
          need_ = need_ ? std::min(need_, risk - 1) : risk - 1;
          continue;
        }
        bd = d, out = g, at = s.node, face = s.face;
      }
    }
    return bd < INT_MAX;
  };
  Goal g;
  int at = -1, face = -1;
  // Objectifs ordinaires : le circuit (les 10 plus proches, accessibles sans herbes trop fortes)
  auto tour = [&](Goal& out, int& at, int& face) {
    struct C {
      Goal g;
      int node, face, d;
    };
    std::vector<C> cs;
    for (auto& g : gs) {
      if (g.kind != Normal) continue;
      int bn = -1, bf = -1, bd = INT_MAX;
      for (auto& s : g.spots)
        if (dist_[s.node] < bd && pathRisk(s.node) <= avgLvl + 1) bd = dist_[s.node], bn = s.node, bf = s.face;
      if (bn >= 0) cs.push_back({g, bn, bf, bd});
    }
    if (cs.empty()) return false;
    std::sort(cs.begin(), cs.end(), [](const C& a, const C& b) { return a.d < b.d; });
    if (cs.size() > 10) cs.resize(10);
    std::vector<std::string> keys;
    for (auto& c : cs) keys.push_back(c.g.key);
    std::vector<std::string> sorted = keys;
    std::sort(sorted.begin(), sorted.end());
    std::string set;
    for (auto& k : sorted) set += k + "|";
    if (set != tourSet_ || tour_.empty()) {  // nouveau circuit
      size_t n = cs.size();
      std::vector<std::vector<int>> D(n, std::vector<int>(n, 0));
      for (size_t i = 0; i < n; i++) {
        auto d = distFrom(cs[i].node);
        for (size_t j = 0; j < n; j++) D[i][j] = d[cs[j].node] == INT_MAX ? 1 << 20 : d[cs[j].node];
      }
      std::vector<int> order;  // le plus proche d'abord, en partant d'ici
      std::vector<bool> used(n, false);
      int last = -1;
      for (size_t s = 0; s < n; s++) {
        int bi = -1, bv = INT_MAX;
        for (size_t i = 0; i < n; i++)
          if (!used[i] && (last < 0 ? cs[i].d : D[last][i]) < bv) bv = last < 0 ? cs[i].d : D[last][i], bi = (int)i;
        used[bi] = true, order.push_back(bi), last = bi;
      }
      auto len = [&](const std::vector<int>& o) {
        long t = cs[o[0]].d;
        for (size_t i = 1; i < o.size(); i++) t += D[o[i - 1]][o[i]];
        return t;
      };
      for (bool better = true; better;) {  // 2-opt : inverser un morceau du circuit tant que c'est plus court
        better = false;
        for (size_t i = 0; i + 1 < n; i++)
          for (size_t j = i + 1; j < n; j++) {
            auto o = order;
            std::reverse(o.begin() + i, o.begin() + j + 1);
            if (len(o) < len(order)) order = o, better = true;
          }
      }
      tour_.clear();
      for (int i : order) tour_.push_back(cs[(size_t)i].g.key);
      tourSet_ = set;
    }
    for (auto& k : tour_)
      for (auto& c : cs)
        if (c.g.key == k) {
          out = c.g, at = c.node, face = c.face;
          return true;
        }
    return false;
  };
  bool found = (hurt && best(Heal, g, at, face)) || best(Shop, g, at, face) ||
               tour(g, at, face);
  if (!found) {  // une carte encore jamais vue
    int bd = INT_MAX;
    for (int n = 0; n < (int)dist_.size(); n++) {
      if (dist_[n] >= bd) continue;
      int m, x, y;
      unpack(n, m, x, y);
      if (!visited_.count(m)) bd = dist_[n], at = n;
    }
    if (bd < INT_MAX) {
      int m, x, y;
      unpack(at, m, x, y);
      g = Goal{"carte:" + maps()[m].id, "découvrir : " + maps()[m].name, Normal, {}, ""};
      face = -1, found = true;
    }
  }
  if (!found && best(Danger, g, at, face)) {  // combat difficile : en pleine forme d'abord
    found = true;
    // Pas en pleine forme : il répète le combat avec son état actuel ; si c'est assez sûr, il y va
    // quand même (sinon, entre une auberge lointaine et le boss, il ferait des allers-retours sans fin)
    bool goNow = frac >= .98f && mpFrac >= .9f;
    if (!goNow && !g.fight.is_null()) {
      std::string k = g.key + "@" + teamSignature() + "/" + std::to_string(int(frac * 10)) + "/" + std::to_string(int(mpFrac * 10));
      if (!nowChances_.count(k)) nowChances_[k] = prudent(rehearse(g.fight, 16, false), 16);
      goNow = nowChances_[k] >= wantChance(g.key);
    }
    if (!goNow) {  // l'auberge d'abord : PV et PM
      Goal h;
      int hat = -1, hface = -1;
      if (best(Heal, h, hat, hface)) g = h, at = hat, face = hface;
    }
  }
  if (!found && need_ > 0) {
    // Entraînement : les hautes herbes d'une zone à la mesure de l'équipe, jusqu'au niveau voulu
    int avg = G.avgLevel(), bestScore = INT_MIN;
    for (int m = 0; m < (int)maps().size(); m++)
      for (auto& z : maps()[m].zones) {
        if (z.lo > avg + 1) continue;
        int near = INT_MAX, nearNode = -1;
        for (int y = z.y; y < z.y + z.h && y < maps()[m].h(); y++)
          for (int x = z.x; x < z.x + z.w && x < maps()[m].w(); x++)
            if (tileEncounter(maps()[m].rows[y][x]) && dist_[node(m, x, y)] < near) near = dist_[node(m, x, y)], nearNode = node(m, x, y);
        if (nearNode < 0 || near == INT_MAX) continue;
        int score = std::min(z.hi, avg + 2) * 1000 - std::min(near, 999);
        if (score <= bestScore) continue;
        bestScore = score, at = nearNode;
        grind_.clear();
        for (int y = z.y; y < z.y + z.h && y < maps()[m].h(); y++)
          for (int x = z.x; x < z.x + z.w && x < maps()[m].w(); x++)
            // pas trop loin dans une nature qui l'a déjà battu (mémoire) : là où les meutes sont petites
            if (tileEncounter(maps()[m].rows[y][x]) && !blockedAt(m, x, y) &&
                (fear(m) == 0 || depth_[(size_t)node(m, x, y)] <= std::max(6, 40 - 8 * fear(m))))
              grind_.insert(node(m, x, y));
      }
    if (at >= 0) {
      g = Goal{"entrainement", "s'entraîner jusqu'au niveau " + std::to_string(need_), Grind, {}, ""};
      face = -1, found = true;
    }
  }
  if (!found) {  // plus rien à faire : la partie est finie si l'écran de fin a été vu
    if (endings_ > 0) {
      finished = true;
      // Partie finie : la crainte de la nature s'estompe un peu (il est devenu plus fort)
      for (auto it = learned_.begin(); it != learned_.end();)
        if (it->first.rfind("nature:", 0) == 0 && (it->second = it->second * 3 / 4) == 0) it = learned_.erase(it);
        else ++it;
      saveMemory();
      recordRun();
      note("FIN : plus rien à faire, l'histoire est terminée", "termine");
    } else {
      stuck = true;
      note("BLOQUÉ : plus rien à faire, et l'écran de fin n'a pas été atteint", "bloque");
    }
    return;
  }
  goal_ = g;
  hasGoal_ = true;
  paused_ = false;
  target_ = at, targetFace_ = face;
  status = g.what;
  if (SDL_getenv("BRUMEVAL_TRACE")) {  // chaque décision, pour comprendre un blocage
    int m, x, y;
    unpack(at, m, x, y);
    std::printf("  [%s] objectif : %s -> %s (%d, %d), distance %d\n", hms().c_str(), g.what.c_str(), maps()[m].id.c_str(), x, y, dist_[at]);
  }
  for (int v = at; v != cur && prev_[v] >= 0; v = prev_[v]) path_.push_back({prev_[v], v, prevDir_[v]});
  std::reverse(path_.begin(), path_.end());
}

void Pilot::watchSight() {
  watched_.clear();
  for (int m = 0; m < (int)maps().size(); m++) {
    const MapDef& M = maps()[m];
    for (size_t i = 0; i < M.npcs.size(); i++) {
      const Npc& n = M.npcs[i];
      if (n.sight <= 0 || n.event.empty() || !G.npcVisible(n) || (!n.sightUntil.empty() && G.has(n.sightUntil))) continue;
      Info in = inspect(n.event);
      if (!in.combat || ready(in, "pnj:" + M.id + ":" + std::to_string(i))) continue;
      int dx = n.dir == LEFT ? -1 : n.dir == RIGHT ? 1 : 0, dy = n.dir == UP ? -1 : n.dir == DOWN ? 1 : 0;
      for (int k = 1; k <= n.sight; k++) {
        int x = n.x + dx * k, y = n.y + dy * k;
        if (x < 0 || y < 0 || x >= M.w() || y >= M.h() || blockedAt(m, x, y)) break;
        watched_.insert(node(m, x, y));
      }
    }
  }
}

int Pilot::pathRisk(int at) const {
  int risk = 0, cur = node(G.mapId, G.px, G.py);
  for (int v = at; v != cur && v >= 0 && prev_[v] >= 0; v = prev_[v]) {
    int m, x, y;
    unpack(v, m, x, y);
    if (tileEncounter(maps()[m].rows[y][x])) risk = std::max(risk, zoneLo_[(size_t)v]);
  }
  return risk;
}

// Un pas vers l'objectif, ou l'action une fois arrivé
void Pilot::walk(float dt) {
  if (!puzzleMoves_.empty()) {  // énigme : un pas (ou une poussée) après l'autre
    if (G.moving) return;
    int d = puzzleMoves_.front();
    puzzleMoves_.erase(puzzleMoves_.begin());
    G.in.hold[d] = true, G.in.press[d] = true;
    if (puzzleMoves_.empty()) needPlan_ = true;
    return;
  }
  int cur = node(G.mapId, G.px, G.py);
  if (cur == lastNode_) sameT_ += dt;
  else sameT_ = 0, lastNode_ = cur;
  bool there = cur == target_ || (goal_.kind == Grind && grind_.count(cur));
  if (there) {
    if (goal_.kind == Grind) return pace();
    if (goal_.key.rfind("enigme:", 0) == 0 && !G.puzzleSolved(G.M())) {  // résoudre l'énigme (3.0)
      tries_[goal_.key]++;
      G.resetBoulders();  // depuis le départ : la solution vaut pour les rochers à leur place
      puzzleMoves_ = solvePuzzle(G.M(), G.px, G.py, G.boulders_, [this](int x, int y) { return !G.boulderAt(x, y) && G.blocked(x, y); });
      note("Énigme : " + std::to_string(puzzleMoves_.size()) + " pas (" + G.M().name + ")");
      if (puzzleMoves_.empty()) needPlan_ = true;
      return;
    }
    if (targetFace_ < 0) {  // découvrir une carte : y être suffit
      needPlan_ = true;
      return;
    }
    if (G.dir != targetFace_) {  // se tourner (la case devant est occupée : on ne bouge pas)
      G.in.hold[targetFace_] = true;
      return;
    }
    if (demo_ && !paused_) {  // une seule pause, pour voir à qui l'on parle (update remet wait_ à 0 ensuite)
      paused_ = true;
      wait_ = .35f;
      return;
    }
    return interactNow();
  }
  for (auto& s : path_)
    if (s.from == cur) {
      if (sameT_ > 2.5f) {  // ça ne bouge plus : on évite ce passage
        banned_.insert((long long)cur * 4 + s.dir);
        sameT_ = 0;
        needPlan_ = true;
        return;
      }
      G.in.hold[s.dir] = true;
      G.in.press[s.dir] = true;  // nécessaire pour entrer par une porte
      return;
    }
  needPlan_ = true;  // pas sur le chemin prévu (événement, téléportation…)
}

void Pilot::interactNow() {
  G.in.confirm = true;
  paused_ = false;
  doneSig_[goal_.key] = goal_.sig;
  tries_[goal_.key]++;
  if (goal_.kind == Danger) {
    engaged_ = goal_.key;
    engagedWhat_ = goal_.what;
    engagedFought_ = false;
    defeatsAtEngage_ = G.defeats_;
    note("Combat : " + goal_.what + " (équipe N." + std::to_string(G.avgLevel()) + ")");
  }
  hasGoal_ = false;
  needPlan_ = true;
}

// Entraînement : un pas vers une autre case de hautes herbes de la zone
void Pilot::pace() {
  int m = G.mapId;
  auto ok = [&](int nx, int ny) {  // case voisine où marcher (pas un passage : il changerait de carte)
    if (nx < 0 || ny < 0 || nx >= mapW(m) || ny >= maps()[m].h() || blockedAt(m, nx, ny)) return false;
    for (auto& w : maps()[m].warps)
      if (w.x == nx && w.y == ny) return false;
    return true;
  };
  // D'abord une autre case de hautes herbes ; sinon (case isolée) n'importe quelle voisine : on
  // reviendra sur l'herbe au pas suivant, et c'est en y arrivant que les rencontres ont lieu
  for (int pass = 0; pass < 2; pass++)
    for (int k = 0; k < 4; k++) {
      int d = (paceDir_ + k) % 4, nx = G.px + DX[d], ny = G.py + DY[d];
      if (!ok(nx, ny) || (pass == 0 && !grind_.count(node(m, nx, ny)))) continue;
      G.in.hold[d] = true;
      paceDir_ = BACK[d];  // la prochaine fois, revenir
      return;
    }
  needPlan_ = true;
}

// ---------------------------------------------------------------------------
// Menus, dialogues et combats
// ---------------------------------------------------------------------------
// Amène le curseur sur ce libellé (une touche par image), puis valide.
// 0 : absent ou impossible ; 1 : le curseur se déplace ; 2 : Entrée appuyée
int Pilot::pickItem(const std::string& label) {
  Menu& t = G.menus.top();
  int idx = -1;
  auto bar = label.find('|');  // « Titre|libellé » : le premier libellé après ce titre de section (ex. « Arme|(rien) »)
  if (bar != std::string::npos) {
    std::string head = label.substr(0, bar), want = label.substr(bar + 1);
    bool after = false;
    for (int i = 0; i < (int)t.items.size() && idx < 0; i++) {
      if (t.items[i].header) after = t.items[i].label == head;
      else if (after && t.items[i].label == want && t.items[i].enabled) idx = i;
    }
  } else
    for (int i = 0; i < (int)t.items.size(); i++)
      if (t.items[i].label == label && t.items[i].enabled && !t.items[i].header) idx = i;
  if (idx < 0) return 0;
  if (t.sel != idx) {
    if (++menuMoves_ > 40) t.sel = idx;  // au cas où le curseur ne suivrait pas
    else G.in.press[t.sel < idx ? DOWN : UP] = true;
    wait_ = .12f;
    return 1;
  }
  menuMoves_ = 0;
  G.in.confirm = true;
  wait_ = .45f;
  return 2;
}

void Pilot::menuStep() {
  Menu& t = G.menus.top();
  if (!G.askText_.empty()) {  // question Oui/Non : toujours Oui (les combats ne sont lancés que quand l'équipe est prête)
    bool restart = G.askText_.find(T("Nouvelle partie +")) != std::string::npos;  // sauf recommencer toute l'histoire
    if (!pickItem(restart ? T("Non") : T("Oui"))) G.in.confirm = true;
    return;
  }
  if (t.title.rfind(T("Boutique"), 0) == 0) return shopping();
  if (t.title.rfind(T("Forge"), 0) == 0) return forging();
  if (t.title.rfind(T("Primes"), 0) == 0) return bountyStep();
  if (t.items.size() > 1 && t.items[0].label == T("Jouer") && t.items[1].label == T("Choisir ma main")) {  // Cartes des gardiens
    pickItem(T("Jouer"));
    wait_ = .3f;
    return;
  }
  status = "menu : " + t.title;
  if (t.cancelable) G.in.cancel = true;
  else G.in.confirm = true;
  wait_ = .3f;
}

void Pilot::battleStep() {
  Battle& B = *G.battle_;
  status = "combat";
  if (B.sc.busy()) {  // messages du combat
    if (!(demo_ && B.sc.showingMessage() && !B.sc.messageDone())) G.in.confirm = true;
    wait_ = demo_ ? .6f : 0;
    return;
  }
  if (!G.menus.active() || !B.actor || !B.isAlly(B.actor)) return;
  Menu& t = G.menus.top();
  if (G.menus.depth() == 1 && !t.items.empty() && t.items[0].label == T("Attaquer")) {
    // Capturer une créature quand il en manque pour accompagner les héros ; sinon, l'ordinateur choisit
    capture_.clear();
    const ItemDef* lantern = nullptr;
    for (auto& d : allItems())
      if (d.capture > 0 && G.items.count(d.id) && G.items[d.id] > 0 && (!lantern || d.capture > lantern->capture)) lantern = &d;
    if (lantern && B.canCapture && creaturesWanted() > 0 && (int)G.team.size() < rules().maxTeam) {
      float bestCh = .2f;  // pas la peine en dessous (lanterne d'argent sur une créature en pleine forme : 24 %)
      for (auto& f : B.alive(B.foes))
        if (!f->S().human && !f->boss && captureChance(*f, lantern->capture) > bestCh) bestCh = captureChance(*f, lantern->capture), capture_ = f->name();
    }
    if (!capture_.empty()) {
      lantern_ = lantern->name;
      pickItem(T("Objet"));
      return;
    }
    if (shouldFlee()) {
      if (pickItem(T("Fuir")) == 2) flights++;
      return;
    }
    G.menus.clear();
    B.autoCommand(B.actor);
    return;
  }
  if (t.title == T("Objets") && !capture_.empty() && pickItem(lantern_)) return;
  if (t.title == T("Capturer") && !capture_.empty() && pickItem(capture_)) return;
  G.menus.clear();  // menu inattendu : l'ordinateur joue ce tour
  B.autoCommand(B.actor);
}

// Combat de créatures sauvages qui tourne mal (l'équipe a perdu plus des deux tiers de ses PV et
// l'adversaire en garde plus de la moitié) : fuir plutôt que perdre la moitié de son or
bool Pilot::shouldFlee() const {
  const Battle& B = *G.battle_;
  if (!battleWild_ || !B.canFlee || !capture_.empty()) return false;
  if (fleeNow_) return true;
  long ah = 0, am = 0, fh = 0, fm = 0;
  for (auto& a : B.allies) ah += std::max(0, a->hp), am += a->mhp;
  for (auto& f : B.foes) fh += std::max(0, f->hp), fm += f->mhp;
  for (auto& f : B.reserve) fh += f->hp, fm += f->mhp;
  return am > 0 && fm > 0 && ah * 3 < am && fh * 2 > fm;
}

int Pilot::creaturesWanted() const {
  int heroes = 0, creatures = 0;
  for (auto& f : G.team) (f->S().human ? heroes : creatures)++;
  return std::max(2, std::min(3, heroes) + 1) - creatures;
}

// Intérêt d'un équipement pour ce combattant (les mages préfèrent la Magie)
float Pilot::gearScore(const Fighter& f, const ItemDef& d) const {
  bool mage = f.S().base[B_MAG] > f.S().base[B_ATK];
  const float w[N_GEAR_STATS] = {.25f, mage ? .4f : .1f, mage ? .2f : 1.2f, 1, mage ? 1.2f : .2f, 1, 1, .3f, .5f, .5f};
  float s = 0;
  for (int i = 0; i < N_GEAR_STATS; i++) s += d.bonus[i] * w[i];
  return s;
}

// Achats utiles dans ce stock : un meilleur équipement pour chacun, des lanternes s'il faut
// capturer, quelques potions ; dans la limite de l'or
std::vector<std::string> Pilot::wishes(const std::vector<std::string>& stock) const {
  std::vector<std::string> buy;
  int money = G.gold;
  std::map<std::string, int> bag = G.items;
  auto stockUp = [&](auto better, int keep) {
    const ItemDef* pick = nullptr;  // le meilleur qu'il peut payer
    for (auto& id : stock)
      if (item(id).price <= money - 40 && better(item(id), pick)) pick = &item(id);
    if (!pick) return;
    for (int n = bag.count(pick->id) ? bag[pick->id] : 0; n < keep && pick->price <= money - 40; n++) {
      buy.push_back(pick->id);
      bag[pick->id]++;
      money -= pick->price;
    }
  };
  auto heal = [](const ItemDef& d, const ItemDef* p) { return d.healHp > 0 && d.battle && d.field && !d.revive && d.healHp < 999 && (!p || d.healHp > p->healHp); };
  stockUp(heal, 2);  // de quoi se soigner en route, avant une arme
  for (auto& f : G.team)
    for (int s = 0; s < N_GEAR; s++) {
      float have = f->gear[s].empty() ? 0 : gearScore(*f, item(f->gear[s]));
      for (auto& [id, n] : bag)  // un objet déjà dans le sac fera l'affaire
        if (n > 0 && hasItem(id) && item(id).slot == s && canEquip(*f, item(id))) have = std::max(have, gearScore(*f, item(id)));
      std::string pick;
      float best = have + .5f;
      for (auto& id : stock) {
        const ItemDef& d = item(id);
        if (d.slot != s || !canEquip(*f, d) || d.price > money) continue;
        if (gearScore(*f, d) > best) best = gearScore(*f, d), pick = id;
      }
      if (pick.empty()) continue;
      buy.push_back(pick);
      money -= item(pick).price;
    }
  if (creaturesWanted() > 0)
    stockUp([](const ItemDef& d, const ItemDef* p) { return d.capture > 0 && (!p || d.capture > p->capture); }, 5);
  // Le reste de l'or en soins : plus il est riche, plus il en garde (de 4 à 15), et des plumes
  stockUp(heal, std::clamp(G.gold / 400, 4, 15));
  stockUp([](const ItemDef& d, const ItemDef* p) { return d.revive > 0 && (!p || d.revive > p->revive); }, G.gold > 1500 ? 4 : 1);
  return buy;
}

bool Pilot::worthTrip(const std::vector<std::string>& stock) const {
  int heals = 0, lanterns = 0;
  for (auto& [id, n] : G.items)
    if (n > 0 && hasItem(id)) {
      const ItemDef& d = item(id);
      if (d.healHp > 0 && d.field && !d.revive) heals += n;
      if (d.capture > 0) lanterns += n;
    }
  for (auto& id : wishes(stock)) {
    const ItemDef& d = item(id);
    if (d.slot >= 0) return true;                                        // meilleur équipement
    if (d.capture > 0 && lanterns == 0) return true;                     // il faut capturer
    if (d.healHp > 0 && !d.revive && heals < 3) return true;             // presque plus de soins
  }
  return false;
}

void Pilot::shopping() {
  status = "boutique";
  Menu& t = G.menus.top();
  if (!shopPlanned_) {
    std::vector<std::string> stock;
    for (auto& it : t.items)
      for (auto& d : allItems())
        if (d.name == it.label) stock.push_back(d.id);
    buyList_ = wishes(stock);
    shopPlanned_ = true;
  }
  while (!buyList_.empty() && item(buyList_.front()).price > G.gold) buyList_.erase(buyList_.begin());
  if (!buyList_.empty()) {
    if (pickItem(item(buyList_.front()).name) == 2) {
      note("Achat : " + item(buyList_.front()).name);
      purchases++;
      buyList_.erase(buyList_.begin());
    }
    return;
  }
  if (pickItem(T("Quitter")) != 1) {
    shopPlanned_ = false;
    equipBest(demo_);
  }
}

bool Pilot::forgeWanted() const {
  for (auto& f : G.team)
    for (int s : {G_WEAPON, G_ARMOR}) {
      const std::string& id = f->gear[s];
      std::string missing;
      if (id.empty() || !hasItem(id) || !forgeable(item(id))) continue;
      G.forgeCost(item(id), &missing);
      if (missing.empty() && G.gold >= rules().forge[(size_t)item(id).forged].gold + 150) return true;  // garder de quoi acheter des potions
    }
  return false;
}

// Primes : il y a une prime à prendre (offerte) ou à toucher (vaincue)
bool Pilot::bountyWork() const {
  for (auto& b : bounties()) {
    std::string id = jget<std::string>(b, "id", "");
    if (G.has(bountyFlag(id, 1)) && !G.has(bountyFlag(id, 2))) return true;
    if (!G.has(bountyFlag(id, 0)) && (!b.contains("si") || G.checkCond(b["si"]))) return true;
  }
  return false;
}

// Au tableau : il touche les primes vaincues et prend toutes les autres (il n'ira les chasser que prêt, comme un boss)
void Pilot::bountyStep() {
  status = "primes";
  Menu& t = G.menus.top();
  for (auto& it : t.items)
    if (it.enabled && (it.right == T("Prime !") || it.right == T("à prendre"))) {
      if (pickItem(it.label) == 2) note("Prime : " + it.label + " (" + it.right + ")");
      return;
    }
  G.in.cancel = true;
  wait_ = .3f;
}

void Pilot::forging() {
  status = "forge";
  Menu& t = G.menus.top();
  int idx = -1;
  if (forgeWanted())
    for (int i = 0; i < (int)t.items.size() && idx < 0; i++)
      if (t.items[i].enabled && !t.items[i].header && t.items[i].right != "sac") idx = i;  // l'équipement porté
  if (idx < 0) {
    G.in.cancel = true;
    wait_ = .3f;
    return;
  }
  if (t.sel != idx) {
    t.sel = idx;
    wait_ = .12f;
    return;
  }
  note("Forge : " + t.items[idx].label + " (" + t.items[idx].right + ")");
  G.in.confirm = true;
  wait_ = .45f;
}

// Met à chacun le meilleur équipement du sac (comme Game::gearPick) ; en démo, par les menus (visites)
void Pilot::equipBest(bool show) {
  for (auto& f : G.team)
    for (int s = 0; s < N_GEAR; s++) {
      std::string& cur = f->gear[s];
      float have = cur.empty() ? 0 : gearScore(*f, item(cur));
      std::string pick;
      for (auto& d : allItems()) {
        auto it = G.items.find(d.id);
        if (d.slot != s || it == G.items.end() || it->second <= 0 || !canEquip(*f, d)) continue;
        if (gearScore(*f, d) > have) have = gearScore(*f, d), pick = d.id;
      }
      if (pick.empty()) continue;
      if (show) {  // Échap > Équipement > le membre > l'emplacement > l'objet
        visit({T("Équipement"), f->name(), std::string(gearSlotName(s)) + "|" + (cur.empty() ? std::string(T("(rien)")) : item(cur).name), item(pick).name},
              2.f, true);
        continue;
      }
      if (!cur.empty()) G.items[cur]++;
      G.items[pick]--;
      int lostHp = f->mhp - f->hp, lostMp = f->mmp - f->mp;
      cur = pick;
      for (auto& g : trimGems(*f)) G.items[g]++;
      f->recalc();
      f->hp = std::clamp(f->mhp - lostHp, f->alive() ? 1 : 0, f->mhp);
      f->mp = std::clamp(f->mmp - lostMp, 0, f->mmp);
      note("Équipe " + f->name() + " : " + item(pick).name);
    }
  socketGems(show);
}

// Tactiques : le profil qui convient à chacun, d'après ce qu'il sait faire ; refait quand il apprend de quoi soigner ou
// qu'il gagne des types d'attaque
void Pilot::setupTactics(bool show) {
  const auto& profiles = rules().tacticProfiles;
  auto find = [&](const char* id) -> const Rules::TacticProfile* {
    for (auto& p : profiles)
      if (p.id == id) return &p;
    return nullptr;
  };
  for (auto& f : G.team) {
    bool heals = false;
    std::set<Type> types;
    auto known = f->techs(), sp = f->spells();
    known.insert(known.end(), sp.begin(), sp.end());
    for (auto& id : known) {
      const Move& m = moveInfo(id);
      heals = heals || (m.kind == ::Kind::Heal && m.power > 0);
      if (m.damaging()) types.insert(m.type);
    }
    const char* want = heals && f->mag >= f->atk ? "soigneur" : f->S().combatRole == "protecteur" ? "protecteur" : types.size() >= 3 ? "briseur" : "combattant";
    const Rules::TacticProfile* p = find(want);
    std::string key = std::string(want) + ":" + std::to_string(std::min(tacticSlots(f->lvl), (int)(p ? p->rules.size() : 0)));
    if (!p || profileOf_[f.get()] == key) continue;  // refait quand une ligne de plus sert le profil
    profileOf_[f.get()] = key;
    if (show) visit({T("Préparation"), T("Tactiques"), f->name(), T("Profils…"), p->name}, 3.f);
    else f->tactics = profileRules(*p, tacticSlots(f->lvl));
    note("Tactiques : " + f->name() + " → " + p->name);
  }
}

// Gemmes du sac (2.7) : chacune dans le chaton libre où elle sert le plus (combattants d'abord)
void Pilot::socketGems(bool show) {
  auto front = G.front();
  auto worth = [&](const Fighter& f, const ItemDef& g) {
    bool mage = f.mag > f.atk;
    float v = 0;
    if (g.gemBonus[GS_HP] > 0) v += 10;
    if (g.gemBonus[GS_ATK] > 0) v += mage ? 3 : 12;
    if (g.gemBonus[GS_MAG] > 0) v += mage ? 12 : 3;
    if (g.gemSupport == "eveil") v += 9;
    if (g.gemSupport == "vol_vie") v += mage ? 4 : 11;
    if (g.gemSupport == "contre") v += mage ? 2 : 8;
    if (g.gemSupport == "briseur") v += 10;
    if (!g.gemMove.empty() && hasMove(g.gemMove)) {
      auto known = f.techs(), sp = f.spells();
      known.insert(known.end(), sp.begin(), sp.end());
      if (std::find(known.begin(), known.end(), g.gemMove) != known.end()) return 0.f;
      const Move& m = moveInfo(g.gemMove);
      v += m.kind == ::Kind::Heal || m.kind == ::Kind::Status ? 7 : mage ? 9 + m.power / 20.f : 2;
    }
    return v;
  };
  for (bool placed = true; placed;) {
    placed = false;
    float best = 0;
    FighterP who;
    int slot = -1;
    std::string gem;
    for (auto& [id, n] : G.items) {
      if (n <= 0 || !hasItem(id) || !item(id).gem) continue;
      for (auto& f : G.team)
        for (int s = 0; s < N_GEAR; s++) {
          const std::string& gid = f->gear[(size_t)s];
          if (gid.empty() || !hasItem(gid) || (int)f->gems[(size_t)s].size() >= gemSockets(item(gid))) continue;
          bool fights = std::find(front.begin(), front.end(), f) != front.end();
          float v = worth(*f, item(id)) * (fights ? 2 : 1);
          if (v > best) best = v, who = f, slot = s, gem = id;
        }
    }
    if (!who) break;
    if (show) {  // Échap > Équipement > le membre > Gemmes > le chaton vide > la gemme
      visit({T("Équipement"), who->name(), T("Gemmes"), std::string(gearSlotName(slot)) + " : " + T("(vide)"), item(gem).name}, 2.f, true);
      break;  // les suivantes après celle-ci (la visite finit par equipBest)
    }
    G.items[gem]--;
    who->gems[(size_t)slot].push_back(gem);
    int lostHp = who->mhp - who->hp, lostMp = who->mmp - who->mp;
    who->recalc();
    who->hp = std::clamp(who->mhp - lostHp, who->alive() ? 1 : 0, who->mhp);
    who->mp = std::clamp(who->mmp - lostMp, 0, who->mmp);
    note("Sertit " + item(gem).name + " : " + who->name());
    placed = true;
  }
}

// ---------------------------------------------------------------------------
// Journal et progrès
// ---------------------------------------------------------------------------
std::string Pilot::hms() const {
  int s = int(clock);
  char b[16];
  std::snprintf(b, sizeof b, "%d:%02d:%02d", s / 3600, s / 60 % 60, s % 60);
  return b;
}
void Pilot::visit(std::vector<std::string> path, float look, bool gear) {
  for (auto& v : visits_)
    if (v.path == path) return;  // déjà prévue
  visits_.push_back({std::move(path), look, gear});
}

void Pilot::lookAt(std::vector<std::string> path, float look) {
  if (!demo_ || clock - lastLook_ < 60 || visits_.size() > 3) return;
  lastLook_ = clock;
  visit(std::move(path), look);
}

bool Pilot::visitStep() {
  if (visits_.empty()) return false;
  if (!visitOpen_) {
    if (G.menus.active() || G.sc.busy() || G.moving) return false;
    status = "menus";
    G.in.menu = true;  // Échap : le menu de pause
    visitOpen_ = true, visitClosing_ = false, visitStep_ = 0;
    wait_ = .6f;
    return true;
  }
  Visit& v = visits_.front();
  if (visitClosing_) {
    if (G.menus.active() && !G.sc.busy()) {
      G.in.cancel = true;
      wait_ = .25f;
      return true;
    }
    if (G.sc.busy()) return false;
    bool gear = v.gear;
    visits_.pop_front();
    visitOpen_ = false;
    bool more = std::any_of(visits_.begin(), visits_.end(), [](const Visit& w) { return w.gear; });
    if (gear && !more) equipBest(false);  // après la dernière : ce que les menus n'auraient pas fait (libellé introuvable…)
    return true;
  }
  if (!G.menus.active()) {
    visitClosing_ = true;
    return true;
  }
  if (visitStep_ >= v.path.size()) {  // arrivé : le temps de regarder, puis on referme
    std::string where;
    for (auto& p : v.path) where += (where.empty() ? "" : " > ") + p;
    note("Menus : " + where, "menus");
    visitClosing_ = true;
    wait_ = v.look;
    return true;
  }
  int r = pickItem(v.path[visitStep_]);
  if (r == 0) {
    visitClosing_ = true;  // introuvable : on referme
    return true;
  }
  if (r == 1) wait_ = .2f;
  if (r == 2) visitStep_++, wait_ = .8f;
  return true;
}

void Pilot::note(const std::string& s, const std::string& capture) {
  journal.push_back("[" + hms() + "] " + s);
  // Démo : le Journal à chaque nouvelle quête, le Bestiaire après une capture
  if (s.rfind("Drapeau : quete:", 0) == 0 && s.find(":fin") == std::string::npos) lookAt({T("Journal")}, 3.f);
  if (s.rfind("Capture", 0) == 0) lookAt({T("Journal"), T("Bestiaire")}, 3.f);
  if (!capture.empty()) shot = std::to_string(++shots_) + "_" + capture;
}

std::string Pilot::timeReport() const {
  std::vector<std::pair<float, std::string>> v;
  for (auto& [k, t] : timeBy) v.push_back({t, k});
  std::sort(v.rbegin(), v.rend());
  std::string s = "temps par activité :";
  for (auto& [t, k] : v) s += " " + k + " " + std::to_string(int(t / 60)) + " min,";
  s.pop_back();
  return s;
}

// Durée pour un vrai joueur, très approximative (étape « 20 heures ») : le joueur automatique sait toujours où
// aller, ne lit rien et combat avec ses tactiques. Hypothèses : il faut deux fois plus de marche pour trouver son
// chemin, 1,4 fois plus de temps en combat (choisir ses ordres), 12 caractères lus par seconde, +10 % de menus
std::string Pilot::humanEstimate() const {
  auto get = [&](const char* k) {
    auto it = timeBy.find(k);
    return it == timeBy.end() ? 0.f : it->second;
  };
  float walk = get("trajet") * 2 + get("entraînement (marche)") * 1.5f;
  float fight = (get("combat (trajet)") + get("combat difficile") + get("combat (entraînement)")) * 1.4f;
  float read = readChars_ / 12.f;
  float total = (walk + fight + read + get("menus") + get("dialogue")) * 1.1f;
  int m = int(total / 60);
  return "estimation pour un joueur (très approximative) : " + std::to_string(m / 60) + " h " + std::to_string(m % 60) + " min (lecture " +
         std::to_string(int(read / 60)) + " min, marche ×2, combats ×1,4, menus +10 %)";
}

std::string Pilot::summary() const {
  std::string t;
  for (auto& f : G.team) t += (t.empty() ? "" : ", ") + f->name() + " N." + std::to_string(f->lvl);
  return "temps de jeu " + hms() + " · niveau moyen " + std::to_string(G.avgLevel()) + " · " + std::to_string(battles) + " combats, " +
         std::to_string(defeats) + " défaite(s), " + std::to_string(captures) + " capture(s), " + std::to_string(purchases) + " achat(s) · " +
         std::to_string(G.gold) + " or · " + std::to_string(flights) + " fuite(s), " + std::to_string(rehearsals) + " combats répétés · équipe : " + t;
}

// Ce qui a changé depuis l'image précédente : nouvelle carte, recrue, capture, drapeau, défaite
void Pilot::watch() {
  if (G.mode == Mode::Map && !visited_.count(G.mapId)) {
    visited_.insert(G.mapId);
    note("Arrivée : " + maps()[G.mapId].name, "carte_" + maps()[G.mapId].id);
  }
  if (G.team.size() > knownTeam_ && knownTeam_ > 0)
    for (size_t i = 0; i < G.team.size(); i++) {
      bool old = std::find(knownMembers_.begin(), knownMembers_.end(), G.team[i].get()) != knownMembers_.end();
      if (old) continue;
      bool human = G.team[i]->S().human;
      if (!human) captures++;
      note((human ? "Rejoint l'équipe : " : "Capture : ") + G.team[i]->name() + " N." + std::to_string(G.team[i]->lvl), human ? "recrue" : "");
    }
  std::map<const Fighter*, std::string> now;  // évolution : même combattant, autre espèce
  for (auto& f : G.team) {
    auto it = knownSpecies_.find(f.get());
    if (it != knownSpecies_.end() && it->second != f->sp && hasSpecies(it->second))
      note("Évolution : " + species(it->second).name + " devient " + f->name() + " (N." + std::to_string(f->lvl) + ")", "evolution");
    now[f.get()] = f->sp;
  }
  knownSpecies_ = std::move(now);
  if (G.team.size() != knownTeam_) {
    knownTeam_ = G.team.size();
    knownMembers_.clear();
    for (auto& f : G.team) knownMembers_.push_back(f.get());
  }
  for (auto& f : G.flags)
    if (!knownFlags_.count(f)) {
      knownFlags_.insert(f);
      if (f.rfind("coffre:", 0) != 0) note("Drapeau : " + f);
    }
  if (G.defeats_ != knownDefeats_) {
    defeats += G.defeats_ - knownDefeats_;
    knownDefeats_ = G.defeats_;
    note("Défaite contre " + battleFoes_ + (battleWild_ ? " (sauvages, " : " (") + maps()[std::max(0, battleMap_)].name + ", équipe N." +
             std::to_string(G.avgLevel()) + ")", "defaite");
    if (battleWild_ && battleMap_ >= 0 && battleMap_ < (int)maps().size()) {  // une meute : s'en souvenir
      int& f = learned_["nature:" + maps()[battleMap_].id];
      f = std::min(10, f + 1);
      saveMemory();
    }
  }
  long p = (long)G.flags.size() * 100 + (long)visited_.size() * 100 + (long)G.team.size() * 100;
  for (auto& f : G.team) p += f->lvl;
  if (p != progress_) progress_ = p, lastProgressT_ = clock;
  if (clock - lastProgressT_ > 30 * 60 && !stuck) {
    stuck = true;
    note("BLOQUÉ : aucun progrès depuis 30 minutes de jeu (" + maps()[G.mapId].name + ", case " + std::to_string(G.px) + ", " +
             std::to_string(G.py) + ", objectif : " + status + ")",
         "bloque");
  }
}

// ---------------------------------------------------------------------------
void Pilot::update(float dt) {
  clock += dt;
  for (bool& h : G.in.hold) h = false;
  if (over()) return;
  if (G.mode == Mode::Ending) {  // écran de fin : il peut y avoir une suite (régions ouvertes après un boss)
    if (!endingNow_) {  // la capture se prend à cette image : on ne valide qu'à la suivante
      endings_++;
      note("Écran de fin (" + std::to_string(endings_) + ")", "fin" + std::to_string(endings_));
      endingNow_ = true;
      wait_ = demo_ ? 4 : 0;  // démo : le temps de lire
      return;
    }
    if (demo_ && (wait_ -= dt) > 0) return;
    G.in.confirm = true;
    return;
  }
  endingNow_ = false;
  {  // où passe le temps (rapport)
    bool b = G.mode == Mode::Battle;
    const char* what = b ? (battleWild_ && goal_.kind == Grind ? "combat (entraînement)" : battleWild_ ? "combat (trajet)" : "combat difficile")
                       : G.menus.active() ? "menus" : G.cards_.on ? "cartes" : G.sc.busy() ? "dialogue" : goal_.kind == Grind ? "entraînement (marche)" : "trajet";
    timeBy[what] += dt;
    if (!b && G.sc.showingMessage() && G.sc.text() != lastRead_) {  // ce qu'un joueur aurait à lire (estimation)
      lastRead_ = G.sc.text();
      for (unsigned char c : lastRead_) readChars_ += (c & 0xC0) != 0x80;
    }
  }
  watch();
  if (stuck) return;
  bool battle = G.mode == Mode::Battle && G.battle_;
  if (battle && !inBattle_) {  // début d'un combat
    inBattle_ = true;
    battles++;
    battleMap_ = G.mapId;
    battleWild_ = engaged_.empty() && G.battle_->foeName.empty() && !G.battle_->boss;
    fleeNow_ = false;
    if (battleWild_ && G.battle_->canFlee) {
      int top = 0;
      Json fight = {{"ennemis", Json::array()}};
      for (auto& f : G.battle_->foes) {
        top = std::max(top, f->lvl);
        fight["ennemis"].push_back({{"espece", f->sp}, {"niveau", f->lvl}});
      }
      fight["embuscade"] = G.battle_->ambush_;  // une embuscade se répète comme telle
      if (G.battle_->foes.size() >= 3 || top > G.avgLevel() + 1 || G.battle_->ambush_) {
        float c = prudent(rehearse(fight, 8, false), 8);
        fleeNow_ = c < .7f;  // fuir coûte peu, perdre coûte cher (retour, soins, moitié de l'or)
        if (SDL_getenv("BRUMEVAL_TRACE"))
          std::printf("  [%s] meute de %d (N.%d max) : %d %% de victoires%s\n", hms().c_str(), (int)G.battle_->foes.size(), top, int(c * 100 + .5f),
                      fleeNow_ ? ", il fuit" : "");
      }
    }
    battleFoes_.clear();
    for (auto& f : G.battle_->foes) battleFoes_ += (battleFoes_.empty() ? "" : ", ") + f->name() + " N." + std::to_string(f->lvl);
    if (!G.battle_->reserve.empty()) battleFoes_ += " (+" + std::to_string(G.battle_->reserve.size()) + " renforts)";
    // Créatures sauvages et compagnons qui manquent : le mode auto est coupé pour ce combat, afin que
    // le menu de commande s'ouvre et que le pilote puisse lancer une lanterne (battleStep)
    const Battle& B = *G.battle_;
    bool lantern = false;
    for (auto& d : allItems()) lantern = lantern || (d.capture > 0 && G.items.count(d.id) && G.items.at(d.id) > 0);
    G.tacticsAuto = !(lantern && B.canCapture && B.foeName.empty() && creaturesWanted() > 0 && (int)G.team.size() < rules().maxTeam);
  }
  if (battle && !engaged_.empty()) engagedFought_ = true;
  if (!battle && inBattle_) inBattle_ = false, needPlan_ = true, G.tacticsAuto = true;  // fin d'un combat
  // Bilan du combat difficile, une fois les messages lus (après une défaite : « Vous reprenez connaissance… »)
  if (!battle && engagedFought_ && !G.sc.busy() && !G.menus.active()) {
    if (G.defeats_ > defeatsAtEngage_) {
      int n = ++fails_[engaged_];
      if (n >= 8) {
        stuck = true;
        note("BLOQUÉ : " + std::to_string(n) + " défaites contre « " + engagedWhat_ + " », même en s'entraînant", "bloque");
      }
    } else {
      // Mémoire : la marge qu'il a fallu (défaites de cette partie), mêlée à celle des précédentes ;
      // une victoire du premier coup la fait baisser (il s'entraînera moins la prochaine fois)
      int need = 2 * fails_[engaged_], old = learned_.count(engaged_) ? learned_[engaged_] : 0;
      learned_[engaged_] = (need + old + 1) / 2;
      if (learned_[engaged_] == 0) learned_.erase(engaged_);
      saveMemory();
      note("Gagné : " + engagedWhat_, "victoire");
    }
    engaged_.clear();
    engagedFought_ = false;
  }
  if (demo_ && (wait_ -= dt) > 0) return;
  wait_ = 0;
  if (battle && battleWild_ && shouldFlee()) G.tacticsAuto = false;  // le menu de commande doit s'ouvrir : « Fuir »
  if (battle) return battleStep();
  if (G.mode != Mode::Map) return;
  if (demo_) {
    if (seenTeam_ == 0 && !G.team.empty()) visit({T("Préparation"), T("Tactiques"), G.team[0]->name()}, 3.5f);  // au début : ses tactiques
    if (G.team.size() > seenTeam_ && seenTeam_ > 0 && !G.sc.busy()) {  // un nouveau membre : ses tactiques, puis la formation
      visit({T("Préparation"), T("Tactiques"), G.team.back()->name()}, 3.5f);
      visit({T("Préparation"), T("Formation")}, 2.5f);
    }
    seenTeam_ = std::max(seenTeam_, G.team.size());
    if (visitStep()) return;
  }
  if (G.menus.active()) return menuStep();
  if (G.cards_.on && !G.sc.busy()) {  // Cartes des gardiens : le jeu joue pour lui (Game::updateCards)
    status = "cartes";
    return;
  }
  if (G.sc.busy()) {  // dialogue
    status = "dialogue";
    if (demo_ && G.sc.showingMessage() && !G.sc.messageDone()) return;
    G.in.confirm = true;
    wait_ = demo_ ? 1.1f : 0;
    needPlan_ = true;
    return;
  }
  if (G.moving) return;
  if (needPlan_ || !hasGoal_) plan();
  if (hasGoal_) walk(dt);
}

// ---------------------------------------------------------------------------
// Game : lancer le joueur automatique, et la partie rapide (brumeval --partie DOSSIER)
// ---------------------------------------------------------------------------
void Game::startPilot(bool demo) {
  if (saveBeforePilot_.empty()) saveBeforePilot_ = saveName_;
  saveName_ = demo ? "sauvegarde_demo.txt" : "sauvegarde_partie_auto.txt";  // jamais la vraie sauvegarde
  menus.clear();
  const char* st = SDL_getenv("BRUMEVAL_COMPAGNON");  // premier compagnon (sinon le premier de la liste)
  std::string starter = st && hasSpecies(st) ? st : rules().starters.at(0);
  newGame(starter);
  pilot_ = std::make_unique<Pilot>(*this, demo);
  demoPaused_ = false;
  if (demo) notice("Démo : Espace pour une pause, Échap pour quitter, F2 pour aller plus vite.");
}

int Game::autoGame(SDL_Surface* target, const std::string& out) {
  std::filesystem::create_directories(out);
  g.checkLayout = true;
  startPilot(SDL_getenv("BRUMEVAL_DEMO") != nullptr);  // BRUMEVAL_DEMO=1 : le joueur de la démo (menus visités, pauses), sans fenêtre
  Pilot& P = *pilot_;
  std::ofstream rep(out + "/rapport.txt");
  auto say = [&](const std::string& s) {
    std::printf("%s\n", s.c_str());
    rep << s << "\n";
  };
  say("Partie automatique de Brumeval (premier compagnon : " + team.at(1)->name() + ")");
  const float dt = 1 / 30.f;
  const float limit = 14 * 3600;  // quatorze heures de jeu au plus (rééquilibrage du 2026-10-23 : le jeu est plus long)
  size_t shown = 0;
  for (long i = 0; !P.over() && P.clock < limit; i++) {
    update(dt);
    while (shown < P.journal.size()) say(P.journal[shown++]);
    if (!P.shot.empty() || i % 20 == 0) {
      draw();
      if (!P.shot.empty()) {
        SDL_RenderPresent(g.renderer());
        std::string p = out + "/" + P.shot + ".bmp";
        SDL_SaveBMP(target, p.c_str());
        g.layoutScene = P.shot;
        P.shot.clear();
      }
    }
    if (i % (30 * 600) == 0) say("  … " + P.summary());  // toutes les dix minutes de jeu
  }
  while (shown < P.journal.size()) say(P.journal[shown++]);
  if (!P.over()) say("ARRÊT : plus de quatorze heures de jeu sans finir");
  say("");
  // (2026-10-23) les quatre actes finis suffisent (les donjons, primes et épreuves d'après la fin sont en plus) : depuis le
  // rééquilibrage, tout faire prend plus que la limite sur une machine de GitHub
  bool acts = has("echos_fin");
  say(P.finished ? "RÉSULTAT : histoire terminée" : acts ? "RÉSULTAT : histoire terminée (les quatre actes ; pas tout le contenu d'après la fin)" : "RÉSULTAT : histoire NON terminée");
  say(P.summary());
  say(P.timeReport());
  say(P.humanEstimate());
  say("exploits : " + std::to_string(exploitsDone()) + "/" + std::to_string(exploitCount()));
  for (auto& s : g.layoutIssues) say("mise en page : " + s);
  return (P.finished || acts) && g.layoutIssues.empty() ? 0 : 1;
}
