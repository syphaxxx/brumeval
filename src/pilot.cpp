#include "pilot.hpp"

#include <algorithm>
#include <climits>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <queue>

#include "battle.hpp"
#include "events.hpp"
#include "game.hpp"
#include "world.hpp"

static const int DX[4] = {0, 0, -1, 1}, DY[4] = {-1, 1, 0, 0};  // UP, DOWN, LEFT, RIGHT (ui.hpp)
static const int BACK[4] = {DOWN, UP, RIGHT, LEFT};              // direction opposée

Pilot::Pilot(Game& g, bool demo) : G(g), demo_(demo) {
  offset_.push_back(0);
  for (auto& m : maps()) offset_.push_back(offset_.back() + m.w() * m.h());
  G.tacticsAuto = true;  // les héros suivent leurs tactiques ; le pilote ne choisit que quand le menu s'ouvre
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
  // Les hautes herbes coûtent plus cher : on les évite, sauf pour s'entraîner
  cost = 1 + (tileEncounter(M.rows[ny][nx]) ? 3 : 0);
  for (auto& w : M.warps)
    if (w.x == nx && w.y == ny) {
      int t = mapIndex(w.map);
      if (t >= 0) return node(t, w.tx, w.ty);
    }
  return node(m, nx, ny);
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
    if (k == "si") {
      bool ok = G.checkCond(a["condition"]);
      in.sig += ok ? '1' : '0';
      inspectList(a.value(ok ? "alors" : "sinon", Json::array()), in, depth);
    } else if (k == "question") inspectList(a.value("oui", Json::array()), in, depth);  // le pilote répond toujours Oui
    else if (k == "combat") {
      in.combat = true;
      in.boss = in.boss || jget(a, "boss", false);
      for (auto& e : a["ennemis"]) in.lvl = std::max(in.lvl, jget(e, "niveau", G.avgLevel()));
    } else if (k == "boutique") {
      for (auto& id : nameList(a["objets"])) in.shop.push_back(id);
    } else if (k == "soigner" && jget(a, "pv", true)) in.heal = true;  // la chapelle ne rend que les PM : pas un soin
    else if (k == "evenement" && depth < 4) {
      const Json* ev = findEvent(jget<std::string>(a, "id", ""));
      if (ev) {
        Info sub = inspect(jget<std::string>(a, "id", ""));
        in.sig += "(" + sub.sig + ")";
        in.combat = in.combat || sub.combat, in.boss = in.boss || sub.boss, in.heal = in.heal || sub.heal;
        in.lvl = std::max(in.lvl, sub.lvl);
        in.shop.insert(in.shop.end(), sub.shop.begin(), sub.shop.end());
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

// Prêt pour ce combat ? Niveau moyen assez proche du sien ; deux niveaux de plus à chaque défaite
bool Pilot::ready(const Info& in, const std::string& key) const {
  auto f = fails_.find(key);
  int need = in.lvl - (in.boss ? 2 : 3) + 2 * (f == fails_.end() ? 0 : f->second);
  return G.avgLevel() >= need;
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
        auto f = fails_.find(g.key);
        g.need = in.lvl - (in.boss ? 2 : 3) + 2 * (f == fails_.end() ? 0 : f->second);
        g.kind = ready(in, g.key) ? Danger : Grind;
        out.push_back(g);
        return;
      }
      if (in.heal) {
        Goal h = g;
        h.kind = Heal;
        out.push_back(h);
      }
      if (!in.shop.empty() && !wishes(in.shop).empty()) {
        Goal s = g;
        s.kind = Shop;
        out.push_back(s);
      }
      if ((in.heal || !in.shop.empty()) ? !seen : (!seen || doneSig_[g.key] != in.sig) && tries_[g.key] < 6) {
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
      around(g, n.x, n.y, 1, 1);
      if (!n.event.empty()) byEvent(g, n.event);
      else if (!n.lines.empty() && !doneSig_.count(g.key)) out.push_back(g);
    }
    for (auto& c : M.chests) {
      Goal g;
      g.key = "coffre:" + M.id + ":" + std::to_string(c.x) + ":" + std::to_string(c.y);  // comme Game::chestFlag
      if (G.has(g.key) || tries_[g.key] >= 3) continue;
      g.what = "ouvrir un coffre (" + M.name + ")";
      around(g, c.x, c.y, 1, 1);
      out.push_back(g);
    }
    for (auto& s : M.signs) {
      Goal g;
      g.key = "panneau:" + M.id + ":" + std::to_string(s.x) + ":" + std::to_string(s.y);
      if (doneSig_.count(g.key)) continue;
      g.what = "lire un panneau (" + M.name + ")";
      around(g, s.x, s.y, 1, 1);
      out.push_back(g);
    }
    for (auto& b : M.buildings) {
      if (!b.interior.empty() || b.event.empty()) continue;  // une maison avec un intérieur : c'est un passage
      Goal g;
      g.key = "porte:" + M.id + ":" + b.name;
      g.what = "porte : " + b.name + " (" + M.name + ")";
      spot(g, b.doorX(), b.doorY() + 1, UP);
      byEvent(g, b.event);
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
  dijkstra(cur);
  std::vector<Goal> gs = goals();

  int hp = 0, mhp = 0;
  bool ko = false;
  for (auto& f : G.front()) hp += f->hp, mhp += f->mhp, ko = ko || !f->alive();
  float frac = mhp ? float(hp) / mhp : 1;
  bool danger = std::any_of(gs.begin(), gs.end(), [](const Goal& g) { return g.kind == Danger; });
  need_ = 0;  // niveau à atteindre pour le combat difficile accessible le plus facile
  for (auto& g : gs)
    if (g.kind == Grind && std::any_of(g.spots.begin(), g.spots.end(), [&](const Spot& s) { return dist_[s.node] < INT_MAX; }))
      need_ = need_ ? std::min(need_, g.need) : g.need;
  bool hurt = ko || frac < .5f || (danger && frac < .98f);  // toujours soigné avant un combat difficile

  auto best = [&](Kind k, Goal& out, int& at, int& face) {
    int bd = INT_MAX;
    for (auto& g : gs) {
      if (g.kind != k) continue;
      for (auto& s : g.spots)
        if (dist_[s.node] < bd) bd = dist_[s.node], out = g, at = s.node, face = s.face;
    }
    return bd < INT_MAX;
  };
  Goal g;
  int at = -1, face = -1;
  bool found = (hurt && best(Heal, g, at, face)) || best(Shop, g, at, face) || best(Normal, g, at, face);
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
  if (!found) found = best(Danger, g, at, face);
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
            if (tileEncounter(maps()[m].rows[y][x]) && !blockedAt(m, x, y)) grind_.insert(node(m, x, y));
      }
    if (at >= 0) {
      g = Goal{"entrainement", "s'entraîner jusqu'au niveau " + std::to_string(need_), Grind, {}, ""};
      face = -1, found = true;
    }
  }
  if (!found) {  // plus rien à faire : la partie est finie si l'écran de fin a été vu
    if (endings_ > 0) {
      finished = true;
      note("FIN : plus rien à faire, l'histoire est terminée", "termine");
    } else {
      stuck = true;
      note("BLOQUÉ : plus rien à faire, et l'écran de fin n'a pas été atteint", "bloque");
    }
    return;
  }
  goal_ = g;
  hasGoal_ = true;
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

// Un pas vers l'objectif, ou l'action une fois arrivé
void Pilot::walk(float dt) {
  int cur = node(G.mapId, G.px, G.py);
  if (cur == lastNode_) sameT_ += dt;
  else sameT_ = 0, lastNode_ = cur;
  bool there = cur == target_ || (goal_.kind == Grind && grind_.count(cur));
  if (there) {
    if (goal_.kind == Grind) return pace();
    if (targetFace_ < 0) {  // découvrir une carte : y être suffit
      needPlan_ = true;
      return;
    }
    if (G.dir != targetFace_) {  // se tourner (la case devant est occupée : on ne bouge pas)
      G.in.hold[targetFace_] = true;
      return;
    }
    if (demo_ && wait_ <= 0) {
      wait_ = .35f;  // un temps pour voir à qui l'on parle
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
  for (int k = 0; k < 4; k++) {
    int d = (paceDir_ + k) % 4, nx = G.px + DX[d], ny = G.py + DY[d];
    if (!grind_.count(node(m, std::clamp(nx, 0, mapW(m) - 1), std::clamp(ny, 0, maps()[m].h() - 1)))) continue;
    if (nx < 0 || ny < 0 || nx >= mapW(m) || ny >= maps()[m].h() || blockedAt(m, nx, ny)) continue;
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
    if (!pickItem("Oui")) G.in.confirm = true;
    return;
  }
  if (t.title.rfind("Boutique", 0) == 0) return shopping();
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
  if (G.menus.depth() == 1 && !t.items.empty() && t.items[0].label == "Attaquer") {
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
      captureTeam_ = G.team.size();
      pickItem("Objet");
      return;
    }
    G.menus.clear();
    B.autoCommand(B.actor);
    return;
  }
  if (t.title == "Objets" && !capture_.empty() && pickItem(lantern_)) return;
  if (t.title == "Capturer" && !capture_.empty() && pickItem(capture_)) return;
  G.menus.clear();  // menu inattendu : l'ordinateur joue ce tour
  B.autoCommand(B.actor);
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
  auto stockUp = [&](auto better, int keep) {
    const ItemDef* pick = nullptr;
    for (auto& id : stock)
      if (better(item(id), pick)) pick = &item(id);
    if (!pick) return;
    for (int n = bag.count(pick->id) ? bag[pick->id] : 0; n < keep && pick->price <= money - 40; n++) {
      buy.push_back(pick->id);
      money -= pick->price;
    }
  };
  if (creaturesWanted() > 0)
    stockUp([](const ItemDef& d, const ItemDef* p) { return d.capture > 0 && (!p || d.capture > p->capture); }, 5);
  stockUp([](const ItemDef& d, const ItemDef* p) { return d.healHp > 0 && d.battle && !d.revive && (!p || d.healHp > p->healHp); }, 3);
  return buy;
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
  if (pickItem("Quitter") != 1) {
    shopPlanned_ = false;
    equipBest();
  }
}

// Met à chacun le meilleur équipement du sac (comme Game::gearPick)
void Pilot::equipBest() {
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
      if (!cur.empty()) G.items[cur]++;
      G.items[pick]--;
      int lostHp = f->mhp - f->hp, lostMp = f->mmp - f->mp;
      cur = pick;
      f->recalc();
      f->hp = std::clamp(f->mhp - lostHp, f->alive() ? 1 : 0, f->mhp);
      f->mp = std::clamp(f->mmp - lostMp, 0, f->mmp);
      note("Équipe " + f->name() + " : " + item(pick).name);
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
void Pilot::note(const std::string& s, const std::string& capture) {
  journal.push_back("[" + hms() + "] " + s);
  if (!capture.empty()) shot = std::to_string(++shots_) + "_" + capture;
}

std::string Pilot::summary() const {
  std::string t;
  for (auto& f : G.team) t += (t.empty() ? "" : ", ") + f->name() + " N." + std::to_string(f->lvl);
  return "temps de jeu " + hms() + " · niveau moyen " + std::to_string(G.avgLevel()) + " · " + std::to_string(battles) + " combats, " +
         std::to_string(defeats) + " défaite(s), " + std::to_string(captures) + " capture(s), " + std::to_string(purchases) + " achat(s) · " +
         std::to_string(G.gold) + " or · équipe : " + t;
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
    note("Défaite : retour chez la guérisseuse (équipe N." + std::to_string(G.avgLevel()) + ")", "defaite");
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
  watch();
  if (stuck) return;
  bool battle = G.mode == Mode::Battle && G.battle_;
  if (battle && !inBattle_) {  // début d'un combat
    inBattle_ = true;
    battles++;
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
    } else note("Gagné : " + engagedWhat_, "victoire");
    engaged_.clear();
    engagedFought_ = false;
  }
  if (demo_ && (wait_ -= dt) > 0) return;
  wait_ = 0;
  if (battle) return battleStep();
  if (G.mode != Mode::Map) return;
  if (G.menus.active()) return menuStep();
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
  saveName_ = demo ? "sauvegarde_demo.txt" : "sauvegarde_partie_auto.txt";  // jamais la vraie sauvegarde
  menus.clear();
  const char* st = SDL_getenv("BRUMEVAL_COMPAGNON");  // premier compagnon (sinon le premier de la liste)
  std::string starter = st && hasSpecies(st) ? st : rules().starters.at(0);
  newGame(starter);
  pilot_ = std::make_unique<Pilot>(*this, demo);
  if (demo) notice("Démo : le jeu joue tout seul. Une touche pour reprendre la main.");
}

int Game::autoGame(SDL_Surface* target, const std::string& out) {
  std::filesystem::create_directories(out);
  g.checkLayout = true;
  startPilot(false);
  Pilot& P = *pilot_;
  std::ofstream rep(out + "/rapport.txt");
  auto say = [&](const std::string& s) {
    std::printf("%s\n", s.c_str());
    rep << s << "\n";
  };
  say("Partie automatique de Brumeval (premier compagnon : " + team.at(1)->name() + ")");
  const float dt = 1 / 30.f;
  const float limit = 10 * 3600;  // dix heures de jeu au plus
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
  if (!P.over()) say("ARRÊT : plus de dix heures de jeu sans finir");
  say("");
  say(P.finished ? "RÉSULTAT : histoire terminée" : "RÉSULTAT : histoire NON terminée");
  say(P.summary());
  for (auto& s : g.layoutIssues) say("mise en page : " + s);
  return P.finished && g.layoutIssues.empty() ? 0 : 1;
}
