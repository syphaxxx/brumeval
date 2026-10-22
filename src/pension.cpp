// La pension de Moulinbourg (2026-10-20, « 20 heures ») : comme la pension des Pokémon. On y laisse jusqu'à deux
// créatures (pas de héros) ; elles gagnent de l'expérience à chaque pas du joueur (règles « pension » : une montée
// de niveau tous les `pas_par_niveau` pas environ, jamais au-dessus du plus haut niveau de l'équipe). Deux créatures
// qui ont un type en commun laissent parfois un œuf (`pas_oeuf`, `chance_oeuf`) ; emporté, il éclot après
// `pas_eclosion` pas : la forme de base d'un des deux parents, au niveau 1, chromatique plus souvent (`chroma_oeuf`).
// Reprendre une créature coûte `prix_par_niveau` pièces par niveau gagné. Sauvegarde : lignes « pension_niveaux »,
// « pension_pas », « pension_oeuf », « oeuf », puis « pension » suivie des lignes « membre » des pensionnaires.
#include <algorithm>

#include "audio.hpp"
#include "data.hpp"
#include "game.hpp"
#include "i18n.hpp"

namespace {
struct PensionRules {
  int stepsPerLevel = 120, eggSteps = 300, hatchSteps = 400, pricePerLevel = 100, maxEggs = 3;
  float eggChance = .5f, eggChroma = .03f;
};
const PensionRules& prules() {
  static PensionRules r = [] {
    PensionRules p;
    Json d = readJson("regles.json");
    Json j = d.value("pension", Json::object());
    p.stepsPerLevel = std::max(1, jget(j, "pas_par_niveau", p.stepsPerLevel));
    p.eggSteps = std::max(1, jget(j, "pas_oeuf", p.eggSteps));
    p.hatchSteps = std::max(1, jget(j, "pas_eclosion", p.hatchSteps));
    p.pricePerLevel = jget(j, "prix_par_niveau", p.pricePerLevel);
    p.maxEggs = jget(j, "oeufs_max", p.maxEggs);
    p.eggChance = jget(j, "chance_oeuf", p.eggChance);
    p.eggChroma = jget(j, "chroma_oeuf", p.eggChroma);
    return p;
  }();
  return r;
}
bool shareType(const Species& a, const Species& b) {
  auto has = [](const Species& s, Type t) { return t >= 0 && (s.type == t || s.type2 == t); };
  return has(b, a.type) || has(b, a.type2);
}
}  // namespace

// La forme de base d'une espèce (celle dont on remonte les évolutions)
std::string baseForm(const std::string& id) {
  std::string cur = id;
  for (int guard = 0; guard < 8; guard++) {
    std::string prev;
    for (auto& s : allSpecies())
      if (s.evolveTo == cur) prev = s.id;
    if (prev.empty()) break;
    cur = prev;
  }
  return cur;
}

bool Game::pensionCanEgg() const {
  return pension_.size() == 2 && pension_[0] && pension_[1] && shareType(pension_[0]->S(), pension_[1]->S());
}

void Game::pensionStep() {
  if (inExpedition() || (pension_.empty() && eggs_.empty())) return;
  const PensionRules& R = prules();
  int cap = 1;
  for (auto& f : team) cap = std::max(cap, f->lvl);
  for (auto& f : pension_)
    if (f && f->lvl < cap) {
      gainXp(*f, std::max(1, f->need() / R.stepsPerLevel));
      if (f->lvl > cap) f->lvl = cap, f->xp = 0, f->recalc();
    }
  if (pensionCanEgg() && !pensionEgg_ && ++pensionSteps_ >= R.eggSteps) {
    pensionSteps_ = 0;
    if (frand() < R.eggChance) pensionEgg_ = true;
  }
  for (auto& e : eggs_) e.second--;
  for (size_t i = 0; i < eggs_.size(); i++)
    if (eggs_[i].second <= 0 && !sc.busy()) {  // éclosion
      std::string sp = eggs_[i].first;
      eggs_.erase(eggs_.begin() + (long)i);
      auto f = makeFighter(sp, 1);
      f->chroma = frand() < R.eggChroma;
      caught.insert(sp), seen.insert(sp);
      flags.insert("oeuf_eclos");
      sc.call([] { audio::play("capture"); });
      sc.say(T("Hein ? L'œuf bouge… Il éclot : ") + f->name() + (f->chroma ? T(" (couleurs rares !)") : "") + " !");
      if ((int)team.size() < rules().maxTeam) team.push_back(f);
      else {
        reserve.push_back(f);
        sc.say(f->name() + T(" va dans la réserve (l'équipe est complète)."));
      }
      break;
    }
}

void Game::pensionMenu(int sel) {
  const PensionRules& R = prules();
  Menu m;
  m.title = T("Pension de Moulinbourg");
  m.x = 8, m.y = 8, m.w = 240, m.sel = sel;
  for (size_t i = 0; i < pension_.size(); i++) {
    FighterP f = pension_[i];
    int gained = std::max(0, f->lvl - (i < pensionLvl_.size() ? pensionLvl_[i] : f->lvl)), price = gained * R.pricePerLevel;
    MenuItem it{T("Reprendre ") + f->name(), T("N.") + std::to_string(f->lvl) + (gained > 0 ? " (+" + std::to_string(gained) + ")" : ""),
                T("Elle revient dans l'équipe (ou la réserve si l'équipe est complète). Prix : ") + std::to_string(price) + T(" pièces d'or."),
                gold >= price, [this, i, price] {
                  FighterP f = pension_[i];
                  gold -= price;
                  pension_.erase(pension_.begin() + (long)i);
                  if (i < pensionLvl_.size()) pensionLvl_.erase(pensionLvl_.begin() + (long)i);
                  pensionSteps_ = 0;
                  if ((int)team.size() < rules().maxTeam) team.push_back(f);
                  else reserve.push_back(f);
                  audio::play("valider");
                  menus.pop();
                  pensionMenu();
                }};
    it.shrink = true;
    m.items.push_back(it);
  }
  if (pension_.size() < 2)
    m.items.push_back({T("Laisser une créature"), std::to_string(pension_.size()) + "/2",
                       T("Elle gagne de l'expérience à chacun de vos pas. Deux créatures d'un même type laissent parfois un œuf."), true,
                       [this] { pensionPick(); }});
  if (pensionEgg_)
    m.items.push_back({T("Prendre l'œuf"), "", T("Il éclora après quelques centaines de pas, dans l'équipe."), (int)eggs_.size() < R.maxEggs, [this] {
                         const FighterP& parent = pension_[(size_t)irand(0, (int)pension_.size() - 1)];
                         eggs_.push_back({baseForm(parent->sp), prules().hatchSteps});
                         pensionEgg_ = false;
                         menus.clear();
                         sc.call([] { audio::play("niveau"); });
                         sc.say(T("Vous recevez un œuf ! Marchez un peu : il éclora bientôt."));
                       }});
  std::string info = pensionCanEgg() ? T("Ces deux-là s'entendent bien.") : pension_.size() == 2 ? T("Ces deux-là s'ignorent (pas de type en commun).") : "";
  m.items.push_back({T("Partir"), eggs_.empty() ? "" : std::to_string(eggs_.size()) + T(" œuf(s)"), info, true, [this] { menus.pop(); }});
  m.onCancel = m.items.back().act;
  menus.push(m);
}

void Game::pensionPick() {
  Menu m;
  m.title = T("Laisser qui ?");
  m.x = 20, m.y = 20, m.w = 220, m.rows = 9;
  for (int pass = 0; pass < 2; pass++) {
    auto& list = pass == 0 ? team : reserve;
    for (size_t i = 0; i < list.size(); i++) {
      FighterP f = list[i];
      if (!f || f->S().human) continue;
      int left = 0;  // il faut garder au moins une créature dans l'équipe (compagnon des héros)
      for (auto& o : team) left += o != f && !o->S().human;
      bool ok = pass == 1 || left > 0;
      MenuItem it{f->name(), T("N.") + std::to_string(f->lvl) + (pass ? T(" (réserve)") : ""), typesName(f->S()), ok, [this, f, pass] {
                    auto& l = pass == 0 ? team : reserve;
                    l.erase(std::remove(l.begin(), l.end(), f), l.end());
                    pension_.push_back(f);
                    pensionLvl_.push_back(f->lvl);
                    pensionSteps_ = 0;
                    audio::play("valider");
                    menus.pop();
                    menus.pop();
                    pensionMenu();
                  }};
      it.shrink = true;
      m.items.push_back(it);
    }
  }
  if (m.items.empty()) m.items.push_back({T("(aucune créature)"), "", "", false, nullptr});
  m.onCancel = [this] { menus.pop(); };
  menus.push(m);
}
