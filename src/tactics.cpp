#include "tactics.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <stdexcept>

#include "battle.hpp"
#include "game.hpp"

// ---------------------------------------------------------------------------
// Conditions et actions automatiques
// ---------------------------------------------------------------------------
static const std::vector<TacticCond> CONDS = {
    {"ennemi", "Ennemi : n'importe lequel", TSide::Foe, 0, 0, 0, 0, "L'ennemi sur lequel l'action fait le plus d'effet."},
    {"ennemi_pv_bas", "Ennemi : le moins de PV", TSide::Foe, 0, 0, 0, 0, "L'ennemi qui a le moins de PV : pratique pour l'achever."},
    {"ennemi_pv_haut", "Ennemi : le plus de PV", TSide::Foe, 0, 0, 0, 0, "L'ennemi qui a le plus de PV."},
    {"ennemi_pv_moins", "Ennemi : PV < {n} %", TSide::Foe, 50, 10, 90, 10,
     "Un ennemi affaibli (gauche/droite : seuil). Avec une lanterne, pour le capturer."},
    {"ennemi_pv_plus", "Ennemi : PV > {n} %", TSide::Foe, 70, 10, 90, 10, "Un ennemi encore en forme (gauche/droite : seuil)."},
    {"ennemi_faible", "Ennemi : faible à l'action", TSide::Foe, 0, 0, 0, 0, "Un ennemi sur qui l'action est super efficace (selon son type)."},
    {"ennemi_sans_effet", "Ennemi : sans l'effet", TSide::Foe, 0, 0, 0, 0,
     "Un ennemi qui n'a pas encore l'état ou le malus de l'action (poison, sommeil, Résistance -2…)."},
    {"ennemi_boss", "Ennemi : boss", TSide::Foe, 0, 0, 0, 0, "Le boss du combat, s'il y en a un."},
    {"ennemis_nombre", "Ennemis : {n} ou plus", TSide::Foe, 2, 2, 3, 1,
     "Quand il reste au moins ce nombre d'ennemis (gauche/droite) : pour les attaques de zone."},
    {"allie_pv_moins", "Allié : PV < {n} %", TSide::Ally, 40, 10, 90, 10, "L'allié le plus blessé sous ce seuil (gauche/droite : seuil)."},
    {"allie_ko", "Allié : K.O.", TSide::Ally, 0, 0, 0, 0, "Un allié K.O. : seulement pour une réanimation (sort ou plume)."},
    {"allie_etat", "Allié : avec un état", TSide::Ally, 0, 0, 0, 0, "Un allié empoisonné, brûlé, paralysé ou endormi."},
    {"allie_pm_moins", "Allié : PM < {n} %", TSide::Ally, 30, 10, 90, 10, "Un allié qui manque de PM : pour un éther (gauche/droite : seuil)."},
    {"allie_sans_effet", "Allié : sans l'effet", TSide::Ally, 0, 0, 0, 0, "Un allié qui n'a pas encore le bonus de l'action (Hâte, Rempart…)."},
    {"allie_chef", "Allié : chef d'équipe", TSide::Ally, 0, 0, 0, 0, "Le premier membre de l'équipe."},
    {"soi", "Soi : toujours", TSide::Self, 0, 0, 0, 0, "Le membre lui-même, à chaque tour."},
    {"soi_pv_moins", "Soi : PV < {n} %", TSide::Self, 30, 10, 90, 10, "Quand ses propres PV passent sous ce seuil (gauche/droite)."},
    {"soi_pm_moins", "Soi : PM < {n} %", TSide::Self, 30, 10, 90, 10, "Quand ses propres PM passent sous ce seuil (gauche/droite)."},
    {"soi_limite", "Soi : Limite prête", TSide::Self, 0, 0, 0, 0, "Quand sa jauge Limite est pleine."},
    {"soi_sans_effet", "Soi : sans l'effet", TSide::Self, 0, 0, 0, 0, "Quand il n'a pas encore le bonus de l'action (Carapace…)."},
};
static const std::vector<TacticAuto> AUTOS = {
    {"attaque", "Meilleure attaque", TSide::Foe, "La technique ou le sort qui fait le plus de dégâts à la cible (peut dépenser des PM)."},
    {"technique", "Attaque sans PM", TSide::Foe, "La technique gratuite qui fait le plus de dégâts : garde les PM pour les soins."},
    {"soin", "Meilleur soin", TSide::Ally, "Le sort de soin le plus utile, de groupe si plusieurs alliés sont blessés."},
    {"reanimation", "Réanimation", TSide::Ally, "Un sort qui relève un allié K.O."},
    {"guerison", "Guérison", TSide::Ally, "Un sort qui guérit les états (poison, sommeil…)."},
    {"limite", "Limite", TSide::Any, "La technique ultime, quand la jauge Limite est pleine."},
};

const std::vector<TacticCond>& tacticConds() { return CONDS; }
const std::vector<TacticAuto>& tacticAutos() { return AUTOS; }
const TacticCond* findTacticCond(const std::string& id) {
  for (auto& c : CONDS)
    if (id == c.id) return &c;
  return nullptr;
}
const TacticAuto* findTacticAuto(const std::string& id) {
  for (auto& a : AUTOS)
    if (id == a.id) return &a;
  return nullptr;
}

std::string tacticCondName(const Tactic& t) {
  const TacticCond* c = findTacticCond(t.cond);
  if (!c) return "? " + t.cond;
  std::string s = c->label;
  size_t at = s.find("{n}");
  if (at != std::string::npos) s.replace(at, 3, std::to_string(t.value));
  return s;
}

std::string tacticActName(const Tactic& t) {
  switch (t.kind) {
    case Tactic::Act::Move: return hasMove(t.act) ? moveInfo(t.act).name : "? " + t.act;
    case Tactic::Act::Item: return hasItem(t.act) ? item(t.act).name : "? " + t.act;
    default: {
      const TacticAuto* a = findTacticAuto(t.act);
      return a ? a->name : "? " + t.act;
    }
  }
}

TSide tacticActSide(const Tactic& t) {
  switch (t.kind) {
    case Tactic::Act::Move:
      if (!hasMove(t.act)) return TSide::Any;
      return moveInfo(t.act).target == Target::Enemy || moveInfo(t.act).target == Target::AllEnemies ? TSide::Foe : TSide::Ally;
    case Tactic::Act::Item:
      if (!hasItem(t.act)) return TSide::Any;
      return item(t.act).capture > 0 ? TSide::Foe : TSide::Ally;
    default: {
      const TacticAuto* a = findTacticAuto(t.act);
      return a ? a->side : TSide::Any;
    }
  }
}

static bool cures(const Move& m) {
  for (auto& e : m.effects)
    if (e.cure) return true;
  return false;
}
static bool isRevive(const Tactic& t) {
  switch (t.kind) {
    case Tactic::Act::Move: return hasMove(t.act) && moveInfo(t.act).kind == Kind::Revive;
    case Tactic::Act::Item: return hasItem(t.act) && item(t.act).revive > 0;
    default: return t.act == "reanimation";
  }
}

std::string tacticProblem(const Tactic& t, const Fighter* f) {
  const TacticCond* c = findTacticCond(t.cond);
  if (!c) return "condition inconnue « " + t.cond + " ».";
  switch (t.kind) {
    case Tactic::Act::Move:
      if (!hasMove(t.act)) return "technique inconnue « " + t.act + " ».";
      break;
    case Tactic::Act::Item:
      if (!hasItem(t.act)) return "objet inconnu « " + t.act + " ».";
      if (!item(t.act).battle) return item(t.act).name + " ne s'utilise pas en combat.";
      break;
    default:
      if (!findTacticAuto(t.act)) return "action inconnue « " + t.act + " ».";
  }
  TSide a = tacticActSide(t);
  if (a == TSide::Foe && c->side != TSide::Foe) return "cette action vise un ennemi : il faut une condition « Ennemi ».";
  if (a == TSide::Ally && c->side == TSide::Foe) return "cette action vise un allié : il faut une condition « Allié » ou « Soi ».";
  bool rev = isRevive(t);
  if (t.cond == "allie_ko" && !rev) return "un allié K.O. ne peut recevoir qu'une réanimation.";
  if (rev && t.cond != "allie_ko") return "une réanimation va avec la condition « Allié : K.O. ».";
  if (!f) return "";
  std::vector<std::string> known = f->techs();
  for (auto& id : f->spells()) known.push_back(id);
  auto knows = [&](auto keep) {
    for (auto& id : known)
      if (keep(moveInfo(id))) return true;
    return false;
  };
  if (t.kind == Tactic::Act::Move && std::find(known.begin(), known.end(), t.act) == known.end())
    return f->name() + " ne connaît pas (ou plus) " + moveInfo(t.act).name + ".";
  if (t.kind == Tactic::Act::Auto) {
    if (t.act == "soin" && !knows([](const Move& m) { return m.kind == Kind::Heal && m.power > 0; }))
      return f->name() + " ne connaît aucun sort de soin.";
    if (t.act == "reanimation" && !knows([](const Move& m) { return m.kind == Kind::Revive; }))
      return f->name() + " ne connaît aucun sort de réanimation.";
    if (t.act == "guerison" && !knows([](const Move& m) { return m.kind == Kind::Heal && cures(m); }))
      return f->name() + " ne connaît aucun sort de guérison.";
    if (t.act == "limite" && f->S().limit.empty()) return f->name() + " n'a pas de Limite.";
  }
  return "";
}

int tacticSlots(int lvl) {
  const Rules& r = rules();
  int n = r.tacticStart + (r.tacticPerLvl > 0 ? lvl / r.tacticPerLvl : 0);
  return std::clamp(n, 0, std::max(r.tacticStart, r.tacticMax));
}

std::vector<Tactic> defaultTactics(const Species& s) { return s.tactics.empty() ? rules().tactics : s.tactics; }

// ---------------------------------------------------------------------------
// Fichiers : { "si": "allie_pv_moins", "valeur": 40, "faire": "soin" }
// (ou "technique": identifiant, ou "objet": identifiant ; "active": false)
// ---------------------------------------------------------------------------
Tactic tacticFromJson(const Json& o) {
  Tactic t;
  t.cond = o.at("si").get<std::string>();
  const TacticCond* c = findTacticCond(t.cond);
  if (!c) throw std::runtime_error("condition de tactique inconnue : « " + t.cond + " »");
  t.value = c->hasValue() ? std::clamp(jget(o, "valeur", c->def), c->min, c->max) : 0;
  t.on = jget(o, "active", true);
  if (o.contains("technique")) {
    t.kind = Tactic::Act::Move;
    t.act = o["technique"].get<std::string>();
  } else if (o.contains("objet")) {
    t.kind = Tactic::Act::Item;
    t.act = o["objet"].get<std::string>();
  } else {
    t.act = o.at("faire").get<std::string>();
    if (!findTacticAuto(t.act)) throw std::runtime_error("action de tactique inconnue : « " + t.act + " »");
  }
  return t;
}

Json tacticToJson(const Tactic& t) {
  Json o = Json::object();
  o["si"] = t.cond;
  const TacticCond* c = findTacticCond(t.cond);
  if (c && c->hasValue()) o["valeur"] = t.value;
  o[t.kind == Tactic::Act::Move ? "technique" : t.kind == Tactic::Act::Item ? "objet" : "faire"] = t.act;
  if (!t.on) o["active"] = false;
  return o;
}

std::vector<std::string> checkTactics(const std::vector<Tactic>& list, const std::string& who) {
  std::vector<std::string> err;
  for (size_t i = 0; i < list.size(); i++) {
    std::string p = tacticProblem(list[i], nullptr);
    if (!p.empty()) err.push_back(who + ", tactique " + std::to_string(i + 1) + " : " + p);
  }
  return err;
}

// ---------------------------------------------------------------------------
// Éditeur
// ---------------------------------------------------------------------------
namespace {
const uint32_t GREY = 0x8a92b8, ORANGE = 0xffa060;

class TacticsEditor : public std::enable_shared_from_this<TacticsEditor> {
 public:
  TacticsEditor(Game& g, TacticsTarget t) : G(g), T(std::move(t)), base_(g.menus.depth()) {}
  void menuList(int sel);

 private:
  Game& G;
  TacticsTarget T;
  size_t base_;                        // profondeur des menus à l'ouverture
  std::map<std::string, int> values_;  // seuils réglés dans la liste des conditions

  std::vector<Tactic>& L() { return *T.list; }
  int first() const { return T.on ? 2 : 1; }  // ligne de la règle 1 dans le menu
  void popAll() {
    while (G.menus.depth() > base_) G.menus.pop();
  }
  void changed() {
    if (T.changed) T.changed();
  }
  void close() {
    popAll();
    if (T.closed) T.closed();
  }
  int unlockLevel(int i) const { return (i + 1 - rules().tacticStart) * std::max(1, rules().tacticPerLvl); }
  void menuRule(int i, int sel = 0);
  void reopenRule(int i, int sel) {
    menuList(first() + i);
    menuRule(i, sel);
  }
  void pickCond(int i, bool isNew, int sel = -1);
  void pickAction(int i, bool isNew, const Tactic& draft);
  void setAction(int i, bool isNew, const Tactic& t);
  void confirmReset();
};

void TacticsEditor::menuList(int sel) {
  popAll();
  auto self = shared_from_this();
  Menu m;
  m.title = T.title;
  m.x = 6, m.y = 6, m.w = 308, m.rows = 14;
  m.sel = sel;
  if (T.on) {
    auto flip = [self] {
      *self->T.on = !*self->T.on;
      self->changed();
    };
    MenuItem it{"Tactiques actives", "", "Non : ce membre attend toujours vos ordres, même en mode auto.", true, flip};
    it.adjust = [flip](int) { flip(); };
    it.rightFn = [self] { return std::string(*self->T.on ? "Oui" : "Non"); };
    m.items.push_back(it);
  }
  m.items.push_back(menuHeader("Règles (la première possible est jouée)"));
  int lines = std::max({rules().tacticMax, T.slots, (int)L().size()});
  for (int i = 0; i < lines; i++) {
    std::string num = std::to_string(i + 1) + ". ";
    if (i < (int)L().size()) {
      const Tactic& t = L()[i];
      std::string bad = tacticProblem(t, nullptr), later = tacticProblem(t, T.fighter.get());
      MenuItem it{num + tacticCondName(t), tacticActName(t), "", true, [self, i] { self->menuRule(i); }};
      if (i >= T.slots) {
        it.color = GREY;
        it.help = "Ligne verrouillée jusqu'au niveau " + std::to_string(unlockLevel(i)) + " : cette règle n'est pas encore jouée.";
      } else if (!bad.empty()) {
        it.color = ORANGE;
        it.help = "Ne marchera pas : " + bad;
      } else if (!later.empty()) {
        it.color = GREY;
        it.help = "Pas encore utilisable : " + later + " La règle est sautée en attendant.";
      } else if (!t.on) {
        it.color = GREY;
        it.help = "Règle désactivée. Entrée : la modifier, la réactiver, la déplacer ou l'effacer.";
      } else
        it.help = "Entrée : modifier, désactiver, déplacer ou effacer cette règle.";
      m.items.push_back(it);
    } else if (i < T.slots)
      m.items.push_back({num + "—", "", "Ligne libre. Entrée : ajouter une règle.", true, [self] { self->pickCond((int)self->L().size(), true); }});
    else
      m.items.push_back({num + "(verrouillée)", "niveau " + std::to_string(unlockLevel(i)),
                         "Une ligne de plus tous les " + std::to_string(rules().tacticPerLvl) + " niveaux.", false, nullptr});
  }
  if (T.fighter)
    m.items.push_back({"Tactiques de départ", "", "Remplacer toutes les règles par celles de départ.", true, [self] { self->confirmReset(); }});
  m.items.push_back({"Retour", "", "", true, [self] { self->close(); }});
  m.onCancel = m.items.back().act;
  G.menus.push(m);
}

void TacticsEditor::menuRule(int i, int sel) {
  auto self = shared_from_this();
  const Tactic& t = L()[i];
  const TacticCond* c = findTacticCond(t.cond);
  Menu m;
  m.title = "Règle " + std::to_string(i + 1);
  m.x = 40, m.y = 50, m.w = 240, m.rows = 9;
  m.sel = sel;
  m.items.push_back({"Condition", tacticCondName(t), c ? c->help : "", true, [self, i] { self->pickCond(i, false); }});
  if (c && c->hasValue()) {
    int row = (int)m.items.size();
    bool pct = std::string(c->label).find('%') != std::string::npos;
    MenuItem it{"Seuil", "", "Gauche/droite : changer le seuil de la condition.", true};
    it.adjust = [self, i, row](int d) {
      Tactic& t = self->L()[i];
      const TacticCond* c = findTacticCond(t.cond);
      t.value = std::clamp(t.value + d * c->step, c->min, c->max);
      self->changed();
      self->reopenRule(i, row);
    };
    it.rightFn = [self, i, pct] { return "< " + std::to_string(self->L()[i].value) + (pct ? " %" : "") + " >"; };
    m.items.push_back(it);
  }
  std::string bad = tacticProblem(t, nullptr), later = tacticProblem(t, T.fighter.get());
  m.items.push_back({"Action", tacticActName(t),
                     !bad.empty()     ? "Ne marchera pas : " + bad
                     : !later.empty() ? "Pas encore utilisable : " + later
                                      : "Ce que fait le membre quand la condition est remplie.",
                     true, [self, i] { self->pickAction(i, false, self->L()[i]); }});
  {
    int row = (int)m.items.size();
    auto flip = [self, i, row] {
      self->L()[i].on = !self->L()[i].on;
      self->changed();
      self->reopenRule(i, row);
    };
    MenuItem it{"Active", "", "Non : la règle reste dans la liste mais n'est pas jouée.", true, flip};
    it.adjust = [flip](int) { flip(); };
    it.rightFn = [self, i] { return std::string(self->L()[i].on ? "Oui" : "Non"); };
    m.items.push_back(it);
  }
  int row = (int)m.items.size();
  m.items.push_back({"Monter", "", "Plus haut dans la liste : passe avant les autres.", i > 0, [self, i, row] {
                       std::swap(self->L()[i], self->L()[i - 1]);
                       self->changed();
                       self->reopenRule(i - 1, row);
                     }});
  m.items.push_back({"Descendre", "", "Plus bas dans la liste : passe après les autres.", i + 1 < (int)L().size(), [self, i, row] {
                       std::swap(self->L()[i], self->L()[i + 1]);
                       self->changed();
                       self->reopenRule(i + 1, row + 1);
                     }});
  m.items.push_back({"Effacer", "", "Retirer cette règle.", true, [self, i] {
                       self->L().erase(self->L().begin() + i);
                       self->changed();
                       self->menuList(self->first() + std::min(i, (int)self->L().size()));
                     }});
  m.items.push_back({"Retour", "", "", true, [self, i] { self->menuList(self->first() + i); }});
  m.onCancel = m.items.back().act;
  G.menus.push(m);
}

void TacticsEditor::pickCond(int i, bool isNew, int sel) {
  auto self = shared_from_this();
  const Tactic* cur = isNew ? nullptr : &L()[i];
  Menu m;
  m.title = isNew ? "Nouvelle règle : condition" : "Condition";
  m.x = 6, m.y = 6, m.w = 308, m.rows = 14;
  int side = -1;
  for (auto& c : tacticConds()) {
    if ((int)c.side != side) {
      side = (int)c.side;
      m.items.push_back(menuHeader(c.side == TSide::Foe ? "Ennemis" : c.side == TSide::Ally ? "Alliés" : "Soi-même"));
    }
    std::string id = c.id;
    int v = values_.count(id) ? values_[id] : cur && cur->cond == id ? cur->value : c.def;
    Tactic probe = cur ? *cur : Tactic{};
    probe.cond = id;
    probe.value = v;
    int idx = (int)m.items.size();
    if (sel < 0 && cur && cur->cond == id) m.sel = idx;
    MenuItem it{tacticCondName(probe), c.hasValue() ? "< >" : "", c.help, true, [self, i, isNew, id, v] {
                  if (isNew) {
                    Tactic d;
                    d.cond = id;
                    d.value = v;
                    self->pickAction(i, true, d);
                    return;
                  }
                  Tactic& t = self->L()[i];
                  t.cond = id;
                  t.value = v;
                  self->changed();
                  bool bad = !tacticProblem(t, nullptr).empty();  // l'action ne va plus avec la condition
                  self->reopenRule(i, 0);
                  if (bad) self->pickAction(i, false, t);
                }};
    if (cur && !tacticProblem(probe, nullptr).empty()) it.help += " (l'action devra changer)";
    if (c.hasValue())
      it.adjust = [self, i, isNew, id, idx, v](int d) {
        const TacticCond* c = findTacticCond(id);
        self->values_[id] = std::clamp(v + d * c->step, c->min, c->max);
        self->popAll();
        if (isNew) self->menuList(self->first() + i);
        else self->reopenRule(i, 0);
        self->pickCond(i, isNew, idx);
      };
    m.items.push_back(it);
  }
  if (sel >= 0) m.sel = sel;
  G.menus.push(m);
}

void TacticsEditor::pickAction(int i, bool isNew, const Tactic& draft) {
  auto self = shared_from_this();
  Menu m;
  m.title = isNew ? "Nouvelle règle : action" : "Action";
  m.x = 6, m.y = 6, m.w = 308, m.rows = 14;
  auto add = [&](Tactic::Act kind, const std::string& id, const std::string& right, const std::string& desc) {
    Tactic cand = draft;
    cand.kind = kind;
    cand.act = id;
    std::string bad = tacticProblem(cand, nullptr), later = tacticProblem(cand, T.fighter.get());
    if (!isNew && kind == draft.kind && id == draft.act) m.sel = (int)m.items.size();
    MenuItem it{tacticActName(cand), right, !bad.empty() ? "Impossible avec cette condition : " + bad : desc, bad.empty(),
                [self, i, isNew, cand] { self->setAction(i, isNew, cand); }};
    if (bad.empty() && !later.empty()) {
      it.color = GREY;
      it.help = "Pas encore utilisable : " + later + " " + desc;
    }
    m.items.push_back(it);
  };
  m.items.push_back(menuHeader("Automatique"));
  for (auto& a : tacticAutos()) add(Tactic::Act::Auto, a.id, "", a.help);
  if (T.fighter) {
    auto techs = T.fighter->techs(), spells = T.fighter->spells();
    if (!techs.empty()) m.items.push_back(menuHeader("Techniques"));
    for (auto& id : techs) add(Tactic::Act::Move, id, typeName(moveInfo(id).type), moveDetails(moveInfo(id)));
    if (!spells.empty()) m.items.push_back(menuHeader("Sorts"));
    for (auto& id : spells) {
      const Move& mv = moveInfo(id);
      add(Tactic::Act::Move, id, std::to_string(mv.cost) + " PM", mv.desc.empty() ? moveDetails(mv) : mv.desc + " (" + moveDetails(mv) + ")");
    }
  }
  m.items.push_back(menuHeader("Objets (pris dans le sac)"));
  for (auto& d : allItems()) {
    if (!d.battle) continue;
    int n = G.items.count(d.id) ? G.items.at(d.id) : 0;
    add(Tactic::Act::Item, d.id, "×" + std::to_string(n), d.desc);
  }
  m.onCancel = [self, i, isNew] {
    if (isNew) self->menuList(self->first() + i);
    else self->reopenRule(i, 0);
  };
  G.menus.push(m);
}

void TacticsEditor::setAction(int i, bool isNew, const Tactic& t) {
  if (isNew) {
    i = std::min(i, (int)L().size());
    L().insert(L().begin() + i, t);
    changed();
    menuList(first() + i);
    return;
  }
  L()[i] = t;
  changed();
  const TacticCond* c = findTacticCond(t.cond);
  reopenRule(i, c && c->hasValue() ? 2 : 1);
}

void TacticsEditor::confirmReset() {
  auto self = shared_from_this();
  Menu q;
  q.title = "Remettre les tactiques de départ ?";
  q.x = 50, q.y = 90, q.w = 220, q.rows = 2;
  q.items.push_back({"Oui", "", "Les règles actuelles sont remplacées.", true, [self] {
                       *self->T.list = defaultTactics(self->T.fighter->S());
                       self->changed();
                       self->menuList(self->first());
                     }});
  q.items.push_back({"Non", "", "", true, [self] { self->G.menus.pop(); }});
  G.menus.push(q);
}
}  // namespace

void openTacticsEditor(Game& g, TacticsTarget t, int sel) {
  auto ed = std::make_shared<TacticsEditor>(g, std::move(t));
  ed->menuList(sel);
}
