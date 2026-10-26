// Forge de Forgeroc (2.1) : Brann renforce une arme ou une armure (+1, +2, +3) contre de l'or et
// des matériaux (minerai, cœur de braise, cristal de givre, trophées des boss). Les objets forgés
// sont générés au chargement des données (data.cpp) ; les coûts sont dans data/regles.json
// (« forge »). Méthodes de Game (game.hpp) ; ouverte par l'action d'événement « forge ».
#include <algorithm>

#include "i18n.hpp"
#include "audio.hpp"
#include "battle.hpp"
#include "game.hpp"

// Quantité possédée d'un matériau ; « trophee » : tous les trophées de boss réunis
int Game::forgeStock(const std::string& id) const {
  int n = 0;
  for (auto& d : allItems())
    if (id == "trophee" ? d.trophy : d.id == id) {
      auto it = items.find(d.id);
      if (it != items.end()) n += it->second;
    }
  return n;
}

static std::string matName(const std::string& id) { return id == "trophee" ? T("Trophée de boss") : item(id).name; }

// Ce qu'il faut pour forger le niveau suivant, et ce qui manque (vide : tout est là)
std::string Game::forgeCost(const ItemDef& d, std::string* missing) const {
  const auto& L = rules().forge[(size_t)d.forged];
  std::string s = std::to_string(L.gold) + T(" or"), miss;
  if (gold < L.gold) miss = std::to_string(L.gold - gold) + T(" or");
  for (auto& [id, n] : L.items) {
    int have = forgeStock(id);
    s += ", " + std::to_string(n) + " " + matName(id) + " (" + std::to_string(have) + ")";
    if (have < n) miss += (miss.empty() ? "" : ", ") + std::to_string(n - have) + " " + matName(id);
  }
  if (missing) *missing = miss;
  return s;
}

void Game::forgeMenu(int sel) {
  panelMode_ = 0;
  // Ce qu'on peut forger : l'équipement porté par l'équipe, puis celui du sac
  struct Entry {
    FighterP owner;
    std::string id;
  };
  std::vector<Entry> list;
  for (auto& f : team)
    for (int s : {G_WEAPON, G_ARMOR})
      if (!f->gear[s].empty() && hasItem(f->gear[s]) && item(f->gear[s]).slot == s) list.push_back({f, f->gear[s]});
  for (auto& d : allItems())
    if ((d.slot == G_WEAPON || d.slot == G_ARMOR) && items.count(d.id) && items.at(d.id) > 0) list.push_back({nullptr, d.id});
  Menu m;
  m.title = T("Forge  ·  ") + std::to_string(gold) + T(" or");
  m.x = 40, m.y = 20, m.w = 240, m.rows = 9, m.sel = sel;
  for (size_t i = 0; i < list.size(); i++) {
    const ItemDef& d = item(list[i].id);
    FighterP owner = list[i].owner;
    // (2026-10-26) deux groupes titrés au lieu de « sac » répété à chaque ligne ; le joueur automatique s'arrête au second
    if (i == 0 && owner) m.items.push_back(menuHeader(T("Équipement porté")));
    if (!owner && (i == 0 || list[i - 1].owner)) m.items.push_back(menuHeader(T("Dans le sac")));
    int n = owner ? 0 : items.at(d.id);
    std::string id = d.id, who = owner ? utf8Prefix(owner->name(), 7) : n > 1 ? "×" + std::to_string(n) : "";
    if (!forgeable(d)) {
      m.items.push_back({d.name, who, d.bonusText() + T(". Déjà au maximum (+") + std::to_string(d.forged) + ").", false, nullptr});
      continue;
    }
    const ItemDef& next = item(forgedId(d.forged ? d.base : d.id, d.forged + 1));
    std::string missing, cost = forgeCost(d, &missing);
    std::string help = T("En +") + std::to_string(next.forged) + " : " + next.bonusText() + T(" (au lieu de ") +
                       (d.bonusText().empty() ? T("rien") : d.bonusText()) + T("). Coût : ") + cost + "." +
                       (missing.empty() ? T(" Entrée : forger.") : T(" Il manque : ") + missing + ".");
    int at = (int)m.items.size();
    // Entrée : forge tout de suite (comme un achat en boutique ; le coût est dans l'aide en bas)
    MenuItem it{d.name, who, help, missing.empty(), [this, owner, id, at] {
                  forge(owner, id);
                  menus.pop();
                  forgeMenu(at);
                }};
    it.shrink = true;
    m.items.push_back(it);
  }
  if (m.items.empty()) m.items.push_back({T("(rien à forger)"), "", T("Achetez une arme ou une armure : Brann pourra la renforcer."), false, nullptr});
  m.sel = std::clamp(sel, 0, (int)m.items.size() - 1);
  menus.push(m);
}

// Forge l'objet id (porté par owner, sinon pris dans le sac) au niveau suivant : paie or et matériaux
bool Game::forge(const FighterP& owner, const std::string& id) {
  if (!hasItem(id) || !forgeable(item(id))) return false;
  const ItemDef& d = item(id);
  std::string missing;
  forgeCost(d, &missing);
  if (!missing.empty()) return false;
  const auto& L = rules().forge[(size_t)d.forged];
  std::string next = forgedId(d.forged ? d.base : d.id, d.forged + 1);
  gold -= L.gold;
  for (auto& [mat, n] : L.items) {
    int left = n;
    for (auto& m : allItems())  // « trophee » : les trophées dans l'ordre de la liste des objets
      if (left > 0 && (mat == "trophee" ? m.trophy : m.id == mat) && items.count(m.id)) {
        int take = std::min(left, items[m.id]);
        items[m.id] -= take, left -= take;
      }
  }
  if (owner) {
    owner->gear[d.slot] = next;
    owner->recalc();
  } else {
    items[id]--;
    items[next]++;
  }
  audio::play("coup_lourd");
  notice(T("Brann forge : ") + item(next).name + " !");
  return true;
}
