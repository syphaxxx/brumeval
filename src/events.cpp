#include "events.hpp"

#include <algorithm>
#include <map>

#include "data.hpp"
#include "game.hpp"
#include "world.hpp"

static Json EVENTS = Json::object();
Json& events() { return EVENTS; }
const Json* findEvent(const std::string& id) {
  auto it = EVENTS.find(id);
  return it == EVENTS.end() ? nullptr : &*it;
}
void loadEvents() {
  EVENTS = readJson("evenements.json");
  if (!EVENTS.is_object()) throw std::runtime_error("data/evenements.json doit contenir un objet { identifiant : événement }");
}
void saveEvents() { writeJson("evenements.json", EVENTS); }

// Pages d'un événement : "pages" ou, en raccourci, une seule liste "actions"
static Json pagesOf(const Json& ev) {
  if (ev.contains("pages")) return ev["pages"];
  return Json::array({{{"actions", ev.value("actions", Json::array())}}});
}
static std::vector<std::string> names(const Json& v) {
  std::vector<std::string> o;
  if (v.is_string()) o.push_back(v.get<std::string>());
  else if (v.is_array())
    for (auto& x : v) o.push_back(x.get<std::string>());
  return o;
}

// ---------------------------------------------------------------------------
// Vérifications
// ---------------------------------------------------------------------------
static void checkList(const Json& list, const std::string& where, std::vector<std::string>& err);

static void checkCondition(const Json& c, const std::string& where, std::vector<std::string>& err) {
  static const std::vector<std::string> keys = {"drapeau", "sans_drapeau", "objet", "sans_objet", "or_min", "membre"};
  for (auto& [k, v] : c.items()) {
    if (std::find(keys.begin(), keys.end(), k) == keys.end()) err.push_back(where + " : condition inconnue « " + k + " »");
    if (k == "objet" || k == "sans_objet")
      for (auto& id : names(v))
        if (!hasItem(id)) err.push_back(where + " : objet inconnu « " + id + " »");
    if (k == "membre")
      for (auto& id : names(v))
        if (!hasSpecies(id)) err.push_back(where + " : espèce inconnue « " + id + " »");
  }
}

static void checkAction(const Json& a, const std::string& where, std::vector<std::string>& err) {
  if (!a.is_object() || !a.contains("action")) {
    err.push_back(where + " : chaque action doit avoir un champ \"action\"");
    return;
  }
  std::string k = a["action"].get<std::string>();
  std::string w = where + " (" + k + ")";
  auto need = [&](const char* f) {
    if (!a.contains(f)) err.push_back(w + " : champ « " + f + " » manquant");
    return a.contains(f);
  };
  if (k == "dire") need("texte");
  else if (k == "donner" || k == "retirer") {
    if (need("objet") && !hasItem(a["objet"].get<std::string>())) err.push_back(w + " : objet inconnu « " + a["objet"].get<std::string>() + " »");
  } else if (k == "or") need("quantite");
  else if (k == "drapeau") need("nom");
  else if (k == "recruter") {
    if (need("espece") && !hasSpecies(a["espece"].get<std::string>())) err.push_back(w + " : espèce inconnue « " + a["espece"].get<std::string>() + " »");
  } else if (k == "combat") {
    if (need("ennemis")) {
      if (a["ennemis"].empty() || a["ennemis"].size() > 3) err.push_back(w + " : il faut entre 1 et 3 ennemis");
      for (auto& e : a["ennemis"])
        if (!hasSpecies(jget<std::string>(e, "espece", ""))) err.push_back(w + " : espèce inconnue « " + jget<std::string>(e, "espece", "") + " »");
    }
    checkList(a.value("victoire", Json::array()), w + " > victoire", err);
    checkList(a.value("defaite", Json::array()), w + " > défaite", err);
  } else if (k == "question") {
    need("texte");
    checkList(a.value("oui", Json::array()), w + " > oui", err);
    checkList(a.value("non", Json::array()), w + " > non", err);
  } else if (k == "si") {
    if (need("condition")) checkCondition(a["condition"], w, err);
    checkList(a.value("alors", Json::array()), w + " > alors", err);
    checkList(a.value("sinon", Json::array()), w + " > sinon", err);
  } else if (k == "boutique") {
    if (need("objets"))
      for (auto& id : names(a["objets"]))
        if (!hasItem(id)) err.push_back(w + " : objet inconnu « " + id + " »");
  } else if (k == "teleporter") {
    if (need("carte") && mapIndex(a["carte"].get<std::string>()) < 0) err.push_back(w + " : carte inconnue « " + a["carte"].get<std::string>() + " »");
    need("x"), need("y");
  } else if (k == "evenement") {
    if (need("id") && !findEvent(a["id"].get<std::string>())) err.push_back(w + " : événement inconnu « " + a["id"].get<std::string>() + " »");
  } else if (k == "attendre") need("secondes");
  else if (k != "soigner" && k != "reveil" && k != "fin") err.push_back(where + " : action inconnue « " + k + " »");
}

static void checkList(const Json& list, const std::string& where, std::vector<std::string>& err) {
  for (size_t i = 0; i < list.size(); i++) checkAction(list[i], where + " #" + std::to_string(i + 1), err);
}

std::vector<std::string> checkEvents() {
  std::vector<std::string> err;
  for (auto& [id, ev] : EVENTS.items()) {
    Json pages = pagesOf(ev);
    for (size_t p = 0; p < pages.size(); p++) {
      std::string where = "Événement « " + id + " »" + (pages.size() > 1 ? " page " + std::to_string(p + 1) : "");
      if (pages[p].contains("si")) checkCondition(pages[p]["si"], where, err);
      checkList(pages[p].value("actions", Json::array()), where, err);
    }
  }
  auto ref = [&](const std::string& id, const std::string& who) {
    if (!id.empty() && !findEvent(id)) err.push_back(who + " : événement inconnu « " + id + " »");
  };
  for (auto& m : maps()) {
    for (auto& n : m.npcs) ref(n.event, m.name + ", habitant (" + std::to_string(n.x) + ", " + std::to_string(n.y) + ")");
    for (auto& b : m.buildings) ref(b.event, m.name + ", bâtiment « " + b.name + " »");
    for (auto& b : m.bosses) ref(b.event, m.name + ", boss « " + b.id + " »");
  }
  ref(rules().startEvent, "Règles, départ");
  return err;
}

// ---------------------------------------------------------------------------
// Exécution (méthodes de Game)
// ---------------------------------------------------------------------------
bool Game::checkCond(const Json& c) const {
  auto count = [&](const std::string& id) {
    auto it = items.find(id);
    return it == items.end() ? 0 : it->second;
  };
  for (auto& f : names(c.value("drapeau", Json())))
    if (!has(f)) return false;
  for (auto& f : names(c.value("sans_drapeau", Json())))
    if (has(f)) return false;
  for (auto& id : names(c.value("objet", Json())))
    if (count(id) <= 0) return false;
  for (auto& id : names(c.value("sans_objet", Json())))
    if (count(id) > 0) return false;
  if (c.contains("or_min") && gold < c["or_min"].get<int>()) return false;
  for (auto& id : names(c.value("membre", Json()))) {
    bool found = false;
    for (auto& f : team) found = found || f->sp == id;
    if (!found) return false;
  }
  return true;
}

std::string Game::fillText(std::string s) const {
  auto rep = [&](const std::string& key, const std::string& val) {
    for (size_t p; (p = s.find(key)) != std::string::npos;) s.replace(p, key.size(), val);
  };
  if (s.find('{') == std::string::npos) return s;
  rep("{heros}", team.size() > 0 ? team[0]->name() : "");
  rep("{compagnon}", team.size() > 1 ? team[1]->name() : "");
  rep("{or}", std::to_string(gold));
  return s;
}

void Game::runEvent(const std::string& id) {
  const Json* ev = findEvent(id);
  if (!ev) {
    sc.say("(Événement introuvable : " + id + ")");
    return;
  }
  for (auto& page : pagesOf(*ev))
    if (!page.contains("si") || checkCond(page["si"])) {
      runActions(page.value("actions", Json::array()));
      return;
    }
}

void Game::runActions(const Json& list) {
  for (auto& a : list) {
    std::string k = jget<std::string>(a, "action", "");
    if (k == "dire") sc.say(fillText(jget<std::string>(a, "texte", "")), jget(a, "auto", 0.f));
    else if (k == "attendre") sc.wait(jget(a, "secondes", .5f));
    else sc.call([this, a] { execAction(a); });
  }
}

void Game::execAction(const Json& a) {
  std::string k = jget<std::string>(a, "action", "");
  if (k == "donner") {
    std::string id = a["objet"].get<std::string>();
    int q = jget(a, "quantite", 1);
    items[id] += q;
    if (!jget(a, "silencieux", false)) sc.say("Vous obtenez : " + item(id).name + (q > 1 ? " ×" + std::to_string(q) : "") + " !");
  } else if (k == "retirer") {
    std::string id = a["objet"].get<std::string>();
    int q = jget(a, "quantite", 0);  // 0 = tout
    items[id] = q > 0 ? std::max(0, items[id] - q) : 0;
  } else if (k == "or") {
    int q = a["quantite"].get<int>();
    gold = std::max(0, gold + q);
    if (q > 0 && !jget(a, "silencieux", false)) sc.say("Vous recevez " + std::to_string(q) + " pièces d'or.");
  } else if (k == "drapeau") {
    std::string f = a["nom"].get<std::string>();
    if (jget(a, "valeur", true)) flags.insert(f);
    else flags.erase(f);
  } else if (k == "recruter") {
    recruit(a["espece"].get<std::string>(), jget(a, "niveau", 1));
  } else if (k == "combat") {
    std::vector<FighterP> foes;
    for (auto& e : a["ennemis"]) {
      auto f = makeFighter(e.at("espece").get<std::string>(), jget(e, "niveau", avgLevel()));
      float mult = jget(e, "pv", 1.f);
      if (mult != 1.f) f->mhp = std::max(1, int(f->mhp * mult + 1e-4f));
      f->hp = f->mhp;
      f->boss = jget(e, "boss", false);
      foes.push_back(f);
    }
    Json win = a.value("victoire", Json::array()), lose = a.value("defaite", Json::array());
    startBattle(foes, jget(a, "boss", false), [this, win, lose](BattleResult r) {
      if (r == BattleResult::Win) runActions(win);
      else if (r == BattleResult::Lose) runActions(lose);
    }, jget(a, "fuite", true), jget(a, "capture", true));
  } else if (k == "question") {
    Json yes = a.value("oui", Json::array()), no = a.value("non", Json::array());
    ask(fillText(a["texte"].get<std::string>()), [this, yes] { runActions(yes); }, [this, no] { runActions(no); });
  } else if (k == "si") {
    runActions(checkCond(a["condition"]) ? a.value("alors", Json::array()) : a.value("sinon", Json::array()));
  } else if (k == "soigner") {
    bool hp = jget(a, "pv", true), mp = jget(a, "pm", true);
    for (auto& f : team) {
      if (hp) f->hp = f->mhp;
      if (mp) f->mp = f->mmp;
    }
  } else if (k == "boutique") {
    shopMenu(names(a["objets"]));
  } else if (k == "reveil") {
    respawnMap = mapId, respawnX = px, respawnY = py;
  } else if (k == "teleporter") {
    int m = mapIndex(a["carte"].get<std::string>());
    if (m >= 0) changeMap(m, a["x"].get<int>(), a["y"].get<int>(), dirOf(jget<std::string>(a, "direction", "bas")));
  } else if (k == "evenement") {
    runEvent(a["id"].get<std::string>());
  } else if (k == "fin") {
    mode = Mode::Ending;
  }
}
