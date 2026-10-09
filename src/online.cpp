#include "online.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "battle.hpp"
#include "expedition.hpp"
#include "game.hpp"
#include "tactics.hpp"
#include "version.hpp"

static const Color WHITE = rgb(0xffffff), GOLD = rgb(0xffd34d), MUTED = rgb(0xaab3d8), GREEN = rgb(0x7dffa8), RED = rgb(0xff8a7a);

Online::Online(Game& g) : G(g) { loadSettings(); }

// ---------------------------------------------------------------------------
// Réglages et équipe
// ---------------------------------------------------------------------------
std::string Online::file(const std::string& name) const { return userFile(name); }
void Online::loadSettings() {
  std::ifstream f(file(settingsName));
  std::string line;
  while (std::getline(f, line)) {
    if (line.rfind("pseudo ", 0) == 0) pseudo = line.substr(7);
    else if (line.rfind("adresse ", 0) == 0) address = line.substr(8);
  }
}
void Online::saveSettings() const { writeUserFile(settingsName, "pseudo " + pseudo + "\nadresse " + address + "\n"); }

std::string Online::dataHash() {
  Fingerprint h;
  h.add(BRUMEVAL_VERSION);
  for (int f = 0; f < N_DATAFILES; f++) h.add(dataDoc(DataFile(f)).dump());
  return h.hex();
}

// Équipe de la partie principale : les trois premiers membres et leurs tactiques
// (sans rien changer à la partie en cours) ; sinon le héros et deux compagnons de départ
std::vector<FighterP> Online::myTeam() const {
  std::vector<FighterP> team;
  std::ifstream f(file(G.saveName_));
  std::string line;
  while (std::getline(f, line)) {
    std::istringstream s(line);
    std::string k;
    s >> k;
    if (k == "membre" && std::count_if(team.begin(), team.end(), [](const FighterP& m) { return m != nullptr; }) >= 3) break;
    readMemberLine(k, s, team);  // même lecture que Game::loadGame
  }
  team.erase(std::remove(team.begin(), team.end(), nullptr), team.end());
  for (auto& m : team) {  // duel à armes égales : sans équipement (l'autre joueur ne connaît que l'espèce et le niveau)
    m->gear = {};
    m->recalc();
  }
  if (team.empty()) {
    const Rules& r = rules();
    if (hasSpecies(r.hero)) team.push_back(makeFighter(r.hero, 10));
    for (size_t i = 0; i < r.starters.size() && team.size() < 3; i++)
      if (hasSpecies(r.starters[i])) team.push_back(makeFighter(r.starters[i], 10));
  }
  for (auto& m : team) m->hp = m->mhp, m->mp = m->mmp;
  return team;
}
void Online::refreshTeam() {
  myView_.clear();
  for (auto& f : myTeam()) myView_.push_back({f->name(), f->lvl});
}

// ---------------------------------------------------------------------------
// Joueurs et connexions
// ---------------------------------------------------------------------------
Online::Peer* Online::peer(int id) {
  for (auto& p : peers_)
    if (p.id == id) return &p;
  return nullptr;
}
Online::Player* Online::player(int id) {
  for (auto& p : players_)
    if (p.id == id) return &p;
  return nullptr;
}
const Online::Player* Online::player(int id) const {
  for (auto& p : players_)
    if (p.id == id) return &p;
  return nullptr;
}
std::string Online::nameOf(int id) const {
  if (id == myId_) return me_;
  const Player* p = player(id);
  return p ? p->name : "?";
}
Online::Player* Online::opponent() { return players_.size() == 1 ? &players_[0] : nullptr; }

Json Online::hello() const {
  Json team = Json::array();
  for (auto& f : myTeam()) team.push_back(Json::array({f->sp, f->lvl}));
  return Json{{"t", "bonjour"}, {"version", BRUMEVAL_VERSION}, {"donnees", hash_}, {"pseudo", pseudo}, {"equipe", team}};
}

// Hôte : chaque message part vers chaque invité concerné. Invité : tout passe par l'hôte,
// qui fait suivre à « a » (un joueur, ou -1 pour tous)
void Online::sendTo(int to, Json m) {
  m["de"] = myId_;
  if (host_) {
    for (auto& p : peers_)
      if (p.greeted && (to == -1 || p.id == to)) p.conn->send(m);
  } else if (!peers_.empty() && to != myId_) {
    if (to != 0) m["a"] = to;
    peers_[0].conn->send(m);
  }
}

Json Online::playersJson() const {
  Json a = Json::array();
  auto team = [](const std::vector<Member>& t) {
    Json e = Json::array();
    for (auto& m : t) e.push_back(Json::array({m.sp, m.lvl}));
    return e;
  };
  std::vector<Member> mine;
  for (auto& f : myTeam()) mine.push_back({f->sp, f->lvl});
  a.push_back(Json{{"id", myId_}, {"nom", me_}, {"equipe", team(mine)}});
  for (auto& p : players_) a.push_back(Json{{"id", p.id}, {"nom", p.name}, {"equipe", team(p.team)}});
  return a;
}

static std::vector<std::pair<std::string, int>> readTeam(const Json& team) {
  std::vector<std::pair<std::string, int>> out;
  for (auto& e : team)
    if (e.is_array() && e.size() == 2 && e[0].is_string() && e[1].is_number_integer() && out.size() < 3)
      out.push_back({e[0].get<std::string>(), std::clamp(e[1].get<int>(), 1, 100)});
  return out;
}

// Invité : la liste des joueurs envoyée par l'hôte (à l'arrivée ou au départ de quelqu'un)
void Online::applyPlayers(const Json& list) {
  std::vector<Player> next;
  bool added = false;
  for (auto& e : list) {
    int id = jget(e, "id", -1);
    if (id == myId_) {
      me_ = jget<std::string>(e, "nom", me_);
      continue;
    }
    Player p;
    if (const Player* old = player(id)) p = *old;
    else added = true;
    p.id = id;
    p.name = jget<std::string>(e, "nom", "Joueur");
    p.team.clear();
    for (auto& [sp, lvl] : readTeam(e.value("equipe", Json::array()))) p.team.push_back({sp, lvl});
    next.push_back(p);
  }
  for (auto& p : players_) {
    bool still = false;
    for (auto& n : next) still = still || n.id == p.id;
    if (!still && G.battle_ && G.groupBattle_) G.battle_->dropPlayer(p.id);
  }
  players_ = next;
  if (added) sent_ = Json();  // les nouveaux venus doivent savoir où j'en suis
  if (state_ == State::Salon && screen_ && G.menus.depth() == 1 && !G.editingText()) salon(G.menus.top().sel);
  if (ready_) guardianWait();
}

// ---------------------------------------------------------------------------
// Menus
// ---------------------------------------------------------------------------
void Online::menu(int sel) {
  refreshTeam();
  screen_ = true;
  state_ = State::Menu;
  G.mode = Mode::Title;
  G.menus.clear();
  Menu m;
  m.title = "Multijoueur";
  m.x = 8, m.y = 40, m.w = 150, m.rows = 5, m.sel = sel;
  m.items.push_back({"Héberger une partie", "",
                     "Attendre ses amis (trois au plus) : donnez-leur votre adresse. Puis duel ou expédition à plusieurs.", true,
                     [this] { host(); }});
  m.items.push_back({"Rejoindre une partie", "", "Se connecter à l'ordinateur d'un ami avec son adresse.", true, [this] {
                       G.editText("Adresse de l'hôte (ex. 100.101.102.103)", address, 60, [this](const std::string& a) {
                         if (a.empty()) return;
                         address = a;
                         saveSettings();
                         join(a);
                       });
                     }});
  {
    MenuItem it{"Pseudo", "", "Le nom que voient les autres joueurs.", true, [this] {
                  G.editText("Votre pseudo", pseudo, 12, [this](const std::string& p) {
                    if (p.empty()) return;
                    pseudo = p;
                    saveSettings();
                    menu(2);
                  });
                }};
    it.rightFn = [this] { return pseudo; };
    m.items.push_back(it);
  }
  m.items.push_back({"Retour", "", "", true, [this] {
                       leave();
                       G.titleMenu();
                     }});
  m.onCancel = m.items.back().act;
  G.menus.push(m);
}

void Online::waitMenu(const std::string& title, const std::string& help) {
  G.menus.clear();
  Menu m;
  m.title = title;
  m.x = 8, m.y = 40, m.w = 150, m.rows = 1;
  m.items.push_back({"Annuler", "", help, true, [this] {
                       disconnect("");
                       menu();
                     }});
  m.onCancel = m.items.back().act;
  G.menus.push(m);
}

void Online::host() {
  std::string err;
  disconnect("");
  hash_ = dataHash();
  if (!server_.listen(port, err, loopback)) {
    status_ = err;
    return menu(0);
  }
  host_ = true;
  myId_ = 0;
  me_ = pseudo;
  ended_.clear();
  addresses_ = net::localAddresses();
  status_.clear();
  salon();
}

void Online::join(const std::string& addr) {
  std::string err;
  disconnect("");
  hash_ = dataHash();
  auto c = net::Conn::connect(addr, port, err);
  if (!c) {
    status_ = err;
    return menu(1);
  }
  host_ = false;
  me_ = pseudo;
  ended_.clear();
  Peer p;
  p.conn = std::move(c);
  p.id = 0;  // l'hôte
  peers_.push_back(std::move(p));
  state_ = State::Joining;
  status_.clear();
  waitMenu("Connexion…", "Connexion à " + addr + ", port " + std::to_string(port) + ".");
}

void Online::disconnect(const std::string& why) {
  for (auto& p : peers_) {
    if (p.conn->connected()) p.conn->send(Json{{"t", "quitte"}});
    p.conn->poll();  // envoie ce qui reste
    p.conn->close();
  }
  peers_.clear();
  server_.close();
  players_.clear();
  host_ = false;
  sameLevel_ = false;
  myId_ = -1;
  session_ = Session{};
  ready_ = 0;
  sent_ = Json();
  battleIds_.clear();
  trade_ = Trade{};
  if (!why.empty()) status_ = why;
}

void Online::leave() {
  if (state_ == State::Expedition) leaveRun();
  disconnect("");
  screen_ = false;
  state_ = State::Menu;
}

// Salon : les joueurs sont connectés ; l'hôte lance un duel ou une expédition
void Online::salon(int sel) {
  refreshTeam();
  // Mon équipe pour un duel : celle de ma partie principale (pas celle d'une expédition qui vient de finir)
  Json team = Json::array();
  for (auto& f : myTeam()) team.push_back(Json::array({f->sp, f->lvl}));
  send(Json{{"t", "equipe"}, {"equipe", team}});
  state_ = State::Salon;
  screen_ = true;
  ready_ = 0;
  G.mode = Mode::Title;
  G.menus.clear();
  Menu m;
  int n = (int)players_.size() + 1;
  m.title = "Salon : " + std::to_string(n) + (n > 1 ? " joueurs" : " joueur");
  m.x = 8, m.y = 40, m.w = 150, m.rows = 4, m.sel = sel;
  if (host_) {
    bool busy = anyoneInRun();
    m.items.push_back({"Expédition", "",
                       busy ? "Des joueurs sont encore en expédition : attendez leur retour au salon."
                            : "À plusieurs dans le même monde : chacun explore de son côté, le gardien de chaque région se combat ensemble.",
                       !busy, [this] { expeditionMenu(); }});
    bool two = n == 2;
    m.items.push_back({"Duel", "",
                       two ? "Chacun commande l'équipe de sa partie principale ; Tab : mode auto (tactiques)."
                           : "Le duel se joue à deux joueurs exactement.",
                       two, [this] {
                         if (!opponent()) return;
                         int theme = irand(0, 4);
                         send(Json{{"t", "debut"}, {"theme", theme}, {"egaux", sameLevel_}});
                         startDuel(theme);
                       }});
    MenuItem lv{"Niveaux", "", "Duel. Réels : chacun garde ses niveaux. Égaux : tous les combattants au niveau 50.", true, [this] {
                  sameLevel_ = !sameLevel_;
                  send(Json{{"t", "regles"}, {"egaux", sameLevel_}});
                }};
    lv.adjust = [this](int) {
      sameLevel_ = !sameLevel_;
      send(Json{{"t", "regles"}, {"egaux", sameLevel_}});
    };
    lv.rightFn = [this] { return std::string(sameLevel_ ? "égaux (50)" : "réels"); };
    m.items.push_back(lv);
  } else if (session_.on) {
    bool can = !ended_.count(session_.seed);
    std::string help = can ? "Reprendre votre expédition de ce monde, ou partir avec un nouveau héros de la région du groupe (" +
                                 std::to_string(session_.region) + ")."
                           : "Votre expédition est terminée : attendez que " + nameOf(0) + " en lance une autre.";
    m.items.push_back({"Rejoindre l'expédition", "", help, can,
                       [this] { startRun(session_.seed, session_.text, session_.region, session_.world); }});
  } else
    m.items.push_back({"Prêt !", "", nameOf(0) + " choisit : duel ou expédition.", false, nullptr});
  m.items.push_back({"Échanger", "", "Échanger une créature de votre partie principale avec un autre joueur, ou lui en offrir une.",
                     !players_.empty(), [this] { tradeMenu(); }});
  m.rows = (int)m.items.size() + 1;
  m.items.push_back({"Quitter", "", "Se déconnecter.", true, [this] {
                       disconnect("");
                       menu();
                     }});
  m.onCancel = m.items.back().act;
  G.menus.push(m);
}

// Duel : chaque joueur construit les deux équipes dans le même ordre ; l'hôte calcule tout
void Online::startDuel(int theme) {
  Player* o = opponent();
  if (!o) return;
  std::vector<FighterP> mine = myTeam(), theirs;
  for (auto& p : o->team)
    if (hasSpecies(p.sp)) theirs.push_back(makeFighter(p.sp, p.lvl));
  if (sameLevel_) {
    std::vector<FighterP> all = mine;
    all.insert(all.end(), theirs.begin(), theirs.end());
    for (auto& f : all) {
      f->lvl = 50;
      f->recalc();
      f->hp = f->mhp, f->mp = f->mmp;
    }
  }
  if (mine.empty() || theirs.empty()) {
    status_ = "Équipe vide.";
    return;
  }
  state_ = State::Duel;
  screen_ = false;
  G.team = mine;
  G.items.clear();  // à armes égales : pas d'objets
  BattleSetup s;
  s.foes = theirs;
  s.foeName = o->name;
  s.canFlee = s.canCapture = false;
  s.theme = std::clamp(theme, 0, 4);
  G.startBattle(std::move(s), nullptr);
  G.duelBattle_ = true;
  G.battle_->goOnline(host_ ? Battle::Net::Host : Battle::Net::Guest, [this](const Json& m) { send(m); }, me_, o->name);
  stateT_ = 0;
}

void Online::onBattleEnd(BattleResult r) {
  Player* o = opponent();
  if (host_ && o) sendTo(o->id, Json{{"t", "fin"}, {"gagnant", r == BattleResult::Win ? "hote" : "invite"}});
  if (host_ || !peers_.empty()) {
    if (o) status_ = r == BattleResult::Win ? "Victoire contre " + o->name + " !" : "Défaite contre " + o->name + "…";
    salon(0);
  } else menu();
}

// ---------------------------------------------------------------------------
// Réseau : à chaque image
// ---------------------------------------------------------------------------
void Online::update(float dt) {
  // Hôte : nouveaux joueurs (pas pendant un duel)
  if (host_ && server_.open() && state_ != State::Duel)
    if (auto c = server_.accept()) {
      int id = 0;
      for (int i = 1; i < MAX_PLAYERS && !id; i++)
        if (!peer(i)) id = i;
      if (!id) {
        c->send(Json{{"t", "refus"}, {"raison", "La partie est complète (" + std::to_string(MAX_PLAYERS) + " joueurs au plus)."}});
        c->poll();
        c->close();
      } else {
        Peer p;
        p.conn = std::move(c);
        p.id = id;
        p.conn->send(hello());
        peers_.push_back(std::move(p));
      }
    }
  // Messages reçus (la liste des connexions peut changer pendant leur traitement)
  std::vector<int> ids;
  for (auto& p : peers_) ids.push_back(p.id);
  for (int id : ids) {
    Peer* p = peer(id);
    if (!p) continue;
    bool wasConnecting = p->conn->connecting();
    auto msgs = p->conn->poll();
    if (wasConnecting && p->conn->connected()) p->conn->send(hello());
    for (auto& m : msgs) {
      try {
        onMessage(id, m);
      } catch (const std::exception&) {  // message mal formé (autre programme, version modifiée) : on coupe cette connexion
        if ((p = peer(id))) p->conn->error = "Message reçu invalide : connexion coupée.", p->conn->close();
      }
      if (!(p = peer(id))) break;
    }
    if (p && p->conn->closed()) lost(id, p->conn->error);
  }
  if (myId_ >= 0 && (host_ || !peers_.empty())) {
    syncGroup(dt);
    updateTrade();
  }
  // État du combat dix fois par seconde : l'hôte d'un duel à l'invité, le chef du gardien aux autres
  bool duel = host_ && state_ == State::Duel && G.battle_ && G.duelBattle_;
  bool lead = G.battle_ && G.groupBattle_ && G.battle_->online() == Battle::Net::Lead && battleIds_.size() > 1;
  if (duel || lead) {
    stateT_ -= dt;
    if (stateT_ <= 0) {
      Json st = G.battle_->netState();
      if (duel) send(st);
      else
        for (int id : battleIds_)
          if (id != myId_) sendTo(id, st);
      stateT_ = .1f;
    }
  }
}

void Online::onMessage(int from, Json m) {
  std::string k = jget<std::string>(m, "t", "");
  if (k == "bonjour") return onHello(from, m);
  if (host_) {
    Peer* p = peer(from);
    if (!p || !p->greeted) return;  // il se présente d'abord
    m["de"] = from;
    // Message pour un autre joueur, ou pour tous : l'hôte le fait suivre
    if (m.contains("a")) {
      int to = jget(m, "a", 0);
      for (auto& q : peers_)
        if (q.greeted && q.id != from && (to == -1 || q.id == to)) q.conn->send(m);
      if (to != -1 && to != 0) return;
    }
  }
  int de = jget(m, "de", from);
  if (k == "refus" && !host_) hostLost(jget<std::string>(m, "raison", "Connexion refusée."));
  else if (k == "bienvenue" && !host_) {
    myId_ = jget(m, "id", -1);
    applyPlayers(m.value("joueurs", Json::array()));
    Json ex = m.value("expedition", Json());
    if (ex.is_object()) {
      session_.on = true;
      session_.seed = std::strtoull(jget<std::string>(ex, "graine", "0").c_str(), nullptr, 10);
      session_.text = jget<std::string>(ex, "texte", "");
      session_.world = jget<std::string>(ex, "monde", "");
      session_.region = std::max(1, jget(ex, "region", 1));
    }
    status_ = "Connecté à " + nameOf(0) + ".";
    salon();
  } else if (k == "joueurs" && !host_) applyPlayers(m.value("joueurs", Json::array()));
  else if (k == "equipe") {  // retour au salon : l'équipe de sa partie principale (pour un duel)
    if (Player* p = player(de)) {
      p->team.clear();
      for (auto& [sp, lvl] : readTeam(m.value("equipe", Json::array()))) p->team.push_back({sp, lvl});
    }
  } else if (k == "regles") sameLevel_ = jget(m, "egaux", false);
  else if (k == "debut" && !host_ && state_ == State::Salon) {
    sameLevel_ = jget(m, "egaux", false);
    startDuel(jget(m, "theme", 0));
  } else if (k == "fin" && !host_ && G.battle_ && G.duelBattle_) {
    G.battle_->netEnd(jget<std::string>(m, "gagnant", "") == "invite");
  } else if (k == "quitte") {
    if (Peer* p = peer(from)) {
      p->conn->error = nameOf(from) + " est parti.";
      p->conn->close();
    }
  } else if (k == "expedition" && !host_) {  // l'hôte lance une expédition à plusieurs
    session_.on = true;
    session_.seed = std::strtoull(jget<std::string>(m, "graine", "0").c_str(), nullptr, 10);
    session_.text = jget<std::string>(m, "texte", "");
    session_.world = jget<std::string>(m, "monde", "");
    session_.region = std::max(1, jget(m, "region", 1));
    ended_.clear();
    if (state_ == State::Salon) startRun(session_.seed, session_.text, session_.region, session_.world);
  } else if (k == "ou") onStatus(de, m);
  else if (k == "echange") onTrade(de, m);
  else if (k == "gardien") onGuardian(m);
  else if (k == "absent") {
    if (G.battle_ && G.groupBattle_) G.battle_->dropPlayer(de);
  } else if (G.battle_ && G.groupBattle_) {
    if (std::find(battleIds_.begin(), battleIds_.end(), de) != battleIds_.end()) G.battle_->netMessage(m);
  } else if (G.battle_ && G.duelBattle_)
    G.battle_->netMessage(m);
}

// Présentation : même version, mêmes données. L'hôte donne au nouveau son numéro et la liste des joueurs
void Online::onHello(int from, const Json& m) {
  std::string v = jget<std::string>(m, "version", "?"), why;
  if (v != BRUMEVAL_VERSION)
    why = "Versions différentes : vous avez la " + std::string(BRUMEVAL_VERSION) + ", l'autre joueur la " + v + ". Mettez le jeu à jour.";
  else if (jget<std::string>(m, "donnees", "") != hash_)
    why = "Données du jeu différentes (Réglages modifiés d'un côté ?). Il faut les mêmes des deux côtés.";
  if (!host_) {
    if (!why.empty()) hostLost(why);
    return;  // l'hôte nous attribuera notre numéro (« bienvenue »)
  }
  Peer* p = peer(from);
  if (!why.empty()) {
    status_ = why;
    if (p) {
      p->conn->send(Json{{"t", "refus"}, {"raison", why}});
      p->conn->poll();
      p->conn->close();
      peers_.erase(std::remove_if(peers_.begin(), peers_.end(), [from](const Peer& q) { return q.id == from; }), peers_.end());
    }
    return;
  }
  if (!p || p->greeted) return;
  p->greeted = true;
  // Un nom différent pour chacun
  std::string base = utf8Prefix(jget<std::string>(m, "pseudo", "Joueur"), 12), name = base;
  if (base.empty()) base = name = "Joueur";
  auto taken = [&](const std::string& n) {
    bool t = n == me_;
    for (auto& q : players_) t = t || q.name == n;
    return t;
  };
  for (int i = 2; taken(name); i++) name = base + " (" + std::to_string(i) + ")";
  Player pl;
  pl.id = from;
  pl.name = name;
  for (auto& [sp, lvl] : readTeam(m.value("equipe", Json::array()))) pl.team.push_back({sp, lvl});
  players_.push_back(pl);
  Json welcome{{"t", "bienvenue"}, {"id", from}, {"joueurs", playersJson()}};
  if (session_.on)
    welcome["expedition"] = Json{
        {"graine", std::to_string(session_.seed)}, {"texte", session_.text}, {"monde", session_.world}, {"region", session_.region}};
  sendTo(from, welcome);
  for (auto& q : peers_)
    if (q.greeted && q.id != from) sendTo(q.id, Json{{"t", "joueurs"}, {"joueurs", playersJson()}});
  status_ = name + " est connecté.";
  sent_ = Json();  // le nouveau doit savoir où j'en suis
  if (state_ == State::Salon && screen_ && G.menus.depth() == 1 && !G.editingText()) salon(G.menus.top().sel);
  if (ready_) guardianWait();
}

// Connexion perdue (ou départ annoncé) avec ce joueur
void Online::lost(int id, const std::string& why) {
  if (!host_) return hostLost(why.empty() ? "La connexion avec l'hôte est coupée." : why);
  bool known = player(id) != nullptr;
  std::string name = nameOf(id);
  peers_.erase(std::remove_if(peers_.begin(), peers_.end(), [id](const Peer& p) { return p.id == id; }), peers_.end());
  players_.erase(std::remove_if(players_.begin(), players_.end(), [id](const Player& p) { return p.id == id; }), players_.end());
  if (!known) return;  // il ne s'était pas encore présenté
  status_ = name + " est parti.";
  send(Json{{"t", "joueurs"}, {"joueurs", playersJson()}});
  if (state_ == State::Duel && G.battle_ && G.duelBattle_) return G.battle_->netEnd(true);  // l'hôte gagne par abandon
  if (G.battle_ && G.groupBattle_) G.battle_->dropPlayer(id);
  if (state_ == State::Salon && screen_ && G.menus.depth() == 1 && !G.editingText()) salon(G.menus.top().sel);
  if (ready_) guardianWait();
  checkGuardians();  // il était peut-être le dernier attendu devant un gardien
}

// Invité : l'hôte est parti (ou la connexion est coupée) : retour au menu Multijoueur
void Online::hostLost(const std::string& why) {
  if (state_ == State::Duel && G.battle_ && G.duelBattle_) {  // l'invité perd le duel en cours
    status_ = why;
    peers_.clear();
    players_.clear();
    G.battle_->netEnd(false);
    return;
  }
  bool run = state_ == State::Expedition;
  if (run && G.battle_) {  // combat en cours : arrêté, sans conséquence
    G.battle_.reset();
    G.groupBattle_ = false;
    G.afterBattle_ = nullptr;
    G.mode = Mode::Map;
  }
  if (run) leaveRun();
  disconnect(why + (run ? " Votre expédition est sauvegardée." : ""));
  menu();
}

// ---------------------------------------------------------------------------
// Dessin des écrans du multijoueur (à la place de l'écran titre)
// ---------------------------------------------------------------------------
void Online::draw(Gfx& g) {
  float L = g.left(), W = (float)g.fullW;
  g.gradV(L, 0, W, SCREEN_H, rgb(0x16123a), rgb(0x3a2c6e));
  g.poly({{L, 176}, {50, 150}, {120, 168}, {190, 136}, {260, 160}, {L + W, 140}, {L + W, 240}, {L, 240}}, rgb(0x241e50));
  g.text(160, 10, "MULTIJOUEUR", GOLD, 1);
  // Panneau de droite : ce qu'il faut savoir à cette étape
  if (G.menus.maxRight() <= 158) {
    std::vector<std::pair<std::string, Color>> lines;
    auto team = [&](const std::string& who, const std::vector<std::pair<std::string, int>>& t) {
      lines.push_back({who, GOLD});
      for (auto& [name, lvl] : t) lines.push_back({"  " + utf8Prefix(name, 12) + " N." + std::to_string(sameLevel_ ? 50 : lvl), WHITE});
    };
    if (state_ == State::Salon && host_ && players_.empty()) {
      lines.push_back({"Vos adresses :", GOLD});
      if (addresses_.empty()) lines.push_back({"  (aucune trouvée)", MUTED});
      for (size_t i = 0; i < addresses_.size() && i < 4; i++) lines.push_back({"  " + addresses_[i], WHITE});
      lines.push_back({"Port : " + std::to_string(port), MUTED});
      lines.push_back({"En attente d'amis…", MUTED});
    } else if (state_ == State::Joining) {
      lines.push_back({"Connexion à :", GOLD});
      lines.push_back({"  " + utf8Prefix(address, 20), WHITE});
      lines.push_back({"Port : " + std::to_string(port), MUTED});
    } else if (state_ == State::Salon && players_.size() == 1) {  // à deux : les équipes du duel
      std::vector<std::pair<std::string, int>> them;
      for (auto& p : players_[0].team)
        if (hasSpecies(p.sp)) them.push_back({species(p.sp).name, p.lvl});
      team("Vous (" + utf8Prefix(me_, 10) + ")", myView_);
      team(utf8Prefix(players_[0].name, 16), them);
    } else if (state_ == State::Salon) {
      lines.push_back({"Joueurs", GOLD});
      lines.push_back({"  " + utf8Prefix(me_, 16) + " (vous)", WHITE});
      for (auto& p : players_) lines.push_back({"  " + utf8Prefix(p.name, 16) + (p.run ? " (exp.)" : ""), WHITE});
      if (session_.on) lines.push_back({"Expédition : région " + std::to_string(session_.region), MUTED});
    } else {
      team("Votre équipe", myView_);
      lines.push_back({"Version " + std::string(BRUMEVAL_VERSION), MUTED});
    }
    int h = 10 + 12 * (int)lines.size();
    g.window(164, 40, 150, h);
    for (size_t i = 0; i < lines.size(); i++) g.text(172, 45 + 12 * (int)i, lines[i].first, lines[i].second);
  }
  if (!status_.empty()) {
    bool good = status_.find("Victoire") != std::string::npos || status_.find("onnecté") != std::string::npos ||
                status_.rfind("Échange fait", 0) == 0 || status_.rfind("Cadeau", 0) == 0;
    auto ls = Gfx::wrap(status_, 284);
    int h = 9 + 11 * (int)ls.size();
    g.window(14, 158, 292, h);
    for (size_t i = 0; i < ls.size(); i++) g.text(160, 162 + 11 * (int)i, ls[i], good ? GREEN : RED, 1);
  }
  G.menus.draw(g, G.time);
  auto lines = Gfx::wrap(G.menus.help(), 284);
  if (!lines.empty() && !lines[0].empty()) {
    int h = 9 + 11 * (int)lines.size();
    g.window(14, 236 - h, 292, h);
    for (size_t i = 0; i < lines.size(); i++) g.text(160, 236 - h + 4 + i * 11, lines[i], WHITE, 1);
  }
}
