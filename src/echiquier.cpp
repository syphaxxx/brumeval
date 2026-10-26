#include "i18n.hpp"
#include "battle.hpp"

// Échiquier (3.4, demandé le 2026-10-18) : le plateau commun des deux camps. Placement et dessin des cases,
// déplacements (une fois par tour, sans finir le tour), portée du corps à corps, coups dans le dos, encerclement,
// Percée, techniques qui font bondir ou échanger, choix de la case par l'ordinateur, défis à positions imposées.
// Le reste du combat est dans battle.cpp.
#include <algorithm>
#include <cmath>

#include "audio.hpp"
#include "game.hpp"
#include "store.hpp"

static const Color GOLD = rgb(0xffe066);

// Échiquier à l'écran (3.4) : un plateau de 8 colonnes de 32 px, les alliés partent de la gauche, les ennemis de
// la droite. Les lignes du bas sont un peu décalées (léger effet de profondeur).
Pt Battle::cellPos(int c) const {
  if (c < 0 || c >= BOARD_CELLS) c = boardAt(1, 1);
  float col = (float)boardX(c), row = (float)boardY(c);
  return {160 + (col - (BOARD_W - 1) / 2.f) * 32 + (row - 1.5f) * 3, 72 + row * 22};
}

Pt Battle::pos(const FighterP& f) const {
  Pt p = cellPos(cellOf(f));
  auto it = f ? slide_.find(f.get()) : slide_.end();
  if (it != slide_.end()) {  // il glisse depuis son ancienne case (un quart de seconde)
    float k = std::clamp((G.time - it->second.second) / .25f, 0.f, 1.f);
    k = k * k * (3 - 2 * k);
    p.x = it->second.first.x + (p.x - it->second.first.x) * k;
    p.y = it->second.first.y + (p.y - it->second.first.y) * k - std::sin(k * 3.14159f) * 3;  // petit bond
  }
  return p;
}

int Battle::cellOf(const FighterP& f) const {
  auto it = f ? cells_.find(f.get()) : cells_.end();
  return it == cells_.end() ? -1 : it->second;
}

void Battle::place(const std::vector<FighterP>& v, bool keep, bool left) {
  auto cells = formation(v, keep);
  for (size_t i = 0; i < v.size(); i++) cells_[v[i].get()] = cells[i] < 0 ? -1 : toBoard(left, cells[i]);
}

FighterP Battle::at(int c) const {
  if (c < 0) return nullptr;
  for (auto* side : {&allies, &foes})
    for (auto& f : *side)
      if (f->alive() && cellOf(f) == c) return f;
  return nullptr;
}

int Battle::freeCellNear(bool left, int near) const {
  int best = -1, bd = 99;
  for (int c = 0; c < BOARD_CELLS; c++) {
    if (!inHalf(left, c) || at(c) || blockedCell(c)) continue;
    // près de near ; sans near, le plus près de l'avant de ce camp
    int d = near >= 0 ? boardSteps(c, near) * 2 + std::abs(boardY(c) - boardY(near))
                      : std::abs(boardX(c) - (left ? GRID - 1 : GRID)) * 4 + std::abs(boardY(c) * 2 - (GRID - 1));
    if (d < bd) bd = d, best = c;
  }
  if (best < 0)  // sa moitié est pleine (combattants passés de ce côté, obstacles) : la case libre la plus proche ailleurs
    for (int c = 0; c < BOARD_CELLS; c++)
      if (!at(c) && !blockedCell(c)) {
        int d = near >= 0 ? boardSteps(c, near) : std::abs(boardX(c) - (left ? 0 : BOARD_W - 1));
        if (d < bd) bd = d, best = c;
      }
  return best;
}

int Battle::moveRange(const FighterP& f) const { return fighterMoveRange(*f); }

bool Battle::flier(const FighterP& f) const { return speciesFlies(f->S()); }

std::map<int, int> Battle::walkable(const FighterP& f, std::map<int, int>* parent) const {
  std::map<int, int> out;
  int from = cellOf(f), range = moveRange(f);
  if (from < 0 || range <= 0) return out;
  bool fly = flier(f);
  const auto& opp = oppOf(f);
  auto foeOn = [&](int c) {
    for (auto& o : opp)
      if (o->alive() && cellOf(o) == c) return true;
    return false;
  };
  auto contact = [&](int c) {  // arriver au contact d'un adversaire arrête la marche
    for (auto& o : opp)
      if (o->alive() && boardDist(cellOf(o), c) <= 1) return true;
    return false;
  };
  // Recherche en largeur : pas d'un côté, sans diagonale
  std::map<int, int> dist{{from, 0}};
  std::vector<int> todo{from};
  for (size_t i = 0; i < todo.size(); i++) {
    int c = todo[i], d = dist[c];
    if (d >= range) continue;
    if (c != from && !fly && contact(c)) continue;  // au contact : on ne va pas plus loin
    static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
    for (int k = 0; k < 4; k++) {
      int x = boardX(c) + DX[k], y = boardY(c) + DY[k];
      if (x < 0 || x >= BOARD_W || y < 0 || y >= GRID) continue;
      int n = boardAt(y, x);
      if (dist.count(n) || (!fly && (foeOn(n) || blockedCell(n)))) continue;
      dist[n] = d + 1;
      if (parent) (*parent)[n] = c;
      todo.push_back(n);
    }
  }
  for (auto& [c, d] : dist)
    if (c != from && !at(c) && !blockedCell(c)) out[c] = d;  // on traverse un allié, mais on ne s'arrête pas sur lui
  return out;
}

std::vector<int> Battle::pathTo(const FighterP& f, int cell) const {
  std::map<int, int> parent;
  std::vector<int> path;
  if (!walkable(f, &parent).count(cell)) return path;
  for (int c = cell; c != cellOf(f) && parent.count(c); c = parent[c]) path.push_back(c);
  path.push_back(cellOf(f));
  std::reverse(path.begin(), path.end());
  return path;
}

int Battle::reach(const FighterP& a) const { return a->boss ? rules().bossReach : 1; }

bool Battle::engaged(const FighterP& f, int cell) const { return foesAround(f, cell) > 0; }

int Battle::foesAround(const FighterP& d, int cell) const {
  int n = 0, c = cell >= 0 ? cell : cellOf(d);
  for (auto& o : oppOf(d))
    if (o->alive() && boardDist(cellOf(o), c) <= 1) n++;
  return n;
}

bool Battle::behind(const FighterP& a, const FighterP& d, int from) const {
  int ax = boardX(from >= 0 ? from : cellOf(a)), dx = boardX(cellOf(d));
  return ax != dx && (ax - dx) * facingOf(d) < 0;  // du côté opposé à son regard (même colonne : de flanc, pas de dos)
}

FighterP Battle::backReach(const FighterP& a, int cell) const {
  for (auto& e : oppOf(a))
    if (e->alive() && cellOf(e) >= 0 && boardDist(cell, cellOf(e)) <= reach(a) && behind(a, e, cell)) return e;
  return nullptr;
}

int Battle::facingOf(const FighterP& f) const {
  auto it = facing_.find(f.get());
  return it != facing_.end() ? it->second : isAlly(f) ? 1 : -1;
}

void Battle::face(const FighterP& a, int cell) {
  int ac = cellOf(a);
  if (!a || ac < 0 || cell < 0 || boardX(cell) == boardX(ac)) return;
  facing_[a.get()] = boardX(cell) > boardX(ac) ? 1 : -1;
}

float Battle::posMult(const FighterP& a, const FighterP& d, const Move& m, int from) const {
  const Rules& R = rules();
  if (!a || !d || cellOf(a) < 0 || cellOf(d) < 0 || isAlly(a) == isAlly(d)) return 1;
  float k = 1;
  if (m.melee && behind(a, d, from)) k *= float(R.backStab);                // dans le dos
  // Un boss, trop grand pour être encerclé, ne craint pas non plus de lancer ses sorts au contact
  int n = foesAround(d);
  if (from >= 0 && boardDist(from, cellOf(d)) <= 1 && boardDist(cellOf(a), cellOf(d)) > 1) n++;  // il arrive au contact
  if (n >= 2 && !d->boss) k *= 1 + float(R.surround) * (n - 1);              // encerclé
  if (!m.melee && !a->boss && engaged(a, from)) k *= float(R.contact);       // tirer ou lancer un sort au contact
  return k;
}

int Battle::approachCell(const FighterP& a, const FighterP& t) const {
  int best = -1;
  float bs = -1e9f;
  int tc = cellOf(t), r = reach(a);
  for (auto& [c, d] : walkable(a)) {
    if (boardDist(c, tc) > r) continue;
    float s = -d * 2.f;
    if (behind(a, t, c)) s += 6;  // dans son dos (d'après son regard)
    for (auto& o : oppOf(a))
      if (o->alive() && o != t && boardDist(cellOf(o), c) <= 1) s -= 4;  // d'autres adversaires au contact
    if (terrainHurts(a, c)) s -= 10;
    for (auto& w : warnings_)
      if (isAlly(a) && inBoardZone(w.zone, w.center, c)) s -= 20;
    if (s > bs) bs = s, best = c;
  }
  return best;
}

bool Battle::reachable(const FighterP& a, const Move& m, const FighterP& t) const {
  if (!m.melee || m.zone == Area::All || !t || !a) return true;
  int ac = cellOf(a), tc = cellOf(t);
  if (ac < 0 || tc < 0) return true;
  if (boardDist(ac, tc) <= reach(a)) return true;
  if (moved_.count(a.get())) return false;
  return approachCell(a, t) >= 0;
}

std::vector<FighterP> Battle::zoneTargets(const Move& m, const FighterP& center) const {
  std::vector<FighterP> v{center};
  if (!center || m.zone == Area::Single || m.zone == Area::All) return v;
  int c = cellOf(center);
  for (auto& f : sideOf(center))
    if (f != center && f->alive() && inBoardZone(m.zone, c, cellOf(f))) v.push_back(f);
  return v;
}

float Battle::zoneScore(const FighterP& a, const Move& m, const FighterP& center) const {
  float s = 0;
  for (auto& t : zoneTargets(m, center)) s += attackScore(a, m, t) * (t == center ? 1.f : float(rules().zoneSide));
  return s;
}

bool Battle::facesRight(const FighterP& f) const { return facingOf(f) > 0; }

int Battle::nearestFoeSide(const FighterP& f) const {
  int c = cellOf(f), best = 99;
  bool right = isAlly(f);
  for (auto& o : oppOf(f))
    if (o->alive() && cellOf(o) >= 0) {
      int d = boardSteps(c, cellOf(o));
      if (d < best || (d == best && boardX(cellOf(o)) != boardX(c))) best = d, right = boardX(cellOf(o)) > boardX(c) || (boardX(cellOf(o)) == boardX(c) && isAlly(f));
    }
  return right ? 1 : -1;
}

// Les cases voyagent dans les coordonnées de l'hôte : seul l'invité du duel les retourne (à la réception de l'état,
// à l'envoi d'un déplacement), pour voir son équipe à gauche
int Battle::meleeThreat(const FighterP& f, int cell) const {
  int n = 0;
  for (auto& o : oppOf(f)) {
    if (!o->alive() || cellOf(o) < 0) continue;
    bool melee = false;
    for (auto& id : o->techs()) melee = melee || (moveInfo(id).melee && moveInfo(id).damaging());
    // pas jusqu'à une case d'où il frappe (diagonales comprises) : la plus proche dans le carré de rayon reach autour
    int dx = std::abs(boardX(cellOf(o)) - boardX(cell)), dy = std::abs(boardY(cellOf(o)) - boardY(cell)), r = reach(o);
    if (melee && std::max(0, dx - r) + std::max(0, dy - r) <= moveRange(o)) n++;
  }
  return n;
}

int Battle::netCell(int c) const {
  if (net_ != Net::Guest || c < 0 || c >= BOARD_CELLS) return c;
  return boardAt(boardY(c), BOARD_W - 1 - boardX(c));
}

// Déplacement d'un combattant : il glisse vers sa case. Percée (3.4) : la première fois qu'il atteint le bord du
// camp adverse, il gagne +1 en Attaque, Magie et Vitesse (règle damier.percee).
void Battle::step(const FighterP& a, int cell) {
  if (cell < 0 || cell == cellOf(a)) return;
  if (shown()) slide_[a.get()] = {pos(a), G.time};
  face(a, cell);  // il regarde où il va
  cells_[a.get()] = cell;
  moved_.insert(a.get());
  sound("pas");
  log.push_back(a->name() + T(" va en ") + boardName(cell));
  int edge = isAlly(a) ? BOARD_W - 1 : 0;
  int bonus = rules().breakthrough;
  if (bonus > 0 && boardX(cell) == edge && breakthrough_.insert(a.get()).second) {
    for (int st : {S_ATK, S_MAG, S_SPD}) a->stage[(size_t)st] = std::min(3, a->stage[(size_t)st] + bonus);
    pop(a, T("Percée !"), GOLD);
    bursts_.push_back({a, "percee", G.time});
    sound("bonus");
    log.push_back("  " + a->name() + T(" : Percée"));
    if (isAlly(a)) tip("percee");
  }
}

// ---------------------------------------------------------------------------
// Échiquier (3.4) : où l'ordinateur va avant d'agir
// ---------------------------------------------------------------------------
// Intérêt d'une case (sans hasard) : la meilleure attaque possible de là (dos, encerclement, distance au contact
// compris), moins le danger (adversaires qui pourront le frapper au prochain tour), le terrain et les zones
// annoncées ; un lanceur de sorts garde ses distances, un combattant s'approche, un protecteur reste près des siens.
float Battle::posValue(const FighterP& a, int cell) {
  int keep = cellOf(a);
  bool had = moved_.count(a.get()) > 0;
  cells_[a.get()] = cell;
  moved_.insert(a.get());  // de là, seulement ce qui est au contact (il ne bougera plus)
  const Species& s = a->S();
  auto theirs = alive(oppOf(a));
  float best = 0;
  std::vector<std::string> moves = a->techs();
  for (auto& id : a->spells())
    if (a->mp >= moveInfo(id).cost) moves.push_back(id);
  // Lanceur de sorts : seulement s'il lui reste une attaque à distance (sinon, à court de PM, il doit aller au contact,
  // ou les deux camps peuvent s'attendre sans fin)
  bool ranged = false;
  for (auto& id : moves) ranged = ranged || (moveInfo(id).damaging() && !moveInfo(id).melee);
  bool stalled = quietT_ > 40;  // rien ne se passe depuis longtemps : tout le monde va au contact
  // (2026-10-25) un soigneur ennemi qui sait soigner reste aussi à l'abri, derrière les siens
  bool medic = !isAlly(a) && s.combatRole == "soigneur" && hasMove("soin");
  bool caster = (s.base[B_MAG] > s.base[B_ATK] || medic) && (ranged || medic) && !stalled;
  for (auto& id : moves) {
    const Move& m = moveInfo(id);
    if (!m.damaging()) continue;
    if (m.target == Target::AllEnemies || m.zone == Area::All) {
      float sum = 0;
      for (auto& t : theirs) sum += attackScore(a, m, t);
      best = std::max(best, sum * .7f);
    } else
      for (auto& t : theirs)
        if (reachable(a, m, t)) best = std::max(best, zoneScore(a, m, t));
  }
  float v = best;
  // Danger : chaque adversaire qui pourra le rejoindre au corps à corps à son prochain tour
  float hpR = float(a->hp) / std::max(1, a->mhp), fear = stalled ? 0.f : (caster ? 7.f : 3.5f) * (1.6f - hpR);
  int nearest = 99;
  int side = facingOf(a);  // (2026-10-23) son regard à la fin du tour : vers l'adversaire le plus proche de cette case
  for (auto& o : theirs) {
    int d = boardSteps(cellOf(o), cell);
    if (d < nearest && boardX(cellOf(o)) != boardX(cell)) side = boardX(cellOf(o)) > boardX(cell) ? 1 : -1;
    nearest = std::min(nearest, d);
  }
  float backFear = fear * float(rules().backStab - 1) * 3;  // un coup dans le dos fait plus mal : le craindre davantage
  for (auto& o : theirs) {
    int d = boardSteps(cellOf(o), cell);
    if (d > moveRange(o) + reach(o)) continue;
    v -= fear * (o->boss ? 2.f : 1.f);
    if ((boardX(cellOf(o)) - boardX(cell)) * side < 0) v -= backFear;  // il viendrait par derrière
  }
  if (int n = foesAround(a); n >= 2) v -= 8.f * (n - 1);  // encerclé
  if (caster) v += std::min(nearest, 4) * 2.f;            // garder ses distances
  else v -= nearest * 2.5f;                                // aller au contact (blocage : voir quietT_)
  if (s.combatRole == "protecteur")                        // rester près des siens
    for (auto& o : alive(sideOf(a)))
      if (o != a && inBoardZone(Area::Cross, cell, cellOf(o)))  // (2026-10-25) surtout d'un blessé : il le couvre
        v += 6 + (!isAlly(a) && o->hp * 2 < o->mhp && o->S().combatRole != "protecteur" ? 30.f * (1 - float(o->hp) / std::max(1, o->mhp)) : 0.f);
  // (2026-10-26) couvrir : un allié solide se met en travers de la route d'un ennemi qui a annoncé un corps à corps sur un
  // soigneur ou un blessé des siens (les ennemis ne traversent pas les adversaires et s'arrêtent à leur contact)
  if (isAlly(a) && !caster && hpR >= .6f)
    for (auto& o : theirs) {
      auto it = intents_.find(o.get());
      if (it == intents_.end() || it->second.wait || it->second.targets.size() != 1 || !hasMove(it->second.move)) continue;
      const Move& m = moveInfo(it->second.move);
      FighterP tg = it->second.targets[0];
      if (!m.melee || !m.damaging() || !tg || tg == a || !tg->alive() || !isAlly(tg) || (!Battle::medic(tg) && tg->hp * 2 >= tg->mhp)) continue;
      if (boardDist(cellOf(o), cellOf(tg)) > reach(o) && !reachable(o, m, tg)) v += 30;  // de cette case, il lui barre la route
    }
  if (terrainHurts(a, cell)) v -= 25;
  for (auto& w : warnings_)
    if (isAlly(a) && inBoardZone(w.zone, w.center, cell)) v -= 60;
  int edge = isAlly(a) ? BOARD_W - 1 : 0;
  if (rules().breakthrough > 0 && boardX(cell) == edge && !breakthrough_.count(a.get())) v += 10;  // Percée
  cells_[a.get()] = keep;
  if (!had) moved_.erase(a.get());
  return v;
}

int Battle::planMove(const FighterP& a) {
  if (moved_.count(a.get()) || cellOf(a) < 0) return -1;
  float noise = isAlly(a) ? 0.f : .15f;  // les ennemis ne choisissent pas toujours la meilleure case
  float stay = posValue(a, cellOf(a)) * (1 + noise * (frand() * 2 - 1)), best = stay + 2;  // bouger doit valoir la peine
  int to = -1;
  for (auto& [c, d] : walkable(a)) {
    float v = (posValue(a, c) - d * .5f) * (1 + noise * (frand() * 2 - 1));
    if (v > best) best = v, to = c;
  }
  return to;
}

int Battle::leapDest(const FighterP& a, const FighterP& t) const {
  int c = cellOf(t), ac = cellOf(a);
  if (c < 0 || ac < 0) return -1;
  int dx = boardX(c) - boardX(ac), dy = boardY(c) - boardY(ac), x = boardX(c), y = boardY(c);
  if (std::abs(dy) > std::abs(dx)) y += dy > 0 ? 1 : -1;  // dans l'axe principal qui les sépare, de l'autre côté
  else x += dx > 0 ? 1 : dx < 0 ? -1 : isAlly(t) ? -1 : 1;
  if (x < 0 || x >= BOARD_W || y < 0 || y >= GRID) return -1;
  int to = boardAt(y, x);
  return at(to) || blockedCell(to) ? -1 : to;
}

void Battle::motionAfterHit(const FighterP& a, const FighterP& t, const Move& m, std::vector<FighterP>& ko) {
  if (m.motion == "bond") {
    int to = leapDest(a, t);
    if (to < 0) return;
    step(a, to);
    face(a, cellOf(t));  // retombé derrière elle, il se retourne vers sa cible
    sound("bond");
    pop(a, T("Bond !"), rgb(0xd8e0ff));
    bursts_.push_back({a, "poussiere", G.time + .2f});  // quand il touche le sol
  } else if (m.motion == "echange" && t->alive() && !t->boss) {
    int ca = cellOf(a), ct = cellOf(t);
    if (shown()) slide_[t.get()] = {pos(t), G.time};
    cells_[t.get()] = ca;
    step(a, ct);
    face(a, ca);  // toujours face à celui avec qui il a échangé
    sound("echange");
    pop(t, T("Échange !"), rgb(0xd8e0ff));
    bursts_.push_back({a, "echange", G.time}), bursts_.push_back({t, "echange", G.time});
    log.push_back("  " + a->name() + T(" échange de place avec ") + t->name());
    // la cible arrive sur une case pendant le tour d'un autre : son terrain agit aussitôt (comme une poussée)
    if (terrainTurn(t, true) && std::find(ko.begin(), ko.end(), t) == ko.end()) ko.push_back(t);
  }
}

void Battle::moveTo(FighterP a, int cell) {
  G.menus.clear();
  if (follower()) {  // l'invité envoie son ordre ; l'hôte (ou le chef) vérifie et déplace
    send_(net_ == Net::Follow ? leader_ : -1, Json{{"t", "ordre"}, {"i", mine(a)[1]}, {"deplacer", netCell(cell)}});
    actor = nullptr;
    return;
  }
  cursor = nullptr;
  moveFrom_.erase(a.get());  // une seule origine : celle de ce déplacement
  int from = cellOf(a);
  bool broke = breakthrough_.count(a.get()) > 0;
  faceFrom_[a.get()] = facing_.count(a.get()) ? facing_[a.get()] : 0;
  step(a, cell);
  moved_.insert(a.get());
  if (net_ == Net::None && breakthrough_.count(a.get()) == (broke ? 1u : 0u)) moveFrom_[a.get()] = from;  // « Revenir » possible
  sc.wait(.25f);  // le temps de glisser jusqu'à la case
  sc.call([this, a] { resumeTurn(a); });
}

// Après « Se déplacer », le tour continue : son joueur (ou l'ordinateur) choisit maintenant son action
void Battle::resumeTurn(FighterP a) {
  if (!a->alive()) return afterAction(a);  // tombé en chemin (terrain)
  actor = a;
  if (isAlly(a) && coop() && owner(a) != me_) {  // gardien : allié d'un autre joueur
    if (owner(a) < 0) return autoCommand(a);
    remoteTurn_ = a;
    send_(owner(a), Json{{"t", "tour"}, {"i", mine(a)[1]}, {"bouge", true}});
  } else if (isAlly(a)) {
    if (tacticsActive() && tacticTurn(a)) return;
    if (autoPlay) autoCommand(a);
    else command(a);
  } else if (net_ == Net::Host) {  // combattant de l'invité
    remoteTurn_ = a;
    send_(-1, Json{{"t", "tour"}, {"i", mine(a)[1]}, {"bouge", true}});
  } else enemyTurn(a);
}

void Battle::waitTurn(FighterP a) {
  G.menus.clear();
  cursor = nullptr;
  if (follower()) {
    send_(net_ == Net::Follow ? leader_ : -1, Json{{"t", "ordre"}, {"i", mine(a)[1]}, {"attendre", true}});
    actor = nullptr;
    return;
  }
  log.push_back(a->name() + T(" se met en garde"));
  facing_[a.get()] = nearestFoeSide(a);  // face à l'adversaire le plus proche
  guarding_.insert(a.get());             // (2026-10-24) et il encaisse moins jusqu'à son prochain tour
  sound("garde");
  if (isAlly(a) && shown()) tip("garde");
  sc.say(a->name() + T(" se met en garde."), .45f);
  sc.call([this, a] { afterAction(a, float(rules().quickGauge)); });
}

bool challengeSetup(const std::string& id, BattleSetup& s) {
  Json d = readJson("defis.json");
  for (auto& c : d) {
    if (jget<std::string>(c, "id", "") != id) continue;
    auto cell = [](const Json& e) {
      Json p = e.value("case", Json::array({1, 1}));
      return boardAt(std::clamp(p.at(0).get<int>(), 0, GRID - 1), std::clamp(p.at(1).get<int>(), 0, BOARD_W - 1));
    };
    for (auto& e : c.value("allies", Json::array())) {
      s.allies.push_back(makeFighter(e.at("espece").get<std::string>(), jget(e, "niveau", 10)));
      s.allyCells.push_back(cell(e));
    }
    int bonus = std::exchange(foeLevelBonus, 0);  // (2026-10-26) combattants prêtés à niveau fixe : pas de bonus de Nouvelle partie +
    for (auto& e : c.value("ennemis", Json::array())) {
      s.foes.push_back(makeFoe(e, 10));
      s.foeCells.push_back(cell(e));
    }
    foeLevelBonus = bonus;
    for (auto& o : c.value("obstacles", Json::array())) {  // seulement sur le plateau, sur une case libre
      int y = o.at(0).get<int>(), x = o.at(1).get<int>(), b = boardAt(y, x);
      bool free = y >= 0 && y < GRID && x >= 0 && x < BOARD_W;
      for (int t : s.allyCells) free = free && t != b;
      for (int t : s.foeCells) free = free && t != b;
      if (free) s.obstacleCells.push_back(b);
    }
    s.fixedBoard = true;
    s.canCapture = false;
    s.training = true;  // combattants prêtés : jamais de gains pour l'équipe du joueur
    s.theme = (int)themeOf(jget<std::string>(c, "decor", "vallee"));
    s.intro = T("Défi : ") + tr(jget<std::string>(c, "nom", "")) + ". " + tr(jget<std::string>(c, "description", ""));
    return !s.allies.empty() && !s.foes.empty();
  }
  return false;
}
