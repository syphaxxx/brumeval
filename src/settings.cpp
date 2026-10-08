#include "settings.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "arena.hpp"
#include "events.hpp"
#include "game.hpp"
#include "sprites.hpp"
#include "tactics.hpp"
#include "world.hpp"

static const Color WHITE = rgb(0xffffff), GOLD = rgb(0xffd34d), MUTED = rgb(0xaab3d8), GREEN = rgb(0x7dffa8), RED = rgb(0xff8a7a);

// ---------------------------------------------------------------------------
// Outils
// ---------------------------------------------------------------------------
static double round6(double v) { return std::round(v * 1e6) / 1e6; }
static int decimalsFor(double step) { return step >= 1 ? 0 : step >= .1 ? 1 : 2; }
static std::string fmtNum(double v, int dec) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", dec, v);
  std::string s = b;
  if (dec > 0) {
    while (s.back() == '0') s.pop_back();
    if (s.back() == '.') s.pop_back();
  }
  for (auto& c : s)
    if (c == '.') c = ',';
  return s;
}

// Objet d'une liste JSON par son identifiant
static Json& byId(Json& arr, const std::string& id) {
  for (auto& o : arr)
    if (jget<std::string>(o, "id", "") == id) return o;
  throw std::runtime_error("identifiant introuvable : " + id);
}
// Remet les clés d'un objet dans l'ordre habituel des fichiers
static void reorderKeys(Json& o, const std::vector<std::string>& order) {
  Json r = Json::object();
  for (auto& k : order)
    if (o.contains(k)) r[k] = o[k];
  for (auto it = o.begin(); it != o.end(); ++it)
    if (!r.contains(it.key())) r[it.key()] = it.value();
  o = r;
}
static const std::vector<std::string> SPECIES_KEYS = {"id", "nom", "type", "types", "base", "precision", "esquive", "critique", "resistances",
                                                      "immunites", "humain", "apparence", "role", "forme", "couleurs", "apprend", "limite",
                                                      "tactiques"};
static const std::vector<std::string> MOVE_KEYS = {"id", "nom", "type", "genre", "cible", "puissance", "cout", "precision", "critique", "rythme",
                                                   "effet", "effets", "description"};
static const std::vector<std::string> ITEM_KEYS = {"id", "nom", "description", "prix", "combat", "menu", "important", "soin_pv", "soin_pm",
                                                   "rappel", "capture", "soin_statut"};
static const char* BASE_KEYS[N_BASE] = {"pv", "pm", "attaque", "defense", "magie", "resistance", "vitesse"};
static const char* BASE_NAMES[N_BASE] = {"PV", "PM", "Attaque", "Défense", "Magie", "Résistance", "Vitesse"};
static const char* KIND_IDS[] = {"physique", "magique", "soin", "rappel", "statut"};
static const char* KIND_NAMES[] = {"Physique", "Magique", "Soin", "Réanimation", "Statut"};
static const char* TARGET_IDS[] = {"ennemi", "tous_ennemis", "allie", "tous_allies", "allie_ko"};
static const char* TARGET_NAMES[] = {"Un ennemi", "Tous les ennemis", "Un allié", "Toute l'équipe", "Un allié K.O."};
static const char* STAGE_IDS[] = {"attaque", "defense", "magie", "resistance", "vitesse"};
static const Status STATUSES[] = {Status::Poison, Status::Burn, Status::Paralysis, Status::Sleep};

static MenuItem numItem(const std::string& label, const std::string& help, std::function<double()> get, std::function<void(double)> set,
                        double step, double mn, double mx) {
  MenuItem it{label, "", help, true};
  int dec = decimalsFor(step);
  it.rightFn = [get, dec] { return "< " + fmtNum(get(), dec) + " >"; };
  it.adjust = [get, set, step, mn, mx](int d) { set(std::clamp(round6(get() + d * step), mn, mx)); };
  return it;
}
static MenuItem toggleItem(const std::string& label, const std::string& help, std::function<bool()> get, std::function<void(bool)> set) {
  MenuItem it{label, "", help, true, [get, set] { set(!get()); }};
  it.adjust = [get, set](int) { set(!get()); };
  it.rightFn = [get] { return std::string(get() ? "Oui" : "Non"); };
  return it;
}
// Choix dans une liste de n valeurs (de lo à n-1), avec gauche/droite
static MenuItem cycleItem(const std::string& label, const std::string& help, std::function<int()> get, std::function<void(int)> set, int n,
                          std::function<std::string(int)> name, int lo = 0) {
  MenuItem it{label, "", help, true};
  it.rightFn = [get, name] { return "< " + name(get()) + " >"; };
  it.adjust = [get, set, n, lo](int d) {
    int span = n - lo;
    set(lo + ((get() - lo + d) % span + span) % span);
  };
  return it;
}

MenuItem Settings::textItem(const std::string& label, const std::string& title, std::function<std::string()> get,
                            std::function<void(const std::string&)> set, int maxChars) {
  MenuItem it{label, "", "Entrée : modifier le texte.", true};
  it.rightFn = [get] {
    std::string s = get();
    return utf8Prefix(s, 8) + (utf8Prefix(s, 8).size() < s.size() ? "…" : "");
  };
  it.act = [this, title, get, set, maxChars] { G.editText(title, get(), maxChars, set); };
  return it;
}

Settings::Settings(Game& g) : G(g) {}

void Settings::snapshot() {
  for (int f = 0; f < N_DATAFILES; f++) saved_[f] = dataDoc(DataFile(f));
}
bool Settings::dirty(DataFile f) const { return dataDoc(f) != saved_[f]; }
int Settings::dirtyCount() const {
  int n = 0;
  for (int f = 0; f < N_DATAFILES; f++) n += dirty(DataFile(f));
  return n;
}
void Settings::say(const std::string& m) {
  message_ = m;
  messageT_ = G.time;
}

bool Settings::change(DataFile f, const std::function<void(Json&)>& fn) {
  Json backup = dataDoc(f);
  try {
    fn(dataDoc(f));
    rebuildData();
  } catch (const std::exception& e) {
    dataDoc(f) = backup;
    try {
      rebuildData();
    } catch (...) {
    }
    say(std::string("Refusé : ") + e.what());
    return false;
  }
  problems_ = checkData();
  return true;
}

void Settings::save() {
  std::string names;
  try {
    for (int f = 0; f < N_DATAFILES; f++)
      if (dirty(DataFile(f))) {
        saveDataDoc(DataFile(f));
        names += (names.empty() ? "" : ", ") + std::string(dataFileName(DataFile(f)));
      }
  } catch (const std::exception& e) {
    say(std::string("Erreur : ") + e.what());
    return;
  }
  snapshot();
  say(names.empty() ? "Rien à enregistrer." : "Enregistré : " + names);
}

void Settings::revert() {
  try {
    loadData();
  } catch (const std::exception& e) {
    say(std::string("Erreur : ") + e.what());
    return;
  }
  snapshot();
  problems_ = checkData();
  say("Modifications annulées : données rechargées depuis les fichiers.");
}

void Settings::open() {
  snapshot();
  problems_ = checkData();
  grid_ = false;
  menuMain();
}

void Settings::quit() {
  panel_ = Panel::None;
  G.titleMenu();
}

// ---------------------------------------------------------------------------
// Menu principal
// ---------------------------------------------------------------------------
void Settings::menuMain(int sel) {
  G.menus.clear();
  grid_ = false;
  panel_ = Panel::None;
  Menu m;
  m.x = 4, m.y = 18, m.w = 152, m.rows = 14;
  m.sel = sel;
  m.items.push_back(menuHeader("Données"));
  m.items.push_back({"Règles du jeu", ">", "Départ, rencontres, combat, états, récompenses, capture, formules, butin.", true, [this] { menuRules(); }});
  m.items.push_back({"Espèces", ">", "Héros, créatures, boss et adversaires : types, statistiques, techniques.", true, [this] { menuSpeciesList(); }});
  m.items.push_back({"Techniques", ">", "Techniques, sorts et Limites : puissance, précision, effets.", true, [this] { menuMovesList(); }});
  m.items.push_back({"Types", ">", "Table d'efficacité et immunités des types.", true, [this] { menuTypes(); }});
  m.items.push_back({"Objets", ">", "Prix, soins, capture.", true, [this] { menuItemsList(); }});
  m.items.push_back(menuHeader("Fichiers"));
  m.items.push_back({"Tester dans l'Arène", "", "Essayer les réglages en combat (sans les enregistrer). Revenez ensuite par Outils > Réglages.", true, [this] {
                       if (!G.arena_) G.arena_ = std::make_unique<Arena>(G);
                       G.mode = Mode::Arena;
                       G.arena_->open();
                     }});
  MenuItem sv{"Enregistrer", "", "Écrit les fichiers modifiés dans data/.", true, [this] { save(); }};
  sv.rightFn = [this] {
    int n = dirtyCount();
    return n ? std::to_string(n) + " fichier" + (n > 1 ? "s" : "") : std::string("à jour");
  };
  m.items.push_back(sv);
  m.items.push_back({"Tout annuler", "", "Recharge les fichiers : toutes les modifications non enregistrées sont perdues.", true,
                     [this] { revert(); }});
  m.items.push_back({"Quitter", "", "", true, [this] {
                       if (!dirtyCount()) return quit();
                       Menu q;
                       q.title = "Modifications non enregistrées";
                       q.x = 60, q.y = 80, q.w = 200, q.rows = 3;
                       q.items.push_back({"Enregistrer et quitter", "", "", true, [this] {
                                            save();
                                            quit();
                                          }});
                       q.items.push_back({"Quitter sans enregistrer", "", "Les données reviennent au contenu des fichiers.", true, [this] {
                                            revert();
                                            quit();
                                          }});
                       q.items.push_back({"Annuler", "", "", true, [this] { G.menus.pop(); }});
                       G.menus.push(q);
                     }});
  m.onCancel = m.items.back().act;
  G.menus.push(m);
}

// ---------------------------------------------------------------------------
// Règles
// ---------------------------------------------------------------------------
void Settings::menuRules(int sel) {
  G.menus.clear();
  grid_ = false;
  panel_ = Panel::None;
  Menu m;
  m.title = "Règles du jeu";
  m.x = 4, m.y = 18, m.w = 312, m.rows = 14;  // laisse la place à l'aide en bas
  m.sel = sel;
  auto setDepart = [this](const char* key, Json v) { change(DF_RULES, [key, v](Json& d) { d["depart"][key] = v; }); };
  m.items.push_back(menuHeader("Départ"));
  m.items.push_back(numItem("Niveau de départ", "Niveau du héros et du premier compagnon.", [] { return (double)rules().startLevel; },
                            [setDepart](double v) { setDepart("niveau", (int)v); }, 1, 1, 100));
  m.items.push_back(numItem("Or de départ", "", [] { return (double)rules().startGold; }, [setDepart](double v) { setDepart("or", (int)v); }, 10, 0,
                            99999));
  {
    std::vector<std::string> heroes;
    for (auto& s : allSpecies())
      if (s.human && !s.limit.empty()) heroes.push_back(s.id);
    auto idx = [heroes] {
      auto it = std::find(heroes.begin(), heroes.end(), rules().hero);
      return it == heroes.end() ? 0 : int(it - heroes.begin());
    };
    m.items.push_back(cycleItem("Héros", "Personnage principal de la nouvelle partie.", idx,
                                [heroes, setDepart](int i) { setDepart("heros", heroes[i]); }, (int)heroes.size(),
                                [heroes](int i) { return species(heroes[i]).name; }));
  }
  m.items.push_back(cycleItem("Carte de départ", "Pensez à régler la position : le joueur doit tomber sur une case praticable.",
                              [] { return std::max(0, mapIndex(rules().startMap)); },
                              [setDepart](int i) { setDepart("carte", maps()[i].id); }, (int)maps().size(), [](int i) { return maps()[i].name; }));
  m.items.push_back(numItem("Départ : colonne (x)", "", [] { return (double)rules().startX; }, [setDepart](double v) { setDepart("x", (int)v); }, 1, 0,
                            200));
  m.items.push_back(numItem("Départ : ligne (y)", "", [] { return (double)rules().startY; }, [setDepart](double v) { setDepart("y", (int)v); }, 1, 0,
                            200));
  m.items.push_back({"Objets de départ", ">", "", true, [this] { menuStartItems(); }});
  m.items.push_back({"Butin des combats", ">", "Objets trouvés après un combat (un seul au plus).", true, [this] { menuDrops(); }});
  {
    int row = (int)m.items.size();
    m.items.push_back({"Tactiques de départ", ">", "Règles de combat données à chaque nouveau membre (sauf espèce qui a les siennes).", true,
                       [this, row] {
                         tactics_ = rules().tactics;
                         TacticsTarget t;
                         t.list = &tactics_;
                         t.title = "Tactiques de départ";
                         t.slots = std::max(rules().tacticMax, (int)tactics_.size());
                         t.changed = [this] {
                           change(DF_RULES, [this](Json& d) {
                             Json list = Json::array();
                             for (auto& t : tactics_) list.push_back(tacticToJson(t));
                             d["tactiques"]["defaut"] = list;
                           });
                         };
                         t.closed = [this, row] { menuRules(row); };
                         openTacticsEditor(G, t);
                       }});
  }
  static const std::pair<const char*, const char*> GROUPS[] = {{"equipe", "Équipe"},   {"rencontres", "Rencontres"},   {"combat", "Combat"},
                                                               {"etats", "États"},     {"recompenses", "Récompenses"}, {"capture", "Capture"},
                                                               {"formules", "Formules des statistiques"}, {"tactiques", "Tactiques"}};
  std::string group;
  for (auto& f : ruleFields(editRules())) {
    if (group != f.group) {
      group = f.group;
      for (auto& [g, t] : GROUPS)
        if (group == g) m.items.push_back(menuHeader(t));
    }
    std::string gk = f.group, key = f.key;
    double* dp = f.d;
    int* ip = f.i;
    auto get = [dp, ip] { return dp ? *dp : (double)*ip; };
    auto set = [this, gk, key, ip](double v) {
      change(DF_RULES, [gk, key, ip, v](Json& d) {
        if (ip) d[gk][key] = (int)std::lround(v);
        else d[gk][key] = v;
      });
    };
    m.items.push_back(numItem(f.label, std::string("regles.json : ") + f.group + " > " + f.key, get, set, f.step, f.min, f.max));
  }
  m.onCancel = [this] { menuMain(1); };
  G.menus.push(m);
}

void Settings::menuStartItems(int sel) {
  Menu m;
  m.title = "Objets de départ";
  m.x = 24, m.y = 30, m.w = 200, m.rows = 12;
  m.sel = sel;
  for (auto& d : allItems()) {
    std::string id = d.id;
    m.items.push_back(numItem(d.name, d.desc,
                              [id] {
                                for (auto& [k, n] : rules().startItems)
                                  if (k == id) return (double)n;
                                return 0.0;
                              },
                              [this, id](double v) {
                                change(DF_RULES, [id, v](Json& d) {
                                  Json& o = d["depart"]["objets"];
                                  if (v <= 0) o.erase(id);
                                  else o[id] = (int)v;
                                });
                              },
                              1, 0, 99));
  }
  G.menus.push(m);
}

void Settings::menuDrops(int sel) {
  Menu m;
  m.title = "Butin des combats";
  m.x = 24, m.y = 30, m.w = 200, m.rows = 12;
  m.sel = sel;
  for (auto& d : allItems()) {
    std::string id = d.id;
    m.items.push_back(numItem(d.name, "Chance de trouver cet objet après un combat contre des créatures sauvages.",
                              [id] {
                                for (auto& [k, c] : rules().drops)
                                  if (k == id) return c;
                                return 0.0;
                              },
                              [this, id](double v) {
                                change(DF_RULES, [id, v](Json& d) {
                                  Json& b = d["recompenses"]["butin"];
                                  if (!b.is_array()) b = Json::array();
                                  for (size_t i = 0; i < b.size(); i++)
                                    if (jget<std::string>(b[i], "objet", "") == id) {
                                      if (v <= 0) b.erase(b.begin() + i);
                                      else b[i]["chance"] = v;
                                      return;
                                    }
                                  if (v > 0) b.push_back({{"objet", id}, {"chance", v}});
                                });
                              },
                              .01, 0, 1));
  }
  G.menus.push(m);
}

// ---------------------------------------------------------------------------
// Espèces
// ---------------------------------------------------------------------------
static void speciesGroups(Menu& m, const std::string& sel, const std::function<MenuItem(const Species&)>& make) {
  auto group = [&](const char* title, auto keep) {
    bool any = false;
    for (auto& s : allSpecies()) {
      if (!keep(s)) continue;
      if (!any) m.items.push_back(menuHeader(title));
      any = true;
      if (s.id == sel) m.sel = (int)m.items.size();
      m.items.push_back(make(s));
    }
  };
  group("Héros", [](const Species& s) { return s.human && !s.limit.empty(); });
  group("Créatures", [](const Species& s) { return !s.human && !s.limit.empty(); });
  group("Boss", [](const Species& s) { return !s.human && s.limit.empty(); });
  group("Adversaires", [](const Species& s) { return s.human && s.limit.empty(); });
}

void Settings::menuSpeciesList(const std::string& sel) {
  G.menus.clear();
  grid_ = false;
  panel_ = Panel::Species;
  if (!sel.empty()) panelId_ = sel;
  Menu m;
  m.title = "Espèces";
  m.x = 4, m.y = 18, m.w = 152, m.rows = 14;
  speciesGroups(m, sel, [this](const Species& s) {
    std::string id = s.id;
    return MenuItem{s.name, "", typesName(s) + (s.role.empty() ? "" : " · " + s.role), true, [this, id] { editSpecies(id); },
                    [this, id] {
                      panel_ = Panel::Species;
                      panelId_ = id;
                    }};
  });
  m.onCancel = [this] { menuMain(2); };
  G.menus.push(m);
}

// Modifie l'espèce id dans especes.json
#define EDIT_SPECIES(id, ...)         \
  change(DF_SPECIES, [=](Json& d) {   \
    Json& o = byId(d, id);            \
    __VA_ARGS__;                      \
    reorderKeys(o, SPECIES_KEYS);     \
  })

void Settings::editSpecies(const std::string& id, int sel) {
  G.menus.clear();
  grid_ = false;
  panel_ = Panel::Species;
  panelId_ = id;
  Menu m;
  m.title = species(id).name;
  m.x = 4, m.y = 18, m.w = 152, m.rows = 14;
  m.sel = sel;
  m.items.push_back(textItem("Nom", "Nom de l'espèce", [id] { return species(id).name; },
                             [this, id](const std::string& v) {
                               if (!v.empty()) EDIT_SPECIES(id, o["nom"] = v);
                             },
                             16));
  m.items.push_back(menuHeader("Types"));
  auto setTypes = [this, id](int t1, int t2) {
    std::string a = types()[t1].id, b = t2 >= 0 ? types()[t2].id : "";
    EDIT_SPECIES(id, {
      o.erase("type");
      o.erase("types");
      if (b.empty() || b == a) o["type"] = a;
      else o["types"] = {a, b};
    });
  };
  int nt = (int)types().size();
  m.items.push_back(cycleItem("Type", "", [id] { return (int)species(id).type; }, [id, setTypes](int t) { setTypes(t, species(id).type2); }, nt,
                              [](int t) { return std::string(typeName(t)); }));
  m.items.push_back(cycleItem("Second type", "", [id] { return species(id).type2; }, [id, setTypes](int t) { setTypes(species(id).type, t); }, nt,
                              [](int t) { return t < 0 ? std::string("aucun") : std::string(typeName(t)); }, -1));
  m.items.push_back(menuHeader("Statistiques de base"));
  for (int b = 0; b < N_BASE; b++) {
    std::string key = BASE_KEYS[b];
    m.items.push_back(numItem(BASE_NAMES[b], "Multipliée par le niveau (formules des règles). Voir la fiche à droite.",
                              [id, b] { return (double)species(id).base[b]; },
                              [this, id, key](double v) { EDIT_SPECIES(id, o["base"][key] = (int)v); }, 1, 1, 255));
  }
  struct Sec {
    const char* label;
    const char* key;
    int Species::*field;
    int def;
  };
  for (auto& s : {Sec{"Précision %", "precision", &Species::acc, rules().accBase}, Sec{"Esquive %", "esquive", &Species::eva, rules().evaBase},
                  Sec{"Critique %", "critique", &Species::crit, rules().critBase}}) {
    std::string key = s.key;
    int def = s.def;
    auto field = s.field;
    m.items.push_back(numItem(s.label, "Ne dépend pas du niveau. Valeur par défaut des règles : " + std::to_string(def) + ".",
                              [id, field] { return (double)(species(id).*field); },
                              [this, id, key, def](double v) {
                                EDIT_SPECIES(id, {
                                  if ((int)v == def) o.erase(key);
                                  else o[key] = (int)v;
                                });
                              },
                              1, 0, 200));
  }
  m.items.push_back(menuHeader("Faiblesses"));
  m.items.push_back({"Résistances", ">", "Multiplicateurs de dégâts propres à l'espèce (par type, coups physiques, magie).", true,
                     [this, id] { menuResist(id); }});
  m.items.push_back({"Immunités", ">", "États qui n'ont aucun effet sur l'espèce.", true, [this, id] { menuImmune(id); }});
  m.items.push_back(menuHeader("Techniques"));
  m.items.push_back({"Techniques apprises", ">", "Niveau d'apprentissage de chaque technique.", true, [this, id] { menuLearn(id); }});
  {
    MenuItem lim{"Limite", "", "Technique ultime (jauge Limite). Entrée : choisir.", true};
    lim.rightFn = [id] { return species(id).limit.empty() ? std::string("aucune") : utf8Prefix(moveInfo(species(id).limit).name, 12); };
    lim.act = [this, id] {
      pickMove("Limite de " + species(id).name, species(id).limit,
               [this, id](const std::string& mv) {
                 EDIT_SPECIES(id, {
                   if (mv.empty()) o.erase("limite");
                   else o["limite"] = mv;
                 });
                 editSpecies(id, 20);
               },
               [this, id] { editSpecies(id, 20); });
    };
    m.items.push_back(lim);
  }
  m.items.push_back(menuHeader("Fiche"));
  m.items.push_back(numItem("Niveau de la fiche", "Niveau utilisé pour la fiche de droite (seulement l'aperçu).",
                            [this] { return (double)previewLvl_; }, [this](double v) { previewLvl_ = (int)v; }, 1, 1, 100));
  m.onCancel = [this, id] { menuSpeciesList(id); };
  G.menus.push(m);
}

void Settings::menuResist(const std::string& id, int sel) {
  Menu m;
  m.title = "Résistances";
  m.x = 8, m.y = 30, m.w = 144, m.rows = 13;  // la fiche de droite reste visible
  m.sel = sel;
  auto add = [&](const std::string& key, const std::string& label) {
    m.items.push_back(numItem(label, "Multiplicateur des dégâts reçus : 0 = immunisé, 0,5 = résiste, 2 = craint. 1 = normal.",
                              [id, key] {
                                auto& r = species(id).resist;
                                auto it = r.find(key);
                                return it == r.end() ? 1.0 : (double)it->second;
                              },
                              [this, id, key](double v) {
                                EDIT_SPECIES(id, {
                                  Json& r = o["resistances"];
                                  if (std::fabs(v - 1) < 1e-6) r.erase(key);
                                  else r[key] = v;
                                  if (r.empty()) o.erase("resistances");
                                });
                              },
                              .25, 0, 4));
  };
  add("physique", "Physique");
  add("magique", "Magie");
  m.items.push_back(menuHeader("Par type"));
  for (auto& t : types()) add(t.id, t.name);
  G.menus.push(m);
}

void Settings::menuImmune(const std::string& id, int sel) {
  Menu m;
  m.title = "Immunités";
  m.x = 8, m.y = 30, m.w = 144, m.rows = 4;
  m.sel = sel;
  for (Status st : STATUSES) {
    std::string sid = statusId(st);
    MenuItem it = toggleItem(statusName(st), "Oui : cet état n'a aucun effet. (type) : immunité donnée par un des types de l'espèce.",
                             [id, sid] {
                               auto& v = species(id).immune;
                               return std::find(v.begin(), v.end(), sid) != v.end();
                             },
                             [this, id, sid](bool on) {
                               EDIT_SPECIES(id, {
                                 Json arr = o.value("immunites", Json::array());
                                 Json out = Json::array();
                                 for (auto& x : arr)
                                   if (x.get<std::string>() != sid) out.push_back(x);
                                 if (on) out.push_back(sid);
                                 if (out.empty()) o.erase("immunites");
                                 else o["immunites"] = out;
                               });
                             });
    it.rightFn = [id, sid, st] {
      auto& v = species(id).immune;
      bool own = std::find(v.begin(), v.end(), sid) != v.end();
      return std::string(own ? "Oui" : immuneTo(species(id), st) ? "(type)" : "Non");
    };
    m.items.push_back(it);
  }
  G.menus.push(m);
}

void Settings::menuLearn(const std::string& id, int sel) {
  G.menus.clear();
  grid_ = false;
  panel_ = Panel::Species;
  panelId_ = id;
  // Trie la liste par niveau (l'ordre compte : on garde les 4 dernières techniques)
  auto sortLearn = [this, id] {
    EDIT_SPECIES(id, {
      std::vector<std::pair<int, std::string>> v;
      for (auto& l : o["apprend"]) v.push_back({l.at(0).get<int>(), l.at(1).get<std::string>()});
      std::stable_sort(v.begin(), v.end(), [](auto& a, auto& b) { return a.first < b.first; });
      Json arr = Json::array();
      for (auto& [lv, mv] : v) arr.push_back({lv, mv});
      o["apprend"] = arr;
    });
  };
  Menu m;
  m.title = "Techniques apprises";  // l'espèce est sur la fiche de droite
  m.x = 4, m.y = 18, m.w = 152, m.rows = 14;
  m.sel = sel;
  const auto& learn = species(id).learn;
  for (size_t k = 0; k < learn.size(); k++) {
    std::string mv = learn[k].move;
    MenuItem it{utf8Prefix(moveInfo(mv).name, 13), "", "Gauche/droite : niveau. Entrée : changer ou supprimer.", true};
    it.rightFn = [id, k] {
      auto& l = species(id).learn;
      return k < l.size() ? "< " + std::to_string(l[k].lvl) + " >" : std::string();
    };
    it.adjust = [this, id, k](int d) {
      EDIT_SPECIES(id, {
        Json& l = o["apprend"][k];
        l[0] = std::clamp(l[0].get<int>() + d, 1, 100);
      });
    };
    it.act = [this, id, k, mv] {
      Menu s;
      s.title = moveInfo(mv).name;
      s.x = 60, s.y = 60, s.w = 170, s.rows = 3;
      s.items.push_back({"Changer de technique", "", "", true, [this, id, k, mv] {
                           pickMove("Remplacer " + moveInfo(mv).name, mv,
                                    [this, id, k](const std::string& nmv) {
                                      if (!nmv.empty()) EDIT_SPECIES(id, o["apprend"][k][1] = nmv);
                                      menuLearn(id, (int)k);
                                    },
                                    [this, id, k] { menuLearn(id, (int)k); });
                         }});
      s.items.push_back({"Supprimer", "", "", true, [this, id, k] {
                           EDIT_SPECIES(id, o["apprend"].erase(o["apprend"].begin() + k));
                           menuLearn(id, 0);
                         }});
      s.items.push_back({"Retour", "", "", true, [this] { G.menus.pop(); }});
      G.menus.push(s);
    };
    it.hover = [this, mv] {
      panel_ = Panel::Move;
      panelId_ = mv;
    };
    m.items.push_back(it);
  }
  m.items.push_back({"Ajouter une technique", "", "", true, [this, id, sortLearn] {
                       pickMove("Nouvelle technique", "",
                                [this, id, sortLearn](const std::string& mv) {
                                  if (!mv.empty()) EDIT_SPECIES(id, o["apprend"].push_back({1, mv}));
                                  sortLearn();
                                  menuLearn(id, 0);
                                },
                                [this, id] { menuLearn(id, 0); });
                     },
                     [this, id] {
                       panel_ = Panel::Species;
                       panelId_ = id;
                     }});
  m.onCancel = [this, id, sortLearn] {
    sortLearn();
    editSpecies(id, 18);
  };
  G.menus.push(m);
}

void Settings::pickMove(const std::string& title, const std::string& current, std::function<void(const std::string&)> done,
                        std::function<void()> back, bool allowNone) {
  G.menus.clear();
  grid_ = false;
  Menu m;
  m.title = title;
  m.x = 4, m.y = 18, m.w = 152, m.rows = 14;
  if (allowNone) m.items.push_back({"(aucune)", "", "", true, [done] { done(""); }});
  auto group = [&](const char* name, auto keep) {
    bool any = false;
    for (auto& mv : allMoves()) {
      if (!keep(mv)) continue;
      if (!any) m.items.push_back(menuHeader(name));
      any = true;
      std::string id = mv.id;
      if (id == current) m.sel = (int)m.items.size();
      m.items.push_back({utf8Prefix(mv.name, 18), "", moveDetails(mv), true, [done, id] { done(id); }, [this, id] {
                           panel_ = Panel::Move;
                           panelId_ = id;
                         }});
    }
  };
  group("Techniques", [](const Move& m) { return m.cost == 0 && m.id.rfind("lim_", 0) != 0; });
  group("Sorts", [](const Move& m) { return m.cost > 0; });
  group("Limites", [](const Move& m) { return m.id.rfind("lim_", 0) == 0; });
  m.onCancel = back;
  G.menus.push(m);
}

// ---------------------------------------------------------------------------
// Techniques
// ---------------------------------------------------------------------------
void Settings::menuMovesList(const std::string& sel) {
  pickMove("Techniques", sel, [this](const std::string& id) { editMove(id); }, [this] { menuMain(3); }, false);
}

#define EDIT_MOVE(id, ...)          \
  change(DF_MOVES, [=](Json& d) {   \
    Json& o = byId(d, id);          \
    __VA_ARGS__;                    \
    reorderKeys(o, MOVE_KEYS);      \
  })

void Settings::editMove(const std::string& id, int sel) {
  G.menus.clear();
  grid_ = false;
  panel_ = Panel::Move;
  panelId_ = id;
  Menu m;
  m.title = utf8Prefix(moveInfo(id).name, 20);
  m.x = 4, m.y = 18, m.w = 152, m.rows = 14;
  m.sel = sel;
  m.items.push_back(textItem("Nom", "Nom de la technique", [id] { return moveInfo(id).name; },
                             [this, id](const std::string& v) {
                               if (!v.empty()) EDIT_MOVE(id, o["nom"] = v);
                             },
                             20));
  m.items.push_back(cycleItem("Type", "", [id] { return (int)moveInfo(id).type; },
                              [this, id](int t) {
                                std::string ty = types()[t].id;
                                EDIT_MOVE(id, o["type"] = ty);
                              },
                              (int)types().size(), [](int t) { return std::string(typeName(t)); }));
  m.items.push_back(cycleItem("Genre", "Physique : Attaque contre Défense. Magique : Magie contre Résistance. Statut : seulement l'effet.",
                              [id] { return (int)moveInfo(id).kind; },
                              [this, id](int k) {
                                std::string v = KIND_IDS[k];
                                EDIT_MOVE(id, o["genre"] = v);
                              },
                              5, [](int k) { return std::string(KIND_NAMES[k]); }));
  m.items.push_back(cycleItem("Cible", "", [id] { return (int)moveInfo(id).target; },
                              [this, id](int k) {
                                std::string v = TARGET_IDS[k];
                                EDIT_MOVE(id, o["cible"] = v);
                              },
                              5, [](int k) { return std::string(TARGET_NAMES[k]); }));
  m.items.push_back(numItem("Puissance", "Dégâts ou soins de base.", [id] { return (double)moveInfo(id).power; },
                            [this, id](double v) { EDIT_MOVE(id, o["puissance"] = (int)v); }, 5, 0, 250));
  m.items.push_back(numItem("Coût en PM", "0 : technique gratuite. Plus de 0 : sort (menu Magie).", [id] { return (double)moveInfo(id).cost; },
                            [this, id](double v) {
                              EDIT_MOVE(id, {
                                if (v <= 0) o.erase("cout");
                                else o["cout"] = (int)v;
                              });
                            },
                            1, 0, 99));
  m.items.push_back(numItem("Précision %", "", [id] { return (double)moveInfo(id).acc; },
                            [this, id](double v) {
                              EDIT_MOVE(id, {
                                if (v >= 100) o.erase("precision");
                                else o["precision"] = (int)v;
                              });
                            },
                            5, 5, 100));
  m.items.push_back(numItem("Critique + %", "Chance de coup critique en plus de celle du lanceur.", [id] { return (double)moveInfo(id).critBonus; },
                            [this, id](double v) {
                              EDIT_MOVE(id, {
                                if (v <= 0) o.erase("critique");
                                else o["critique"] = (int)v;
                              });
                            },
                            5, 0, 100));
  m.items.push_back(cycleItem("Rythme",
                              "Rapide : la jauge du lanceur repart à " + std::to_string(rules().quickGauge) +
                                  " %. Lourde : elle repart à -" + std::to_string(rules().heavyDelay) + " % (règles, combat).",
                              [id] { return (int)moveInfo(id).pace; },
                              [this, id](int k) {
                                EDIT_MOVE(id, {
                                  if (k == 0) o.erase("rythme");
                                  else o["rythme"] = paceId(Pace(k));
                                });
                              },
                              3, [](int k) { return std::string(paceName(Pace(k))); }));
  {
    MenuItem e{"Effet", "", "État, bonus/malus ou guérison.", true, [this, id] { menuEffect(id); }};
    e.rightFn = [id] {
      auto& ef = moveInfo(id).effects;
      if (ef.empty()) return std::string("aucun >");
      const Effect& x = ef[0];
      if (x.status != Status::None) return std::string(statusName(x.status)) + " >";
      if (x.stat >= 0) return std::string(stageName(x.stat)) + " >";
      return std::string("Guérison >");
    };
    m.items.push_back(e);
  }
  m.items.push_back(textItem("Description", "Description (affichée dans les menus)", [id] { return moveInfo(id).desc; },
                             [this, id](const std::string& v) {
                               EDIT_MOVE(id, {
                                 if (v.empty()) o.erase("description");
                                 else o["description"] = v;
                               });
                             },
                             90));
  m.onCancel = [this, id] { menuMovesList(id); };
  G.menus.push(m);
}

void Settings::menuEffect(const std::string& id, int sel) {
  G.menus.clear();
  grid_ = false;
  panel_ = Panel::Move;
  panelId_ = id;
  // Une seule technique d'effet est réglable ici : « effets » (liste) devient « effet »
  auto setEffect = [this, id](Json e) {
    EDIT_MOVE(id, {
      o.erase("effets");
      if (e.is_null()) o.erase("effet");
      else o["effet"] = e;
    });
  };
  auto cur = [id] { return moveInfo(id).effects.empty() ? Effect{} : moveInfo(id).effects[0]; };
  auto toJson = [](const Effect& e) {
    Json o = Json::object();
    if (e.status != Status::None) o["statut"] = statusId(e.status);
    if (e.stat >= 0) o["stat"] = STAGE_IDS[e.stat], o["niveaux"] = e.stages;
    if (e.cure) o["guerison"] = true;
    if (e.self) o["sur"] = "lanceur";
    if (e.chance < 100) o["chance"] = e.chance;
    return o;
  };
  auto kindOf = [](const Effect& e) { return e.status != Status::None ? 1 : e.stat >= 0 ? 2 : e.cure ? 3 : 0; };
  bool none = moveInfo(id).effects.empty();
  Effect e = cur();
  int kind = none ? 0 : kindOf(e);
  Menu m;
  m.title = "Effet de " + utf8Prefix(moveInfo(id).name, 14);
  m.x = 4, m.y = 18, m.w = 152, m.rows = 10;
  m.sel = sel;
  static const char* KINDS[] = {"aucun", "État", "Bonus/malus", "Guérison"};
  m.items.push_back(cycleItem("Genre d'effet", "", [kind] { return kind; },
                              [this, id, setEffect](int k) {
                                if (k == 0) setEffect(Json());
                                if (k == 1) setEffect({{"statut", "poison"}, {"chance", 30}});
                                if (k == 2) setEffect({{"stat", "attaque"}, {"niveaux", 1}});
                                if (k == 3) setEffect({{"guerison", true}});
                                menuEffect(id, 0);
                              },
                              4, [](int k) { return std::string(KINDS[k]); }));
  if (kind == 1)
    m.items.push_back(cycleItem("État", "", [cur] { return std::max(0, (int)cur().status - 1); },
                                [cur, toJson, setEffect](int k) {
                                  Effect x = cur();
                                  x.status = STATUSES[k];
                                  setEffect(toJson(x));
                                },
                                4, [](int k) { return std::string(statusName(STATUSES[k])); }));
  if (kind == 2) {
    m.items.push_back(cycleItem("Statistique", "", [cur] { return std::max(0, cur().stat); },
                                [cur, toJson, setEffect](int k) {
                                  Effect x = cur();
                                  x.stat = k;
                                  setEffect(toJson(x));
                                },
                                N_STAGES, [](int k) { return std::string(stageName(k)); }));
    m.items.push_back(numItem("Niveaux", "Positif : bonus. Négatif : malus.", [cur] { return (double)cur().stages; },
                              [cur, toJson, setEffect](double v) {
                                Effect x = cur();
                                int n = (int)v;
                                x.stages = n == 0 ? (x.stages > 0 ? -1 : 1) : n;
                                setEffect(toJson(x));
                              },
                              1, -3, 3));
    m.items.push_back(toggleItem("Sur le lanceur", "Oui : le bonus ou malus touche celui qui utilise la technique.", [cur] { return cur().self; },
                                 [cur, toJson, setEffect](bool b) {
                                   Effect x = cur();
                                   x.self = b;
                                   setEffect(toJson(x));
                                 }));
  }
  if (kind == 1 || kind == 2)
    m.items.push_back(numItem("Chance %", "", [cur] { return (double)cur().chance; },
                              [cur, toJson, setEffect](double v) {
                                Effect x = cur();
                                x.chance = (int)v;
                                setEffect(toJson(x));
                              },
                              5, 5, 100));
  m.items.push_back({"Retour", "", "", true, [this, id] { editMove(id, 8); }});
  m.onCancel = m.items.back().act;
  G.menus.push(m);
}

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------
void Settings::menuTypes(int sel) {
  G.menus.clear();
  grid_ = false;
  grid_ = false;
  panel_ = Panel::None;
  Menu m;
  m.title = "Types";
  m.x = 4, m.y = 18, m.w = 152, m.rows = 14;
  m.sel = sel;
  m.items.push_back({"Table d'efficacité", "", "Grille attaque/défense : flèches pour se déplacer, Entrée pour changer.", true, [this] {
                       G.menus.clear();
                       grid_ = true;
                     }});
  m.items.push_back(menuHeader("Immunités et noms"));
  for (int t = 0; t < (int)types().size(); t++)
    m.items.push_back({types()[t].name, ">", "", true, [this, t] { editType(t); }, [this, t] {
                         panel_ = Panel::Type;
                         panelType_ = t;
                       }});
  m.onCancel = [this] { menuMain(4); };
  G.menus.push(m);
}

void Settings::editType(int t, int sel) {
  G.menus.clear();
  grid_ = false;
  panel_ = Panel::Type;
  panelType_ = t;
  Menu m;
  m.title = types()[t].name;
  m.x = 4, m.y = 18, m.w = 152, m.rows = 8;
  m.sel = sel;
  m.items.push_back(textItem("Nom", "Nom du type", [t] { return types()[t].name; },
                             [this, t](const std::string& v) {
                               if (!v.empty()) change(DF_TYPES, [t, v](Json& d) { d["types"][t]["nom"] = v; });
                             },
                             12));
  m.items.push_back(menuHeader("Insensible à"));
  for (Status st : STATUSES) {
    std::string sid = statusId(st);
    m.items.push_back(toggleItem(statusName(st), "Toutes les espèces de ce type sont insensibles à cet état.",
                                 [t, sid] {
                                   auto& v = types()[t].immune;
                                   return std::find(v.begin(), v.end(), sid) != v.end();
                                 },
                                 [this, t, sid](bool on) {
                                   change(DF_TYPES, [t, sid, on](Json& d) {
                                     Json& o = d["types"][t];
                                     Json out = Json::array();
                                     for (auto& x : o.value("immunites", Json::array()))
                                       if (x.get<std::string>() != sid) out.push_back(x);
                                     if (on) out.push_back(sid);
                                     if (out.empty()) o.erase("immunites");
                                     else o["immunites"] = out;
                                   });
                                 }));
  }
  m.onCancel = [this, t] { menuTypes(2 + t); };
  G.menus.push(m);
}

// ---------------------------------------------------------------------------
// Objets
// ---------------------------------------------------------------------------
void Settings::menuItemsList(const std::string& sel) {
  G.menus.clear();
  grid_ = false;
  panel_ = Panel::Item;
  Menu m;
  m.title = "Objets";
  m.x = 4, m.y = 18, m.w = 152, m.rows = 14;
  for (auto& d : allItems()) {
    std::string id = d.id;
    if (id == sel) m.sel = (int)m.items.size();
    m.items.push_back({d.name, "", d.desc, true, [this, id] { editItem(id); }, [this, id] {
                         panel_ = Panel::Item;
                         panelId_ = id;
                       }});
  }
  m.onCancel = [this] { menuMain(5); };
  G.menus.push(m);
}

#define EDIT_ITEM(id, ...)          \
  change(DF_ITEMS, [=](Json& d) {   \
    Json& o = byId(d, id);          \
    __VA_ARGS__;                    \
    reorderKeys(o, ITEM_KEYS);      \
  })

void Settings::editItem(const std::string& id, int sel) {
  G.menus.clear();
  grid_ = false;
  panel_ = Panel::Item;
  panelId_ = id;
  Menu m;
  m.title = item(id).name;
  m.x = 4, m.y = 18, m.w = 152, m.rows = 14;
  m.sel = sel;
  m.items.push_back(textItem("Nom", "Nom de l'objet", [id] { return item(id).name; },
                             [this, id](const std::string& v) {
                               if (!v.empty()) EDIT_ITEM(id, o["nom"] = v);
                             },
                             18));
  m.items.push_back(textItem("Description", "Description de l'objet", [id] { return item(id).desc; },
                             [this, id](const std::string& v) { EDIT_ITEM(id, o["description"] = v); }, 90));
  m.items.push_back(numItem("Prix", "Prix en boutique (0 : objet introuvable en boutique).", [id] { return (double)item(id).price; },
                            [this, id](double v) { EDIT_ITEM(id, o["prix"] = (int)v); }, 5, 0, 99999));
  auto num = [&](const char* label, const char* key, const char* help, auto getter, double step, double mx, bool asInt) {
    std::string k = key;
    m.items.push_back(numItem(label, help, [id, getter] { return (double)getter(item(id)); },
                              [this, id, k, asInt](double v) {
                                EDIT_ITEM(id, {
                                  if (v <= 0) o.erase(k);
                                  else if (asInt) o[k] = (int)v;
                                  else o[k] = v;
                                });
                              },
                              step, 0, mx));
  };
  m.items.push_back(menuHeader("Effets"));
  num("Soin PV", "soin_pv", "PV rendus à un allié.", [](const ItemDef& d) { return d.healHp; }, 10, 9999, true);
  num("Soin PM", "soin_pm", "PM rendus à un allié.", [](const ItemDef& d) { return d.healMp; }, 5, 999, true);
  num("Réanimation %", "rappel", "Relève un allié K.O. avec ce pourcentage de ses PV.", [](const ItemDef& d) { return d.revive; }, 5, 100, true);
  num("Capture ×", "capture", "Lanterne : multiplicateur de la chance de capture (0 : pas une lanterne).",
      [](const ItemDef& d) { return d.capture; }, .1, 5, false);
  auto flag = [&](const char* label, const char* key, const char* help, bool ItemDef::*field) {
    std::string k = key;
    m.items.push_back(toggleItem(label, help, [id, field] { return item(id).*field; },
                                 [this, id, k](bool b) {
                                   EDIT_ITEM(id, {
                                     if (b) o[k] = true;
                                     else o.erase(k);
                                   });
                                 }));
  };
  flag("Soigne les états", "soin_statut", "Guérit poison, brûlure, paralysie et sommeil.", &ItemDef::cure);
  m.items.push_back(menuHeader("Utilisation"));
  flag("En combat", "combat", "", &ItemDef::battle);
  flag("Dans le menu", "menu", "", &ItemDef::field);
  flag("Objet important", "important", "Objet de quête : ne s'utilise pas.", &ItemDef::key);
  m.onCancel = [this, id] { menuItemsList(id); };
  G.menus.push(m);
}

// ---------------------------------------------------------------------------
// Mise à jour et dessin
// ---------------------------------------------------------------------------
static const float CYCLE[] = {1.f, 2.f, .5f, 0.f};

void Settings::update(float) {
  if (G.editingText()) return;
  if (grid_) {
    int n = (int)types().size();
    Input& in = G.in;
    if (in.press[LEFT]) gx_ = (gx_ + n - 1) % n;
    if (in.press[RIGHT]) gx_ = (gx_ + 1) % n;
    if (in.press[UP]) gy_ = (gy_ + n - 1) % n;
    if (in.press[DOWN]) gy_ = (gy_ + 1) % n;
    if (in.confirm) {
      float cur = typeEff(gy_, gx_);
      int k = 0;
      for (int i = 0; i < 4; i++)
        if (std::fabs(CYCLE[i] - cur) < 1e-4f) k = i;
      double next = CYCLE[(k + 1) % 4];
      std::string a = types()[gy_].id, d = types()[gx_].id;
      change(DF_TYPES, [a, d, next](Json& doc) {
        Json& t = doc["efficacite"];
        if (std::fabs(next - 1) < 1e-6) {
          if (t.contains(a)) t[a].erase(d);
          if (t.contains(a) && t[a].empty()) t.erase(a);
        } else t[a][d] = next;
      });
    }
    if (in.cancel) {
      grid_ = false;
      menuTypes(0);
    }
    return;
  }
  if (!G.menus.active()) menuMain();
  G.menus.update(G.in);
}

static std::string effStr(float e) { return e == 0 ? "0" : e == .5f ? ",5" : e == .25f ? ",25" : fmtNum(e, 2); }

void Settings::drawGrid() {
  Gfx& g = G.g;
  int n = (int)types().size();
  int cw = 18, ch = 13, x0 = 56, y0 = 30;
  g.window(4, 16, 312, 194);
  g.text(10, 19, "Attaque en lignes, défense en colonnes", MUTED);
  for (int d = 0; d < n; d++) {
    std::string ab = utf8Prefix(types()[d].name, 2);
    g.text(x0 + d * cw + cw / 2, y0 - 1, ab, d == gx_ ? GOLD : rgb(types()[d].color), 1);
  }
  for (int a = 0; a < n; a++) {
    float y = y0 + 10 + a * ch;
    g.text(10, y, utf8Prefix(types()[a].name, 7), a == gy_ ? GOLD : rgb(types()[a].color));
    for (int d = 0; d < n; d++) {
      float e = typeEff(a, d);
      float x = x0 + d * cw;
      Color bg = e > 1 ? rgb(0x7a2a2a) : e == 0 ? rgb(0x101018) : e < 1 ? rgb(0x2a4a7a) : rgb(0x2a3270);
      g.rect(x + 1, y - 1, cw - 2, ch - 1, bg);
      if (e != 1) g.text(x + cw / 2, y, e > 1 ? fmtNum(e, 2) : effStr(e), WHITE, 1, false);
      if (a == gy_ && d == gx_) g.frame(x, y - 2, cw, ch + 1, GOLD);
    }
  }
  float e = typeEff(gy_, gx_);
  std::string line = std::string(types()[gy_].name) + " attaque " + types()[gx_].name + " : ×" + fmtNum(e, 2) +
                     (e > 1 ? " (super efficace)" : e == 0 ? " (aucun effet)" : e < 1 ? " (peu efficace)" : "");
  g.window(4, 211, 312, 27);
  g.text(10, 214, line, WHITE);
  g.text(10, 225, "Flèches · Entrée : changer · Échap : retour", MUTED);
}

void Settings::drawPanel(int x, int y, int w, int h) {
  Gfx& g = G.g;
  auto para = [&](int& ly, const std::string& t, Color c) {
    for (auto& l : Gfx::wrap(t, w - 16)) {
      if (ly > y + h - 12) return;
      g.text(x + 8, ly, l, c);
      ly += 10;
    }
  };
  if (panel_ == Panel::Species && hasSpecies(panelId_)) return drawFighterCard(g, *makeFighter(panelId_, previewLvl_), x, y, w, h, G.time);
  g.window(x, y, w, h);
  int ly = y + 6;
  if (panel_ == Panel::Move && hasMove(panelId_)) {
    const Move& mv = moveInfo(panelId_);
    g.text(x + 8, ly, mv.name, GOLD), ly += 13;
    para(ly, moveDetails(mv), WHITE);
    ly += 4;
    if (!mv.desc.empty()) para(ly, mv.desc, MUTED), ly += 4;
    std::string who;
    for (auto& s : allSpecies()) {
      for (auto& l : s.learn)
        if (l.move == mv.id) who += (who.empty() ? "" : ", ") + s.name + " " + std::to_string(l.lvl);
      if (s.limit == mv.id) who += (who.empty() ? "" : ", ") + s.name + " (Limite)";
    }
    para(ly, "Appris par : " + (who.empty() ? std::string("personne") : who), MUTED);
    return;
  }
  if (panel_ == Panel::Item && hasItem(panelId_)) {
    const ItemDef& d = item(panelId_);
    g.text(x + 8, ly, d.name, GOLD), ly += 13;
    para(ly, d.desc, WHITE);
    ly += 4;
    std::string fx;
    if (d.healHp) fx += "Soin " + std::to_string(d.healHp) + " PV. ";
    if (d.healMp) fx += "Soin " + std::to_string(d.healMp) + " PM. ";
    if (d.revive) fx += "Réanimation " + std::to_string(d.revive) + " %. ";
    if (d.capture > 0) fx += "Capture ×" + fmtNum(d.capture, 1) + ". ";
    if (d.cure) fx += "Soigne les états. ";
    para(ly, fx.empty() ? "Aucun effet en combat." : fx, GREEN);
    para(ly, "Prix : " + std::to_string(d.price) + " or", MUTED);
    return;
  }
  if (panel_ == Panel::Type && panelType_ < (int)types().size()) {
    int t = panelType_;
    g.text(x + 8, ly, types()[t].name, rgb(types()[t].color)), ly += 13;
    std::string strong, weakAtk, weakDef, resDef;
    for (int o = 0; o < (int)types().size(); o++) {
      if (typeEff(t, o) > 1) strong += (strong.empty() ? "" : ", ") + types()[o].name;
      if (typeEff(t, o) < 1) weakAtk += (weakAtk.empty() ? "" : ", ") + types()[o].name;
      if (typeEff(o, t) > 1) weakDef += (weakDef.empty() ? "" : ", ") + types()[o].name;
      if (typeEff(o, t) < 1) resDef += (resDef.empty() ? "" : ", ") + types()[o].name;
    }
    para(ly, "Efficace contre : " + (strong.empty() ? "—" : strong), GREEN), ly += 3;
    para(ly, "Peu efficace contre : " + (weakAtk.empty() ? "—" : weakAtk), MUTED), ly += 3;
    para(ly, "Craint : " + (weakDef.empty() ? "—" : weakDef), RED), ly += 3;
    para(ly, "Résiste à : " + (resDef.empty() ? "—" : resDef), GREEN);
    return;
  }
  // Résumé
  g.text(x + 8, ly, "Données du jeu", GOLD), ly += 13;
  para(ly, std::to_string(allSpecies().size()) + " espèces, " + std::to_string(allMoves().size()) + " techniques, " +
               std::to_string(types().size()) + " types, " + std::to_string(allItems().size()) + " objets.",
       WHITE);
  ly += 4;
  std::string files;
  for (int f = 0; f < N_DATAFILES; f++)
    if (dirty(DataFile(f))) files += (files.empty() ? "" : ", ") + std::string(dataFileName(DataFile(f)));
  para(ly, files.empty() ? "Aucune modification en attente." : "Modifié : " + files, files.empty() ? MUTED : GOLD);
  ly += 4;
  if (problems_.empty()) para(ly, "Aucun problème détecté.", GREEN);
  else {
    para(ly, std::to_string(problems_.size()) + " problème(s) :", RED);
    for (auto& p : problems_) para(ly, "- " + p, RED);
  }
}

void Settings::draw() {
  Gfx& g = G.g;
  g.gradV(g.left(), 0, g.fullW, SCREEN_H, rgb(0x14203a), rgb(0x2a3a5e));
  g.text(160, 4, "RÉGLAGES", GOLD, 1);
  if (grid_) {
    drawGrid();
    return;
  }
  bool wide = G.menus.maxRight() > 158;  // un menu déborde sur la place du panneau de droite
  if (!wide) drawPanel(160, 18, 156, 190);
  G.menus.draw(g, G.time);
  std::string h = G.time - messageT_ < 3 ? message_ : G.menus.help();
  if (!h.empty()) {
    auto lines = Gfx::wrap(h, 300);
    int hh = 8 + 11 * (int)lines.size();
    g.window(4, 238 - hh, 312, hh);
    for (size_t i = 0; i < lines.size(); i++) g.text(10, 238 - hh + 4 + i * 11, lines[i], G.time - messageT_ < 3 ? GOLD : MUTED);
  }
}
