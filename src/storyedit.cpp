#include "storyedit.hpp"

#include <algorithm>
#include <cmath>
#include <set>

#include "arena.hpp"
#include "data.hpp"
#include "events.hpp"
#include "game.hpp"
#include "mapedit.hpp"
#include "world.hpp"

static const Color WHITE = rgb(0xffffff), GOLD = rgb(0xffd34d), MUTED = rgb(0xaab3d8), RED = rgb(0xff8a7a), GREEN = rgb(0x7dffa8);

static const char* ACTIONS[] = {"dire",     "donner",   "retirer", "or",     "drapeau",    "recruter", "combat",   "question",
                                "si",       "soigner",  "boutique", "reveil", "teleporter", "evenement", "attendre", "fin", "quete"};
static const char* ACTIONS_FR[] = {"Dire un message",       "Donner un objet",    "Retirer un objet", "Gagner ou payer de l'or",
                                   "Poser un drapeau",      "Recruter",           "Combat",           "Question Oui / Non",
                                   "Si (condition)",        "Soigner l'équipe",   "Boutique",         "Point de réveil",
                                   "Aller sur une carte",   "Lancer un événement", "Attendre",        "Écran de fin",
                                   "Quête (début ou fin)"};
static const char* ACTIONS_HELP[] = {
    "Affiche un message. {heros}, {compagnon} et {or} sont remplacés.", "Ajoute un objet au sac (avec un message).",
    "Enlève un objet du sac.", "Quantité positive : gagner ; négative : payer.", "Retient qu'une chose a eu lieu (pour les conditions).",
    "Un personnage ou une créature rejoint l'équipe.", "Lance un combat, avec des suites en cas de victoire ou de défaite.",
    "Pose une question avec une suite pour Oui et une pour Non.", "Joue « alors » si la condition est vraie, sinon « sinon ».",
    "Rend les PV et/ou les PM de toute l'équipe.", "Ouvre une boutique.", "Le joueur se réveillera ici après une défaite.",
    "Emmène le joueur sur une carte.", "Joue un autre événement.", "Une courte pause.", "Affiche l'écran de fin.",
    "Commence une quête de data/quetes.json (elle apparaît dans le journal), ou la termine."};
static const std::set<std::string> LIST_KEYS = {"actions", "oui", "non", "alors", "sinon", "victoire", "defaite"};

// ---------------------------------------------------------------------------
// Résumés lisibles
// ---------------------------------------------------------------------------
static std::string itemName(const std::string& id) { return hasItem(id) ? item(id).name : id; }
static std::string spName(const std::string& id) { return hasSpecies(id) ? species(id).name : id; }
static std::string listStr(const Json& v, std::string (*name)(const std::string&)) {
  std::string s;
  auto add = [&](const std::string& x) { s += (s.empty() ? "" : ", ") + (name ? name(x) : x); };
  if (v.is_string()) add(v.get<std::string>());
  else if (v.is_array())
    for (auto& x : v) add(x.get<std::string>());
  return s;
}
static std::string fmtNum(double v) {
  char b[24];
  if (std::fabs(v - std::round(v)) < 1e-6) std::snprintf(b, sizeof b, "%d", (int)std::lround(v));
  else std::snprintf(b, sizeof b, "%.1f", v);
  std::string s = b;
  for (auto& c : s)
    if (c == '.') c = ',';
  return s;
}

std::string conditionSummary(const Json& c) {
  if (!c.is_object()) return "toujours";
  std::vector<std::string> p;
  if (c.contains("drapeau")) p.push_back("drapeau " + listStr(c["drapeau"], nullptr));
  if (c.contains("sans_drapeau")) p.push_back("sans le drapeau " + listStr(c["sans_drapeau"], nullptr));
  if (c.contains("objet")) p.push_back("avec " + listStr(c["objet"], itemName));
  if (c.contains("sans_objet")) p.push_back("sans " + listStr(c["sans_objet"], itemName));
  if (c.contains("or_min")) p.push_back("au moins " + std::to_string(jget(c, "or_min", 0)) + " or");
  if (c.contains("membre")) p.push_back(listStr(c["membre"], spName) + " dans l'équipe");
  std::string s;
  for (size_t i = 0; i < p.size(); i++) s += (i ? " et " : "") + p[i];
  return s.empty() ? "toujours" : s;
}

std::string actionSummary(const Json& a) {
  std::string k = jget<std::string>(a, "action", "?");
  if (k == "dire") return "Dire : " + jget<std::string>(a, "texte", "");
  if (k == "donner") {
    int q = jget(a, "quantite", 1);
    return "Donner : " + itemName(jget<std::string>(a, "objet", "")) + (q > 1 ? " ×" + std::to_string(q) : "");
  }
  if (k == "retirer") {
    int q = jget(a, "quantite", 0);
    return "Retirer : " + itemName(jget<std::string>(a, "objet", "")) + (q > 0 ? " ×" + std::to_string(q) : " (tout)");
  }
  if (k == "or") {
    int q = jget(a, "quantite", 0);
    return q >= 0 ? "Gagner " + std::to_string(q) + " or" : "Payer " + std::to_string(-q) + " or";
  }
  if (k == "drapeau") return std::string(jget(a, "valeur", true) ? "Poser le drapeau " : "Effacer le drapeau ") + jget<std::string>(a, "nom", "");
  if (k == "recruter") return "Recruter : " + spName(jget<std::string>(a, "espece", "")) + " N." + std::to_string(jget(a, "niveau", 1));
  if (k == "combat") {
    std::string who = jget<std::string>(a, "nom", "");
    Json foes = a.value("ennemis", Json::array());
    for (auto& e : foes)
      if (who.empty() || (jget(e, "boss", false) && !a.contains("nom"))) who = spName(jget<std::string>(e, "espece", ""));
    size_t n = foes.size() + a.value("renforts", Json::array()).size();
    return "Combat : " + who + " (" + std::to_string(n) + " adversaire" + (n > 1 ? "s" : "") + ")";
  }
  if (k == "question") return "Question : " + jget<std::string>(a, "texte", "");
  if (k == "si") return "Si " + conditionSummary(a.value("condition", Json::object()));
  if (k == "soigner") {
    bool pv = jget(a, "pv", true), pm = jget(a, "pm", true);
    return std::string("Soigner l'équipe") + (pv && pm ? "" : pv ? " (PV)" : pm ? " (PM)" : " (rien)");
  }
  if (k == "boutique") return "Boutique : " + std::to_string(a.value("objets", Json::array()).size()) + " objets";
  if (k == "reveil") return "Point de réveil ici";
  if (k == "teleporter") {
    std::string m = jget<std::string>(a, "carte", "");
    int i = mapIndex(m);
    return "Aller à : " + (i >= 0 ? maps()[i].name : m) + " (" + std::to_string(jget(a, "x", 0)) + ", " + std::to_string(jget(a, "y", 0)) + ")";
  }
  if (k == "evenement") return "Lancer l'événement « " + jget<std::string>(a, "id", "") + " »";
  if (k == "attendre") return "Attendre " + fmtNum(jget(a, "secondes", .5)) + " s";
  if (k == "fin") return "Écran de fin" + (jget<std::string>(a, "texte", "").empty() ? std::string() : " : " + jget<std::string>(a, "texte", ""));
  if (k == "quete") {
    std::string id = jget<std::string>(a, "id", "");
    const Json* q = findQuest(id);
    return std::string(jget(a, "fin", false) ? "Terminer" : "Commencer") + " la quête « " + (q ? jget<std::string>(*q, "nom", id) : id) + " »";
  }
  return "Action inconnue : " + k;
}

// ---------------------------------------------------------------------------
// Chemins
// ---------------------------------------------------------------------------
static std::vector<std::string> split(const std::string& ptr) {
  std::vector<std::string> t;
  size_t i = 1;
  while (i <= ptr.size() && !ptr.empty()) {
    size_t j = ptr.find('/', i);
    if (j == std::string::npos) j = ptr.size();
    t.push_back(ptr.substr(i, j - i));
    i = j + 1;
  }
  return t;
}
static std::string join(const std::vector<std::string>& t, size_t n) {
  std::string s;
  for (size_t i = 0; i < n && i < t.size(); i++) s += "/" + t[i];
  return s;
}
static bool isNum(const std::string& s) { return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; }); }
static std::string titleOf(const std::string& ptr) {
  auto t = split(ptr);
  if (t.empty()) return "";
  static const std::pair<const char*, const char*> NAMES[] = {{"actions", "Actions"}, {"oui", "Si oui"}, {"non", "Si non"},
                                                               {"alors", "Alors"},     {"sinon", "Sinon"}, {"victoire", "Victoire"},
                                                               {"defaite", "Défaite"}, {"ennemis", "Ennemis"}, {"renforts", "Renforts"},
                                                               {"condition", "Condition"}, {"si", "Condition"}, {"objets", "Objets en vente"}};
  std::string s = t[0];
  if (t.size() >= 3 && t[1] == "pages") s += ", page " + std::to_string(std::stoi(t[2]) + 1);
  std::string last = t.back();
  for (auto& [k, v] : NAMES)
    if (last == k) return s + " > " + v;
  return s;
}

StoryEditor::StoryEditor(Game& g) : G(g) {}

const Json& StoryEditor::peek(const std::string& ptr) const {
  static const Json none;
  try {
    Json::json_pointer p(ptr);
    return events().contains(p) ? events().at(p) : none;
  } catch (...) {
    return none;
  }
}

void StoryEditor::say(const std::string& m) {
  message_ = m;
  messageT_ = G.time;
}

bool StoryEditor::change(const std::function<void(Json&)>& fn) {
  undo_.push_back(events());
  if (undo_.size() > 100) undo_.erase(undo_.begin());
  redo_.clear();
  try {
    fn(events());
  } catch (const std::exception& e) {
    events() = undo_.back();
    undo_.pop_back();
    say(std::string("Refusé : ") + e.what());
    return false;
  }
  errors_ = checkEvents();
  return true;
}

void StoryEditor::undo() {
  if (undo_.empty()) return say("Rien à annuler.");
  redo_.push_back(events());
  events() = undo_.back();
  undo_.pop_back();
  errors_ = checkEvents();
  say("Annulé.");
  goTo(current_);
}
void StoryEditor::redo() {
  if (redo_.empty()) return say("Rien à rétablir.");
  undo_.push_back(events());
  events() = redo_.back();
  redo_.pop_back();
  errors_ = checkEvents();
  say("Rétabli.");
  goTo(current_);
}

bool StoryEditor::dirty() const { return events() != saved_; }

void StoryEditor::save() {
  try {
    saveEvents();
  } catch (const std::exception& e) {
    return say(std::string("Erreur : ") + e.what());
  }
  saved_ = events();
  say("Enregistré : data/evenements.json" + (errors_.empty() ? "" : " (" + std::to_string(errors_.size()) + " erreur(s) à corriger)"));
}

void StoryEditor::revert() {
  events() = saved_;
  errors_ = checkEvents();
  undo_.clear(), redo_.clear();
  say("Modifications annulées.");
}

void StoryEditor::open() {
  saved_ = events();
  undo_.clear(), redo_.clear();
  errors_ = checkEvents();
  menuEvents();
}

void StoryEditor::quit() {
  if (!dirty()) return G.titleMenu();
  Menu q;
  q.title = "Histoire modifiée, non enregistrée";
  q.x = 50, q.y = 80, q.w = 220, q.rows = 3;
  q.items.push_back({"Enregistrer et quitter", "", "", true, [this] {
                       save();
                       G.titleMenu();
                     }});
  q.items.push_back({"Quitter sans enregistrer", "", "", true, [this] {
                       revert();
                       G.titleMenu();
                     }});
  q.items.push_back({"Annuler", "", "", true, [this] { G.menus.pop(); }});
  G.menus.push(q);
}

std::vector<std::string> StoryEditor::usage(const std::string& id) const {
  std::vector<std::string> u;
  auto pos = [](int x, int y) { return "(" + std::to_string(x) + ", " + std::to_string(y) + ")"; };
  for (auto& m : maps()) {
    for (auto& n : m.npcs)
      if (n.event == id) u.push_back(m.name + " : " + look(n.look).name + " " + pos(n.x, n.y));
    for (auto& b : m.buildings)
      if (b.event == id) u.push_back(m.name + " : porte « " + b.name + " »");
    for (auto& b : m.bosses)
      if (b.event == id) u.push_back(m.name + " : boss " + spName(b.id));
    for (auto& t : m.triggers)
      if (t.event == id) u.push_back(m.name + " : zone déclencheuse " + pos(t.x, t.y));
  }
  if (rules().startEvent == id) u.push_back("Début de la partie");
  std::function<bool(const Json&)> calls = [&](const Json& j) -> bool {
    if (j.is_object() && jget<std::string>(j, "action", "") == "evenement" && jget<std::string>(j, "id", "") == id) return true;
    if (j.is_structured())
      for (auto& v : j)
        if (calls(v)) return true;
    return false;
  };
  for (auto& [eid, ev] : events().items())
    if (eid != id && calls(ev)) u.push_back("Événement « " + eid + " »");
  return u;
}

// Ouvre l'écran correspondant au chemin
void StoryEditor::goTo(const std::string& ptr, int sel) {
  auto t = split(ptr);
  if (t.empty() || !findEvent(t[0])) return menuEvents();
  if (t.size() == 1) return editEvent(t[0], sel);
  std::string last = t.back();
  if (t.size() == 3 && t[1] == "pages") return peek(ptr).is_null() ? editEvent(t[0]) : editPage(t[0], std::stoi(t[2]), sel);
  if (isNum(last)) {
    if (peek(ptr).is_null()) return goUp(ptr);
    std::string key = t[t.size() - 2];
    return key == "ennemis" || key == "renforts" ? editEnemy(ptr, sel) : editAction(ptr, sel);
  }
  if (LIST_KEYS.count(last)) return editList(ptr, sel);
  if (last == "si" || last == "condition") return editCondition(ptr, sel);
  if (last == "ennemis" || last == "renforts") return editEnemies(ptr, sel);
  if (last == "objets") return pickItems(ptr);
  goUp(ptr);
}

// Revient à l'écran du dessus
void StoryEditor::goUp(const std::string& ptr) {
  auto t = split(ptr);
  if (t.size() <= 1) return menuEvents(t.empty() ? "" : t[0]);
  std::string last = t.back(), parent = join(t, t.size() - 1);
  if (t.size() == 3 && t[1] == "pages") return editEvent(t[0], std::stoi(t[2]));
  if (isNum(last)) {
    std::string key = t[t.size() - 2];
    return key == "ennemis" || key == "renforts" ? editEnemies(parent, std::stoi(last)) : editList(parent, std::stoi(last));
  }
  if (t.size() == 2) return editEvent(t[0]);
  if (t.size() == 4 && t[1] == "pages") return editPage(t[0], std::stoi(t[2]), last == "si" ? 0 : 1);
  editAction(parent);
}

// ---------------------------------------------------------------------------
// Petits éléments de menu
// ---------------------------------------------------------------------------
static MenuItem numItem(const std::string& label, const std::string& help, std::function<double()> get, std::function<void(double)> set,
                        double step, double mn, double mx) {
  MenuItem it{label, "", help, true};
  it.rightFn = [get] { return "< " + fmtNum(get()) + " >"; };
  it.adjust = [get, set, step, mn, mx](int d) { set(std::clamp(std::round((get() + d * step) * 1e6) / 1e6, mn, mx)); };
  return it;
}
static MenuItem cycleItem(const std::string& label, const std::string& help, std::function<int()> get, std::function<void(int)> set, int n,
                          std::function<std::string(int)> name) {
  MenuItem it{label, "", help, true};
  it.rightFn = [get, name] { return "< " + name(get()) + " >"; };
  it.adjust = [get, set, n](int d) { set(((get() + d) % n + n) % n); };
  return it;
}

MenuItem StoryEditor::textField(const std::string& ptr, const std::string& key, const std::string& label, const std::string& title, int maxChars,
                                int row) {
  MenuItem it{label, "", "Entrée : écrire le texte.", true};
  it.rightFn = [this, ptr, key] {
    std::string s = jget<std::string>(peek(ptr), key.c_str(), "");
    return s.empty() ? std::string("—") : utf8Prefix(s, 28) + (utf8Prefix(s, 28).size() < s.size() ? "…" : "");
  };
  it.act = [this, ptr, key, title, maxChars, row] {
    std::string cur = jget<std::string>(peek(ptr), key.c_str(), "");
    G.editText(title, cur, maxChars, [this, ptr, key, row](const std::string& v) {
      change([ptr, key, v](Json& ev) {
        Json& o = ev[Json::json_pointer(ptr)];
        if (v.empty()) o.erase(key);
        else o[key] = v;
      });
      goTo(ptr, row);
    });
  };
  return it;
}

// Monter, descendre, dupliquer, supprimer un élément d'une liste
void StoryEditor::organize(std::vector<MenuItem>& items, const std::string& ptr, bool keepOne) {
  auto t = split(ptr);
  std::string list = join(t, t.size() - 1);
  int i = std::stoi(t.back());
  int n = (int)peek(list).size();
  items.push_back(menuHeader("Organiser"));
  items.push_back({"Monter", "", "", i > 0, [this, list, i] {
                     change([list, i](Json& ev) {
                       Json& a = ev.at(Json::json_pointer(list));
                       std::swap(a[i - 1], a[i]);
                     });
                     goTo(list + "/" + std::to_string(i - 1));
                   }});
  items.push_back({"Descendre", "", "", i + 1 < n, [this, list, i] {
                     change([list, i](Json& ev) {
                       Json& a = ev.at(Json::json_pointer(list));
                       std::swap(a[i], a[i + 1]);
                     });
                     goTo(list + "/" + std::to_string(i + 1));
                   }});
  items.push_back({"Dupliquer", "", "", true, [this, list, i] {
                     change([list, i](Json& ev) {
                       Json& a = ev.at(Json::json_pointer(list));
                       Json copy = a[i];
                       a.insert(a.begin() + i + 1, copy);
                     });
                     goTo(list, i + 1);
                   }});
  items.push_back({"Supprimer", "", keepOne && n <= 1 ? "Il en faut au moins un." : "", !(keepOne && n <= 1), [this, list, i] {
                     change([list, i](Json& ev) {
                       Json& a = ev.at(Json::json_pointer(list));
                       a.erase(a.begin() + i);
                     });
                     goTo(list, std::max(0, i - 1));
                   }});
}

// ---------------------------------------------------------------------------
// Liste des événements
// ---------------------------------------------------------------------------
static std::string firstMessage(const Json& j) {
  if (j.is_object() && jget<std::string>(j, "action", "") == "dire") return jget<std::string>(j, "texte", "");
  if (j.is_structured())
    for (auto& v : j) {
      std::string s = firstMessage(v);
      if (!s.empty()) return s;
    }
  return "";
}

void StoryEditor::menuEvents(const std::string& sel) {
  G.menus.clear();
  current_ = "";
  Menu m;
  m.title = "Histoire : " + std::to_string(events().size()) + " événements";
  m.x = 4, m.y = 16, m.w = 312, m.rows = 13;
  m.items.push_back({"Nouvel événement", "", "Créer un événement vide (à relier ensuite à un habitant, une porte ou un boss).", true,
                     [this] { newEvent(); }});
  m.items.push_back(menuHeader("Événements"));
  std::vector<std::string> ids;
  for (auto& [id, ev] : events().items()) ids.push_back(id);
  std::sort(ids.begin(), ids.end());
  for (auto& id : ids) {
    auto u = usage(id);
    std::string where;
    for (size_t k = 0; k < u.size() && k < 2; k++) where += (k ? " ; " : "") + u[k];
    if (u.size() > 2) where += " …";
    if (id == sel) m.sel = (int)m.items.size();
    std::string first = firstMessage(*findEvent(id));
    m.items.push_back({id, u.empty() ? "inutilisé" : std::to_string(u.size()) + " lieu" + (u.size() > 1 ? "x" : ""),
                       (where.empty() ? std::string("Inutilisé.") : where) + (first.empty() ? "" : " — « " + first + " »"), true,
                       [this, id] { editEvent(id); }});
  }
  m.items.push_back(menuHeader("Fichier et test"));
  MenuItem sv{"Enregistrer (Ctrl+S)", "", "Écrit data/evenements.json.", true, [this] { save(); }};
  sv.rightFn = [this] { return std::string(dirty() ? "modifié" : "à jour"); };
  m.items.push_back(sv);
  m.items.push_back({"Tout annuler", "", "Revient au contenu du fichier.", true, [this] {
                       revert();
                       menuEvents();
                     }});
  m.items.push_back(numItem("Niveau de l'équipe de test", "Équipe utilisée par « Jouer l'événement ».", [this] { return (double)testLevel_; },
                            [this](double v) { testLevel_ = (int)v; }, 1, 1, 100));
  MenuItem fl{"Drapeaux du test", "", "Drapeaux déjà posés quand on joue un événement (séparés par des virgules), ex. boss1, maelle.", true};
  fl.rightFn = [this] { return testFlags.empty() ? std::string("aucun") : utf8Prefix(testFlags, 24); };
  fl.act = [this] {
    G.editText("Drapeaux déjà posés pour le test", testFlags, 120, [this](const std::string& v) {
      testFlags = v;
      menuEvents();
    });
  };
  m.items.push_back(fl);
  m.items.push_back({"Quitter l'éditeur", "", "", true, [this] { quit(); }});
  m.onCancel = [this] { quit(); };
  G.menus.push(m);
}

void StoryEditor::newEvent() {
  G.editText("Nom du nouvel événement (ex. marchand_ambulant)", "", 30, [this](const std::string& name) {
    if (name.empty()) return menuEvents();
    std::string id = makeSlug(name), base = id;
    for (int n = 2; findEvent(id); n++) id = base + "_" + std::to_string(n);
    change([id](Json& ev) { ev[id] = {{"actions", Json::array({{{"action", "dire"}, {"texte", "…"}}})}}; });
    editEvent(id);
  });
}

void StoryEditor::editEvent(const std::string& id, int sel) {
  G.menus.clear();
  const Json* evp = findEvent(id);
  if (!evp) return menuEvents();
  current_ = "/" + id;
  lastEvent_ = id;
  const Json& ev = *evp;
  Menu m;
  m.title = "Événement « " + id + " »";
  m.x = 4, m.y = 16, m.w = 312, m.rows = 13;
  m.sel = sel;
  if (ev.contains("pages")) {
    m.items.push_back(menuHeader("Pages (la première vraie est jouée)"));
    for (size_t p = 0; p < ev["pages"].size(); p++) {
      const Json& pg = ev["pages"][p];
      std::string cond = pg.contains("si") ? "si " + conditionSummary(pg["si"]) : "sinon (toujours)";
      m.items.push_back({utf8Prefix("Page " + std::to_string(p + 1) + " : " + cond, 40),
                         std::to_string(pg.value("actions", Json::array()).size()) + " >", "Page " + std::to_string(p + 1) + " : " + cond, true,
                         [this, id, p] { editPage(id, (int)p); }});
    }
    m.items.push_back({"Ajouter une page", "", "Nouvelle page avec sa condition (placée avant la dernière).", true, [this, id] {
                         int at = 0;
                         change([id, &at](Json& e) {
                           Json& pages = e[id]["pages"];
                           at = std::max(0, (int)pages.size() - 1);
                           pages.insert(pages.begin() + at, Json{{"si", {{"drapeau", "nouveau_drapeau"}}}, {"actions", Json::array()}});
                         });
                         editPage(id, at);
                       }});
  } else {
    m.items.push_back({"Actions", std::to_string(ev.value("actions", Json::array()).size()) + " >", "Ce que fait l'événement, dans l'ordre.", true,
                       [this, id] { editList("/" + id + "/actions"); }});
    m.items.push_back({"Découper en pages", "", "Pour un comportement qui change selon la progression (ex. avant et après un boss).", true,
                       [this, id] {
                         change([id](Json& e) {
                           Json acts = e[id].value("actions", Json::array());
                           e[id] = {{"pages", Json::array({Json{{"si", {{"drapeau", "nouveau_drapeau"}}}, {"actions", Json::array()}},
                                                           Json{{"actions", acts}}})}};
                         });
                         editEvent(id, 1);
                       }});
  }
  m.items.push_back(menuHeader("Outils"));
  m.items.push_back({"Jouer l'événement", "", "Lance l'événement là où il est utilisé, avec l'équipe et les drapeaux de test. Échap > Retour.",
                     true, [this, id] { play(id); }});
  auto u = usage(id);
  std::string all;
  for (auto& s : u) all += (all.empty() ? "" : " ; ") + s;
  m.items.push_back({"Utilisé par", std::to_string(u.size()), u.empty() ? "Personne : reliez-le dans l'éditeur de cartes." : all, false});
  m.items.push_back({"Dupliquer", "", "Copie l'événement sous un nouveau nom.", true, [this, id] {
                       G.editText("Nom de la copie", id + "_copie", 30, [this, id](const std::string& name) {
                         if (name.empty()) return editEvent(id);
                         std::string nid = makeSlug(name), base = nid;
                         for (int n = 2; findEvent(nid); n++) nid = base + "_" + std::to_string(n);
                         change([id, nid](Json& e) { e[nid] = e[id]; });
                         editEvent(nid);
                       });
                     }});
  m.items.push_back({"Supprimer l'événement", "", u.empty() ? "" : "Impossible : il est encore utilisé.", u.empty(), [this, id] {
                       change([id](Json& e) { e.erase(id); });
                       menuEvents();
                     }});
  m.onCancel = [this, id] { menuEvents(id); };
  G.menus.push(m);
}

void StoryEditor::editPage(const std::string& id, int p, int sel) {
  G.menus.clear();
  std::string ptr = "/" + id + "/pages/" + std::to_string(p);
  const Json& pg = peek(ptr);
  if (pg.is_null()) return editEvent(id);
  current_ = ptr;
  Menu m;
  m.title = titleOf(ptr);
  m.x = 4, m.y = 16, m.w = 312, m.rows = 13;
  m.sel = sel;
  std::string cond = pg.contains("si") ? conditionSummary(pg["si"]) : "toujours (page par défaut)";
  m.items.push_back({"Condition", utf8Prefix(cond, 30) + " >", "Condition : " + cond, true, [this, ptr] { editCondition(ptr + "/si"); }});
  m.items.push_back({"Actions", std::to_string(pg.value("actions", Json::array()).size()) + " >", "", true,
                     [this, ptr] { editList(ptr + "/actions"); }});
  std::vector<MenuItem> org;
  organize(org, ptr, true);
  for (auto& o : org) m.items.push_back(o);
  m.onCancel = [this, ptr] { goUp(ptr); };
  G.menus.push(m);
}

// ---------------------------------------------------------------------------
// Listes d'actions
// ---------------------------------------------------------------------------
static bool hasSublists(const Json& a) {
  std::string k = jget<std::string>(a, "action", "");
  return k == "combat" || k == "question" || k == "si";
}

void StoryEditor::editList(const std::string& ptr, int sel) {
  G.menus.clear();
  current_ = ptr;
  const Json& list = peek(ptr);
  int n = list.is_array() ? (int)list.size() : 0;
  Menu m;
  m.title = titleOf(ptr) + " (" + std::to_string(n) + ")";
  m.x = 4, m.y = 16, m.w = 312, m.rows = 13;
  m.sel = sel;
  for (int k = 0; k < n; k++) {
    std::string s = actionSummary(list[k]);
    m.items.push_back({utf8Prefix(s, 46), hasSublists(list[k]) ? ">" : "", s, true,
                       [this, ptr, k] { editAction(ptr + "/" + std::to_string(k)); }});
  }
  m.items.push_back({"+ Ajouter une action", "", "", true, [this, ptr] { addAction(ptr); }});
  m.onCancel = [this, ptr] { goUp(ptr); };
  G.menus.push(m);
}

void StoryEditor::addAction(const std::string& listPtr) {
  Menu m;
  m.title = "Nouvelle action";
  m.x = 40, m.y = 20, m.w = 200, m.rows = 12;
  for (int k = 0; k < (int)(sizeof ACTIONS / sizeof *ACTIONS); k++) {
    std::string type = ACTIONS[k];
    m.items.push_back({ACTIONS_FR[k], "", ACTIONS_HELP[k], true, [this, listPtr, type] {
                         Json a = {{"action", type}};
                         std::string firstCreature = "piafouine", firstEvent = events().empty() ? "" : events().begin().key();
                         for (auto& s : allSpecies())
                           if (!s.human && !s.limit.empty()) {
                             firstCreature = s.id;
                             break;
                           }
                         if (type == "dire") a["texte"] = "…";
                         if (type == "donner") a["objet"] = allItems()[0].id, a["quantite"] = 1;
                         if (type == "retirer") a["objet"] = allItems()[0].id;
                         if (type == "or") a["quantite"] = 100;
                         if (type == "drapeau") a["nom"] = "nouveau_drapeau";
                         if (type == "recruter") a["espece"] = rules().hero, a["niveau"] = 10;
                         if (type == "combat") {
                           a["ennemis"] = Json::array({{{"espece", firstCreature}, {"niveau", 10}}});
                           a["fuite"] = false, a["capture"] = false, a["victoire"] = Json::array();
                         }
                         if (type == "question") a["texte"] = "Voulez-vous continuer ?", a["oui"] = Json::array(), a["non"] = Json::array();
                         if (type == "si") a["condition"] = {{"drapeau", "nouveau_drapeau"}}, a["alors"] = Json::array(), a["sinon"] = Json::array();
                         if (type == "boutique") a["objets"] = Json::array({allItems()[0].id});
                         if (type == "teleporter") a["carte"] = rules().startMap, a["x"] = rules().startX, a["y"] = rules().startY, a["direction"] = "bas";
                         if (type == "evenement") a["id"] = firstEvent;
                         if (type == "attendre") a["secondes"] = 1.0;
                         if (type == "quete") a["id"] = quests().empty() ? std::string("?") : jget<std::string>(quests()[0], "id", "?");
                         int at = 0;
                         change([listPtr, a, &at](Json& ev) {
                           Json& l = ev[Json::json_pointer(listPtr)];
                           if (!l.is_array()) l = Json::array();
                           l.push_back(a);
                           at = (int)l.size() - 1;
                         });
                         std::string ptr = listPtr + "/" + std::to_string(at);
                         editAction(ptr);
                         if (type == "dire")  // on écrit tout de suite le message
                           G.editText("Message", "", 300, [this, ptr](const std::string& v) {
                             change([ptr, v](Json& ev) { ev[Json::json_pointer(ptr)]["texte"] = v.empty() ? "…" : v; });
                             editAction(ptr);
                           });
                       }});
  }
  G.menus.push(m);
}

// ---------------------------------------------------------------------------
// Une action
// ---------------------------------------------------------------------------
void StoryEditor::editAction(const std::string& ptr, int sel) {
  G.menus.clear();
  const Json& a = peek(ptr);
  if (!a.is_object()) return goUp(ptr);
  current_ = ptr;
  std::string k = jget<std::string>(a, "action", "");
  Menu m;
  m.title = utf8Prefix(actionSummary(a), 46);
  m.x = 4, m.y = 16, m.w = 312, m.rows = 13;
  m.sel = sel;
  auto set = [this, ptr](const std::string& key, Json v) { change([ptr, key, v](Json& ev) { ev[Json::json_pointer(ptr)][key] = v; }); };
  auto erase = [this, ptr](const std::string& key) { change([ptr, key](Json& ev) { ev[Json::json_pointer(ptr)].erase(key); }); };
  auto num = [&](const std::string& label, const std::string& key, double def, double step, double mn, double mx, bool asInt,
                 const std::string& help = "", bool eraseDef = false) {
    m.items.push_back(numItem(label, help, [this, ptr, key, def] { return jget(peek(ptr), key.c_str(), def); },
                              [set, erase, key, asInt, def, eraseDef](double v) {
                                if (eraseDef && std::fabs(v - def) < 1e-6) erase(key);
                                else if (asInt) set(key, (int)std::lround(v));
                                else set(key, v);
                              },
                              step, mn, mx));
  };
  auto toggle = [&](const std::string& label, const std::string& key, bool def, const std::string& help = "") {
    MenuItem it{label, "", help, true};
    auto flip = [this, ptr, key, def, set] { set(key, !jget(peek(ptr), key.c_str(), def)); };
    it.act = flip;
    it.adjust = [flip](int) { flip(); };
    it.rightFn = [this, ptr, key, def] { return std::string(jget(peek(ptr), key.c_str(), def) ? "Oui" : "Non"); };
    m.items.push_back(it);
  };
  auto cycleIds = [&](const std::string& label, const std::string& key, std::vector<std::string> ids, std::vector<std::string> names,
                      const std::string& help = "") {
    m.items.push_back(cycleItem(label, help,
                                [this, ptr, key, ids] {
                                  auto it = std::find(ids.begin(), ids.end(), jget<std::string>(peek(ptr), key.c_str(), ""));
                                  return it == ids.end() ? 0 : int(it - ids.begin());
                                },
                                [set, key, ids](int i) { set(key, ids[i]); }, (int)ids.size(), [names](int i) { return utf8Prefix(names[i], 22); }));
  };
  auto sub = [&](const std::string& label, const std::string& key, const std::string& help = "") {
    std::string p = ptr + "/" + key;
    m.items.push_back({label, std::to_string(a.value(key, Json::array()).size()) + " >", help, true, [this, p] { goTo(p); }});
  };
  std::vector<std::string> itemIds, itemNames, spIds, spNames, mapIds, mapNames, evIds;
  for (auto& d : allItems()) itemIds.push_back(d.id), itemNames.push_back(d.name);
  for (auto& s : allSpecies()) spIds.push_back(s.id), spNames.push_back(s.name);
  for (auto& mp : maps()) mapIds.push_back(mp.id), mapNames.push_back(mp.name);
  for (auto& [id, ev] : events().items()) evIds.push_back(id);
  int row = 0;
  if (k == "dire") {
    m.items.push_back(textField(ptr, "texte", "Texte", "Message : {heros}, {compagnon} et {or} sont remplacés par leur valeur", 300, row));
    num("Défilement auto (s)", "auto", 0, .5, 0, 10, false, "0 : le joueur appuie sur Entrée pour continuer.", true);
  } else if (k == "donner" || k == "retirer") {
    cycleIds("Objet", "objet", itemIds, itemNames);
    if (k == "donner") {
      num("Quantité", "quantite", 1, 1, 1, 99, true, "", true);
      toggle("Sans message", "silencieux", false, "Oui : pas de « Vous obtenez … ».");
    } else num("Quantité", "quantite", 0, 1, 0, 99, true, "0 : retire tous les exemplaires.", true);
  } else if (k == "or") {
    num("Quantité", "quantite", 100, 10, -99999, 99999, true, "Positif : gagner. Négatif : payer.");
    toggle("Sans message", "silencieux", false);
  } else if (k == "drapeau") {
    m.items.push_back(textField(ptr, "nom", "Nom du drapeau", "Nom du drapeau (sans espace, ex. pont_repare)", 40, row));
    toggle("Poser (Non : effacer)", "valeur", true);
  } else if (k == "recruter") {
    cycleIds("Personnage", "espece", spIds, spNames);
    num("Niveau minimum", "niveau", 1, 1, 1, 100, true, "Il arrive au moins au niveau moyen de l'équipe.");
  } else if (k == "combat") {
    m.items.push_back(textField(ptr, "nom", "Nom de l'adversaire", "Nom de l'adversaire (vide : créatures sauvages)", 40, row));
    sub("Ennemis", "ennemis", "1 à 3 ennemis en première ligne.");
    sub("Renforts", "renforts", "Entrent un par un quand un ennemi tombe.");
    toggle("Boss", "boss", false, "Décor de boss, pas de butin aléatoire.");
    toggle("Fuite possible", "fuite", true);
    toggle("Capture possible", "capture", true);
    sub("En cas de victoire", "victoire");
    sub("En cas de défaite", "defaite", "Après le réveil chez la guérisseuse.");
    m.items.push_back({"Tester dans l'Arène", "", "Ouvre l'Arène avec ces adversaires (votre équipe actuelle de l'Arène).", true, [this, ptr] {
                         if (!G.arena_) G.arena_ = std::make_unique<Arena>(G);
                         G.arena_->setFoes(peek(ptr));
                         G.mode = Mode::Arena;
                         G.arena_->open();
                       }});
  } else if (k == "question") {
    m.items.push_back(textField(ptr, "texte", "Question", "Question posée au joueur", 120, row));
    sub("Si oui", "oui");
    sub("Si non", "non");
  } else if (k == "si") {
    m.items.push_back({"Condition", utf8Prefix(conditionSummary(a.value("condition", Json::object())), 30) + " >", "", true,
                       [this, ptr] { editCondition(ptr + "/condition"); }});
    sub("Alors", "alors", "Joué si la condition est vraie.");
    sub("Sinon", "sinon", "Joué sinon.");
  } else if (k == "soigner") {
    toggle("Rendre les PV", "pv", true);
    toggle("Rendre les PM", "pm", true);
  } else if (k == "boutique") {
    sub("Objets en vente", "objets");
  } else if (k == "teleporter") {
    cycleIds("Carte", "carte", mapIds, mapNames);
    num("Colonne (x)", "x", 0, 1, 0, 300, true);
    num("Ligne (y)", "y", 0, 1, 0, 300, true);
    cycleIds("Regard", "direction", {"haut", "bas", "gauche", "droite"}, {"haut", "bas", "gauche", "droite"});
  } else if (k == "evenement") {
    cycleIds("Événement", "id", evIds, evIds);
  } else if (k == "attendre") {
    num("Secondes", "secondes", .5, .5, .5, 10, false);
  } else if (k == "fin") {
    m.items.push_back(textField(ptr, "texte", "Texte", "Texte de l'écran de fin (vide : texte par défaut)", 160, row));
  } else if (k == "quete") {
    std::vector<std::string> qIds, qNames;
    for (auto& q : quests()) qIds.push_back(jget<std::string>(q, "id", "")), qNames.push_back(jget<std::string>(q, "nom", ""));
    cycleIds("Quête", "id", qIds, qNames);
    toggle("La terminer", "fin", false, "Non : la quête commence. Oui : elle est terminée.");
  } else {
    m.items.push_back({"(pas de réglage)", "", "", false});
  }
  std::vector<MenuItem> org;
  organize(org, ptr, false);
  for (auto& o : org) m.items.push_back(o);
  m.onCancel = [this, ptr] { goUp(ptr); };
  G.menus.push(m);
}

// ---------------------------------------------------------------------------
// Conditions, ennemis, objets d'une boutique
// ---------------------------------------------------------------------------
void StoryEditor::editCondition(const std::string& ptr, int sel) {
  G.menus.clear();
  current_ = ptr;
  Menu m;
  m.title = titleOf(ptr) + " : " + utf8Prefix(conditionSummary(peek(ptr)), 30);
  m.x = 4, m.y = 16, m.w = 312, m.rows = 13;
  m.sel = sel;
  auto setKey = [this, ptr](const std::string& key, Json v) {
    change([ptr, key, v](Json& ev) {
      Json& c = ev[Json::json_pointer(ptr)];
      if (!c.is_object()) c = Json::object();
      if (v.is_null()) c.erase(key);
      else c[key] = v;
    });
  };
  auto flagField = [&](const std::string& label, const std::string& key, int row) {
    MenuItem it{label, "", "Un ou plusieurs drapeaux séparés par des virgules (vide : aucun).", true};
    it.rightFn = [this, ptr, key] {
      const Json& c = peek(ptr);
      std::string s = c.is_object() && c.contains(key) ? listStr(c[key], nullptr) : "";
      return s.empty() ? std::string("—") : utf8Prefix(s, 24);
    };
    it.act = [this, ptr, key, setKey, row] {
      const Json& c = peek(ptr);
      std::string cur = c.is_object() && c.contains(key) ? listStr(c[key], nullptr) : "";
      G.editText("Drapeaux (séparés par des virgules)", cur, 120, [this, ptr, key, setKey, row](const std::string& v) {
        std::vector<std::string> parts;
        std::string w;
        for (char ch : v + ",") {
          if (ch == ',') {
            while (!w.empty() && w.front() == ' ') w.erase(w.begin());
            while (!w.empty() && w.back() == ' ') w.pop_back();
            if (!w.empty()) parts.push_back(w);
            w.clear();
          } else w += ch;
        }
        setKey(key, parts.empty() ? Json() : parts.size() == 1 ? Json(parts[0]) : Json(parts));
        editCondition(ptr, row);
      });
    };
    m.items.push_back(it);
  };
  auto idCycle = [&](const std::string& label, const std::string& key, std::vector<std::string> ids, std::vector<std::string> names) {
    ids.insert(ids.begin(), ""), names.insert(names.begin(), "—");
    m.items.push_back(cycleItem(label, "",
                                [this, ptr, key, ids] {
                                  const Json& c = peek(ptr);
                                  std::string cur = c.is_object() && c.contains(key) && c[key].is_string() ? c[key].get<std::string>() : "";
                                  auto it = std::find(ids.begin(), ids.end(), cur);
                                  return it == ids.end() ? 0 : int(it - ids.begin());
                                },
                                [setKey, key, ids](int i) { setKey(key, ids[i].empty() ? Json() : Json(ids[i])); }, (int)ids.size(),
                                [names](int i) { return utf8Prefix(names[i], 22); }));
  };
  std::vector<std::string> itemIds, itemNames, spIds, spNames;
  for (auto& d : allItems()) itemIds.push_back(d.id), itemNames.push_back(d.name);
  for (auto& s : allSpecies()) spIds.push_back(s.id), spNames.push_back(s.name);
  flagField("Drapeau posé", "drapeau", 0);
  flagField("Drapeau absent", "sans_drapeau", 1);
  idCycle("Possède l'objet", "objet", itemIds, itemNames);
  idCycle("Ne possède pas", "sans_objet", itemIds, itemNames);
  m.items.push_back(numItem("Or minimum", "0 : pas de condition sur l'or.",
                            [this, ptr] { return (double)jget(peek(ptr), "or_min", 0); },
                            [setKey](double v) { setKey("or_min", v <= 0 ? Json() : Json((int)v)); }, 10, 0, 99999));
  idCycle("Dans l'équipe", "membre", spIds, spNames);
  auto t = split(ptr);
  if (t.back() == "si")
    m.items.push_back({"Retirer la condition", "", "La page sera jouée dans tous les cas (à mettre en dernier).", true, [this, ptr] {
                         change([ptr](Json& ev) {
                           auto tt = split(ptr);
                           ev[Json::json_pointer(join(tt, tt.size() - 1))].erase("si");
                         });
                         goUp(ptr);
                       }});
  m.onCancel = [this, ptr] { goUp(ptr); };
  G.menus.push(m);
}

void StoryEditor::editEnemies(const std::string& ptr, int sel) {
  G.menus.clear();
  current_ = ptr;
  const Json& list = peek(ptr);
  int n = list.is_array() ? (int)list.size() : 0;
  int maxN = split(ptr).back() == "ennemis" ? 3 : 6;
  Menu m;
  m.title = titleOf(ptr) + " (" + std::to_string(n) + "/" + std::to_string(maxN) + ")";
  m.x = 4, m.y = 16, m.w = 312, m.rows = 13;
  m.sel = sel;
  for (int k = 0; k < n; k++) {
    const Json& e = list[k];
    float pv = jget(e, "pv", 1.f);
    std::string s = spName(jget<std::string>(e, "espece", "")) + " N." + std::to_string(jget(e, "niveau", 1)) +
                    (pv != 1.f ? " ×" + fmtNum(pv) : "") + (jget(e, "boss", false) ? " (boss)" : "");
    m.items.push_back({s, ">", "", true, [this, ptr, k] { editEnemy(ptr + "/" + std::to_string(k)); }});
  }
  m.items.push_back({"+ Ajouter", "", n >= maxN ? "Maximum atteint." : "", n < maxN, [this, ptr] {
                       std::string sp = "piafouine";
                       for (auto& s : allSpecies())
                         if (!s.human && !s.limit.empty()) {
                           sp = s.id;
                           break;
                         }
                       int at = 0;
                       change([ptr, sp, &at](Json& ev) {
                         Json& l = ev[Json::json_pointer(ptr)];
                         if (!l.is_array()) l = Json::array();
                         l.push_back({{"espece", sp}, {"niveau", 10}});
                         at = (int)l.size() - 1;
                       });
                       editEnemy(ptr + "/" + std::to_string(at));
                     }});
  m.onCancel = [this, ptr] { goUp(ptr); };
  G.menus.push(m);
}

void StoryEditor::editEnemy(const std::string& ptr, int sel) {
  G.menus.clear();
  if (!peek(ptr).is_object()) return goUp(ptr);
  current_ = ptr;
  Menu m;
  m.title = titleOf(split(ptr).size() > 1 ? ptr.substr(0, ptr.rfind('/')) : ptr);
  m.x = 4, m.y = 16, m.w = 312, m.rows = 13;
  m.sel = sel;
  auto set = [this, ptr](const std::string& key, Json v) {
    change([ptr, key, v](Json& ev) {
      Json& o = ev[Json::json_pointer(ptr)];
      if (v.is_null()) o.erase(key);
      else o[key] = v;
    });
  };
  std::vector<std::string> ids;
  for (auto& s : allSpecies()) ids.push_back(s.id);
  m.items.push_back(cycleItem("Espèce", "",
                              [this, ptr, ids] {
                                auto it = std::find(ids.begin(), ids.end(), jget<std::string>(peek(ptr), "espece", ""));
                                return it == ids.end() ? 0 : int(it - ids.begin());
                              },
                              [set, ids](int i) { set("espece", ids[i]); }, (int)ids.size(), [ids](int i) { return species(ids[i]).name; }));
  m.items.push_back(numItem("Niveau", "", [this, ptr] { return (double)jget(peek(ptr), "niveau", 1); }, [set](double v) { set("niveau", (int)v); },
                            1, 1, 100));
  m.items.push_back(numItem("PV ×", "Multiplicateur de PV (les boss ont ×3 à ×5).", [this, ptr] { return (double)jget(peek(ptr), "pv", 1.f); },
                            [set](double v) { set("pv", std::fabs(v - 1) < 1e-6 ? Json() : Json(v)); }, .5, .5, 20));
  MenuItem b{"Boss", "", "Affichage plus grand, plus d'expérience et d'or.", true};
  auto flip = [this, ptr, set] { set("boss", jget(peek(ptr), "boss", false) ? Json() : Json(true)); };
  b.act = flip;
  b.adjust = [flip](int) { flip(); };
  b.rightFn = [this, ptr] { return std::string(jget(peek(ptr), "boss", false) ? "Oui" : "Non"); };
  m.items.push_back(b);
  std::vector<MenuItem> org;
  organize(org, ptr, split(ptr)[split(ptr).size() - 2] == "ennemis");
  for (auto& o : org) m.items.push_back(o);
  m.onCancel = [this, ptr] { goUp(ptr); };
  G.menus.push(m);
}

void StoryEditor::pickItems(const std::string& ptr) {
  G.menus.clear();
  current_ = ptr;
  Menu m;
  m.title = titleOf(ptr);
  m.x = 4, m.y = 16, m.w = 312, m.rows = 13;
  for (auto& d : allItems()) {
    std::string id = d.id;
    auto has = [this, ptr, id] {
      const Json& l = peek(ptr);
      if (!l.is_array()) return false;
      for (auto& x : l)
        if (x.is_string() && x.get<std::string>() == id) return true;
      return false;
    };
    MenuItem it{d.name, "", std::to_string(d.price) + " or · " + d.desc, true};
    auto flip = [this, ptr, id, has] {
      bool on = has();
      change([ptr, id, on](Json& ev) {
        Json& l = ev[Json::json_pointer(ptr)];
        if (!l.is_array()) l = Json::array();
        Json out = Json::array();
        for (auto& x : l)
          if (!(x.is_string() && x.get<std::string>() == id)) out.push_back(x);
        if (!on) out.push_back(id);
        l = out;
      });
    };
    it.act = flip;
    it.adjust = [flip](int) { flip(); };
    it.rightFn = [has] { return std::string(has() ? "En vente" : "—"); };
    m.items.push_back(it);
  }
  m.onCancel = [this, ptr] { goUp(ptr); };
  G.menus.push(m);
}

// ---------------------------------------------------------------------------
// Jouer un événement
// ---------------------------------------------------------------------------
void StoryEditor::play(const std::string& id) {
  // Lieu : là où l'événement est utilisé, sinon le départ de la partie
  int mi = std::max(0, mapIndex(rules().startMap)), x = rules().startX, y = rules().startY, dir = DOWN;
  bool found = false;
  for (int k = 0; k < (int)maps().size() && !found; k++) {
    const MapDef& m = maps()[k];
    auto spot = [&](int tx, int ty, int d) {
      if (found || tx < 0 || ty < 0 || tx >= m.w() || ty >= m.h() || !tileWalkable(m.rows[ty][tx])) return;
      mi = k, x = tx, y = ty, dir = d, found = true;
    };
    for (auto& n : m.npcs)
      if (n.event == id) spot(n.x, n.y + 1, UP), spot(n.x, n.y - 1, DOWN), spot(n.x - 1, n.y, RIGHT), spot(n.x + 1, n.y, LEFT);
    for (auto& b : m.bosses)
      if (b.event == id) spot(b.x, b.y + 2, UP), spot(b.x, b.y - 1, DOWN);
    for (auto& b : m.buildings)
      if (b.event == id) spot(b.doorX(), b.doorY() + 1, UP);
    for (auto& t : m.triggers)
      if (t.event == id) spot(t.x, t.y, DOWN);
  }
  G.menus.clear();
  G.sc.clear();
  G.flags.clear();
  std::string w;
  for (char c : testFlags + ",") {
    if (c == ',' || c == ' ') {
      if (!w.empty()) G.flags.insert(w);
      w.clear();
    } else w += c;
  }
  G.team.clear();
  G.team.push_back(makeFighter(rules().hero, testLevel_));
  for (size_t k = 0; k < rules().starters.size() && k < 2; k++) G.team.push_back(makeFighter(rules().starters[k], testLevel_));
  G.items.clear();
  for (auto [iid, n] : std::vector<std::pair<std::string, int>>{{"potion", 5}, {"ether", 2}, {"remede", 2}, {"lanterne", 5}})
    if (hasItem(iid)) G.items[iid] = n;
  G.gold = 500;
  G.storyTest_ = true;
  G.respawnMap = mi, G.respawnX = x, G.respawnY = y;
  G.mapId = -1;
  G.changeMap(mi, x, y, dir);
  G.mode = Mode::Map;
  lastEvent_ = id;
  G.runEvent(id);
}

void StoryEditor::returnFromTest() {
  G.storyTest_ = false;
  G.menus.clear();
  G.sc.clear();
  G.mode = Mode::Story;
  say("Retour à l'éditeur d'histoire.");
  editEvent(lastEvent_);
}

// ---------------------------------------------------------------------------
void StoryEditor::update(float) {
  if (G.editingText()) return;
  Input& in = G.in;
  if (in.undo) return undo();
  if (in.redo) return redo();
  if (in.saveKey) save();
  if (!G.menus.active()) menuEvents();
  G.menus.update(in);
}

void StoryEditor::draw() {
  Gfx& g = G.g;
  g.gradV(g.left(), 0, g.fullW, SCREEN_H, rgb(0x241a30), rgb(0x3e2c4e));
  g.text(160, 3, "ÉDITEUR D'HISTOIRE", GOLD, 1);
  if (!errors_.empty()) g.text(316, 3, std::to_string(errors_.size()) + " erreur(s)", RED, 2);
  else g.text(316, 3, dirty() ? "modifié" : "ok", dirty() ? GOLD : GREEN, 2);
  G.menus.draw(g, G.time);
  std::string h = G.time - messageT_ < 3 ? message_ : G.menus.help();
  // Les erreurs restent visibles en bas quand il n'y a pas d'aide
  if (h.empty() && !errors_.empty()) h = errors_[0];
  if (!h.empty()) {
    auto lines = Gfx::wrap(h, 300);
    if (lines.size() > 3) lines.resize(3);
    int hh = 8 + 11 * (int)lines.size();
    g.window(4, 238 - hh, 312, hh);
    Color c = G.time - messageT_ < 3 ? GOLD : (G.menus.help().empty() && !errors_.empty()) ? RED : MUTED;
    for (size_t i = 0; i < lines.size(); i++) g.text(10, 238 - hh + 4 + i * 11, lines[i], c);
  }
}
