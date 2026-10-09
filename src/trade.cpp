// Échange de créatures entre joueurs (voir online.hpp) : dans le salon, avec les créatures
// de la partie principale (lues et réécrites dans sa sauvegarde), ou pendant l'expédition à
// plusieurs (menu de pause > Groupe), avec celles de l'expédition. Une créature contre une,
// ou en cadeau. Les héros (humains) ne s'échangent pas.
//
// Messages « echange » (champ « e ») : « offre » (créature, cadeau, lieu), puis « contre » (la
// créature donnée en retour) ou « accepte » (le cadeau), « fait », « refus » (raison).
#include <algorithm>

#include "expedition.hpp"
#include "game.hpp"
#include "online.hpp"
#include "tactics.hpp"

// Une créature dans un message : tout ce que garde la sauvegarde
static Json creatureJson(const Fighter& f) {
  Json tac = Json::array();
  for (auto& t : f.tactics) tac.push_back(Json::array({t.on, t.cond, t.value, (int)t.kind, t.act}));
  Json gear = Json::array();
  for (auto& g : f.gear) gear.push_back(g);
  return Json{{"sp", f.sp}, {"lvl", f.lvl}, {"xp", f.xp}, {"hp", f.hp}, {"mp", f.mp}, {"lim", (int)f.lim}, {"auto", f.tacticsOn}, {"tac", tac},
              {"equipement", gear}};
}

// Une créature reçue, vérifiée : espèce connue ici (à plusieurs, une créature d'une région pas
// encore atteinte n'existe pas encore), pas un humain ; nullptr sinon
static FighterP creatureFrom(const Json& j) {
  if (!j.is_object()) return nullptr;
  std::string sp = jget<std::string>(j, "sp", "");
  if (!hasSpecies(sp) || species(sp).human) return nullptr;
  FighterP f = makeFighter(sp, std::clamp(jget(j, "lvl", 1), 1, 100));
  Json gear = j.value("equipement", Json::array());  // l'accessoire d'une créature part avec elle
  for (size_t i = 0; i < gear.size() && i < f->gear.size(); i++)
    if (gear[i].is_string() && hasItem(gear[i].get<std::string>()) && item(gear[i].get<std::string>()).slot == (int)i &&
        canEquip(*f, item(gear[i].get<std::string>())))
      f->gear[i] = gear[i].get<std::string>();
  f->recalc();
  f->xp = std::clamp(jget(j, "xp", 0), 0, f->need());
  f->hp = std::clamp(jget(j, "hp", f->mhp), 0, f->mhp);
  f->mp = std::clamp(jget(j, "mp", f->mmp), 0, f->mmp);
  f->lim = (float)std::clamp(jget(j, "lim", 0), 0, 100);
  f->tacticsOn = jget(j, "auto", true);
  Json tac = j.value("tac", Json());
  if (tac.is_array()) {
    f->tactics.clear();
    for (auto& e : tac) {
      if (!e.is_array() || e.size() != 5 || !e[0].is_boolean() || !e[1].is_string() || !e[2].is_number_integer() ||
          !e[3].is_number_integer() || !e[4].is_string())
        continue;
      Tactic t;
      t.on = e[0].get<bool>(), t.cond = e[1].get<std::string>(), t.value = e[2].get<int>(), t.act = e[4].get<std::string>();
      int kind = e[3].get<int>();
      if (kind < 0 || kind > 2 || !findTacticCond(t.cond)) continue;
      t.kind = Tactic::Act(kind);
      f->tactics.push_back(t);
    }
  }
  return f;
}

static std::string label(const Fighter& f) { return f.name() + " N." + std::to_string(f.lvl); }
static std::string label(const Json& j) {
  FighterP f = creatureFrom(j);
  return f ? label(*f) : "?";
}

// Texte d'aide : types, PV et techniques
static std::string describe(const Fighter& f) {
  std::string s = typesName(f.S()) + ", " + std::to_string(f.mhp) + " PV. Techniques :";
  std::vector<std::string> moves = f.techs(), spells = f.spells();
  moves.insert(moves.end(), spells.begin(), spells.end());
  for (size_t i = 0; i < moves.size(); i++) s += (i ? ", " : " ") + moveInfo(moves[i]).name;
  return s + ".";
}
static std::string describe(const Json& j) {
  FighterP f = creatureFrom(j);
  return f ? describe(*f) : "";
}

// ---------------------------------------------------------------------------
// Qui peut échanger
// ---------------------------------------------------------------------------
bool Online::canTrade(std::string* why) const {
  auto no = [why](const std::string& s) {
    if (why) *why = s;
    return false;
  };
  if (state_ == State::Salon) return G.inExpedition() ? no("Échange impossible pour le moment.") : true;
  const Expedition* X = G.expedition_.get();
  if (state_ != State::Expedition || !X || !X->active() || X->onScreen()) return no("Échange impossible pour le moment.");
  if (G.battle_ || G.mode != Mode::Map) return no("Pas pendant un combat.");
  if (ready_) return no("Pas en attendant devant le gardien.");
  return true;
}

// Au salon, il doit aussi y être ; pendant l'expédition, explorer (ni combat, ni gardien)
bool Online::partnerOk(const Player& p) const {
  if (state_ == State::Salon) return !p.run;
  return p.run && !p.hero.empty() && !p.fighting && !p.ready;
}

bool Online::loadMine(std::string& why) {
  if (state_ == State::Expedition) return true;  // l'équipe de l'expédition est déjà là
  if (G.inExpedition() || !G.loadGame()) {
    why = "Il faut une partie sauvegardée (écran titre > Nouvelle partie).";
    return false;
  }
  return true;
}

// Fait l'échange chez moi : ma créature part (vérifiée : toujours la même), l'autre arrive à sa place
bool Online::commitTrade() {
  std::string why;
  if (!loadMine(why)) return false;
  FighterP in;
  if (!trade_.theirs.is_null() && !(in = creatureFrom(trade_.theirs))) return false;
  auto& team = G.team;
  if (trade_.slot >= 0) {
    if (trade_.slot >= (int)team.size() || team[(size_t)trade_.slot]->sp != trade_.sp) return false;
    if (in) team[(size_t)trade_.slot] = in;
    else team.erase(team.begin() + trade_.slot);
  } else if (!in)
    return false;
  else if ((int)team.size() >= rules().maxTeam)
    G.reserve.push_back(in);  // équipe complète : la créature reçue va dans la réserve
  else
    team.push_back(in);
  return G.saveGame();
}

void Online::tradeNote(const std::string& s) {
  if (state_ == State::Expedition) G.notice(s);
  else status_ = s;
}

// ---------------------------------------------------------------------------
// Proposer
// ---------------------------------------------------------------------------
// Place des menus : dans le salon sous le menu du salon, pendant l'expédition à droite du menu de pause
static void place(Menu& m, bool run, int depth) {
  if (run) m.x = 104 - 8 * depth, m.y = 20 + 12 * depth, m.w = 208;
  else m.x = 20 + 12 * depth, m.y = 56 + 12 * depth, m.w = 200;
}

// Avec qui ? (directement le choix de la créature s'il n'y a qu'un joueur possible)
void Online::tradeMenu() {
  std::string why;
  if (trade_.step != Trade::Step::None) return showTrade();
  if (!canTrade(&why)) return tradeNote(why);
  std::vector<int> ok;
  for (auto& p : players_)
    if (partnerOk(p)) ok.push_back(p.id);
  if (ok.empty()) return tradeNote(state_ == State::Salon ? "Personne n'est au salon pour échanger." : "Personne ne peut échanger pour le moment.");
  if (ok.size() == 1) return pickCreature(ok[0]);
  Menu m;
  m.title = "Échanger avec…";
  place(m, state_ == State::Expedition, 0);
  m.rows = (int)players_.size() + 1;
  for (auto& p : players_) {
    int id = p.id;
    MenuItem it{p.name, "", partnerOk(p) ? "" : "En combat ou ailleurs : il faudra attendre.", partnerOk(p), [this, id] { pickCreature(id); }};
    it.shrink = true;
    m.items.push_back(it);
  }
  m.items.push_back({"Retour", "", "", true, [this] { G.menus.pop(); }});
  G.menus.push(m);
}

// Quelle créature donner ? puis : en échange ou en cadeau
void Online::pickCreature(int with) {
  std::string why;
  if (!loadMine(why)) return tradeNote(why);
  bool run = state_ == State::Expedition;
  Menu m;
  m.title = "Donner à " + utf8Prefix(nameOf(with), 12);
  place(m, run, 1);
  m.rows = 8;
  for (size_t i = 0; i < G.team.size(); i++) {
    const Fighter& f = *G.team[i];
    if (f.S().human) continue;
    int slot = (int)i;
    MenuItem it{f.name(), "N." + std::to_string(f.lvl), describe(f), true, [this, with, slot, run] {
                  std::string name = utf8Prefix(nameOf(with), 12);
                  Menu h;
                  h.title = "Comment ?";
                  place(h, run, 2);
                  h.rows = 3;
                  h.items.push_back({"Échange", "", "Une contre une : " + name + " choisit la créature qu'il ou elle donne, puis vous acceptez ou non.",
                                     true, [this, with, slot] { offerTrade(with, slot, false); }});
                  h.items.push_back({"Cadeau", "", name + " la reçoit sans rien donner en retour.", true,
                                     [this, with, slot] { offerTrade(with, slot, true); }});
                  h.items.push_back({"Retour", "", "", true, [this] { G.menus.pop(); }});
                  G.menus.push(h);
                }};
    it.shrink = true;
    m.items.push_back(it);
  }
  if (m.items.empty()) return tradeNote("Vous n'avez aucune créature à donner (les héros ne s'échangent pas).");
  m.items.push_back({"Retour", "", "", true, [this] { G.menus.pop(); }});
  G.menus.push(m);
}

void Online::offerTrade(int with, int slot, bool gift) {
  std::string why;
  const Player* p = player(with);
  if (trade_.step != Trade::Step::None || !p || !partnerOk(*p) || !canTrade(&why) || !loadMine(why) || slot < 0 || slot >= (int)G.team.size() ||
      G.team[(size_t)slot]->S().human)
    return tradeNote(why.empty() ? "Échange impossible pour le moment." : why);
  trade_ = Trade{};
  trade_.step = Trade::Step::Offered;
  trade_.asked = true;
  trade_.with = with;
  trade_.run = state_ == State::Expedition;
  trade_.gift = gift;
  trade_.slot = slot;
  trade_.sp = G.team[(size_t)slot]->sp;
  trade_.mine = creatureJson(*G.team[(size_t)slot]);
  sendTo(with, Json{{"t", "echange"}, {"e", "offre"}, {"creature", trade_.mine}, {"cadeau", gift}, {"run", trade_.run}});
  showTrade();
}

// ---------------------------------------------------------------------------
// Répondre
// ---------------------------------------------------------------------------
void Online::answerTrade(int slot) {
  if (trade_.step != Trade::Step::Answering) return;
  std::string why;
  if (!loadMine(why)) return endTrade(why, me_ + " ne peut pas échanger.");
  if (trade_.gift) {
    sendTo(trade_.with, Json{{"t", "echange"}, {"e", "accepte"}});
  } else {
    if (slot < 0 || slot >= (int)G.team.size() || G.team[(size_t)slot]->S().human) return;
    trade_.slot = slot;
    trade_.sp = G.team[(size_t)slot]->sp;
    trade_.mine = creatureJson(*G.team[(size_t)slot]);
    sendTo(trade_.with, Json{{"t", "echange"}, {"e", "contre"}, {"creature", trade_.mine}});
  }
  trade_.step = Trade::Step::Waiting;
  showTrade();
}

void Online::endTrade(const std::string& note, const std::string& tell) {
  if (!tell.empty() && trade_.with >= 0) sendTo(trade_.with, Json{{"t", "echange"}, {"e", "refus"}, {"raison", tell}});
  trade_ = Trade{};
  tradeNote(note);
  // Ferme le menu de l'échange (pas un autre : un dialogue en cours peut attendre une réponse)
  if (state_ == State::Salon && screen_) salon();
  else if (G.menus.active() && G.menus.top().title.rfind("Échange :", 0) == 0) G.menus.clear();
}

void Online::onTrade(int from, const Json& m) {
  std::string e = jget<std::string>(m, "e", "");
  std::string name = nameOf(from);
  if (e == "offre") {
    auto refuse = [&](const std::string& why) { sendTo(from, Json{{"t", "echange"}, {"e", "refus"}, {"raison", why}}); };
    std::string why;
    const Player* p = player(from);
    bool gift = jget(m, "cadeau", false);
    Json c = m.value("creature", Json());
    if (trade_.step != Trade::Step::None || !p || jget(m, "run", false) != (state_ == State::Expedition) || !canTrade())
      return refuse(me_ + " ne peut pas échanger pour le moment : réessayez plus tard.");
    if (!creatureFrom(c)) return refuse(me_ + " n'a pas encore atteint la région de cette créature.");
    if (!loadMine(why)) return refuse(me_ + " n'a pas de partie sauvegardée.");
    bool any = false;
    for (auto& f : G.team) any = any || !f->S().human;
    if (!gift && !any) return refuse(me_ + " n'a aucune créature à donner en retour.");
    trade_ = Trade{};
    trade_.step = Trade::Step::Answering;
    trade_.with = from;
    trade_.run = state_ == State::Expedition;
    trade_.gift = gift;
    trade_.theirs = c;
    return;  // le menu s'ouvre dès que ce joueur est libre (updateTrade)
  }
  if (trade_.step == Trade::Step::None || from != trade_.with) return;  // échange déjà fini (annulé de ce côté)
  if (e == "refus") {
    trade_.with = -1;  // pas de réponse au refus
    return endTrade(jget<std::string>(m, "raison", name + " refuse l'échange."));
  }
  // Celui qui reçoit la proposition fait l'échange chez lui en premier (« go »), puis prévient
  // (« fait ») : si la connexion coupe entre les deux, la créature est en double, jamais perdue.
  if (e == "accepte" && trade_.step == Trade::Step::Offered && trade_.gift) {
    sendTo(from, Json{{"t", "echange"}, {"e", "go"}});
    trade_.step = Trade::Step::Waiting;
  } else if (e == "go" && trade_.step == Trade::Step::Waiting && !trade_.asked) {
    std::string got = label(trade_.theirs), gave = trade_.gift ? "" : label(trade_.mine);
    if (!commitTrade()) return endTrade("L'échange n'a pas pu être enregistré.", "L'échange n'a pas pu être fait chez " + me_ + ".");
    sendTo(from, Json{{"t", "echange"}, {"e", "fait"}});
    if (trade_.gift) endTrade("Cadeau reçu : " + got + ", de la part de " + name + ".");
    else endTrade("Échange fait : " + gave + " contre " + got + ".");
  } else if (e == "fait" && trade_.step == Trade::Step::Waiting && trade_.asked) {
    std::string gave = label(trade_.mine), got = trade_.gift ? "" : label(trade_.theirs);
    if (!commitTrade()) endTrade("L'échange n'a pas pu être enregistré chez vous.");
    else if (trade_.gift) endTrade("Cadeau envoyé : " + gave + " part chez " + name + ".");
    else endTrade("Échange fait : " + gave + " contre " + got + ".");
  } else if (e == "contre" && trade_.step == Trade::Step::Offered && !trade_.gift) {
    Json c = m.value("creature", Json());
    if (!creatureFrom(c)) return endTrade("Vous n'avez pas encore atteint la région de la créature proposée.", me_ + " n'a pas encore atteint la région de cette créature.");
    trade_.theirs = c;
    trade_.step = Trade::Step::Countered;
  }
}

// À chaque image : l'échange s'arrête si l'autre joueur est parti ou n'est plus au même endroit
// (salon ou expédition) ; son menu s'ouvre dès que ce joueur est libre (pas en combat ni en dialogue)
void Online::updateTrade() {
  if (trade_.step == Trade::Step::None) return;
  const Player* p = player(trade_.with);
  bool run = state_ == State::Expedition;
  if (!p || (state_ != State::Salon && !run) || run != trade_.run || p->run != trade_.run) {
    if (!p) trade_.with = -1;
    return endTrade(p ? "Échange annulé." : "L'autre joueur est parti : échange annulé.", p ? me_ + " a quitté l'échange." : "");
  }
  const Expedition* X = G.expedition_.get();
  bool free = run ? G.mode == Mode::Map && !G.battle_ && !G.sc.busy() && !G.moving && X && X->active() && !X->onScreen()
                  : screen_;
  bool open = G.menus.active() && G.menus.top().title.rfind("Échange :", 0) == 0;
  if (free && !G.editingText() && (!open || trade_.shown != trade_.step)) showTrade();
}

// ---------------------------------------------------------------------------
// Menu de l'échange en cours (attente, réponse, confirmation)
// ---------------------------------------------------------------------------
void Online::showTrade() {
  std::string name = utf8Prefix(nameOf(trade_.with), 12);
  Menu m;
  bool run = trade_.run;
  if (run) m.x = 8, m.y = 8, m.w = 214;
  else m.x = 20, m.y = 56, m.w = 230;
  auto info = [&](const std::string& left, const std::string& right, const std::string& help) {
    MenuItem it{left, right, help, false, nullptr};
    it.shrink = true;
    m.items.push_back(it);
  };
  auto refuse = [this] { endTrade("Vous avez refusé l'échange.", me_ + " refuse l'échange."); };
  switch (trade_.step) {
    case Trade::Step::None: return;
    case Trade::Step::Offered:
      m.title = "Échange : en attente";
      info("Vous donnez", label(trade_.mine), describe(trade_.mine));
      info(trade_.gift ? "En cadeau à" : "Contre une de", name, "");
      m.items.push_back({"Annuler", "", "En attente de la réponse de " + name + ".", true,
                         [this] { endTrade("Échange annulé.", me_ + " a annulé l'échange."); }});
      m.onCancel = m.items.back().act;
      break;
    case Trade::Step::Answering:
      if (trade_.gift) {
        m.title = "Échange : cadeau de " + name;
        info(name + " offre", label(trade_.theirs), describe(trade_.theirs));
        m.items.push_back({"Accepter", "", "Recevoir " + label(trade_.theirs) + " sans rien donner.", true, [this] { answerTrade(-1); }});
      } else {
        std::string why;
        m.title = "Échange : avec " + name;
        info(name + " donne", label(trade_.theirs), describe(trade_.theirs));
        m.items.push_back(menuHeader("Votre créature en retour"));
        if (loadMine(why))
          for (size_t i = 0; i < G.team.size(); i++) {
            const Fighter& f = *G.team[i];
            if (f.S().human) continue;
            int slot = (int)i;
            MenuItem it{f.name(), "N." + std::to_string(f.lvl), describe(f), true, [this, slot] { answerTrade(slot); }};
            it.shrink = true;
            m.items.push_back(it);
          }
      }
      m.items.push_back({"Refuser", "", "", true, refuse});
      m.onCancel = refuse;
      m.sel = 1;
      while (m.sel < (int)m.items.size() - 1 && m.items[(size_t)m.sel].header) m.sel++;
      break;
    case Trade::Step::Countered:
      m.title = "Échange : avec " + name;
      info("Vous donnez", label(trade_.mine), describe(trade_.mine));
      info("Vous recevez", label(trade_.theirs), describe(trade_.theirs));
      m.items.push_back({"Accepter", "", "Faire l'échange.", true, [this] {
                           sendTo(trade_.with, Json{{"t", "echange"}, {"e", "go"}});  // l'autre le fait d'abord (voir onTrade)
                           trade_.step = Trade::Step::Waiting;
                           showTrade();
                         }});
      m.items.push_back({"Refuser", "", "", true, refuse});
      m.onCancel = refuse;
      m.sel = 2;
      break;
    case Trade::Step::Waiting:
      m.title = "Échange : un instant…";
      info(trade_.gift ? "Vous recevez" : "Vous donnez", label(trade_.gift ? trade_.theirs : trade_.mine), "");
      m.cancelable = false;
      break;
  }
  m.rows = std::min((int)m.items.size(), 9);
  trade_.shown = trade_.step;
  if (run) {
    G.menus.clear();
    G.panelMode_ = 0;
    G.noticeT_ = 0;  // le message précédent passerait sur l'aide du menu
  } else {
    status_.clear();  // l'ancien message (résultat d'un duel…) n'a plus rien à voir
    if (!G.menus.active()) salon();
    while (G.menus.depth() > 1) G.menus.pop();
  }
  G.menus.push(m);
}
