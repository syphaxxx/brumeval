#include "i18n.hpp"
#include "battle.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

#include "audio.hpp"
#include "game.hpp"
#include "sprites.hpp"
#include "tactics.hpp"

static const Color WHITE = rgb(0xffffff), GOLD = rgb(0xffe066), GREY = rgb(0xb9c0de), GREEN = rgb(0x7dffa8), BLUE = rgb(0x8fd0ff),
                   RED = rgb(0xff8a7a);

// ---------------------------------------------------------------------------
// Textes d'aide
// ---------------------------------------------------------------------------
static std::string pct(double x) { return std::to_string(int(std::lround(x * 100))) + " %"; }
// 0.75 → « 0,75 », 2 → « 2 »
static std::string numFr(double x) {
  char b[32];
  std::snprintf(b, sizeof b, "%.2f", x);
  std::string s = b;
  while (s.back() == '0') s.pop_back();
  if (s.back() == '.') s.pop_back();
  std::replace(s.begin(), s.end(), '.', ',');
  return s;
}

// Effet secondaire, avec qui il touche : « Brûlure (20 %) », « Défense −1 de la cible »
static std::string effectText(const Effect& e, const Move& m) {
  bool onAllies = m.target == Target::Ally || m.target == Target::AllAllies || m.target == Target::AllyKO;
  std::string who = e.self ? T("du lanceur") : m.target == Target::AllAllies ? T("de l'équipe") : onAllies ? T("de l'allié")
                    : m.target == Target::AllEnemies || m.zone != Area::Single ? T("des ennemis touchés") : T("de la cible");
  std::string s;
  if (e.cure) s = T("guérit les états");
  if (e.status != Status::None) s = statusName(e.status);
  if (e.stat >= 0) s = std::string(stageName(e.stat)) + (e.stages > 0 ? " +" : " −") + std::to_string(std::abs(e.stages)) + " " + who;
  if (e.chance < 100) s += " (" + std::to_string(e.chance) + T(" % de chance)");
  return s;
}

// Tout ce que fait une technique, en clair : genre, type, puissance, coût, cible et zone, portée,
// effets, rythme. user : le lanceur, pour chiffrer un soin.
std::string moveDetails(const Move& m, const Fighter* user) {
  const Rules& R = rules();
  static const char* KIND[] = {"Physique", "Magique", "Soin", "Réanimation", "Statut"};
  std::string s = std::string(T(KIND[(int)m.kind])) + " · " + typeName(m.type);
  if (m.damaging()) s += T(" · Puissance ") + std::to_string(m.power);
  s += m.cost > 0 ? " · " + std::to_string(m.cost) + T(" PM") : std::string(T(" · Gratuite"));
  if (m.acc < 100) s += T(" · Précision ") + std::to_string(m.acc) + " %";
  if (m.critBonus > 0) s += T(" · Critique +") + std::to_string(m.critBonus) + " %";
  // Cible et zone, portée
  std::string side = m.damaging() ? ", " + pct(R.zoneSide) + T(" autour") : "";
  switch (m.target) {
    case Target::Enemy:
      switch (m.zone) {
        case Area::Line: s += T(". Un ennemi et toute sa ligne (de l'avant à l'arrière") + side + ")"; break;
        case Area::Column: s += T(". Un ennemi et toute sa colonne (de haut en bas") + side + ")"; break;
        case Area::Cross: s += T(". Un ennemi et ses voisins en croix") + (side.empty() ? "" : " (" + side.substr(2) + ")"); break;
        case Area::All: s += T(". Tous les ennemis"); break;
        case Area::Single: s += T(". Un ennemi"); break;
      }
      break;
    case Target::AllEnemies: s += T(". Tous les ennemis, sans viser"); break;
    case Target::Ally: s += T(". Un allié"); break;
    case Target::AllAllies: s += T(". Toute l'équipe"); break;
    case Target::AllyKO: s += T(". Un allié K.O."); break;
  }
  if (m.damaging() && m.target == Target::Enemy) s += m.melee ? T(", corps à corps (colonne ennemie la plus avancée)") : T(", à distance (toute case)");
  // Soins
  if (m.kind == Kind::Heal && m.power > 0)
    s += user ? T(". Rend environ ") + std::to_string(int(m.power * user->eMag() / 20) + 2) + T(" PV (Magie ") + std::to_string(int(user->eMag())) + ")"
              : T(". Rend Puissance ") + std::to_string(m.power) + T(" × Magie ÷ 20 PV");
  if (m.kind == Kind::Revive) s += T(". Relève avec ") + std::to_string(m.power) + T(" % des PV");
  if (!m.effects.empty()) {
    s += T(". Effet : ");
    for (size_t i = 0; i < m.effects.size(); i++) s += (i ? ", " : "") + effectText(m.effects[i], m);
  }
  if (m.shove > 0) s += T(". Pousse la cible d'une case vers l'arrière (case prise ou bord : choc pour les deux)");
  if (m.shove < 0) s += T(". Tire la cible d'une case vers l'avant (case prise : choc pour les deux)");
  if (m.pace == Pace::Quick) s += T(". Rapide : rejoue plus tôt (jauge à ") + std::to_string(R.quickGauge) + " %)";
  if (m.pace == Pace::Heavy) s += T(". Lourde : rejoue plus tard (jauge à −") + std::to_string(R.heavyDelay) + " %)";
  return s + ".";
}

std::string moveHelp(const Move& m, const Fighter* user) { return (m.desc.empty() ? "" : m.desc + " ") + moveDetails(m, user); }

std::string moveBrief(const Move& m, const Fighter* user) {
  const Rules& R = rules();
  static const char* KIND[] = {"Physique", "Magique", "Soin", "Réanimation", "Statut"};
  std::vector<std::string> v;
  v.push_back(std::string(T(KIND[(int)m.kind])) + " " + typeName(m.type));
  if (m.damaging()) v.push_back(T("Puiss. ") + std::to_string(m.power));
  v.push_back(m.cost > 0 ? std::to_string(m.cost) + T(" PM") : std::string(T("Gratuit")));
  if (m.acc < 100) v.push_back(T("Préc. ") + std::to_string(m.acc) + " %");
  if (m.critBonus > 0) v.push_back(T("Crit. +") + std::to_string(m.critBonus) + " %");
  if (m.damaging() && m.target == Target::Enemy && m.zone != Area::All) v.push_back(m.melee ? T("Mêlée") : T("Distance"));
  std::string side = m.damaging() ? T(" (voisins ") + pct(R.zoneSide) + ")" : "";
  switch (m.target) {
    case Target::Enemy:
      switch (m.zone) {
        case Area::Line: v.push_back(T("Ligne") + side); break;
        case Area::Column: v.push_back(T("Colonne") + side); break;
        case Area::Cross: v.push_back(T("Croix") + side); break;
        case Area::All: v.push_back(T("Tous les ennemis")); break;
        case Area::Single: break;
      }
      break;
    case Target::AllEnemies: v.push_back(T("Tous les ennemis")); break;
    case Target::Ally: v.push_back(T("1 allié")); break;
    case Target::AllAllies: v.push_back(T("Toute l'équipe")); break;
    case Target::AllyKO: v.push_back(T("1 allié K.O.")); break;
  }
  if (m.kind == Kind::Heal && m.power > 0)
    v.push_back(user ? T("Rend ≈") + std::to_string(int(m.power * user->eMag() / 20) + 2) + T(" PV") : T("Rend Puiss. × Magie ÷ 20 PV"));
  if (m.kind == Kind::Revive) v.push_back(T("Relève à ") + std::to_string(m.power) + T(" % des PV"));
  bool onAllies = m.target == Target::Ally || m.target == Target::AllAllies || m.target == Target::AllyKO;
  for (auto& e : m.effects) {
    std::string s;
    if (e.cure) s = T("Guérit les états");
    if (e.status != Status::None) s = statusName(e.status);
    if (e.stat >= 0)
      s = std::string(stageName(e.stat)) + (e.stages > 0 ? " +" : " −") + std::to_string(std::abs(e.stages)) +
          (e.self ? T(" lanceur") : m.target == Target::AllAllies ? T(" équipe") : onAllies ? T(" allié") : T(" cible"));
    if (e.chance < 100) s += " " + std::to_string(e.chance) + " %";
    v.push_back(s);
  }
  if (m.shove > 0) v.push_back(T("Repousse d'1 case"));
  if (m.shove < 0) v.push_back(T("Attire d'1 case"));
  if (m.pace == Pace::Quick) v.push_back(T("Rapide (jauge ") + std::to_string(R.quickGauge) + " %)");
  if (m.pace == Pace::Heavy) v.push_back(T("Lourde (jauge −") + std::to_string(R.heavyDelay) + " %)");
  std::string s;
  for (auto& x : v) s += (s.empty() ? "" : " · ") + x;
  return s;
}

// Ce que fait un objet, chiffré : soins, réanimation, capture, bonus d'équipement, où l'utiliser
std::string itemDetails(const ItemDef& d) {
  const Rules& R = rules();
  std::vector<std::string> v;
  if (d.gem) {  // gemme (2.7)
    std::string g = T("Gemme à sertir (Échap > Équipement > Gemmes)");
    if (!d.gemMove.empty() && hasMove(d.gemMove)) g += T(" : donne ") + moveInfo(d.gemMove).name;
    for (int k = 0; k < N_GEAR_STATS; k++)
      if (d.gemBonus[k] != 0) g += std::string(" : +") + std::to_string(int(std::lround(d.gemBonus[k] * 100))) + " % " + gearStatName(k);
    if (d.gemSupport == "vol_vie") g += T(" : rend ") + std::to_string(int(std::lround(d.gemValue * 100))) + T(" % des dégâts infligés en PV");
    if (d.gemSupport == "contre") g += " : " + std::to_string(int(std::lround(d.gemValue * 100))) + T(" % de chances de riposter au corps à corps");
    if (d.gemSupport == "eveil") g += T(" : jauge ATB avancée de ") + std::to_string(int(d.gemValue)) + T(" % au début du combat");
    if (d.gemSupport == "briseur") g += T(" : une faiblesse retire ") + std::to_string(int(d.gemValue)) + T(" point de bouclier de plus");
    v.push_back(g);
  }
  if (d.slot >= 0) {
    std::string b = d.bonusText();
    v.push_back(std::string(gearSlotName(d.slot)) + (b.empty() ? "" : " : " + b) + (d.slot != G_ACCESSORY ? T(" (héros seulement)") : ""));
  }
  if (d.healHp) v.push_back(d.healHp >= 999 ? T("Rend tous les PV") : T("Rend ") + std::to_string(d.healHp) + T(" PV"));
  if (d.healMp) v.push_back(d.healMp >= 999 ? T("Rend tous les PM") : T("Rend ") + std::to_string(d.healMp) + T(" PM"));
  if (d.revive) v.push_back(T("Relève un allié K.O. avec ") + std::to_string(d.revive) + T(" % de ses PV"));
  if (d.cure) v.push_back(T("Guérit poison, brûlure, paralysie et sommeil"));
  if (d.capture > 0) {
    double lo = std::min(R.capMax, R.capBase * d.capture), hi = std::min(R.capMax, (R.capBase + R.capHp) * d.capture);
    v.push_back(T("Capture : ") + pct(lo) + T(" (pleine forme) à ") + pct(hi) + T(" (presque K.O.)"));
  }
  for (size_t L = 0; L < R.forge.size(); L++)  // matériau de la forge
    for (auto& [id, n] : R.forge[L].items)
      if (id == d.id || (id == "trophee" && d.trophy))
        v.push_back(T("Forge de Forgeroc : ") + std::to_string(n) + T(" pour un objet +") + std::to_string(L + 1));
  for (auto& td : R.typeDrops)
    if (td.item == d.id) {
      std::string tn = td.type;
      for (auto& t : types())
        if (t.id == td.type) tn = t.name;
      v.push_back(T("Butin des créatures ") + tn + " (" + pct(td.chance) + ")");
    }
  if (d.slot >= 0 && forgeable(d)) v.push_back(T("Peut être renforcé à la forge de Forgeroc (+") + std::to_string(d.forged + 1) + ")");
  if (d.key && d.trophy) v.push_back(T("Trophée"));
  else if (d.key) v.push_back(T("Objet important"));
  else if (d.slot < 0 && (d.battle || d.field))
    v.push_back(d.battle && d.field ? T("Combat et menu") : d.battle ? T("Combat seulement") : T("Menu seulement"));
  std::string s;
  for (auto& x : v) s += (s.empty() ? "" : ". ") + x;
  return s.empty() ? s : s + ".";
}

// Forces et faiblesses d'une espèce : efficacité de chaque type d'attaque (ses deux types et ses
// résistances propres), résistances aux coups physiques ou à la magie, états impossibles
std::string matchupText(const Species& sp, const std::set<std::string>* known) {
  std::map<float, std::string, std::greater<float>> by;
  auto r = [&](const std::string& k) {
    auto it = sp.resist.find(k);
    return it == sp.resist.end() ? 1.f : it->second;
  };
  for (Type t = 0; t < (Type)types().size(); t++) {
    float e = typeEff(t, sp.type) * (sp.type2 >= 0 ? typeEff(t, sp.type2) : 1.f) * r(types()[t].id);
    if (e != 1) by[e] += (by[e].empty() ? "" : ", ") + types()[t].name;
  }
  if (known) {  // Brisure : seulement les faiblesses découvertes en combat
    std::string found;
    int hidden = 0;
    for (Type t = 0; t < (Type)types().size(); t++) {
      float e = typeEff(t, sp.type) * (sp.type2 >= 0 ? typeEff(t, sp.type2) : 1.f) * r(types()[t].id);
      if (e <= 1) continue;
      if (known->count(sp.id + ":" + std::to_string(t))) found += (found.empty() ? "" : ", ") + std::string(types()[t].name) + T(" (×") + numFr(e) + ")";
      else hidden++;
    }
    std::string s = T("Faiblesses découvertes : ") + (found.empty() ? std::string("aucune") : found) + ".";
    if (hidden) s += T(" Encore ") + std::to_string(hidden) + T(" à découvrir (frappez-la avec d'autres types).");
    return s;
  }
  std::string weak, strong, none;
  for (auto& [e, names] : by) {
    std::string mult = T(" (×") + numFr(e) + ")";
    if (e == 0) none = names;
    else if (e > 1) weak += (weak.empty() ? "" : ", ") + names + mult;
    else strong += (strong.empty() ? "" : ", ") + names + mult;
  }
  std::string s = T("Faible contre : ") + (weak.empty() ? std::string("rien") : weak) + ".";
  if (!strong.empty()) s += T(" Résiste : ") + strong + ".";
  if (!none.empty()) s += T(" Sans effet : ") + none + ".";
  if (r("physique") != 1) s += std::string(r("physique") < 1 ? T(" Encaisse") : T(" Craint")) + T(" les coups physiques (×") + numFr(r("physique")) + ").";
  if (r("magique") != 1) s += std::string(r("magique") < 1 ? T(" Encaisse") : T(" Craint")) + T(" la magie (×") + numFr(r("magique")) + ").";
  std::string imm;
  for (Status st : {Status::Poison, Status::Burn, Status::Paralysis, Status::Sleep})
    if (immuneTo(sp, st)) imm += (imm.empty() ? "" : ", ") + std::string(statusName(st));
  if (!imm.empty()) s += T(" Insensible : ") + imm + ".";
  return s;
}

std::string itemHelp(const ItemDef& d) {
  std::string s = itemDetails(d);
  return d.desc.empty() ? s : s.empty() ? d.desc : s + " " + d.desc;
}

float captureChance(const Fighter& f, float mult) {
  if (f.S().human) return 0;
  const Rules& R = rules();
  return std::min(float(R.capMax), float(R.capBase + R.capHp * (1 - double(f.hp) / f.mhp)) * mult);
}

// ---------------------------------------------------------------------------
Battle::Battle(Game& game, BattleSetup s, Theme bg_)
    : G(game), foes(std::move(s.foes)), reserve(std::move(s.reserve)), foeName(std::move(s.foeName)), boss(s.boss), canFlee(s.canFlee),
      canCapture(s.canCapture), bg(bg_) {
  allies = s.allies.empty() ? G.front() : std::move(s.allies);
  for (auto& a : allies) hasHero_ = hasHero_ || a->S().human;
  place(allies, !coop());  // la formation de l'équipe (au gardien à plusieurs : placement automatique)
  place(foes, false);
  // Ennemis : quelques combattants au milieu plutôt qu'à l'avant, pour varier les formations
  for (auto& f : foes) {
    int c = cells_[f.get()], mid = cellAt(cellRow(c), 1);
    bool taken = false;
    for (auto& o : foes) taken = taken || cells_[o.get()] == mid;
    if (cellCol(c) == 0 && !f->boss && !taken && frand() < .35f) cells_[f.get()] = mid;
  }
  start = G.time;
  std::map<std::string, int> cnt, seen;
  for (auto* v : {&foes, &reserve})
    for (auto& e : *v) cnt[e->sp]++;
  for (auto* v : {&foes, &reserve})
    for (auto& e : *v)
      if (cnt[e->sp] > 1) e->tag = std::string(1, char('A' + seen[e->sp]++));
  roster_ = foes;
  roster_.insert(roster_.end(), reserve.begin(), reserve.end());
  for (auto& a : G.team) a->clearBattle();
  for (auto& a : allies) a->clearBattle();
  for (auto* v : {&foes, &reserve})
    for (auto& e : *v) e->clearBattle();
  // Brisure : bouclier de chaque ennemi (le combat, sinon l'espèce, sinon les règles)
  for (auto* v : {&foes, &reserve})
    for (auto& e : *v) initFoe(e);
  // Météo et terrain (2.6) : tirés selon le décor du combat
  for (auto& side : terrain_) side.fill(-1);
  {
    const Rules& R = rules();
    std::string th = themeName(bg);
    float r = frand();
    for (size_t i = 0; i < R.weathers.size() && weather_ < 0; i++) {
      const auto& w = R.weathers[i];
      if (std::find(w.themes.begin(), w.themes.end(), th) == w.themes.end()) continue;
      if (r < w.chance) weather_ = (int)i;
      r -= float(w.chance);
    }
    std::vector<int> pool;
    for (size_t i = 0; i < R.terrains.size(); i++)
      if (std::find(R.terrains[i].themes.begin(), R.terrains[i].themes.end(), th) != R.terrains[i].themes.end()) pool.push_back((int)i);
    if (!pool.empty())
      for (auto& side : terrain_) {
        int n = irand(R.terrainMin, std::max(R.terrainMin, R.terrainMax));
        for (int k = 0; k < n; k++) side[(size_t)irand(0, N_CELLS - 1)] = pool[(size_t)irand(0, (int)pool.size() - 1)];
      }
  }
  for (auto& a : allies) a->atb = 20 + frand() * 50;
  for (auto& e : foes) e->atb = frand() * 30;
  // Talents de début de combat : Vif (jauge avancée), Intimidation (Attaque des adversaires −1)
  for (auto* side : {&allies, &foes})
    for (auto& f : *side) {
      f->atb = std::min(95.f, f->atb + float(talentValue(f->S(), "vif") + gemSupport(*f, "eveil")));  // talent Vif, gemme d'éveil
      if (talentValue(f->S(), "intimidation") > 0)
        for (auto& o : side == &allies ? foes : allies) o->stage[S_ATK] = std::max(-3, o->stage[S_ATK] - 1);
    }
  G.menus.clear();
  std::string intro;
  FighterP leader = foes[0];
  for (auto& e : foes)
    if (e->boss) leader = e;
  if (!foeName.empty()) intro = foeName + T(" vous défie !");
  else if (leader->S().human) intro = leader->name() + T(" engage le combat !");
  else if (boss) intro = leader->name() + T(" surgit devant vous !");
  else if (foes.size() > 1) intro = std::to_string(foes.size()) + T(" créatures sauvages surgissent !");
  else intro = leader->name() + T(" sauvage apparaît !");
  sc.say(intro, 1.3f);
}

bool Battle::isAlly(const FighterP& f) const { return std::find(allies.begin(), allies.end(), f) != allies.end(); }
std::vector<FighterP> Battle::alive(const std::vector<FighterP>& v) const {
  std::vector<FighterP> o;
  for (auto& f : v)
    if (f->alive()) o.push_back(f);
  return o;
}

// Damier à l'écran (3.2.1) : les alliés à gauche, les ennemis à droite ; l'avant de chaque camp est du
// côté du milieu de l'écran. Les lignes du bas sont un peu décalées (léger effet de profondeur).
Pt Battle::cellPos(bool allySide, int c) const {
  if (c < 0 || c >= N_CELLS) c = cellAt(1, 1);
  float col = (float)cellCol(c), row = (float)cellRow(c);
  float x = 196 + col * 30 + row * 4;  // 4 colonnes de 30 px, chaque ligne un peu décalée (perspective)
  return {allySide ? 320 - x : x, 72 + row * 22};
}
Pt Battle::pos(const FighterP& f) const { return cellPos(isAlly(f), cellOf(f)); }

int Battle::cellOf(const FighterP& f) const {
  auto it = f ? cells_.find(f.get()) : cells_.end();
  return it == cells_.end() ? -1 : it->second;
}
void Battle::place(const std::vector<FighterP>& v, bool keep) {
  auto cells = formation(v, keep);
  for (size_t i = 0; i < v.size(); i++) cells_[v[i].get()] = cells[i];
}
bool Battle::reachable(const FighterP& a, const Move& m, const FighterP& t) const {
  (void)a;
  if (!m.melee || m.zone == Area::All || !t) return true;
  int front = BACK_COL;
  for (auto& f : sideOf(t))
    if (f->alive()) front = std::min(front, std::max(0, cellCol(std::max(0, cellOf(f)))));
  return std::max(0, cellCol(std::max(0, cellOf(t)))) <= front;
}
std::vector<FighterP> Battle::zoneTargets(const Move& m, const FighterP& center) const {
  std::vector<FighterP> v{center};
  if (!center || m.zone == Area::Single || m.zone == Area::All) return v;
  int c = cellOf(center);
  for (auto& f : sideOf(center))
    if (f != center && f->alive() && inZone(m.zone, c, cellOf(f))) v.push_back(f);
  return v;
}
float Battle::zoneScore(const FighterP& a, const Move& m, const FighterP& center) const {
  float s = 0;
  for (auto& t : zoneTargets(m, center)) s += attackScore(a, m, t) * (t == center ? 1.f : float(rules().zoneSide));
  return s;
}
// Nombre ou texte flottant au-dessus d'un combattant ; ils s'empilent s'ils apparaissent ensemble
void Battle::pop(const FighterP& f, const std::string& t, Color c) {
  Pt p = pos(f);
  int stack = 0;
  for (auto& q : pops)
    if (q.x == p.x && G.time - q.t < .3f) stack++;
  pops.push_back({p.x, p.y - 22 - 11.f * stack, t, c, G.time});
  if (net_ == Net::Host || net_ == Net::Lead) send_(-1, Json{{"t", "pop"}, {"f", mine(f)}, {"txt", t}, {"c", Json::array({c.r, c.g, c.b})}});
}

bool Battle::shown() const { return G.battle_.get() == this; }

// Menus du combat : en bas de l'écran, pour laisser les deux damiers visibles (zones en or).
// Le menu de commande à gauche, ses sous-menus et le choix de la cible à droite.
// Le bas de l'écran est partagé : le menu de commande à gauche, l'ATH (les alliés) à droite.
// Les autres menus s'ouvrent juste au-dessus : à droite (techniques, cible ennemie : le damier
// ennemi reste visible) ou à gauche (cible alliée, se déplacer : le damier allié reste visible).
// Panneau du bas (3.2.1) : rien ne s'ouvre au-dessus du damier. Le menu de commande à gauche (5 lignes, il défile),
// les sous-menus (techniques, objets, remplaçants…) à droite, à la place de l'ATH le temps du choix.
enum class Dock { Command, Above, AboveLeft };
static const int PANEL_Y = 168;  // haut du panneau du bas
static void dock(Menu& t, Dock d = Dock::Above) {
  if (d == Dock::Command) {
    t.rows = std::min(t.rows, 5);
    t.x = 4, t.w = 96, t.y = 238 - (std::min(t.rows, (int)t.items.size()) * 12 + 8);
    return;
  }
  int th = t.title.empty() ? 0 : 13;
  t.rows = std::min(t.rows, (238 - PANEL_Y - 8 - th) / 12);
  t.x = 102, t.w = 214, t.y = 238 - (std::min(t.rows, (int)t.items.size()) * 12 + 8 + th);
}

static Battle::Fx fxOf(const Move& m) {
  switch (m.kind) {
    case Kind::Physical: return Battle::Fx::Hit;
    case Kind::Magic: return Battle::Fx::Magic;
    case Kind::Heal:
    case Kind::Revive: return Battle::Fx::Heal;
    case Kind::Status: return Battle::Fx::Status;
  }
  return Battle::Fx::Hit;
}

void Battle::startAnim(const FighterP& a, const std::vector<FighterP>& targets, Fx fx, Color col, bool limit, int type) {
  anim_ = Anim{a, targets, fx, col, limit, G.time, type};
  if (net_ == Net::Host || net_ == Net::Lead) {
    Json c = Json::array();
    for (auto& f : targets) c.push_back(mine(f));
    send_(-1, Json{{"t", "anim"}, {"a", mine(a)}, {"c", c}, {"fx", (int)fx}, {"col", Json::array({col.r, col.g, col.b})}, {"l", limit}, {"ty", type}});
  }
}

// Élan : le lanceur avance vers ses adversaires (coup) ou fait un petit bond (sort, soin) ; le
// combattant touché tremble un instant
Pt Battle::offset(const FighterP& f) const {
  Pt o{0, 0};
  float e = G.time - anim_.t0;
  if (anim_.actor == f && e >= 0 && e < .4f) {
    float k = std::sin(3.14159f * e / .4f);
    if (anim_.fx == Fx::Hit) o.x = (isAlly(f) ? 16.f : -16.f) * k;  // vers le camp adverse
    else o.y = -6 * k;
  }
  if (blink == f && G.time - blinkT < .3f) o.x += std::sin((G.time - blinkT) * 70) * 2.5f;
  if (G.time - shakeT_ < .35f) o.y += std::sin((G.time - shakeT_) * 90) * 2.f * (1 - (G.time - shakeT_) / .35f);  // tremblement (brisure)
  return o;
}

// Particules propres au type : k va de 0 (début) à 1 (fin) ; p est le centre de la cible
// Effets dessinés d'après la planche des sorts (3.2) : une suite d'images par type, jouée sur la cible pendant
// l'effet (k de 0 à 1). Faux si ce type n'en a pas (les particules en code restent).
static bool spriteFx(Gfx& g, const std::string& type, const Pt& p, float k, bool heal) {
  static const std::map<std::string, std::vector<const char*>> FX = {
      {"feu", {"fx_feu_1", "fx_feu_2", "fx_feu_3"}},
      {"glace", {"fx_glace_1", "fx_glace_2", "fx_glace_cristal"}},
      {"foudre", {"fx_eclair_2", "fx_eclair_1", "fx_eclair_pilier"}},
      {"vent", {"fx_vent_1", "fx_vent_2", "fx_vent_tornade"}},
      {"eau", {"fx_eau_1", "fx_eau_2", "fx_eau_3"}},
      {"poison", {"fx_poison_2", "fx_poison_4", "fx_poison_5", "fx_poison_nuage"}},
      {"plante", {"fx_soin_1", "fx_regen"}},
      {"roche", {"fx_terre_1", "fx_terre_2", "fx_terre_pics"}},
      {"ombre", {"fx_ombre_1", "fx_ombre_3", "fx_ombre_4", "fx_ombre_5"}},
      {"esprit", {"fx_ombre_2", "fx_teleport"}},
      {"lumiere", {"fx_revelation"}},
      {"soin", {"fx_soin_1", "fx_soin_2", "fx_soin_3", "fx_soin_4"}},
  };
  auto it = FX.find(heal ? "soin" : type);
  if (it == FX.end()) return false;
  const auto& v = it->second;
  const SpriteRect* r = findSprite(v[std::min(v.size() - 1, size_t(k * v.size()))]);
  if (!r) return false;
  Color c{255, 255, 255, uint8_t(255 * std::clamp((1 - k) * 2.5f, 0.f, 1.f))};
  g.sprite(*r, std::round(p.x - r->w / 2.f), std::round(p.y + 12 - r->h), false, 1, c);
  return true;
}

void Battle::drawTypeFx(const Pt& p, float k) {
  Gfx& g = G.g;
  const std::string& t = types()[anim_.type].id;
  if (spriteFx(g, t, p, k, false)) return;
  auto col = [&](uint32_t c, float a) {
    Color x = rgb(c);
    x.a = uint8_t(std::clamp(a, 0.f, 1.f) * 255);
    return x;
  };
  float a = 1 - k;
  if (t == "feu") {  // flammes qui montent
    for (int i = 0; i < 6; i++) {
      float x = p.x - 12 + i * 5, h = 10 + 8 * std::sin(i * 1.7f + k * 9), y = p.y + 8 - k * 14;
      g.tri(x - 3, y, x + 3, y, x, y - h, col(i % 2 ? 0xff8a2a : 0xffd34d, a));
    }
  } else if (t == "eau") {  // gerbe de gouttes
    for (int i = 0; i < 9; i++) {
      float ang = -2.6f + i * .26f, r = 4 + k * 22;
      g.ellipse(p.x + std::cos(ang) * r, p.y + std::sin(ang) * r + k * k * 18, 1.6f, 2.4f, col(i % 2 ? 0x7fc8ff : 0xe0f4ff, a));
    }
  } else if (t == "plante" || t == "poison") {  // feuilles qui tourbillonnent, ou bulles
    for (int i = 0; i < 8; i++) {
      float ang = i * .785f + k * 6, r = 6 + k * 18;
      float x = p.x + std::cos(ang) * r, y = p.y + std::sin(ang) * r * .6f - (t == "poison" ? k * 16 : 0);
      if (t == "poison") g.ellipse(x, y, 2.5f, 2.5f, col(i % 2 ? 0xb06ae0 : 0xe0b0ff, a));
      else g.poly({{x - 3, y}, {x, y - 2}, {x + 3, y}, {x, y + 2}}, col(i % 2 ? 0x5fbf52 : 0xa8e070, a));
    }
  } else if (t == "foudre") {  // éclair qui tombe d'en haut
    float x = p.x, y = p.y - 40;
    for (int i = 0; i < 5; i++) {
      float nx = p.x + (i % 2 ? 6 : -6) * (1 - k), ny = y + 9;
      g.line(x, y, nx, ny, 2.5f, col(0xfff27a, a));
      g.line(x, y, nx, ny, 1, col(0xffffff, a));
      x = nx, y = ny;
    }
  } else if (t == "glace") {  // cristaux qui s'écartent
    for (int i = 0; i < 6; i++) {
      float ang = i * 1.047f, r = 5 + k * 16, x = p.x + std::cos(ang) * r, y = p.y + std::sin(ang) * r * .7f;
      g.poly({{x, y - 4}, {x + 2.5f, y}, {x, y + 4}, {x - 2.5f, y}}, col(i % 2 ? 0xbfe8ff : 0xffffff, a));
    }
  } else if (t == "roche") {  // pierres qui tombent
    for (int i = 0; i < 5; i++) {
      float x = p.x - 12 + i * 6, y = p.y - 30 + std::min(1.f, k * 2 + i * .1f) * 30;
      g.ellipse(x, y, 3, 2.5f, col(i % 2 ? 0x8a8278 : 0xb8b0a4, a + .3f));
    }
  } else if (t == "vent") {  // traînées
    for (int i = 0; i < 5; i++) {
      float y = p.y - 12 + i * 6, x0 = p.x - 20 + k * 30;
      g.line(x0, y, x0 + 14, y - 2, 1.5f, col(0xd8f8ec, a));
    }
  } else if (t == "ombre" || t == "esprit") {  // volutes sombres, ou orbes
    for (int i = 0; i < 6; i++) {
      float ang = i * 1.047f + k * 5, r = 14 - k * 6;
      g.ellipse(p.x + std::cos(ang) * r, p.y - 4 + std::sin(ang) * r * .5f, 3.5f, 2.5f, col(t == "ombre" ? 0x4a2a6a : 0xc8a8ff, a));
    }
  } else if (t == "lumiere") {  // rayon
    g.rect(p.x - 4 * a, p.y - 50, 8 * a, 60, col(0xfff8d0, a * .8f));
  } else if (t == "metal") {  // étincelles
    for (int i = 0; i < 10; i++) {
      float ang = i * .628f, r0 = 3 + k * 12, r1 = r0 + 4;
      g.line(p.x + std::cos(ang) * r0, p.y + std::sin(ang) * r0, p.x + std::cos(ang) * r1, p.y + std::sin(ang) * r1, 1, col(0xe8f0ff, a));
    }
  }
}

void Battle::drawAnim() {
  Gfx& g = G.g;
  float e = G.time - anim_.t0 - .2f;  // l'effet arrive sur la cible juste après l'élan
  if (e < 0 || e > .6f) return;
  float k = e / .6f;  // 0 au début, 1 à la fin
  auto fade = [](Color c, float a) {
    c.a = uint8_t(std::clamp(a, 0.f, 1.f) * 255);
    return c;
  };
  // Limite : une invocation (planche des sorts) se dresse au-dessus des cibles, selon le type de la technique
  if (anim_.limit && !anim_.targets.empty()) {
    std::string ty = anim_.type >= 0 ? types()[anim_.type].id : "";
    const char* who = ty == "feu" || ty == "foudre" ? "inv_dragon"
                      : ty == "eau" || ty == "glace" || ty == "esprit" || ty == "lumiere" ? "inv_esprit"
                      : ty == "plante" || ty == "roche" || ty == "metal" || ty == "poison" ? "inv_orc" : "inv_loup";
    if (const SpriteRect* r = findSprite(who)) {
      Pt c{0, 0};
      for (auto& f : anim_.targets) c.x += pos(f).x, c.y += pos(f).y;
      c.x /= anim_.targets.size(), c.y /= anim_.targets.size();
      float a = std::min({1.f, k * 4, (1 - k) * 3});
      g.sprite(*r, std::round(c.x - r->w / 2.f), std::round(c.y - 14 - r->h + (1 - std::min(1.f, k * 3)) * 10), false, 1,
               Color{255, 255, 255, uint8_t(230 * a)});
    }
  }
  for (auto& f : anim_.targets) {
    Pt p = pos(f);
    p.y -= 4;
    Color c = anim_.col, white = rgb(0xffffff);
    switch (anim_.fx) {
      case Fx::Hit: {
        for (int i = 0; i < 3; i++) {  // trois entailles qui apparaissent l'une après l'autre
          float a = std::clamp(k * 3 - i * .5f, 0.f, 1.f), x = -10 + i * 7.f;
          if (a <= 0) continue;
          g.line(p.x + x - 6, p.y - 14, p.x + x - 6 + 16 * a, p.y - 14 + 24 * a, 2, fade(white, 1.2f - k));
          g.line(p.x + x - 5, p.y - 13, p.x + x - 5 + 15 * a, p.y - 13 + 22 * a, 1, fade(c, 1.2f - k));
        }
        for (int i = 0; i < 8; i++) {  // éclat de l'impact
          float ang = i * .785f + .3f, r0 = 4 + k * 10, r1 = r0 + 6 * (1 - k);
          g.line(p.x + std::cos(ang) * r0, p.y + std::sin(ang) * r0, p.x + std::cos(ang) * r1, p.y + std::sin(ang) * r1, 1.5f,
                 fade(i % 2 ? c : white, 1 - k));
        }
        break;
      }
      case Fx::Magic:
        for (int ring = 0; ring < 2; ring++) {  // anneaux de la couleur du type qui s'élargissent
          float r = 4 + (k - ring * .25f) * 34;
          if (r < 4) continue;
          for (int i = 0; i < 16; i++) {
            float ang = i * .3927f + G.time * 3;
            g.rect(p.x + std::cos(ang) * r - 1, p.y + std::sin(ang) * r * .6f - 1, 2, 2, fade(i % 2 ? white : c, 1 - k));
          }
        }
        g.ellipse(p.x, p.y, 10 * (1 - k) + 2, 8 * (1 - k) + 2, fade(c, .7f * (1 - k)));
        break;
      case Fx::Heal:
      case Fx::Capture:
        if (anim_.fx == Fx::Heal && spriteFx(g, "", p, k, true)) break;
        for (int i = 0; i < 7; i++) {  // étincelles qui montent
          float x = std::sin(i * 2.4f) * 13, y = 12 - std::fmod(k * 40 + i * 6, 40.f);
          g.ellipse(p.x + x, p.y + y, 1.6f, 1.6f, fade(i % 2 ? white : c, 1 - k * .8f));
          g.rect(p.x + x - .5f, p.y + y - 3, 1, 6, fade(white, .6f * (1 - k)));
        }
        if (anim_.fx == Fx::Capture) g.ellipse(p.x, p.y, 18 * k + 4, 16 * k + 4, fade(rgb(0xfff0a0), .5f * (1 - k)));
        break;
      case Fx::Status:
        for (int i = 0; i < 6; i++) {  // tourbillon autour de la cible
          float ang = i * 1.047f + k * 9, r = 16 - k * 8;
          g.ellipse(p.x + std::cos(ang) * r, p.y - 6 + std::sin(ang) * r * .5f, 2, 2, fade(c, 1 - k));
        }
        break;
    }
    if (anim_.type >= 0 && (anim_.fx == Fx::Hit || anim_.fx == Fx::Magic)) drawTypeFx(p, k);
    if (anim_.limit)  // Limite : explosion, puis grande étoile dorée
      if (const SpriteRect* r = findSprite(k < .5f ? "fx_explosion" : "fx_impact"))
        g.sprite(*r, std::round(p.x - r->w / 2.f), std::round(p.y + 14 - r->h), false, 1, Color{255, 255, 255, uint8_t(255 * std::min(1.f, (1 - k) * 2))});
    if (anim_.limit)
      for (int i = 0; i < 12; i++) {
        float ang = i * .5236f, r = 6 + k * 30;
        g.line(p.x, p.y, p.x + std::cos(ang) * r, p.y + std::sin(ang) * r, 1.5f, fade(rgb(0xffd34d), 1 - k));
      }
  }
}
void Battle::sound(const std::string& id, bool share) {
  if (shown()) audio::play(id);
  if (share && (net_ == Net::Host || net_ == Net::Lead)) send_(-1, Json{{"t", "son"}, {"s", id}});
}

// ---------------------------------------------------------------------------
void Battle::tip(const char* id) {
  if (!shown() || autoPlay || net_ != Net::None) return;
  G.tip(id, sc);
}

// Une seule astuce au début d'un combat : la première règle nouvelle qu'on y rencontre
void Battle::startTips() {
  if (!shown() || autoPlay || net_ != Net::None) return;
  bool shield = false, role = false, terrain = false;
  for (auto& f : foes) shield = shield || f->shieldMax > 0, role = role || !f->S().combatRole.empty();
  for (auto& side : terrain_)
    for (int v : side) terrain = terrain || v >= 0;
  std::pair<const char*, bool> order[] = {{"bouclier", shield}, {"ordre", true}, {"role", role}, {"meteo", weather_ >= 0}, {"terrain", terrain}, {"talent", true}};
  for (auto& [id, ok] : order)
    if (ok && G.tip(id, sc)) return;
}

void Battle::update(float dt) {
  if (finished_) return;
  if (!startTips_) {
    startTips_ = true;
    startTips();
  }
  if (summaryOn_) {  // fenêtre des gains : Entrée (ou un clic) la ferme
    if (G.in.confirm || G.in.cancel || G.in.mclick[0] || autoPlay || G.time - summaryT_ > 8) {  // ou toute seule après 8 s
      summaryOn_ = false;
      G.in.confirm = false;
      sc.resume();
    }
    return;
  }
  // Tab : mode auto (tactiques) activé ou coupé
  if (G.in.tab && !autoPlay && !ending_) {
    G.in.tab = false;
    G.tacticsAuto = !G.tacticsAuto;
    toggledT_ = G.time;
    // Un allié attendait un ordre : ses tactiques jouent ce tour à la place du joueur
    if (G.tacticsAuto && G.menus.active() && !sc.busy() && actor && isAlly(actor) && tacticTurn(actor)) return;
  }
  if (sc.busy()) {
    sc.update(dt, G.in);
    return;
  }
  if (ending_) return;
  if (G.menus.active()) {
    if (boardPicking() && !G.pilot_) boardInput();
    G.menus.update(G.in);
    return;
  }
  if (follower() || remoteTurn_) return;  // invité : l'hôte fait avancer le temps ; hôte : ordre attendu
  tickATB(dt);
}

float Battle::atbRate(const FighterP& f) const {
  const Rules& R = rules();
  float rate = float((R.atbBase + f->eSpd()) * R.atbSpeed * (companion(f) ? R.companionAtb : 1));
  const Rules::Terrain* t = terrainAt(f);
  if (t && t->kind == "lent" && terrainHits(f, t)) rate *= float(1 - t->value);  // verglas
  for (auto& o : sideOf(f))
    if (o != f && o->alive() && o->S().combatRole == "chef") {  // rôle : un chef vivant presse ses alliés
      rate *= float(1 + R.roleChief);
      break;
    }
  return rate;
}

std::vector<FighterP> Battle::turnOrder(int n) const {
  std::vector<std::pair<float, FighterP>> ev;
  for (auto* side : {&allies, &foes})
    for (auto& f : *side) {
      if (!f->alive()) continue;
      float rate = atbRate(f);
      if (rate <= 0) continue;
      float t = std::max(0.f, (100 - f->atb) / rate);
      for (int k = 0; k < n; k++) ev.push_back({t + k * 100 / rate, f});
    }
  std::stable_sort(ev.begin(), ev.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  std::vector<FighterP> out;
  for (size_t i = 0; i < ev.size() && (int)out.size() < n; i++) out.push_back(ev[i].second);
  return out;
}

void Battle::drawWeather() {
  if (weather_ < 0) return;
  Gfx& g = G.g;
  float t = G.time, L = g.left(), W = (float)g.fullW;
  const std::string& fx = rules().weathers[(size_t)weather_].fx;
  auto hash = [](int i) {  // nombre pseudo-aléatoire fixe pour chaque particule (0 à 1)
    uint32_t h = uint32_t(i) * 2654435761u;
    h ^= h >> 13, h *= 0x5bd1e995u, h ^= h >> 15;
    return float(h % 10000) / 10000.f;
  };
  if (fx == "pluie" || fx == "orage") {
    g.rect(L, 0, W, SCREEN_H, rgb(0x203050, 40));
    for (int i = 0; i < 70; i++) {
      float x = L + std::fmod(hash(i) * W + t * 40, W), y = std::fmod(hash(i + 99) * SCREEN_H + t * 260, (float)SCREEN_H);
      g.line(x, y, x - 2, y + 7, 1, rgb(0xb8d8ff, 150));
    }
    if (fx == "orage" && std::fmod(t, 4.3f) < .12f) g.rect(L, 0, W, SCREEN_H, rgb(0xffffff, 120));  // éclair
  } else if (fx == "neige") {
    g.rect(L, 0, W, SCREEN_H, rgb(0xe8f4ff, 30));
    for (int i = 0; i < 80; i++) {
      float x = L + std::fmod(hash(i) * W + std::sin(t + i) * 6 + t * 25, W), y = std::fmod(hash(i + 7) * SCREEN_H + t * 45, (float)SCREEN_H);
      g.ellipse(x, y, 1.2f, 1.2f, rgb(0xffffff, 210));
    }
  } else if (fx == "chaleur") {
    g.rect(L, 0, W, SCREEN_H, rgb(0xff8a3a, 28));
    for (int i = 0; i < 30; i++) {  // cendres qui montent
      float x = L + std::fmod(hash(i) * W + std::sin(t * 1.5f + i) * 5, W), y = SCREEN_H - std::fmod(hash(i + 3) * SCREEN_H + t * 30, (float)SCREEN_H);
      g.rect(x, y, 1, 1, rgb(0xffb050, 200));
    }
  } else if (fx == "brume") {
    for (int i = 0; i < 6; i++) {
      float x = L + std::fmod(hash(i) * W + t * (8 + i * 2), W + 120) - 60, y = 60 + i * 22;
      g.ellipse(x, y, 70, 12, rgb(0xd8d4e8, 38));
    }
  } else if (fx == "soleil") {
    for (int i = 0; i < 4; i++) {
      float x = 40 + i * 70 + std::sin(t * .5f + i) * 6;
      g.poly({{x, 0}, {x + 18, 0}, {x + 70, 150}, {x + 40, 150}}, rgb(0xfff4c0, uint8_t(22 + 10 * std::sin(t + i))));
    }
  }
}

void Battle::drawTurnOrder(float y, float minX) {
  Gfx& g = G.g;
  auto order = turnOrder(8);
  if (order.empty()) return;
  float x = std::max(minX, 160 - order.size() * 12.f);
  std::map<const Fighter*, int> seenOnce;  // un ennemi brisé : seul son prochain tour est perdu
  for (size_t i = 0; i < order.size(); i++, x += 24) {
    const FighterP& f = order[i];
    bool ally = isAlly(f), lost = !ally && f->broken && seenOnce[f.get()]++ == 0;
    Color edge = ally ? rgb(0x8fd0ff) : f->boss ? rgb(0xffe066) : rgb(0xff8a7a);
    g.rect(x, y, 22, 10, rgb(0x0a0f33, 200));
    g.rect(x, y + 9, 22, 1, edge);
    if (i == 0) g.rect(x, y, 22, 1, edge);  // le prochain à jouer
    g.text(x + 11, y + 1, utf8Prefix(f->name(), 3), lost ? GREY : ally ? WHITE : edge, 1, false);
    if (lost) g.rect(x + 2, y + 5, 18, 1, rgb(0xffe066));  // barré : tour perdu (brisé)
  }
}

void Battle::tickATB(float dt) {
  std::vector<FighterP> all = alive(allies);
  for (auto& e : alive(foes)) all.push_back(e);
  for (auto& f : all) {
    f->atb += dt * atbRate(f);
    if (f->atb >= 100) {
      f->atb = 100;
      if (skipTurn(f)) return;
      if (isAlly(f) && coop() && owner(f) != me_) {  // gardien : allié d'un autre joueur
        if (owner(f) < 0) autoCommand(f);            // joueur parti : l'ordinateur joue
        else {
          remoteTurn_ = f;
          actor = f;
          send_(owner(f), Json{{"t", "tour"}, {"i", mine(f)[1]}});
        }
      } else if (isAlly(f)) {  // héros ou compagnon : ses tactiques (mode auto), sinon son menu
        if (tacticsActive() && tacticTurn(f)) return;
        if (autoPlay) autoCommand(f);
        else command(f);
      } else if (net_ == Net::Host) {  // combattant de l'invité : on attend son ordre
        remoteTurn_ = f;
        actor = f;
        send_(-1, Json{{"t", "tour"}, {"i", mine(f)[1]}});
      } else enemyTurn(f);
      return;
    }
  }
}

bool Battle::skipTurn(FighterP f) {
  if (f->broken) {  // brisé : il perd ce tour, puis se ressaisit (bouclier rechargé)
    actor = f;
    f->broken = false;
    f->shield = f->shieldMax;
    log.push_back("  " + f->name() + T(" se ressaisit (brisé)"));
    sc.say(f->name() + T(" se ressaisit."), .7f);
    sc.call([this, f] { afterAction(f); });
    return true;
  }
  if (f->status == Status::Sleep) {
    actor = f;
    log.push_back("  " + f->name() + T(" dort"));
    if (--f->statusTurns <= 0) {
      f->status = Status::None;
      sc.say(f->name() + T(" se réveille !"), .8f);
    } else sc.say(f->name() + T(" dort profondément…"), .7f);
    sc.call([this, f] { afterAction(f); });
    return true;
  }
  if (f->status == Status::Paralysis && frand() < rules().paraSkip) {
    actor = f;
    log.push_back("  " + f->name() + T(" paralysé"));
    sc.say(f->name() + T(" est paralysé et ne peut pas bouger !"), .8f);
    sc.call([this, f] { afterAction(f); });
    return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// Menus de commande
// ---------------------------------------------------------------------------
void Battle::command(FighterP a) {
  actor = a;
  Menu m;
  m.rows = 6, m.cancelable = false;
  m.items.push_back({T("Attaquer"), "", T("Utiliser une technique."), true, [this, a] {
                       Menu t;
                       t.title = T("Techniques");
                       t.rows = 5;
                       for (auto& id : a->techs()) {
                         const Move& mv = moveInfo(id);
                         t.items.push_back({mv.name, typeName(mv.type), moveBrief(mv, a.get()), true, [this, a, id] { moveTarget(a, id); },
                                            [this, a, id] { previewMove(a, id); }});
                       }
                       dock(t);
                       G.menus.push(t);
                     }});
  auto sp = a->spells();
  if (!sp.empty())
    m.items.push_back({T("Magie"), "", T("Lancer un sort (coûte des PM)."), true, [this, a, sp] {
                         Menu t;
                         t.title = T("Magie  ") + std::to_string(a->mp) + "/" + std::to_string(a->mmp) + T(" PM");
                         t.rows = 5;
                         for (auto& id : sp) {
                           const Move& mv = moveInfo(id);
                           t.items.push_back({mv.name, std::to_string(mv.cost) + T(" PM"), moveBrief(mv, a.get()), a->mp >= mv.cost,
                                              [this, a, id] { moveTarget(a, id); }, [this, a, id] { previewMove(a, id); }});
                         }
                         dock(t);
                         G.menus.push(t);
                       }});
  if (!a->S().limit.empty()) {
    const Move& lim = moveInfo(a->S().limit);
    m.items.push_back({T("Limite"), "", (a->lim >= 100 ? lim.name + T(" : technique ultime ! ") : T("La jauge Limite (rose) se remplit quand on reçoit des coups. ") + lim.name + " : ") +
                           moveDetails(lim, a.get()),
                       a->lim >= 100, [this, a, lim] {
                         G.menus.clear();
                         useMove(a, lim.id, lim.target == Target::AllAllies ? alive(allies) : alive(foes), true);
                       }});
  }
  if (!duoOptions(a, false).empty()) {  // techniques en duo possibles dans ce combat
    auto duos = duoOptions(a);
    if (!duos.empty()) tip("duo");
    std::string why = T("Technique à deux. Il faut un partenaire dont la jauge ATB atteint ") + std::to_string(rules().duoGauge) +
                      T(" % (il la dépense) et les PM des deux.");
    m.items.push_back({T("Duo"), "", duos.empty() ? why : T("Technique à deux : le partenaire dépense sa jauge ATB."), !duos.empty(), [this, a, duos] {
                         Menu t;
                         t.title = T("Duo");
                         t.rows = 5;
                         for (auto& d : duos) {
                           const Move& mv = moveInfo(d.move);
                           std::string id = d.move;
                           FighterP p = d.partner;
                           t.items.push_back({mv.name, mv.cost ? std::to_string(mv.cost) + T(" PM") : p->name(),
                                              T("Avec ") + p->name() + (mv.cost ? " (" + std::to_string(mv.cost) + T(" PM chacun)") : "") + ". " + moveBrief(mv, a.get()), true,
                                              [this, a, id, p] { moveTarget(a, id, p); }, [this, a, id] { previewMove(a, id); }});
                         }
                         dock(t);
                         G.menus.push(t);
                       }});
  }
  m.items.push_back({T("Objet"), "", T("Utiliser un objet du sac."), true, [this, a] {
                       Menu t;
                       t.title = T("Objets");
                       t.rows = 5;
                       bool creature = false;
                       for (auto& f : alive(foes)) creature = creature || !f->S().human;
                       for (auto& d : allItems()) {
                         int n = G.items.count(d.id) ? G.items[d.id] : 0;
                         if (!d.battle || n <= 0) continue;
                         std::string id = d.id;
                         bool ok = true;
                         bool lantern = d.capture > 0;
                         std::string help = itemDetails(d);  // en combat : sans la phrase d'ambiance
                         if (lantern) {
                           ok = canCapture && creature;
                           if (!canCapture) help = T("Impossible de capturer ici.");
                           else if (!creature) help = T("On ne capture pas une personne !");
                           else if ((int)G.team.size() >= rules().maxTeam) help += T(" Équipe complète : la créature ira dans la réserve.");
                         }
                         if (d.revive) ok = alive(allies).size() < allies.size();
                         t.items.push_back({d.name, T("×") + std::to_string(n), help, ok, [this, a, id, lantern] {
                                              if (lantern) {
                                                pickFoe(T("Capturer"), [id](const Fighter& f) {
                                                  return T("Chance de capture : ") + std::to_string(int(captureChance(f, item(id).capture) * 100)) + " %";
                                                }, [this, a, id](FighterP t) { useItem(a, id, t); }, true);
                                              } else pickAlly(T("Sur qui ?"), item(id).revive > 0, [this, a, id](FighterP t) { useItem(a, id, t); });
                                            }});
                       }
                       if (t.items.empty()) t.items.push_back({T("(sac vide)"), "", "", false, nullptr});
                       dock(t);
                       G.menus.push(t);
                     }});
  // Se déplacer : vers une case libre de son damier (action rapide)
  std::vector<int> freeCells;
  for (int c = 0; c < N_CELLS; c++) {
    bool taken = false;
    for (auto& f : allies) taken = taken || cellOf(f) == c;
    if (!taken) freeCells.push_back(c);
  }
  m.items.push_back({T("Se déplacer"), "", freeCells.empty() ? T("Aucune case libre.") : T("Changer de case sur le damier (action rapide)."), !freeCells.empty(),
                     [this, a, freeCells] {
                       Menu t;
                       t.title = T("Se déplacer");
                       t.rows = 5;
                       for (int c : freeCells) {
                         const Rules::Terrain* tr = terrainOf(true, c);
                         std::string where = cellCol(c) == 0 ? T("Avant : frappe et reçoit plus fort au corps à corps.")
                                             : cellCol(c) == BACK_COL ? T("Arrière : moins de dégâts au corps à corps (donnés et reçus).")
                                                                      : T("Milieu.");
                         t.items.push_back({cellName(c), "", cellName(c) + " · " + (tr ? tr->name + " : " + tr->desc : where), true,
                                            [this, a, c] { moveTo(a, c); }});
                       }
                       // la case libre la plus proche du combattant d'abord
                       Pt from = cellPos(true, cellOf(a));
                       float best = 1e9f;
                       for (size_t i = 0; i < freeCells.size(); i++) {
                         Pt p = cellPos(true, freeCells[i]);
                         float d = std::abs(p.x - from.x) + std::abs(p.y - from.y);
                         if (d < best) best = d, t.sel = (int)i;
                       }
                       boardMenu(t, true, freeCells, true);
                     }});
  std::vector<FighterP> bench;  // avec des héros : un héros n'est remplacé que par un héros
  for (auto& r : G.team)
    if (r->alive() && !isAlly(r) && (!hasHero_ || r->S().human == a->S().human)) bench.push_back(r);
  m.items.push_back({T("Changer"), "", bench.empty() ? T("Personne pour vous remplacer.") : T("Faire entrer un remplaçant de l'équipe."), !bench.empty(),
                     [this, a, bench] {
                       Menu t;
                       t.title = T("Remplaçants");
                       t.rows = 5;
                       for (auto& r : bench)
                         t.items.push_back({r->name() + T(" N.") + std::to_string(r->lvl), std::to_string(r->hp) + "/" + std::to_string(r->mhp),
                                            typesName(r->S()) + " · " + std::to_string(r->mp) + T(" PM"), true, [this, a, r] { swapIn(a, r); }});
                       dock(t);
                       G.menus.push(t);
                     }});
  if (canFlee) m.items.push_back({T("Fuir"), "", T("Tenter de fuir avec toute l'équipe."), true, [this, a] { tryFlee(a); }});
  // Duel : ni objets ni remplaçants (l'équipement compte) ; gardien à plusieurs : pas de remplaçants
  if (net_ != Net::None) {
    bool d = duel();
    m.items.erase(std::remove_if(m.items.begin(), m.items.end(),
                                 [d](const MenuItem& it) { return (d && it.label == T("Objet")) || it.label == T("Changer"); }),
                  m.items.end());
  }
  dock(m, Dock::Command);
  G.menus.push(m);
}

void Battle::pickFoe(const std::string& title, std::function<std::string(const Fighter&)> info, std::function<void(FighterP)> done,
                     bool creaturesOnly) {
  Menu t;
  t.title = title;
  t.rows = 6;
  t.onCancel = [this] {
    cursor = nullptr;
    aiming_ = nullptr;
    G.menus.pop();
  };
  std::vector<FighterP> list;
  for (auto& f : alive(foes)) {
    if (creaturesOnly && f->S().human) continue;
    if (aiming_ && !reachable(actor, *aiming_, f)) continue;  // corps à corps : seulement l'avant
    list.push_back(f);
  }
  // dans l'ordre du damier (de haut en bas, de l'avant à l'arrière) : Haut et Bas suivent l'écran
  std::stable_sort(list.begin(), list.end(), [&](const FighterP& a, const FighterP& b) {
    int ca = std::max(0, cellOf(a)), cb = std::max(0, cellOf(b));
    return cellRow(ca) != cellRow(cb) ? cellRow(ca) < cellRow(cb) : cellCol(ca) < cellCol(cb);
  });
  std::vector<int> cells;
  for (auto& f : list) {
    if (f == cursor) t.sel = (int)t.items.size();  // la cible de l'aperçu
    t.items.push_back({f->name(), T("N.") + std::to_string(f->lvl), f->name() + T(" N.") + std::to_string(f->lvl) + " · " + info(*f), true,
                       [done, f] { done(f); }, [this, f] { cursor = f; }});
    cells.push_back(cellOf(f));
  }
  boardMenu(t, false, cells);
}
void Battle::pickAlly(const std::string& title, bool ko, std::function<void(FighterP)> done) {
  Menu t;
  t.title = title;
  t.rows = 6;
  t.onCancel = [this] {
    cursor = nullptr;
    G.menus.pop();
  };
  std::vector<int> cells;
  int first = -1;  // le premier choix possible
  for (auto& f : allies) {
    bool ok = ko ? !f->alive() : f->alive();
    std::string st = f->status != Status::None ? std::string(" · ") + statusName(f->status) : "";
    if (ok && first < 0) first = (int)t.items.size();
    t.items.push_back({f->name(), std::to_string(f->hp) + "/" + std::to_string(f->mhp),
                       f->name() + " · " + std::to_string(f->hp) + "/" + std::to_string(f->mhp) + T(" PV · ") + std::to_string(f->mp) + "/" +
                           std::to_string(f->mmp) + T(" PM") + (f->alive() ? st : T(" · K.O.")),
                       ok, [done, f] { done(f); }, [this, f] { cursor = f; }});
    cells.push_back(cellOf(f));
  }
  t.sel = std::max(0, first);
  boardMenu(t, true, cells);
}

// ---------------------------------------------------------------------------
// Choix sur le damier (3.2)
// ---------------------------------------------------------------------------
void Battle::boardMenu(Menu t, bool allySide, std::vector<int> cells, bool move) {
  t.hidden = true;
  dock(t);
  pick_ = {G.menus.depth() + 1, allySide, std::move(cells), move};
  G.menus.push(t);
}
bool Battle::boardPicking() const {
  return pick_.depth && G.menus.depth() == pick_.depth && G.menus.active() && G.menus.top().hidden;
}
int Battle::cellUnder(bool allySide, float x, float y, const std::vector<int>& among) const {
  int best = -1;
  float bd = 1e9f;
  for (size_t i = 0; i < among.size(); i++) {
    Pt p = cellPos(allySide, among[i]);
    if (std::abs(x - p.x) > 15 || y < p.y - 26 || y > p.y + 19) continue;  // la dalle et le combattant debout dessus
    float d = std::abs(x - p.x) + std::abs(y - (p.y + 4));
    if (d < bd) bd = d, best = (int)i;
  }
  return best;
}
void Battle::boardInput() {
  Input& in = G.in;
  Menu& m = G.menus.top();
  int n = (int)std::min(m.items.size(), pick_.cells.size());
  if (n == 0) return;
  auto choose = [&](int i) {
    if (i < 0 || i == m.sel || !m.items[(size_t)i].enabled) return;
    m.sel = i;
    audio::play("curseur");
    if (m.items[(size_t)i].hover) m.items[(size_t)i].hover();
  };
  // Flèches : la case la plus proche dans cette direction, à l'écran
  static const float DX[4] = {0, 0, -1, 1}, DY[4] = {-1, 1, 0, 0};
  for (int d = 0; d < 4; d++) {
    if (!in.press[d]) continue;
    in.press[d] = false;
    Pt from = cellPos(pick_.allySide, pick_.cells[(size_t)std::clamp(m.sel, 0, n - 1)]);
    int best = -1;
    float bs = 1e9f;
    for (int i = 0; i < n; i++) {
      if (i == m.sel || !m.items[(size_t)i].enabled) continue;
      Pt p = cellPos(pick_.allySide, pick_.cells[(size_t)i]);
      float vx = p.x - from.x, vy = p.y - from.y, along = vx * DX[d] + vy * DY[d], across = std::abs(vx * DY[d] - vy * DX[d]);
      if (along < 3) continue;
      if (along + across * 2 < bs) bs = along + across * 2, best = i;
    }
    choose(best);
  }
  if (in.mouseOn) {
    int at = cellUnder(pick_.allySide, (float)in.mx, (float)in.my, pick_.cells);
    if (at >= n) at = -1;
    if (in.moved) choose(at);
    if (in.mclick[0]) {
      in.mclick[0] = false;
      if (at >= 0 && m.items[(size_t)at].enabled) {
        m.sel = at;
        in.confirm = true;
      }
    }
  }
}
// Survol d'une technique : sa zone autour de la cible probable (la dernière visée si elle est encore à portée,
// sinon le premier ennemi à portée)
void Battle::previewMove(const FighterP& a, const std::string& id) {
  preview_ = id;
  previewDepth_ = G.menus.depth();
  const Move& m = moveInfo(id);
  if (m.target != Target::Enemy) return;
  if (cursor && cursor->alive() && !isAlly(cursor) && reachable(a, m, cursor)) return;
  cursor = nullptr;
  for (auto& f : alive(foes)) {
    if (!reachable(a, m, f)) continue;
    if (!cursor || cellCol(std::max(0, cellOf(f))) < cellCol(std::max(0, cellOf(cursor)))) cursor = f;
  }
}

// Fiche rapide d'un ennemi visé : types, PV, état, bonus/malus et efficacité
static std::string foeInfo(const Game& G, const Fighter& f, const Move* m) {
  std::string s = typesName(f.S()) + " · " + std::to_string(f.hp) + "/" + std::to_string(f.mhp) + T(" PV");
  if (!f.S().combatRole.empty()) s += " · " + std::string(f.S().combatRole == "protecteur" ? T("Protecteur") : f.S().combatRole == "soigneur" ? T("Soigneur") : T("Chef"));
  if (f.broken) s += T(" · BRISÉ");
  else if (f.shieldMax > 0) s += T(" · bouclier ") + std::to_string(f.shield);
  if (f.status != Status::None) s += " · " + std::string(statusName(f.status));
  for (int st = 0; st < N_STAGES; st++)
    if (f.stage[st]) s += " · " + std::string(stageName(st)) + (f.stage[st] > 0 ? " +" : " ") + std::to_string(f.stage[st]);
  if (m && m->damaging()) {  // seulement une faiblesse déjà découverte (Brisure : à trouver en essayant)
    if (G.weakKnown(f.sp, m->type)) s += T(" · faiblesse !");
    else if (!G.weak.empty() || G.seen.count(f.sp)) s += " · ?";
  }
  return s;
}

void Battle::moveTarget(FighterP a, const std::string& id, FighterP duo) {
  const Move& m = moveInfo(id);
  auto go = [this, a, id, duo](std::vector<FighterP> t) {
    if (duo) duoMove(a, duo, id, std::move(t));
    else useMove(a, id, std::move(t));
  };
  switch (m.target) {
    case Target::Enemy:
      aiming_ = &m;
      pickFoe(T("Cible"), [this, &m](const Fighter& f) { return foeInfo(G, f, &m); }, [go](FighterP t) { go({t}); });
      break;
    case Target::AllEnemies: go(alive(foes)); break;
    case Target::Ally: pickAlly(T("Sur qui ?"), false, [go](FighterP t) { go({t}); }); break;
    case Target::AllAllies: go(alive(allies)); break;
    case Target::AllyKO:
      if (alive(allies).size() == allies.size()) {
        sc.say(T("Personne n'est K.O."), .8f);
        return;
      }
      pickAlly(T("Qui relever ?"), true, [go](FighterP t) { go({t}); });
      break;
  }
}

std::vector<Battle::DuoOpt> Battle::duoOptions(const FighterP& a, bool ready) const {
  std::vector<DuoOpt> out;
  if (net_ != Net::None || !isAlly(a)) return out;  // pas de duo en ligne
  const Rules& R = rules();
  for (auto& d : R.duos) {
    if (d.members.size() != 2 || !hasMove(d.move)) continue;
    const Move& m = moveInfo(d.move);
    FighterP best;
    for (auto& p : allies) {
      if (p == a || !p->alive()) continue;
      bool match = (duoMember(d.members[0], a->S()) && duoMember(d.members[1], p->S())) ||
                   (duoMember(d.members[1], a->S()) && duoMember(d.members[0], p->S()));
      if (!match) continue;
      if (ready && (p->atb < R.duoGauge || p->status == Status::Sleep || p->status == Status::Paralysis || a->mp < m.cost || p->mp < m.cost))
        continue;
      if (!best || p->atb > best->atb) best = p;
    }
    if (best) out.push_back({d.move, best});
  }
  return out;
}

void Battle::duoMove(FighterP a, FighterP p, const std::string& id, std::vector<FighterP> targets) {
  const Move& m = moveInfo(id);
  if (m.cost > 0) p->mp = std::max(0, p->mp - m.cost);
  p->atb = 0;  // le partenaire dépense sa jauge
  pop(p, T("Duo !"), rgb(0xffd34d));
  log.push_back(a->name() + " + " + p->name() + T(" : duo"));
  duoPartner_ = p;
  useMove(a, id, std::move(targets));
  duoPartner_ = nullptr;
}

// ---------------------------------------------------------------------------
// Intelligence de l'ordinateur : ennemis, et alliés en mode automatique
// ---------------------------------------------------------------------------
Battle::Plan Battle::think(FighterP a) {
  bool ally = isAlly(a);
  const auto& side = ally ? allies : foes;
  auto mine = alive(side), theirs = alive(ally ? foes : allies);
  if (theirs.empty()) return {a->techs()[0], {}, false};
  if (ally && a->lim >= 100 && !a->S().limit.empty()) {
    const Move& lim = moveInfo(a->S().limit);
    return {lim.id, lim.target == Target::AllAllies ? mine : theirs, true};
  }
  std::vector<std::string> moves = a->techs();
  for (auto& id : a->spells())
    if (a->mp >= moveInfo(id).cost) moves.push_back(id);
  // Les ennemis ne visent pas toujours la meilleure cible : souvent, ils frappent au hasard
  FighterP focus = !ally && frand() < .65f ? theirs[irand(0, (int)theirs.size() - 1)] : nullptr;

  Plan best;
  float bestScore = 0;
  auto consider = [&](const std::string& id, std::vector<FighterP> tg, float score, FighterP duo = nullptr) {
    score *= .85f + frand() * .3f;
    if (!tg.empty() && score > bestScore) bestScore = score, best = {id, tg, false}, best.duo = duo;
  };
  // Duos : comptés moins la part de tour que le partenaire dépense
  if (ally)
    for (auto& d : duoOptions(a)) {
      const Move& m = moveInfo(d.move);
      if (d.partner->mp - m.cost < .4f * d.partner->mmp) continue;  // garder les PM du partenaire (soins)
      float keep = 1 - .45f * d.partner->atb / 100;
      if (m.damaging()) {
        if (m.target == Target::AllEnemies) {
          float sum = 0;
          for (auto& t : theirs) sum += attackScore(a, m, t);
          consider(d.move, theirs, sum * .8f * keep, d.partner);
        } else
          for (auto& t : theirs)
            if (reachable(a, m, t)) consider(d.move, {t}, zoneScore(a, m, t) * keep, d.partner);
      } else if (m.kind == Kind::Heal && m.target == Target::AllAllies) {
        float lack = 0;
        for (auto& f : mine) lack += f->hp * 2 < f->mhp ? 1 - float(f->hp) / f->mhp : 0;
        if (lack > 0) consider(d.move, mine, 110 * lack * keep, d.partner);
      }
    }
  for (auto& id : moves) {
    const Move& m = moveInfo(id);
    switch (m.kind) {
      case Kind::Heal: {
        FighterP worst;
        int hurt = 0;
        for (auto& f : mine) {
          if (f->hp * 2 < f->mhp) hurt++;
          if (!worst || f->hp * worst->mhp < worst->hp * f->mhp) worst = f;
        }
        if (m.power > 0 && worst && worst->hp * 2 < worst->mhp) {
          float lack = 1 - float(worst->hp) / worst->mhp;
          consider(id, m.target == Target::AllAllies ? mine : std::vector<FighterP>{worst}, 110 * lack * (m.target == Target::AllAllies ? hurt : 1));
        }
        for (auto& e : m.effects)
          if (e.cure)
            for (auto& f : mine)
              if (f->status != Status::None) consider(id, m.target == Target::AllAllies ? mine : std::vector<FighterP>{f}, 50);
        break;
      }
      case Kind::Revive:
        for (auto& f : side)
          if (!f->alive()) consider(id, {f}, 140);
        break;
      case Kind::Status: {
        bool onFoes = m.target == Target::Enemy || m.target == Target::AllEnemies;
        auto pool = onFoes ? theirs : mine;
        for (auto& t : pool) {
          float v = 0;
          for (auto& e : m.effects) {
            if (e.status != Status::None && t->status == Status::None && !immuneTo(t->S(), e.status)) v += 35 * e.chance / 100.f;
            if (e.stat >= 0 && !e.self && (e.stages > 0 ? t->stage[e.stat] < 2 : t->stage[e.stat] > -2)) v += 18;
            if (e.stat >= 0 && e.self && a->stage[e.stat] < 2) v += 16;
          }
          if (v <= 0) continue;
          if (m.target == Target::AllEnemies || m.target == Target::AllAllies) consider(id, pool, v * (.6f + .4f * pool.size()));
          else if (!focus || !onFoes || t == focus) consider(id, {t}, v);
        }
        break;
      }
      default:
        if (m.target == Target::AllEnemies) {
          float sum = 0;
          for (auto& t : theirs) sum += attackScore(a, m, t);
          consider(id, theirs, sum * (ally ? .8f : .6f));  // les ennemis abusent moins des attaques de zone
        } else
          for (auto& t : theirs)
            // cible préférée hors de portée (corps à corps, elle est à l'arrière) : le meilleur adversaire à portée
            if ((!focus || t == focus || !reachable(a, m, focus)) && reachable(a, m, t)) consider(id, {t}, zoneScore(a, m, t));
        break;
    }
  }
  if (best.move.empty()) {  // rien d'intéressant : la première technique, sur une cible à portée
    const Move& m0 = moveInfo(a->techs()[0]);
    std::vector<FighterP> ok;
    for (auto& t : theirs)
      if (reachable(a, m0, t)) ok.push_back(t);
    if (ok.empty()) ok = theirs;
    best = {m0.id, {ok[irand(0, (int)ok.size() - 1)]}, false};
  }
  return best;
}

// Intérêt d'une attaque sur une cible (sans hasard), dans l'unité de Battle::think :
// dégâts attendus (puissance x efficacité x Attaque/Défense x précision), divisés par
// le temps que prend la technique (rapide ou lourde). Les alliés comptent aussi les
// critiques et les effets en plus ; les ennemis non, pour garder la difficulté des
// combats telle qu'elle a été réglée.
float Battle::attackScore(const FighterP& a, const Move& m, const FighterP& t) const {
  const Rules& R = rules();
  bool magic = m.kind == Kind::Magic, ally = isAlly(a);
  if (!reachable(a, m, t)) return 0;
  float A = magic ? a->eMag() : a->eAtk(), D = std::max(1.f, magic ? t->eRes() : t->eDef());
  float hit = std::clamp(m.acc / 100.f * a->acc / 100.f - (magic ? 0 : t->eva / 100.f), 0.f, 1.f);
  float score = m.power * moveEff(m, t->S()) * (hasType(a->S(), m.type) ? float(R.stab) : 1.f) * A / D * hit * weatherMult(m.type) * guardMult(t);
  if (ally && t->broken) score *= float(R.breakDamage);                                      // Brisure : il encaisse plus
  if (ally && !t->broken && t->shieldMax > 0 && moveEff(m, t->S()) > 1) score *= 1.f + .6f / float(std::max(1, t->shield));  // près de le briser
  if (m.melee) {  // depuis l'arrière, ou sur une cible à l'arrière : moins fort
    if (cellCol(std::max(0, cellOf(a))) == BACK_COL) score *= float(R.backRow);
    if (cellCol(std::max(0, cellOf(t))) == BACK_COL) score *= float(R.backRow);
  }
  if (!ally) return score / paceTime(m);
  score *= 1 + std::min(1.f, (a->crit + m.critBonus) / 100.f) * float(R.critMult - 1);
  if (score >= t->hp * 1.2f) score += 15;  // achever un adversaire affaibli
  if (!t->S().combatRole.empty() && !t->boss) score *= 1.25f;  // rôles : abattre d'abord soigneur, chef et protecteur
  if (m.shove && !t->boss) {  // pousser / tirer : un choc contre un voisin ou le bord, ou une case qui blesse
    int to = shoveDest(t, m);
    bool clash = to == -1 && m.shove > 0;
    for (auto& o : sideOf(t)) clash = clash || (to >= 0 && o != t && o->alive() && cellOf(o) == to);
    if (clash) score *= 1 + float(R.shoveClash);
    else if (to >= 0 && terrainHurts(t, to)) score += 12;
    else if (to >= 0 && m.shove < 0) score += 4;  // tiré vers l'avant : à portée du corps à corps
  }
  // Effets en plus : un état ou un malus sur la cible, un bonus pour le lanceur
  for (auto& e : m.effects) {
    float ch = e.chance / 100.f * hit;
    if (e.self) {
      if (e.stat >= 0 && e.stages > 0 && a->stage[e.stat] < 2) score += 16 * ch;
    } else {
      if (e.status != Status::None && t->status == Status::None && !immuneTo(t->S(), e.status)) score += 35 * ch;
      if (e.stat >= 0 && (e.stages > 0 ? t->stage[e.stat] < 2 : t->stage[e.stat] > -2)) score += 18 * ch;
    }
  }
  return score / paceTime(m);
}

void Battle::autoCommand(FighterP a) {
  actor = a;
  if (isAlly(a) && threatened(a)) {  // une attaque annoncée vise sa case : se mettre à l'abri
    int c = safeCell(a);
    if (c >= 0) return moveTo(a, c);
  }
  Plan p = think(a);
  if (p.duo) duoMove(a, p.duo, p.move, p.targets);
  else useMove(a, p.move, p.targets, p.limit);
}

void Battle::enemyTurn(FighterP e) {
  actor = e;
  if (bossPlan(e) || roleHeal(e) || enemyMove(e)) return;
  Plan p = think(e);
  useMove(e, p.move, p.targets);
}

// ---------------------------------------------------------------------------
// Tactiques des alliés (voir tactics.hpp) : de haut en bas, la première règle
// dont la condition vise quelqu'un et dont l'action est possible est jouée.
// ---------------------------------------------------------------------------
bool Battle::tacticsActive() const { return autoPlay ? simTactics : G.tacticsAuto; }

// ---------------------------------------------------------------------------
// Damier : attaques annoncées des boss, déplacements de l'ordinateur
// ---------------------------------------------------------------------------
bool Battle::threatened(const FighterP& f) const {
  int c = cellOf(f);
  if (terrainHurts(f, c)) return true;  // braises, ronces, verglas : mieux vaut changer de case
  for (auto& w : warnings_)
    if (inZone(w.zone, w.center, c)) return true;
  return false;
}
int Battle::safeCell(const FighterP& f) const {
  int from = cellOf(f), best = -1, bestD = 99;
  for (int c = 0; c < N_CELLS; c++) {
    bool taken = false, danger = false;
    for (auto& o : sideOf(f)) taken = taken || (o != f && cellOf(o) == c);
    for (auto& w : warnings_) danger = danger || inZone(w.zone, w.center, c);
    danger = danger || terrainHurts(f, c);
    int d = std::abs(cellCol(c) - cellCol(from)) * 2 + std::abs(cellRow(c) - cellRow(from));  // garder sa colonne si possible
    if (!taken && !danger && c != from && d < bestD) best = c, bestD = d;
  }
  return best;
}
bool Battle::bossPlan(FighterP e) {
  if (!e->boss || isAlly(e) || duel()) return false;
  for (auto it = warnings_.begin(); it != warnings_.end(); ++it)
    if (it->who == e) {  // l'attaque annoncée part : seulement sur ceux restés dans la zone
      Warning w = *it;
      warnings_.erase(it);
      std::vector<FighterP> hit;
      for (auto& a : alive(allies))
        if (inZone(w.zone, w.center, cellOf(a))) hit.push_back(a);
      if (hit.empty()) {
        log.push_back(e->name() + " : " + moveInfo(w.move).name + T(" (annoncée), personne dans la zone"));
        sc.say(e->name() + T(" déchaîne ") + moveInfo(w.move).name + T("… mais tout le monde s'est écarté !"), 1.2f);
        return false;  // il joue tout de même une attaque normale
      }
      log.push_back(e->name() + " : " + moveInfo(w.move).name + T(" (annoncée) sur ") + std::to_string(hit.size()));
      useMove(e, w.move, hit, false, float(rules().bossBurst));
      return true;
    }
  if (alive(allies).empty() || frand() >= rules().bossWarn) return false;
  // Sa plus forte attaque, concentrée sur la zone où se trouvent le plus d'alliés
  std::string best;
  int power = 0;
  std::vector<std::string> moves = e->techs();
  for (auto& id : e->spells())
    if (e->mp >= moveInfo(id).cost) moves.push_back(id);
  for (auto& id : moves)
    if (moveInfo(id).damaging() && moveInfo(id).power > power) power = moveInfo(id).power, best = id;
  if (best.empty()) return false;
  int center = -1, most = 0;
  Area zone = Area::Cross;
  for (Area z : {Area::Column, Area::Cross, Area::Line})
    for (auto& a : alive(allies)) {
      int n = 0;
      for (auto& b : alive(allies)) n += inZone(z, cellOf(a), cellOf(b));
      if (n > most || (n == most && frand() < .3f)) most = n, zone = z, center = cellOf(a);
    }
  warnings_.push_back({e, best, zone, center});
  log.push_back(e->name() + T(" annonce ") + moveInfo(best).name + " (" + zoneName(zone) + ", " + cellName(center) + ")");
  sound("statut");
  sc.say(e->name() + T(" prépare ") + moveInfo(best).name + T(" ! Écartez-vous de la zone rouge."), 1.4f);
  return false;  // l'annonce ne lui coûte pas son tour : il attaque aussitôt
}
bool Battle::enemyMove(FighterP e) {
  if (isAlly(e) || e->boss || duel()) return false;
  int c = cellOf(e), col = cellCol(c), row = cellRow(c);
  const Species& s = e->S();
  bool caster = s.base[B_MAG] > s.base[B_ATK];
  auto freeCell = [&](int cc) {
    for (auto& o : foes)
      if (cellOf(o) == cc && o->alive()) return false;
    return true;
  };
  int to = -1;
  if (terrainHurts(e, c) && frand() < .6f)  // une case qui blesse ou ralentit : la quitter
    for (int k : {col, col == 0 ? 1 : col - 1, col == BACK_COL ? BACK_COL - 1 : col + 1})
      for (int rr = 0; rr < GRID && to < 0; rr++) {
        int cc = cellAt((row + rr) % GRID, k);
        if (freeCell(cc) && !terrainHurts(e, cc)) to = cc;
      }
  if (caster && col <= 1 && frand() < .5f)  // un lanceur de sorts trop en avant se met à l'abri
    for (int k : {BACK_COL, BACK_COL - 1})
      if (to < 0 && freeCell(cellAt(row, k))) to = cellAt(row, k);
  if (!caster && col >= 2 && frand() < .4f)  // un combattant trop loin avance
    for (int k : {0, 1})
      if (to < 0 && freeCell(cellAt(row, k))) to = cellAt(row, k);
  if (to < 0) return false;
  moveTo(e, to);
  return true;
}

bool Battle::tacticTurn(FighterP a) {
  if (!a->tacticsOn) return false;
  Plan p;
  if (!tacticPlan(a, p)) return false;
  actor = a;
  log.push_back(a->name() + T(" : tactique ") + std::to_string(p.tactic + 1));
  if (p.cell >= 0) {  // se mettre à l'abri
    moveTo(a, p.cell);
    return true;
  }
  if (!p.item.empty()) useItem(a, p.item, p.targets.at(0));
  else if (p.duo) duoMove(a, p.duo, p.move, p.targets);
  else useMove(a, p.move, p.targets, p.limit);
  return true;
}

bool Battle::tacticPlan(FighterP a, Plan& out) {
  struct Act {
    const Move* m = nullptr;  // technique, sort ou Limite
    std::string item;         // ou objet du sac
    bool limit = false;
    FighterP duo;             // technique en duo : le partenaire
  };
  std::vector<std::string> known = a->techs();
  for (auto& id : a->spells()) known.push_back(id);
  auto liveFoes = alive(foes), liveAllies = alive(allies);
  auto cures = [](const Move& m) {
    for (auto& e : m.effects)
      if (e.cure) return true;
    return false;
  };
  auto healed = [&](const Move& m, const FighterP& t) { return std::min(m.power * a->eMag() / 20 + 2, float(t->mhp - t->hp)); };
  // La cible n'a pas encore l'effet de l'action (état, bonus/malus, soin)
  auto lacks = [&](const Act& x, const FighterP& t) {
    if (!x.m) {
      if (x.item.empty()) return true;
      const ItemDef& d = item(x.item);
      return (d.healHp && t->hp < t->mhp) || (d.healMp && t->mp < t->mmp) || (d.cure && t->status != Status::None);
    }
    bool any = false;
    for (auto& e : x.m->effects) {
      const Fighter& w = e.self ? *a : *t;
      any = true;
      if (e.status != Status::None && w.status == Status::None && !immuneTo(w.S(), e.status)) return true;
      if (e.stat >= 0 && (e.stages > 0 ? w.stage[e.stat] < e.stages : w.stage[e.stat] > e.stages)) return true;
      if (e.cure && w.status != Status::None) return true;
    }
    return !any && (x.m->kind != Kind::Heal || t->hp < t->mhp);
  };
  auto holds = [&](const Tactic& tc, const Act& x, const FighterP& t) {
    const std::string& c = tc.cond;
    if (c == "ennemi_pv_moins" || c == "allie_pv_moins" || c == "soi_pv_moins") return t->hp * 100 < tc.value * t->mhp;
    if (c == "ennemi_pv_plus") return t->hp * 100 > tc.value * t->mhp;
    if (c == "allie_pm_moins" || c == "soi_pm_moins") return t->mmp > 0 && t->mp * 100 < tc.value * t->mmp;
    if (c == "ennemi_faible") return x.m && x.m->damaging() && moveEff(*x.m, t->S()) > 1;
    if (c == "ennemi_sans_effet" || c == "allie_sans_effet" || c == "soi_sans_effet") return lacks(x, t);
    if (c == "ennemi_boss") return t->boss;
    if (c == "ennemis_nombre") return (int)liveFoes.size() >= tc.value;
    if (c == "allie_ko") return !t->alive();
    if (c == "allie_etat") return t->status != Status::None;
    if (c == "allie_chef") return !allies.empty() && t == allies[0];
    if (c == "soi_limite") return a->lim >= 100;
    if (c == "soi_menace") return threatened(a);
    if (c == "allie_avant_pv_moins") return cellCol(std::max(0, cellOf(t))) == 0 && t->hp * 100 < tc.value * t->mhp;
    if (c == "ennemis_groupes") {
      if (!x.m) return false;
      if (x.m->zone == Area::All) return (int)liveFoes.size() >= tc.value;
      return (int)zoneTargets(*x.m, t).size() >= tc.value;
    }
    return true;  // ennemi, ennemi_pv_bas, ennemi_pv_haut, soi
  };
  // L'action sert à quelque chose sur cette cible (pas de soin sur un allié en pleine forme…)
  auto useful = [&](const Act& x, const FighterP& t) {
    if (x.limit) return true;
    if (x.m) {
      switch (x.m->kind) {
        case Kind::Physical:
        case Kind::Magic: return moveEff(*x.m, t->S()) > 0;
        case Kind::Heal: return (x.m->power > 0 && t->hp < t->mhp) || (cures(*x.m) && t->status != Status::None);
        case Kind::Revive: return !t->alive();
        default: return true;
      }
    }
    const ItemDef& d = item(x.item);
    if (d.capture > 0) return canCapture && !t->S().human;
    if (d.revive) return !t->alive();
    return (d.healHp && t->hp < t->mhp) || (d.healMp && t->mp < t->mmp) || (d.cure && t->status != Status::None);
  };
  // Ordre des cibles imposé par la condition (le plus petit d'abord)
  auto order = [&](const std::string& c, const FighterP& t) {
    float hpR = float(t->hp) / t->mhp, mpR = t->mmp ? float(t->mp) / t->mmp : 1.f;
    if (c == "ennemi_pv_bas") return float(t->hp);
    if (c == "ennemi_pv_haut") return -float(t->hp);
    if (c == "ennemi_pv_moins" || c == "allie_pv_moins") return hpR;
    if (c == "ennemi_pv_plus") return -hpR;
    if (c == "allie_pm_moins") return mpR;
    return 0.f;
  };

  int n = std::min((int)a->tactics.size(), tacticSlots(a->lvl));
  for (int i = 0; i < n; i++) {
    const Tactic& tc = a->tactics[i];
    const TacticCond* c = findTacticCond(tc.cond);
    if (!tc.on || !c || !tacticProblem(tc, nullptr).empty()) continue;
    // Actions possibles maintenant
    std::vector<Act> acts;
    auto canCast = [&](const std::string& id) {
      return std::find(known.begin(), known.end(), id) != known.end() && a->mp >= moveInfo(id).cost;
    };
    if (tc.kind == Tactic::Act::Move) {
      if (canCast(tc.act)) acts.push_back({&moveInfo(tc.act)});
    } else if (tc.kind == Tactic::Act::Item) {
      if (item(tc.act).battle && G.items.count(tc.act) && G.items[tc.act] > 0) acts.push_back({nullptr, tc.act});
    } else if (tc.act == "esquiver") {  // se mettre à l'abri : seulement si une attaque annoncée vise sa case
      if (!threatened(a)) continue;
      bool cond = c->side == TSide::Self ? holds(tc, Act{}, a) : true;
      int cell = safeCell(a);
      if (!cond || cell < 0) continue;
      out = Plan{};
      out.tactic = i, out.cell = cell;
      return true;
    } else if (tc.act == "limite") {
      if (a->lim >= 100 && !a->S().limit.empty()) acts.push_back({&moveInfo(a->S().limit), "", true});
    } else
      for (auto& id : known) {
        const Move& m = moveInfo(id);
        if (a->mp < m.cost) continue;
        const std::string& k = tc.act;
        if ((k == "attaque" && m.damaging()) || (k == "technique" && m.damaging() && m.cost == 0) ||
            (k == "soin" && m.kind == Kind::Heal && m.power > 0) || (k == "reanimation" && m.kind == Kind::Revive) ||
            (k == "guerison" && m.kind == Kind::Heal && cures(m)))
          acts.push_back({&m});
      }
    if (tc.kind == Tactic::Act::Auto && tc.act == "attaque")
      for (auto& d : duoOptions(a))  // sans vider les PM du partenaire (il soigne peut-être)
        if (moveInfo(d.move).damaging() && d.partner->mp - moveInfo(d.move).cost >= .4f * d.partner->mmp)
          acts.push_back({&moveInfo(d.move), "", false, d.partner});
    // Meilleure paire (action, cible) : d'abord l'ordre de la condition, puis l'effet de l'action
    bool found = false;
    float bestKey = 0, bestScore = 0;
    for (auto& x : acts) {
      bool revive = x.m ? x.m->kind == Kind::Revive : item(x.item).revive > 0;
      Target tg = x.m ? x.m->target : Target::Ally;
      bool multi = x.limit || tg == Target::AllEnemies || tg == Target::AllAllies;
      std::vector<FighterP> pool;
      if (c->side == TSide::Foe) pool = liveFoes;
      else if (c->side == TSide::Self) {
        if (!revive) pool = {a};
      } else if (revive) {
        for (auto& f : allies)
          if (!f->alive()) pool.push_back(f);
      } else pool = liveAllies;
      bool onAllies = x.limit ? tg == Target::AllAllies : c->side != TSide::Foe;
      auto& side = onAllies ? liveAllies : liveFoes;
      for (auto& t : pool) {
        if (!holds(tc, x, t) || !useful(x, t)) continue;
        float key = order(tc.cond, t), score = 1;
        if (x.m && x.m->damaging()) {
          score = 0;
          if (multi)
            for (auto& f : side) score += attackScore(a, *x.m, f) * .8f;
          else if (!reachable(a, *x.m, t))
            continue;  // corps à corps : hors de portée
          else score = zoneScore(a, *x.m, t);
        } else if (x.m && x.m->kind == Kind::Heal && x.m->power > 0) {
          score = 0;
          if (multi)
            for (auto& f : side) score += healed(*x.m, f);
          else score = healed(*x.m, t);
        }
        if (x.duo) score *= 1 - .45f * x.duo->atb / 100;  // le partenaire dépense sa jauge
        if (x.m) score -= x.m->cost * .3f;
        if (found && (key > bestKey + 1e-4f || (key > bestKey - 1e-4f && score <= bestScore))) continue;
        found = true;
        bestKey = key, bestScore = score;
        out = Plan{};
        out.tactic = i;
        if (x.m) out.move = x.m->id, out.limit = x.limit, out.duo = x.duo;
        else out.item = x.item;
        out.targets = multi ? side : std::vector<FighterP>{t};
      }
    }
    if (found) return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// Résolution des actions
// ---------------------------------------------------------------------------
int Battle::applyHit(const FighterP& a, const FighterP& d, const Move& m, float mult) {
  const Rules& R = rules();
  bool magic = m.kind == Kind::Magic;
  // Précision : l'esquive ne joue que contre les attaques physiques
  float hit = m.acc / 100.f * a->acc / 100.f - (magic ? 0 : d->eva / 100.f);
  if (!magic && cellCol(std::max(0, cellOf(d))) == BACK_COL) hit -= float(talentValue(d->S(), "esquive_arriere"));  // talent Furtif
  if (!magic && weather_ >= 0) hit += R.weathers[(size_t)weather_].acc / 100.f;                            // brume, blizzard
  if (const Rules::Terrain* tr = terrainAt(d); !magic && tr && tr->kind == "esquive") hit -= float(tr->value) / 100.f;  // herbes hautes
  if (frand() >= hit) {
    pop(d, T("Raté"), GREY);
    sound("rate");
    log.push_back(a->name() + " > " + d->name() + " : " + m.name + T(" ratée"));
    return -1;
  }
  float eff = moveEff(m, d->S());
  if (eff <= 0) {
    pop(d, T("Aucun effet"), GREY);
    sound("rate");
    return 0;
  }
  float A = magic ? a->eMag() : a->eAtk();
  float D = magic ? d->eRes() : d->eDef();
  float stab = hasType(a->S(), m.type) ? float(R.stab) : 1.f;
  float spread = float(R.spreadMin) + frand() * float(1 - R.spreadMin);
  bool crit = frand() * 100 < a->crit + m.critBonus + float(talentValue(a->S(), "critique"));  // talent Œil perçant
  float k = crit ? float(R.critMult) : 1.f;
  if (m.melee) {  // damier : l'arrière frappe et reçoit moins fort au corps à corps
    if (cellCol(std::max(0, cellOf(a))) == BACK_COL) k *= float(R.backRow);
    if (cellCol(std::max(0, cellOf(d))) == BACK_COL) k *= float(R.backRow);
  }
  k *= mult;
  if (d->broken) k *= float(R.breakDamage);  // Brisure : un ennemi brisé encaisse plus
  k *= 1 - float(talentValue(d->S(), magic ? "resiste_magie" : "resiste_physique"));  // talents Carapace, Écorce runique
  if (hasType(a->S(), m.type) && a->hp * 3 < a->mhp) k *= 1 + float(talentValue(a->S(), "rage"));  // talent Rage
  k *= weatherMult(m.type);                                                                          // météo
  k *= guardMult(d);                                                                                 // rôle : protecteur voisin
  if (const Rules::Terrain* tr = terrainAt(a); magic && tr && tr->kind == "magie") k *= 1 + float(tr->value);  // cristaux
  int dmg = std::max(1, int(((2 * a->lvl / 5.f + 2) * m.power * A / std::max(1.f, D) / float(R.dmgDivisor) + 2) * stab * eff * spread * k));
  d->hp = std::max(0, d->hp - dmg);
  if (eff > 1 && d->shieldMax > 0) {  // Brisure : une faiblesse entame le bouclier, et l'équipe la retient
    if (isAlly(a) && G.weak.insert(d->sp + ":" + std::to_string(m.type)).second) tip("faiblesse");
    if (!d->broken && d->alive()) {
      d->shield = std::max(0, d->shield - (eff >= 4 ? 2 : 1) - int(talentValue(a->S(), "briseur") + gemSupport(*a, "briseur")));  // talent, gemme
      if (d->shield == 0) breakFoe(d);
    }
  }
  setBlink(d);
  // Bruitage : sort selon l'élément (« sort_feu »…), coup selon le rythme (rapide, normal, lourd)
  std::string fx = crit ? "critique" : magic ? "sort_" + types()[m.type].id : m.pace == Pace::Quick ? "coup_rapide" : m.pace == Pace::Heavy ? "coup_lourd" : "coup";
  if (!audio::has(fx)) fx = magic ? "sort" : "coup";
  sound(fx);
  pop(d, std::to_string(dmg), eff > 1 ? GOLD : eff < 1 ? GREY : WHITE);
  if (crit) pop(d, T("Critique !"), RED), shakeT_ = G.time;
  if (eff > 1) pop(d, T("Efficace !"), GOLD);
  else if (eff < 1) pop(d, T("Résiste"), GREY);
  if (isAlly(d) || duel()) d->lim = std::min(100.f, d->lim + dmg * float(R.limitGain) / d->mhp);  // en duel, les deux équipes
  if (double steal = gemSupport(*a, "vol_vie"); steal > 0 && a->alive() && a->hp < a->mhp) {  // gemme de vol de vie
    int h = std::max(1, int(dmg * steal));
    a->hp = std::min(a->mhp, a->hp + h);
    pop(a, "+" + std::to_string(h), GREEN);
  }
  if (m.melee && a->alive() && d->alive() && !countering_ && frand() < gemSupport(*d, "contre") && !d->techs().empty()) {  // gemme de riposte
    const Move& c = moveInfo(d->techs()[0]);
    if (c.damaging()) {
      countering_ = true;
      pop(d, T("Riposte !"), rgb(0xffd34d));
      log.push_back("  " + d->name() + T(" riposte"));
      if (applyHit(d, a, c, .7f) >= 0 && !a->alive()) knockOut(a);
      countering_ = false;
      if (!a->alive()) return dmg;
    }
  }
  if (m.melee && a->alive()) {  // talent Peau piquante : qui le frappe au corps à corps se blesse
    double v = talentValue(d->S(), "piquant");
    if (v > 0) {
      int thorn = std::max(1, int(a->mhp * v));
      a->hp = std::max(0, a->hp - thorn);
      pop(a, std::to_string(thorn), GREY);
      if (!a->alive()) knockOut(a);
    }
  }
  log.push_back(a->name() + " > " + d->name() + " : " + m.name + " " + std::to_string(dmg) + (crit ? T(" critique") : "") +
                (eff > 1 ? T(" efficace") : eff < 1 ? T(" résiste") : "") + T(" (reste ") + std::to_string(d->hp) + "/" + std::to_string(d->mhp) + ")");
  return dmg;
}

// ---------------------------------------------------------------------------
// Pousser / tirer, météo et terrain (2.6)
// ---------------------------------------------------------------------------
int Battle::shoveDest(const FighterP& t, const Move& m) const {
  if (t->boss) return -2;
  int c = cellOf(t);
  if (c < 0) return -2;
  int col = cellCol(c) + m.shove;
  if (col < 0 || col > BACK_COL) return -1;
  return cellAt(cellRow(c), col);
}

void Battle::shove(const FighterP& a, const FighterP& t, const Move& m, int dmg, std::vector<FighterP>& ko) {
  int to = shoveDest(t, m);
  if (isAlly(a)) tip("pousser");
  if (to == -2) {
    if (t->boss) pop(t, T("Inébranlable"), GREY);
    return;
  }
  int clash = std::max(1, int(dmg * rules().shoveClash));
  auto hurt = [&](const FighterP& f) {
    f->hp = std::max(0, f->hp - clash);
    pop(f, std::to_string(clash), rgb(0xffb070));
    setBlink(f);
    if (!f->alive() && std::find(ko.begin(), ko.end(), f) == ko.end()) ko.push_back(f);
  };
  if (to == -1) {  // pousser contre le bord du damier : choc ; tirer depuis l'avant : rien
    if (m.shove > 0) {
      hurt(t);
      pop(t, T("Choc !"), rgb(0xffb070));
      shakeT_ = G.time;
      log.push_back("  " + t->name() + T(" poussé contre le bord : choc ") + std::to_string(clash));
    }
    return;
  }
  FighterP occ;
  for (auto& o : sideOf(t))
    if (o != t && cellOf(o) == to) occ = o;
  if (occ && occ->alive()) {  // case prise : les deux se heurtent
    hurt(t), hurt(occ);
    pop(t, T("Choc !"), rgb(0xffb070));
    shakeT_ = G.time;
    log.push_back("  " + t->name() + T(" heurte ") + occ->name() + T(" : choc ") + std::to_string(clash));
    return;
  }
  if (occ) cells_[occ.get()] = cellOf(t);  // un combattant K.O. sur la case : il prend la place
  cells_[t.get()] = to;
  pop(t, m.shove > 0 ? T("Recule") : T("Avance"), rgb(0xd8e0ff));
  log.push_back("  " + t->name() + (m.shove > 0 ? T(" poussé : ") : T(" tiré : ")) + cellName(to));
  if (terrainTurn(t, true) && std::find(ko.begin(), ko.end(), t) == ko.end()) ko.push_back(t);
}

const Rules::Terrain* Battle::terrainOf(bool allySide, int cell) const {
  if (cell < 0 || cell >= N_CELLS) return nullptr;
  int i = terrain_[allySide ? 0 : 1][(size_t)cell];
  return i >= 0 && i < (int)rules().terrains.size() ? &rules().terrains[(size_t)i] : nullptr;
}
const Rules::Terrain* Battle::terrainAt(const FighterP& f) const { return terrainOf(isAlly(f), cellOf(f)); }
bool Battle::terrainHits(const FighterP& f, const Rules::Terrain* t) const {
  if (!t) return false;
  for (auto& ty : t->immune)
    for (size_t k = 0; k < types().size(); k++)
      if (types()[k].id == ty && hasType(f->S(), (int)k)) return false;
  return true;
}
bool Battle::terrainHurts(const FighterP& f, int cell) const {
  const Rules::Terrain* t = terrainOf(isAlly(f), cell);
  if (!t || !terrainHits(f, t)) return false;
  return t->kind == "degats" || t->kind == "lent" || (t->kind == "poison" && f->status == Status::None && !immuneTo(f->S(), Status::Poison));
}
bool Battle::terrainTurn(const FighterP& f, bool arrived) {
  const Rules::Terrain* t = terrainAt(f);
  if (!t || !f->alive() || !terrainHits(f, t)) return false;
  if (t->kind == "degats") {
    int dmg = std::max(1, int(f->mhp * t->value));
    f->hp = std::max(0, f->hp - dmg);
    setBlink(f);
    pop(f, std::to_string(dmg), rgb(t->color));
    log.push_back("  " + f->name() + " : " + t->name + " " + std::to_string(dmg));
    return !f->alive();
  }
  if (t->kind == "poison" && f->status == Status::None && !immuneTo(f->S(), Status::Poison) && frand() < t->value) {
    f->status = Status::Poison;
    pop(f, statusName(Status::Poison), rgb(statusColor(Status::Poison)));
    log.push_back("  " + f->name() + " : " + t->name + T(", empoisonné"));
  }
  if (t->kind == "soin" && !arrived && f->hp < f->mhp) {
    int h = std::max(1, int(f->mhp * t->value));
    f->hp = std::min(f->mhp, f->hp + h);
    pop(f, "+" + std::to_string(h), GREEN);
  }
  return false;
}
float Battle::weatherMult(int type) const {
  if (weather_ < 0 || type < 0 || type >= (int)types().size()) return 1;
  const auto& tm = rules().weathers[(size_t)weather_].typeMult;
  auto it = tm.find(types()[(size_t)type].id);
  return it == tm.end() ? 1.f : float(it->second);
}

void Battle::initFoe(const FighterP& e) {
  if (e->shieldMax <= 0) e->shieldMax = e->S().shield > 0 ? e->S().shield : e->boss ? rules().breakShieldBoss : rules().breakShield;
  e->shieldMax += int(talentValue(e->S(), "cuirasse"));  // talent Cuirasse : bouclier plus gros
  e->shield = e->shieldMax, e->broken = false;
}

void Battle::checkPhase(const FighterP& f) {
  if (net_ != Net::None || !f->alive() || isAlly(f) || !f->phases.is_array()) return;
  while (f->phase < (int)f->phases.size()) {
    const Json& p = f->phases[(size_t)f->phase];
    if (f->hp > jget(p, "pv", .5f) * f->mhp) return;
    f->phase++;
    log.push_back("  " + f->name() + T(" : phase ") + std::to_string(f->phase + 1));
    tip("phase");
    sc.say(p.contains("message") ? tr(jget<std::string>(p, "message", "")) : f->name() + T(" entre dans une nouvelle phase !"), 1.4f);
    sound("statut");
    flashT = G.time, flashCol = rgb(0xff5a3a);
    shakeT_ = G.time;
    int sh = jget(p, "bouclier", -1);
    if (sh >= 0) f->shieldMax = sh;
    f->shield = f->shieldMax, f->broken = false;  // il se ressaisit, bouclier rechargé
    for (auto& id : jget(p, "techniques", std::vector<std::string>{}))
      if (hasMove(id)) f->bonusMoves.push_back(id);
    Json b = p.value("bonus", Json::object());
    for (auto& [st, v] : b.items()) f->stage[(size_t)stageOf(st)] = std::clamp(f->stage[(size_t)stageOf(st)] + v.get<int>(), -3, 3);
    if (float heal = jget(p, "soin", 0.f); heal > 0) {
      int h = int(f->mhp * heal);
      f->hp = std::min(f->mhp, f->hp + h);
      pop(f, "+" + std::to_string(h), GREEN);
    }
    std::string w = jget<std::string>(p, "meteo", "");
    for (size_t i = 0; i < rules().weathers.size(); i++)
      if (rules().weathers[i].id == w) weather_ = (int)i;
    for (auto& r : p.value("renforts", Json::array())) {
      if (alive(foes).size() >= 6) break;
      int cell = -1;
      for (int col = 0; col <= BACK_COL && cell < 0; col++)
        for (int row = 0; row < GRID && cell < 0; row++) {
          int c = cellAt(row, col);
          bool taken = false;
          for (auto& o : foes) taken = taken || (o->alive() && cellOf(o) == c);
          if (!taken) cell = c;
        }
      if (cell < 0) break;
      FighterP n = makeFoe(r, std::max(1, f->lvl - 4));
      initFoe(n);
      n->atb = frand() * 30;
      cells_[n.get()] = cell;
      foes.push_back(n);
      roster_.push_back(n);
      G.see(*n);
      sc.say(n->name() + T(" vient à l'aide de ") + f->name() + " !", 1.0f);
    }
  }
}

float Battle::guardMult(const FighterP& d) const {
  if (d->S().combatRole == "protecteur") return 1;
  for (auto& o : sideOf(d))
    if (o != d && o->alive() && o->S().combatRole == "protecteur" && inZone(Area::Cross, cellOf(o), cellOf(d))) return float(1 - rules().roleGuard);
  return 1;
}

bool Battle::roleHeal(const FighterP& e) {
  if (e->S().combatRole != "soigneur" || isAlly(e) || !hasMove("soin")) return false;
  FighterP worst;
  for (auto& o : alive(foes))
    if (o->hp * 2 < o->mhp && (!worst || o->hp * worst->mhp < worst->hp * o->mhp)) worst = o;
  if (!worst || frand() < .25f) return false;
  useMove(e, "soin", {worst});
  return true;
}

// Brisure : bouclier à zéro. Sa jauge retombe, son attaque annoncée est annulée, il perd son prochain tour
void Battle::breakFoe(const FighterP& d) {
  d->broken = true;
  d->atb = 0;
  for (auto it = warnings_.begin(); it != warnings_.end();)
    if (it->who == d) it = warnings_.erase(it);
    else ++it;
  pop(d, T("BRISÉ !"), GOLD);
  sound("critique");
  flashT = G.time, flashCol = rgb(0xffe066);
  shakeT_ = G.time;
  log.push_back("  " + d->name() + T(" : BRISÉ"));
  tip("brisure");
}

void Battle::applyEffect(const FighterP& t, const Effect& e) {
  if (!t->alive() || frand() * 100 >= e.chance) return;
  if (e.cure && t->status != Status::None) {
    t->status = Status::None;
    pop(t, T("Guéri"), GREEN);
  }
  if (e.status != Status::None) {
    if (immuneTo(t->S(), e.status)) {
      if (e.chance >= 100) pop(t, T("Insensible"), GREY);
    } else if (t->status == Status::None) {
      t->status = e.status;
      t->statusTurns = e.status == Status::Sleep ? irand(rules().sleepMin, rules().sleepMax) : 0;
      pop(t, statusName(e.status), rgb(statusColor(e.status)));
      sound("statut");
      log.push_back("  " + t->name() + " : " + statusName(e.status));
    }
  }
  if (e.stat >= 0) {
    int old = t->stage[e.stat];
    t->stage[e.stat] = std::clamp(old + e.stages, -3, 3);
    if (t->stage[e.stat] != old) {
      pop(t, std::string(stageName(e.stat)) + (e.stages > 0 ? " +" : " -"), e.stages > 0 ? BLUE : GREY);
      sound(e.stages > 0 ? "bonus" : "malus");
      log.push_back("  " + t->name() + " : " + stageName(e.stat) + " " + std::to_string(t->stage[e.stat]));
    }
  }
}

void Battle::knockOut(const FighterP& f) {
  log.push_back("  " + f->name() + T(" K.O."));
  f->clearBattle();
  f->atb = 0;
  if (!isAlly(f) && std::find(defeated.begin(), defeated.end(), f) == defeated.end()) defeated.push_back(f);
}

void Battle::useMove(FighterP a, const std::string& id, std::vector<FighterP> targets, bool isLimit, float boost) {
  if (follower()) {  // l'invité envoie son ordre ; l'hôte (ou le chef) le joue et renvoie le résultat
    Json t = Json::array();
    for (auto& f : targets) t.push_back(mine(f));
    send_(net_ == Net::Follow ? leader_ : -1, Json{{"t", "ordre"}, {"i", mine(a)[1]}, {"move", id}, {"limite", isLimit}, {"cibles", t}});
    G.menus.clear();
    cursor = nullptr;
    actor = nullptr;
    return;
  }
  G.menus.clear();
  cursor = nullptr;
  aiming_ = nullptr;
  const Move& m = moveInfo(id);
  // Zone autour d'une cible : les combattants voisins sont touchés aussi, un peu moins fort
  FighterP center = boost == 1 && m.zone != Area::Single && m.zone != Area::All && targets.size() == 1 ? targets[0] : nullptr;
  if (center) targets = zoneTargets(m, center);
  if (m.cost > 0) a->mp = std::max(0, a->mp - m.cost);
  if (isLimit) a->lim = 0;
  startAnim(a, targets, fxOf(m), rgb(types()[m.type].color), isLimit, m.damaging() ? m.type : -1);
  if (duoPartner_) sc.say(T("DUO — ") + a->name() + T(" et ") + duoPartner_->name() + " : " + m.name + " !", .9f);
  else sc.say(isLimit ? T("LIMITE — ") + a->name() + " : " + m.name + " !" : a->name() + " : " + m.name, isLimit ? .9f : .55f);
  bool duo = duoPartner_ != nullptr;
  auto ko = std::make_shared<std::vector<FighterP>>();
  sc.call([this, a, targets, center, &m, isLimit, ko, boost, duo] {
    if (isLimit || duo) sound("limite");
    if (m.kind == Kind::Heal || m.kind == Kind::Revive) sound("soin");
    if (isLimit || duo || m.target == Target::AllEnemies) {
      flashT = G.time;
      flashCol = m.kind == Kind::Heal ? rgb(0xfff4c0) : isLimit ? rgb(0xff8ac8) : duo ? rgb(0xffe066) : rgb(0xffffff);
      if (net_ == Net::Host || net_ == Net::Lead) send_(-1, Json{{"t", "eclair"}, {"c", Json::array({flashCol.r, flashCol.g, flashCol.b})}});
    }
    bool landed = false;
    for (auto& t : targets) {
      if (m.kind == Kind::Revive) {
        if (t->alive()) continue;
        t->clearBattle();
        t->hp = std::max(1, t->mhp * m.power / 100);
        t->atb = 0;
        pop(t, "+" + std::to_string(t->hp), GREEN);
        landed = true;
        continue;
      }
      if (!t->alive()) continue;
      if (m.kind == Kind::Heal) {
        if (m.power > 0) {
          int amt = std::max(1, int(m.power * a->eMag() / 20) + irand(0, 4));
          amt = std::min(amt, t->mhp - t->hp);
          t->hp += amt;
          pop(t, "+" + std::to_string(amt), GREEN);
          log.push_back(a->name() + " > " + t->name() + " : " + m.name + " +" + std::to_string(amt));
        }
        for (auto& e : m.effects)
          if (!e.self) applyEffect(t, e);
        landed = true;
      } else if (m.kind == Kind::Status) {
        bool hostile = isAlly(a) != isAlly(t);
        if (hostile && frand() >= m.acc / 100.f * a->acc / 100.f) {
          pop(t, T("Raté"), GREY);
          continue;
        }
        for (auto& e : m.effects)
          if (!e.self) applyEffect(t, e);
        landed = true;
      } else {
        int dmg = applyHit(a, t, m, (center && t != center ? float(rules().zoneSide) : 1.f) * boost);
        if (dmg < 0) continue;
        landed = true;
        if (!t->alive()) ko->push_back(t);
        else {
          for (auto& e : m.effects)
            if (!e.self) applyEffect(t, e);
          if (m.shove && t == targets[0] && dmg > 0) shove(a, t, m, dmg, *ko);
        }
      }
    }
    if (landed || m.kind == Kind::Status)
      for (auto& e : m.effects)
        if (e.self) applyEffect(a, e);
    for (auto& f : *ko) knockOut(f);
    for (auto& t : targets) checkPhase(t);  // un boss passe peut-être une phase
  });
  sc.wait(.75f);
  sc.call([this, ko] {
    if (!ko->empty()) sound("ko");
    for (auto& f : *ko) sc.say(f->name() + (isAlly(f) || duel() ? T(" est K.O. !") : T(" est vaincu !")), .75f);
  });
  float gauge = isLimit ? 0.f : paceGauge(m);
  sc.call([this, a, gauge] { afterAction(a, gauge); });
}

void Battle::useItem(FighterP a, const std::string& id, FighterP t) {
  if (duel()) return;  // pas d'objets en duel
  G.menus.clear();
  cursor = nullptr;
  if (net_ == Net::Follow) {  // gardien : l'objet est pris dans mon sac, le chef applique son effet
    G.items[id]--;
    send_(leader_, Json{{"t", "ordre"}, {"i", mine(a)[1]}, {"objet", id}, {"cibles", Json::array({mine(t)})}});
    actor = nullptr;
    return;
  }
  if (mineToCommand(a)) G.items[id]--;  // l'objet d'un autre joueur a déjà été pris dans son sac
  const ItemDef& d = item(id);
  startAnim(a, {t}, d.capture > 0 ? Fx::Capture : Fx::Heal, d.capture > 0 ? rgb(0xffe27a) : rgb(0x7dffa8));
  sc.say(a->name() + T(" utilise : ") + d.name, .7f);
  sc.call([this, a, id, t] {
    const ItemDef& d = item(id);
    sound(d.capture > 0 ? "lancer" : "soin");
    if (d.revive) {
      t->clearBattle();
      t->hp = std::max(1, t->mhp * d.revive / 100);
      t->atb = 0;
      pop(t, "+" + std::to_string(t->hp), GREEN);
      sc.say(t->name() + T(" se relève !"), .9f);
    } else if (d.capture <= 0) {
      if (d.healHp) {
        int amt = std::min(d.healHp, t->mhp - t->hp);
        t->hp += amt;
        pop(t, "+" + std::to_string(amt), GREEN);
      }
      if (d.healMp) {
        int amt = std::min(d.healMp, t->mmp - t->mp);
        t->mp += amt;
        pop(t, "+" + std::to_string(amt) + T(" PM"), BLUE);
      }
      if (d.cure && t->status != Status::None) {
        t->status = Status::None;
        pop(t, T("Guéri"), GREEN);
      }
    } else {
      float c = captureChance(*t, d.capture);
      flashT = G.time;
      flashCol = rgb(0xfff0a0);
      if (frand() < c) {
        foes.erase(std::find(foes.begin(), foes.end(), t));
        t->tag.clear();
        t->clearBattle();
        t->hp = t->mhp;
        t->mp = t->mmp;
        t->lim = 0;
        t->atb = 0;
        bool full = (int)G.team.size() >= rules().maxTeam;  // équipe complète : elle part dans la réserve
        (full ? G.reserve : G.team).push_back(t);
        G.caught.insert(t->sp);
        sc.call([this] { sound("capture"); });
        sc.say(T("Capturé ! ") + t->name() + (full ? T(" est envoyé dans la réserve (Échap > Réserve).") : T(" rejoint l'équipe.")), 1.5f);
      } else {
        sc.call([this] { sound("capture_rate"); });
        sc.say(t->name() + T(" s'échappe de la lumière !"), 1.0f);
      }
    }
  });
  sc.wait(.4f);
  sc.call([this, a] { afterAction(a); });
}

void Battle::moveTo(FighterP a, int cell) {
  G.menus.clear();
  if (follower()) {  // l'invité envoie son ordre ; l'hôte (ou le chef) vérifie et déplace
    send_(net_ == Net::Follow ? leader_ : -1, Json{{"t", "ordre"}, {"i", mine(a)[1]}, {"deplacer", cell}});
    actor = nullptr;
    return;
  }
  cells_[a.get()] = cell;
  log.push_back(a->name() + T(" se déplace : ") + cellName(cell));
  sc.say(a->name() + T(" se déplace."), .45f);
  sc.call([this, a] { afterAction(a, float(rules().quickGauge)); });
}

void Battle::swapIn(FighterP a, FighterP r) {
  G.menus.clear();
  cells_[r.get()] = cellOf(a);  // le remplaçant prend sa case
  for (auto& x : allies)
    if (x == a) x = r;
  auto ia = std::find(G.team.begin(), G.team.end(), a), ib = std::find(G.team.begin(), G.team.end(), r);
  std::iter_swap(ia, ib);
  a->stage.fill(0);  // les bonus et malus ne suivent pas celui qui recule
  r->atb = 0;
  a->atb = 0;
  sc.say(a->name() + T(" recule. ") + r->name() + T(" entre au combat !"), 1.1f);
  sc.call([this, r] {
    actor = nullptr;
    r->atb = 0;
  });
}

void Battle::tryFlee(FighterP a) {
  G.menus.clear();
  sc.call([this, a] {
    if (frand() < rules().flee) {
      sound("fuite");
      sc.say(T("L'équipe prend la fuite !"), 1.0f);
      sc.call([this] { finish(BattleResult::Fled); });
    } else {
      sc.say(T("Impossible de fuir !"), .9f);
      sc.call([this, a] { afterAction(a); });
    }
  });
}

void Battle::afterAction(FighterP a, float gauge) {
  a->atb = gauge;
  actor = nullptr;
  // Poison et brûlure : dégâts à la fin du tour de celui qui en souffre
  if (a->alive() && (a->status == Status::Poison || a->status == Status::Burn)) {
    bool poison = a->status == Status::Poison;
    int dmg = std::max(1, int(a->mhp * (poison ? rules().poisonDmg : rules().burnDmg)));
    a->hp = std::max(0, a->hp - dmg);
    setBlink(a);
    pop(a, std::to_string(dmg), rgb(statusColor(a->status)));
    sound("statut");
    log.push_back("  " + a->name() + " : " + statusName(a->status) + " " + std::to_string(dmg));
    sc.say(a->name() + (poison ? T(" souffre du poison.") : T(" souffre de sa brûlure.")), .6f);
    if (!a->alive()) {
      knockOut(a);
      sc.say(a->name() + (isAlly(a) || duel() ? T(" est K.O. !") : T(" est vaincu !")), .75f);
    }
  }
  // Case de terrain (braises, ronces, source)
  if (a->alive() && terrainTurn(a, false)) {
    knockOut(a);
    sc.say(a->name() + (isAlly(a) || duel() ? T(" est K.O. !") : T(" est vaincu !")), .75f);
  }
  // Talents de fin de tour
  if (a->alive()) {
    int regen = int(a->mhp * talentValue(a->S(), "regeneration")), mpGain = int(a->mmp * talentValue(a->S(), "meditation"));
    if (regen > 0 && a->hp < a->mhp) {
      a->hp = std::min(a->mhp, a->hp + regen);
      pop(a, "+" + std::to_string(regen), GREEN);
    }
    if (mpGain > 0) a->mp = std::min(a->mmp, a->mp + mpGain);
    double heal = talentValue(a->S(), "soin_voisins");
    if (heal > 0)
      for (auto& o : sideOf(a))
        if (o != a && o->alive() && o->hp < o->mhp && inZone(Area::Cross, cellOf(a), cellOf(o))) {
          int h = std::max(1, int(o->mhp * heal));
          o->hp = std::min(o->mhp, o->hp + h);
          pop(o, "+" + std::to_string(h), GREEN);
        }
  }
  sc.call([this] { checkEnd(); });
}

void Battle::checkEnd() {
  if (duel()) {
    if (!alive(foes).empty() && !alive(allies).empty()) return;
    bool won = alive(foes).empty();
    ending_ = true;
    sound(won ? "victoire" : "defaite", false);
    sc.say(names_[won ? 0 : 1] + T(" remporte le duel !"), 1.8f);
    sc.call([this, won] { finish(won ? BattleResult::Win : BattleResult::Lose); });
    return;
  }
  // Renforts ennemis : remplacent ceux qui sont tombés
  for (size_t i = 0; i < foes.size(); i++)
    if (!foes[i]->alive() && !reserve.empty()) {
      FighterP n = reserve.front();
      reserve.erase(reserve.begin());
      cells_[n.get()] = cellOf(foes[i]);  // le renfort prend la case de l'ennemi tombé
      G.see(*n);  // bestiaire
      foes[i] = n;
      n->atb = frand() * 30;
      if (net_ == Net::Lead) send_(-1, Json{{"t", "renfort"}, {"i", (int)i}});
      sc.say(foeName.empty() ? n->name() + T(" entre dans le combat !") : foeName + T(" envoie ") + n->name() + " !", 1.1f);
    }
  // Gardien à plusieurs : chacun termine le combat chez lui (expérience et or de son équipe)
  if (net_ == Net::Lead && (alive(foes).empty() || alive(allies).empty())) {
    send_(-1, netState());
    send_(-1, Json{{"t", "issue"}, {"gagne", alive(foes).empty()}});
  }
  if (alive(foes).empty()) victory();
  else if (alive(allies).empty()) {
    ending_ = true;
    sound("defaite", false);
    sc.say(T("Toute l'équipe est à terre…"), 1.6f);
    sc.call([this] { finish(BattleResult::Lose); });
  }
}

void Battle::victory() {
  ending_ = true;
  const Rules& R = rules();
  int total = 0, goldGain = 0;
  for (auto& d : defeated) {
    total += d->lvl * R.xpPerLevel * (d->boss ? R.xpBoss : 1);
    goldGain += d->lvl * R.goldPerLevel * (d->boss ? R.goldBoss : 1);
  }
  sound("victoire", false);  // chacun joue la fin chez lui
  sc.wait(.6f);
  int share = int(total * R.xpFront + 1e-6), res = int(total * R.xpReserve + 1e-6);
  // Tous les gains d'un coup, dans une fenêtre (Battle::update la ferme sur Entrée)
  sc.call([this, share, res, goldGain] {
    const Rules& R = rules();
    summary_.clear();
    bool up = false;
    if (share > 0) {
      for (auto& a : alive(allies)) {
        if (!mineToCommand(a)) continue;  // gardien à plusieurs : chacun ne fait progresser que ses combattants
        int before = a->lvl;
        auto msgs = gainXp(*a, share);
        summary_.push_back(a->name() + " : +" + std::to_string(share) + T(" exp") + (a->lvl > before ? T(" · niveau ") + std::to_string(a->lvl) + " !" : ""));
        for (auto& m : msgs)
          if (m.find(T(" apprend ")) != std::string::npos) summary_.push_back("   " + m);
        up = up || a->lvl > before;
      }
      bool others = false;
      for (auto& m : G.team)
        if (!isAlly(m) && m->alive()) gainXp(*m, res), others = true;
      if (others && res > 0) summary_.push_back(T("Le reste de l'équipe : +") + std::to_string(res) + T(" exp"));
    }
    if (goldGain > 0) {
      G.gold += goldGain;
      summary_.push_back(T("Or : +") + std::to_string(goldGain));
    }
    if (!boss) {
      // Un seul objet au plus : les chances s'additionnent dans l'ordre de la liste
      float r = frand(), acc = 0;
      for (auto& [id, chance] : R.drops) {
        acc += float(chance);
        if (r < acc) {
          G.items[id]++;
          summary_.push_back(T("Objet trouvé : ") + item(id).name);
          break;
        }
      }
      // Matériaux de la forge : selon le type de chaque créature vaincue
      for (auto& f : roster_)
        if (!f->S().human)
          for (auto& td : R.typeDrops)
            if (hasType(f->S(), typeOf(td.type)) && frand() < float(td.chance)) {
              G.items[td.item]++;
              summary_.push_back(T("Objet trouvé : ") + item(td.item).name);
            }
    }
    if (up) sound("niveau", false);
    for (auto& l : summary_) log.push_back("  " + l);
    summaryOn_ = true;
    summaryT_ = G.time;
    sc.halt();
  });
  sc.call([this] { finish(BattleResult::Win); });
}

void Battle::finish(BattleResult r) {
  result_ = r;
  finished_ = true;
  G.menus.clear();
  for (auto& f : G.team) {
    f->atb = 0;
    f->clearBattle();
  }
}

// ---------------------------------------------------------------------------
// Dessin
// ---------------------------------------------------------------------------
// Dalle d'une case (losange) : pleine, ou seulement son contour
static void tile(Gfx& g, Pt p, Color c, bool outline = false) {
  std::vector<Pt> d{{p.x - 14, p.y + 13}, {p.x, p.y + 8}, {p.x + 14, p.y + 13}, {p.x, p.y + 18}};
  if (!outline) return g.poly(d, c);
  for (int i = 0; i < 4; i++) g.line(d[i].x, d[i].y, d[(i + 1) % 4].x, d[(i + 1) % 4].y, 1, c);
}

// Zones sur le damier (3.2) : technique survolée dans le menu (aperçu autour de la cible probable), cible ou case
// en train d'être choisie. Or : la case visée ; orange : le reste de la zone ; contour blanc : les autres choix.
void Battle::drawZones(float t) {
  Gfx& g = G.g;
  float pulse = .5f + .5f * std::sin(t * 7);
  bool picking = boardPicking();
  const Move* mv = nullptr;
  int center = -1;
  bool side = false;  // camp de la zone (vrai : alliés)
  if (picking) {
    Menu& m = G.menus.top();
    side = pick_.allySide;
    for (size_t i = 0; i < pick_.cells.size() && i < m.items.size(); i++)
      if ((int)i != m.sel && m.items[i].enabled) tile(g, cellPos(side, pick_.cells[i]), rgb(0xffffff, uint8_t(90 + 60 * pulse)), true);
    if (m.sel >= 0 && m.sel < (int)pick_.cells.size()) center = pick_.cells[(size_t)m.sel];
    if (!pick_.move) mv = aiming_;
    if (pick_.move && center >= 0 && actor) {  // le combattant en transparence sur la case choisie
      Pt p = cellPos(true, center);
      g.alpha = .45f;
      drawFighterSprite(g, *actor, p.x, p.y, 1, true, t);
      g.alpha = 1;
    }
  } else if (!preview_.empty() && previewDepth_ == G.menus.depth() && G.menus.active() && actor) {
    mv = &moveInfo(preview_);
    if (mv->target == Target::Enemy) {
      if (cursor && cursor->alive()) center = cellOf(cursor);
      // corps à corps : les ennemis hors de portée sont barrés
      for (auto& f : alive(foes))
        if (!reachable(actor, *mv, f)) {
          Pt p = cellPos(false, cellOf(f));
          g.line(p.x - 6, p.y + 10, p.x + 6, p.y + 16, 2, rgb(0xff5a4a, 200));
          g.line(p.x + 6, p.y + 10, p.x - 6, p.y + 16, 2, rgb(0xff5a4a, 200));
        }
    } else if (mv->target == Target::Ally || mv->target == Target::AllyKO) {
      side = true;
      center = cellOf(actor);
    } else side = mv->target == Target::AllAllies;
  }
  if (!mv && center >= 0) {  // objet, allié, case : seulement la case choisie
    tile(g, cellPos(side, center), rgb(0xffd34d, uint8_t(150 + 80 * pulse)));
    return;
  }
  if (!mv) return;
  bool all = mv->zone == Area::All || mv->target == Target::AllEnemies || mv->target == Target::AllAllies;
  for (int c = 0; c < N_CELLS; c++) {
    bool main = c == center, in = all || (center >= 0 && inZone(mv->zone, center, c));
    if (!in) continue;
    tile(g, cellPos(side, c), main ? rgb(0xffd34d, uint8_t(150 + 80 * pulse)) : rgb(0xff9a3a, uint8_t(90 + 50 * pulse)));
    if (main) tile(g, cellPos(side, c), rgb(0xfff6c8, 230), true);
  }
}

// ATH (3.2) : un tableau en bas à droite (à gauche : le menu de commande), une ligne par allié avec ses PV
// (chiffres et barre colorée), ses PM, sa jauge ATB (or : prêt ; rouge : en retard après une technique lourde)
// et sa Limite (rose, clignote quand elle est prête). Les compagnons sont en retrait sous leur héros. Au-dessus,
// les titres des colonnes, le mode (Tab), les renforts ennemis.
void Battle::drawHud(float t) {
  Gfx& g = G.g;
  Color lab = rgb(0xaab3d8);
  size_t n = std::min<size_t>(allies.size(), 6);
  const float hx = 104, rowH = 10, hy = SCREEN_H - 2 - rowH * n;
  g.rect(hx - 4, hy - 13, g.right() - hx + 4, SCREEN_H - hy + 13, rgb(0x060a24, 190));
  g.rect(hx - 4, hy - 13, g.right() - hx + 4, 1, rgb(0x8090c8, 120));
  bool flash = t - toggledT_ < 1.2f && int(t * 8) % 2;  // Tab : mode auto activé ou coupé
  g.text(hx, hy - 12, G.tacticsAuto ? T("Auto") : T("Manuel"), flash ? WHITE : G.tacticsAuto ? GOLD : lab);
  g.text(hx + 92, hy - 12, T("PV"), lab, 2);
  g.text(hx + 152, hy - 12, T("PM"), lab, 2);
  g.text(hx + 175, hy - 12, T("ATB"), lab, 1);
  g.text(hx + 203, hy - 12, T("Lim"), lab, 1);
  for (size_t i = 0; i < n; i++) {
    auto& a = allies[i];
    float y = hy + i * rowH;
    bool comp = companion(a);
    if (actor == a) g.rect(hx - 3, y - 1, 216, rowH, rgb(0xffd34d, 40));
    // Gardien à plusieurs : les combattants des autres joueurs en bleu
    Color nc = !a->alive() ? rgb(0xff7b6b) : !mineToCommand(a) ? BLUE : actor == a ? GOLD : comp ? rgb(0xd8def2) : WHITE;
    g.text(hx + (comp ? 5 : 0), y - 2, utf8Prefix(a->name(), comp ? 6 : 7), nc);
    if (a->status != Status::None && a->alive()) {  // état : sa pastille à la fin du nom
      Color sc2 = rgb(statusColor(a->status));
      g.rect(hx + 45, y + 1, 5, 5, sc2);
    }
    float r = a->mhp ? float(a->hp) / a->mhp : 0;
    Color hc = r > .5f ? GREEN : r > .25f ? GOLD : RED;
    g.text(hx + 92, y - 2, std::to_string(std::max(0, a->hp)), a->alive() ? (r <= .25f ? hc : WHITE) : rgb(0xff7b6b), 2);
    g.rect(hx + 95, y + 2, 34, 4, rgb(0x1a2050));
    g.rect(hx + 95, y + 2, 34 * std::clamp(r, 0.f, 1.f), 4, hc);
    g.rect(hx + 95, y + 2, 34 * std::clamp(r, 0.f, 1.f), 1, shade(hc, -.35f));
    g.text(hx + 152, y - 2, std::to_string(a->mp), rgb(0x9fd0ff), 2);
    // Jauge ATB
    float bx = hx + 158, bw = 34;
    g.rect(bx, y + 2, bw, 4, rgb(0x1a2050));
    if (a->alive() && a->atb < 0) g.rect(bx, y + 2, bw * std::clamp(-a->atb / 100, 0.f, 1.f), 4, RED);
    else if (a->alive()) g.rect(bx, y + 2, bw * std::clamp(a->atb / 100, 0.f, 1.f), 4, a->atb >= 100 ? GOLD : rgb(0xe8ecff));
    // Limite
    float lx = hx + 196, lw = 14, lr = std::clamp(a->lim / 100, 0.f, 1.f);
    g.rect(lx, y + 2, lw, 4, rgb(0x1a2050));
    g.rect(lx, y + 2, lw * lr, 4, lr >= 1 && int(t * 5) % 2 ? WHITE : rgb(0xff6fb0));
  }

}

void Battle::drawStatusTag(const Fighter& f, float x, float y) {
  if (f.status == Status::None || !f.alive()) return;
  Gfx& g = G.g;
  Color c = rgb(statusColor(f.status));
  g.rect(x - 11, y - 1, 22, 9, rgb(0x0a0f33, 220));
  g.frame(x - 11, y - 1, 22, 9, c);
  g.text(x, y - 2, statusTag(f.status), c, 1, false);
}

void Battle::draw() {
  Gfx& g = G.g;
  float t = G.time;
  // Décor, sur toute la largeur de l'écran (de L à R ; la scène des combattants va de 0 à 320)
  float L = g.left(), R = g.right(), W = (float)g.fullW;
  int more = (int)(W / SCREEN_W * 100);  // nombre de particules : proportionnel à la largeur (en %)
  // Horizon haut (y = HZ) : les trois rangées du damier sont posées sur le sol, qui s'éclaircit vers
  // l'avant (profondeur). Les reliefs du fond sont dessinés à moitié de leur hauteur (Y).
  const float HZ = 56;
  auto Y = [](float y) { return y * .5f; };
  auto ridge = [&](std::vector<Pt> p, float yl, float yr) {  // prolongé de chaque côté sur un écran large
    for (auto& q : p) q.y = Y(q.y);
    if (L < 0) {
      p.insert(p.begin(), {{L, HZ}, {L, Y(yl)}});
      p.push_back({R, Y(yr)});
      p.push_back({R, HZ});
    }
    return p;
  };
  auto ground = [&](uint32_t far, uint32_t nearC) { g.gradV(L, HZ - 2, W, SCREEN_H - HZ + 2, rgb(far), rgb(nearC)); };
  if (bg == Theme::Vallee && !boss) {
    g.gradV(L, 0, W, HZ, rgb(0x8fc0db), rgb(0xdfeac0));
    if (L < 0) g.ellipse(L + 10, HZ, 70, 9, rgb(0xaed08c)), g.ellipse(R - 10, HZ, 70, 10, rgb(0xbcd89a));
    g.ellipse(70, HZ, 120, 11, rgb(0xbcd89a));
    g.ellipse(260, HZ + 1, 110, 9, rgb(0xaed08c));
    ground(0x8cba6c, 0xa8d488);
    for (int i = 0; i < 22 * more / 100; i++) g.rect(L + std::fmod(i * 53 + 11.f, W), HZ + 6 + (i * 13) % 120, 8, 2, rgb(0xb6dc94));
  } else if (bg == Theme::Vallee) {
    g.gradV(L, 0, W, HZ, rgb(0x1d1838), rgb(0x5a4d80));
    ground(0x2e2650, 0x463c72);
    for (int i = 0; i < 8 * more / 100; i++) g.ellipse(L + std::fmod(i * 70 + t * 12, W + 100) - 50, HZ + 4 + (i % 8) * 14, 60, 6, rgb(0xd8d0ff, 26));
  } else if (bg == Theme::Foret) {
    g.gradV(L, 0, W, HZ, rgb(0x1e3a2a), rgb(0x4a7a4a));
    for (int i = L < 0 ? (int)std::floor(L / 40) - 1 : 0; i * 40.f - 10 < R + 30; i++) {
      int odd = i & 1, row = ((i % 3) + 3) % 3;
      float x = i * 40.f - 10 + odd * 12, top = Y(30 + row * 8.f);
      g.rect(x + 9, top, 5, HZ - top, rgb(0x2a1e16));
      g.ellipse(x + 11, top + 3, 17, 15, rgb(odd ? 0x173a22 : 0x1f4a2a));
    }
    ground(0x33582f, 0x4c7c45);
    for (int i = 0; i < 14 * more / 100; i++)
      g.ellipse(L + std::fmod(i * 53.f + std::sin(t + i) * 8 + 10, W), 12 + std::fmod(i * 23.f, 150.f), 1.5f, 1.5f,
                rgb(0xe8ff9a, uint8_t(100 + 80 * std::sin(t * 3 + i))));
  } else if (bg == Theme::Neige) {
    g.gradV(L, 0, W, HZ, boss ? rgb(0x2a3a5a) : rgb(0x9ab8d8), boss ? rgb(0x7a9ac8) : rgb(0xe6eef8));
    g.poly(ridge({{0, 112}, {50, 50}, {100, 90}, {160, 30}, {220, 86}, {270, 56}, {320, 80}, {320, 112}}, 66, 58), rgb(0xc8d6e8));
    g.poly({{150, Y(42)}, {160, Y(30)}, {171, Y(42)}}, rgb(0xffffff));
    ground(0xd6e0ec, 0xf2f6fb);
    for (int i = 0; i < 30 * more / 100; i++)
      g.rect(L + std::fmod(i * 41 + t * 8 + std::sin(t + i) * 4 + 10, W), std::fmod(i * 23 + t * 20, 180.f), 1, 1, rgb(0xffffff, 220));
  } else if (bg == Theme::Cendres) {
    g.gradV(L, 0, W, HZ, boss ? rgb(0x2a0e08) : rgb(0x3a2420), boss ? rgb(0xa8321e) : rgb(0x8a4a2a));
    g.poly(ridge({{0, 112}, {60, 60}, {110, 100}, {170, 40}, {240, 96}, {290, 70}, {320, 90}, {320, 112}}, 70, 64), rgb(0x4a3a36));
    ground(0x5c524c, 0x7c7068);
    g.rect(L, HZ - 2, W, 2, rgb(0xd9541e));
    for (int i = 0; i < 18 * more / 100; i++)
      g.rect(L + std::fmod(i * 41 + t * 6, W), std::fmod(i * 23 + t * 14, 180.f), 1, 1, rgb(0xffb347, 160));
  } else {
    g.gradV(L, 0, W, HZ, rgb(0x120e18), rgb(0x2e2838));
    ground(0x3a3444, 0x585060);
    for (int i = L < 0 ? (int)std::floor((L - 20) / 70) : 0; 20 + i * 70.f < R; i++) {
      float x = 20 + i * 70.f;
      g.ellipse(x, HZ - 4, 9, 6, rgb(0x7fd6ff, uint8_t(40 + 30 * std::sin(t * 2 + i))));
      g.tri(x - 4, HZ - 1, x, HZ - 16, x + 4, HZ - 1, rgb(0x7fd6ff));
    }
  }
  if (float night = shown() ? G.nightAmount() : 0.f; night > 0)  // jour et nuit (2.6), comme sur la carte
    g.rect(L, 0, W, SCREEN_H, rgb(0x0a1440, uint8_t(80 * night)));
  bool blinking = blink && t - blinkT < .42f && int((t - blinkT) / .07f) % 2 == 0;
  // Damier : une marque au sol par case ; la zone de la technique visée et la case choisie pour se déplacer en or
  for (int side = 0; side < 2; side++)
    for (int c = 0; c < N_CELLS; c++) {
      Pt p = cellPos(side == 1, c);
      // une dalle claire en losange, vue en perspective
      g.poly({{p.x - 14, p.y + 13}, {p.x, p.y + 8}, {p.x + 14, p.y + 13}, {p.x, p.y + 18}}, rgb(0xffffff, 34));
      g.poly({{p.x - 10, p.y + 13}, {p.x, p.y + 9.5f}, {p.x + 10, p.y + 13}, {p.x, p.y + 16.5f}}, rgb(0x000000, 18));
      if (const Rules::Terrain* tr = terrainOf(side == 1, c)) {  // case de terrain (2.6) : couleur et petit décor animé
        Color col = rgb(tr->color);
        col.a = 120;
        g.poly({{p.x - 14, p.y + 13}, {p.x, p.y + 8}, {p.x + 14, p.y + 13}, {p.x, p.y + 18}}, col);
        float ph = t * 3 + c * 1.3f + side * 5;
        const std::string& k = tr->kind;
        if (k == "degats")  // braises qui rougeoient
          for (int i = 0; i < 3; i++) g.ellipse(p.x - 6 + i * 6, p.y + 13 + (i % 2) * 2 - std::fmod(ph + i, 3.f), 1.5f, 1.5f, rgb(i % 2 ? 0xffd34d : 0xff5a1a));
        else if (k == "poison")  // épines
          for (int i = 0; i < 4; i++) g.line(p.x - 8 + i * 5, p.y + 15, p.x - 6 + i * 5, p.y + 10, 1, rgb(0x2e4a1e));
        else if (k == "esquive")  // brins d'herbe qui ondulent
          for (int i = 0; i < 5; i++) g.line(p.x - 9 + i * 4.5f, p.y + 15, p.x - 9 + i * 4.5f + std::sin(ph + i) * 1.5f, p.y + 9, 1, rgb(0x4f8a2a));
        else if (k == "soin")  // ondes de la source
          g.ellipse(p.x, p.y + 13, 4 + std::fmod(ph, 2.f) * 4, 1.5f + std::fmod(ph, 2.f) * 1.5f, rgb(0xe0f4ff, uint8_t(160 - std::fmod(ph, 2.f) * 70)));
        else if (k == "lent")  // reflet du verglas
          g.line(p.x - 6 + std::fmod(ph, 4.f) * 3, p.y + 11, p.x - 3 + std::fmod(ph, 4.f) * 3, p.y + 15, 1, rgb(0xffffff, 200));
        else if (k == "magie")  // petits cristaux
          for (int i = 0; i < 2; i++) {
            float x = p.x - 4 + i * 8, y = p.y + 13;
            g.poly({{x, y - 5}, {x + 2, y}, {x, y + 1}, {x - 2, y}}, rgb(0xf0e0ff, uint8_t(170 + 60 * std::sin(ph + i))));
          }
      }
    }
  drawZones(t);
  // Zones des attaques annoncées par les boss, en rouge sur le damier allié
  std::vector<std::pair<int, int>> warn = netWarn_;
  for (auto& w : warnings_) warn.push_back({w.center, (int)w.zone});
  for (auto& [center, zone] : warn)
    for (int c = 0; c < N_CELLS; c++)
      if (inZone(Area(zone), center, c)) {
        Pt p = cellPos(true, c);
        g.ellipse(p.x, p.y + 13, 14, 5, rgb(0xff3b30, uint8_t(120 + 90 * std::sin(t * 7))));
      }
  // Combattants, du haut du damier vers le bas : ceux de devant passent par-dessus
  std::vector<FighterP> shown;
  for (auto& f : foes)
    if (f->alive()) shown.push_back(f);
  for (auto& a : allies) shown.push_back(a);
  std::stable_sort(shown.begin(), shown.end(), [&](const FighterP& a, const FighterP& b) { return pos(a).y < pos(b).y; });
  // Zone en cours (choix sur le damier, ou aperçu de la technique survolée) : les combattants du camp visé hors de
  // la zone sont estompés, pour que la cible ressorte même quand ils se chevauchent
  int zSide = -1, zCenter = -1;
  std::set<int> zCells;
  {
    const Move* zm = nullptr;
    bool side = false;
    if (boardPicking()) {
      Menu& m = G.menus.top();
      if (!pick_.move && m.sel >= 0 && m.sel < (int)pick_.cells.size()) zCenter = pick_.cells[(size_t)m.sel], side = pick_.allySide, zm = aiming_;
    } else if (!preview_.empty() && previewDepth_ == G.menus.depth() && G.menus.active() && cursor && cursor->alive() && !isAlly(cursor) &&
               moveInfo(preview_).target == Target::Enemy) {
      zm = &moveInfo(preview_);
      zCenter = cellOf(cursor);
    }
    if (zCenter >= 0) {
      zSide = side;
      for (int c = 0; c < N_CELLS; c++)
        if (c == zCenter || (zm && (zm->zone == Area::All || inZone(zm->zone, zCenter, c)))) zCells.insert(c);
    }
  }
  for (auto& f : shown) {
    Pt p = pos(f);
    Pt o = offset(f);
    bool dim = zSide >= 0 && isAlly(f) == (zSide == 1) && !zCells.count(cellOf(f));
    if (!isAlly(f)) {  // ennemi : barre de PV au-dessus, nom sous le curseur
      float s = f->boss ? 1.45f : f->sp == "brumelin" ? .85f : 1.f;
      if (dim) g.alpha = .45f;
      if (!(blinking && blink == f)) drawFighterSprite(g, *f, p.x + o.x, p.y + o.y, s, false, t + p.x * .01f);  // regarde à gauche
      float bw = f->boss ? 24 : 15, by = std::max(46.f, p.y - fighterTop(*f, s) - 6);
      g.rect(p.x - bw - 1, by - 1, bw * 2 + 2, 4, rgb(0x0a0f33));
      g.rect(p.x - bw, by, bw * 2 * f->hp / f->mhp, 2, rgb(f->broken ? 0xffe066 : 0xff8a7a));
      // Brisure : bouclier (points restants) à gauche de la barre, ou « BRISÉ » ; dessous, les faiblesses découvertes
      if (f->broken) {
        g.text(p.x, by - 20, T("BRISÉ"), rgb(0xffe066, uint8_t(180 + 70 * std::sin(t * 9))), 1);
      } else if (f->shieldMax > 0) {
        float sx = p.x - bw - 9, sy = by - 4;
        g.poly({{sx - 5, sy - 1}, {sx + 5, sy - 1}, {sx + 5, sy + 6}, {sx, sy + 10}, {sx - 5, sy + 6}}, rgb(0xc8d6f0));
        g.poly({{sx - 4, sy}, {sx + 4, sy}, {sx + 4, sy + 6}, {sx, sy + 9}, {sx - 4, sy + 6}}, rgb(0x2a4a8a));
        g.text(sx + 1, sy + 1, std::to_string(f->shield), WHITE, 1, false);
      }
      // Rôle (2.7) : petite icône à droite de la barre (écu gris, croix verte, couronne dorée)
      if (const std::string& role = f->S().combatRole; !role.empty()) {
        float rx = p.x + bw + 5, ry = by - 2;
        if (role == "protecteur") {
          g.poly({{rx - 3, ry}, {rx + 3, ry}, {rx + 3, ry + 4}, {rx, ry + 6}, {rx - 3, ry + 4}}, rgb(0xd8dce8));
          g.rect(rx - .5f, ry + 1, 1, 4, rgb(0x5a6070));
        } else if (role == "soigneur") {
          g.rect(rx - 1, ry, 2, 6, rgb(0x6fe08a));
          g.rect(rx - 3, ry + 2, 6, 2, rgb(0x6fe08a));
        } else {
          g.poly({{rx - 3, ry + 5}, {rx - 3, ry}, {rx - 1.5f, ry + 2}, {rx, ry}, {rx + 1.5f, ry + 2}, {rx + 3, ry}, {rx + 3, ry + 5}}, rgb(0xffd34d));
        }
      }
      int nw = 0;
      for (int ty = 0; ty < (int)types().size(); ty++)
        if (G.weakKnown(f->sp, ty)) {
          g.rect(p.x - bw + nw * 5, by + 4, 4, 3, rgb(0x0a0f33));
          g.rect(p.x - bw + nw * 5, by + 4, 3, 2, rgb(types()[ty].color));
          nw++;
        }
      drawStatusTag(*f, p.x, by - 10);
      g.alpha = 1;
      continue;
    }
    if (!f->alive()) g.alpha = .35f;
    else if (dim) g.alpha = .45f;
    if (!(blinking && blink == f)) drawFighterSprite(g, *f, p.x + o.x, p.y + o.y, 1, true, t + p.x * .01f);  // regarde à droite
    g.alpha = 1;
    drawStatusTag(*f, p.x - 24, p.y - 16);
    if (actor == f) {  // celui qui choisit son action : une flèche au-dessus de sa tête
      float ty = std::max(44.f, p.y - fighterTop(*f, 1) - 10);
      g.tri(p.x - 4, ty, p.x + 4, ty, p.x, ty + 6, rgb(0xffd34d));
    }
    if (cursor == f) g.cursor(p.x - 24, p.y - 4);
  }
  // Par-dessus les combattants : contour de la zone et flèche au-dessus de la cible (le sprite cache sa case)
  if (zSide >= 0) {
    for (int c : zCells) tile(g, cellPos(zSide == 1, c), rgb(c == zCenter ? 0xfff2a0 : 0xffb050, 210), true);
    for (auto& f : zSide == 1 ? allies : foes)
      if (cellOf(f) == zCenter) {
        Pt p = cellPos(zSide == 1, zCenter);
        float y = std::max(48.f, p.y - fighterTop(*f, 1) - 16) + std::sin(t * 8) * 1.5f;
        g.tri(p.x - 5, y, p.x + 5, y, p.x, y + 6, rgb(0xffe066));
        g.tri(p.x - 3, y + 1, p.x + 3, y + 1, p.x, y + 4, rgb(0xfff8d0));
      }
  }
  drawAnim();
  // Nombres flottants
  pops.erase(std::remove_if(pops.begin(), pops.end(), [&](const Pop& p) { return t - p.t > 1.f; }), pops.end());
  for (auto& p : pops) g.text(p.x, p.y - std::min(1.f, (t - p.t) * 3) * 8, p.text, p.col, 1);
  if (flashT >= 0 && t - flashT < .5f) {
    Color c = flashCol;
    c.a = uint8_t(130 * (1 - (t - flashT) / .5f));
    g.rect(g.left(), 0, g.fullW, 168, c);
  }
  // Fenêtre du haut : message ou aide
  std::string top;
  if (G.menus.active()) top = G.menus.help();
  else if (sc.showingMessage()) top = utf8Prefix(sc.text(), sc.visibleChars());
  else if (follower() && !netMsg_.empty()) top = utf8Prefix(netMsg_, int((t - netMsgT_) * 60));
  else if (!choosing().empty()) top = choosing() + T(" choisit…");
  float orderY = 3;
  if (!top.empty()) {
    auto lines = Gfx::wrap(top, 300);
    size_t most = G.menus.active() ? 2 : 3;  // aide : deux lignes ; message du combat : trois au plus
    if (lines.size() > most) {
      lines.resize(most);
      lines.back() = utf8Prefix(lines.back(), 47) + "…";
    }
    if (boardPicking() && lines.size() == 1) lines.push_back(T("Flèches ou souris pour choisir · Entrée · Échap"));
    g.window(4, 2, 312, 7 + 11 * (int)lines.size());
    for (size_t i = 0; i < lines.size(); i++) g.text(160, 5 + i * 11, lines[i], WHITE, 1);
    orderY = 2 + 7 + 11 * (int)lines.size() + 2;
  }
  float orderX = 6;
  if (weather_ >= 0 && !ending_ && !summaryOn_) {  // météo, puis l'ordre des tours sur la même ligne
    const std::string& wn = rules().weathers[(size_t)weather_].name;
    g.text(6, orderY, wn, rgb(0xe8ecff));
    orderX += Gfx::textW(wn) + 6;
  }
  if (!reserve.empty() && !ending_ && !summaryOn_) {  // renforts ennemis : à droite de la même ligne
    std::string rs = T("Renforts : ") + std::to_string(reserve.size());
    g.text(314, orderY, rs, GREY, 2);
  }
  if (!ending_ && !summaryOn_ && !follower()) drawTurnOrder(orderY, orderX);  // ordre des prochains tours, sous le message
  if (G.menus.shownDepth() <= 1) drawHud(t);  // un sous-menu ouvert prend sa place
  drawWeather();
  if (summaryOn_) {  // fenêtre des gains de la victoire
    int h = 36 + 11 * (int)summary_.size(), y = std::max(26, 92 - h / 2);
    g.window(30, y, 260, h);
    g.text(160, y + 5, T("Victoire !"), GOLD, 1);
    for (size_t i = 0; i < summary_.size(); i++) g.text(38, y + 19 + i * 11, summary_[i], WHITE);
    g.text(160, y + h - 12, T("Entrée : continuer"), rgb(0xaab3d8), 1);
  }
  G.menus.draw(g, t);
  // Entrée en combat : bandes sombres qui s'écartent en alternance, après un éclair blanc ;
  // contre un boss, son nom passe dans un bandeau
  float k = (t - start) / .6f;
  if (k < 1) {
    const int BANDS = 12;
    float bh = SCREEN_H / float(BANDS), e = k * k * (3 - 2 * k);
    for (int i = 0; i < BANDS; i++) {
      float off = e * (g.fullW + 20) * (i % 2 ? 1 : -1);
      g.rect(g.left() + off, i * bh, g.fullW, bh + 1, rgb(boss ? 0x1a0610 : 0x0a0f33));
    }
    if (k < .3f) g.rect(g.left(), 0, g.fullW, SCREEN_H, rgb(0xffffff, uint8_t(255 * (1 - k / .3f))));
  }
  float kb = (t - start) / 1.6f;
  std::string bossName = foeName;
  for (auto& f : foes)
    if (bossName.empty() && f->boss) bossName = f->name();
  if (boss && kb < 1 && !bossName.empty()) {
    float a = std::min({1.f, kb * 6, (1 - kb) * 5});
    float y = 80, h = 22 * a;
    g.rect(g.left(), y - h / 2, g.fullW, h, rgb(0x1a0610, uint8_t(220 * a)));
    g.rect(g.left(), y - h / 2, g.fullW, 1, rgb(0xffe066, uint8_t(255 * a)));
    g.rect(g.left(), y + h / 2 - 1, g.fullW, 1, rgb(0xffe066, uint8_t(255 * a)));
    if (a > .5f) g.text(160 + (1 - std::min(1.f, kb * 4)) * 60, y - 4, bossName, rgb(0xffe066), 1);
  }
}

// ---------------------------------------------------------------------------
// Duel en ligne (online.hpp) : l'hôte joue tout le combat et envoie son état ;
// l'invité l'affiche et répond quand un de ses combattants doit agir.
// Gardien à plusieurs (coop.cpp) : même principe, le chef joue le combat et chacun
// commande ses combattants ; tous voient les alliés dans le même ordre.
// ---------------------------------------------------------------------------
void Battle::goOnline(Net mode, std::function<void(const Json&)> send, const std::string& me, const std::string& other) {
  net_ = mode;
  send_ = [send = std::move(send)](int, const Json& m) { send(m); };
  names_[0] = me, names_[1] = other;
  for (auto* v : {&foes, &reserve})  // pas de Brisure en duel : les deux camps sont des joueurs
    for (auto& e : *v) e->shieldMax = e->shield = 0, e->broken = false;
  weather_ = -1;  // ni météo ni terrain : le duel reste le même chez les deux joueurs
  for (auto& side : terrain_) side.fill(-1);
  sc.clear();
  if (mode == Net::Host) sc.say(T("Duel : ") + me + T(" contre ") + other + " !", 1.3f);
}

void Battle::goCoop(bool lead, int me, int leader, std::vector<int> owners, std::map<int, std::string> names,
                    std::function<void(int, const Json&)> send) {
  net_ = lead ? Net::Lead : Net::Follow;
  me_ = me, leader_ = leader;
  owners_ = std::move(owners);
  players_ = std::move(names);
  send_ = std::move(send);
  if (!lead) sc.clear();  // ceux qui suivent affichent les messages du chef
}

int Battle::owner(const FighterP& f) const {
  for (size_t i = 0; i < allies.size() && i < owners_.size(); i++)
    if (allies[i] == f) return owners_[i];
  return me_;
}

std::string Battle::choosing() const {
  auto name = [this](int id) {
    auto it = players_.find(id);
    return it == players_.end() ? std::string() : it->second;
  };
  if (net_ == Net::Host && remoteTurn_) return names_[1];
  if (net_ == Net::Guest && netWait_ == "hote") return names_[1];
  if (net_ == Net::Lead && remoteTurn_) return name(owner(remoteTurn_));
  if (net_ == Net::Follow && netWait_ != name(me_)) return netWait_;
  return "";
}

Json Battle::mine(const FighterP& f) const {
  for (size_t i = 0; i < allies.size(); i++)
    if (allies[i] == f) return Json::array({0, (int)i});
  for (size_t i = 0; i < foes.size(); i++)
    if (foes[i] == f) return Json::array({1, (int)i});
  return Json::array({-1, -1});
}
FighterP Battle::other(const Json& s) const {
  if (!s.is_array() || s.size() != 2) return nullptr;
  int side = s[0].get<int>(), i = s[1].get<int>();
  if (coop() && side >= 0) side = 1 - side;  // gardien : les alliés sont les mêmes pour tous
  const auto& v = side == 0 ? foes : allies;  // duel : l'équipe de l'autre joueur, ce sont mes « ennemis »
  return side >= 0 && side <= 1 && i >= 0 && i < (int)v.size() ? v[(size_t)i] : nullptr;
}
void Battle::setBlink(const FighterP& f) {
  blink = f;
  blinkT = G.time;
  if (net_ == Net::Host || net_ == Net::Lead) send_(-1, Json{{"t", "clignote"}, {"f", mine(f)}});
}

Json Battle::netState() const {
  auto pack = [this](const std::vector<FighterP>& v) {
    Json a = Json::array();
    for (auto& f : v) {
      Json st = Json::array();
      for (int k = 0; k < N_STAGES; k++) st.push_back(f->stage[k]);
      a.push_back(Json::array({f->hp, f->mp, (int)std::lround(f->atb), (int)std::lround(f->lim), (int)f->status, st, cellOf(f), f->shield, f->broken ? 1 : 0}));
    }
    return a;
  };
  // Qui choisit en ce moment : duel « invite » ou « hote » ; gardien, le nom du joueur
  std::string wait = remoteTurn_ ? "invite" : G.menus.active() ? "hote" : "";
  if (coop()) {
    auto it = players_.find(remoteTurn_ ? owner(remoteTurn_) : me_);
    wait = (remoteTurn_ || G.menus.active()) && it != players_.end() ? it->second : "";
  }
  Json warn = Json::array();
  for (auto& w : warnings_) warn.push_back(Json::array({w.center, (int)w.zone}));
  Json ter = Json::array();
  for (auto& side : terrain_)
    for (int v : side) ter.push_back(v);
  return Json{{"t", "etat"}, {"a", pack(allies)}, {"b", pack(foes)}, {"msg", sc.showingMessage() ? sc.text() : std::string()}, {"attente", wait},
              {"w", warn}, {"meteo", weather_}, {"terrain", ter}};
}

void Battle::netMessage(const Json& m) {
  std::string k = jget<std::string>(m, "t", "");
  if (follower()) {
    if (net_ == Net::Follow && jget(m, "de", -1) != leader_) return;  // seul le chef décrit le combat
    if (k == "son") return sound(jget<std::string>(m, "s", ""), false);
    if (k == "anim") {  // animation d'une action, décidée chez l'hôte ou le chef
      std::vector<FighterP> c;
      Json cs = m.value("c", Json::array()), col = m.value("col", Json::array({255, 255, 255}));
      for (auto& x : cs)
        if (FighterP f = other(x)) c.push_back(f);
      FighterP a = other(m.value("a", Json()));
      if (a && col.size() == 3) {
        Color cc = rgb(uint32_t(col[0].get<int>()) << 16 | uint32_t(col[1].get<int>()) << 8 | uint32_t(col[2].get<int>()));
        int ty = jget(m, "ty", -1);
        anim_ = Anim{a, c, Fx(std::clamp(jget(m, "fx", 0), 0, 4)), cc, jget(m, "l", false), G.time, ty >= 0 && ty < (int)types().size() ? ty : -1};
      }
      return;
    }
    if (ending_) return;                                               // la fin se joue chez chacun
    if (k == "etat") {
      // Duel : « a » est l'équipe de l'hôte (mes ennemis), « b » la mienne. Gardien : « a » les alliés, « b » les ennemis
      auto apply = [this](std::vector<FighterP>& v, const Json& a) {
        for (size_t i = 0; i < v.size() && i < a.size(); i++) {
          const Json& x = a[i];
          Fighter& f = *v[i];
          if (x.size() > 6) cells_[&f] = std::clamp(x.at(6).get<int>(), -1, N_CELLS - 1);  // la case, décidée chez l'hôte
          if (x.size() > 8) f.shield = x.at(7).get<int>(), f.broken = x.at(8).get<int>() != 0;  // Brisure
          // at() : une liste trop courte lève une erreur (rattrapée par Online::update) au lieu de lire n'importe quoi
          f.hp = x.at(0).get<int>(), f.mp = x.at(1).get<int>(), f.atb = x.at(2).get<float>(), f.lim = x.at(3).get<float>();
          f.status = Status(std::clamp(x.at(4).get<int>(), 0, 4));
          for (int s = 0; s < N_STAGES && s < (int)x.at(5).size(); s++) f.stage[s] = x.at(5).at((size_t)s).get<int>();
        }
      };
      apply(duel() ? foes : allies, m.value("a", Json::array()));
      apply(duel() ? allies : foes, m.value("b", Json::array()));
      std::string msg = jget<std::string>(m, "msg", "");
      if (msg != netMsg_) netMsg_ = msg, netMsgT_ = G.time;
      netWarn_.clear();
      for (auto& w : m.value("w", Json::array()))
        if (w.is_array() && w.size() == 2) netWarn_.push_back({std::clamp(w[0].get<int>(), 0, N_CELLS - 1), std::clamp(w[1].get<int>(), 0, 4)});
      netWait_ = jget<std::string>(m, "attente", "");
      int wi = jget(m, "meteo", -1);
      weather_ = wi >= 0 && wi < (int)rules().weathers.size() ? wi : -1;
      Json ter = m.value("terrain", Json::array());
      if (ter.size() == 2 * N_CELLS)
        for (size_t i = 0; i < ter.size(); i++) {
          int v = ter[i].is_number_integer() ? ter[i].get<int>() : -1;
          terrain_[i / N_CELLS][i % N_CELLS] = v >= 0 && v < (int)rules().terrains.size() ? v : -1;
        }
    } else if (k == "tour") {
      int i = jget(m, "i", -1);
      if (i < 0 || i >= (int)allies.size() || !allies[(size_t)i]->alive() || !mineToCommand(allies[(size_t)i])) return;
      FighterP a = allies[(size_t)i];
      actor = a;
      if (G.tacticsAuto && tacticTurn(a)) return;  // mode auto : les tactiques choisissent
      command(a);
    } else if (k == "pop") {
      if (FighterP f = other(m.value("f", Json()))) {
        Json c = m.value("c", Json::array({255, 255, 255}));
        Color col;
        col.r = c.at(0).get<uint8_t>(), col.g = c.at(1).get<uint8_t>(), col.b = c.at(2).get<uint8_t>();
        // même empilement que Battle::pop, sans renvoyer le message
        Net keep = net_;
        net_ = Net::None;
        pop(f, jget<std::string>(m, "txt", ""), col);
        net_ = keep;
      }
    } else if (k == "clignote") {
      if (FighterP f = other(m.value("f", Json()))) blink = f, blinkT = G.time;
    } else if (k == "eclair") {
      Json c = m.value("c", Json::array({255, 255, 255}));
      flashT = G.time;
      flashCol = rgb(uint32_t(c.at(0).get<int>()) << 16 | uint32_t(c.at(1).get<int>()) << 8 | uint32_t(c.at(2).get<int>()));
    } else if (k == "renfort") {  // gardien : un renfort prend la place d'un ennemi tombé
      int i = jget(m, "i", -1);
      if (i >= 0 && i < (int)foes.size() && !reserve.empty()) {
        cells_[reserve.front().get()] = cellOf(foes[(size_t)i]);
        foes[(size_t)i] = reserve.front();
        reserve.erase(reserve.begin());
      }
    } else if (k == "issue") {  // gardien : fin du combat, jouée chez chacun (expérience et or de son équipe)
      G.menus.clear();
      cursor = nullptr;
      actor = nullptr;
      netMsg_.clear();
      netWait_.clear();
      if (jget(m, "gagne", false)) {
        defeated = roster_;
        victory();
      } else {
        ending_ = true;
        sc.say(T("Toute l'équipe est à terre…"), 1.6f);
        sc.call([this] { finish(BattleResult::Lose); });
      }
    }
    return;
  }
  // Hôte ou chef : l'ordre pour le combattant en attente (vérifié : technique connue, PM, cibles)
  if ((net_ == Net::Host || net_ == Net::Lead) && k == "ordre" && remoteTurn_) {
    FighterP f = remoteTurn_;
    if (net_ == Net::Lead && jget(m, "de", -1) != owner(f)) return;  // seul son joueur le commande
    remoteTurn_ = nullptr;
    int i = jget(m, "i", -1);
    const auto& side = net_ == Net::Host ? foes : allies;
    std::vector<FighterP> targets;
    Json cs = m.value("cibles", Json::array());
    for (auto& c : cs)
      if (FighterP t = other(c)) targets.push_back(t);
    bool ok = i >= 0 && i < (int)side.size() && side[(size_t)i] == f && !targets.empty();
    if (m.contains("deplacer")) {  // se déplacer : vers une case libre de son camp
      int c = jget(m, "deplacer", -1);
      bool free = i >= 0 && i < (int)side.size() && side[(size_t)i] == f && c >= 0 && c < N_CELLS;
      for (auto& x : side) free = free && cellOf(x) != c;
      if (free) return moveTo(f, c);
      if (net_ == Net::Lead) return autoCommand(f);
      return enemyTurn(f);
    }
    std::string obj = jget<std::string>(m, "objet", ""), id = jget<std::string>(m, "move", "");
    bool lim = jget(m, "limite", false);
    if (ok && !obj.empty()) {  // gardien : objet déjà pris dans le sac de ce joueur
      const FighterP& t = targets[0];
      ok = net_ == Net::Lead && hasItem(obj) && item(obj).battle && item(obj).capture <= 0 && isAlly(t) && (item(obj).revive > 0) != t->alive();
      if (ok) return useItem(f, obj, t);
    } else if (ok && lim)
      ok = hasMove(id) && f->S().limit == id && f->lim >= 100;
    else if (ok) {
      auto known = f->techs();
      for (auto& s : f->spells()) known.push_back(s);
      ok = hasMove(id) && std::find(known.begin(), known.end(), id) != known.end() && f->mp >= moveInfo(id).cost;
    }
    if (ok) useMove(f, id, targets, lim);
    else if (net_ == Net::Lead) autoCommand(f);  // ordre impossible : l'ordinateur joue à sa place
    else enemyTurn(f);
  }
}

void Battle::netEnd(bool won) {
  if (finished_) return;
  G.menus.clear();
  if (duel()) sound(won ? "victoire" : "defaite", false);
  finish(won ? BattleResult::Win : BattleResult::Lose);
}

void Battle::dropPlayer(int id) {
  if (!coop() || finished_) return;
  if (net_ == Net::Follow && id == leader_) {  // le chef est parti : le combat s'arrête, sans conséquence
    G.menus.clear();
    finish(BattleResult::Fled);
    return;
  }
  for (auto& o : owners_)
    if (o == id) o = -1;
  if (net_ == Net::Lead && remoteTurn_ && owner(remoteTurn_) < 0) {  // c'était son tour : l'ordinateur le joue
    FighterP f = remoteTurn_;
    remoteTurn_ = nullptr;
    autoCommand(f);
  }
}
