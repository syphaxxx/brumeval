// Expédition à plusieurs (voir online.hpp) : l'hôte lance une graine, chacun joue sa
// propre expédition dans ce monde (expedition.hpp) et envoie aux autres où il en est
// (message « ou » : région, case, héros, gardien vaincu, en combat, prêt). Devant le
// gardien, on attend tous ceux qui ne l'ont pas encore battu ; l'hôte lance alors le
// combat (« gardien ») chez chacun, le premier du groupe le calcule (Battle, Net::Lead).
#include <algorithm>
#include <cmath>

#include "battle.hpp"
#include "events.hpp"
#include "expedition.hpp"
#include "game.hpp"
#include "mapedit.hpp"
#include "online.hpp"

Expedition& Online::expedition() {
  if (!G.expedition_) G.expedition_ = std::make_unique<Expedition>(G);
  G.expedition_->groupTag = makeSlug(me_.empty() ? pseudo : me_);  // une sauvegarde par joueur
  return *G.expedition_;
}

// ---------------------------------------------------------------------------
// Départ
// ---------------------------------------------------------------------------
// Hôte : menu « Expédition » du salon
void Online::expeditionMenu(int sel) {
  Expedition& X = expedition();
  uint64_t seed = 0;
  int saved = X.savedRegion(true, &seed);
  Menu m;
  m.title = "Expédition à plusieurs";
  m.x = 20, m.y = 56, m.w = 176, m.rows = 4, m.sel = sel;
  m.items.push_back({"Continuer", saved ? "région " + std::to_string(saved) : "",
                     "Reprendre votre expédition à plusieurs : chacun reprend la sienne, ou part de votre région.", saved > 0,
                     [this, seed] { launch(seed, "", false); }});
  m.items.push_back({"Nouvelle", "", "Un monde neuf, tiré au hasard, pour tout le groupe.", true,
                     [this] { confirmLaunch(procgen::randomSeed(), ""); }});
  m.items.push_back({"Graine…", "", "Écrire une graine (un nombre ou un mot) : la même graine redonne le même monde.", true, [this] {
                       G.editText("Graine : un nombre ou un mot", "", 20, [this](const std::string& s) {
                         if (!s.empty()) confirmLaunch(procgen::seedFromText(s), s);
                       });
                     }});
  m.items.push_back({"Retour", "", "", true, [this] { G.menus.pop(); }});
  if (!saved && sel == 0) m.sel = 1;
  G.menus.push(m);
}

// Une expédition à plusieurs déjà sauvegardée serait remplacée : on demande d'abord
void Online::confirmLaunch(uint64_t seed, const std::string& text) {
  if (expedition().savedRegion(true) <= 0) return launch(seed, text, true);
  Menu m;
  m.title = "Expédition en cours";
  m.x = 20, m.y = 92, m.w = 200, m.rows = 2;
  m.items.push_back({"L'abandonner", "", "Votre expédition à plusieurs sauvegardée sera perdue (sans éclats).", true,
                     [this, seed, text] { launch(seed, text, true); }});
  m.items.push_back({"Garder", "", "", true, [this] { G.menus.pop(); }});
  m.onCancel = m.items.back().act;
  G.menus.push(m);
}

void Online::launch(uint64_t seed, const std::string& text, bool fresh) {
  Expedition& X = expedition();
  if (fresh) X.removeSave(true);
  ended_.clear();
  std::string world = Expedition::worldHash(seed, 1);
  startRun(seed, text, 1, "");
  session_ = Session{true, seed, X.seedText(), world, X.region()};
  send(Json{{"t", "expedition"}, {"graine", std::to_string(seed)}, {"texte", session_.text}, {"monde", world}, {"region", session_.region}});
}

void Online::startRun(uint64_t seed, const std::string& text, int region, const std::string& world) {
  if (!world.empty() && Expedition::worldHash(seed, 1) != world) {
    status_ = "Le monde généré n'est pas le même que chez l'hôte (versions différentes du jeu ?).";
    return salon();
  }
  state_ = State::Expedition;
  screen_ = false;
  ready_ = 0;
  sent_ = Json();
  G.sc.clear();
  expedition().startGroup(seed, text, region);
}

// Déconnexion pendant l'expédition : elle est sauvegardée, les données du jeu reviennent
void Online::leaveRun() {
  Expedition* X = G.expedition_.get();
  if (X && X->active()) {
    if (!X->onScreen() && !G.team.empty()) G.saveGame();
    X->leave();
  }
  ready_ = 0;
}

void Online::runEnded() {
  ended_.insert(session_.seed);
  salon();
}

bool Online::anyoneInRun() const {
  for (auto& p : players_)
    if (p.run) return true;
  return false;
}

// ---------------------------------------------------------------------------
// Où en est chacun
// ---------------------------------------------------------------------------
Json Online::myFighters() const {
  Json a = Json::array();
  for (size_t i = 0; i < G.team.size() && a.size() < 3; i++) {
    const Fighter& f = *G.team[i];
    if (!f.alive()) continue;
    a.push_back(Json{{"rang", (int)i}, {"sp", f.sp}, {"lvl", f.lvl}, {"hp", f.hp}, {"mp", f.mp}, {"lim", (int)f.lim}});
  }
  return a;
}

Json Online::myStatus() const {
  Json s{{"t", "ou"}};
  const Expedition* X = G.expedition_.get();
  bool run = state_ == State::Expedition && X && X->active();
  s["run"] = run;
  if (!run) return s;
  std::string hero;  // vide tant qu'il choisit son héros
  if (!X->onScreen())
    for (auto& f : G.team)
      if (hasSpecies(f->sp) && f->S().human) {  // (mode test : l'autre jeu a pu remettre les données du jeu)
        hero = f->sp;
        break;
      }
  s["r"] = X->region(), s["h"] = hero;
  s["x"] = G.px, s["y"] = G.py, s["d"] = G.dir;
  s["v"] = X->beaten(), s["c"] = G.mode == Mode::Battle, s["p"] = ready_;
  if (ready_) s["m"] = myFighters();
  return s;
}

void Online::syncGroup(float dt) {
  Json s = myStatus();
  if (s != sent_) {
    send(s);
    sent_ = s;
    if (host_) {
      if (session_.on && jget(s, "run", false)) session_.region = std::max(session_.region, jget(s, "r", 1));
      checkGuardians();
    }
  }
  // Les autres joueurs glissent d'une case à l'autre (7,5 cases par seconde, comme le joueur)
  for (auto& p : players_) {
    float dx = p.x - p.sx, dy = p.y - p.sy, d = std::fabs(dx) + std::fabs(dy), step = dt * 7.5f;
    if (d > 2.5f || d <= step) p.sx = (float)p.x, p.sy = (float)p.y;  // arrivé (ou trop loin : on le pose)
    else p.sx += dx / d * step, p.sy += dy / d * step;
  }
}

void Online::onStatus(int from, const Json& m) {
  Player* p = player(from);
  if (!p) return;
  bool wasRun = p->run;
  int oldRegion = p->region;
  p->run = jget(m, "run", false);
  p->region = jget(m, "r", 0);
  p->hero = jget<std::string>(m, "h", "");
  p->x = jget(m, "x", 0), p->y = jget(m, "y", 0), p->dir = jget(m, "d", 0);
  p->beaten = jget(m, "v", false);
  p->fighting = jget(m, "c", false);
  p->ready = jget(m, "p", 0);
  p->fighters = m.value("m", Json::array());
  if (!p->ready) p->launched = 0;
  if (!wasRun || oldRegion != p->region) p->sx = (float)p->x, p->sy = (float)p->y;  // arrivée : pas de glissade
  if (host_) {
    if (session_.on && p->run) session_.region = std::max(session_.region, p->region);  // les nouveaux partiront de là
    checkGuardians();
  }
  if (wasRun != p->run && state_ == State::Salon && screen_ && G.menus.depth() == 1 && !G.editingText()) salon(G.menus.top().sel);
}

std::vector<Online::Avatar> Online::avatars() const {
  std::vector<Avatar> v;
  const Expedition* X = G.expedition_.get();
  if (!inGroup() || !X || !X->active()) return v;
  for (auto& p : players_) {
    if (!p.run || p.hero.empty() || p.region != X->region()) continue;
    bool moving = std::fabs(p.sx - p.x) + std::fabs(p.sy - p.y) > .01f;
    int look = hasSpecies(p.hero) ? species(p.hero).look : 0;
    v.push_back({p.sx, p.sy, p.dir, moving ? 1 + int(G.time * 8) % 2 : 0, look, p.name, p.fighting, p.ready > 0});
  }
  return v;
}

// Menu de pause > Groupe
void Online::groupMenu() {
  G.panelMode_ = 0;  // le résumé de l'équipe laisse la place
  Menu m;
  m.title = "Groupe";
  m.x = 112, m.y = 8, m.w = 200, m.rows = (int)players_.size() + 2;
  auto where = [this](int id) {
    const Player* p = player(id);
    if (!p) return std::string("parti");
    if (!p->run) return std::string("au salon");
    if (p->hero.empty()) return std::string("choisit son héros");
    if (p->fighting) return "en combat (région " + std::to_string(p->region) + ")";
    return "région " + std::to_string(p->region);
  };
  MenuItem me{me_, "région " + std::to_string(expedition().region()), "Vous.", false, nullptr};
  me.shrink = true;
  m.items.push_back(me);
  for (auto& p : players_) {
    int id = p.id;
    MenuItem it{p.name, "", "Le gardien de chaque région se combat avec tous ceux qui ne l'ont pas encore battu.", false, nullptr};
    it.rightFn = [where, id] { return where(id); };
    it.shrink = true;
    m.items.push_back(it);
  }
  m.items.push_back({"Retour", "", "", true, [this] { G.menus.pop(); }});
  m.sel = (int)m.items.size() - 1;
  G.menus.push(m);
}

// ---------------------------------------------------------------------------
// Le gardien, à plusieurs
// ---------------------------------------------------------------------------
std::vector<int> Online::missing(int k) const {
  std::vector<int> v;
  for (auto& p : players_)
    if (p.run && (p.region < k || (p.region == k && !p.beaten))) v.push_back(p.id);
  return v;
}

void Online::guardian(const std::string& event) {
  Expedition& X = expedition();
  int k = X.region();
  if (missing(k).empty()) return G.runEvent(event);  // personne d'autre à attendre : combat comme en solo
  G.sc.say(X.current().bossName + ", gardien de la région, vous barre la route !");
  G.sc.say("Il ne s'affronte qu'à plusieurs : attendez ici le reste du groupe.");
  G.sc.call([this, k] { waitGuardian(k); });
}

void Online::waitGuardian(int k) {
  if (!inGroup() || expedition().region() != k) return;
  ready_ = k;
  guardianWait();
}

std::string Online::waitText(int id) const {
  const Player* p = player(id);
  if (!p) return "parti";
  if (!p->run) return "au salon";
  if (ready_ && p->ready == ready_) return "prêt";
  if (p->hero.empty()) return "choisit son héros";
  if (p->region < ready_) return "région " + std::to_string(p->region);
  if (p->region > ready_ || p->beaten) return "plus loin";
  if (p->fighting) return "en combat";
  return "en route";
}

// Menu d'attente devant le gardien : qui est prêt, qui est en route
void Online::guardianWait() {
  if (!ready_ || G.mode != Mode::Map) return;
  G.menus.clear();
  Menu m;
  m.title = "Gardien : le groupe";
  m.x = 8, m.y = 8, m.w = 214, m.rows = (int)players_.size() + 2;
  const std::string help = "Le combat commence quand tous ceux qui n'ont pas battu ce gardien sont là.";
  MenuItem me{me_, "prêt", help, false, nullptr};
  me.shrink = true;
  m.items.push_back(me);
  for (auto& p : players_) {
    int id = p.id;
    MenuItem it{p.name, "", help, false, nullptr};
    it.rightFn = [this, id] { return waitText(id); };
    it.shrink = true;
    m.items.push_back(it);
  }
  auto stop = [this] {
    ready_ = 0;
    G.menus.clear();
  };
  m.items.push_back({"Ne plus attendre", "", "Repartir explorer : revenez devant le gardien quand vous voulez.", true, stop});
  m.onCancel = stop;
  m.sel = (int)m.items.size() - 1;
  G.menus.push(m);
}

// Hôte : tous ceux qui n'ont pas encore battu un gardien attendent devant lui ?
void Online::checkGuardians() {
  if (!host_) return;
  struct S {
    int id, region, ready;
    bool beaten;
  };
  std::vector<S> all;
  const Expedition* X = G.expedition_.get();
  if (state_ == State::Expedition && X && X->active()) all.push_back({myId_, X->region(), ready_, X->beaten()});
  for (auto& p : players_)
    if (p.run) all.push_back({p.id, p.region, p.launched == p.ready ? 0 : p.ready, p.beaten});
  for (auto& s : all) {
    int k = s.ready;
    if (!k) continue;
    std::vector<int> group;
    bool wait = false;
    for (auto& o : all)
      if (o.ready == k) group.push_back(o.id);
      else if (o.region < k || (o.region == k && !o.beaten)) wait = true;
    if (!wait) return launchGuardian(k, group);
  }
}

// Hôte : le combat commence chez chaque joueur du groupe. Combattants : 3 seul, 2 chacun à deux, 1 chacun à trois ou quatre
void Online::launchGuardian(int k, const std::vector<int>& group) {
  std::vector<int> ids = group;
  std::sort(ids.begin(), ids.end());
  size_t per = ids.size() == 1 ? 3 : ids.size() == 2 ? 2 : 1;
  Json g = Json::array();
  for (int id : ids) {
    Json f = id == myId_ ? myFighters() : player(id)->fighters, keep = Json::array();
    for (size_t i = 0; i < f.size() && keep.size() < per; i++) keep.push_back(f[i]);
    g.push_back(Json{{"id", id}, {"nom", nameOf(id)}, {"membres", keep}});
    if (Player* p = player(id)) p->launched = k;
  }
  Json msg{{"t", "gardien"}, {"r", k}, {"chef", ids[0]}, {"groupe", g}};
  for (int id : ids)
    if (id != myId_) sendTo(id, msg);
  if (std::find(ids.begin(), ids.end(), myId_) != ids.end()) {
    msg["de"] = myId_;
    onGuardian(msg);
  }
}

// La première action « combat » d'un événement (le gardien demande d'abord « Affronter ? »)
static Json findCombat(const Json& j) {
  if (j.is_object() && jget<std::string>(j, "action", "") == "combat") return j;
  if (j.is_object() || j.is_array())
    for (auto& v : j) {  // valeurs d'un objet ou éléments d'une liste
      Json r = findCombat(v);
      if (!r.is_null()) return r;
    }
  return Json();
}

// Chez chaque joueur du groupe : les alliés dans le même ordre (mes combattants sont les vrais,
// ceux des autres des copies que le chef fait vivre), le gardien et ses acolytes plus robustes
// quand il y a plus de trois alliés
void Online::onGuardian(const Json& m) {
  int k = jget(m, "r", 0), chef = jget(m, "chef", -1);
  Json grp = m.value("groupe", Json::array());
  std::vector<int> ids;
  for (auto& e : grp) ids.push_back(jget(e, "id", -1));
  Expedition* X = G.expedition_.get();
  Json fight;
  if (inGroup() && X && X->active() && X->region() == k && !G.battle_ && G.mode == Mode::Map)
    for (auto& b : G.M().bosses)
      if (b.flag == X->current().bossFlag)
        if (const Json* ev = findEvent(b.event)) fight = findCombat(*ev);
  std::vector<FighterP> allies;
  std::vector<int> owners;
  std::map<int, std::string> names;
  for (auto& e : grp) {
    int id = jget(e, "id", -1);
    names[id] = jget<std::string>(e, "nom", "?");
    Json mem = e.value("membres", Json::array());
    for (auto& d : mem) {
      std::string sp = jget<std::string>(d, "sp", "");
      if (allies.size() >= 4 || !hasSpecies(sp)) continue;
      int r = jget(d, "rang", -1);
      FighterP f;
      if (id == myId_ && r >= 0 && r < (int)G.team.size() && G.team[(size_t)r]->sp == sp && G.team[(size_t)r]->alive())
        f = G.team[(size_t)r];
      else {
        f = makeFighter(sp, jget(d, "lvl", 1));
        f->hp = std::clamp(jget(d, "hp", f->mhp), 1, f->mhp);
        f->mp = std::clamp(jget(d, "mp", f->mmp), 0, f->mmp);
        f->lim = (float)std::clamp(jget(d, "lim", 0), 0, 100);
      }
      allies.push_back(f);
      owners.push_back(id);
    }
  }
  if (fight.is_null() || allies.empty()) {  // impossible ici : les autres combattent sans ce joueur
    for (int id : ids)
      if (id != myId_) sendTo(id, Json{{"t", "absent"}});
    ready_ = 0;
    return;
  }
  float scale = std::max(1.f, allies.size() / 3.f);
  auto make = [scale](const Json& e) {
    auto f = makeFighter(jget<std::string>(e, "espece", ""), jget(e, "niveau", 1));
    float mult = jget(e, "pv", 1.f) * scale;
    if (mult != 1.f) f->mhp = std::max(1, int(f->mhp * mult + 1e-4f));
    f->hp = f->mhp;
    f->boss = jget(e, "boss", false);
    return f;
  };
  BattleSetup s;
  s.allies = allies;
  Json foes = fight.value("ennemis", Json::array()), more = fight.value("renforts", Json::array());
  for (auto& e : foes)
    if (hasSpecies(jget<std::string>(e, "espece", ""))) s.foes.push_back(make(e));
  for (auto& e : more)
    if (hasSpecies(jget<std::string>(e, "espece", ""))) s.reserve.push_back(make(e));
  s.boss = true;
  s.canFlee = s.canCapture = false;
  Json win = fight.value("victoire", Json::array());
  ready_ = 0;
  G.sc.clear();
  G.menus.clear();
  battleIds_ = ids;
  G.startBattle(std::move(s), [this, win](BattleResult r) {
    if (r == BattleResult::Win) G.runActions(win);
  });
  G.groupBattle_ = true;
  G.battle_->goCoop(chef == myId_, myId_, chef, owners, names, [this](int to, const Json& msg) {
    if (to >= 0) return sendTo(to, msg);
    for (int id : battleIds_)
      if (id != myId_) sendTo(id, msg);
  });
  stateT_ = 0;
}
