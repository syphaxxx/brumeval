// Primes : le tableau d'annonces (Game::bountyMenu) et la récompense (claimBounty). Données : primes.hpp.
#include "audio.hpp"
#include "data.hpp"
#include "game.hpp"
#include "i18n.hpp"
#include "primes.hpp"
#include "world.hpp"

int Game::bountiesClaimed() const {
  int n = 0;
  for (auto& b : bounties())
    if (has(bountyFlag(jget<std::string>(b, "id", ""), 2))) n++;
  return n;
}

std::string Game::hunterTitle() const {
  std::string t = T("Novice");
  for (auto& r : bountyRanks())
    if (bountiesClaimed() >= jget(r, "primes", 0)) t = tr(jget<std::string>(r, "titre", ""));
  return t;
}

static std::string stars(int n) {
  std::string s;
  for (int i = 0; i < n; i++) s += "★";
  return s;
}

static std::string rewardText(const Json& b) {
  std::string s;
  int gold = jget(b, "or", 0);
  if (gold > 0) s = std::to_string(gold) + T(" or");
  for (auto& o : b.value("objets", Json::array())) {
    std::string id = jget<std::string>(o, "id", "");
    if (!hasItem(id)) continue;
    int q = jget(o, "quantite", 1);
    s += (s.empty() ? "" : ", ") + item(id).name + (q > 1 ? T(" ×") + std::to_string(q) : "");
  }
  return s;
}

// Tableau d'annonces : les primes offertes (condition « si » vraie), acceptées ou finies
void Game::bountyMenu(int sel) {
  panelMode_ = 0;
  Menu m;
  m.title = T("Primes") + std::string("  ·  ") + hunterTitle();
  m.x = 24, m.y = 16, m.w = 272, m.rows = 9;
  int hidden = 0;
  for (auto& b : bounties()) {
    std::string id = jget<std::string>(b, "id", "");
    bool taken = has(bountyFlag(id, 0)), beaten = has(bountyFlag(id, 1)), done = has(bountyFlag(id, 2));
    if (!taken && b.contains("si") && !checkCond(b["si"])) {
      hidden++;
      continue;
    }
    std::string state = done ? T("touchée") : beaten ? T("Prime !") : taken ? T("en chasse") : T("à prendre");
    std::string help = T("Récompense : ") + rewardText(b) + ". " + T("Lieu : ") + tr(jget<std::string>(b, "lieu", "")) + ". " +
                       tr(jget<std::string>(b, "description", ""));
    MenuItem it{stars(jget(b, "etoiles", 1)) + " " + tr(jget<std::string>(b, "nom", id)), state, help, true, [this, id, taken, beaten, done] {
                  int at = menus.top().sel;
                  if (done) return;
                  if (beaten) {
                    menus.pop();
                    claimBounty(id);
                    return;
                  }
                  if (taken) return;
                  flags.insert(bountyFlag(id, 0));
                  audio::play("niveau");
                  const Json* b = findBounty(id);
                  notice(T("Prime acceptée : ") + tr(jget<std::string>(*b, "lieu", "")) + ".");
                  menus.pop();
                  bountyMenu(at);
                }};
    it.shrink = true;
    if (done) it.color = 0xaab3d8;
    if (beaten && !done) it.color = 0xffd75a;
    m.items.push_back(it);
  }
  if (hidden > 0) m.items.push_back({T("D'autres primes viendront…"), "", T("De nouveaux avis seront affichés quand vous irez plus loin."), false, nullptr});
  if (m.items.empty()) m.items.push_back({T("(aucune prime)"), "", "", false, nullptr});
  m.sel = std::clamp(sel, 0, (int)m.items.size() - 1);
  menus.push(m);
}

// Toucher une prime : or, objets, puis les récompenses d'un nouveau rang
void Game::claimBounty(const std::string& id) {
  const Json* b = findBounty(id);
  if (!b || has(bountyFlag(id, 2)) || !has(bountyFlag(id, 1))) return;
  flags.insert(bountyFlag(id, 2));
  gold += jget(*b, "or", 0);
  for (auto& o : b->value("objets", Json::array())) {
    std::string it = jget<std::string>(o, "id", "");
    if (hasItem(it)) items[it] += jget(o, "quantite", 1);
  }
  sc.call([] { audio::play("victoire"); });
  sc.say(T("Prime touchée : ") + rewardText(*b) + " !");
  int n = bountiesClaimed();
  for (auto& r : bountyRanks())
    if (jget(r, "primes", 0) == n) {
      for (auto& o : r.value("objets", Json::array())) {
        std::string it = jget<std::string>(o, "id", "");
        if (hasItem(it)) items[it] += jget(o, "quantite", 1);
      }
      sc.call([] { audio::play("niveau"); });
      sc.say(T("Nouveau rang de chasseur : ") + tr(jget<std::string>(r, "titre", "")) + " ! " + T("Récompense : ") + rewardText(r) + ".");
    }
}
