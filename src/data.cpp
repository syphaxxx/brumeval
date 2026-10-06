#include "data.hpp"

#include <algorithm>
#include <random>
#include <stdexcept>
#include <unordered_map>

// ---------------------------------------------------------------------------
// Contenu chargé depuis data/
// ---------------------------------------------------------------------------
static std::vector<TypeDef> TYPES;
static std::vector<std::vector<float>> CHART;  // CHART[attaque][défense]
static std::vector<Move> MOVES;
static std::vector<Species> SPECIES;
static std::vector<ItemDef> ITEMS;
static std::vector<Look> LOOKS;
static Rules RULES;
static std::unordered_map<std::string, size_t> MOVE_IX, SPECIES_IX, ITEM_IX;

static const char* SHAPES[] = {"renard", "goutte", "bourgeon", "oiseau", "souris", "insecte", "grenouille",
                               "champignon", "rocher", "feu_follet", "boss", "lezard", "golem", "humain"};
static const char* TARGETS[] = {"ennemi", "tous_ennemis", "allie", "tous_allies", "allie_ko"};
static const char* KINDS[] = {"physique", "magique", "soin", "rappel"};
static const char* HATS[] = {"aucune", "capuche", "chapeau", "bandeau"};
static const char* WEAPONS[] = {"aucune", "epee", "baton", "hache", "dague"};
static const char* DIRS[] = {"haut", "bas", "gauche", "droite"};

template <size_t N>
static int nameIndex(const char* (&names)[N], const std::string& s, const char* what) {
  for (size_t i = 0; i < N; i++)
    if (s == names[i]) return (int)i;
  std::string all;
  for (size_t i = 0; i < N; i++) all += (i ? ", " : "") + std::string(names[i]);
  throw std::runtime_error(std::string(what) + " inconnu(e) : « " + s + " » (possibles : " + all + ")");
}

Shape shapeOf(const std::string& s) { return Shape(nameIndex(SHAPES, s, "Forme")); }
const char* shapeName(Shape s) { return SHAPES[(int)s]; }
int dirOf(const std::string& s) { return nameIndex(DIRS, s, "Direction"); }
const char* dirName(int d) { return DIRS[d & 3]; }

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------
const std::vector<TypeDef>& types() { return TYPES; }
Type typeOf(const std::string& id) {
  for (size_t i = 0; i < TYPES.size(); i++)
    if (TYPES[i].id == id) return (Type)i;
  throw std::runtime_error("Type inconnu : « " + id + " »");
}
const char* typeName(Type t) { return t >= 0 && t < (int)TYPES.size() ? TYPES[t].name.c_str() : "?"; }
float typeEff(Type a, Type d) {
  if (a < 0 || d < 0 || a >= (int)CHART.size() || d >= (int)CHART.size()) return 1.f;
  return CHART[a][d];
}

// ---------------------------------------------------------------------------
// Règles
// ---------------------------------------------------------------------------
const Rules& rules() { return RULES; }
Rules& editRules() { return RULES; }
std::vector<RuleField> ruleFields(Rules& r) {
  auto D = [](const char* g, const char* k, const char* l, double& v, double mn, double mx, double st) {
    RuleField f{g, k, l};
    f.d = &v, f.min = mn, f.max = mx, f.step = st;
    return f;
  };
  auto I = [](const char* g, const char* k, const char* l, int& v, double mn, double mx, double st) {
    RuleField f{g, k, l};
    f.i = &v, f.min = mn, f.max = mx, f.step = st;
    return f;
  };
  return {
      I("equipe", "taille_max", "Taille max. de l'équipe", r.maxTeam, 3, 20, 1),
      I("equipe", "en_combat", "Combattants en première ligne", r.frontSize, 1, 3, 1),
      D("rencontres", "taux", "Chance de rencontre par pas", r.encounterRate, 0, 1, .01),
      I("rencontres", "pas_minimum", "Pas sans rencontre après un combat", r.minSteps, 0, 50, 1),
      D("rencontres", "chance_2e", "Chance d'un 2e ennemi", r.second, 0, 1, .05),
      D("rencontres", "chance_3e", "Chance d'un 3e ennemi", r.third, 0, 1, .05),
      D("combat", "atb_base", "Vitesse ATB de base", r.atbBase, 0, 100, 1),
      D("combat", "atb_vitesse", "Multiplicateur de vitesse ATB", r.atbSpeed, .1, 5, .1),
      D("combat", "fuite", "Chance de fuite", r.flee, 0, 1, .05),
      D("combat", "limite_gain", "Gain de Limite (coups reçus)", r.limitGain, 0, 500, 10),
      D("combat", "bonus_meme_type", "Bonus si technique du même type", r.stab, 1, 3, .05),
      D("combat", "alea_min", "Dégâts aléatoires minimum", r.spreadMin, .5, 1, .01),
      D("combat", "diviseur_degats", "Diviseur des dégâts", r.dmgDivisor, 5, 200, 1),
      I("recompenses", "xp_par_niveau", "Expérience par niveau d'ennemi", r.xpPerLevel, 0, 100, 1),
      I("recompenses", "xp_boss", "Multiplicateur d'expérience des boss", r.xpBoss, 1, 20, 1),
      I("recompenses", "or_par_niveau", "Or par niveau d'ennemi", r.goldPerLevel, 0, 100, 1),
      I("recompenses", "or_boss", "Multiplicateur d'or des boss", r.goldBoss, 1, 50, 1),
      D("recompenses", "part_combattants", "Part d'expérience des combattants", r.xpFront, 0, 2, .05),
      D("recompenses", "part_reserve", "Part d'expérience de la réserve", r.xpReserve, 0, 2, .05),
      D("capture", "base", "Chance de capture de base", r.capBase, 0, 1, .01),
      D("capture", "bonus_pv", "Bonus de capture (PV perdus)", r.capHp, 0, 1, .01),
      D("capture", "max", "Chance de capture maximale", r.capMax, 0, 1, .01),
      I("formules", "pv_diviseur", "PV : diviseur", r.hpDiv, 1, 100, 1),
      I("formules", "pv_par_niveau", "PV : bonus par niveau", r.hpPerLvl, 0, 20, 1),
      I("formules", "pv_base", "PV : base", r.hpBase, 0, 200, 1),
      I("formules", "pm_diviseur", "PM : diviseur", r.mpDiv, 1, 100, 1),
      I("formules", "pm_base", "PM : base", r.mpBase, 0, 100, 1),
      I("formules", "stat_diviseur", "Stats : diviseur", r.statDiv, 1, 100, 1),
      I("formules", "stat_base", "Stats : base", r.statBase, 0, 100, 1),
      I("formules", "xp_base", "Expérience : base", r.xpBase, 0, 1000, 1),
      D("formules", "xp_carre", "Expérience : facteur niveau²", r.xpSquare, .1, 10, .1),
  };
}

static void loadRules() {
  Json j = readJson("regles.json");
  Rules r;
  for (auto& f : ruleFields(r)) {
    if (!j.contains(f.group) || !j[f.group].contains(f.key)) continue;
    const Json& v = j[f.group][f.key];
    if (f.d) *f.d = v.get<double>();
    else *f.i = v.get<int>();
  }
  const Json& d = j.at("depart");
  r.startMap = d.at("carte").get<std::string>();
  r.startX = d.at("x").get<int>();
  r.startY = d.at("y").get<int>();
  r.startDir = dirOf(jget<std::string>(d, "direction", "bas"));
  r.hero = d.at("heros").get<std::string>();
  r.starters = d.at("compagnons").get<std::vector<std::string>>();
  r.startLevel = jget(d, "niveau", 5);
  r.startGold = jget(d, "or", 0);
  Json startItems = d.value("objets", Json::object());  // variable : la boucle doit la garder en vie
  for (auto& [k, v] : startItems.items()) r.startItems.push_back({k, v.get<int>()});
  const Json& rv = d.at("reveil");
  r.respawnMap = rv.at("carte").get<std::string>();
  r.respawnX = rv.at("x").get<int>();
  r.respawnY = rv.at("y").get<int>();
  r.startEvent = jget<std::string>(d, "evenement", "");
  if (j.contains("recompenses"))
    for (auto& b : j["recompenses"].value("butin", Json::array())) r.drops.push_back({b.at("objet").get<std::string>(), b.at("chance").get<double>()});
  RULES = r;
}

// ---------------------------------------------------------------------------
// Chargement
// ---------------------------------------------------------------------------
template <class T, class F>
static void loadList(const char* file, std::vector<T>& out, F parse) {
  out.clear();
  Json j = readJson(file);
  for (auto& o : j) {
    std::string id = jget<std::string>(o, "id", "?");
    try {
      out.push_back(parse(o));
    } catch (const std::exception& e) {
      throw std::runtime_error(std::string("data/") + file + ", « " + id + " » : " + e.what());
    }
  }
}

void loadData() {
  // Types et table d'efficacité
  Json t = readJson("types.json");
  TYPES.clear();
  for (auto& o : t.at("types")) TYPES.push_back({o.at("id").get<std::string>(), o.at("nom").get<std::string>(), parseColor(o.value("couleur", Json("#c8c8c8")))});
  CHART.assign(TYPES.size(), std::vector<float>(TYPES.size(), 1.f));
  Json chart = t.value("efficacite", Json::object());  // variable : la boucle doit la garder en vie
  for (auto& [a, row] : chart.items())
    for (auto& [d, v] : row.items()) CHART[typeOf(a)][typeOf(d)] = v.get<float>();

  loadList("techniques.json", MOVES, [](const Json& o) {
    Move m;
    m.id = o.at("id").get<std::string>();
    m.name = o.at("nom").get<std::string>();
    m.type = typeOf(o.at("type").get<std::string>());
    m.kind = Kind(nameIndex(KINDS, o.at("genre").get<std::string>(), "Genre"));
    m.target = Target(nameIndex(TARGETS, o.at("cible").get<std::string>(), "Cible"));
    m.power = o.at("puissance").get<int>();
    m.cost = jget(o, "cout", 0);
    m.desc = jget<std::string>(o, "description", "");
    return m;
  });

  loadList("apparences.json", LOOKS, [](const Json& o) {
    Look l;
    l.id = o.at("id").get<std::string>();
    l.name = jget<std::string>(o, "nom", l.id);
    l.hair = parseColor(o.at("cheveux"));
    l.skin = parseColor(o.at("peau"));
    l.top = parseColor(o.at("haut"));
    l.bottom = parseColor(o.at("bas"));
    l.hat = nameIndex(HATS, jget<std::string>(o, "coiffe", "aucune"), "Coiffe");
    l.weapon = nameIndex(WEAPONS, jget<std::string>(o, "arme", "aucune"), "Arme");
    return l;
  });

  loadList("especes.json", SPECIES, [](const Json& o) {
    Species s;
    s.id = o.at("id").get<std::string>();
    s.name = o.at("nom").get<std::string>();
    s.type = typeOf(o.at("type").get<std::string>());
    const Json& b = o.at("base");
    s.base = {b.at("pv").get<int>(), b.at("pm").get<int>(), b.at("attaque").get<int>(),
              b.at("defense").get<int>(), b.at("magie").get<int>(), b.at("vitesse").get<int>()};
    s.human = jget(o, "humain", false);
    if (s.human) {
      s.shape = Shape::Human;
      s.look = lookIndex(o.at("apparence").get<std::string>());
      if (s.look < 0) throw std::runtime_error("apparence inconnue : " + o.at("apparence").get<std::string>());
      s.c1 = s.c2 = 0;
    } else {
      s.shape = shapeOf(o.at("forme").get<std::string>());
      const Json& c = o.at("couleurs");
      s.c1 = parseColor(c.at(0));
      s.c2 = parseColor(c.at(1));
    }
    s.role = jget<std::string>(o, "role", "");
    for (auto& l : o.at("apprend")) s.learn.push_back({l.at(0).get<int>(), l.at(1).get<std::string>()});
    s.limit = jget<std::string>(o, "limite", "");  // vide : pas de Limite
    return s;
  });

  loadList("objets.json", ITEMS, [](const Json& o) {
    ItemDef d;
    d.id = o.at("id").get<std::string>();
    d.name = o.at("nom").get<std::string>();
    d.desc = jget<std::string>(o, "description", "");
    d.price = jget(o, "prix", 0);
    d.battle = jget(o, "combat", false);
    d.field = jget(o, "menu", false);
    d.key = jget(o, "important", false);
    d.healHp = jget(o, "soin_pv", 0);
    d.healMp = jget(o, "soin_pm", 0);
    d.revive = jget(o, "rappel", 0);
    d.capture = jget(o, "capture", 0.f);
    return d;
  });

  loadRules();

  MOVE_IX.clear(), SPECIES_IX.clear(), ITEM_IX.clear();
  for (size_t i = 0; i < MOVES.size(); i++) MOVE_IX[MOVES[i].id] = i;
  for (size_t i = 0; i < SPECIES.size(); i++) SPECIES_IX[SPECIES[i].id] = i;
  for (size_t i = 0; i < ITEMS.size(); i++) ITEM_IX[ITEMS[i].id] = i;
}

std::vector<std::string> checkData() {
  std::vector<std::string> err;
  auto dup = [&](auto& v, const char* what) {
    for (size_t i = 0; i < v.size(); i++)
      for (size_t k = 0; k < i; k++)
        if (v[i].id == v[k].id) err.push_back(std::string(what) + " en double : " + v[i].id);
  };
  dup(MOVES, "Technique");
  dup(SPECIES, "Espèce");
  dup(ITEMS, "Objet");
  dup(LOOKS, "Apparence");
  for (auto& s : SPECIES) {
    for (auto& l : s.learn)
      if (!hasMove(l.move)) err.push_back(s.name + " apprend une technique inconnue : " + l.move);
    if (!s.limit.empty() && !hasMove(s.limit)) err.push_back(s.name + " a une Limite inconnue : " + s.limit);
    bool tech = false;
    for (auto& l : s.learn)
      if (hasMove(l.move) && l.lvl <= 1 && moveInfo(l.move).cost == 0) tech = true;
    if (!tech) err.push_back(s.name + " ne connaît aucune technique gratuite au niveau 1");
  }
  auto& r = RULES;
  if (!hasSpecies(r.hero)) err.push_back("Héros de départ inconnu : " + r.hero);
  for (auto& s : r.starters)
    if (!hasSpecies(s)) err.push_back("Compagnon de départ inconnu : " + s);
  for (auto& [id, n] : r.startItems)
    if (!hasItem(id)) err.push_back("Objet de départ inconnu : " + id);
  for (auto& [id, c] : r.drops)
    if (!hasItem(id)) err.push_back("Butin inconnu : " + id);
  return err;
}

// ---------------------------------------------------------------------------
// Accès
// ---------------------------------------------------------------------------
template <class V>
static const typename V::value_type& findIx(const V& v, const std::unordered_map<std::string, size_t>& ix, const std::string& id, const char* what) {
  auto it = ix.find(id);
  if (it == ix.end()) throw std::runtime_error(std::string(what) + " inconnu : " + id);
  return v[it->second];
}
const Move& moveInfo(const std::string& id) { return findIx(MOVES, MOVE_IX, id, "Technique"); }
const Species& species(const std::string& id) { return findIx(SPECIES, SPECIES_IX, id, "Espèce"); }
const ItemDef& item(const std::string& id) { return findIx(ITEMS, ITEM_IX, id, "Objet"); }
bool hasMove(const std::string& id) { return MOVE_IX.count(id) > 0; }
bool hasSpecies(const std::string& id) { return SPECIES_IX.count(id) > 0; }
bool hasItem(const std::string& id) { return ITEM_IX.count(id) > 0; }
const std::vector<ItemDef>& allItems() { return ITEMS; }
const std::vector<Move>& allMoves() { return MOVES; }
const std::vector<Species>& allSpecies() { return SPECIES; }
const std::vector<Look>& allLooks() { return LOOKS; }
const Look& look(int i) { return LOOKS[(size_t)std::max(0, i) % LOOKS.size()]; }
int lookIndex(const std::string& id) {
  for (size_t i = 0; i < LOOKS.size(); i++)
    if (LOOKS[i].id == id) return (int)i;
  return -1;
}

// ---------------------------------------------------------------------------
// Combattants
// ---------------------------------------------------------------------------
std::string Fighter::name() const { return tag.empty() ? S().name : S().name + " " + tag; }

void Fighter::recalc() {
  const auto& b = S().base;
  const Rules& r = RULES;
  auto stat = [&](int base) { return base * lvl / r.statDiv + r.statBase; };
  mhp = b[0] * lvl / r.hpDiv + lvl * r.hpPerLvl + r.hpBase;
  mmp = b[1] * lvl / r.mpDiv + r.mpBase;
  atk = stat(b[2]);
  def = stat(b[3]);
  mag = stat(b[4]);
  spd = stat(b[5]);
}
int Fighter::need() const { return RULES.xpBase + int(lvl * lvl * RULES.xpSquare + 1e-6); }

std::vector<std::string> Fighter::techs() const {
  std::vector<std::string> v;
  for (auto& l : S().learn)
    if (l.lvl <= lvl && moveInfo(l.move).cost == 0) v.push_back(l.move);
  if (v.size() > 4) v.erase(v.begin(), v.end() - 4);
  return v;
}
std::vector<std::string> Fighter::spells() const {
  std::vector<std::string> v;
  for (auto& l : S().learn)
    if (l.lvl <= lvl && moveInfo(l.move).cost > 0) v.push_back(l.move);
  return v;
}

FighterP makeFighter(const std::string& id, int lvl) {
  auto f = std::make_shared<Fighter>();
  f->sp = id;
  f->lvl = std::max(1, lvl);
  f->recalc();
  f->hp = f->mhp;
  f->mp = f->mmp;
  return f;
}

std::vector<std::string> gainXp(Fighter& f, int xp) {
  std::vector<std::string> out;
  f.xp += xp;
  while (f.xp >= f.need()) {
    f.xp -= f.need();
    int oldHp = f.mhp, oldMp = f.mmp;
    f.lvl++;
    f.recalc();
    if (f.hp > 0) f.hp = std::min(f.mhp, f.hp + f.mhp - oldHp);
    f.mp = std::min(f.mmp, f.mp + f.mmp - oldMp);
    out.push_back(f.name() + " passe au niveau " + std::to_string(f.lvl) + " !");
    for (auto& l : f.S().learn)
      if (l.lvl == f.lvl) out.push_back(f.name() + " apprend " + moveInfo(l.move).name + " !");
  }
  return out;
}

static std::mt19937& rng() {
  static std::mt19937 r(std::random_device{}());
  return r;
}
float frand() { return std::uniform_real_distribution<float>(0.f, 1.f)(rng()); }
int irand(int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng()); }
