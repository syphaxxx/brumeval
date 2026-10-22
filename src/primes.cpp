#include "primes.hpp"

#include <map>

#include "data.hpp"
#include "i18n.hpp"
#include "world.hpp"

static Json BOUNTIES = Json::array(), RANKS = Json::array();
static std::map<std::string, Json> EVENTS_;  // « prime:<id> » → événement généré

const Json& bounties() { return BOUNTIES; }
const Json& bountyRanks() { return RANKS; }
std::string bountyFlag(const std::string& id, int stage) {
  return "prime:" + id + (stage == 1 ? ":vaincue" : stage == 2 ? ":fin" : "");
}
const Json* findBounty(const std::string& id) {
  for (auto& b : BOUNTIES)
    if (jget<std::string>(b, "id", "") == id) return &b;
  return nullptr;
}
const Json* bountyEvent(const std::string& eventId) {
  auto it = EVENTS_.find(eventId);
  return it == EVENTS_.end() ? nullptr : &it->second;
}

// L'événement du monstre : sa phrase d'arrivée, puis le combat (sans fuite ni capture) ; la victoire pose « vaincue »
static Json makeEvent(const Json& b) {
  std::string id = jget<std::string>(b, "id", "");
  Json fight = b.value("combat", Json::object());
  fight["action"] = "combat";
  if (!fight.contains("nom")) fight["nom"] = jget<std::string>(b, "nom", id);
  fight["boss"] = true;
  fight["capture"] = false;
  if (!fight.contains("fuite")) fight["fuite"] = true;
  Json win = Json::array({Json{{"action", "drapeau"}, {"nom", bountyFlag(id, 1)}},
                          Json{{"action", "dire"}, {"texte", N_("Prime remplie ! La récompense attend au tableau d'annonces.")}}});
  for (auto& a : fight.value("victoire", Json::array())) win.push_back(a);
  fight["victoire"] = win;
  Json acts = Json::array();
  if (b.contains("arrivee")) acts.push_back(Json{{"action", "dire"}, {"texte", b["arrivee"]}});
  acts.push_back(fight);
  return Json{{"actions", acts}};
}

void loadBounties() {
  BOUNTIES = Json::array(), RANKS = Json::array();
  EVENTS_.clear();
  Json doc = readJson("primes.json");
  if (!doc.is_object()) throw std::runtime_error("data/primes.json doit contenir un objet { \"primes\": […], \"rangs\": […] }");
  BOUNTIES = doc.value("primes", Json::array());
  RANKS = doc.value("rangs", Json::array());
  for (auto& b : BOUNTIES) EVENTS_["prime:" + jget<std::string>(b, "id", "")] = makeEvent(b);
}

void addBountySpots(MapDef& m) {
  for (auto& b : BOUNTIES) {
    std::string id = jget<std::string>(b, "id", "");
    if (m.id != jget<std::string>(b, "carte", "")) continue;
    BossSpot s{jget(b, "x", 0), jget(b, "y", 0), jget<std::string>(b, "espece", ""), bountyFlag(id, 1), "prime:" + id};
    s.cond = Json{{"drapeau", bountyFlag(id, 0)}};
    s.bounty = true;
    m.bosses.push_back(s);
  }
}

std::vector<std::string> checkBounties() {
  std::vector<std::string> err;
  std::map<std::string, int> seen;
  for (auto& b : BOUNTIES) {
    std::string id = jget<std::string>(b, "id", "");
    std::string w = T("Prime « ") + id + " »";
    if (id.empty() || seen[id]++) err.push_back(w + T(" : identifiant vide ou en double"));
    if (mapIndex(jget<std::string>(b, "carte", "")) < 0) err.push_back(w + T(" : carte inconnue « ") + jget<std::string>(b, "carte", "") + " »");
    if (!hasSpecies(jget<std::string>(b, "espece", ""))) err.push_back(w + T(" : espèce inconnue « ") + jget<std::string>(b, "espece", "") + " »");
    if (!b.contains("combat") || !b["combat"].contains("ennemis")) err.push_back(w + T(" : combat manquant"));
    for (auto& o : b.value("objets", Json::array()))
      if (!hasItem(jget<std::string>(o, "id", ""))) err.push_back(w + T(" : objet inconnu « ") + jget<std::string>(o, "id", "") + " »");
    int rank = jget(b, "etoiles", 0);
    if (rank < 1 || rank > 5) err.push_back(w + T(" : « etoiles » doit aller de 1 à 5"));
  }
  int last = 0;
  for (auto& r : RANKS) {
    int n = jget(r, "primes", 0);
    if (n <= last) err.push_back(T("Primes : les rangs doivent demander de plus en plus de primes"));
    last = n;
    for (auto& o : r.value("objets", Json::array()))
      if (!hasItem(jget<std::string>(o, "id", ""))) err.push_back(T("Primes, rang : objet inconnu « ") + jget<std::string>(o, "id", "") + " »");
  }
  return err;
}
