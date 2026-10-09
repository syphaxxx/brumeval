#include "procgen.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>
#include <functional>

#include "data.hpp"
#include "world.hpp"

namespace procgen {

// ===========================================================================
// Hasard
// ===========================================================================
static uint64_t splitmix(uint64_t& x) {
  uint64_t z = (x += 0x9E3779B97F4A7C15ull);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}
Rng::Rng(uint64_t seed) {
  for (auto& s : s_) s = splitmix(seed);
}
static uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }
uint64_t Rng::next() {
  uint64_t r = rotl(s_[1] * 5, 7) * 9, t = s_[1] << 17;
  s_[2] ^= s_[0];
  s_[3] ^= s_[1];
  s_[1] ^= s_[2];
  s_[0] ^= s_[3];
  s_[2] ^= t;
  s_[3] = rotl(s_[3], 45);
  return r;
}
int Rng::range(int lo, int hi) { return hi <= lo ? lo : lo + int(next() % uint64_t(hi - lo + 1)); }
float Rng::real() { return float(next() >> 40) / 16777216.f; }

// Flux de hasard indépendants : chaque partie du monde ne dépend que de la graine
// et de son propre numéro (les techniques, les héros, la région 3…)
static uint64_t stream(uint64_t seed, uint64_t salt) {
  uint64_t x = seed ^ (salt * 0xD1B54A32D192ED03ull);
  return splitmix(x);
}
uint64_t seedFromText(const std::string& s) {
  bool digits = !s.empty() && s.size() <= 18 && std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
  if (digits) return std::stoull(s);
  uint64_t h = 1469598103934665603ull;
  for (unsigned char c : s) h = (h ^ c) * 1099511628211ull;
  return h;
}
uint64_t randomSeed() {
  uint64_t x = (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count() ^
               (uint64_t)std::chrono::system_clock::now().time_since_epoch().count();
  return splitmix(x) % 1000000000ull;  // 9 chiffres au plus : facile à noter et à partager
}
template <class T>
static void shuffle(Rng& r, std::vector<T>& v) {
  for (int i = (int)v.size() - 1; i > 0; i--) std::swap(v[i], v[r.range(0, i)]);
}

// ===========================================================================
// Textes
// ===========================================================================
static int u8len(const std::string& s) {
  int n = 0;
  for (unsigned char c : s) n += (c & 0xC0) != 0x80;
  return n;
}
static bool vowel(unsigned char c) { return std::strchr("aeiouyAEIOUY", c) != nullptr || c >= 0x80; }
static bool vowelStart(const std::string& w) { return !w.empty() && vowel((unsigned char)w[0]); }
// « de braise », « d'écume »
static std::string de(const std::string& w) { return (vowelStart(w) ? "d'" : "de ") + w; }
static std::string cap(std::string s) {
  if (!s.empty() && s[0] >= 'a' && s[0] <= 'z') s[0] = char(s[0] - 32);
  else if (s.size() > 1 && (unsigned char)s[0] == 0xC3 && (unsigned char)s[1] >= 0xA0 && (unsigned char)s[1] <= 0xBE)
    s[1] = char((unsigned char)s[1] - 0x20);  // é → É
  return s;
}
// Colle deux morceaux de nom sans trois voyelles de suite : « Aqua » + « on » → « Aquon »
static std::string glue(std::string a, const std::string& b) {
  if (!a.empty() && !b.empty() && vowel((unsigned char)a.back()) && (unsigned char)a.back() < 0x80 && vowel((unsigned char)b[0])) a.pop_back();
  return a + b;
}

// ===========================================================================
// Vocabulaire
// ===========================================================================
struct Noun {
  const char* w;
  bool fem;
};
struct Adj {
  const char* m;
  const char* f;
};
// Saveur de chaque type : compléments et adjectifs des techniques, racines des
// noms de créatures, formes de créatures
struct Flavor {
  const char* type;
  std::vector<const char*> compl_;
  std::vector<Adj> adj;
  std::vector<const char*> roots;
  std::vector<const char*> shapes;
};
static const std::vector<Flavor>& flavors() {
  static const std::vector<Flavor> F = {
      {"normal", {"rage", "force", "furie"}, {{"brutal", "brutale"}, {"sauvage", "sauvage"}, {"vif", "vive"}, {"puissant", "puissante"}, {"furieux", "furieuse"}},
       {"mul", "piaf", "louv", "terr", "bouv", "lap", "renar", "pel", "trott"}, {"souris", "oiseau", "loup", "renard"}},
      {"feu", {"braise", "flammes", "cendres", "lave", "feu", "brasier"}, {{"ardent", "ardente"}, {"brûlant", "brûlante"}, {"embrasé", "embrasée"}},
       {"brais", "flamm", "tison", "cendr", "pyr", "fumer", "ign", "calc"}, {"renard", "feu_follet", "lezard", "loup"}},
      {"eau", {"eau", "écume", "marée", "pluie", "embruns"}, {{"marin", "marine"}, {"limpide", "limpide"}, {"salé", "salée"}},
       {"goutt", "flot", "ond", "bull", "aqu", "marin", "nag", "rinç"}, {"goutte", "grenouille", "tortue", "serpent"}},
      {"plante", {"ronces", "sève", "feuilles", "épines", "lierre", "racines"}, {{"sylvestre", "sylvestre"}, {"épineux", "épineuse"}, {"verdoyant", "verdoyante"}},
       {"ronc", "feuill", "sev", "mouss", "lierr", "bourg", "flor", "herb"}, {"bourgeon", "champignon", "insecte", "tortue"}},
      {"foudre", {"foudre", "tonnerre", "éclairs", "orage", "étincelles"}, {{"électrique", "électrique"}, {"foudroyant", "foudroyante"}, {"fulgurant", "fulgurante"}},
       {"volt", "étinc", "fulg", "zap", "orag", "tonn", "elect"}, {"oiseau", "souris", "insecte", "feu_follet"}},
      {"ombre", {"ombre", "ténèbres", "nuit", "brume", "suie"}, {{"sombre", "sombre"}, {"ténébreux", "ténébreuse"}, {"obscur", "obscure"}, {"nocturne", "nocturne"}},
       {"brum", "noct", "ombr", "suit", "sombr", "nyct", "vesp"}, {"chauve_souris", "fantome", "loup", "renard"}},
      {"lumiere", {"lumière", "aube", "étoiles", "soleil", "aurore"}, {{"sacré", "sacrée"}, {"radieux", "radieuse"}, {"céleste", "céleste"}, {"solaire", "solaire"}},
       {"lum", "aur", "clar", "sol", "lux", "cristall", "ray"}, {"feu_follet", "cristal", "oiseau"}},
      {"glace", {"givre", "glace", "neige", "frimas", "grêle"}, {{"glacial", "glaciale"}, {"gelé", "gelée"}, {"polaire", "polaire"}, {"givré", "givrée"}},
       {"givr", "glac", "frim", "neig", "gel", "bris", "flocon"}, {"cristal", "renard", "oiseau", "tortue"}},
      {"roche", {"roc", "pierre", "granit", "sable", "gravats"}, {{"rocheux", "rocheuse"}, {"tellurique", "tellurique"}, {"massif", "massive"}},
       {"roc", "pierr", "gal", "caill", "grav", "basalt", "silex"}, {"rocher", "tortue", "lezard"}},
      {"vent", {"vent", "rafales", "plumes", "zéphyr", "nuages"}, {{"aérien", "aérienne"}, {"cinglant", "cinglante"}, {"léger", "légère"}},
       {"zéph", "briz", "souffl", "plum", "vol", "aér", "bours"}, {"oiseau", "chauve_souris", "insecte"}},
      {"poison", {"venin", "poison", "miasmes", "acide", "toxines"}, {{"toxique", "toxique"}, {"venimeux", "venimeuse"}, {"corrosif", "corrosive"}},
       {"venin", "tox", "crap", "serp", "fiel", "miasm", "vip"}, {"serpent", "grenouille", "champignon", "insecte"}},
      {"metal", {"acier", "fer", "bronze", "mercure", "fonte"}, {{"métallique", "métallique"}, {"acéré", "acérée"}, {"blindé", "blindée"}, {"trempé", "trempée"}},
       {"ferr", "aciér", "font", "cuivr", "bronz", "mithr", "lam"}, {"rocher", "cristal", "insecte", "lezard"}},
      {"esprit", {"esprit", "songes", "âmes", "rêves", "spectres"}, {{"psychique", "psychique"}, {"spectral", "spectrale"}, {"onirique", "onirique"}, {"mystique", "mystique"}},
       {"spectr", "song", "esprill", "rêv", "âm", "mirag", "fantasm"}, {"fantome", "feu_follet", "chauve_souris"}},
  };
  return F;
}
static const Flavor& flavor(const std::string& type) {
  for (auto& f : flavors())
    if (type == f.type) return f;
  return flavors()[0];
}

static const std::vector<Noun> PHYS = {{"Coup", false},     {"Croc", false},    {"Poing", false},  {"Dard", false},    {"Bond", false},
                                       {"Assaut", false},   {"Choc", false},    {"Fouet", false},  {"Revers", false},  {"Griffe", true},
                                       {"Morsure", true},   {"Charge", true},   {"Frappe", true},  {"Lame", true},     {"Ruade", true},
                                       {"Taillade", true},  {"Corne", true},    {"Serre", true},   {"Queue", true},    {"Estocade", true}};
static const std::vector<Noun> MAGIC = {{"Souffle", false}, {"Rayon", false},  {"Jet", false},   {"Éclat", false},  {"Orbe", false},
                                        {"Trait", false},   {"Sceau", false},  {"Chant", false}, {"Vague", true},   {"Onde", true},
                                        {"Sphère", true},   {"Lueur", true},   {"Gerbe", true},  {"Flèche", true},  {"Spirale", true}};
static const std::vector<Noun> AREA = {{"Tempête", true},  {"Nuée", true},    {"Pluie", true},  {"Avalanche", true}, {"Explosion", true},
                                       {"Rafale", true},   {"Déferlante", true}, {"Tourbillon", false}, {"Déluge", false}, {"Cyclone", false},
                                       {"Torrent", false}, {"Nuage", false}};
static const std::vector<Noun> HEAL = {{"Baume", false}, {"Souffle", false}, {"Onguent", false}, {"Rosée", true}, {"Lueur", true}, {"Prière", true}, {"Caresse", true}};
static const std::vector<Noun> HEAL_ALL = {{"Chant", false}, {"Hymne", false}, {"Pluie", true}, {"Ronde", true}, {"Brume", true}};
static const std::vector<Noun> GUARD = {{"Rempart", false}, {"Bouclier", false}, {"Égide", true}, {"Muraille", true}};
static const std::vector<Noun> CRY = {{"Cri", false}, {"Hurlement", false}, {"Clameur", true}, {"Fureur", true}};
static const std::vector<Noun> FOCUS = {{"Œil", false}, {"Regard", false}, {"Visée", true}};
static const std::vector<Noun> SHELL = {{"Carapace", true}, {"Cuirasse", true}, {"Armure", true}, {"Blindage", false}};
static const std::vector<Noun> HASTE = {{"Pas", false}, {"Élan", false}, {"Hâte", true}, {"Envolée", true}};
static const std::vector<Noun> SLEEP = {{"Berceuse", true}, {"Voile", false}, {"Torpeur", true}};
static const std::vector<Noun> HYPNO = {{"Hypnose", true}, {"Transe", true}, {"Mirage", false}};
static const std::vector<Noun> TOXIN = {{"Piqûre", true}, {"Toxine", true}, {"Miasme", false}};
static const std::vector<Noun> STUN = {{"Décharge", true}, {"Onde", true}, {"Choc", false}};
static const std::vector<Noun> CURSE = {{"Maléfice", false}, {"Regard", false}, {"Sort", false}};
static const std::vector<Noun> BREAK = {{"Brèche", true}, {"Fendoir", false}, {"Coup", false}};

// Personnes
static const std::vector<const char*> H_START = {"Al", "Bel", "Cael", "Dar", "El", "Fen", "Gal", "Hel", "Is", "Jor", "Kal", "Li",
                                                 "Mae", "Nor", "Or", "Per", "Ril", "Sel", "Tal", "Ul", "Val", "Wen", "Ys", "Zar",
                                                 "Ar", "Bran", "Cor", "Ery", "Lys", "Mir", "Ner", "Aub", "Ten", "Vio"};
static const std::vector<const char*> H_END = {"an", "as", "el", "en", "ia", "ine", "is", "ix", "on", "or", "ra", "ric",
                                               "ys", "wen", "ane", "elle", "ard", "in", "o", "a", "é", "enne", "eth", "yn"};
// Lieux
static const std::vector<const char*> P_START = {"Val", "Brum", "Ker", "Mor", "Syl", "Gar", "Lun", "Fen", "Ombr", "Ast", "Cal", "Dor",
                                                 "Hel", "Ys", "Tor", "Vel", "Eri", "Arn", "Bel", "Cor", "Sau", "Lir", "Grav", "Haut"};
static const std::vector<const char*> P_END = {"ombre", "mir", "vel", "ac", "or", "enne", "ance", "is", "oth", "ard", "ys", "eval",
                                               "rac", "ine", "as", "oux", "ève", "ir", "ande", "elle", "mont", "bois", "val"};
// Créatures
static const std::vector<const char*> SUFFIX = {"in", "on", "et", "ou", "ard", "elle", "ine", "ot", "ille", "inet",
                                                "elin", "eron", "oche", "ix", "ar", "or", "aud", "uche", "eau", "ise"};
static const std::vector<const char*> BOSS_SUFFIX = {"arque", "arok", "orne", "orax", "ion", "oth", "ira", "aure", "ombe", "akar"};
static const std::vector<const char*> BOSS_SHAPES = {"boss", "golem", "lezard", "loup", "serpent", "tortue", "fantome", "cristal"};

// Couleurs
static const std::vector<uint32_t> HAIR = {0x2a1d14, 0x5a3a22, 0x8a3b1e, 0xe8d27a, 0xc8c8d0, 0x1f2a44, 0x6b2a4a, 0xb86b2a, 0x3a3a3a, 0xf0e6d0};
static const std::vector<uint32_t> SKIN = {0xf6d6b8, 0xf2c9a0, 0xd9a47a, 0xb07a52, 0x7a5236, 0xe8b890};
static const std::vector<uint32_t> DARK = {0x2b2f45, 0x3d3a36, 0x4a3426, 0x2a3a2a, 0x3a2a4a, 0x5a5a62, 0x1f2a44};
static const std::vector<uint32_t> ROOF = {0x7a8a4a, 0xa0503a, 0x4a6aa0, 0x8a6a3a, 0x6a4a7a, 0x3a7a6a, 0xb08a3a};
static const char* HATS[] = {"aucune", "capuche", "chapeau", "bandeau", "casque", "foulard"};
static const char* WEAPONS[] = {"aucune", "epee", "baton", "hache", "dague", "arc", "lance"};

static uint32_t typeColor(const std::string& t) { return types()[typeOf(t)].color; }
static uint32_t jitter(Rng& r, uint32_t c, int amount) {
  int ch[3] = {int(c >> 16 & 255), int(c >> 8 & 255), int(c & 255)};
  int d = r.range(-amount, amount);  // même décalage pour garder la teinte
  uint32_t o = 0;
  for (int i = 0; i < 3; i++) o = o << 8 | uint32_t(std::clamp(ch[i] + d + r.range(-amount / 3, amount / 3), 0, 255));
  return o;
}
static uint32_t lighten(uint32_t c, float k) {
  uint32_t o = 0;
  for (int s : {16, 8, 0}) o = o << 8 | uint32_t(std::min(255.f, (c >> s & 255) + (255 - (c >> s & 255)) * k));
  return o;
}

// ===========================================================================
// Noms uniques
// ===========================================================================
static std::string unique(Content& c, const std::function<std::string()>& make, int maxLen) {
  for (int i = 0; i < 60; i++) {
    std::string s = make();
    if (u8len(s) <= maxLen && !c.names.count(s)) {
      c.names.insert(s);
      return s;
    }
  }
  for (int n = 2;; n++) {  // dernier recours : un numéro
    std::string s = make();
    if (u8len(s) > maxLen - 3) continue;
    s += " " + std::to_string(n % 90 + 2);
    if (!c.names.count(s)) {
      c.names.insert(s);
      return s;
    }
  }
}
// « Griffe de braise » ou « Griffe ardente »
static std::string phrase(Rng& r, const std::vector<Noun>& nouns, const std::string& type) {
  const Flavor& f = flavor(type);
  const Noun& n = r.pick(nouns);
  if (!f.adj.empty() && r.chance(.45f)) {
    const Adj& a = r.pick(f.adj);
    return std::string(n.w) + " " + (n.fem ? a.f : a.m);
  }
  return std::string(n.w) + " " + de(r.pick(f.compl_));
}
static std::string personName(Content& c, Rng& r) {
  return unique(c, [&] {
    std::string a = r.pick(H_START), b = r.pick(H_END);
    if (vowel((unsigned char)a.back()) && vowel((unsigned char)b[0])) a += r.chance(.5f) ? "l" : "r";
    return a + b;
  }, 8);
}
static std::string placeName(Content* c, Rng& r) {
  // Un tirage après l'autre (voir genCreature)
  auto make = [&] {
    std::string end = r.pick(P_END);
    return cap(glue(r.pick(P_START), end));
  };
  if (!c) return make();
  return unique(*c, make, 10);
}

// ===========================================================================
// Techniques
// ===========================================================================
static Json moveJson(const std::string& id, const std::string& name, const std::string& type, const char* genre, const char* target, int power) {
  Json m = Json::object();
  m["id"] = id;
  m["nom"] = name;
  m["type"] = type;
  m["genre"] = genre;
  m["cible"] = target;
  m["puissance"] = power;
  return m;
}
static std::string addMove(Content& c, const Json& m) {
  c.moves.push_back(m);
  return m["id"].get<std::string>();
}

// Effet en plus d'une attaque, selon son type
static Json hitEffect(Rng& r, const std::string& t, int lo, int hi) {
  int ch = r.range(lo / 5, hi / 5) * 5;
  auto status = [&](const char* s) { return Json{{"statut", s}, {"chance", ch}}; };
  auto stage = [&](const char* s, int n, bool self) {
    Json e{{"stat", s}, {"niveaux", n}};
    if (self) e["sur"] = "lanceur";
    e["chance"] = ch;
    return e;
  };
  if (t == "feu") return status("brulure");
  if (t == "foudre") return status("paralysie");
  if (t == "poison") return status("poison");
  if (t == "plante") return r.chance(.5f) ? status("poison") : stage("vitesse", -1, false);
  if (t == "glace") return stage("vitesse", -1, false);
  if (t == "eau") return stage("attaque", -1, false);
  if (t == "ombre") return stage(r.chance(.5f) ? "magie" : "resistance", -1, false);
  if (t == "roche") return stage("defense", -1, false);
  if (t == "vent") return stage("vitesse", 1, true);
  if (t == "metal") return stage("defense", 1, true);
  if (t == "esprit") return stage("resistance", -1, false);
  if (t == "lumiere") return stage("magie", 1, true);
  return stage("attaque", 1, true);
}

// Toutes les techniques d'un type, de la plus faible à la plus forte
static void genTypeMoves(Content& c, Rng& r, const std::string& t) {
  Pool p;
  auto id = [&](const char* slot) { return "x_" + t + "_" + slot; };
  auto name = [&](const std::vector<Noun>& nouns) { return unique(c, [&] { return phrase(r, nouns, t); }, 18); };
  // La puissance est tirée avant le nom, sur sa propre ligne : un seul tirage par appel (voir genCreature)
  {  // rapide, souvent avec un effet
    int pw = r.range(36, 42);
    Json m = moveJson(id("p1"), name(PHYS), t, "physique", "ennemi", pw);
    m["rythme"] = "rapide";
    if (r.chance(.5f)) m["effet"] = hitEffect(r, t, 15, 25);
    p.p1 = addMove(c, m);
  }
  {
    int pw = r.range(50, 56);
    Json m = moveJson(id("p2"), name(PHYS), t, "physique", "ennemi", pw);
    float k = r.real();
    if (k < .3f) m["critique"] = r.range(2, 3) * 5;
    else if (k < .65f) m["effet"] = hitEffect(r, t, 15, 25);
    p.p2 = addMove(c, m);
  }
  {
    int pw = r.range(64, 70);
    Json m = moveJson(id("p3"), name(PHYS), t, "physique", "ennemi", pw);
    if (r.chance(.4f)) m["precision"] = 95;
    if (r.chance(.3f)) m["effet"] = hitEffect(r, t, 10, 20);
    p.p3 = addMove(c, m);
  }
  {  // lourde : gros coup, le lanceur rejoue plus tard
    int pw = r.range(82, 90);
    Json m = moveJson(id("p4"), name(PHYS), t, "physique", "ennemi", pw);
    m["precision"] = r.range(17, 19) * 5;
    if (r.chance(.3f)) m["critique"] = 10;
    m["rythme"] = "lourde";
    p.p4 = addMove(c, m);
  }
  {
    int pw = r.range(45, 52);
    Json m = moveJson(id("zone"), name(AREA), t, "physique", "tous_ennemis", pw);
    if (r.chance(.5f)) m["precision"] = 95;
    p.area = addMove(c, m);
  }
  {
    int pw = r.range(40, 45);
    Json m = moveJson(id("m1"), name(MAGIC), t, "magique", "ennemi", pw);
    if (r.chance(.3f)) m["rythme"] = "rapide";
    if (r.chance(.4f)) m["effet"] = hitEffect(r, t, 15, 25);
    p.m1 = addMove(c, m);
  }
  {
    int pw = r.range(58, 64);
    Json m = moveJson(id("m3"), name(MAGIC), t, "magique", "ennemi", pw);
    if (r.chance(.25f)) m["effet"] = hitEffect(r, t, 10, 20);
    p.m3 = addMove(c, m);
  }
  {
    int pw = r.range(44, 48);
    Json m = moveJson(id("s1"), name(MAGIC), t, "magique", "ennemi", pw);
    m["cout"] = 4;
    p.s1 = addMove(c, m);
  }
  {
    int pw = r.range(46, 52);
    Json m = moveJson(id("s2"), name(AREA), t, "magique", "tous_ennemis", pw);
    m["cout"] = r.range(10, 12);
    if (r.chance(.25f)) m["effet"] = hitEffect(r, t, 15, 25);
    p.s2 = addMove(c, m);
  }
  {
    int pw = r.range(66, 72);
    Json m = moveJson(id("s3"), name(MAGIC), t, "magique", "ennemi", pw);
    m["cout"] = r.range(8, 10);
    p.s3 = addMove(c, m);
  }
  {
    int pw = r.range(60, 65);
    Json m = moveJson(id("s4"), name(AREA), t, "magique", "tous_ennemis", pw);
    m["cout"] = r.range(16, 18);
    if (r.chance(.5f)) m["rythme"] = "lourde";
    p.s4 = addMove(c, m);
  }
  c.pools[t] = p;
}

// Soins, bonus, états
static void genSupport(Content& c, Rng& r) {
  Support& s = c.sup;
  auto name = [&](const std::vector<Noun>& nouns, const std::string& t) { return unique(c, [&] { return phrase(r, nouns, t); }, 18); };
  auto fixed = [&](std::vector<const char*> list, const std::vector<Noun>& backup, const std::string& t) {
    shuffle(r, list);
    for (auto* n : list)
      if (!c.names.count(n)) {
        c.names.insert(n);
        return std::string(n);
      }
    return name(backup, t);
  };
  auto add = [&](const std::string& id, const std::string& nm, const std::string& t, const char* genre, const char* target, int power, int cost,
                 int acc, Json effect) {
    Json m = moveJson("x_" + id, nm, t, genre, target, power);
    if (cost > 0) m["cout"] = cost;
    if (acc < 100) m["precision"] = acc;
    if (!effect.is_null()) m["effet"] = effect;
    return addMove(c, m);
  };
  s.heal = add("soin", name(HEAL, "lumiere"), "lumiere", "soin", "allie", 30, 4, 100, Json());
  s.healWater = add("soin_eau", name(HEAL, "eau"), "eau", "soin", "allie", 28, 5, 100, Json());
  s.healAll = add("soins", name(HEAL_ALL, "lumiere"), "lumiere", "soin", "tous_allies", 20, 10, 100, Json());
  s.healAllPlant = add("seve", name(HEAL_ALL, "plante"), "plante", "soin", "tous_allies", 18, 9, 100, Json());
  s.revive = add("reveil", fixed({"Renaissance", "Second souffle", "Relève", "Résurrection", "Éveil sacré"}, HEAL, "lumiere"), "lumiere", "rappel",
                 "allie_ko", 40, 16, 100, Json());
  s.cure = add("purification", fixed({"Purification", "Délivrance", "Remède sacré", "Bain de lumière"}, HEAL, "lumiere"), "lumiere", "soin",
               "allie", 0, 3, 100, Json{{"guerison", true}});
  s.guardAll = add("rempart", name(GUARD, "lumiere"), "lumiere", "statut", "tous_allies", 0, 6, 100, Json{{"stat", "defense"}, {"niveaux", 1}});
  s.warCry = add("cri", name(CRY, "normal"), "normal", "statut", "tous_allies", 0, 0, 100, Json{{"stat", "attaque"}, {"niveaux", 1}});
  s.focus = add("visee", name(FOCUS, "vent"), "vent", "statut", "tous_allies", 0, 0, 100,
                Json{{"stat", "attaque"}, {"niveaux", 2}, {"sur", "lanceur"}});
  s.shell = add("carapace", name(SHELL, "roche"), "roche", "statut", "tous_allies", 0, 0, 100,
                Json{{"stat", "defense"}, {"niveaux", 2}, {"sur", "lanceur"}});
  s.haste = add("hate", name(HASTE, "vent"), "lumiere", "statut", "allie", 0, 5, 100, Json{{"stat", "vitesse"}, {"niveaux", 2}});
  s.sleep = add("sommeil", name(SLEEP, "ombre"), "ombre", "statut", "ennemi", 0, 5, 75, Json{{"statut", "sommeil"}});
  s.hypno = add("hypnose", name(HYPNO, "esprit"), "esprit", "statut", "ennemi", 0, 0, 70, Json{{"statut", "sommeil"}});
  s.toxin = add("toxine", name(TOXIN, "poison"), "poison", "statut", "ennemi", 0, 4, 90, Json{{"statut", "poison"}});
  s.stun = add("paralysie", name(STUN, "foudre"), "foudre", "statut", "ennemi", 0, 5, 85, Json{{"statut", "paralysie"}});
  s.curse = add("malefice", name(CURSE, "esprit"), "esprit", "statut", "ennemi", 0, 0, 100, Json{{"stat", "resistance"}, {"niveaux", -2}});
  s.breaker = add("brise", name(BREAK, "metal"), "normal", "physique", "ennemi", 40, 0, 100, Json{{"stat", "defense"}, {"niveaux", -1}});
}

// Techniques de soutien qu'une créature de ce type peut apprendre (pas de soins :
// une créature qui se soigne fait traîner les combats ; seuls les personnages soignent)
static std::vector<std::string> typeSupport(const Content& c, const std::string& t) {
  const Support& s = c.sup;
  if (t == "plante") return {s.toxin};
  if (t == "foudre") return {s.stun};
  if (t == "ombre") return {s.sleep};
  if (t == "lumiere") return {s.guardAll};
  if (t == "roche") return {s.shell};
  if (t == "vent") return {s.focus};
  if (t == "poison") return {s.toxin};
  if (t == "metal") return {s.shell, s.guardAll};
  if (t == "esprit") return {s.hypno, s.curse};
  if (t == "normal") return {s.warCry};
  return {};
}

// Limite d'une espèce : grande attaque sur tous les ennemis (ou soin de toute l'équipe)
static std::string genLimit(Content& c, Rng& r, const std::string& speciesId, const std::string& t, const char* genre, int power) {
  bool heal = std::strcmp(genre, "soin") == 0;
  std::string nm = unique(c, [&] { return phrase(r, heal ? HEAL_ALL : AREA, t); }, 18);
  return addMove(c, moveJson("lim_" + speciesId, nm, t, genre, heal ? "tous_allies" : "tous_ennemis", power));
}

// ===========================================================================
// Espèces
// ===========================================================================
static const char* STAT_KEYS[] = {"pv", "pm", "attaque", "defense", "magie", "resistance", "vitesse"};
using Stats = std::array<int, 7>;
static Json baseJson(const Stats& v) {
  Json b = Json::object();
  for (int i = 0; i < 7; i++) b[STAT_KEYS[i]] = v[i];
  return b;
}
static Stats vary(Rng& r, Stats s, int amount) {
  for (auto& v : s) v = std::max(8, v + r.range(-amount, amount));
  return s;
}
using Learnset = std::vector<std::pair<int, std::string>>;
static Json learnJson(Learnset l) {
  std::stable_sort(l.begin(), l.end(), [](auto& a, auto& b) { return a.first < b.first; });
  Json o = Json::array();
  std::set<std::string> seen;
  for (auto& [lvl, id] : l)
    if (!id.empty() && seen.insert(id).second) o.push_back(Json::array({lvl, id}));
  return o;
}
static Json typesJson(const std::string& t1, const std::string& t2) {
  if (t2.empty() || t2 == t1) return Json(t1);
  return Json::array({t1, t2});
}

// Profils de créatures : PV, PM, Attaque, Défense, Magie, Résistance, Vitesse
struct Profile {
  const char* name;
  Stats s;
  bool magic;
};
static const std::vector<Profile> PROFILES = {
    {"rapide", {42, 18, 52, 38, 36, 38, 64}, false},  {"costaud", {62, 12, 56, 64, 26, 46, 30}, false},
    {"mage", {44, 34, 30, 40, 60, 54, 50}, true},     {"brute", {50, 14, 64, 44, 26, 36, 48}, false},
    {"equilibre", {50, 22, 50, 48, 46, 46, 48}, false}, {"ruse", {46, 28, 42, 44, 54, 50, 56}, true},
};

static Json creatureJson(const std::string& id, const std::string& name, const std::string& t1, const std::string& t2, const Stats& st) {
  Json o = Json::object();
  o["id"] = id;
  o["nom"] = name;
  if (t2.empty() || t2 == t1) o["type"] = t1;
  else o["types"] = typesJson(t1, t2);
  o["base"] = baseJson(st);
  return o;
}

// Créature sauvage (ou de départ)
static std::string genCreature(Content& c, Rng& r, const std::string& id, const std::string& t1, std::string t2) {
  if (t2 == t1) t2.clear();
  const Profile& pr = r.pick(PROFILES);
  std::string name = unique(c, [&] {
    const Flavor& f = flavor(!t2.empty() && r.chance(.3f) ? t2 : t1);
    // Jamais deux tirages dans les arguments d'un même appel : leur ordre dépend du compilateur
    // (Clang, sur Mac, commence par la gauche). Le suffixe d'abord, comme MSVC et GCC.
    std::string end = r.pick(SUFFIX);
    return cap(glue(r.pick(f.roots), end));
  }, 10);
  Json o = creatureJson(id, name, t1, t2, vary(r, pr.s, 5));
  if (std::strcmp(pr.name, "rapide") == 0) o["esquive"] = r.range(6, 10);
  if (r.chance(.4f)) o["critique"] = r.range(6, 10);
  o["forme"] = r.pick(flavor(t1).shapes);
  uint32_t c1 = jitter(r, typeColor(t1), 30), c2 = t2.empty() ? lighten(c1, .55f) : lighten(typeColor(t2), .2f);
  o["couleurs"] = Json::array({colorStr(c1), colorStr(c2)});
  // Techniques : une rapide dès le début, puis de plus en plus fortes
  const Pool& P = c.pools.at(t1);
  const Pool& Q = c.pools.at(t2.empty() ? "normal" : t2);
  auto sup = typeSupport(c, t1);
  if (!t2.empty())
    for (auto& s : typeSupport(c, t2)) sup.push_back(s);
  Learnset l;
  bool mg = pr.magic;
  l.push_back({1, mg && r.chance(.5f) ? P.m1 : P.p1});
  l.push_back({1, r.chance(.6f) ? Q.p1 : c.pools.at("normal").p1});
  l.push_back({r.range(5, 7), mg ? P.s1 : P.p2});
  l.push_back({r.range(9, 12), !sup.empty() && r.chance(.7f) ? r.pick(sup) : Q.p2});
  l.push_back({r.range(14, 17), mg ? P.m3 : P.p3});
  l.push_back({r.range(19, 22), mg ? P.s2 : (r.chance(.5f) ? P.area : Q.p3)});
  l.push_back({r.range(26, 29), mg ? P.s3 : P.p4});
  l.push_back({r.range(33, 37), mg ? P.s4 : (r.chance(.5f) ? Q.p4 : P.area)});
  o["apprend"] = learnJson(l);
  o["limite"] = genLimit(c, r, id, t1, mg ? "magique" : "physique", r.range(55, 60));
  c.species.push_back(o);
  return id;
}

// Gardien d'une région
static std::string genBoss(Content& c, Rng& r, const std::string& id, const std::string& t1, std::string t2, std::string& name) {
  if (t2 == t1) t2.clear();
  name = unique(c, [&] {
    std::string end = r.pick(BOSS_SUFFIX);  // un tirage après l'autre (voir genCreature)
    return cap(glue(r.pick(flavor(t1).roots), end));
  }, 10);
  Stats st = vary(r, {82, 30, 54, 50, 52, 50, 44}, 3);
  bool mg = st[4] > st[2];
  Json o = creatureJson(id, name, t1, t2, st);
  o["esquive"] = 0;
  o["immunites"] = Json::array({"sommeil"});
  o["forme"] = r.pick(BOSS_SHAPES);
  uint32_t c1 = jitter(r, typeColor(t1), 24), c2 = t2.empty() ? lighten(c1, .6f) : lighten(typeColor(t2), .3f);
  o["couleurs"] = Json::array({colorStr(c1), colorStr(c2)});
  const Pool& P = c.pools.at(t1);
  const Pool& Q = c.pools.at(t2.empty() ? t1 : t2);
  Learnset l = {{1, mg ? P.m3 : P.p3}, {1, P.area}, {1, mg ? Q.m1 : Q.p2}};
  auto sup = typeSupport(c, t2.empty() ? t1 : t2);
  if (!sup.empty() && r.chance(.6f)) l.push_back({1, r.pick(sup)});
  o["apprend"] = learnJson(l);
  c.species.push_back(o);
  return id;
}

// Classes de personnages : héros jouables, recrues et dresseurs
struct HeroClass {
  const char* id;
  const char* m;  // nom masculin, féminin, et version « dresseur » (minuscules)
  const char* f;
  const char* tm;
  const char* tf;
  int weapon;
  std::vector<int> hats;
  std::vector<const char*> kinds;  // types possibles
  Stats s;
  int crit, eva, acc;
};
static const std::vector<HeroClass>& classes() {
  static const std::vector<HeroClass> C = {
      {"epeiste", "Épéiste", "Épéiste", "bretteur", "bretteuse", 1, {0, 3}, {"normal", "metal", "vent", "feu"}, {55, 20, 56, 48, 30, 40, 50}, 8, 0, 100},
      {"guerrier", "Guerrier", "Guerrière", "pillard", "pillarde", 3, {0, 4}, {"normal", "roche", "feu", "metal"}, {70, 12, 66, 60, 20, 40, 36}, 10, 0, 95},
      {"chevalier", "Chevalier", "Chevalière", "lancier", "lancière", 6, {4, 0}, {"metal", "lumiere", "glace", "roche"}, {64, 30, 58, 64, 38, 54, 42}, 5, 0, 100},
      {"voleur", "Voleur", "Voleuse", "brigand", "brigande", 4, {1, 5}, {"ombre", "poison", "vent", "normal"}, {46, 18, 54, 38, 30, 38, 64}, 15, 8, 100},
      {"archer", "Archer", "Archère", "chasseur", "chasseuse", 5, {1, 5, 0}, {"vent", "plante", "foudre", "normal"}, {50, 24, 58, 42, 34, 40, 62}, 12, 0, 100},
      {"mage_noir", "Mage noir", "Mage noire", "sorcier", "sorcière", 2, {2}, {"ombre", "feu", "foudre", "eau", "glace", "esprit"}, {40, 70, 26, 36, 66, 51, 54}, 8, 5, 100},
      {"mage_blanc", "Mage blanc", "Mage blanche", "druide", "druidesse", 2, {1}, {"lumiere", "eau", "plante"}, {44, 60, 28, 42, 58, 50, 48}, 5, 0, 100},
  };
  return C;
}

// Personnage : héros jouable (trainer = false) ou dresseur ennemi.
// personal reçoit son prénom ; title son titre (« Mage noire », « sorcière »).
static std::string genPerson(Content& c, Rng& r, const std::string& id, const HeroClass& k, bool trainer, std::string* personal = nullptr,
                             std::string* title = nullptr) {
  bool fem = r.chance(.5f);
  std::string t = r.pick(k.kinds);
  std::string pname = personName(c, r);
  std::string cls = fem ? k.f : k.m, tcls = fem ? k.tf : k.tm;
  if (personal) *personal = pname;
  if (title) *title = trainer ? tcls : cls;
  // Apparence
  Json lk = Json::object();
  lk["id"] = "lk_" + id;
  lk["nom"] = trainer ? cap(tcls) : pname;
  lk["cheveux"] = colorStr(r.pick(HAIR));
  lk["peau"] = colorStr(r.pick(SKIN));
  lk["haut"] = colorStr(jitter(r, typeColor(t), 26));
  lk["bas"] = colorStr(r.pick(DARK));
  lk["coiffe"] = HATS[r.pick(k.hats)];
  lk["arme"] = WEAPONS[k.weapon];
  c.looks.push_back(lk);
  Stats st = vary(r, k.s, 4);
  if (trainer)
    for (auto& v : st) v = v * 92 / 100;
  Json o = Json::object();
  o["id"] = id;
  o["nom"] = trainer ? cap(tcls) : pname;
  o["type"] = t;
  o["base"] = baseJson(st);
  if (k.acc != 100) o["precision"] = k.acc;
  if (k.eva) o["esquive"] = k.eva;
  o["critique"] = k.crit;
  o["humain"] = true;
  o["apparence"] = "lk_" + id;
  o["role"] = cls + ", type " + typeName(typeOf(t));
  // Techniques selon la classe
  const Pool& P = c.pools.at(t);
  const Pool& N = c.pools.at("normal");
  const Support& S = c.sup;
  std::string kid = k.id;
  Learnset l;
  const char* limGenre = "physique";
  int limPower = r.range(72, 78);
  if (kid == "epeiste") l = {{1, P.p1}, {1, N.p2}, {6, P.p2}, {8, S.heal}, {10, S.warCry}, {14, P.p3}, {22, P.p4}, {30, P.area}};
  else if (kid == "guerrier") l = {{1, P.p2}, {1, N.p1}, {8, S.breaker}, {14, P.p3}, {20, P.p4}, {26, P.area}, {32, N.p4}};
  else if (kid == "chevalier") l = {{1, P.p2}, {1, S.guardAll}, {1, S.heal}, {12, P.p3}, {20, S.cure}, {26, P.p4}, {32, P.area}};
  else if (kid == "voleur") l = {{1, P.p1}, {1, N.p1}, {6, S.toxin}, {10, P.p2}, {16, P.p3}, {20, S.sleep}, {26, P.p4}};
  else if (kid == "archer") l = {{1, P.p1}, {1, P.p2}, {14, S.focus}, {18, P.area}, {24, P.p3}, {30, P.p4}};
  else if (kid == "mage_noir") {
    // Trois éléments, comme Isra : le type du personnage d'abord
    std::vector<std::string> el = {"feu", "eau", "foudre", "glace", "ombre", "esprit", "vent", "poison"};
    shuffle(r, el);
    el.erase(std::remove(el.begin(), el.end(), t), el.end());
    el.insert(el.begin(), t);
    const Pool &A = c.pools.at(el[0]), &B = c.pools.at(el[1]), &C2 = c.pools.at(el[2]);
    l = {{1, N.p1}, {1, A.s1}, {1, B.s1}, {1, C2.s1}, {15, S.sleep}, {16, S.haste}, {17, A.s2}, {18, B.s2}, {19, C2.s2}, {26, A.s3}, {32, A.s4}};
    limGenre = "magique";
  } else {  // mage blanc
    l = {{1, N.p1}, {1, S.heal}, {1, P.s1}, {5, S.cure}, {7, S.guardAll}, {9, S.healAll}, {12, S.revive}, {17, P.s2}, {24, P.s3}};
    limGenre = "soin";
    limPower = 45;
  }
  o["apprend"] = learnJson(l);
  if (!trainer) o["limite"] = genLimit(c, r, id, t, limGenre, limPower);
  c.species.push_back(o);
  return id;
}

// ===========================================================================
// Contenu de base : techniques, héros, créatures de départ
// ===========================================================================
Content generateBase(uint64_t seed) {
  Content c;
  Rng rm(stream(seed, 1));
  for (auto& t : types()) genTypeMoves(c, rm, t.id);
  genSupport(c, rm);
  // Six héros de classes toutes différentes
  Rng rh(stream(seed, 2));
  std::vector<int> cls;
  for (int i = 0; i < (int)classes().size(); i++) cls.push_back(i);
  shuffle(rh, cls);
  for (int i = 0; i < 6; i++) c.heroes.push_back(genPerson(c, rh, "xh" + std::to_string(i + 1), classes()[cls[i]], false));
  // Cinq créatures de départ, de types différents
  Rng rs(stream(seed, 3));
  std::vector<std::string> ts;
  for (auto& t : types())
    if (t.id != "normal") ts.push_back(t.id);
  shuffle(rs, ts);
  for (int i = 0; i < 5; i++) {
    std::string t2 = rs.chance(.25f) ? ts[size_t(i + 5 + rs.range(0, 4)) % ts.size()] : "";
    c.starters.push_back(genCreature(c, rs, "xs" + std::to_string(i + 1), ts[i], t2));
  }
  return c;
}

// ===========================================================================
// Régions
// ===========================================================================
int regionLevel(int k) { return 4 + 4 * (k - 1); }

static const std::vector<std::string> THEMES = {"vallee", "foret", "cendres", "neige"};
static std::string regionTheme(uint64_t seed, int k) {
  Rng r0(stream(seed, 7001));
  std::string th = r0.chance(.5f) ? "vallee" : "foret";  // la première région est verte
  for (int j = 2; j <= k; j++) {
    Rng r(stream(seed, 7000 + (uint64_t)j));
    std::vector<std::string> other;
    for (auto& t : THEMES)
      if (t != th) other.push_back(t);
    th = r.pick(other);
  }
  return th;
}
std::string regionName(uint64_t seed, int k) {
  static const std::map<std::string, std::vector<const char*>> PLACES = {
      {"vallee", {"Plaines", "Vallon", "Collines", "Landes", "Prairies"}},
      {"foret", {"Forêt", "Bois", "Sylve", "Futaie"}},
      {"cendres", {"Monts", "Cratère", "Caldeira", "Volcan"}},
      {"neige", {"Pics", "Glacier", "Toundra", "Cols"}}};
  Rng r(stream(seed, 5000 + (uint64_t)k));
  std::string place = placeName(nullptr, r);
  return std::string(r.pick(PLACES.at(regionTheme(seed, k)))) + " " + de(place);
}

// Types des créatures selon le décor (poids)
static std::string themeType(Rng& r, const std::string& th) {
  static const std::map<std::string, std::vector<std::pair<const char*, int>>> W = {
      {"vallee", {{"normal", 3}, {"plante", 2}, {"vent", 2}, {"eau", 2}, {"lumiere", 1}, {"foudre", 1}, {"poison", 1}}},
      {"foret", {{"plante", 3}, {"poison", 2}, {"ombre", 2}, {"esprit", 1}, {"vent", 1}, {"normal", 1}, {"eau", 1}}},
      {"cendres", {{"feu", 3}, {"roche", 2}, {"metal", 1}, {"foudre", 1}, {"ombre", 1}, {"normal", 1}}},
      {"neige", {{"glace", 3}, {"eau", 2}, {"roche", 1}, {"vent", 1}, {"metal", 1}, {"esprit", 1}, {"lumiere", 1}}}};
  auto& w = W.at(th);
  int total = 0;
  for (auto& p : w) total += p.second;
  int x = r.range(1, total);
  for (auto& p : w)
    if ((x -= p.second) <= 0) return p.first;
  return w[0].first;
}

// Tuiles de chaque décor
struct Tiles {
  char base, wall, grass, grass2, water, bridge, ob1, ob2, deco;
  const char* ambiance;
};
static Tiles themeTiles(const std::string& th) {
  if (th == "foret") return {'.', 'T', ',', 'z', '~', 'B', 'T', 'T', 'F', "lucioles"};
  if (th == "cendres") return {'a', 'm', 'g', 'g', 'l', 'b', 'm', 'd', 'R', "cendres"};
  if (th == "neige") return {'.', '#', 'n', 'n', '~', 'B', 'T', '#', 'i', "neige"};
  return {'.', 'T', ',', ',', '~', 'B', 'T', 'R', 'F', "brume"};
}

// Bruit doux (relief, herbes) : valeurs au hasard sur une grille, lissées entre les points
struct Noise {
  int gw, gh, cell;
  std::vector<float> v;
  Noise(Rng& r, int w, int h, int c) : gw(w / c + 2), gh(h / c + 2), cell(c), v((size_t)gw * gh) {
    for (auto& x : v) x = r.real();
  }
  float at(int x, int y) const {
    float fx = float(x) / cell, fy = float(y) / cell;
    int ix = (int)fx, iy = (int)fy;
    float tx = fx - ix, ty = fy - iy;
    tx = tx * tx * (3 - 2 * tx), ty = ty * ty * (3 - 2 * ty);
    auto V = [&](int a, int b) { return v[(size_t)b * gw + a]; };
    float a = V(ix, iy) + (V(ix + 1, iy) - V(ix, iy)) * tx, b = V(ix, iy + 1) + (V(ix + 1, iy + 1) - V(ix, iy + 1)) * tx;
    return a + (b - a) * ty;
  }
};

// Ce qu'il y a dans un coffre
static Json chestContent(Rng& r, int k) {
  if (r.chance(.25f)) return Json{{"or", (20 + 10 * k) * r.range(2, 4) / 2}};
  std::vector<std::pair<const char*, int>> o = {{"potion", 2}, {"ether", 1}, {"lanterne", 2}, {"remede", 2}, {"plume", 1}};
  if (k >= 3) o.push_back({"superpotion", 1}), o.push_back({"lanterne_argent", 1});
  if (k >= 6) o.push_back({"elixir", 1}), o.push_back({"lanterne_or", 1});
  auto& p = r.pick(o);
  return Json{{"objet", p.first}, {"quantite", p.second}};
}
static Json shopStock(int k) {
  Json s = Json::array({"potion", "remede", "lanterne", "ether", "plume"});
  if (k >= 3) s.push_back("superpotion"), s.push_back("lanterne_argent");
  if (k >= 6) s.push_back("elixir"), s.push_back("lanterne_or");
  // Équipement des boutiques du jeu, de plus en plus fort selon la région (si les données l'ont)
  static const std::vector<std::vector<const char*>> GEAR = {
      {"epee_fer", "baton_chene", "tunique_cuir", "amulette_vie"},
      {"hache_braise", "baton_lune", "arc_frene", "cotte_mailles", "robe_mage", "bague_vif", "talisman_esprit"},
      {"lame_givre", "sceptre_aurore", "armure_givre", "plume_ange", "pendentif_brume"}};
  for (const char* id : GEAR[k >= 6 ? 2 : k >= 3 ? 1 : 0])
    if (hasItem(id)) s.push_back(id);
  return s;
}

static const int D4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

static Json say(const std::string& s) { return Json{{"action", "dire"}, {"texte", s}}; }

// Type d'attaque le plus efficace contre un boss (pour les conseils des habitants)
static std::string weakness(const std::string& t1, const std::string& t2) {
  float best = 1;
  std::string w;
  for (auto& a : types()) {
    float e = typeEff(typeOf(a.id), typeOf(t1)) * (t2.empty() ? 1.f : typeEff(typeOf(a.id), typeOf(t2)));
    if (e > best) best = e, w = a.name;
  }
  return w;
}

Region generateRegion(uint64_t seed, int k, Content& c) {
  Region R;
  R.index = k;
  R.level = regionLevel(k);
  const int L = R.level;
  Rng r(stream(seed, 1000 + (uint64_t)k));
  const std::string theme = regionTheme(seed, k);
  const Tiles tt = themeTiles(theme);
  R.name = regionName(seed, k);
  R.nextName = regionName(seed, k + 1);
  R.village = placeName(&c, r);
  const std::string px = "x" + std::to_string(k);  // préfixe des identifiants de la région
  R.bossFlag = px + "_boss";

  // --- Créatures, gardien, dresseurs, recrue ---
  int nWild = 5 + k % 2;
  for (int i = 0; i < nWild; i++) {
    std::string t1 = themeType(r, theme), t2 = r.chance(.3f) ? themeType(r, theme) : "";
    R.wild.push_back(genCreature(c, r, px + "c" + std::to_string(i + 1), t1, t2));
  }
  std::string bt1 = themeType(r, theme), bt2 = r.chance(.6f) ? themeType(r, theme) : "";
  if (bt2 == bt1) bt2.clear();
  std::string boss = genBoss(c, r, px + "b", bt1, bt2, R.bossName);
  const auto& CL = classes();
  std::string tr[2];
  for (int i = 0; i < 2; i++) {
    const HeroClass& hc = CL[r.range(0, 5)];  // pas de druide parmi les dresseurs
    tr[i] = genPerson(c, r, px + "t" + std::to_string(i + 1), hc, true);
  }
  bool hasRecruit = k == 1 || r.chance(.45f);
  std::string recruit, recruitName, recruitClass;
  if (hasRecruit) recruit = genPerson(c, r, px + "r", CL[r.range(0, (int)CL.size() - 1)], false, &recruitName, &recruitClass);

  // --- Carte ---
  const int W = r.range(54, 60), H = r.range(34, 38);
  std::vector<std::string> g(H, std::string(W, tt.base));
  std::vector<std::string> kind(H, std::string(W, ' '));  // ' ' libre, 'p' chemin, 'v' village, 'x' réservé
  auto in = [&](int x, int y) { return x >= 0 && y >= 0 && x < W && y < H; };
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++)
      if (x < 2 || y < 2 || x >= W - 2 || y >= H - 2) g[y][x] = tt.wall;

  // Village : bâtiments au-dessus d'une rue
  const int vy = r.range(3, H - 14), street = vy + 4, VX = 22;
  for (int y = vy - 1; y <= vy + 9; y++)
    for (int x = 2; x <= VX; x++)
      if (y >= 2 && y < H - 2) g[y][x] = tt.base, kind[y][x] = 'v';
  for (int x = 2; x <= VX; x++) g[street][x] = '=';
  struct B {
    int x, y, w, h;
    std::string kind, name, ev;
  };
  std::string shop = theme == "cendres" || theme == "neige" ? "boutique2" : "boutique1";
  std::vector<B> buildings = {{3, vy, 5, 4, "soin", "Guérisseuse", px + "_soin"},
                              {9, vy, 5, 4, shop, "Boutique", px + "_boutique"},
                              {15, vy + 1, 5, 3, "maison", "Maison", px + "_maison"}};
  if (theme == "vallee") g[vy + 1][21] = 'W';
  R.startX = 2, R.startY = street;

  // Chemin principal : de la sortie du village jusqu'au gardien, à droite
  const int bossX = W - 8, ey = r.range(5, H - 7);
  bool river = r.chance(.55f) && bossX - 14 > 30;
  int rx = river ? r.range(30, bossX - 14) : 0, ry = 0;
  std::vector<std::pair<int, int>> wp = {{VX, street}};
  std::vector<std::pair<int, int>> path;  // cases du chemin, dans l'ordre (hors village)
  std::vector<int> segOf;                 // tronçon de chaque case
  {
    int x = VX, y = street;
    while (x < bossX - 8) {
      int nx = std::min(bossX - 2, x + r.range(6, 10)), ny = std::clamp(y + r.range(-6, 6), 4, H - 5);
      if (river && x < rx - 2 && nx >= rx - 2) {  // la rivière se traverse en ligne droite (pont)
        ry = ny;
        wp.push_back({rx - 2, ny});
        wp.push_back({rx + 4, ny});
        x = rx + 4, y = ny;
        continue;
      }
      wp.push_back({nx, ny});
      x = nx, y = ny;
    }
    wp.push_back({bossX - 1, ey});
    for (size_t i = 1; i < wp.size(); i++) {
      int cx = wp[i - 1].first, cy = wp[i - 1].second;
      while (cx != wp[i].first || cy != wp[i].second) {
        bool h = cx != wp[i].first && (cy == wp[i].second || r.chance(.6f));
        if (h) cx += cx < wp[i].first ? 1 : -1;
        else cy += cy < wp[i].second ? 1 : -1;
        if (kind[cy][cx] != 'p') {
          g[cy][cx] = '=', kind[cy][cx] = 'p';
          path.push_back({cx, cy});
          segOf.push_back((int)i);
        }
      }
    }
  }
  auto nearPath = [&](int x, int y, int d) {
    for (int j = -d; j <= d; j++)
      for (int i = -d; i <= d; i++)
        if (in(x + i, y + j) && kind[y + j][x + i] == 'p') return true;
    return false;
  };
  // Relief, mares, hautes herbes
  Noise obs(r, W, H, 5), obs2(r, W, H, 3), grs(r, W, H, 6), pond(r, W, H, 7);
  for (int y = 2; y < H - 2; y++)
    for (int x = 2; x < W - 2; x++) {
      if (kind[y][x] != ' ' || g[y][x] != tt.base || x >= bossX - 1) continue;
      float o = .65f * obs.at(x, y) + .35f * obs2.at(x, y);
      if (!nearPath(x, y, 1) && o > .6f) g[y][x] = r.chance(.75f) ? tt.ob1 : tt.ob2;
      else if (!nearPath(x, y, 1) && pond.at(x, y) > .78f) g[y][x] = tt.water;
      else if (grs.at(x, y) > .55f) g[y][x] = r.chance(.3f) ? tt.grass2 : tt.grass;
      else if (r.chance(.03f)) g[y][x] = tt.deco;
    }
  // Rivière (pont sur le chemin)
  if (river) {
    int off = 0;
    for (int y = 2; y < H - 2; y++) {
      if (y != ry && y != ry + 1 && y != ry - 1) off = std::clamp(off + r.range(-1, 1), 0, 2);
      for (int x = rx + off; x <= rx + off + 1; x++) {
        if (kind[y][x] == 'p') g[y][x] = tt.bridge;
        else if (kind[y][x] == ' ') g[y][x] = tt.water, kind[y][x] = 'x';
      }
    }
  }
  // Certains tronçons du chemin traversent les hautes herbes
  std::set<int> grassy;
  for (size_t i = 2; i + 1 < wp.size(); i++)
    if (r.chance(.35f)) grassy.insert((int)i);
  for (size_t i = 0; i < path.size(); i++) {
    auto [x, y] = path[i];
    if (grassy.count(segOf[i]) && g[y][x] == '=') g[y][x] = tt.grass;
  }
  // Le gardien bloque le seul passage vers la sortie
  for (int y = 0; y < H; y++)
    for (int x = bossX; x < W; x++) {
      bool corridor = (y == ey || y == ey + 1) && x >= bossX;
      g[y][x] = corridor ? (x >= bossX + 2 ? '=' : tt.base) : tt.wall;
      kind[y][x] = 'x';
    }
  R.exitX = W - 1, R.exitY = ey;

  // --- Objets : vérification que tout reste accessible à pied ---
  struct Person {
    int x, y, dir;
    std::string look, event;
    std::vector<std::string> lines;
    int sight = 0;
    std::string hideIf, sightUntil;
  };
  std::vector<Person> npcs;
  std::vector<std::pair<std::pair<int, int>, Json>> chests;
  std::vector<std::pair<std::pair<int, int>, std::string>> signs;
  auto blockedAt = [&](int x, int y) {
    if (!in(x, y) || !tileWalkable(g[y][x])) return true;
    for (auto& b : buildings)
      if (x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h) return true;
    for (auto& n : npcs)
      if (n.x == x && n.y == y) return true;
    for (auto& ch : chests)
      if (ch.first.first == x && ch.first.second == y) return true;
    for (auto& s : signs)
      if (s.first.first == x && s.first.second == y) return true;
    return x >= bossX && x <= bossX + 1 && (y == ey || y == ey + 1);
  };
  auto reach = [&] {
    std::vector<std::vector<char>> seen(H, std::vector<char>(W, 0));
    std::deque<std::pair<int, int>> q = {{R.startX, R.startY}};
    seen[R.startY][R.startX] = 1;
    while (!q.empty()) {
      auto [x, y] = q.front();
      q.pop_front();
      for (auto& d : D4) {
        int nx = x + d[0], ny = y + d[1];
        if (in(nx, ny) && !seen[ny][nx] && !blockedAt(nx, ny)) seen[ny][nx] = 1, q.push_back({nx, ny});
      }
    }
    return seen;
  };
  auto touches = [&](const std::vector<std::vector<char>>& s, int x, int y) {
    for (auto& d : D4)
      if (in(x + d[0], y + d[1]) && s[y + d[1]][x + d[0]]) return true;
    return false;
  };
  auto allReachable = [&] {
    auto s = reach();
    if (!s[ey][bossX - 1]) return false;
    for (auto& n : npcs)
      if (!touches(s, n.x, n.y)) return false;
    for (auto& ch : chests)
      if (!touches(s, ch.first.first, ch.first.second)) return false;
    for (auto& sg : signs)
      if (!touches(s, sg.first.first, sg.first.second)) return false;
    for (auto& b : buildings)
      if (!s[b.y + b.h][b.x + b.w / 2]) return false;
    return true;
  };
  // Les poches d'herbe ou de sol coupées du reste deviennent du relief
  {
    auto s = reach();
    for (int y = 2; y < H - 2; y++)
      for (int x = 2; x < bossX; x++)
        if (!s[y][x] && tileWalkable(g[y][x]) && kind[y][x] != 'v') g[y][x] = tt.ob1;
  }

  // Habitants du village
  std::vector<std::string> wildNames;
  for (auto& w : R.wild)
    for (auto& s : c.species)
      if (s["id"] == w) wildNames.push_back(s["nom"].get<std::string>());
  std::string weak = weakness(bt1, bt2);
  std::vector<std::string> tips = {
      "Bienvenue à " + R.village + " ! Les voyageurs se font rares par ici.",
      R.bossName + " garde la route vers " + R.nextName + "." +
          (weak.empty() ? " Personne ne sait ce qu'il craint…" : " Il paraît qu'il craint le type " + weak + "."),
      "Dans les hautes herbes, on croise des " + wildNames[0] + " et des " + wildNames[1] + ".",
      "Une Lanterne capture plus facilement une créature affaiblie.",
      "Les dresseurs postés le long du chemin vous défient dès qu'ils vous voient.",
      "Échap > Tactiques : votre équipe peut combattre toute seule (Tab en combat).",
      "On raconte que la route continue sans fin, de région en région…",
      "Plus on va loin, plus les créatures sont fortes. Capturez-en de nouvelles !"};
  std::vector<int> tipIx = {0, 1, 2, 3, 4, 5, 6, 7};
  shuffle(r, tipIx);
  std::swap(*std::find(tipIx.begin(), tipIx.end(), 1), tipIx[0]);  // le conseil sur le gardien est toujours donné
  int nVill = r.range(2, 3);
  std::vector<std::pair<int, int>> spots;
  for (int y = street + 2; y <= street + 4; y++)
    for (int x = 3; x <= 20; x += 2) spots.push_back({x, y});
  shuffle(r, spots);
  size_t spot = 0;
  for (int i = 0; i < nVill; i++) {
    Json lk = Json::object();
    lk["id"] = "lk_" + px + "v" + std::to_string(i + 1);
    lk["nom"] = "Habitant";
    lk["cheveux"] = colorStr(r.pick(HAIR));
    lk["peau"] = colorStr(r.pick(SKIN));
    lk["haut"] = colorStr(jitter(r, r.pick(ROOF), 30));
    lk["bas"] = colorStr(r.pick(DARK));
    lk["coiffe"] = HATS[r.chance(.3f) ? r.range(1, 5) : 0];
    lk["arme"] = "aucune";
    c.looks.push_back(lk);
    Person p{spots[spot].first, spots[spot].second, r.range(0, 3), lk["id"].get<std::string>(), "", {tips[tipIx[i]]}};
    spot++;
    npcs.push_back(p);
  }
  signs.push_back({{VX - 1, vy + 3}, R.village + ". À l'est : " + R.bossName + ", gardien de la route."});
  if (hasRecruit) {
    Person p{spots[spot].first, spots[spot].second, 1, "lk_" + recruit, px + "_recrue", {}};
    p.hideIf = px + "_recrue";
    spot++;
    npcs.push_back(p);
  }

  // Dresseurs le long du chemin, tournés vers lui
  int nT = std::min(5, 2 + k / 2);
  std::vector<std::pair<int, int>> cand;
  for (auto& pc : path)
    if (pc.first >= 26 && pc.first <= bossX - 6) cand.push_back(pc);
  std::vector<std::string> trainerNames, trainerTitles, trainerEvents;
  for (int i = 0; i < nT && !cand.empty(); i++) {
    size_t lo = cand.size() * i / nT, hi = cand.size() * (i + 1) / nT;
    if (hi <= lo) continue;
    for (int tries = 0; tries < 8; tries++) {
      auto [cx, cy] = cand[lo + (size_t)r.range(0, int(hi - lo) - 1)];
      int d = r.range(0, 3);
      static const int DX[] = {0, 0, -1, 1}, DY[] = {-1, 1, 0, 0};
      int nx = cx + DX[d], ny = cy + DY[d];
      if (!in(nx, ny) || nx < 3 || ny < 3 || nx >= bossX - 2 || ny >= H - 3 || kind[ny][nx] != ' ') continue;
      char old = g[ny][nx];
      g[ny][nx] = tt.base;
      // il regarde le chemin : direction opposée au décalage
      static const int FACE[] = {1, 0, 3, 2};  // haut↔bas, gauche↔droite (0 haut, 1 bas, 2 gauche, 3 droite)
      std::string sid = tr[i % 2], ev = px + "_dresseur" + std::to_string(i + 1);
      Person p{nx, ny, FACE[d], "lk_" + sid, ev, {}};
      p.sight = 3;
      p.sightUntil = ev;
      npcs.push_back(p);
      if (!allReachable()) {
        npcs.pop_back();
        g[ny][nx] = old;
        continue;
      }
      kind[ny][nx] = 'x';
      trainerNames.push_back(personName(c, r));
      trainerTitles.push_back(sid);
      trainerEvents.push_back(ev);
      break;
    }
  }
  // Coffres dans les recoins
  {
    auto s = reach();
    std::vector<std::pair<int, int>> pockets;
    for (int y = 3; y < H - 3; y++)
      for (int x = VX + 2; x < bossX - 2; x++) {
        if (!s[y][x] || kind[y][x] != ' ' || nearPath(x, y, 1) || !tileWalkable(g[y][x])) continue;
        int walls = 0;
        for (auto& d : D4) walls += !tileWalkable(g[y + d[1]][x + d[0]]);
        if (walls >= 2) pockets.push_back({x, y});
      }
    shuffle(r, pockets);
    int nC = r.range(3, 5);
    for (auto& pk : pockets) {
      if ((int)chests.size() >= nC) break;
      bool far = true;
      for (auto& ch : chests) far = far && std::abs(ch.first.first - pk.first) + std::abs(ch.first.second - pk.second) >= 8;
      if (!far) continue;
      chests.push_back({pk, chestContent(r, k)});
      if (!allReachable()) chests.pop_back();
    }
  }
  // Panneau avant le gardien
  if (in(bossX - 2, ey - 1) && kind[ey - 1][bossX - 2] == ' ') {
    char old = g[ey - 1][bossX - 2];
    g[ey - 1][bossX - 2] = tt.base;
    signs.push_back({{bossX - 2, ey - 1}, "Ici veille " + R.bossName + ". Au-delà : " + R.nextName + "."});
    if (!allReachable()) signs.pop_back(), g[ey - 1][bossX - 2] = old;
  }

  // --- Carte au format data/cartes ---
  Json m = Json::object();
  m["id"] = "expedition_" + std::to_string(k);
  m["nom"] = R.name;
  m["theme"] = theme;
  m["ambiance"] = Json{{"effet", tt.ambiance}};
  if (theme == "vallee") m["ambiance"]["jusqua"] = R.bossFlag;  // la brume se lève quand le gardien tombe
  m["tuiles"] = g;
  Json jb = Json::array();
  for (auto& b : buildings)
    jb.push_back(Json{{"x", b.x}, {"y", b.y}, {"l", b.w}, {"h", b.h}, {"genre", b.kind}, {"nom", b.name}, {"toit", colorStr(r.pick(ROOF))}, {"evenement", b.ev}});
  m["batiments"] = jb;
  Json jn = Json::array();
  for (auto& n : npcs) {
    Json o = Json{{"x", n.x}, {"y", n.y}, {"apparence", n.look}, {"direction", dirName(n.dir)}};
    if (!n.event.empty()) o["evenement"] = n.event;
    if (!n.hideIf.empty()) o["cache_si"] = n.hideIf;
    if (n.sight > 0) o["vue"] = n.sight, o["vue_jusqua"] = n.sightUntil;
    if (!n.lines.empty()) o["dialogue"] = n.lines;
    jn.push_back(o);
  }
  m["habitants"] = jn;
  Json jc = Json::array();
  for (auto& ch : chests) {
    Json o = Json{{"x", ch.first.first}, {"y", ch.first.second}};
    for (auto& [kk, v] : ch.second.items()) o[kk] = v;
    jc.push_back(o);
  }
  m["coffres"] = jc;
  Json js = Json::array();
  for (auto& s : signs) js.push_back(Json{{"x", s.first.first}, {"y", s.first.second}, {"texte", s.second}});
  m["panneaux"] = js;
  m["passages"] = Json::array();
  int mid = (VX + bossX) / 2;
  std::vector<std::string> poolA(R.wild.begin(), R.wild.begin() + 3), poolB(R.wild.begin() + 2, R.wild.end());
  int maxN = k == 1 ? 2 : 3;
  m["zones"] = Json::array({Json{{"x", VX + 1}, {"y", 0}, {"l", mid - VX - 1}, {"h", H}, {"niveau_min", std::max(2, L - 1)}, {"niveau_max", L + 1},
                                 {"max_ennemis", maxN}, {"creatures", poolA}},
                            Json{{"x", mid}, {"y", 0}, {"l", bossX - mid}, {"h", H}, {"niveau_min", L}, {"niveau_max", L + 2}, {"max_ennemis", maxN},
                                 {"creatures", poolB}}});
  m["boss"] = Json::array({Json{{"x", bossX}, {"y", ey}, {"espece", boss}, {"drapeau", R.bossFlag}, {"evenement", px + "_gardien"}}});
  R.map = m;

  // --- Événements ---
  Json& E = R.events;
  E[px + "_soin"] = Json{{"actions", Json::array({Json{{"action", "soigner"}},
                                                    say("Guérisseuse : Reposez-vous… Voilà, toute votre équipe est en pleine forme !")})}};
  E[px + "_boutique"] = Json{{"actions", Json::array({Json{{"action", "boutique"}, {"objets", shopStock(k)}}})}};
  E[px + "_maison"] = Json{{"pages", Json::array({Json{{"si", Json{{"sans_drapeau", px + "_cadeau"}}},
                                                      {"actions", Json::array({Json{{"action", "drapeau"}, {"nom", px + "_cadeau"}},
                                                                               say("Habitante : Vous partez vers " + R.bossName + " ? Prenez ceci."),
                                                                               Json{{"action", "donner"}, {"objet", "potion"}, {"quantite", 2}}})}},
                                                 Json{{"actions", Json::array({say("Habitante : " + tips[tipIx[nVill % tips.size()]])})}}})}};
  // Gardien : un acolyte dans les deux premières régions, deux ensuite, des renforts à partir de la région 5
  int bossLvl = L + 2;
  float hpMult = std::min(3.2f, 2.f + .08f * k);
  Json foes = Json::array({Json{{"espece", R.wild[0]}, {"niveau", L - 1}},
                           Json{{"espece", boss}, {"niveau", bossLvl}, {"pv", std::round(hpMult * 10) / 10}, {"boss", true}}});
  if (k >= 3) foes.push_back(Json{{"espece", R.wild[(size_t)nWild - 1]}, {"niveau", L - 1}});
  Json fight = Json{{"action", "combat"}, {"ennemis", foes}};
  if (k >= 5) fight["renforts"] = Json::array({Json{{"espece", R.wild[2]}, {"niveau", L}}});
  fight["boss"] = true;
  fight["fuite"] = false;
  fight["capture"] = false;
  fight["victoire"] = Json::array({Json{{"action", "drapeau"}, {"nom", R.bossFlag}},
                                   say(R.bossName + " s'efface dans un dernier grondement. La route vers " + R.nextName + " est libre !"),
                                   Json{{"action", "or"}, {"quantite", 40 + 20 * k}}});
  E[px + "_gardien"] = Json{{"actions", Json::array({say(R.bossName + ", gardien de la région, vous barre la route !"),
                                                      Json{{"action", "question"}, {"texte", "Affronter " + R.bossName + " ?"}, {"oui", Json::array({fight})}}})}};
  // Dresseurs
  static const std::vector<std::string> TAUNT = {"Halte ! Personne ne passe sans m'affronter.", "Vos créatures ont l'air fortes. Voyons ça !",
                                                 "Encore des voyageurs ? En garde !", "Je m'entraîne ici depuis des lunes. À vous !"};
  static const std::vector<std::string> LOST = {"Bien joué… Vous irez loin.", "Je n'ai rien vu venir !", "Quelle équipe ! Respect."};
  static const std::vector<std::string> AFTER = {"Bonne route, voyageurs.", "Je vais m'entraîner encore.", R.bossName + " est bien plus fort que moi. Prudence !"};
  for (size_t i = 0; i < trainerNames.size(); i++) {
    std::string ev = trainerEvents[i], sid = trainerTitles[i];
    std::string title, nm = trainerNames[i];
    for (auto& s : c.species)
      if (s["id"] == sid) title = s["nom"].get<std::string>();
    std::string lower = title;
    if (!lower.empty() && lower[0] >= 'A' && lower[0] <= 'Z') lower[0] = char(lower[0] + 32);
    bool fem = lower.size() > 1 && (lower.back() == 'e');
    std::string who = std::string(fem ? "La " : "Le ") + lower + " " + nm;
    Json en = Json::array({Json{{"espece", sid}, {"niveau", L + 1}}, Json{{"espece", R.wild[(size_t)r.range(0, nWild - 1)]}, {"niveau", L}}});
    Json f = Json{{"action", "combat"}, {"nom", who}, {"ennemis", en}};
    if (k >= 2) f["renforts"] = Json::array({Json{{"espece", R.wild[(size_t)r.range(0, nWild - 1)]}, {"niveau", L + 1}}});
    f["fuite"] = false;
    f["capture"] = false;
    f["victoire"] = Json::array({Json{{"action", "drapeau"}, {"nom", ev}}, say(nm + " : " + r.pick(LOST))});
    E[ev] = Json{{"pages", Json::array({Json{{"si", Json{{"sans_drapeau", ev}}}, {"actions", Json::array({say(nm + " : " + r.pick(TAUNT)), f})}},
                                        Json{{"actions", Json::array({say(nm + " : " + r.pick(AFTER))})}}})}};
  }
  // Recrue : rejoint l'équipe (après un duel à partir de la région 3)
  if (hasRecruit) {
    std::string ev = px + "_recrue";
    Json join = Json::array({Json{{"action", "drapeau"}, {"nom", ev}}, Json{{"action", "recruter"}, {"espece", recruit}, {"niveau", L}}});
    Json yes;
    if (k >= 3) {
      Json duel = Json{{"action", "combat"}, {"ennemis", Json::array({Json{{"espece", recruit}, {"niveau", L + 1}, {"pv", 2.0}, {"boss", true}}})}};
      duel["boss"] = true;
      duel["fuite"] = false;
      duel["capture"] = false;
      Json win = Json::array({say(recruitName + " : Bien joué ! Je vous suis.")});
      for (auto& a : join) win.push_back(a);
      duel["victoire"] = win;
      yes = Json::array({say(recruitName + " : Montrez-moi d'abord ce que vous valez !"), duel});
    } else
      yes = join;
    E[ev] = Json{{"actions", Json::array({say(recruitName + " (" + recruitClass + ") : Je voudrais aller plus loin, moi aussi."),
                                          Json{{"action", "question"}, {"texte", "Proposer à " + recruitName + " de vous suivre ?"}, {"oui", yes}}})}};
  }
  c.regions = k;
  return R;
}

}  // namespace procgen
