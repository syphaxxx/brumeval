#include "online.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

#include "battle.hpp"
#include "game.hpp"
#include "tactics.hpp"
#include "version.hpp"

static const Color WHITE = rgb(0xffffff), GOLD = rgb(0xffd34d), MUTED = rgb(0xaab3d8), GREEN = rgb(0x7dffa8), RED = rgb(0xff8a7a);

Online::Online(Game& g) : G(g) { loadSettings(); }

// ---------------------------------------------------------------------------
// Réglages et équipe
// ---------------------------------------------------------------------------
std::string Online::file(const std::string& name) const {
  char* p = SDL_GetPrefPath("Brumeval", "Brumeval");
  std::string s = p ? p : "";
  SDL_free(p);
  return s + name;
}
void Online::loadSettings() {
  std::ifstream f(file(settingsName));
  std::string line;
  while (std::getline(f, line)) {
    if (line.rfind("pseudo ", 0) == 0) pseudo = line.substr(7);
    else if (line.rfind("adresse ", 0) == 0) address = line.substr(8);
  }
}
void Online::saveSettings() const {
  std::ofstream f(file(settingsName));
  f << "pseudo " << pseudo << "\nadresse " << address << '\n';
}

std::string Online::dataHash() {
  uint64_t h = 1469598103934665603ull;
  auto mix = [&](const std::string& s) {
    for (unsigned char c : s) h = (h ^ c) * 1099511628211ull;
  };
  mix(BRUMEVAL_VERSION);
  for (int f = 0; f < N_DATAFILES; f++) mix(dataDoc(DataFile(f)).dump());
  char buf[20];
  std::snprintf(buf, sizeof buf, "%016llx", (unsigned long long)h);
  return buf;
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
    if (k == "membre") {
      std::string sp;
      int lvl = 1;
      s >> sp >> lvl;
      if ((int)team.size() >= 3) break;
      if (!hasSpecies(sp)) continue;
      team.push_back(makeFighter(sp, lvl));
    } else if (k == "tactiques" && !team.empty()) {
      s >> team.back()->tacticsOn;
      team.back()->tactics.clear();
    } else if (k == "tactique" && !team.empty()) {
      Tactic t;
      char kind = 'a';
      s >> t.on >> t.cond >> t.value >> kind >> t.act;
      t.kind = kind == 't' ? Tactic::Act::Move : kind == 'o' ? Tactic::Act::Item : Tactic::Act::Auto;
      if (s && findTacticCond(t.cond)) team.back()->tactics.push_back(t);
    }
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
  m.items.push_back({"Héberger un duel", "", "Attendre qu'un ami vous rejoigne : donnez-lui votre adresse.", true, [this] { host(); }});
  m.items.push_back({"Rejoindre un duel", "", "Se connecter à l'ordinateur d'un ami avec son adresse.", true, [this] {
                       G.editText("Adresse de l'hôte (ex. 100.101.102.103)", address, 60, [this](const std::string& a) {
                         if (a.empty()) return;
                         address = a;
                         saveSettings();
                         join(a);
                       });
                     }});
  {
    MenuItem it{"Pseudo", "", "Le nom que voit l'autre joueur.", true, [this] {
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
  if (!server_.listen(port, err, loopback)) {
    status_ = err;
    return menu(0);
  }
  host_ = true;
  state_ = State::Hosting;
  addresses_ = net::localAddresses();
  status_.clear();
  waitMenu("En attente…", "Votre ami choisit « Rejoindre un duel » et entre une de vos adresses.");
}

void Online::join(const std::string& addr) {
  std::string err;
  conn_ = net::Conn::connect(addr, port, err);
  if (!conn_) {
    status_ = err;
    return menu(1);
  }
  host_ = false;
  state_ = State::Joining;
  status_.clear();
  waitMenu("Connexion…", "Connexion à " + addr + ", port " + std::to_string(port) + ".");
}

void Online::send(const Json& m) {
  if (conn_) conn_->send(m);
}

void Online::disconnect(const std::string& why) {
  if (conn_) {
    if (conn_->connected()) conn_->send(Json{{"t", "quitte"}});
    conn_->poll();  // envoie ce qui reste
    conn_->close();
  }
  conn_.reset();
  server_.close();
  peer_.clear();
  peerTeam_.clear();
  if (!why.empty()) status_ = why;
}

void Online::leave() {
  disconnect("");
  screen_ = false;
  state_ = State::Menu;
}

// Salon : les deux joueurs sont connectés ; l'hôte lance le duel
void Online::salon(int sel) {
  refreshTeam();
  state_ = State::Salon;
  screen_ = true;
  G.mode = Mode::Title;
  G.menus.clear();
  Menu m;
  m.title = "Duel contre " + peer_;
  m.x = 8, m.y = 40, m.w = 150, m.rows = 4, m.sel = sel;
  if (host_) {
    m.items.push_back({"Lancer le duel", "", "Chacun commande son équipe ; Tab : mode auto (tactiques).", true, [this] {
                         int theme = irand(0, 4);
                         send(Json{{"t", "debut"}, {"theme", theme}, {"egaux", sameLevel_}});
                         startDuel(theme);
                       }});
    MenuItem lv{"Niveaux", "", "Réels : chacun garde ses niveaux. Égaux : tous les combattants au niveau 50.", true, [this] {
                  sameLevel_ = !sameLevel_;
                  send(Json{{"t", "regles"}, {"egaux", sameLevel_}});
                }};
    lv.adjust = [this](int) {
      sameLevel_ = !sameLevel_;
      send(Json{{"t", "regles"}, {"egaux", sameLevel_}});
    };
    lv.rightFn = [this] { return std::string(sameLevel_ ? "égaux (50)" : "réels"); };
    m.items.push_back(lv);
  } else
    m.items.push_back({"Prêt !", "", "C'est " + peer_ + " qui lance le duel.", false, nullptr});
  m.items.push_back({"Quitter", "", "Se déconnecter.", true, [this] {
                       disconnect("");
                       menu();
                     }});
  m.onCancel = m.items.back().act;
  G.menus.push(m);
}

// Duel : chaque joueur construit les deux équipes dans le même ordre ; l'hôte calcule tout
void Online::startDuel(int theme) {
  std::vector<FighterP> mine = myTeam(), theirs;
  for (auto& p : peerTeam_)
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
  if (mine.empty() || theirs.empty()) return disconnect("Équipe vide.");
  state_ = State::Duel;
  screen_ = false;
  G.team = mine;
  G.items.clear();  // à armes égales : pas d'objets
  BattleSetup s;
  s.foes = theirs;
  s.foeName = peer_;
  s.canFlee = s.canCapture = false;
  s.theme = std::clamp(theme, 0, 4);
  G.startBattle(std::move(s), nullptr);
  G.duelBattle_ = true;
  G.battle_->goOnline(host_ ? Battle::Net::Host : Battle::Net::Guest, [this](const Json& m) { send(m); }, pseudo, peer_);
  stateT_ = 0;
}

void Online::onBattleEnd(BattleResult r) {
  if (host_ && conn_ && conn_->connected()) send(Json{{"t", "fin"}, {"gagnant", r == BattleResult::Win ? "hote" : "invite"}});
  if (conn_ && conn_->connected()) {
    status_ = r == BattleResult::Win ? "Victoire contre " + peer_ + " !" : "Défaite contre " + peer_ + "…";
    salon(0);
  } else menu();
}

// ---------------------------------------------------------------------------
// Réseau : à chaque image
// ---------------------------------------------------------------------------
void Online::update(float dt) {
  if (state_ == State::Hosting && !conn_) {
    conn_ = server_.accept();
    if (conn_) {
      server_.close();  // un seul adversaire
      send(Json{{"t", "bonjour"}, {"version", BRUMEVAL_VERSION}, {"donnees", dataHash()}, {"pseudo", pseudo}});
    }
  }
  if (!conn_) return;
  bool wasConnecting = conn_->connecting();
  auto msgs = conn_->poll();
  if (wasConnecting && conn_->connected())
    send(Json{{"t", "bonjour"}, {"version", BRUMEVAL_VERSION}, {"donnees", dataHash()}, {"pseudo", pseudo}});
  for (auto& m : msgs) {
    onMessage(m);
    if (!conn_) return;
  }
  if (conn_->closed()) {
    std::string why = conn_->error.empty() ? "La connexion est coupée." : conn_->error;
    if (state_ == State::Duel && G.battle_ && G.duelBattle_) {
      status_ = why;
      conn_.reset();
      G.battle_->netEnd(host_);  // l'hôte gagne par abandon ; l'invité perd la partie en cours
      return;
    }
    bool screen = screen_;
    disconnect(why);
    if (screen) menu();
    return;
  }
  // Hôte : état du combat envoyé dix fois par seconde
  if (host_ && state_ == State::Duel && G.battle_ && G.duelBattle_) {
    stateT_ -= dt;
    if (stateT_ <= 0) {
      send(G.battle_->netState());
      stateT_ = .1f;
    }
  }
}

void Online::onMessage(const Json& m) {
  std::string k = jget<std::string>(m, "t", "");
  if (k == "bonjour") {
    std::string v = jget<std::string>(m, "version", "?");
    if (v != BRUMEVAL_VERSION) {
      disconnect("Versions différentes : vous avez la " + std::string(BRUMEVAL_VERSION) + ", l'autre joueur la " + v + ". Mettez le jeu à jour.");
      return menu();
    }
    if (jget<std::string>(m, "donnees", "") != dataHash()) {
      disconnect("Données du jeu différentes (Réglages modifiés d'un côté ?). Il faut les mêmes des deux côtés.");
      return menu();
    }
    peer_ = jget<std::string>(m, "pseudo", "Joueur");
    if (peer_ == pseudo) peer_ += " (2)";
    Json team = Json::array();
    for (auto& f : myTeam()) team.push_back(Json::array({f->sp, f->lvl}));
    send(Json{{"t", "equipe"}, {"equipe", team}});
  } else if (k == "equipe") {
    peerTeam_.clear();
    Json team = m.value("equipe", Json::array());  // variable : la boucle doit la garder en vie
    for (auto& e : team)
      if (e.is_array() && e.size() == 2 && hasSpecies(e[0].get<std::string>()) && peerTeam_.size() < 3)
        peerTeam_.push_back({e[0].get<std::string>(), std::clamp(e[1].get<int>(), 1, 100)});
    if (peerTeam_.empty()) {
      disconnect("L'équipe de l'autre joueur est inconnue.");
      return menu();
    }
    status_ = peer_ + " est connecté.";
    salon(0);
  } else if (k == "regles") {
    sameLevel_ = jget(m, "egaux", false);
  } else if (k == "debut" && !host_ && state_ == State::Salon) {
    sameLevel_ = jget(m, "egaux", false);
    startDuel(jget(m, "theme", 0));
  } else if (k == "fin" && !host_ && G.battle_ && G.duelBattle_) {
    G.battle_->netEnd(jget<std::string>(m, "gagnant", "") == "invite");
  } else if (k == "quitte") {
    if (conn_) conn_->error = peer_.empty() ? "L'autre joueur est parti." : peer_ + " est parti.";
    if (conn_) conn_->close();
  } else if (G.battle_ && G.duelBattle_) G.battle_->netMessage(m);
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
    std::vector<std::pair<std::string, int>> me = myView_, them;
    for (auto& p : peerTeam_) them.push_back({species(p.sp).name, p.lvl});
    if (state_ == State::Hosting) {
      lines.push_back({"Vos adresses :", GOLD});
      if (addresses_.empty()) lines.push_back({"  (aucune trouvée)", MUTED});
      for (size_t i = 0; i < addresses_.size() && i < 4; i++) lines.push_back({"  " + addresses_[i], WHITE});
      lines.push_back({"Port : " + std::to_string(port), MUTED});
    } else if (state_ == State::Joining) {
      lines.push_back({"Connexion à :", GOLD});
      lines.push_back({"  " + utf8Prefix(address, 20), WHITE});
      lines.push_back({"Port : " + std::to_string(port), MUTED});
    } else if (state_ == State::Salon) {
      team("Vous (" + utf8Prefix(pseudo, 10) + ")", me);
      team(utf8Prefix(peer_, 16), them);
    } else {
      team("Votre équipe", me);
      lines.push_back({"Version " + std::string(BRUMEVAL_VERSION), MUTED});
    }
    int h = 10 + 12 * (int)lines.size();
    g.window(164, 40, 150, h);
    for (size_t i = 0; i < lines.size(); i++) g.text(172, 45 + 12 * (int)i, lines[i].first, lines[i].second);
  }
  if (!status_.empty()) {
    bool bad = status_.find("Victoire") == std::string::npos && status_.find("connecté") == std::string::npos;
    auto ls = Gfx::wrap(status_, 284);
    int h = 9 + 11 * (int)ls.size();
    g.window(14, 158, 292, h);
    for (size_t i = 0; i < ls.size(); i++) g.text(160, 162 + 11 * (int)i, ls[i], bad ? RED : GREEN, 1);
  }
  G.menus.draw(g, G.time);
  auto lines = Gfx::wrap(G.menus.help(), 284);
  if (!lines.empty() && !lines[0].empty()) {
    int h = 9 + 11 * (int)lines.size();
    g.window(14, 236 - h, 292, h);
    for (size_t i = 0; i < lines.size(); i++) g.text(160, 236 - h + 4 + i * 11, lines[i], WHITE, 1);
  }
}
