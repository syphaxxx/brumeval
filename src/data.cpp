#include "data.hpp"

#include <algorithm>
#include <random>
#include <stdexcept>
#include <unordered_map>

using T = Type;
using G = Target;
using K = Kind;

const char* typeName(Type t) {
  switch (t) {
    case T::Normal: return "Normal";
    case T::Feu: return "Feu";
    case T::Eau: return "Eau";
    case T::Plante: return "Plante";
    case T::Foudre: return "Foudre";
    case T::Ombre: return "Ombre";
    case T::Lumiere: return "Lumière";
  }
  return "?";
}

float typeEff(Type a, Type d) {
  if (a == T::Feu) return d == T::Plante ? 2.f : (d == T::Eau || d == T::Feu) ? .5f : 1.f;
  if (a == T::Eau) return d == T::Feu ? 2.f : (d == T::Plante || d == T::Eau) ? .5f : 1.f;
  if (a == T::Plante) return d == T::Eau ? 2.f : (d == T::Feu || d == T::Plante) ? .5f : 1.f;
  if (a == T::Foudre) return d == T::Eau ? 2.f : (d == T::Plante || d == T::Foudre) ? .5f : 1.f;
  if (a == T::Lumiere) return d == T::Ombre ? 2.f : 1.f;
  if (a == T::Ombre) return d == T::Lumiere ? 2.f : 1.f;
  return 1.f;
}

// ---------------------------------------------------------------------------
// Techniques et sorts
// ---------------------------------------------------------------------------
static const std::vector<Move> MOVES = {
    // Techniques des héros
    {"lame", "Lame d'acier", T::Normal, 40, G::Enemy, K::Physical, 0, "Un coup d'épée net."},
    {"estoc", "Estoc", T::Normal, 55, G::Enemy, K::Physical, 0, "Une frappe perçante."},
    {"eclair", "Taillade éclair", T::Normal, 72, G::Enemy, K::Physical, 0, "Un enchaînement fulgurant."},
    {"hache", "Coup de hache", T::Normal, 48, G::Enemy, K::Physical, 0, "Lourd et brutal."},
    {"fracas", "Fracas", T::Normal, 68, G::Enemy, K::Physical, 0, "Une frappe qui ébranle le sol."},
    {"baton", "Coup de bâton", T::Normal, 28, G::Enemy, K::Physical, 0, "Mieux vaut lancer un sort."},
    {"dague", "Dague", T::Normal, 32, G::Enemy, K::Physical, 0, "Rapide mais légère."},
    // Techniques des créatures
    {"charge", "Charge", T::Normal, 35, G::Enemy, K::Physical, 0, ""},
    {"flam", "Flammèche", T::Feu, 40, G::Enemy, K::Physical, 0, ""},
    {"brasier", "Brasier", T::Feu, 65, G::Enemy, K::Physical, 0, ""},
    {"jet", "Jet d'eau", T::Eau, 40, G::Enemy, K::Physical, 0, ""},
    {"vague", "Vague", T::Eau, 65, G::Enemy, K::Physical, 0, ""},
    {"ronce", "Fouet-ronce", T::Plante, 40, G::Enemy, K::Physical, 0, ""},
    {"tempete", "Tempête de feuilles", T::Plante, 65, G::Enemy, K::Physical, 0, ""},
    {"bec", "Coup de bec", T::Normal, 40, G::Enemy, K::Physical, 0, ""},
    {"morsure", "Morsure", T::Normal, 45, G::Enemy, K::Physical, 0, ""},
    {"etincelle", "Étincelle", T::Feu, 40, G::Enemy, K::Physical, 0, ""},
    {"bulle", "Bulle", T::Eau, 40, G::Enemy, K::Physical, 0, ""},
    {"spore", "Spore", T::Plante, 40, G::Enemy, K::Physical, 0, ""},
    {"eboul", "Éboulis", T::Normal, 55, G::Enemy, K::Physical, 0, ""},
    {"griffe", "Griffe d'ombre", T::Ombre, 50, G::Enemy, K::Physical, 0, ""},
    {"crocs", "Crocs ardents", T::Feu, 55, G::Enemy, K::Physical, 0, ""},
    {"decharge", "Décharge", T::Foudre, 50, G::Enemy, K::Physical, 0, ""},
    {"pique", "Piqué tonnerre", T::Foudre, 65, G::Enemy, K::Physical, 0, ""},
    {"gel", "Morsure de gel", T::Eau, 55, G::Enemy, K::Physical, 0, ""},
    {"suie", "Nuage de suie", T::Ombre, 40, G::AllEnemies, K::Magic, 0, ""},
    {"magma", "Torrent de magma", T::Feu, 50, G::AllEnemies, K::Magic, 0, ""},
    {"poing", "Poing de lave", T::Feu, 72, G::Enemy, K::Physical, 0, ""},
    {"brume", "Souffle de brume", T::Ombre, 40, G::AllEnemies, K::Magic, 0, ""},
    {"voile", "Voile noir", T::Ombre, 45, G::Enemy, K::Magic, 0, ""},
    // Sorts (coûtent des PM)
    {"soin", "Soin", T::Lumiere, 30, G::Ally, K::Heal, 4, "Rend des PV à un allié."},
    {"soins", "Soin de groupe", T::Lumiere, 20, G::AllAllies, K::Heal, 10, "Rend des PV à toute l'équipe."},
    {"reveil", "Réanimation", T::Lumiere, 40, G::AllyKO, K::Revive, 16, "Relève un allié K.O."},
    {"lumiere", "Lumière", T::Lumiere, 42, G::Enemy, K::Magic, 4, "Très efficace contre l'Ombre."},
    {"rayon", "Rayon sacré", T::Lumiere, 55, G::AllEnemies, K::Magic, 14, "Lumière sur tous les ennemis."},
    {"feu", "Feu", T::Feu, 45, G::Enemy, K::Magic, 4, "Brûle un ennemi."},
    {"givre", "Givre", T::Eau, 45, G::Enemy, K::Magic, 4, "Gèle un ennemi."},
    {"foudre", "Foudre", T::Foudre, 45, G::Enemy, K::Magic, 4, "Foudroie un ennemi."},
    {"pyro", "Pyrosphère", T::Feu, 50, G::AllEnemies, K::Magic, 12, "Feu sur tous les ennemis."},
    {"blizzard", "Blizzard", T::Eau, 50, G::AllEnemies, K::Magic, 12, "Glace sur tous les ennemis."},
    {"orage", "Orage", T::Foudre, 50, G::AllEnemies, K::Magic, 12, "Foudre sur tous les ennemis."},
    {"seve", "Sève", T::Plante, 18, G::AllAllies, K::Heal, 9, "Une sève douce soigne l'équipe."},
    // Limites
    {"lim_lior", "Taille-brume", T::Normal, 62, G::AllEnemies, K::Physical, 0, ""},
    {"lim_maelle", "Grâce de l'aube", T::Lumiere, 45, G::AllAllies, K::Heal, 0, ""},
    {"lim_brann", "Tourbillon d'acier", T::Normal, 72, G::AllEnemies, K::Physical, 0, ""},
    {"lim_isra", "Comète", T::Normal, 75, G::AllEnemies, K::Magic, 0, ""},
    {"lim_braisenard", "Ouragan de braises", T::Feu, 58, G::AllEnemies, K::Physical, 0, ""},
    {"lim_gouttelin", "Déluge", T::Eau, 58, G::AllEnemies, K::Physical, 0, ""},
    {"lim_ronceau", "Ronces infinies", T::Plante, 58, G::AllEnemies, K::Physical, 0, ""},
    {"lim_piafouine", "Rafale de plumes", T::Normal, 55, G::AllEnemies, K::Physical, 0, ""},
    {"lim_mulotin", "Frénésie", T::Normal, 55, G::AllEnemies, K::Physical, 0, ""},
    {"lim_lucioline", "Nuée ardente", T::Feu, 55, G::AllEnemies, K::Physical, 0, ""},
    {"lim_grenouillon", "Raz-de-marée", T::Eau, 55, G::AllEnemies, K::Physical, 0, ""},
    {"lim_champichou", "Nuage de spores", T::Plante, 55, G::AllEnemies, K::Physical, 0, ""},
    {"lim_rocaillou", "Avalanche", T::Normal, 55, G::AllEnemies, K::Physical, 0, ""},
    {"lim_brumelin", "Ombre profonde", T::Ombre, 55, G::AllEnemies, K::Magic, 0, ""},
    {"lim_tisonnel", "Éruption", T::Feu, 60, G::AllEnemies, K::Physical, 0, ""},
    {"lim_voltigeon", "Tempête d'éclairs", T::Foudre, 60, G::AllEnemies, K::Physical, 0, ""},
    {"lim_galetor", "Séisme", T::Normal, 60, G::AllEnemies, K::Physical, 0, ""},
    {"lim_glaconnet", "Ère glaciaire", T::Eau, 60, G::AllEnemies, K::Magic, 0, ""},
    {"lim_fumerole", "Brouillard toxique", T::Ombre, 60, G::AllEnemies, K::Magic, 0, ""},
};

// ---------------------------------------------------------------------------
// Apparences des humains
// ---------------------------------------------------------------------------
static const std::vector<Look> LOOKS = {
    {0x5a3a22, 0xf2c9a0, 0xd2493f, 0x2b2f45, 3, 1},  // 0 Lior : bandeau bleu, épée
    {0xe8d27a, 0xf6d6b8, 0xf2f0ea, 0x9fb3d9, 1, 2},  // 1 Maëlle : capuche blanche, bâton
    {0x8a3b1e, 0xd9a47a, 0x6b4a2f, 0x3d3a36, 0, 3},  // 2 Brann : hache
    {0x2b2238, 0xeec4a8, 0x5b3f8c, 0x2a2140, 2, 4},  // 3 Isra : chapeau pointu, dague
    {0x6b4a2f, 0xf2c9a0, 0x4e8c5a, 0x5a4a3a, 0, 0},  // 4 villageois
    {0xc9c4bc, 0xe8c0a0, 0x7a5c9a, 0x4a4060, 0, 0},  // 5 ancien
    {0xd98a3a, 0xf2c9a0, 0xe0b34a, 0x3a5a8a, 0, 0},  // 6 enfant
    {0x3a2a1e, 0xc99a74, 0x8a8f9a, 0x3a3a44, 3, 0},  // 7 garde / mineur
    {0x4a2e1e, 0xf2d0b0, 0xc85a7a, 0x5a3a4a, 0, 0},  // 8 villageoise
    {0x9a9a9a, 0xe0b898, 0x3f6f8f, 0x2f3f4f, 1, 0},  // 9 pêcheur / érudit
};

// ---------------------------------------------------------------------------
// Espèces : héros et créatures
// base = PV, PM, Attaque, Défense, Magie, Vitesse
// ---------------------------------------------------------------------------
static std::vector<Species> makeSpecies() {
  std::vector<Species> v;
  auto H = [&](std::string id, std::string n, Type t, std::array<int, 6> b, std::vector<Learn> l, std::string lim, int lk,
               std::string role) {
    Species s{id, n, t, b, Shape::Human, 0, 0, l, lim};
    s.human = true;
    s.look = lk;
    s.role = role;
    v.push_back(s);
  };
  auto C = [&](std::string id, std::string n, Type t, std::array<int, 6> b, Shape sh, uint32_t c1, uint32_t c2,
               std::vector<Learn> l) {
    v.push_back(Species{id, n, t, b, sh, c1, c2, l, "lim_" + id});
  };
  H("lior", "Lior", T::Normal, {55, 20, 55, 48, 30, 50}, {{1, "lame"}, {6, "estoc"}, {8, "soin"}, {14, "eclair"}}, "lim_lior", 0,
    "Apprenti gardien, épéiste");
  H("maelle", "Maëlle", T::Lumiere, {44, 60, 28, 42, 58, 48},
    {{1, "baton"}, {1, "soin"}, {1, "lumiere"}, {9, "soins"}, {12, "reveil"}, {17, "rayon"}}, "lim_maelle", 1,
    "Mage blanche du village");
  H("brann", "Brann", T::Normal, {70, 12, 66, 60, 20, 36}, {{1, "hache"}, {15, "fracas"}}, "lim_brann", 2,
    "Forgeron et guerrier à la hache");
  H("isra", "Isra", T::Ombre, {40, 70, 26, 36, 66, 54},
    {{1, "dague"}, {1, "feu"}, {1, "givre"}, {1, "foudre"}, {17, "pyro"}, {18, "blizzard"}, {19, "orage"}}, "lim_isra", 3,
    "Mage noire, exploratrice des grottes");

  C("braisenard", "Braisenard", T::Feu, {45, 22, 56, 42, 40, 60}, Shape::Fox, 0xe8743b, 0xffd27a,
    {{1, "charge"}, {1, "flam"}, {9, "brasier"}, {12, "pyro"}});
  C("gouttelin", "Gouttelin", T::Eau, {50, 22, 50, 54, 42, 46}, Shape::Drop, 0x3f8fd9, 0xbfe6ff,
    {{1, "charge"}, {1, "jet"}, {9, "vague"}, {12, "blizzard"}});
  C("ronceau", "Ronceau", T::Plante, {52, 24, 50, 52, 44, 44}, Shape::Bud, 0x4ea35a, 0xd6f0a0,
    {{1, "charge"}, {1, "ronce"}, {9, "tempete"}, {12, "seve"}});
  C("piafouine", "Piafouine", T::Normal, {40, 10, 45, 38, 20, 58}, Shape::Bird, 0xb98a5e, 0xf3e2c4, {{1, "bec"}, {6, "charge"}});
  C("mulotin", "Mulotin", T::Normal, {38, 10, 50, 36, 20, 52}, Shape::Mouse, 0x9a92a8, 0xefe6f2, {{1, "charge"}, {5, "morsure"}});
  C("lucioline", "Lucioline", T::Feu, {40, 16, 48, 40, 40, 55}, Shape::Bug, 0xd9a23b, 0xfff1a8,
    {{1, "charge"}, {4, "etincelle"}, {10, "feu"}});
  C("grenouillon", "Grenouillon", T::Eau, {48, 16, 46, 48, 36, 42}, Shape::Frog, 0x3fae9f, 0xc9f2e6,
    {{1, "charge"}, {4, "bulle"}, {10, "givre"}});
  C("champichou", "Champichou", T::Plante, {50, 18, 44, 50, 40, 36}, Shape::Mush, 0xc4524f, 0xf6e7d2,
    {{1, "charge"}, {4, "spore"}, {12, "seve"}});
  C("rocaillou", "Rocaillou", T::Normal, {50, 8, 52, 66, 20, 28}, Shape::Rock, 0x8b8378, 0xcfc6b8, {{1, "charge"}, {7, "eboul"}});
  C("brumelin", "Brumelin", T::Ombre, {40, 20, 44, 40, 44, 50}, Shape::Wisp, 0x7a6fb0, 0xe3dcff, {{1, "griffe"}, {8, "voile"}});
  C("tisonnel", "Tisonnel", T::Feu, {52, 20, 58, 46, 40, 56}, Shape::Lizard, 0xd9542b, 0xffc15a, {{1, "flam"}, {1, "crocs"}});
  C("voltigeon", "Voltigeon", T::Foudre, {46, 20, 52, 40, 48, 64}, Shape::Bird, 0xe8c33a, 0xfff6c0,
    {{1, "bec"}, {1, "decharge"}, {14, "pique"}});
  C("galetor", "Galetor", T::Normal, {62, 8, 60, 72, 22, 26}, Shape::Rock, 0x6e6560, 0xa99f96,
    {{1, "charge"}, {1, "eboul"}, {16, "fracas"}});
  C("glaconnet", "Glaçonnet", T::Eau, {50, 24, 44, 52, 52, 44}, Shape::Drop, 0x9fd3ee, 0xffffff,
    {{1, "bulle"}, {1, "gel"}, {12, "givre"}});
  C("fumerole", "Fumerole", T::Ombre, {44, 26, 40, 44, 56, 50}, Shape::Wisp, 0x8a8592, 0xd8d4de,
    {{1, "griffe"}, {1, "voile"}, {14, "suie"}});
  // Boss
  C("sylvarque", "Sylvarque", T::Ombre, {80, 40, 48, 46, 46, 45}, Shape::Boss, 0x5b4a8c, 0xc9b8ff,
    {{1, "griffe"}, {1, "brume"}, {1, "eboul"}});
  C("golem", "Golem de suie", T::Ombre, {90, 10, 60, 66, 30, 24}, Shape::Golem, 0x4a4440, 0xff9a3c,
    {{1, "eboul"}, {1, "suie"}, {1, "fracas"}});
  C("ignarok", "Ignarok", T::Feu, {95, 40, 60, 56, 58, 50}, Shape::Boss, 0xa8321e, 0xffb347,
    {{1, "poing"}, {1, "magma"}, {1, "crocs"}});
  return v;
}
static const std::vector<Species> SPECIES = makeSpecies();

static const std::vector<ItemDef> ITEMS = {
    {"potion", "Potion", "Rend 40 PV à un allié.", 30, true, true, false},
    {"superpotion", "Super-potion", "Rend 120 PV à un allié.", 90, true, true, false},
    {"ether", "Éther", "Rend 25 PM à un allié.", 70, true, true, false},
    {"plume", "Plume ravivante", "Relève un allié K.O. avec la moitié de ses PV.", 120, true, true, false},
    {"lanterne", "Lanterne", "Capture une créature. Plus elle est affaiblie, mieux c'est.", 40, true, false, false},
    {"lanterne_argent", "Lanterne d'argent", "Capture beaucoup plus facile.", 110, true, false, false},
    {"herbe", "Herbe lunaire", "Une herbe qui luit doucement. Maëlle en a besoin.", 0, false, false, true},
};

// ---------------------------------------------------------------------------
template <class V>
static const typename V::value_type& findId(const V& v, const std::string& id, const char* what) {
  for (auto& x : v)
    if (x.id == id) return x;
  throw std::runtime_error(std::string(what) + " inconnu : " + id);
}
const Move& moveInfo(const std::string& id) { return findId(MOVES, id, "Technique"); }
const Species& species(const std::string& id) { return findId(SPECIES, id, "Espèce"); }
const ItemDef& item(const std::string& id) { return findId(ITEMS, id, "Objet"); }
const std::vector<ItemDef>& allItems() { return ITEMS; }
const Look& look(int i) { return LOOKS[(size_t)i % LOOKS.size()]; }

// ---------------------------------------------------------------------------
std::string Fighter::name() const { return tag.empty() ? S().name : S().name + " " + tag; }

static int stat(int b, int l) { return b * l / 25 + 5; }
void Fighter::recalc() {
  const auto& b = S().base;
  mhp = b[0] * lvl / 20 + lvl + 10;
  mmp = b[1] * lvl / 30 + 5;
  atk = stat(b[2], lvl);
  def = stat(b[3], lvl);
  mag = stat(b[4], lvl);
  spd = stat(b[5], lvl);
}
int Fighter::need() const { return 10 + lvl * lvl * 6 / 5; }

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
