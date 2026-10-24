#include "data.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>
#include <unordered_map>

#include "i18n.hpp"
#include "tactics.hpp"

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
                               "champignon", "rocher", "feu_follet", "boss", "lezard", "golem", "serpent",
                               "chauve_souris", "loup", "tortue", "fantome", "cristal", "magma", "humain"};
static const char* TARGETS[] = {"ennemi", "tous_ennemis", "allie", "tous_allies", "allie_ko"};
static const char* KINDS[] = {"physique", "magique", "soin", "rappel", "statut"};
static const char* STATUSES[] = {"aucun", "poison", "brulure", "paralysie", "sommeil"};
static const char* STATUS_NAMES[] = {"", "Poison", "Brûlure", "Paralysie", "Sommeil"};
static const char* STATUS_TAGS[] = {"", "PSN", "BRL", "PAR", "SOM"};
static const uint32_t STATUS_COLORS[] = {0xffffff, 0xc58aff, 0xff8a4a, 0xffe14a, 0x9fb8ff};
static const char* STAGES[] = {"attaque", "defense", "magie", "resistance", "vitesse"};
static const char* STAGE_NAMES[] = {"Attaque", "Défense", "Magie", "Résistance", "Vitesse"};
static const char* HATS[] = {"aucune", "capuche", "chapeau", "bandeau", "casque", "foulard"};
static const char* WEAPONS[] = {"aucune", "epee", "baton", "hache", "dague", "arc", "lance"};
static const char* DIRS[] = {"haut", "bas", "gauche", "droite"};
static const char* ZONES[] = {"case", "ligne", "colonne", "croix", "tout"};
static const char* ZONE_NAMES[] = {"Une cible", "Une ligne", "Une colonne", "En croix", "Tout le camp"};
static const char* PACES[] = {"normale", "rapide", "lourde"};
static const char* PACE_NAMES[] = {"Normale", "Rapide", "Lourde"};

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
Status statusOf(const std::string& s) { return Status(nameIndex(STATUSES, s, "État")); }
const char* statusId(Status s) { return STATUSES[(int)s]; }
const char* statusName(Status s) { return T(STATUS_NAMES[(int)s]); }
const char* statusTag(Status s) { return T(STATUS_TAGS[(int)s]); }
uint32_t statusColor(Status s) { return STATUS_COLORS[(int)s]; }
int stageOf(const std::string& s) { return nameIndex(STAGES, s, "Statistique"); }
const char* stageName(int st) { return T(STAGE_NAMES[st]); }
const char* zoneId(Area z) { return ZONES[(int)z]; }
const char* zoneName(Area z) { return T(ZONE_NAMES[(int)z]); }
std::string cellName(int c) {
  static const char* COLS[GRID] = {"Avant", "Milieu avant", "Milieu arrière", "Arrière"};
  static const char* ROWS[GRID] = {"haut", "centre haut", "centre bas", "bas"};
  if (c < 0 || c >= N_CELLS) return "?";
  return std::string(T(COLS[cellCol(c)])) + " · " + T(ROWS[cellRow(c)]);
}
int boardDist(int a, int b) { return std::max(std::abs(boardX(a) - boardX(b)), std::abs(boardY(a) - boardY(b))); }
int boardSteps(int a, int b) { return std::abs(boardX(a) - boardX(b)) + std::abs(boardY(a) - boardY(b)); }
std::string boardName(int b) {
  static const char* COLS[GRID] = {"arrière", "milieu arrière", "milieu avant", "avant"};
  static const char* ROWS[GRID] = {"haut", "centre haut", "centre bas", "bas"};
  if (b < 0 || b >= BOARD_CELLS) return "?";
  int x = boardX(b);
  bool ours = x < GRID;
  return std::string(ours ? T("Notre camp, ") : T("Camp adverse, ")) + T(COLS[ours ? x : BOARD_W - 1 - x]) + " · " + T(ROWS[boardY(b)]);
}
bool inBoardZone(Area z, int center, int cell) {
  int dc = std::abs(boardX(center) - boardX(cell)), dr = std::abs(boardY(center) - boardY(cell));
  switch (z) {
    case Area::Single: return dc == 0 && dr == 0;
    case Area::Line: return dr == 0;
    case Area::Column: return dc == 0;
    case Area::Cross: return dc + dr <= 1;
    case Area::All: return true;
  }
  return false;
}
bool inZone(Area z, int center, int cell) {
  int dc = std::abs(cellCol(center) - cellCol(cell)), dr = std::abs(cellRow(center) - cellRow(cell));
  switch (z) {
    case Area::Single: return dc == 0 && dr == 0;
    case Area::Line: return dr == 0;    // même ligne : de l'avant jusqu'à l'arrière
    case Area::Column: return dc == 0;  // même colonne : tout un rang
    case Area::Cross: return dc + dr <= 1;
    case Area::All: return true;
  }
  return false;
}
const char* paceId(Pace p) { return PACES[(int)p]; }
const char* paceName(Pace p) { return T(PACE_NAMES[(int)p]); }

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
std::string typeDot(Type t) {
  char b[8];
  std::snprintf(b, sizeof b, "#%06x", t >= 0 && t < (int)TYPES.size() ? (unsigned)(TYPES[t].color & 0xffffff) : 0x8a92b8u);
  return b;
}
float typeEff(Type a, Type d) {
  if (a < 0 || d < 0 || a >= (int)CHART.size() || d >= (int)CHART.size()) return 1.f;
  return CHART[a][d];
}
float moveEff(const Move& m, const Species& s) {
  float e = typeEff(m.type, s.type) * (s.type2 >= 0 ? typeEff(m.type, s.type2) : 1.f);
  auto r = [&](const std::string& k) {
    auto it = s.resist.find(k);
    return it == s.resist.end() ? 1.f : it->second;
  };
  e *= r(TYPES[m.type].id);
  if (m.kind == Kind::Physical) e *= r("physique");
  if (m.kind == Kind::Magic) e *= r("magique");
  return e;
}
std::string typesName(const Species& s) { return s.type2 >= 0 ? std::string(typeName(s.type)) + "/" + typeName(s.type2) : typeName(s.type); }
bool hasType(const Species& s, Type t) { return s.type == t || s.type2 == t; }
const Rules::Talent* talentOf(const Species& s) {
  for (auto& t : RULES.talents)
    if (t.id == s.talent) return &t;
  return nullptr;
}
double talentValue(const Species& s, const char* kind) {
  const Rules::Talent* t = talentOf(s);
  return t && t->kind == kind ? t->value : 0;
}

bool duoMember(const std::string& spec, const Species& s) {
  if (spec == "humain") return s.human;
  if (spec.rfind("type:", 0) == 0) {
    if (s.human) return false;
    for (size_t k = 0; k < TYPES.size(); k++)
      if (TYPES[k].id == spec.substr(5)) return hasType(s, (int)k);
    return false;
  }
  return s.id == spec;
}

bool immuneTo(const Species& s, Status st) {
  if (talentValue(s, "sans_etats") > 0) return true;
  std::string id = statusId(st);
  auto in = [&](const std::vector<std::string>& v) { return std::find(v.begin(), v.end(), id) != v.end(); };
  if (in(s.immune)) return true;
  for (Type t : {s.type, s.type2})
    if (t >= 0 && in(TYPES[t].immune)) return true;
  return false;
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
      I("rencontres", "meute_max", "Meutes : ennemis au plus", r.packMax, 1, 6, 1),
      D("rencontres", "meute_distance", "Meutes : profondeur maximale (cases)", r.packDistance, 5, 200, 1),
      D("rencontres", "nature_dense", "Rencontres en plus au plus profond", r.packDense, 0, 3, .05),
      D("rencontres", "chromatique", "Chance d'une créature chromatique", r.chroma, 0, 1, .001),
      D("rencontres", "alpha", "Chance d'un alpha à la tête d'une meute", r.alpha, 0, 1, .01),
      I("rencontres", "alpha_niveaux", "Niveaux en plus de l'alpha", r.alphaLevels, 0, 20, 1),
      D("rencontres", "part_nuit", "Part des rencontres de nuit (liste « nuit »)", r.nightShare, 0, 1, .05),
      D("rencontres", "par_allie", "Ennemis au moins par allié en combat", r.perAlly, 0, 2, .05),
      D("rencontres", "embuscade", "Chance d'une embuscade (2 créatures ou plus)", r.ambush, 0, 1, .01),
      I("rencontres", "ecart_niveau", "Niveaux au plus sous l'équipe (0 : non)", r.levelGap, 0, 50, 1),
      I("rencontres", "plafond_niveau", "… sans dépasser la zone de", r.levelCap, 0, 50, 1),
      D("combat", "atb_base", "Vitesse ATB de base", r.atbBase, 0, 100, 1),
      D("combat", "atb_vitesse", "Multiplicateur de vitesse ATB", r.atbSpeed, .1, 5, .1),
      D("combat", "fuite", "Chance de fuite", r.flee, 0, 1, .05),
      D("combat", "limite_gain", "Gain de Limite (coups reçus)", r.limitGain, 0, 500, 10),
      D("combat", "bonus_meme_type", "Bonus si technique du même type", r.stab, 1, 3, .05),
      D("combat", "alea_min", "Dégâts aléatoires minimum", r.spreadMin, .5, 1, .01),
      D("combat", "diviseur_degats", "Diviseur des dégâts", r.dmgDivisor, 5, 200, 1),
      D("combat", "critique_mult", "Multiplicateur des critiques", r.critMult, 1, 5, .05),
      D("combat", "etage", "Effet d'un niveau de bonus/malus", r.stageStep, 0, 1, .05),
      I("combat", "precision_base", "Précision par défaut (%)", r.accBase, 0, 200, 1),
      I("combat", "esquive_base", "Esquive par défaut (%)", r.evaBase, 0, 100, 1),
      I("combat", "critique_base", "Critique par défaut (%)", r.critBase, 0, 100, 1),
      I("combat", "jauge_rapide", "Technique rapide : jauge de départ (%)", r.quickGauge, 0, 90, 5),
      D("combat", "zone_voisins", "Zone d'effet : dégâts autour de la cible", r.zoneSide, 0, 1, .05),
      D("combat", "boss_annonce", "Boss : chance d'annoncer une attaque", r.bossWarn, 0, 1, .05),
      D("combat", "boss_annonce_puissance", "Boss : puissance de l'attaque annoncée", r.bossBurst, 1, 5, .1),
      I("combat", "retard_lourde", "Technique lourde : retard de la jauge (%)", r.heavyDelay, 0, 200, 5),
      D("combat", "compagnon_atb", "Compagnons : vitesse de la jauge ATB", r.companionAtb, .1, 2, .05),
      D("combat", "garde", "Garde (Attendre) : part des dégâts reçus", r.guard, .1, 1, .05),
      D("combat", "intention", "Les ennemis annoncent à ce point de jauge (0 : jamais)", r.intentAt, 0, 100, 5),
      I("brisure", "bouclier", "Brisure : bouclier d'un ennemi", r.breakShield, 0, 20, 1),
      I("brisure", "bouclier_boss", "Brisure : bouclier d'un boss", r.breakShieldBoss, 0, 40, 1),
      D("brisure", "degats", "Brisure : dégâts reçus par un ennemi brisé", r.breakDamage, 1, 5, .05),
      I("gemmes", "chatons_arme", "Gemmes : chatons d'une arme", r.socketWeapon, 0, 4, 1),
      I("gemmes", "chatons_armure", "Gemmes : chatons d'une armure", r.socketArmor, 0, 4, 1),
      I("gemmes", "chatons_accessoire", "Gemmes : chatons d'un accessoire", r.socketAccessory, 0, 4, 1),
      I("gemmes", "forge_chaton", "Gemmes : niveau de forge qui ajoute un chaton", r.socketForge, 1, 9, 1),
      D("roles", "protection", "Protecteur : dégâts retirés à ses voisins", r.roleGuard, 0, 1, .05),
      D("roles", "chef", "Chef : vitesse ATB en plus de ses alliés", r.roleChief, 0, 2, .05),
      D("damier", "choc", "Pousser : choc (part des dégâts) si la case est prise", r.shoveClash, 0, 2, .05),
      I("damier", "terrain_min", "Terrain : cases spéciales par camp (minimum)", r.terrainMin, 0, 8, 1),
      I("damier", "terrain_max", "Terrain : cases spéciales par camp (maximum)", r.terrainMax, 0, 8, 1),
      I("damier", "mouvement", "Échiquier : cases parcourues par tour", r.moveRange, 0, 6, 1),
      I("damier", "allonge_boss", "Échiquier : portée du corps à corps d'un boss", r.bossReach, 1, 4, 1),
      D("damier", "dos", "Échiquier : dégâts d'un coup dans le dos", r.backStab, 1, 3, .05),
      D("damier", "encercle", "Échiquier : dégâts en plus par adversaire au contact (au-delà du 1er)", r.surround, 0, 1, .05),
      D("damier", "contact", "Échiquier : distance ou magie au contact d'un ennemi", r.contact, .1, 1, .05),
      I("damier", "percee", "Échiquier : Percée (bonus en atteignant le bord adverse)", r.breakthrough, 0, 3, 1),
      I("damier", "obstacles_min", "Échiquier : obstacles sur le plateau (minimum)", r.obstacleMin, 0, 6, 1),
      I("damier", "obstacles_max", "Échiquier : obstacles sur le plateau (maximum)", r.obstacleMax, 0, 6, 1),
      I("duo", "jauge", "Duo : jauge ATB du partenaire (%)", r.duoGauge, 0, 100, 5),
      D("etats", "poison_degats", "Poison : part des PV perdus par tour", r.poisonDmg, 0, 1, .01),
      D("etats", "brulure_degats", "Brûlure : part des PV perdus par tour", r.burnDmg, 0, 1, .01),
      D("etats", "brulure_attaque", "Brûlure : multiplicateur d'Attaque", r.burnAtk, 0, 1, .05),
      D("etats", "paralysie_vitesse", "Paralysie : multiplicateur de vitesse", r.paraSpeed, 0, 1, .05),
      D("etats", "paralysie_blocage", "Paralysie : chance de rester bloqué", r.paraSkip, 0, 1, .05),
      I("etats", "sommeil_min", "Sommeil : tours minimum", r.sleepMin, 1, 10, 1),
      I("etats", "sommeil_max", "Sommeil : tours maximum", r.sleepMax, 1, 10, 1),
      I("recompenses", "xp_par_niveau", "Expérience par niveau d'ennemi", r.xpPerLevel, 0, 100, 1),
      D("recompenses", "exp_ecart", "Exp. en moins par niveau sous l'équipe", r.xpGapLoss, 0, 1, .01),
      I("recompenses", "exp_marge", "… au-delà de (niveaux)", r.xpGapFree, 0, 50, 1),
      D("recompenses", "exp_min", "… sans descendre sous", r.xpGapMin, 0, 1, .05),
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
      D("multijoueur", "gardien_par_joueur", "Gardien à plusieurs : PV en plus par joueur", r.coopGuardian, 0, 2, .05),
      I("tactiques", "lignes_depart", "Lignes de tactiques au départ", r.tacticStart, 1, 20, 1),
      I("tactiques", "niveaux_par_ligne", "Niveaux pour une ligne de plus", r.tacticPerLvl, 1, 50, 1),
      I("tactiques", "lignes_max", "Lignes de tactiques au maximum", r.tacticMax, 1, 20, 1),
  };
}

// Documents JSON en mémoire : la source de vérité. Les réglages les modifient
// puis appellent rebuildData() ; « Enregistrer » les écrit dans data/.
static const char* FILES[N_DATAFILES] = {"types.json", "techniques.json", "especes.json", "objets.json", "apparences.json", "regles.json"};
static Json DOCS[N_DATAFILES];
const char* dataFileName(DataFile f) { return FILES[f]; }
Json& dataDoc(DataFile f) { return DOCS[f]; }
void saveDataDoc(DataFile f) { writeJson(FILES[f], DOCS[f]); }

static void loadRules(const Json& j) {
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
  if (j.contains("recompenses"))
    for (auto& b : j["recompenses"].value("butin_type", Json::array()))
      r.typeDrops.push_back({b.at("type").get<std::string>(), b.at("objet").get<std::string>(), b.at("chance").get<double>()});
  if (j.contains("damier")) {
    r.flyTypes = jget(j["damier"], "vol", std::vector<std::string>{});
    r.fastTypes = jget(j["damier"], "rapides", std::vector<std::string>{});
    Json obs = j["damier"].value("decors_obstacles", Json::object());  // variable : la boucle doit la garder en vie
    for (auto& [th, v] : obs.items()) r.obstacleSprites[th] = v.get<std::vector<std::string>>();
  }
  for (auto& w : j.value("meteo", Json::array())) {
    Rules::Weather x;
    x.id = w.at("id").get<std::string>(), x.name = tr(w.at("nom").get<std::string>());
    x.desc = tr(jget<std::string>(w, "description", "")), x.fx = jget<std::string>(w, "effet", "");
    x.themes = jget(w, "decors", std::vector<std::string>{});
    x.chance = jget(w, "chance", 0.0), x.acc = jget(w, "precision", 0);
    Json tm = w.value("types", Json::object());
    for (auto& [ty, v] : tm.items()) x.typeMult[ty] = v.get<double>();
    r.weathers.push_back(x);
  }
  for (auto& t : j.value("terrains", Json::array())) {
    Rules::Terrain x;
    x.id = t.at("id").get<std::string>(), x.name = tr(t.at("nom").get<std::string>());
    x.desc = tr(jget<std::string>(t, "description", "")), x.kind = t.at("effet").get<std::string>();
    x.value = jget(t, "valeur", 0.0);
    x.immune = jget(t, "insensibles", std::vector<std::string>{});
    x.themes = jget(t, "decors", std::vector<std::string>{});
    x.color = parseColor(t.value("couleur", Json("#ffffff")));
    r.terrains.push_back(x);
  }
  {
    Json duo = j.value("duo", Json::object());
    for (auto& d : duo.value("liste", Json::array()))
      r.duos.push_back({jget(d, "membres", std::vector<std::string>{}), d.at("technique").get<std::string>()});
  }
  for (auto& a : j.value("conseils", Json::array()))
    r.advice.push_back({a.at("drapeau").get<std::string>(), a.at("boss").get<std::string>(), tr(jget<std::string>(a, "lieu", "")),
                        jget<std::string>(a, "gemme", ""), tr(jget<std::string>(a, "texte", ""))});
  for (auto& t : j.value("astuces", Json::array()))
    r.tips.push_back({t.at("id").get<std::string>(), tr(jget<std::string>(t, "aide", "")), tr(t.at("texte").get<std::string>())});
  for (auto& t : j.value("talents", Json::array()))
    r.talents.push_back({t.at("id").get<std::string>(), tr(t.at("nom").get<std::string>()), tr(jget<std::string>(t, "description", "")),
                         t.at("effet").get<std::string>(), jget(t, "valeur", 0.0)});
  for (auto& l : j.value("forge", Json::array())) {
    Rules::ForgeLevel f;
    f.bonus = jget(l, "bonus", 0.0);
    f.gold = jget(l, "or", 0);
    Json need = l.value("objets", Json::object());  // variable : la boucle doit la garder en vie
    for (auto& [k, v] : need.items()) f.items.push_back({k, v.get<int>()});
    r.forge.push_back(f);
  }
  if (j.contains("tactiques")) {
    for (auto& t : j["tactiques"].value("defaut", Json::array())) r.tactics.push_back(tacticFromJson(t));
    Json profiles = j["tactiques"].value("profils", Json::array());  // variable : la boucle doit la garder en vie
    for (auto& p : profiles) {
      Rules::TacticProfile tp{jget<std::string>(p, "id", ""), tr(jget<std::string>(p, "nom", "")), tr(jget<std::string>(p, "aide", "")), {}};
      for (auto& t : p.value("regles", Json::array())) tp.rules.push_back(tacticFromJson(t));
      r.tacticProfiles.push_back(tp);
    }
  }
  RULES = r;
}

// ---------------------------------------------------------------------------
// Chargement
// ---------------------------------------------------------------------------
template <class T, class F>
static void loadList(DataFile df, std::vector<T>& out, F parse) {
  out.clear();
  const char* file = FILES[df];
  const Json& j = DOCS[df];
  if (!j.is_array()) throw std::runtime_error(std::string("data/") + file + " doit contenir une liste [ … ]");
  for (auto& o : j) {
    std::string id = jget<std::string>(o, "id", "?");
    try {
      out.push_back(parse(o));
    } catch (const std::exception& e) {
      throw std::runtime_error(std::string("data/") + file + ", « " + id + " » : " + e.what());
    }
  }
}

static Effect parseEffect(const Json& o) {
  Effect e;
  if (o.contains("statut")) e.status = statusOf(o["statut"].get<std::string>());
  if (o.contains("stat")) {
    e.stat = stageOf(o["stat"].get<std::string>());
    e.stages = jget(o, "niveaux", 1);
  }
  e.self = jget<std::string>(o, "sur", "cible") == "lanceur";
  e.cure = jget(o, "guerison", false);
  e.chance = jget(o, "chance", 100);
  return e;
}
void loadData() {
  for (int f = 0; f < N_DATAFILES; f++) DOCS[f] = readJson(FILES[f]);
  rebuildData();
}

void rebuildData() {
  loadRules(DOCS[DF_RULES]);
  // Types et table d'efficacité
  const Json& t = DOCS[DF_TYPES];
  TYPES.clear();
  for (auto& o : t.at("types"))
    TYPES.push_back({o.at("id").get<std::string>(), tr(o.at("nom").get<std::string>()), o.at("nom").get<std::string>(), parseColor(o.value("couleur", Json("#c8c8c8"))),
                     jget(o, "immunites", std::vector<std::string>{})});
  CHART.assign(TYPES.size(), std::vector<float>(TYPES.size(), 1.f));
  Json chart = t.value("efficacite", Json::object());  // variable : la boucle doit la garder en vie
  for (auto& [a, row] : chart.items())
    for (auto& [d, v] : row.items()) CHART[typeOf(a)][typeOf(d)] = v.get<float>();

  loadList(DF_MOVES, MOVES, [](const Json& o) {
    Move m;
    m.id = o.at("id").get<std::string>();
    m.name = tr(o.at("nom").get<std::string>());
    m.type = typeOf(o.at("type").get<std::string>());
    m.kind = Kind(nameIndex(KINDS, o.at("genre").get<std::string>(), "Genre"));
    m.target = Target(nameIndex(TARGETS, o.at("cible").get<std::string>(), "Cible"));
    m.power = o.at("puissance").get<int>();
    m.cost = jget(o, "cout", 0);
    m.desc = tr(jget<std::string>(o, "description", ""));
    m.acc = jget(o, "precision", 100);
    m.critBonus = jget(o, "critique", 0);
    m.pace = Pace(nameIndex(PACES, jget<std::string>(o, "rythme", "normale"), "Rythme"));
    bool all = m.target == Target::AllEnemies || m.target == Target::AllAllies;
    m.zone = Area(nameIndex(ZONES, jget<std::string>(o, "zone", all ? "tout" : "case"), "Zone"));
    m.melee = jget<std::string>(o, "portee", m.kind == Kind::Physical ? "corps" : "distance") == "corps";
    std::string sh = jget<std::string>(o, "deplace", "");
    m.shove = sh == "pousse" ? 1 : sh == "tire" ? -1 : 0;
    m.motion = jget<std::string>(o, "mouvement", "");
    if (!sh.empty() && !m.shove) throw std::runtime_error("Technique " + m.id + " : « deplace » doit valoir pousse ou tire");
    if (o.contains("effet")) m.effects.push_back(parseEffect(o["effet"]));
    for (auto& e : o.value("effets", Json::array())) m.effects.push_back(parseEffect(e));
    return m;
  });

  loadList(DF_LOOKS, LOOKS, [](const Json& o) {
    Look l;
    l.id = o.at("id").get<std::string>();
    l.name = tr(jget<std::string>(o, "nom", l.id));
    l.hair = parseColor(o.at("cheveux"));
    l.skin = parseColor(o.at("peau"));
    l.top = parseColor(o.at("haut"));
    l.bottom = parseColor(o.at("bas"));
    l.hat = nameIndex(HATS, jget<std::string>(o, "coiffe", "aucune"), "Coiffe");
    l.weapon = nameIndex(WEAPONS, jget<std::string>(o, "arme", "aucune"), "Arme");
    for (auto& a : jget(o, "aussi", std::vector<std::string>{})) l.aliases.push_back(tr(a));
    l.sprite = jget<std::string>(o, "sprite", "");
    return l;
  });

  loadList(DF_SPECIES, SPECIES, [](const Json& o) {
    Species s;
    s.id = o.at("id").get<std::string>();
    s.name = tr(o.at("nom").get<std::string>());
    if (o.contains("types")) {
      auto ts = o["types"].get<std::vector<std::string>>();
      if (ts.empty() || ts.size() > 2) throw std::runtime_error("« types » doit contenir 1 ou 2 types");
      s.type = typeOf(ts[0]);
      if (ts.size() > 1) s.type2 = typeOf(ts[1]);
    } else s.type = typeOf(o.at("type").get<std::string>());
    const Json& b = o.at("base");
    int def = b.at("defense").get<int>(), mag = b.at("magie").get<int>();
    s.base = {b.at("pv").get<int>(), b.at("pm").get<int>(), b.at("attaque").get<int>(), def, mag,
              jget(b, "resistance", (def + mag) / 2), b.at("vitesse").get<int>()};
    s.acc = jget(o, "precision", RULES.accBase);
    s.eva = jget(o, "esquive", RULES.evaBase);
    s.crit = jget(o, "critique", RULES.critBase);
    s.desc = tr(jget<std::string>(o, "description", ""));
    Json resist = o.value("resistances", Json::object());  // variable : la boucle doit la garder en vie
    for (auto& [k, v] : resist.items()) s.resist[k] = v.get<float>();
    s.immune = jget(o, "immunites", std::vector<std::string>{});
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
    s.role = tr(jget<std::string>(o, "role", ""));
    for (auto& l : o.at("apprend")) s.learn.push_back({l.at(0).get<int>(), l.at(1).get<std::string>()});
    s.limit = jget<std::string>(o, "limite", "");  // vide : pas de Limite
    if (o.contains("evolution")) {
      s.evolveTo = o["evolution"].at("espece").get<std::string>();
      s.evolveLvl = o["evolution"].at("niveau").get<int>();
    }
    s.size = jget(o, "taille", 1.f);
    s.sprite = jget<std::string>(o, "sprite", "");
    s.shield = jget(o, "bouclier", 0);
    s.talent = jget<std::string>(o, "talent", "");
    s.combatRole = jget<std::string>(o, "role_combat", "");
    for (auto& t : o.value("tactiques", Json::array())) s.tactics.push_back(tacticFromJson(t));
    return s;
  });

  loadList(DF_ITEMS, ITEMS, [](const Json& o) {
    ItemDef d;
    d.id = o.at("id").get<std::string>();
    d.name = tr(o.at("nom").get<std::string>());
    d.desc = tr(jget<std::string>(o, "description", ""));
    d.icon = jget<std::string>(o, "icone", "");
    d.price = jget(o, "prix", 0);
    d.battle = jget(o, "combat", false);
    d.field = jget(o, "menu", false);
    d.key = jget(o, "important", false);
    d.healHp = jget(o, "soin_pv", 0);
    d.healMp = jget(o, "soin_pm", 0);
    d.revive = jget(o, "rappel", 0);
    d.capture = jget(o, "capture", 0.f);
    d.cure = jget(o, "soin_statut", false);
    d.trophy = jget(o, "trophee", false);
    if (o.contains("equipement")) {
      static const char* SLOTS[] = {"arme", "armure", "accessoire"};
      std::string s = o["equipement"].get<std::string>();
      for (int i = 0; i < N_GEAR; i++)
        if (s == SLOTS[i]) d.slot = i;
      if (d.slot < 0) throw std::runtime_error("Objet « " + d.id + " » : équipement inconnu « " + s + " » (arme, armure, accessoire)");
      static const char* STATS[] = {"pv", "pm", "attaque", "defense", "magie", "resistance", "vitesse", "precision", "esquive", "critique"};
      Json bonus = o.value("bonus", Json::object());
      for (auto& [k, v] : bonus.items()) {
        int i = 0;
        while (i < N_GEAR_STATS && k != STATS[i]) i++;
        if (i == N_GEAR_STATS) throw std::runtime_error("Objet « " + d.id + " » : bonus inconnu « " + k + " »");
        d.bonus[i] = v.get<int>();
      }
      d.moveBonus = jget(o, "mouvement", 0);
    }
    d.owner = jget<std::string>(o, "pour", "");
    if (o.contains("gemme")) {
      const Json& g = o["gemme"];
      static const char* STATS[] = {"pv", "pm", "attaque", "defense", "magie", "resistance", "vitesse", "precision", "esquive", "critique"};
      d.gem = true;
      d.gemMove = jget<std::string>(g, "technique", "");
      d.gemSupport = jget<std::string>(g, "soutien", "");
      d.gemValue = jget(g, "valeur", 0.0);
      Json bonus = g.value("bonus", Json::object());
      for (auto& [k, v] : bonus.items()) {
        int i = 0;
        while (i < N_GEAR_STATS && k != STATS[i]) i++;
        if (i == N_GEAR_STATS) throw std::runtime_error("Gemme « " + d.id + " » : bonus inconnu « " + k + " »");
        d.gemBonus[i] = v.get<double>();
      }
    }
    return d;
  });

  // Forge : chaque arme et armure existe aussi en +1, +2… (règles « forge ») ; chaque niveau
  // ajoute une part du bonus d'origine (arrondie au-dessus) et coûte l'or de la forge en plus
  for (size_t i = 0, n = ITEMS.size(); i < n; i++) {
    if (ITEMS[i].slot != G_WEAPON && ITEMS[i].slot != G_ARMOR) continue;
    const ItemDef base = ITEMS[i];  // copie : la liste grandit
    int gold = 0;
    for (size_t L = 1; L <= RULES.forge.size(); L++) {
      ItemDef d = base;
      gold += RULES.forge[L - 1].gold;
      d.id = forgedId(base.id, (int)L);
      d.name = base.name + " +" + std::to_string(L);
      d.forged = (int)L, d.base = base.id;
      d.price = base.price + gold;
      for (int k = 0; k < N_GEAR_STATS; k++)
        if (base.bonus[k] > 0) d.bonus[k] = base.bonus[k] + (int)std::ceil(base.bonus[k] * RULES.forge[L - 1].bonus - 1e-6);
      ITEMS.push_back(d);
    }
  }

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
    for (auto& [k, v] : s.resist) {
      bool ok = k == "physique" || k == "magique";
      for (auto& t : TYPES) ok = ok || t.id == k;
      if (!ok) err.push_back(s.name + " : résistance inconnue « " + k + " » (un type, physique ou magique)");
    }
    for (auto& st : s.immune)
      if (std::find(std::begin(STATUSES) + 1, std::end(STATUSES), st) == std::end(STATUSES))
        err.push_back(s.name + " : immunité à un état inconnu « " + st + " »");
    for (auto& e : checkTactics(s.tactics, s.name)) err.push_back(e);
  }
  for (auto& d : ITEMS)
    if (!d.owner.empty() && (!hasSpecies(d.owner) || d.slot < 0))
      err.push_back("Objet « " + d.id + " » : « pour » une espèce inconnue ou un objet qui ne s'équipe pas : " + d.owner);
  for (auto& t : TYPES)
    for (auto& st : t.immune)
      if (std::find(std::begin(STATUSES) + 1, std::end(STATUSES), st) == std::end(STATUSES))
        err.push_back("Type " + t.name + " : immunité à un état inconnu « " + st + " »");
  for (auto& m : MOVES)
    for (auto& e : m.effects)
      if (e.status == Status::None && e.stat < 0 && !e.cure) err.push_back("Technique " + m.name + " : effet vide");
  auto& r = RULES;
  if (!hasSpecies(r.hero)) err.push_back("Héros de départ inconnu : " + r.hero);
  for (auto& s : r.starters)
    if (!hasSpecies(s)) err.push_back("Compagnon de départ inconnu : " + s);
  for (auto& [id, n] : r.startItems)
    if (!hasItem(id)) err.push_back("Objet de départ inconnu : " + id);
  for (auto& [id, c] : r.drops)
    if (!hasItem(id)) err.push_back("Butin inconnu : " + id);
  static const char* KINDS_T[] = {"resiste_physique", "resiste_magie", "rage", "soin_voisins", "regeneration", "meditation", "esquive_arriere",
                                  "vif", "briseur", "critique", "sans_etats", "intimidation", "piquant", "cuirasse"};
  for (auto& t : r.talents)
    if (std::find(std::begin(KINDS_T), std::end(KINDS_T), t.kind) == std::end(KINDS_T))
      err.push_back("Talent " + t.name + " : effet inconnu « " + t.kind + " »");
  for (auto& s : SPECIES)
    if (!s.combatRole.empty() && s.combatRole != "protecteur" && s.combatRole != "soigneur" && s.combatRole != "chef")
      err.push_back("Espèce " + s.name + " : rôle en combat inconnu « " + s.combatRole + " » (protecteur, soigneur ou chef)");
  for (auto& s : SPECIES)
    if (!s.talent.empty() && !talentOf(s)) err.push_back("Espèce " + s.name + " : talent inconnu « " + s.talent + " »");
  static const char* KINDS_TR[] = {"degats", "poison", "soin", "lent", "esquive", "magie"};
  for (auto& t : r.terrains)
    if (std::find(std::begin(KINDS_TR), std::end(KINDS_TR), t.kind) == std::end(KINDS_TR))
      err.push_back("Terrain " + t.name + " : effet inconnu « " + t.kind + " »");
  static const char* THEMES_OK[] = {"vallee", "cendres", "grotte", "foret", "neige", "interieur"};
  for (auto& w : r.weathers)
    for (auto& th : w.themes)
      if (std::find(std::begin(THEMES_OK), std::end(THEMES_OK), th) == std::end(THEMES_OK)) err.push_back("Météo " + w.name + " : décor inconnu « " + th + " »");
  for (auto& t : r.terrains)
    for (auto& th : t.themes)
      if (std::find(std::begin(THEMES_OK), std::end(THEMES_OK), th) == std::end(THEMES_OK)) err.push_back("Terrain " + t.name + " : décor inconnu « " + th + " »");
  for (auto& s : SPECIES)
    if (!s.evolveTo.empty()) {
      if (!hasSpecies(s.evolveTo)) err.push_back("Espèce " + s.name + " : évolution inconnue « " + s.evolveTo + " »");
      else if (species(s.evolveTo).human || s.human) err.push_back("Espèce " + s.name + " : un humain n'évolue pas");
      if (s.evolveLvl < 2) err.push_back("Espèce " + s.name + " : niveau d'évolution trop bas");
    }
  auto knownType = [](const std::string& id) {
    for (auto& t : TYPES)
      if (t.id == id) return true;
    return false;
  };
  for (auto& w : r.weathers)
    for (auto& [ty, v] : w.typeMult)
      if (!knownType(ty)) err.push_back("Météo " + w.name + " : type inconnu « " + ty + " »");
  for (auto& t : r.terrains)
    for (auto& ty : t.immune)
      if (!knownType(ty)) err.push_back("Terrain " + t.name + " : type inconnu « " + ty + " »");
  for (auto& a : r.advice) {
    if (!hasSpecies(a.boss)) err.push_back("Conseil : boss inconnu « " + a.boss + " »");
    if (!a.gem.empty() && !hasItem(a.gem)) err.push_back("Conseil : gemme inconnue « " + a.gem + " »");
  }
  for (auto& d : r.duos) {
    if (!hasMove(d.move)) err.push_back("Duo : technique inconnue « " + d.move + " »");
    if (d.members.size() != 2) err.push_back("Duo " + d.move + " : il faut deux membres");
    for (auto& m : d.members) {
      bool ok = m == "humain" || (m.rfind("type:", 0) == 0 && knownType(m.substr(5)));
      for (auto& s : SPECIES) ok = ok || s.id == m;
      if (!ok) err.push_back("Duo " + d.move + " : membre inconnu « " + m + " »");
    }
  }
  static const char* SUPPORTS[] = {"vol_vie", "contre", "eveil", "briseur"};
  for (auto& d : ITEMS)
    if (d.gem) {
      if (!d.gemMove.empty() && !hasMove(d.gemMove)) err.push_back("Gemme " + d.name + " : technique inconnue « " + d.gemMove + " »");
      if (!d.gemSupport.empty() && std::find(std::begin(SUPPORTS), std::end(SUPPORTS), d.gemSupport) == std::end(SUPPORTS))
        err.push_back("Gemme " + d.name + " : soutien inconnu « " + d.gemSupport + " » (vol_vie, contre, eveil, briseur)");
    }
  for (auto& td : r.typeDrops) {
    if (!hasItem(td.item)) err.push_back("Butin par type : objet inconnu « " + td.item + " »");
    bool known = false;
    for (auto& t : TYPES) known = known || t.id == td.type;
    if (!known) err.push_back("Butin par type : type inconnu « " + td.type + " »");
  }
  for (size_t L = 0; L < r.forge.size(); L++)
    for (auto& [id, n] : r.forge[L].items)
      if (id != "trophee" && !hasItem(id)) err.push_back("Forge +" + std::to_string(L + 1) + " : matériau inconnu « " + id + " »");
  for (auto& e : checkTactics(r.tactics, "Tactiques de départ")) err.push_back(e);
  for (auto& p : r.tacticProfiles)
    for (auto& e : checkTactics(p.rules, "Profil de tactiques « " + p.id + " »")) err.push_back(e);
  if (r.tacticMax < r.tacticStart) err.push_back("Tactiques : le maximum de lignes est plus petit que le départ");
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
float paceGauge(const Move& m) {
  return m.pace == Pace::Quick ? float(RULES.quickGauge) : m.pace == Pace::Heavy ? -float(RULES.heavyDelay) : 0.f;
}
float paceTime(const Move& m) { return (100 - paceGauge(m)) / 100; }
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
  mhp = b[B_HP] * lvl / r.hpDiv + lvl * r.hpPerLvl + r.hpBase;
  mmp = b[B_MP] * lvl / r.mpDiv + r.mpBase;
  atk = stat(b[B_ATK]);
  def = stat(b[B_DEF]);
  mag = stat(b[B_MAG]);
  res = stat(b[B_RES]);
  spd = stat(b[B_SPD]);
  acc = S().acc;
  eva = S().eva;
  crit = S().crit;
  for (auto& id : gear) {  // bonus de l'équipement
    if (id.empty() || !hasItem(id)) continue;
    const auto& g = item(id).bonus;
    mhp += g[GS_HP], mmp += g[GS_MP], atk += g[GS_ATK], def += g[GS_DEF], mag += g[GS_MAG], res += g[GS_RES], spd += g[GS_SPD];
    acc += g[GS_ACC], eva += g[GS_EVA], crit += g[GS_CRIT];
  }
  std::array<double, N_GEAR_STATS> pct{};  // gemmes de bonus : en pourcentage
  for (auto& slot : gems)
    for (auto& id : slot)
      if (hasItem(id))
        for (int k = 0; k < N_GEAR_STATS; k++) pct[k] += item(id).gemBonus[k];
  auto up = [&](int& v, int k) { v += int(std::lround(v * pct[k])); };
  up(mhp, GS_HP), up(mmp, GS_MP), up(atk, GS_ATK), up(def, GS_DEF), up(mag, GS_MAG), up(res, GS_RES), up(spd, GS_SPD);
  acc += int(pct[GS_ACC] * 100), eva += int(pct[GS_EVA] * 100), crit += int(pct[GS_CRIT] * 100);
  mhp = std::max(1, mhp), mmp = std::max(0, mmp);
}

static const char* GEAR_SLOTS[N_GEAR] = {"Arme", "Armure", "Accessoire"};
static const char* GEAR_STATS[N_GEAR_STATS] = {"PV", "PM", "Attaque", "Défense", "Magie", "Résistance", "Vitesse", "Précision", "Esquive", "Critique"};
const char* gearSlotName(int s) { return T(GEAR_SLOTS[std::clamp(s, 0, N_GEAR - 1)]); }
const char* gearStatName(int s) { return T(GEAR_STATS[std::clamp(s, 0, N_GEAR_STATS - 1)]); }
std::string ItemDef::bonusText() const {
  std::string t;
  for (int i = 0; i < N_GEAR_STATS; i++)
    if (bonus[i]) t += (t.empty() ? "" : ", ") + std::string(bonus[i] > 0 ? "+" : "") + std::to_string(bonus[i]) + (i >= GS_ACC ? " % " : " ") + GEAR_STATS[i];
  return t;
}
std::string forgedId(const std::string& base, int level) { return level > 0 ? base + "+" + std::to_string(level) : base; }
bool forgeable(const ItemDef& d) { return (d.slot == G_WEAPON || d.slot == G_ARMOR) && d.forged < (int)RULES.forge.size(); }
bool canEvolve(const Fighter& f) {
  const Species& s = f.S();
  return !s.evolveTo.empty() && hasSpecies(s.evolveTo) && f.lvl >= s.evolveLvl;
}
void evolve(Fighter& f) {
  if (!canEvolve(f)) return;
  float hp = f.mhp ? float(f.hp) / f.mhp : 1, mp = f.mmp ? float(f.mp) / f.mmp : 1;
  bool alive = f.hp > 0;
  f.sp = f.S().evolveTo;
  f.evolveLater = false;
  f.recalc();
  f.hp = alive ? std::clamp(int(std::lround(f.mhp * hp)), 1, f.mhp) : 0;
  f.mp = std::clamp(int(std::lround(f.mmp * mp)), 0, f.mmp);
}
int gemSockets(const ItemDef& d) {
  const Rules& r = RULES;
  int n = d.slot == G_WEAPON ? r.socketWeapon : d.slot == G_ARMOR ? r.socketArmor : d.slot == G_ACCESSORY ? r.socketAccessory : 0;
  return n + (d.forged >= r.socketForge ? 1 : 0);
}
double gemSupport(const Fighter& f, const char* kind) {
  double v = 0;
  for (auto& slot : f.gems)
    for (auto& id : slot)
      if (hasItem(id) && item(id).gemSupport == kind) v += item(id).gemValue;
  return v;
}
std::vector<std::string> trimGems(Fighter& f) {
  std::vector<std::string> out;
  for (int s = 0; s < N_GEAR; s++) {
    auto& v = f.gems[(size_t)s];
    int n = f.gear[(size_t)s].empty() || !hasItem(f.gear[(size_t)s]) ? 0 : gemSockets(item(f.gear[(size_t)s]));
    while ((int)v.size() > n) out.push_back(v.back()), v.pop_back();
  }
  return out;
}

bool canEquip(const Fighter& f, const ItemDef& d) {
  if (!d.owner.empty() && f.sp != d.owner) return false;  // l'arme d'un héros (champ « pour »)
  return d.slot == G_ACCESSORY || (d.slot >= 0 && f.S().human);
}

// Héros ? (faux pour une espèce disparue : monde d'une expédition déjà remis en place)
static bool hero(const FighterP& f) { return f && hasSpecies(f->sp) && f->S().human; }
// Créatures de l'équipe choisies par un héros (elles ne servent pas de compagnon « par défaut » à un autre)
static bool chosen(const std::vector<FighterP>& team, const FighterP& c) {
  for (auto& h : team)
    if (hero(h) && h->companion.lock() == c) return true;
  return false;
}
static bool inTeam(const std::vector<FighterP>& team, const FighterP& f) { return f && std::find(team.begin(), team.end(), f) != team.end(); }

std::vector<FighterP> frontOf(const std::vector<FighterP>& team, int heroes) {
  std::vector<FighterP> v, all;
  for (auto& f : team)
    if (hero(f) && f->alive() && (int)v.size() < heroes) v.push_back(f);
  if (v.empty()) {  // pas de héros : les premiers membres valides
    for (auto& f : team)
      if (f && f->alive() && (int)v.size() < heroes) v.push_back(f);
    return v;
  }
  std::vector<FighterP> used;
  for (auto& h : v) {
    all.push_back(h);
    FighterP c = h->companion.lock();
    if (!inTeam(team, c) || !c->alive()) {  // compagnon par défaut : la première créature libre
      c = nullptr;
      for (auto& f : team)
        if (f && hasSpecies(f->sp) && !hero(f) && f->alive() && !chosen(team, f) && std::find(used.begin(), used.end(), f) == used.end()) {
          c = f;
          break;
        }
    }
    if (c) all.push_back(c), used.push_back(c);
  }
  return all;
}

std::vector<int> formation(const std::vector<FighterP>& v, bool keep) {
  std::vector<int> cells(v.size(), -1);
  bool used[N_CELLS] = {};
  if (keep)
    for (size_t i = 0; i < v.size(); i++) {
      int c = v[i] ? v[i]->cell : -1;
      if (c >= 0 && c < N_CELLS && !used[c]) cells[i] = c, used[c] = true;
    }
  static const int ROWS[GRID] = {1, 2, 0, 3};  // le centre d'abord
  for (size_t i = 0; i < v.size(); i++) {
    if (cells[i] >= 0 || !v[i]) continue;
    const Species& s = v[i]->S();
    bool caster = s.base[B_MAG] > s.base[B_ATK];
    const int front[GRID] = {0, 1, 2, 3}, back[GRID] = {3, 2, 1, 0};
    for (int col : caster ? back : front) {
      for (int r : ROWS)
        if (!used[cellAt(r, col)]) {
          cells[i] = cellAt(r, col), used[cellAt(r, col)] = true;
          break;
        }
      if (cells[i] >= 0) break;
    }
  }
  return cells;
}

FighterP companionOf(const std::vector<FighterP>& team, const FighterP& who) {
  if (!hero(who)) return nullptr;
  // Celui qu'il aurait en combat si toute l'équipe était valide : on rejoue le choix sans les K.O.
  std::vector<FighterP> used;
  for (auto& h : team) {
    if (!hero(h)) continue;
    FighterP c = h->companion.lock();
    if (!inTeam(team, c)) {
      c = nullptr;
      for (auto& f : team)
        if (f && hasSpecies(f->sp) && !hero(f) && !chosen(team, f) && std::find(used.begin(), used.end(), f) == used.end()) {
          c = f;
          break;
        }
    }
    if (c) used.push_back(c);
    if (h == who) return c;
  }
  return nullptr;
}

float guardianScale(int allies, int players) {
  return std::max(1.f, allies / 3.f) * float(1 + RULES.coopGuardian * std::max(0, players - 1));
}

Json gearJson(const Fighter& f) {
  Json g = Json::array();
  for (auto& id : f.gear) g.push_back(id);
  return g;
}
void applyGear(Fighter& f, const Json& gear) {
  if (!gear.is_array()) return;
  for (size_t i = 0; i < gear.size() && i < f.gear.size(); i++) {
    std::string id = gear[i].is_string() ? gear[i].get<std::string>() : "";
    f.gear[i] = hasItem(id) && item(id).slot == (int)i && canEquip(f, item(id)) ? id : "";
  }
  f.recalc();
}
void Fighter::clearBattle() {
  status = Status::None;
  statusTurns = 0;
  stage.fill(0);
  broken = false;
  shield = shieldMax;
}
float Fighter::stageMult(int st) const {
  int n = stage[st];
  float k = float(RULES.stageStep);
  return n >= 0 ? 1 + k * n : 1 / (1 - k * n);
}
float Fighter::eAtk() const { return atk * stageMult(S_ATK) * (status == Status::Burn ? float(RULES.burnAtk) : 1.f); }
float Fighter::eDef() const { return def * stageMult(S_DEF); }
float Fighter::eMag() const { return mag * stageMult(S_MAG); }
float Fighter::eRes() const { return res * stageMult(S_RES); }
float Fighter::eSpd() const { return spd * stageMult(S_SPD) * (status == Status::Paralysis ? float(RULES.paraSpeed) : 1.f); }
int Fighter::need() const { return RULES.xpBase + int(lvl * lvl * RULES.xpSquare + 1e-6); }

std::vector<std::string> Fighter::techs() const {
  std::vector<std::string> v;
  for (auto& l : S().learn)
    if (l.lvl <= lvl && moveInfo(l.move).cost == 0) v.push_back(l.move);
  if (v.size() > 4) v.erase(v.begin(), v.end() - 4);
  for (auto& id : bonusMoves)
    if (hasMove(id) && moveInfo(id).cost == 0 && std::find(v.begin(), v.end(), id) == v.end()) v.push_back(id);
  for (auto& slot : gems)  // gemmes serties
    for (auto& g : slot)
      if (hasItem(g) && hasMove(item(g).gemMove) && moveInfo(item(g).gemMove).cost == 0 && std::find(v.begin(), v.end(), item(g).gemMove) == v.end())
        v.push_back(item(g).gemMove);
  return v;
}
std::vector<std::string> Fighter::spells() const {
  std::vector<std::string> v;
  for (auto& l : S().learn)
    if (l.lvl <= lvl && moveInfo(l.move).cost > 0) v.push_back(l.move);
  for (auto& id : bonusMoves)
    if (hasMove(id) && moveInfo(id).cost > 0 && std::find(v.begin(), v.end(), id) == v.end()) v.push_back(id);
  for (auto& slot : gems)  // gemmes serties
    for (auto& g : slot)
      if (hasItem(g) && hasMove(item(g).gemMove) && moveInfo(item(g).gemMove).cost > 0 && std::find(v.begin(), v.end(), item(g).gemMove) == v.end())
        v.push_back(item(g).gemMove);
  return v;
}

int foeLevelBonus = 0;
FighterP makeFoe(const Json& e, int defaultLevel) {
  auto f = makeFighter(e.at("espece").get<std::string>(), jget(e, "niveau", defaultLevel) + foeLevelBonus);
  float mult = jget(e, "pv", 1.f);
  if (mult != 1.f) f->mhp = std::max(1, int(f->mhp * mult + 1e-4f));
  f->hp = f->mhp;
  f->boss = jget(e, "boss", false);
  f->shieldMax = jget(e, "bouclier", 0);
  if (e.contains("phases") && e["phases"].is_array()) f->phases = e["phases"];
  return f;
}

static bool hasTypeIn(const Species& s, const std::vector<std::string>& ids) {
  for (auto& ty : ids)
    for (size_t k = 0; k < TYPES.size(); k++)
      if (TYPES[k].id == ty && hasType(s, (int)k)) return true;
  return false;
}
int fighterMoveRange(const Fighter& f) {
  int r = RULES.moveRange + (hasTypeIn(f.S(), RULES.fastTypes) ? 1 : 0);
  for (auto& id : f.gear)  // bottes : une case de plus
    if (!id.empty() && hasItem(id)) r += item(id).moveBonus;
  return r;
}
bool speciesFlies(const Species& s) { return hasTypeIn(s, RULES.flyTypes); }

FighterP makeFighter(const std::string& id, int lvl) {
  auto f = std::make_shared<Fighter>();
  f->sp = id;
  f->lvl = std::max(1, lvl);
  f->recalc();
  f->hp = f->mhp;
  f->mp = f->mmp;
  f->tactics = f->S().tactics.empty() ? RULES.tactics : f->S().tactics;
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
void seedRandom(unsigned s) { rng().seed(s); }
int irand(int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng()); }
