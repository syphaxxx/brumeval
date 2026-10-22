// Discussions (2026-10-20, « 20 heures ») : de courtes conversations entre les héros, comme les saynètes des Tales of.
// data/discussions.json : {id, titre, si (condition), membres (espèces présentes dans l'équipe), dialogue}. Échap >
// Discussions les liste (les nouvelles en tête, marquées) ; en écouter une pose « discussion:<id> ». Une nouvelle
// discussion disponible est annoncée sur la carte (Game::checkDiscussions).
#include <algorithm>

#include "audio.hpp"
#include "data.hpp"
#include "game.hpp"
#include "i18n.hpp"

namespace {
const Json& talks() {
  static Json t = [] {
    Json d = readJson("discussions.json");
    return d.is_array() ? d : Json::array();
  }();
  return t;
}
}  // namespace

bool Game::discussionOpen(const Json& d) const {
  if (d.contains("si") && !checkCond(d["si"])) return false;
  for (auto& sp : jget(d, "membres", std::vector<std::string>{})) {
    bool in = false;
    for (auto& f : team) in = in || f->sp == sp;
    if (!in) return false;
  }
  return true;
}

int Game::newDiscussions() const {
  int n = 0;
  for (auto& d : talks())
    if (!has("discussion:" + jget<std::string>(d, "id", "")) && discussionOpen(d)) n++;
  return n;
}

void Game::checkDiscussions() {
  if (inExpedition()) return;
  int n = newDiscussions();
  if (n > lastNewTalks_) notice(T("Nouvelle discussion (Échap > Discussions)"));
  lastNewTalks_ = n;
}

void Game::discussionsMenu(int sel) {
  panelMode_ = 0;
  Menu m;
  m.title = T("Discussions");
  m.x = 8, m.y = 8, m.w = 230, m.rows = 9;
  for (int pass = 0; pass < 2; pass++)  // les nouvelles d'abord
    for (auto& d : talks()) {
      std::string id = jget<std::string>(d, "id", "");
      bool seen = has("discussion:" + id);
      if (seen != (pass == 1) || !discussionOpen(d)) continue;
      MenuItem it{tr(jget<std::string>(d, "titre", id)), seen ? "" : T("nouveau"), T("Écouter ce que se disent les héros."), true, [this, id] {
                    menus.clear();
                    playDiscussion(id);
                  }};
      if (seen) it.color = 0xaab3d8;
      it.shrink = true;
      m.items.push_back(it);
    }
  if (m.items.empty()) m.items.push_back({T("(aucune discussion)"), "", T("Les héros parlent entre eux au fil de l'aventure."), false, nullptr});
  m.sel = std::clamp(sel, 0, (int)m.items.size() - 1);
  m.onCancel = [this] {
    panelMode_ = 1;
    menus.pop();
  };
  menus.push(m);
}

void Game::playDiscussion(const std::string& id) {
  for (auto& d : talks()) {
    if (jget<std::string>(d, "id", "") != id) continue;
    flags.insert("discussion:" + id);
    lastNewTalks_ = newDiscussions();
    audio::play("valider");
    for (auto& l : jget(d, "dialogue", std::vector<std::string>{})) sc.say(tr(l));
    return;
  }
}

std::vector<std::string> checkDiscussionsData() {
  std::vector<std::string> err;
  for (auto& d : talks()) {
    std::string id = jget<std::string>(d, "id", "");
    if (id.empty()) err.push_back(T("data/discussions.json : discussion sans identifiant"));
    for (auto& sp : jget(d, "membres", std::vector<std::string>{}))
      if (!hasSpecies(sp)) err.push_back(T("data/discussions.json : espèce inconnue « ") + sp + " »");
    if (jget(d, "dialogue", std::vector<std::string>{}).empty()) err.push_back(T("data/discussions.json : discussion vide « ") + id + " »");
  }
  return err;
}
int discussionCount() { return (int)talks().size(); }
