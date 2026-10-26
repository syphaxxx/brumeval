#include "i18n.hpp"
#include "battle.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>

#include "audio.hpp"
#include "game.hpp"
#include "options.hpp"
#include "sprites.hpp"
#include "store.hpp"
#include "tactics.hpp"

static const Color WHITE = rgb(0xffffff), GOLD = rgb(0xffe066), GREY = rgb(0xb9c0de), GREEN = rgb(0x7dffa8), BLUE = rgb(0x8fd0ff),
                   RED = rgb(0xff8a7a), WEAK = rgb(0xffa04a), RESIST = rgb(0x9aa8c8);  // faiblesse, résistance (2026-10-26)

// ---------------------------------------------------------------------------
// Textes d'aide
// ---------------------------------------------------------------------------
static std::string pct(double x) { return std::to_string(int(std::lround(x * 100))) + " %"; }
// (2026-10-26) voile d'un type (« type » de sa mécanique : la Brume Noire) : le nom du seul type qui l'atteint, vide sinon
static std::string veilType(const Fighter& f) {
  std::string id = f.mech.is_object() ? jget<std::string>(f.mech, "type", "") : "";
  return id.empty() ? "" : types()[(size_t)typeOf(id)].name;
}
static std::string withType(std::string s, const std::string& ty) { return s.replace(s.find("{t}"), 3, ty); }
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
        case Area::Line: s += T(". Un ennemi et toute sa ligne (d'un bord du plateau à l'autre") + side + ")"; break;
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
  if (m.damaging() && m.target == Target::Enemy)
    s += m.melee ? T(", corps à corps (un ennemi au contact ; s'approche d'abord s'il le faut)") : T(", à distance (toute case ; moins fort au contact d'un ennemi)");
  // Soins
  if (m.kind == Kind::Heal && m.power > 0)
    s += user ? T(". Rend environ ") + std::to_string(int(m.power * user->eMag() / 20) + 2) + T(" PV (Magie ") + std::to_string(int(user->eMag())) + ")"
              : T(". Rend Puissance ") + std::to_string(m.power) + T(" × Magie ÷ 20 PV");
  if (m.kind == Kind::Revive) s += T(". Relève avec ") + std::to_string(m.power) + T(" % des PV");
  if (!m.effects.empty()) {
    s += T(". Effet : ");
    for (size_t i = 0; i < m.effects.size(); i++) s += (i ? ", " : "") + effectText(m.effects[i], m);
  }
  if (m.motion == "bond") s += T(". Puis le lanceur bondit sur la case derrière la cible (si elle est libre)");
  if (m.motion == "echange") s += T(". Puis le lanceur et la cible échangent leurs cases (pas un boss)");
  if (m.shove > 0) s += T(". Repousse la cible d'une case, loin du lanceur (case prise ou bord : choc pour les deux)");
  if (m.shove < 0) s += T(". Attire la cible d'une case vers le lanceur (case prise : choc pour les deux)");
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
  if (m.motion == "bond") v.push_back(T("Bondit derrière"));
  if (m.motion == "echange") v.push_back(T("Échange de cases"));
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
    bool mine = !d.owner.empty() && hasSpecies(d.owner);
    v.push_back(std::string(gearSlotName(d.slot)) + (b.empty() ? "" : " : " + b) +
                (mine ? T(" (pour ") + species(d.owner).name + T(" seulement)") : d.slot != G_ACCESSORY ? T(" (héros seulement)") : ""));
    if (d.moveBonus > 0) v.push_back(T("Échiquier : se déplace de ") + std::to_string(d.moveBonus) + T(" case de plus"));
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
    std::string absorbs;  // une absorption découverte en la frappant
    for (Type t = 0; t < (Type)types().size(); t++)
      if (typeEff(t, sp.type) * (sp.type2 >= 0 ? typeEff(t, sp.type2) : 1.f) * r(types()[t].id) < 0 && known->count(sp.id + ":" + std::to_string(t)))
        absorbs += (absorbs.empty() ? "" : ", ") + std::string(types()[t].name);
    std::string s = T("Faiblesses découvertes : ") + (found.empty() ? std::string(T("aucune")) : found) + ".";
    if (!absorbs.empty()) s += T(" Absorbe : ") + absorbs + ".";
    if (hidden) s += T(" Encore ") + std::to_string(hidden) + T(" à découvrir (frappez-la avec d'autres types).");
    return s;
  }
  std::string weak, strong, none, absorbs;
  for (auto& [e, names] : by) {
    std::string mult = T(" (×") + numFr(e) + ")";
    if (e < 0) absorbs += (absorbs.empty() ? "" : ", ") + names;
    else if (e == 0) none = names;
    else if (e > 1) weak += (weak.empty() ? "" : ", ") + names + mult;
    else strong += (strong.empty() ? "" : ", ") + names + mult;
  }
  std::string s = T("Faible contre : ") + (weak.empty() ? std::string(T("rien")) : weak) + ".";
  if (!strong.empty()) s += T(" Résiste : ") + strong + ".";
  if (!none.empty()) s += T(" Sans effet : ") + none + ".";
  if (!absorbs.empty()) s += T(" Absorbe : ") + absorbs + ".";
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
bool Battle::raining() const { return weather_ >= 0 && rules().weathers[(size_t)weather_].fx == "pluie"; }

Battle::Battle(Game& game, BattleSetup s, Theme bg_)
    : G(game), foes(std::move(s.foes)), reserve(std::move(s.reserve)), foeName(std::move(s.foeName)), boss(s.boss), canFlee(s.canFlee),
      canCapture(s.canCapture), bg(bg_) {
  allies = s.allies.empty() ? G.front() : std::move(s.allies);
  training_ = s.training;
  music = s.music;
  for (auto& a : allies) hasHero_ = hasHero_ || a->S().human;
  place(allies, !coop(), true);  // la formation de l'équipe (au gardien à plusieurs : placement automatique)
  place(foes, false, false);
  // Ennemis : quelques combattants au milieu plutôt qu'à l'avant, pour varier les formations
  for (auto& f : foes) {
    int c = cells_[f.get()], mid = c + 1;  // une case plus loin dans leur camp
    bool taken = false;
    for (auto& o : foes) taken = taken || cells_[o.get()] == mid;
    if (boardX(c) == GRID && !f->boss && !taken && frand() < .35f) cells_[f.get()] = mid;
  }
  // Embuscade : un tiers des créatures (au moins une) part du fond du camp allié, tournée vers l'équipe, qui regarde
  // de l'autre côté : il faut se retourner, se déplacer ou protéger ses lanceurs de sorts
  ambush_ = s.ambush && foes.size() >= 2 && !s.fixedBoard;
  if (ambush_)
    for (size_t i = foes.size() - std::max<size_t>(1, foes.size() / 3); i < foes.size(); i++) {  // un tiers (au moins une)
      int best = -1;
      for (int k = 0; k < GRID && best < 0; k++) {  // la ligne du milieu d'abord, puis de part et d'autre
        int row = (GRID / 2 + (k % 2 ? -(k + 1) / 2 : k / 2) + GRID) % GRID, c = boardAt(row, 0);
        if (!at(c)) best = c;
      }
      if (best < 0) break;
      cells_[foes[i].get()] = best;
      facing_[foes[i].get()] = 1;
      breakthrough_.insert(foes[i].get());  // déjà au bord : pas de Percée pour avoir longé le fond
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
  terrain_.fill(-1);
  {
    const Rules& R = rules();
    std::string th = themeName(bg);
    float r = frand();
    for (size_t i = 0; i < R.weathers.size() && s.rain && weather_ < 0; i++)  // la pluie de la carte continue en combat
      if (R.weathers[i].fx == "pluie" && std::find(R.weathers[i].themes.begin(), R.weathers[i].themes.end(), th) != R.weathers[i].themes.end())
        weather_ = (int)i;
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
      for (int side = 0; side < 2; side++) {  // autant de cases dans chaque moitié du plateau
        int n = irand(R.terrainMin, std::max(R.terrainMin, R.terrainMax));
        for (int k = 0; k < n; k++) {
          int c = toBoard(side == 0, irand(0, N_CELLS - 1));
          terrain_[(size_t)c] = pool[(size_t)irand(0, (int)pool.size() - 1)];
        }
      }
    // Obstacles (3.4) : dans les quatre colonnes du milieu, sur des cases libres et sans terrain
    auto it = R.obstacleSprites.find(th);
    if (it != R.obstacleSprites.end() && !it->second.empty()) {
      int n = irand(R.obstacleMin, std::max(R.obstacleMin, R.obstacleMax));
      for (int k = 0, tries = 0; k < n && tries < 40; tries++) {
        int c = boardAt(irand(0, GRID - 1), irand(GRID - 2, GRID + 1));
        int inCol = 0;  // jamais une colonne bouchée : au plus GRID - 2 obstacles par colonne
        for (auto& [oc, spr] : obstacles_) inCol += boardX(oc) == boardX(c);
        if (at(c) || blockedCell(c) || terrain_[(size_t)c] >= 0 || inCol >= GRID - 2) continue;
        obstacles_[c] = it->second[(size_t)irand(0, (int)it->second.size() - 1)];
        k++;
      }
    }
  }
  if (s.fixedBoard) {  // défi : tout est imposé
    weather_ = -1;
    terrain_.fill(-1);
    obstacles_.clear();
    for (size_t i = 0; i < allies.size() && i < s.allyCells.size(); i++) cells_[allies[i].get()] = s.allyCells[i];
    for (size_t i = 0; i < foes.size() && i < s.foeCells.size(); i++) cells_[foes[i].get()] = s.foeCells[i];
    auto it = rules().obstacleSprites.find(themeName(bg));
    for (size_t i = 0; i < s.obstacleCells.size(); i++)
      obstacles_[s.obstacleCells[i]] = it != rules().obstacleSprites.end() && !it->second.empty() ? it->second[i % it->second.size()] : "rocher";
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
  if (foeName.empty() && !boss && leader->boss && !leader->S().human) intro = T("Une meute menée par un alpha : ") + leader->name() + " !";
  for (auto& e : foes)
    if (e->chroma && foeName.empty() && !boss) intro = T("Une créature chromatique ! ") + e->name() + T(" brille de couleurs rares.");
  if (ambush_) intro = T("Embuscade ! Des créatures surgissent derrière vous.");
  if (!s.intro.empty()) intro = s.intro;
  sc.say(intro, s.intro.empty() ? 1.3f : 0.f);  // la leçon d'un défi attend Entrée
}


bool Battle::isAlly(const FighterP& f) const { return std::find(allies.begin(), allies.end(), f) != allies.end(); }
std::vector<FighterP> Battle::alive(const std::vector<FighterP>& v) const {
  std::vector<FighterP> o;
  for (auto& f : v)
    if (f->alive()) o.push_back(f);
  return o;
}




// Nombre ou texte flottant au-dessus d'un combattant ; ils s'empilent s'ils apparaissent ensemble
// (3.3 : sans chevaucher ceux des voisins, et jamais sur la ligne de l'ordre des tours)
void Battle::pop(const FighterP& f, const std::string& t, Color c, int style) {
  Pt p = pos(f);
  float w = Gfx::textW(t) + 4, x = std::clamp(p.x, w / 2 + 2, 318 - w / 2), y0 = std::max(popTop_, p.y - 22);
  // Chaque texte monte de 8 px pendant son premier tiers de seconde : un texte plus ancien est déjà monté d'autant
  // (k), le nouveau le suivra. Ils se gênent si, sur ce trajet, ils passent à moins de 10 px l'un de l'autre.
  auto clash = [&](float yy) {
    for (auto& q : pops) {
      float age = G.time - q.t, k = std::min(1.f, age * 3) * 8, d = yy - q.y;
      if (age < 1 && std::fabs(q.x - x) < (w + Gfx::textW(q.text) + 4) / 2 && d < 10 && d + k > -10) return true;
    }
    return false;
  };
  float y = y0;
  for (int k = 1; clash(y) && k <= 12; k++) {  // la première place libre : au-dessus, sinon en dessous
    float up = y0 - 11 * ((k + 1) / 2), down = y0 + 11 * (k / 2);
    y = k % 2 && up >= popTop_ ? up : down;
  }
  pops.push_back({x, y, t, c, G.time, style});
  if (style && shown()) {  // (2026-10-26) dessin seul : la cible recule loin du lanceur et s'écrase ; un gros coup fige les deux
    Impact& im = impacts_[f.get()];
    bool same = im.t0 == G.time;  // un critique qui brise ou qui abat : tout dans la même image
    float force = style & PopDamage ? std::clamp(std::atoi(t.c_str()) * 4.f / std::max(1, f->mhp), 0.f, 1.f) : 0.f;  // t : les dégâts
    if (style & PopKO) force = 1;
    float dx = anim_.actor && anim_.actor != f ? pos(f).x - pos(anim_.actor).x : 0;
    im = {G.time, same ? std::max(im.force, force) : force, std::abs(dx) > 1 ? (dx > 0 ? 1.f : -1.f) : -float(facingOf(f)),
          (same && im.flash) || (style & (PopCrit | PopBreak))};
    float stop = style & (PopKO | PopBreak) ? .09f : style & PopCrit ? .07f : style & PopWeak ? .05f : 0.f;
    if (stop > 0) {
      if (G.time > stopT_ + stopLen_) stopped_.clear(), stopT_ = G.time, stopLen_ = 0;
      stopLen_ = std::max(stopLen_, G.time + stop - stopT_);
      stopped_.insert(f.get());
      if (anim_.actor) stopped_.insert(anim_.actor.get());
    }
  }
  if (net_ == Net::Host || net_ == Net::Lead) {
    Json msg{{"t", "pop"}, {"f", mine(f)}, {"txt", t}, {"c", Json::array({c.r, c.g, c.b})}};
    if (style) msg["s"] = style;
    send_(-1, msg);
  }
}

float Battle::since(const Fighter* f, float t0) const {
  float e = G.time - t0;
  if (stopped_.count(f)) e -= std::max(0.f, std::min(G.time, stopT_ + stopLen_) - std::max(t0, stopT_));
  return e;
}

// Étirement et écrasement (2026-10-26, dessin seul) : le lanceur s'allonge dans son élan (ou s'étire vers le haut d'un petit
// bond), la cible s'écrase à l'impact puis rebondit un peu ; ancré à ses pieds (x, y : son point de dessin)
void Battle::squash(const FighterP& f, float x, float y) {
  float sx = 1, sy = 1;
  if (float e = since(f.get(), anim_.t0); anim_.actor == f && e >= 0 && e < .4f) {
    float k = std::sin(3.14159f * e / .4f);
    if (anim_.fx == Fx::Hit) sx += .1f * k, sy -= .07f * k;
    else sx -= .05f * k, sy += .07f * k;
  }
  if (auto it = impacts_.find(f.get()); it != impacts_.end())
    if (float u = since(f.get(), it->second.t0) / .3f; u >= 0 && u < 1) {
      float q = (1 - u) * (1 - u) * std::cos(u * 6) * (.06f + .08f * it->second.force);
      sx *= 1 + q, sy *= 1 - q;
    }
  G.g.squashX = sx, G.g.squashY = sy, G.g.squashAt = {x, y + 14};
}

bool Battle::shown() const { return G.battle_.get() == this; }

// Menus du combat : en bas de l'écran, pour laisser les deux damiers visibles (zones en or).
// Le menu de commande à gauche, ses sous-menus et le choix de la cible à droite.
// Le bas de l'écran est partagé : le menu de commande à gauche, l'ATH (les alliés) à droite.
// Les autres menus s'ouvrent juste au-dessus : à droite (techniques, cible ennemie : le damier
// ennemi reste visible) ou à gauche (cible alliée, se déplacer : le damier allié reste visible).
// Panneau du bas (3.3) : rien ne s'ouvre au-dessus du damier. Le menu de commande à gauche (5 lignes, il défile),
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
  float e = since(f.get(), anim_.t0);  // (2026-10-26) arrêté pendant un arrêt sur image
  if (anim_.actor == f && e >= 0 && e < .4f) {
    float k = std::sin(3.14159f * e / .4f);
    if (anim_.fx == Fx::Hit) {  // vers sa cible (sur l'échiquier, elle peut être derrière lui)
      float dir = facesRight(f) ? 1.f : -1.f;
      if (!anim_.targets.empty() && anim_.targets[0] != f) {
        float dx = cellPos(cellOf(anim_.targets[0])).x - cellPos(cellOf(f)).x;
        if (std::abs(dx) > 1) dir = dx > 0 ? 1.f : -1.f;
      }
      o.x = dir * 16.f * k;
    }
    else o.y = -6 * k;
  }
  for (auto& b : bursts_)  // (2026-10-25) riposte : un court élan vers l'attaquant (son action est encore anim_)
    if (float r = since(f.get(), b.t0); b.f == f && !std::strcmp(b.fx, "riposte") && anim_.actor && anim_.actor != f && r >= 0 && r < .3f) {
      float dx = cellPos(cellOf(anim_.actor)).x - cellPos(cellOf(f)).x;
      o.x += (std::abs(dx) > 1 ? (dx > 0 ? 1.f : -1.f) : facesRight(f) ? 1.f : -1.f) * 10 * std::sin(3.14159f * r / .3f);
    }
  if (float b = since(f.get(), blinkT); blink == f && b < .3f) o.x += std::sin(b * 70) * 2.5f;
  if (auto it = impacts_.find(f.get()); it != impacts_.end())  // (2026-10-26) recul selon la force du coup, puis il revient
    if (float u = since(f.get(), it->second.t0) / .25f; u >= 0 && u < 1) o.x += it->second.dir * (2 + 5 * it->second.force) * (1 - u) * (1 - u);
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
      {"plante", {"gfx_plante_0", "gfx_plante_1", "gfx_plante_2", "gfx_plante_3", "gfx_plante_4", "gfx_plante_5", "gfx_plante_6", "gfx_plante_7"}},  // Godot (2026-10-20) : la planche n'avait qu'une ou deux images
      {"roche", {"fx_terre_1", "fx_terre_2", "fx_terre_pics"}},
      {"ombre", {"fx_ombre_1", "fx_ombre_3", "fx_ombre_4", "fx_ombre_5"}},
      {"esprit", {"gfx_esprit_0", "gfx_esprit_1", "gfx_esprit_2", "gfx_esprit_3", "gfx_esprit_4", "gfx_esprit_5", "gfx_esprit_6", "gfx_esprit_7"}},  // Godot (2026-10-20) : la planche n'avait qu'une ou deux images
      {"lumiere", {"fx_revelation"}},
      {"soin", {"fx_soin_1", "fx_soin_2", "fx_soin_3", "fx_soin_4"}},
      // 3.4 : effets fabriqués avec les particules de Godot (outils/godot/effets/), pour les types sans dessin
      {"metal", {"gfx_metal_0", "gfx_metal_1", "gfx_metal_2", "gfx_metal_3", "gfx_metal_4", "gfx_metal_5", "gfx_metal_6", "gfx_metal_7"}},
      {"normal", {"gfx_normal_0", "gfx_normal_1", "gfx_normal_2", "gfx_normal_3", "gfx_normal_4", "gfx_normal_5", "gfx_normal_6", "gfx_normal_7"}},
  };
  static const std::map<std::string, uint32_t> SPARK = {  // couleur des étincelles de chaque type
      {"feu", 0xff8a3a}, {"glace", 0xbfe8ff}, {"foudre", 0xfff06a}, {"vent", 0xc8ffd8}, {"eau", 0x6ab8ff},
      {"poison", 0xb46aff}, {"plante", 0x8aff6a}, {"roche", 0xc8a070}, {"ombre", 0x8a6ab8}, {"esprit", 0xe0c8ff},
      {"lumiere", 0xfff8c0}, {"soin", 0x9aff9a}, {"metal", 0xd8e0f0}, {"normal", 0xffffff}};
  auto it = FX.find(heal ? "soin" : type);
  if (it == FX.end()) return false;
  const auto& v = it->second;
  // Les images de la planche s'enchaînent en fondu (2 à 4 images : sans fondu, l'effet saccade)
  float f = std::clamp(k, 0.f, .999f) * v.size();
  size_t i = std::min(v.size() - 1, size_t(f));
  float mixT = v.size() > 1 && i + 1 < v.size() ? std::clamp((f - i - .6f) / .4f, 0.f, 1.f) : 0.f;
  const SpriteRect* r = findSprite(v[i]);
  if (!r) return false;
  float fade = std::clamp((1 - k) * 2.5f, 0.f, 1.f);
  g.sprite(*r, std::round(p.x - r->w / 2.f), std::round(p.y + 12 - r->h), false, 1, Color{255, 255, 255, uint8_t(255 * fade * (1 - mixT))});
  if (mixT > 0)
    if (const SpriteRect* r2 = findSprite(v[i + 1]))
      g.sprite(*r2, std::round(p.x - r2->w / 2.f), std::round(p.y + 12 - r2->h), false, 1, Color{255, 255, 255, uint8_t(255 * fade * mixT)});
  // Étincelles du type qui jaillissent de la cible et retombent
  auto sp = SPARK.find(heal ? "soin" : type);
  if (sp != SPARK.end() && k > .1f) {
    float u = (k - .1f) / .9f;
    for (int s = 0; s < 10; s++) {
      float a = s * 2.399f + (heal ? 0 : 1.f), spd = 14 + (s * 7) % 11;
      float x = p.x + std::cos(a) * spd * u * 1.4f, y = p.y - 2 + std::sin(a) * spd * u * .8f + (heal ? -20 * u : 18 * u * u);
      Color c = rgb(sp->second, uint8_t(230 * (1 - u)));
      g.rect(std::round(x), std::round(y), s % 3 ? 1 : 2, s % 3 ? 1 : 2, c);
    }
  }
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
  // l'effet arrive sur la cible juste après l'élan ; 0 au début, 1 à la fin (2026-10-26 : il s'arrête avec elle pendant
  // un arrêt sur image)
  auto phase = [&](const FighterP& f) { return (since(f.get(), anim_.t0) - .2f) / .6f; };
  float k = anim_.targets.empty() ? -1 : phase(anim_.targets[0]);
  auto fade = [](Color c, float a) {
    c.a = uint8_t(std::clamp(a, 0.f, 1.f) * 255);
    return c;
  };
  // Limite : une invocation (planche des sorts) se dresse au-dessus des cibles, selon le type de la technique
  if (anim_.limit && k >= 0 && k <= 1) {
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
    k = phase(f);
    if (k < 0 || k > 1) continue;
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
        if (anim_.fx == Fx::Capture && anim_.actor && k < .45f)
          if (const SpriteRect* lan = findSprite("ic_lanterne")) {  // la lanterne vole vers la créature
            Pt a = pos(anim_.actor);
            float u = k / .45f;
            g.sprite(*lan, std::round(a.x + (p.x - a.x) * u - lan->w / 2.f), std::round(a.y - 14 + (p.y - a.y) * u - std::sin(u * 3.14159f) * 26));
          }
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
  bool shield = false, role = false, terrain = false, armor = false;
  for (auto& f : foes) {
    shield = shield || f->shieldMax > 0, role = role || !f->S().combatRole.empty();
    auto& r = f->S().resist;  // carapace ou peau d'enchantement (2026-10-23) : une attaque encaissée
    armor = armor || (r.count("physique") && r.at("physique") < 1) || (r.count("magique") && r.at("magique") < 1);
  }
  for (int v : terrain_) terrain = terrain || v >= 0;
  std::pair<const char*, bool> order[] = {{"embuscade", ambush_}, {"carapace", armor}, {"echiquier", true}, {"obstacle", !obstacles_.empty()}, {"bouclier", shield}, {"ordre", true}, {"role", role}, {"meteo", weather_ >= 0}, {"terrain", terrain}, {"talent", true}};
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
    bool key = G.in.confirm || G.in.cancel || G.in.mclick[0];
    // pendant le remplissage, les barres pleines d'un coup (le joueur automatique ferme d'un coup, comme avant)
    if (key && !autoPlay && !G.pilot_ && !gainRows_.empty() && gainsFill(G.time) < 1) {
      fillT_ = G.time - 1;
      G.in.confirm = G.in.cancel = G.in.mclick[0] = false;
      return;
    }
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

std::vector<FighterP> Battle::turnOrder(int n, const std::map<const Fighter*, float>& atbIf) const {
  std::vector<std::pair<float, FighterP>> ev;
  for (auto* side : {&allies, &foes})
    for (auto& f : *side) {
      if (!f->alive()) continue;
      float rate = atbRate(f);
      if (rate <= 0) continue;
      auto it = atbIf.find(f.get());
      float t = std::max(0.f, (100 - (it != atbIf.end() ? it->second : f->atb)) / rate);
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
    for (int i = 0; i < 22; i++) {  // (2026-10-21) les gouttes éclaboussent le sol
      float k = std::fmod(t * 2.5f + hash(i + 300), 1.f);
      float x = L + hash(i + 400 + int(t * 2.5f + hash(i + 300)) * 37) * W, y = 70 + hash(i + 500 + int(t * 2.5f + hash(i + 300)) * 13) * 120;
      g.ellipse(x, y, 1 + k * 3, .5f + k, rgb(0xd8e8ff, uint8_t(150 * (1 - k))));
    }
    if (fx == "orage") {  // l'éclair : un trait brisé du ciel à l'horizon, sa lueur, puis l'écran blanchit et s'efface
      float ph = std::fmod(t, 4.3f);
      if (ph < .35f) {
        int n = int(t / 4.3f);
        float a = 1 - ph / .35f, x = L + hash(n * 7 + 1) * W, y = 0;
        g.rect(L, 0, W, SCREEN_H, rgb(0xffffff, uint8_t(110 * a * a)));
        for (int s = 0; s < 7; s++) {
          float nx = x + (hash(n * 31 + s) - .5f) * 16, ny = y + 8 + hash(n * 17 + s) * 4;
          g.line(x, y, nx, ny, 3, rgb(0xb8d0ff, uint8_t(120 * a)));
          g.line(x, y, nx, ny, 1, rgb(0xffffff, uint8_t(255 * a)));
          x = nx, y = ny;
        }
      }
    }
  } else if (fx == "neige") {
    g.rect(L, 0, W, SCREEN_H, rgb(0xe8f4ff, 30));
    for (int i = 0; i < 90; i++) {  // flocons de tailles différentes : les petits, plus pâles et plus lents, sont plus loin
      float d = .5f + hash(i + 13) * .8f;
      float x = L + std::fmod(hash(i) * W + std::sin(t + i) * 6 * d + t * 25 * d, W), y = std::fmod(hash(i + 7) * SCREEN_H + t * 45 * d, (float)SCREEN_H);
      g.ellipse(x, y, .7f + d * .8f, .7f + d * .8f, rgb(0xffffff, uint8_t(120 + 100 * d / 1.3f)));
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

// (2026-10-26, FFX) une action survolée : l'ordre juste après elle, le lanceur d'abord (il joue maintenant), son prochain
// tour en or (la frise s'allonge jusqu'à lui) ; la cible qu'elle briserait perd son prochain tour (barré)
std::map<const Fighter*, float> Battle::paceIf(const Fighter** breaks) const {
  bool open = paceDepth_ && actor && isAlly(actor) && G.menus.active() &&
              (G.menus.depth() == paceDepth_ || (boardPicking() && G.menus.depth() == paceDepth_ + 1));  // ou le choix de sa cible
  if (!open) return {};
  auto atbIf = paceIf_;
  bool b = false;
  if (!paceMove_.empty() && moveInfo(paceMove_).target == Target::Enemy && cursor && cursor->alive() && !isAlly(cursor))
    damageText(actor, moveInfo(paceMove_), cursor, &b);
  if (b) atbIf[cursor.get()] = 0, *breaks = cursor.get();  // breakFoe : sa jauge retombe à zéro
  return atbIf;
}
void Battle::paceAfter(const FighterP& a, float gauge, const std::string& move) {
  paceIf_ = {{a.get(), gauge}}, paceMove_ = move, paceDepth_ = G.menus.depth();
}

void Battle::drawTurnOrder(float y, float minX, float maxX) {
  Gfx& g = G.g;
  const Fighter* breaks = nullptr;
  auto atbIf = paceIf(&breaks);
  auto order = turnOrder(12, atbIf);
  size_t keep = 8;
  if (!atbIf.empty()) {
    order.insert(order.begin(), actor);
    auto next = std::find(order.begin() + 1, order.end(), actor);
    if (next != order.end()) keep = std::max(keep, size_t(next - order.begin()) + 1);
  }
  if (order.size() > keep) order.resize(keep);
  while (!order.empty() && minX + order.size() * 24.f > maxX) order.pop_back();  // place réservée à gauche et à droite
  if (order.empty()) return;
  // (Codex) son prochain tour tombe au-delà de la frise (lent, ou compagnon) : son portrait au bout, avec une flèche
  bool far = !atbIf.empty() && order.size() > 1 && std::find(order.begin() + 1, order.end(), actor) == order.end();
  if (far) order.back() = actor;
  float x = std::clamp(160 - order.size() * 12.f, minX, maxX - order.size() * 24.f);
  std::map<const Fighter*, int> seenOnce;  // un ennemi brisé : seul son prochain tour est perdu
  std::set<const Fighter*> marked;         // (2026-10-25) son prochain tour porte son intention : un chevron orange
  bool nextShown = false;                  // le prochain tour du lanceur (aperçu)
  for (size_t i = 0; i < order.size(); i++, x += 24) {
    const FighterP& f = order[i];
    bool ally = isAlly(f), lost = !ally && (f->broken || f.get() == breaks) && seenOnce[f.get()]++ == 0;
    Color edge = ally ? rgb(0x8fd0ff) : f->boss ? rgb(0xffe066) : rgb(0xff8a7a);
    g.rect(x, y, 22, 12, rgb(0x0a0f33, 200));
    g.rect(x, y + 11, 22, 1, edge);
    if (i == 0) g.rect(x, y, 22, 1, edge);  // le prochain à jouer
    if (lost) g.alpha = .4f;
    if (!drawFighterIcon(g, *f, x + 11, y + 6, 20, 10, ally))  // son portrait, sinon le début de son nom
      g.text(x + 11, y + 1, utf8Prefix(f->name(), 3), lost ? GREY : ally ? WHITE : edge, 1, false);
    g.alpha = 1;
    if (lost) g.rect(x + 2, y + 6, 18, 1, rgb(0xffe066));  // barré : tour perdu (brisé)
    if (i > 0 && !atbIf.empty() && f == actor && !nextShown) {  // contour doré qui pulse
      nextShown = true;
      Color gold = rgb(0xffd34d, uint8_t(170 + 85 * std::sin(G.time * 6)));
      g.rect(x - 1, y - 1, 24, 1, gold), g.rect(x - 1, y + 12, 24, 1, gold);
      g.rect(x - 1, y, 1, 12, gold), g.rect(x + 22, y, 1, 12, gold);
      if (far) g.tri(x + 17, y + 2, x + 17, y + 9, x + 21, y + 5.5f, gold);  // plus loin encore
    }
    if (auto it = intents_.find(f.get()); !ally && !lost && f.get() != breaks && it != intents_.end() && marked.insert(f.get()).second && !it->second.wait &&
                                          hasMove(it->second.move) && moveInfo(it->second.move).damaging()) {
      g.tri(x + 15, y + 8, x + 21, y + 8, x + 18, y + 11, rgb(0x2a1606, 220));
      g.tri(x + 16, y + 8.5f, x + 20, y + 8.5f, x + 18, y + 10.5f, rgb(0xff9a3a));
    }
  }
}

void Battle::tickATB(float dt) {
  long long sum = 0;
  for (auto* side : {&allies, &foes})
    for (auto& f : *side) sum += f->hp;
  if (sum < hpSum_) quietT_ = 0;  // quelqu'un a pris des dégâts (un soin, une source ne comptent pas)
  else quietT_ += dt;
  hpSum_ = sum;
  std::vector<FighterP> all = alive(allies);
  for (auto& e : alive(foes)) all.push_back(e);
  for (auto& f : all) {
    f->atb += dt * atbRate(f);
    // Intention (2026-10-24) : un ennemi annonce ce qu'il prépare avant d'agir
    // (Codex, 2026-10-25) la posture d'un boss (cuirasse, voile) s'annonce toujours, même intentions coupées (à mi-jauge)
    std::string mk = isAlly(f) ? std::string() : mech(f);
    bool posture = (mk == "cuirasse" || mk == "voile") && mechDue(f);
    float at = rules().intentAt > 0 && rules().intentAt < 100 ? float(rules().intentAt) : posture ? 50.f : 1000.f;
    if (!isAlly(f) && !announced(f) && !f->broken && f->atb >= at && f->atb < 100 && !intents_.count(f.get()) && !duel() &&
        !follower())
      declareIntent(f);  // (un boss dont la zone rouge attend : elle est déjà son intention)
    if (f->atb >= 100) {
      f->atb = 100;
      guarding_.erase(f.get());  // sa garde tombe quand son tour revient
      posture_.erase(f.get());   // (2026-10-25) la posture d'un boss aussi
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
    intents_.erase(f.get());
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
  // Échiquier : une technique contre un ennemi sans personne à portée (corps à corps trop loin) n'est pas proposée
  auto inReach = [this, a](const Move& mv) {
    if (mv.target != Target::Enemy) return true;
    for (auto& f : alive(foes))
      if (reachable(a, mv, f)) return true;
    return false;
  };
  bool anyTech = false;
  for (auto& id : a->techs()) anyTech = anyTech || inReach(moveInfo(id));
  m.items.push_back({T("Attaquer"), "", anyTech ? T("Utiliser une technique.") : T("Aucun ennemi à portée : approchez-vous d'abord."), anyTech,
                     [this, a, inReach] {
                       Menu t;
                       t.title = T("Techniques");
                       t.rows = 5;
                       t.sel = -1;
                       for (auto& id : a->techs()) {
                         const Move& mv = moveInfo(id);
                         bool ok = inReach(mv);
                         if (ok && t.sel < 0) t.sel = (int)t.items.size();
                         t.items.push_back({mv.name, ok ? typeName(mv.type) : T("hors d'atteinte"), moveBrief(mv, a.get()), ok,
                                            [this, a, id] { moveTarget(a, id); }, [this, a, id] { previewMove(a, id); }});
                         t.items.back().icon = typeDot(mv.type);
                       }
                       t.sel = std::max(0, t.sel);
                       dock(t);
                       G.menus.push(t);
                     }});
  auto sp = a->spells();
  if (!sp.empty())
    m.items.push_back({T("Magie"), "", T("Lancer un sort (coûte des PM)."), true, [this, a, sp, inReach] {
                         Menu t;
                         t.title = T("Magie  ") + std::to_string(a->mp) + "/" + std::to_string(a->mmp) + T(" PM");
                         t.rows = 5;
                         for (auto& id : sp) {
                           const Move& mv = moveInfo(id);
                           bool ok = inReach(mv);
                           t.items.push_back({mv.name, ok ? std::to_string(mv.cost) + T(" PM") : T("hors d'atteinte"), moveBrief(mv, a.get()),
                                              ok && a->mp >= mv.cost, [this, a, id] { moveTarget(a, id); }, [this, a, id] { previewMove(a, id); }});
                           t.items.back().icon = typeDot(mv.type);
                         }
                         dock(t);
                         G.menus.push(t);
                       }});
  // (2026-10-23) un menu plus court : la Limite n'apparaît que prête (en or ; sa jauge rose est dans l'ATH), le Duo
  // seulement possible, « Changer » seulement s'il y a un remplaçant
  if (!a->S().limit.empty() && a->lim >= 100) {
    const Move& lim = moveInfo(a->S().limit);
    m.items.push_back({T("Limite"), "", (a->lim >= 100 ? lim.name + T(" : technique ultime ! ") : T("La jauge Limite (rose) se remplit quand on reçoit des coups. ") + lim.name + " : ") +
                           moveDetails(lim, a.get()),
                       a->lim >= 100, [this, a, lim] {
                         G.menus.clear();
                         useMove(a, lim.id, lim.target == Target::AllAllies ? alive(allies) : alive(foes), true);
                       }});
    m.items.back().color = 0xffd34d;
  }
  if (!duoOptions(a).empty()) {  // un duo possible maintenant
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
                                              [this, a, id, p] { moveTarget(a, id, p); }, [this, a, id, p] {
                                                previewMove(a, id);
                                                paceIf_[p.get()] = 0;  // le partenaire dépense sa jauge
                                              }});
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
                                            }, [this, a] { paceAfter(a, 0); }});
                       }
                       if (t.items.empty()) t.items.push_back({T("(sac vide)"), "", "", false, nullptr});
                       dock(t);
                       G.menus.push(t);
                     }});
  // Se déplacer (3.4) : une fois par tour, avant d'agir ; le tour continue ensuite
  if (!moved_.count(a.get())) {
    std::vector<int> cells;
    for (auto& [c, d] : walkable(a)) cells.push_back(c);
    m.items.push_back({T("Se déplacer"), "", cells.empty() ? T("Aucune case libre à portée.")
                                                            : T("Aller jusqu'à ") + std::to_string(moveRange(a)) + T(" cases plus loin, puis agir."),
                       !cells.empty(), [this, a, cells] {
                         Menu t;
                         t.title = T("Se déplacer");
                         t.rows = 5;
                         int edge = BOARD_W - 1;
                         for (int c : cells) {
                           const Rules::Terrain* tr = terrainOf(c);
                           std::string where = boardX(c) == edge && !breakthrough_.count(a.get()) && rules().breakthrough > 0
                                                   ? T("Bord adverse : Percée ! Attaque, Magie et Vitesse +") + std::to_string(rules().breakthrough) + "."
                                               : !inHalf(true, c) ? T("Camp adverse : gare à l'encerclement.")
                                                                  : std::string();
                           int threat = meleeThreat(a, c);
                           std::string danger = threat == 0 ? std::string(T(" · Hors de portée des ennemis au prochain tour."))
                                                            : T(" · Danger : ") + std::to_string(threat) + T(" ennemi(s) pourront frapper ici.");
                           FighterP e = backReach(a, c);  // le plus utile d'abord : l'aide est coupée après deux lignes
                           std::string back = e ? T(" · Dans le dos de ") + e->name() + "." : std::string();
                           t.items.push_back({boardName(c), "", boardName(c) + back + (tr ? " · " + tr->name + " : " + tr->desc : where.empty() ? "" : " · " + where) + danger, true,
                                              [this, a, c] { moveTo(a, c); }});
                         }
                         // la case la plus proche du combattant d'abord
                         int best = 99;
                         for (size_t i = 0; i < cells.size(); i++)
                           if (int d = boardSteps(cells[i], cellOf(a)) * 4 + boardY(cells[i]); d < best) best = d, t.sel = (int)i;
                         boardMenu(t, cells, true);
                       }});
  }
  if (moved_.count(a.get()) && moveFrom_.count(a.get()) && !at(moveFrom_[a.get()]))  // annuler son déplacement
    m.items.push_back({T("Revenir"), "", T("Annuler son déplacement : retour à sa case, on peut de nouveau se déplacer."), true, [this, a] {
                         G.menus.clear();
                         if (shown()) slide_[a.get()] = {pos(a), G.time};
                         cells_[a.get()] = moveFrom_[a.get()];
                         moveFrom_.erase(a.get());
                         if (int f = faceFrom_[a.get()]) facing_[a.get()] = f;  // il retrouve aussi son regard
                         else facing_.erase(a.get());
                         moved_.erase(a.get());
                         sound("pas");
                         log.push_back(a->name() + T(" revient sur ses pas"));
                         command(a);
                       }});
  m.items.push_back({T("Attendre"), "", T("Se mettre en garde jusqu'à son prochain tour : un quart de dégâts en moins, une riposte à chaque coup au corps à corps, face à l'ennemi le plus proche ; il rejoue plus tôt."), true, [this, a] { waitTurn(a); }});
  std::vector<FighterP> bench;  // avec des héros : un héros n'est remplacé que par un héros
  bool inTeam = std::find(G.team.begin(), G.team.end(), a) != G.team.end();  // défi : des combattants prêtés, sans remplaçants
  for (auto& r : G.team)
    if (inTeam && r->alive() && !isAlly(r) && (!hasHero_ || r->S().human == a->S().human)) bench.push_back(r);
  if (!bench.empty())
    m.items.push_back({T("Changer"), "", T("Faire entrer un remplaçant de l'équipe."), true, [this, a, bench] {
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
  // Entraînement : le sac du joueur n'est pas à disposition (une défaite ne doit rien coûter)
  if (training_)
    m.items.erase(std::remove_if(m.items.begin(), m.items.end(), [](const MenuItem& it) { return it.label == T("Objet"); }), m.items.end());
  // Duel : ni objets ni remplaçants (l'équipement compte) ; gardien à plusieurs : pas de remplaçants
  if (net_ != Net::None) {
    bool d = duel();
    m.items.erase(std::remove_if(m.items.begin(), m.items.end(),
                                 [d](const MenuItem& it) { return (d && it.label == T("Objet")) || it.label == T("Changer"); }),
                  m.items.end());
  }
  // (2026-10-23) une petite icône devant chaque commande, comme au menu de pause
  setIcons(m, {
      {T("Attaquer"), "ic_epee_1"}, {T("Magie"), "ic_baton_1"}, {T("Limite"), "etoile"}, {T("Duo"), "duo"}, {T("Objet"), "ic_sac"},
      {T("Se déplacer"), "pas"}, {T("Revenir"), "retour"}, {T("Attendre"), "sablier"}, {T("Changer"), "echange"}, {T("Fuir"), "drapeau"}});
  // (2026-10-26) frise : « Attendre » (jauge rapide) et la Limite montrent où tombera son prochain tour, le reste rien
  for (auto& it : m.items)
    it.hover = [this, a, wait = it.label == T("Attendre"), lim = it.label == T("Limite")] {
      if (wait || lim) paceAfter(a, wait ? float(rules().quickGauge) : 0.f);
      else paceDepth_ = 0;
    };
  // Le curseur sur la première action possible (rien à portée : « Se déplacer »)
  for (size_t i = 0; i < m.items.size(); i++)
    if (m.items[i].enabled && (anyTech || m.items[i].label == T("Se déplacer") || m.items[i].label == T("Attendre"))) {
      m.sel = (int)i;
      break;
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
    if (aiming_ && !reachable(actor, *aiming_, f)) continue;  // corps à corps : au contact, ou qu'on peut rejoindre
    list.push_back(f);
  }
  // dans l'ordre du plateau (de haut en bas, de gauche à droite) : Haut et Bas suivent l'écran
  std::stable_sort(list.begin(), list.end(), [&](const FighterP& a, const FighterP& b) {
    int ca = std::max(0, cellOf(a)), cb = std::max(0, cellOf(b));
    return boardY(ca) != boardY(cb) ? boardY(ca) < boardY(cb) : boardX(ca) < boardX(cb);
  });
  std::vector<int> cells;
  for (auto& f : list) {
    if (f == cursor) t.sel = (int)t.items.size();  // la cible de l'aperçu
    std::string plan = intentText(*f);  // ce qu'il prépare (2026-10-24)
    t.items.push_back({f->name(), T("N.") + std::to_string(f->lvl), f->name() + T(" N.") + std::to_string(f->lvl) + " · " + (plan.empty() ? "" : plan + " · ") + info(*f),
                       true, [done, f] { done(f); }, [this, f] { cursor = f; }});
    cells.push_back(cellOf(f));
  }
  boardMenu(t, cells);
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
  boardMenu(t, cells);
}

// ---------------------------------------------------------------------------
// Choix sur le damier (3.2)
// ---------------------------------------------------------------------------
void Battle::boardMenu(Menu t, std::vector<int> cells, bool move) {
  t.hidden = true;
  dock(t);
  pick_ = {G.menus.depth() + 1, std::move(cells), move};
  G.menus.push(t);
}
bool Battle::boardPicking() const {
  return pick_.depth && G.menus.depth() == pick_.depth && G.menus.active() && G.menus.top().hidden;
}
int Battle::cellUnder(float x, float y, const std::vector<int>& among) const {
  int best = -1;
  float bd = 1e9f;
  for (size_t i = 0; i < among.size(); i++) {
    Pt p = cellPos(among[i]);
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
    Pt from = cellPos(pick_.cells[(size_t)std::clamp(m.sel, 0, n - 1)]);
    int best = -1;
    float bs = 1e9f;
    for (int i = 0; i < n; i++) {
      if (i == m.sel || !m.items[(size_t)i].enabled) continue;
      Pt p = cellPos(pick_.cells[(size_t)i]);
      float vx = p.x - from.x, vy = p.y - from.y, along = vx * DX[d] + vy * DY[d], across = std::abs(vx * DY[d] - vy * DX[d]);
      if (along < 3) continue;
      if (along + across * 2 < bs) bs = along + across * 2, best = i;
    }
    choose(best);
  }
  if (in.mouseOn) {
    int at = cellUnder((float)in.mx, (float)in.my, pick_.cells);
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
  paceAfter(a, paceGauge(m), id);  // la frise : son prochain tour après elle
  if (m.target != Target::Enemy) return;
  if (cursor && cursor->alive() && !isAlly(cursor) && reachable(a, m, cursor)) return;
  cursor = nullptr;
  for (auto& f : alive(foes)) {  // le plus proche à portée
    if (!reachable(a, m, f)) continue;
    if (!cursor || boardSteps(cellOf(f), cellOf(a)) < boardSteps(cellOf(cursor), cellOf(a))) cursor = f;
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
    if (G.weakKnown(f.sp, m->type) && moveEff(*m, f.S()) > 1) s += T(" · faiblesse !");
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
      pickFoe(T("Cible"), [this, &m, a](const Fighter& f) {
        FighterP fp;  // le combattant (partagé) qui correspond à cette fiche
        for (auto& o : foes)
          if (o.get() == &f) fp = o;
        return foeInfo(G, f, &m) + damageText(a, m, fp);
      }, [go](FighterP t) { go({t}); });
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
  if (!targets.empty() && targets[0]) face(p, cellOf(targets[0]));  // le partenaire se tourne aussi vers la cible
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
  // Les ennemis ne visent pas toujours la meilleure cible : souvent, ils frappent « au hasard », mais (2026-10-24) les
  // soigneurs, les lanceurs de sorts et les blessés les attirent : l'équipe doit les couvrir (garde, protecteur, recul)
  FighterP focus;
  if (!ally && frand() < .65f) {
    float sum = 0;
    for (auto& t : theirs) sum += lure(t);
    float r = frand() * sum;
    for (auto& t : theirs)
      if (focus = t, (r -= lure(t)) <= 0) break;
  }

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
          if (v <= 0 || (onFoes && postureMult(t, m) <= 0)) continue;  // voilé de brume : l'état le traverse
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
            // (2026-10-25) cible préférée en garde : pas au corps à corps (riposte), un autre adversaire
            if ((!focus || t == focus || !reachable(a, m, focus) || (m.melee && guarding_.count(focus.get()))) && reachable(a, m, t))
              consider(id, {t}, zoneScore(a, m, t));
        break;
    }
  }
  if (best.move.empty()) {  // rien d'intéressant : la première technique, sur une cible à portée
    const Move& m0 = moveInfo(a->techs()[0]);
    std::vector<FighterP> ok;
    for (auto& t : theirs)
      if (reachable(a, m0, t)) ok.push_back(t);
    if (ok.empty()) {  // personne à portée (échiquier) : il attend
      best = Plan{};
      best.wait = true;
      return best;
    }
    best = {m0.id, {ok[irand(0, (int)ok.size() - 1)]}, false};
  }
  return best;
}


// Attrait d'un combattant pour les ennemis qui choisissent qui frapper : un soigneur vaut le double, un lanceur de
// sorts un peu plus, un blessé attire d'autant plus qu'il est bas (achever) ; un boss n'y est pas plus sensible
float Battle::lure(const FighterP& t) const {
  float w = 1;
  bool heals = t->S().combatRole == "soigneur";
  for (auto& id : t->spells()) heals = heals || moveInfo(id).kind == Kind::Heal || moveInfo(id).kind == Kind::Revive;
  if (heals) w += 1;
  if (t->eMag() > t->eAtk()) w += .5f;
  w += 1.5f * (1 - float(t->hp) / std::max(1, t->mhp));
  w += .75f * aimedBy(t);  // meute (2026-10-24) : déjà visé par un des leurs, les autres s'y mettent
  return w;
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
  score *= posMult(a, t, m);  // échiquier : dans le dos, encerclé, distance au contact
  if (m.melee && boardDist(cellOf(a), cellOf(t)) > reach(a)) score *= .9f;  // il faut d'abord s'approcher
  if (boardDist(cellOf(a), cellOf(t)) <= 1 && behind(t, a)) score *= 1.4f;  // il est dans mon dos : le frapper, c'est se retourner
  if (m.melee && guarding_.count(t.get())) score *= .4f;  // (2026-10-25) en garde, il riposterait : frapper ailleurs
  if (postureMult(t, m) <= 0) return 0;                  // voilé de brume : le coup et ses effets le traversent
  score *= postureMult(t, m);                             // un boss cuirassé (coups physiques) ou voilé (hors corps à corps)
  if (!ally) return score / paceTime(m);
  score *= 1 + std::min(1.f, (a->crit + m.critBonus) / 100.f) * float(R.critMult - 1);
  if (score >= t->hp * 1.2f) score += 15;  // achever un adversaire affaibli
  if (!t->S().combatRole.empty() && !t->boss) score *= 1.25f;  // rôles : abattre d'abord soigneur, chef et protecteur
  if (m.shove && !t->boss) {  // pousser / tirer : un choc contre un voisin ou le bord, ou une case qui blesse
    int to = shoveDest(a, t, m);
    bool clash = (to == -1 && m.shove > 0) || (to >= 0 && ((at(to) && at(to) != a) || blockedCell(to)));
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

Battle::Preview Battle::preview(const FighterP& a, const Move& m, const FighterP& d, int fromCell) const {
  Preview pv;
  const Rules& R = rules();
  bool magic = m.kind == Kind::Magic;
  float hit = m.acc / 100.f * a->acc / 100.f - (magic ? 0 : d->eva / 100.f);
  if (!magic && !engaged(d)) hit -= float(talentValue(d->S(), "esquive_arriere"));  // talent Furtif : loin de la mêlée
  if (!magic && weather_ >= 0) hit += R.weathers[(size_t)weather_].acc / 100.f;
  if (const Rules::Terrain* tr = terrainAt(d); !magic && tr && tr->kind == "esquive") hit -= float(tr->value) / 100.f;
  // Brisure : l'efficacité d'un type n'est comptée qu'une fois découverte (sinon neutre, quelle qu'elle soit : rien ne
  // se devine sans attaquer ; la fiche de la cible montre déjà « ? »)
  float eff = G.weakKnown(d->sp, m.type) ? moveEff(m, d->S()) : 1.f;
  if (postureMult(d, m) <= 0) eff = 0;  // (2026-10-25) voilé de brume : le coup le traverse
  pv.eff = eff, pv.hit = std::clamp(hit, 0.f, 1.f);
  if (eff <= 0) return pv;
  float A = magic ? a->eMag() : a->eAtk(), D = magic ? d->eRes() : d->eDef();
  float stab = hasType(a->S(), m.type) ? float(R.stab) : 1.f, k = 1;
  // Échiquier : au corps à corps, il s'approche d'abord (vers la case que choisira approachCell)
  int from = fromCell >= 0 ? fromCell : m.melee && boardDist(cellOf(a), cellOf(d)) > reach(a) ? approachCell(a, d) : -1;
  k *= posMult(a, d, m, from);
  pv.back = m.melee && behind(a, d, from);
  if (d->broken) k *= float(R.breakDamage);
  k *= 1 - float(talentValue(d->S(), magic ? "resiste_magie" : "resiste_physique"));
  if (hasType(a->S(), m.type) && a->hp * 3 < a->mhp) k *= 1 + float(talentValue(a->S(), "rage"));
  k *= weatherMult(m.type) * guardMult(d);
  if (guarding_.count(d.get())) k *= float(R.guard);  // en garde
  k *= postureMult(d, m);                             // cuirasse d'un boss
  if (const Rules::Terrain* tr = terrainAt(a); magic && tr && tr->kind == "magie") k *= 1 + float(tr->value);
  float base = ((2 * a->lvl / 5.f + 2) * m.power * A / std::max(1.f, D) / float(R.dmgDivisor) + 2) * stab * eff * k;
  pv.lo = std::max(1, int(base * float(R.spreadMin))), pv.hi = std::max(1, int(base));
  return pv;
}

std::string Battle::damageText(const FighterP& a, const Move& m, const FighterP& d, bool* breaks) const {
  if (!m.damaging() || !a || !d) return "";
  Preview pv = preview(a, m, d);
  if (pv.eff < 0) return T(" · absorbé : le soigne !");
  if (postureMult(d, m) <= 0)
    return veilType(*d).empty() ? T(" · traverse la brume : au contact !") : withType(T(" · traverse : seuls les coups de type {t} passent"), veilType(*d));
  if (pv.eff <= 0) return T(" · aucun effet");
  std::string s = T(" · ≈ ") + std::to_string(pv.lo) + (pv.hi > pv.lo ? "-" + std::to_string(pv.hi) : "") + T(" PV");
  s += " · " + std::to_string(int(std::lround(pv.hit * 100))) + " %";
  if (pv.back) s += T(" · dans le dos");
  if (m.melee && guarding_.count(d.get()) && isAlly(a) != isAlly(d)) s += T(" · il riposte");  // (2026-10-25) en garde, cuirassé
  // (2026-10-25) ce que le coup changerait : l'abattre annule son intention, le briser lui fait perdre son tour
  if (d->alive() && pv.lo >= d->hp) s += T(" · l'abat");
  else if (d->alive() && pv.hi >= d->hp) s += T(" · peut l'abattre");
  else if (pv.eff > 1 && d->shieldMax > 0 && !d->broken &&
           d->shield <= (pv.eff >= 4 ? 2 : 1) + int(talentValue(a->S(), "briseur") + gemSupport(*a, "briseur"))) {
    s += T(" · le brise");
    if (breaks) *breaks = true;
  }
  return s;
}

void Battle::autoCommand(FighterP a) {
  actor = a;
  if (!moved_.count(a.get())) {  // échiquier : d'abord la meilleure case (à l'abri d'une attaque annoncée)
    int c = planMove(a);
    if (c < 0 && threatened(a)) c = safeCell(a);
    step(a, c);
    moved_.insert(a.get());
  }
  Plan p = think(a);
  if (p.wait) waitTurn(a);
  else if (p.duo) duoMove(a, p.duo, p.move, p.targets);
  else useMove(a, p.move, p.targets, p.limit);
}

void Battle::enemyTurn(FighterP e) {
  actor = e;
  auto it = intents_.find(e.get());
  bool declared = it != intents_.end();
  Intent in = declared ? it->second : Intent{};
  if (declared) intents_.erase(it);
  if (!mech(e).empty()) mechTurns_[e.get()]++;  // (2026-10-25) le tour qui commence compte pour sa mécanique
  if (bossPlan(e)) return;
  if (!moved_.count(e.get())) {
    int c = -1;  // la case annoncée, si un chemin y mène encore (un allié a pu fermer le passage, une poussée le déplacer)
    if (declared && in.cell >= 0)
      for (auto& [w, steps] : walkable(e)) c = w == in.cell ? w : c;
    if (declared && in.cell == cellOf(e)) c = in.cell;
    step(e, c >= 0 ? c : planMove(e));
    moved_.insert(e.get());
  }
  // L'intention annoncée, si elle tient toujours (cibles encore au combat, à portée, PM, soin encore utile) ; sinon il
  // réfléchit à nouveau
  bool valid = declared && !in.wait && !in.move.empty() && hasMove(in.move) && e->mp >= moveInfo(in.move).cost;
  if (valid) {
    const Move& m = moveInfo(in.move);
    if (m.target == Target::AllEnemies) in.targets = alive(allies);  // tout un camp : ceux du moment
    else if (m.target == Target::AllAllies) in.targets = alive(foes);
    valid = !in.targets.empty();
    for (auto& t : in.targets)  // encore au combat (pas un allié qui a reculé : sa case reste dans cells_)
      valid = valid && t && t->alive() && (std::count(allies.begin(), allies.end(), t) || std::count(foes.begin(), foes.end(), t));
    if (valid && in.targets.size() == 1 && !reachable(e, m, in.targets[0])) valid = false;
    if (valid && m.kind == Kind::Heal && in.targets[0]->hp * 4 >= in.targets[0]->mhp * 3) valid = false;  // déjà remis
  }
  // (2026-10-25) dessin : l'icône de l'intention pulse quand il l'exécute, se brise quand elle est déjouée
  if (declared && shown()) intentFx_[e.get()] = {in, G.time, !valid && !in.wait};
  if (valid) return useMove(e, in.move, in.targets);
  if (declared && !in.wait && !in.move.empty()) {  // l'annonce ne tient plus : le joueur a bien réagi, qu'il le sache
    log.push_back(e->name() + T(" : intention déjouée"));
    if (shown()) pop(e, T("Déjoué !"), rgb(0x8fd0ff)), sound("dejoue");
  }
  if (declared && in.wait) return in.posture.empty() ? waitTurn(e) : takePosture(e, in.posture);
  if (roleHeal(e)) return;
  Plan p = think(e);
  if (p.wait) waitTurn(e);
  else useMove(e, p.move, p.targets);
}

// Ce qu'il fera à son tour, décidé comme à son tour (sa case, puis son action de là), gardé pour l'affichage
void Battle::declareIntent(const FighterP& e) {
  Intent in;
  in.t0 = G.time;
  int keep = cellOf(e);
  bool had = moved_.count(e.get()) > 0;
  if (!had && keep >= 0) in.cell = planMove(e);
  if (in.cell >= 0) cells_[e.get()] = in.cell, moved_.insert(e.get());
  Plan p = think(e);
  if (in.cell >= 0) cells_[e.get()] = keep;
  if (!had) moved_.erase(e.get());
  if (e->S().combatRole == "soigneur" && hasMove("soin") && frand() >= .25f) {  // comme roleHeal : le plus mal en point des siens
    FighterP worst;
    for (auto& o : alive(foes))
      if (o->hp * 2 < o->mhp && (!worst || o->hp * worst->mhp < worst->hp * o->mhp)) worst = o;
    if (worst) p = Plan{"soin", {worst}, false};
  }
  // (2026-10-25) un boss à posture (cuirasse, voile) la prend un tour sur « tous » : annoncée ici, avec son message
  if (std::string k = mech(e); (k == "cuirasse" || k == "voile") && mechDue(e)) {
    p = Plan{}, p.wait = true, in.posture = k;
    if (std::string msg = tr(jget<std::string>(e->mech, "message", "")); !msg.empty()) sc.say(msg, 1.2f);
    tip(veilType(*e).empty() ? k.c_str() : "voile_type");
  }
  in.move = p.move, in.targets = p.targets, in.wait = p.wait;
  intents_[e.get()] = in;
  if (shown() && !autoPlay && !in.wait && !in.move.empty()) tip("intention");
  // (2026-10-25) un petit signal quand un allié est visé, au plus un par demi-seconde
  if (!in.wait && G.time - aimSoundT_ >= .5f && std::any_of(in.targets.begin(), in.targets.end(), [&](const FighterP& t) { return t && isAlly(t); }))
    aimSoundT_ = G.time, sound("vise");
}

std::string Battle::intentText(const Fighter& e) const {
  auto it = intents_.find(&e);
  if (it == intents_.end() || !e.alive()) return "";
  const Intent& in = it->second;
  // (2026-10-25) un protecteur dit quel blessé des siens il va couvrir (voisin de sa future case)
  std::string cover;
  int from = in.cell;
  for (auto& o : foes)
    if (o.get() == &e && from < 0) from = cellOf(o);
  if (e.S().combatRole == "protecteur" && from >= 0)
    for (auto& o : foes)
      if (o.get() != &e && o->alive() && o->hp * 2 < o->mhp && o->S().combatRole != "protecteur" && inBoardZone(Area::Cross, from, cellOf(o)))
        cover = T(" (couvre {c})"), cover.replace(cover.find("{c}"), 3, o->name());
  if (in.wait)
    return (in.posture == "cuirasse" ? T("Va se cuirasser : la magie passe, le corps à corps est puni")
            : in.posture == "voile"  ? (veilType(e).empty() ? T("Va se voiler : seul le corps à corps l'atteindra")
                                                             : withType(T("Va se voiler : seuls les coups de type {t} l'atteindront"), veilType(e)))
                                     : T("Se met en garde")) + cover;
  if (in.move.empty() || !hasMove(in.move)) return "";
  std::string who;
  for (auto& t : in.targets)
    if (t && t->alive()) who += (who.empty() ? "" : ", ") + t->name();
  std::string s = who.empty() ? T("Prépare {t}") : T("Prépare {t} sur {c}");
  if (size_t at = s.find("{t}"); at != std::string::npos) s.replace(at, 3, moveInfo(in.move).name);
  if (size_t at = s.find("{c}"); at != std::string::npos) s.replace(at, 3, who);
  return s + cover;
}

// ---------------------------------------------------------------------------
// Tactiques des alliés (voir tactics.hpp) : de haut en bas, la première règle
// dont la condition vise quelqu'un et dont l'action est possible est jouée.
// ---------------------------------------------------------------------------
bool Battle::tacticsActive() const { return autoPlay ? simTactics : G.tacticsAuto; }

// ---------------------------------------------------------------------------
// Damier : attaques annoncées des boss, déplacements de l'ordinateur
// ---------------------------------------------------------------------------
// Une case dangereuse pour f : terrain qui blesse (braises, ronces, verglas) ; pour un allié, la zone rouge d'un boss, ou
// (2026-10-24) une intention ennemie : visé lui-même au corps à corps, seules les cases hors de portée de l'ennemi sont
// sûres (il devra choisir autre chose) ; un voisin visé par une zone, s'en écarter. Un sort à distance n'est pas un
// danger de case : rien à y faire en bougeant (se mettre en garde, ou frapper l'ennemi d'abord)
bool Battle::dangerAt(const FighterP& f, int c) const {
  if (terrainHurts(f, c)) return true;
  if (!isAlly(f)) return false;  // les attaques annoncées visent les alliés
  for (auto& w : warnings_)
    if (inBoardZone(w.zone, w.center, c)) return true;
  for (auto& e : foes) {
    auto it = intents_.find(e.get());
    if (it == intents_.end() || !e->alive()) continue;
    const Intent& in = it->second;
    if (in.wait || in.move.empty() || !hasMove(in.move) || !moveInfo(in.move).damaging()) continue;
    const Move& m = moveInfo(in.move);
    int from = in.cell >= 0 ? in.cell : cellOf(e);
    for (auto& tg : in.targets) {
      if (!tg || !tg->alive()) continue;
      if (tg == f) {
        if (m.melee && from >= 0 && boardDist(from, c) <= reach(e)) return true;
      } else if (m.zone != Area::Single && m.zone != Area::All && cellOf(tg) >= 0 && inBoardZone(m.zone, cellOf(tg), c))
        return true;
    }
  }
  return false;
}
bool Battle::threatened(const FighterP& f) const { return dangerAt(f, cellOf(f)); }
int Battle::aimedBy(const FighterP& f) const {
  int n = 0;
  for (auto& [who, in] : intents_) {
    if (!who->alive() || in.wait || in.move.empty() || !hasMove(in.move) || !moveInfo(in.move).damaging()) continue;
    for (auto& tg : in.targets) n += tg == f;
  }
  return n;
}
bool Battle::announced(const FighterP& f) const {
  for (auto& w : warnings_)
    if (w.who == f) return true;
  return false;
}
int Battle::safeCell(const FighterP& f) const {
  int from = cellOf(f), best = -1, bestD = 99;
  if (moved_.count(f.get())) return -1;
  for (auto& [c, steps] : walkable(f)) {
    bool danger = dangerAt(f, c);
    int d = steps * 2 + (inHalf(isAlly(f), c) ? 0 : 3) + std::abs(boardY(c) - boardY(from));  // rester chez soi si possible
    if (!danger && d < bestD) best = c, bestD = d;
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
        if (inBoardZone(w.zone, w.center, cellOf(a))) hit.push_back(a);
      if (w.kind == "ronces") {  // (2026-10-25) les ronces (braises, foudre…) couvrent toute la zone et y restent ; les anciennes se fanent
        int ti = -1;
        for (size_t i = 0; i < rules().terrains.size(); i++)
          if (rules().terrains[i].id == jget<std::string>(e->mech, "terrain", "")) ti = (int)i;
        if (ti >= 0) {
          for (auto& [c, old] : grown_) terrain_[(size_t)c] = old;
          grown_.clear();
          for (int c = 0; c < BOARD_CELLS; c++)
            if (inBoardZone(w.zone, w.center, c) && inHalf(true, c) && !blockedCell(c)) grown_[c] = terrain_[(size_t)c], terrain_[(size_t)c] = ti;  // (chez les alliés : une ligne traverse le damier)
          log.push_back("  " + rules().terrains[(size_t)ti].name + " : " + zoneName(w.zone) + ", " + boardName(w.center));
          sound("statut");
          sc.say(rules().terrains[(size_t)ti].name + T(" : la zone en reste couverte, n'y finissez pas votre tour !"), 1.1f);
        }
      }
      if (hit.empty()) {
        log.push_back(e->name() + " : " + moveInfo(w.move).name + T(" (annoncée), personne dans la zone"));
        sc.say(e->name() + T(" déchaîne ") + moveInfo(w.move).name + T("… mais tout le monde s'est écarté !"), 1.2f);
        return false;  // il joue tout de même une attaque normale
      }
      log.push_back(e->name() + " : " + moveInfo(w.move).name + T(" (annoncée) sur ") + std::to_string(hit.size()));
      int charge = w.kind == "charge" ? 1 : w.kind == "attire" ? -1 : 0;
      if (charge) {  // (2026-10-25) charge : les plus éloignés reculent d'abord (ceux qui suivent les heurtent) ; attire : les plus proches avancent d'abord
        int ex = boardX(cellOf(e));
        std::stable_sort(hit.begin(), hit.end(), [&](const FighterP& x, const FighterP& y) {
          return std::abs(boardX(cellOf(x)) - ex) * charge > std::abs(boardX(cellOf(y)) - ex) * charge;
        });
        sc.say(e->name() + (charge > 0 ? T(" charge à travers la ligne !") : T(" happe ceux qui sont restés dans la zone !")), .8f);
      }
      useMove(e, w.move, hit, false, float(rules().bossBurst), charge);
      return true;
    }
  // (2026-10-25) un boss à mécanique de zone (ronces, charge, attire) l'annonce un tour sur « tous », à la place des annonces au hasard
  std::string k = mech(e);
  bool zoned = k == "ronces" || k == "charge" || k == "attire";
  if (!zoned && !k.empty() && mechDue(e)) return false;  // sa posture vient au tour suivant : pas de zone rouge qui l'empêche
  if (alive(allies).empty() || (zoned ? !mechDue(e) : frand() >= rules().bossWarn)) return false;
  // Sa plus forte attaque (ou celle de sa mécanique), concentrée sur la zone où se trouvent le plus d'alliés
  std::string best = zoned ? jget<std::string>(e->mech, "technique", "") : "";
  if (!best.empty() && (!hasMove(best) || e->mp < moveInfo(best).cost)) best.clear();
  int power = 0;
  std::vector<std::string> moves = e->techs();
  for (auto& id : e->spells())
    if (e->mp >= moveInfo(id).cost) moves.push_back(id);
  if (best.empty())
    for (auto& id : moves)
      if (moveInfo(id).damaging() && moveInfo(id).power > power) power = moveInfo(id).power, best = id;
  if (best.empty()) return false;
  std::vector<Area> shapes = {Area::Column, Area::Cross, Area::Line};
  if (zoned)  // sa forme, « zone » (par défaut : une ligne pour la charge, une colonne pour attire, une croix pour les ronces)
    for (Area z : {Area::Line, Area::Column, Area::Cross})
      if (jget<std::string>(e->mech, "zone", k == "charge" ? "ligne" : k == "attire" ? "colonne" : "croix") == zoneId(z)) shapes = {z};
  int center = -1, most = 0;
  Area zone = shapes[0];
  for (Area z : shapes)
    for (auto& a : alive(allies)) {
      int n = 0;
      for (auto& b : alive(allies)) n += inBoardZone(z, cellOf(a), cellOf(b));
      if (n > most || (n == most && frand() < .3f)) most = n, zone = z, center = cellOf(a);
    }
  warnings_.push_back({e, best, zone, center, zoned ? k : ""});
  log.push_back(e->name() + T(" annonce ") + moveInfo(best).name + " (" + zoneName(zone) + ", " + boardName(center) + ")");
  sound("statut");
  std::string msg = zoned ? tr(jget<std::string>(e->mech, "message", "")) : "";
  if (msg.empty()) msg = e->name() + T(" prépare ") + moveInfo(best).name + T(" ! Écartez-vous de la zone rouge.");
  if (zoned) tip(k.c_str());
  sc.say(msg, 1.4f);
  return false;  // l'annonce ne lui coûte pas son tour : il attaque aussitôt
}

// Mécaniques des boss (2026-10-25), voir battle.hpp
std::string Battle::mech(const FighterP& e) const {
  if (net_ != Net::None || !e || !e->boss || isAlly(e) || !e->mech.is_object()) return "";
  return jget<std::string>(e->mech, "genre", "");
}
bool Battle::mechDue(const FighterP& e) const {
  auto it = mechTurns_.find(e.get());
  return ((it == mechTurns_.end() ? 0 : it->second) + 1) % std::max(2, jget(e->mech, "tous", 3)) == 0;
}
// Posture prise à son tour (son intention l'annonçait) : jusqu'à son prochain tour. Cuirasse : en garde (il riposte à
// chaque coup au corps à corps), les coups physiques à « valeur » ; voile : seuls les coups au corps à corps l'atteignent
void Battle::takePosture(const FighterP& e, const std::string& kind) {
  bool armor = kind == "cuirasse";
  posture_[e.get()] = kind;
  if (armor) guarding_.insert(e.get()), facing_[e.get()] = nearestFoeSide(e);
  log.push_back(e->name() + (armor ? T(" se cuirasse") : T(" se voile de brume")));
  pop(e, armor ? T("Cuirasse !") : T("Voile !"), armor ? rgb(0xc8d6f0) : rgb(0xc8b8ff));
  sound(armor ? "garde" : "statut");
  sc.say(e->name() + (armor                    ? T(" se cuirasse : la magie passe, le corps à corps est puni !")
                      : veilType(*e).empty() ? T(" se voile : seul le corps à corps l'atteint !")
                                             : withType(T(" se voile : seuls les coups de type {t} l'atteignent !"), veilType(*e))), 1.1f);
  sc.call([this, e] { afterAction(e, float(rules().quickGauge)); });
}
float Battle::postureMult(const FighterP& d, const Move& m) const {
  auto it = posture_.find(d.get());
  if (it == posture_.end()) return 1;
  if (it->second == "voile") {  // sorts, tirs et états la traversent ; voile d'un type : tout, sauf les coups de ce type
    std::string ty = jget<std::string>(d->mech, "type", "");
    return (ty.empty() ? m.melee : types()[(size_t)m.type].id == ty) ? 1.f : 0.f;
  }
  return m.kind == Kind::Physical ? jget(d->mech, "valeur", .5f) : 1.f;
}
bool Battle::tacticTurn(FighterP a) {
  if (!a->tacticsOn) return false;
  // Échiquier : en mode auto, il choisit d'abord sa case comme l'ordinateur (à l'abri d'une zone annoncée, au contact,
  // dans le dos…) ; s'il reste en place, il peut encore s'approcher pour une attaque choisie par ses tactiques
  // (sauf si une de ses règles est un déplacement qui s'applique : il la suit)
  Plan first;
  bool ruleMoves = !moved_.count(a.get()) && tacticPlan(a, first) && first.cell >= 0;
  if (!moved_.count(a.get()) && !follower() && !ruleMoves) {
    int c = planMove(a);
    if (c >= 0) {
      actor = a;
      step(a, c);
    }
  }
  Plan p;
  if (!tacticPlan(a, p)) {
    // Échiquier : si aucune de ses attaques n'atteint personne, il s'approche (puis ses tactiques rejouent), sinon il
    // attend ; s'il a de quoi frapper, c'est au joueur de choisir (menu), comme avant
    std::vector<std::string> mine = a->techs();
    for (auto& id : a->spells())
      if (a->mp >= moveInfo(id).cost) mine.push_back(id);
    for (auto& id : mine)
      for (auto& f : alive(foes))
        if (moveInfo(id).damaging() && reachable(a, moveInfo(id), f)) return false;
    actor = a;
    if (!moved_.count(a.get()))
      if (int c = planMove(a); c >= 0) {
        log.push_back(a->name() + T(" : tactiques, s'approche"));
        moveTo(a, c);
        return true;
      }
    waitTurn(a);
    return true;
  }
  actor = a;
  log.push_back(a->name() + T(" : tactique ") + std::to_string(p.tactic + 1));
  if (p.wait) return waitTurn(a), true;  // se mettre en garde
  if (p.cell >= 0 && follower()) return moveTo(a, p.cell), true;  // le chef déplace, puis renvoie « tour » (bouge)
  if (p.cell >= 0) {  // se mettre à l'abri, puis agir (une autre règle, sinon l'ordinateur)
    step(a, p.cell);
    moved_.insert(a.get());
    Plan q;
    if (!tacticPlan(a, q) || q.cell >= 0) return autoCommand(a), true;
    log.push_back(a->name() + T(" : tactique ") + std::to_string(q.tactic + 1));
    p = q;
    if (p.wait) return waitTurn(a), true;  // se mettre en garde après s'être déplacé (relecture de Codex)
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
    if (c == "allie_avant_pv_moins") return engaged(t) && t->hp * 100 < tc.value * t->mhp;  // au contact d'un ennemi
    if (c == "ennemi_brise") return t->broken;
    if (c == "ennemi_role") return t->S().combatRole == "soigneur" || t->S().combatRole == "chef";
    if (c == "ennemi_dos") {  // son dos est à portée : d'ici, ou d'une case où il peut aller ce tour
      if (boardDist(cellOf(a), cellOf(t)) <= reach(a) && behind(a, t)) return true;
      if (moved_.count(a.get())) return false;
      for (auto& [w, steps] : walkable(a))
        if (boardDist(w, cellOf(t)) <= reach(a) && behind(a, t, w)) return true;
      return false;
    }
    if (c == "allie_vise") return aimedBy(t) > 0;  // un ennemi a annoncé une attaque sur cet allié
    if (c == "soi_vise") {  // une intention ennemie le vise (pas dans une zone rouge : là, il vaut mieux s'écarter)
      for (auto& w : warnings_)
        if (inBoardZone(w.zone, w.center, cellOf(a))) return false;
      return aimedBy(a) > 0;
    }
    if (c == "soi_dos_menace") {
      for (auto& o : liveFoes)
        if (boardDist(cellOf(o), cellOf(a)) <= 1 && behind(o, a)) return true;
      return false;
    }
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
    } else if (tc.act == "approcher" || tc.act == "contourner" || tc.act == "proteger") {
      // Échiquier (3.4) : se déplacer vers la cible de la condition ; le tour continue (tacticTurn rejoue les règles)
      if (moved_.count(a.get())) continue;
      auto ways = walkable(a);
      if (ways.empty()) continue;
      std::vector<FighterP> pool;
      for (auto& t : c->side == TSide::Foe ? liveFoes : liveAllies)
        if (t != a && holds(tc, Act{}, t)) pool.push_back(t);
      std::stable_sort(pool.begin(), pool.end(), [&](const FighterP& x, const FighterP& y) { return order(tc.cond, x) < order(tc.cond, y); });
      int cell = -1;
      for (auto& t : pool) {
        int tcell = cellOf(t), best = 1 << 20;
        for (auto& [w, steps] : ways) {
          int score = -1;
          if (tc.act == "approcher") score = boardDist(w, tcell) * 10 + steps;  // au plus près, puis le chemin le plus court
          else if (tc.act == "contourner") {
            if (boardDist(w, tcell) > reach(a) || !behind(a, t, w)) continue;
            score = steps;
          } else {  // protéger : à côté de l'allié (en croix), le plus près possible des ennemis
            if (!inBoardZone(Area::Cross, tcell, w)) continue;
            int near = 99;
            for (auto& o : liveFoes) near = std::min(near, boardSteps(cellOf(o), w));
            score = near * 10 + steps;
          }
          if (threatened(a) && isAlly(a) && dangerAt(a, w)) continue;
          if (score < best) best = score, cell = w;
        }
        if (tc.act == "approcher" && cell >= 0 && boardDist(cell, tcell) >= boardDist(cellOf(a), tcell)) cell = -1;  // pas plus près
        if (cell >= 0) break;
      }
      if (cell < 0) continue;
      out = Plan{};
      out.tactic = i, out.cell = cell;
      return true;
    } else if (tc.act == "garde") {  // se mettre en garde : attendre, face à l'ennemi le plus proche
      bool cond = false;
      if (c->side == TSide::Self) cond = holds(tc, Act{}, a);
      else
        for (auto& t : c->side == TSide::Foe ? liveFoes : liveAllies) cond = cond || holds(tc, Act{}, t);
      if (!cond) continue;
      out = Plan{};
      out.tactic = i, out.wait = true;
      return true;
    } else if (tc.act == "limite") {
      if (a->lim >= 100 && !a->S().limit.empty()) acts.push_back({&moveInfo(a->S().limit), "", true});
    } else
      for (auto& id : known) {
        const Move& m = moveInfo(id);
        if (a->mp < m.cost) continue;
        const std::string& k = tc.act;
        if ((k == "attaque" && m.damaging()) || (k == "technique" && m.damaging() && m.cost == 0) || (k == "briser" && m.damaging()) ||
            (k == "attaque_prudente" && m.damaging() && (m.cost == 0 || a->mp - m.cost >= .4f * a->mmp)) ||
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
        if (tc.act == "briser" && !(x.m && G.weakKnown(t->sp, x.m->type) && moveEff(*x.m, t->S()) > 1)) continue;  // une faiblesse connue
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
  if (postureMult(d, m) <= 0) {  // (2026-10-25) voilé de brume : sorts et tirs le traversent
    pop(d, T("Traverse la brume"), rgb(0xc8b8ff));
    sound("rate");
    log.push_back(a->name() + " > " + d->name() + " : " + m.name + T(" traverse la brume"));
    if (isAlly(a)) tip(veilType(*d).empty() ? "voile" : "voile_type");  // (Codex, 2026-10-26) un voile d'un type : pas « allez au contact »
    return 0;
  }
  // Précision : l'esquive ne joue que contre les attaques physiques
  float hit = m.acc / 100.f * a->acc / 100.f - (magic ? 0 : d->eva / 100.f);
  if (!magic && !engaged(d)) hit -= float(talentValue(d->S(), "esquive_arriere"));  // talent Furtif : loin de la mêlée
  if (!magic && weather_ >= 0) hit += R.weathers[(size_t)weather_].acc / 100.f;                            // brume, blizzard
  if (const Rules::Terrain* tr = terrainAt(d); !magic && tr && tr->kind == "esquive") hit -= float(tr->value) / 100.f;  // herbes hautes
  if (frand() >= hit) {
    pop(d, T("Raté"), GREY);
    sound("rate");
    log.push_back(a->name() + " > " + d->name() + " : " + m.name + T(" ratée"));
    return -1;
  }
  float eff = moveEff(m, d->S());
  bool absorb = eff < 0;  // (2026-10-23) il absorbe ce type (résistance négative) : le coup le soigne
  if (absorb) eff = -eff;
  if (isAlly(a) && eff <= 1 && !absorb && m.damaging()) {  // (2026-10-25) une résistance se découvre aussi en frappant
    G.weak.insert(d->sp + ":" + std::to_string(m.type));
    G.weak.insert(d->sp + (magic ? ":mag" : ":phys"));
  }
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
  float place = posMult(a, d, m);  // échiquier : dans le dos, encerclé, distance au contact
  k *= place;
  bool back = m.melee && isAlly(a) != isAlly(d) && behind(a, d);
  k *= mult;
  if (d->broken) k *= float(R.breakDamage);  // Brisure : un ennemi brisé encaisse plus
  k *= 1 - float(talentValue(d->S(), magic ? "resiste_magie" : "resiste_physique"));  // talents Carapace, Écorce runique
  if (hasType(a->S(), m.type) && a->hp * 3 < a->mhp) k *= 1 + float(talentValue(a->S(), "rage"));  // talent Rage
  k *= weatherMult(m.type);                                                                          // météo
  k *= guardMult(d);                                                                                 // rôle : protecteur voisin
  if (guarding_.count(d.get())) k *= float(R.guard);                                                 // en garde (Attendre)
  k *= postureMult(d, m);                                                                            // cuirasse d'un boss
  if (const Rules::Terrain* tr = terrainAt(a); magic && tr && tr->kind == "magie") k *= 1 + float(tr->value);  // cristaux
  int dmg = std::max(1, int(((2 * a->lvl / 5.f + 2) * m.power * A / std::max(1.f, D) / float(R.dmgDivisor) + 2) * stab * eff * spread * k));
  if (absorb) {  // soigné au lieu d'être blessé ; l'équipe s'en souvient (aperçu des dégâts, Bestiaire)
    int h = std::min(dmg, d->mhp - d->hp);
    d->hp += h;
    pop(d, "+" + std::to_string(h), GREEN);
    pop(d, T("Absorbe !"), rgb(0x7dffa8));
    sound("soin");
    if (isAlly(a)) G.weak.insert(d->sp + ":" + std::to_string(m.type)), tip("absorbe");
    log.push_back(a->name() + " > " + d->name() + " : " + m.name + T(" absorbé (+") + std::to_string(h) + T(" PV)"));
    return 0;
  }
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
  // (2026-10-26) critique : doré et en grand ; faiblesse : orange ; résistance : gris-bleu ; bleu : encaissé en garde
  pop(d, std::to_string(dmg), crit ? GOLD : eff > 1 ? WEAK : guarding_.count(d.get()) ? rgb(0x8fd0ff) : eff < 1 ? RESIST : WHITE,
      PopDamage | (crit ? PopCrit : 0) | (eff > 1 ? PopWeak : eff < 1 ? PopResist : 0) | (d->alive() ? 0 : PopKO));
  if (guarding_.count(d.get()) && shown()) bursts_.push_back({d, "garde", G.time});  // (2026-10-25) et l'éclat du bouclier
  if (guarding_.count(d.get()) && isAlly(d)) sound("bouclier");                      // qu'un allié en garde encaisse s'entend
  if (crit) pop(d, T("Critique !"), RED), shakeT_ = G.time;
  if (back) {
    pop(d, T("Dans le dos !"), rgb(0xffb070));
    if (isAlly(a)) tip("dos");
  }
  if (eff > 1) pop(d, T("Efficace !"), WEAK);
  else if (eff < 1) pop(d, T("Résiste"), RESIST);
  if (isAlly(d) || duel()) d->lim = std::min(100.f, d->lim + dmg * float(R.limitGain) / d->mhp);  // en duel, les deux équipes
  if (double steal = gemSupport(*a, "vol_vie"); steal > 0 && a->alive() && a->hp < a->mhp) {  // gemme de vol de vie
    int h = std::max(1, int(dmg * steal));
    a->hp = std::min(a->mhp, a->hp + h);
    pop(a, "+" + std::to_string(h), GREEN);
  }
  // Riposte : gemme de riposte, ou (2026-10-24) en garde (« Attendre ») : un coup au corps à corps lui en vaut un en retour
  if (m.melee && a->alive() && d->alive() && !countering_ && !d->techs().empty() &&
      frand() < gemSupport(*d, "contre") + (guarding_.count(d.get()) ? R.guardRiposte : 0)) {
    std::string counter;  // sa première technique qui blesse, une cible de préférence (la toute première peut être un cri)
    for (auto& id : d->techs())
      if (const Move& t = moveInfo(id); t.damaging() && (counter.empty() || (t.target == Target::Enemy && moveInfo(counter).target != Target::Enemy)))
        counter = id;
    if (!counter.empty()) {
      const Move& c = moveInfo(counter);
      countering_ = true;
      pop(d, T("Riposte !"), rgb(0xffd34d));
      sound("riposte");
      if (shown()) bursts_.push_back({d, "riposte", G.time});  // (2026-10-25) un court élan vers l'attaquant
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
  log.push_back(a->name() + " > " + d->name() + " : " + m.name + " " + std::to_string(dmg) + (crit ? T(" critique") : "") + (back ? T(" dans le dos") : "") +
                (place < .99f ? T(" au contact") : place > 1.01f && !back ? T(" encerclé") : "") +
                (eff > 1 ? T(" efficace") : eff < 1 ? T(" résiste") : "") + T(" (reste ") + std::to_string(d->hp) + "/" + std::to_string(d->mhp) + ")");
  return dmg;
}

// ---------------------------------------------------------------------------
// Pousser / tirer, météo et terrain (2.6)
// ---------------------------------------------------------------------------
// Pousser éloigne la cible du lanceur dans l'axe principal qui les sépare (de côté, ou de haut en bas s'il est
// au-dessus ou en dessous d'elle), tirer la rapproche. Le plateau entier compte : on peut pousser un ennemi contre un
// des siens, ou l'attirer chez soi.
int Battle::shoveDest(const FighterP& a, const FighterP& t, const Move& m) const {
  if (t->boss) return -2;
  int c = cellOf(t), ac = cellOf(a);
  if (c < 0 || ac < 0) return -2;
  int dx = boardX(c) - boardX(ac), dy = boardY(c) - boardY(ac), x = boardX(c), y = boardY(c);
  if (std::abs(dy) > std::abs(dx)) y += (dy > 0 ? 1 : -1) * m.shove;
  else x += (dx > 0 ? 1 : dx < 0 ? -1 : isAlly(t) ? -1 : 1) * m.shove;  // même case (impossible) : vers son camp
  if (x < 0 || x >= BOARD_W || y < 0 || y >= GRID) return -1;
  int to = boardAt(y, x);
  if (to == ac) return -2;  // tiré contre le lanceur : il reste où il est
  return to;
}

void Battle::shove(const FighterP& a, const FighterP& t, const Move& m, int dmg, std::vector<FighterP>& ko) {
  if (isAlly(a)) tip("pousser");
  shoveTo(t, shoveDest(a, t, m), m.shove > 0, dmg, ko);
}
void Battle::shoveTo(const FighterP& t, int to, bool push, int dmg, std::vector<FighterP>& ko) {
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
    if (push) {
      hurt(t);
      pop(t, T("Choc !"), rgb(0xffb070));
      shakeT_ = G.time;
      log.push_back("  " + t->name() + T(" poussé contre le bord : choc ") + std::to_string(clash));
    }
    return;
  }
  if (blockedCell(to)) {  // contre un obstacle : choc, comme contre le bord
    hurt(t);
    pop(t, T("Choc !"), rgb(0xffb070));
    shakeT_ = G.time;
    log.push_back("  " + t->name() + T(" heurte un obstacle : choc ") + std::to_string(clash));
    return;
  }
  FighterP occ = at(to);
  if (occ) {  // case prise (par un allié ou un adversaire) : les deux se heurtent
    hurt(t), hurt(occ);
    pop(t, T("Choc !"), rgb(0xffb070));
    shakeT_ = G.time;
    log.push_back("  " + t->name() + T(" heurte ") + occ->name() + T(" : choc ") + std::to_string(clash));
    return;
  }
  if (shown()) slide_[t.get()] = {pos(t), G.time};
  cells_[t.get()] = to;
  pop(t, push ? T("Recule") : T("Avance"), rgb(0xd8e0ff));
  log.push_back("  " + t->name() + (push ? T(" poussé : ") : T(" tiré : ")) + boardName(to));
  if (terrainTurn(t, true) && std::find(ko.begin(), ko.end(), t) == ko.end()) ko.push_back(t);
}


const Rules::Terrain* Battle::terrainOf(int cell) const {
  if (cell < 0 || cell >= BOARD_CELLS) return nullptr;
  int i = terrain_[(size_t)cell];
  return i >= 0 && i < (int)rules().terrains.size() ? &rules().terrains[(size_t)i] : nullptr;
}
const Rules::Terrain* Battle::terrainAt(const FighterP& f) const { return terrainOf(cellOf(f)); }
bool Battle::terrainHits(const FighterP& f, const Rules::Terrain* t) const {
  if (!t) return false;
  for (auto& ty : t->immune)
    for (size_t k = 0; k < types().size(); k++)
      if (types()[k].id == ty && hasType(f->S(), (int)k)) return false;
  return true;
}
bool Battle::terrainHurts(const FighterP& f, int cell) const {
  const Rules::Terrain* t = terrainOf(cell);
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
      int cell = freeCellNear(false, -1);  // dans le camp des ennemis, le plus près de l'avant
      if (cell < 0) break;
      FighterP n = makeFoe(r, std::max(1, f->lvl - 4 - foeLevelBonus));  // le niveau du boss compte déjà le bonus (Nouvelle partie +)
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
    if (o != d && o->alive() && o->S().combatRole == "protecteur" && inBoardZone(Area::Cross, cellOf(o), cellOf(d))) return float(1 - rules().roleGuard);
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
  intents_.erase(d.get());
  for (auto it = warnings_.begin(); it != warnings_.end();)
    if (it->who == d) it = warnings_.erase(it);
    else ++it;
  pop(d, T("BRISÉ !"), GOLD, PopBreak);
  bursts_.push_back({d, "brisure", G.time});
  sound("brisure");
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
  koT_[f.get()] = G.time;  // dessin : l'ennemi s'efface (3.3)
}

void Battle::useMove(FighterP a, const std::string& id, std::vector<FighterP> targets, bool isLimit, float boost, int charge) {
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
  // Échiquier : au corps à corps, il rejoint d'abord sa cible (s'il ne s'est pas encore déplacé)
  if (boost == 1 && !isLimit && m.melee && m.zone != Area::All && m.target == Target::Enemy && targets.size() == 1 && targets[0]) {
    const FighterP& t = targets[0];
    if (boardDist(cellOf(a), cellOf(t)) > reach(a)) {
      int c = moved_.count(a.get()) ? -1 : approachCell(a, t);
      if (c < 0) {
        log.push_back(a->name() + " : " + m.name + T(", cible hors d'atteinte"));
        sc.say(a->name() + T(" ne peut pas atteindre ") + t->name() + ".", .6f);
        sc.call([this, a] { afterAction(a, float(rules().quickGauge)); });
        return;
      }
      step(a, c);
    }
  }
  if (!targets.empty() && targets[0] && targets[0] != a) face(a, cellOf(targets[0]));  // il se tourne vers sa cible
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
  sc.call([this, a, targets, center, &m, isLimit, ko, boost, duo, charge] {
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
        if (at(cellOf(t))) cells_[t.get()] = freeCellNear(isAlly(t), cellOf(t));  // sa case a été prise entre-temps
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
        if (hostile && postureMult(t, m) <= 0) {  // (2026-10-25) voilé de brume : l'état le traverse aussi
          pop(t, T("Traverse la brume"), rgb(0xc8b8ff));
          continue;
        }
        if (hostile && frand() >= m.acc / 100.f * a->acc / 100.f) {
          pop(t, T("Raté"), GREY);
          continue;
        }
        for (auto& e : m.effects)
          if (!e.self) applyEffect(t, e);
        landed = true;
      } else {
        int dmg = applyHit(a, t, m, (center && t != center ? float(rules().zoneSide) : 1.f) * boost);
        if (dmg <= 0) continue;  // raté, sans effet ou absorbé : ni état, ni poussée, ni bonus pour le lanceur (relecture de Codex)
        landed = true;
        if (!t->alive()) ko->push_back(t);
        else {
          for (auto& e : m.effects)
            if (!e.self) applyEffect(t, e);
          if (m.shove && t == targets[0] && dmg > 0 && !charge) shove(a, t, m, dmg, *ko);  // (la charge déplace déjà : pas deux fois)
          if (charge > 0) {  // (2026-10-25) charge d'un boss : chaque cible recule d'une case, loin de lui (choc contre le bord ou un voisin)
            int c = cellOf(t), x = boardX(c) + (boardX(c) >= boardX(cellOf(a)) ? 1 : -1);
            shoveTo(t, x < 0 || x >= BOARD_W ? -1 : boardAt(boardY(c), x), true, dmg, *ko);
          } else if (charge < 0) {  // (2026-10-26) attire : tirée vers lui jusqu'à la ligne de front de son camp ; un combattant devant l'arrête
            int c = cellOf(t), y = boardY(c), d = boardX(cellOf(a)) > boardX(c) ? 1 : -1, to = -1;
            for (int x = boardX(c) + d; x >= 0 && x < BOARD_W && inHalf(isAlly(t), boardAt(y, x)) && !at(boardAt(y, x)) && !blockedCell(boardAt(y, x)); x += d)
              to = boardAt(y, x);
            if (to >= 0) shoveTo(t, to, false, dmg, *ko);
          }
        }
        if (!m.motion.empty() && t == targets[0] && a->alive()) motionAfterHit(a, t, m, *ko);
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
  if (t && t != a) face(a, cellOf(t));  // vers celui qu'il soigne ou capture
  const ItemDef& d = item(id);
  startAnim(a, {t}, d.capture > 0 ? Fx::Capture : Fx::Heal, d.capture > 0 ? rgb(0xffe27a) : rgb(0x7dffa8));
  sc.say(a->name() + T(" utilise : ") + d.name, .7f);
  sc.call([this, a, id, t] {
    const ItemDef& d = item(id);
    sound(d.capture > 0 ? "lancer" : "soin");
    if (d.revive) {
      t->clearBattle();
      if (at(cellOf(t))) cells_[t.get()] = freeCellNear(isAlly(t), cellOf(t));  // sa case a été prise entre-temps
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
        t->boss = false;  // un alpha capturé redevient une créature comme les autres (le statut n'est pas sauvegardé)
        t->shield = t->shieldMax = 0;
        t->hp = t->mhp;
        t->mp = t->mmp;
        t->lim = 0;
        t->atb = 0;
        bool full = (int)G.team.size() >= rules().maxTeam;  // équipe complète : elle part dans la réserve
        (full ? G.reserve : G.team).push_back(t);
        G.caught.insert(t->sp);
        sc.call([this] { sound("capture"); });
        sc.say(T("Capturé ! ") + t->name() + (full ? T(" est envoyé dans la réserve (Échap > Préparation > Réserve).") : T(" rejoint l'équipe.")), 1.5f);
      } else {
        sc.call([this] { sound("capture_rate"); });
        sc.say(t->name() + T(" s'échappe de la lumière !"), 1.0f);
      }
    }
  });
  sc.wait(.4f);
  sc.call([this, a] { afterAction(a); });
}




void Battle::swapIn(FighterP a, FighterP r) {
  G.menus.clear();
  cells_[r.get()] = cellOf(a);  // le remplaçant prend sa case
  for (auto& x : allies)
    if (x == a) x = r;
  auto ia = std::find(G.team.begin(), G.team.end(), a), ib = std::find(G.team.begin(), G.team.end(), r);
  std::iter_swap(ia, ib);
  a->stage.fill(0);  // les bonus et malus ne suivent pas celui qui recule
  moved_.erase(a.get()), moved_.erase(r.get());
  moveFrom_.erase(a.get()), moveFrom_.erase(r.get());
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
  moved_.erase(a.get());  // son prochain tour : il pourra de nouveau se déplacer
  moveFrom_.erase(a.get());
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
        if (o != a && o->alive() && o->hp < o->mhp && inBoardZone(Area::Cross, cellOf(a), cellOf(o))) {
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
      // le renfort entre dans le camp des ennemis, près de la ligne de l'ennemi tombé
      int dead = cellOf(foes[i]);
      cells_[n.get()] = freeCellNear(false, dead >= 0 && inHalf(false, dead) ? dead : boardAt(dead >= 0 ? boardY(dead) : 1, GRID));
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
  if (training_) {  // entraînement : ni expérience, ni or, ni objet
    sound("victoire", false);
    sc.wait(.6f);
    sc.call([this] {
      summary_ = gainNotes_ = {T("Entraînement réussi !")};
      summaryOn_ = true;
      summaryT_ = G.time;
      sc.halt();
    });
    sc.call([this] { finish(BattleResult::Win); });
    return;
  }
  const Rules& R = rules();
  int total = 0, goldGain = 0, sumLvl = 0;
  for (auto& a : allies) sumLvl += a->lvl;
  float teamLvl = allies.empty() ? 0.f : float(sumLvl) / allies.size();
  for (auto& d : defeated) {
    int lv = d->rewardLvl > 0 ? d->rewardLvl : d->lvl;  // relevée au niveau de l'équipe : les gains de sa zone (sinon l'équipe file devant)
    // trop faible pour l'équipe : moins d'expérience (l'équipe ne file plus loin devant l'histoire)
    float gap = teamLvl - lv - R.xpGapFree;
    float k = R.xpGapLoss > 0 && gap > 0 && !G.inExpedition() ? std::max(float(R.xpGapMin), 1 - gap * float(R.xpGapLoss)) : 1.f;  // pas en Expédition
    total += int(lv * R.xpPerLevel * (d->boss ? R.xpBoss : 1) * k + .5f);
    goldGain += lv * R.goldPerLevel * (d->boss ? R.goldBoss : 1);
  }
  sound("victoire", false);  // chacun joue la fin chez lui
  sc.wait(.6f);
  int share = int(total * R.xpFront + 1e-6), res = int(total * R.xpReserve + 1e-6);
  // Tous les gains d'un coup, dans une fenêtre (Battle::update la ferme sur Entrée)
  sc.call([this, share, res, goldGain] {
    const Rules& R = rules();
    summary_.clear();
    gainRows_.clear(), gainNotes_.clear(), loot_.clear();
    goldWon_ = goldGain;
    auto level = [](const Fighter& f) { return f.lvl + (f.need() > 0 ? std::min(.999f, float(f.xp) / f.need()) : 0.f); };
    auto found = [this](const std::string& id) {  // un objet de plus au butin dessiné (le même objet : un seul ×n)
      for (auto& [i, n] : loot_)
        if (i == id) return void(n++);
      loot_.push_back({id, 1});
    };
    bool up = false;
    if (share > 0) {
      for (auto& a : alive(allies)) {
        if (!mineToCommand(a)) continue;  // gardien à plusieurs : chacun ne fait progresser que ses combattants
        int before = a->lvl;
        float from = level(*a);
        auto msgs = gainXp(*a, share);
        GainRow row{a.get(), share, from, level(*a), {}};
        for (int l = before + 1; l <= a->lvl; l++)
          for (auto& ln : a->S().learn)
            if (ln.lvl == l) row.learned.push_back({l, moveInfo(ln.move).name});
        gainRows_.push_back(row);
        summary_.push_back(a->name() + " : +" + std::to_string(share) + T(" exp") + (a->lvl > before ? T(" · niveau ") + std::to_string(a->lvl) + " !" : ""));
        for (auto& m : msgs)
          if (m.find(T(" apprend ")) != std::string::npos) summary_.push_back("   " + m);
        up = up || a->lvl > before;
      }
      bool others = false;
      for (auto& m : G.team)
        if (!isAlly(m) && m->alive()) gainXp(*m, res), others = true;
      if (others && res > 0) summary_.push_back(T("Le reste de l'équipe : +") + std::to_string(res) + T(" exp")), gainNotes_.push_back(summary_.back());
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
          found(id);
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
              found(td.item);
              summary_.push_back(T("Objet trouvé : ") + item(td.item).name);
            }
    }
    if (up) sound("niveau", false);
    for (auto& l : summary_) log.push_back("  " + l);
    summaryOn_ = true;
    summaryT_ = G.time;
    fillT_ = G.time + .3f;  // la fenêtre apparaît, puis les barres se remplissent
    sc.halt();
  });
  sc.call([this] { finish(BattleResult::Win); });
}

float Battle::gainsFill(float t) const {
  float k = std::clamp(t - fillT_, 0.f, 1.f);  // une seconde, de plus en plus lentement
  return 1 - (1 - k) * (1 - k) * (1 - k);
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
  bool side = false;  // camp visé (vrai : alliés), pour une zone « tout le camp »
  // Menu de commande d'un allié (3.4) : ses cases possibles pour ce tour, en bleu léger (portée de déplacement)
  if (!picking && actor && isAlly(actor) && G.menus.active() && G.menus.depth() == 1 && !moved_.count(actor.get()) && !follower())
    for (auto& [c, d] : walkable(actor)) tile(g, cellPos(c), rgb(0x8fd0ff, uint8_t(46 + 18 * pulse)));
  if (picking) {
    Menu& m = G.menus.top();
    for (size_t i = 0; i < pick_.cells.size() && i < m.items.size(); i++)
      if ((int)i != m.sel && m.items[i].enabled) tile(g, cellPos(pick_.cells[i]), rgb(0xffffff, uint8_t(90 + 60 * pulse)), true);
    if (m.sel >= 0 && m.sel < (int)pick_.cells.size()) center = pick_.cells[(size_t)m.sel];
    if (!pick_.move) mv = aiming_;
    if (pick_.move && actor)  // cases où des ennemis pourront frapper au prochain tour : un petit losange rouge par ennemi
      for (size_t i = 0; i < pick_.cells.size(); i++) {
        int n = std::min(3, meleeThreat(actor, pick_.cells[i]));
        Pt p = cellPos(pick_.cells[i]);
        for (int k = 0; k < n; k++) {
          float x = p.x - (n - 1) * 3.f + k * 6, y = p.y + 13;
          g.poly({{x, y - 2.5f}, {x + 2.5f, y}, {x, y + 2.5f}, {x - 2.5f, y}}, rgb(0xff4a3a, 220));
        }
        if (backReach(actor, pick_.cells[i])) {  // (2026-10-23) d'ici, un ennemi au contact est frappé dans le dos : une dague dorée
          float x = p.x - 12, y = p.y + 11;  // au coin gauche de la case (le centre porte l'aperçu du combattant)
          g.line(x - 3, y + 3, x + 3, y - 3, 2, rgb(0x2a1606, 200));
          g.line(x - 3, y + 3, x + 3, y - 3, 1, rgb(0xffe066));
          g.line(x - 3, y - 1, x + 1, y + 3, 1, rgb(0xffb04a));  // la garde
        }
      }
    if (pick_.move && center >= 0 && actor) {  // le chemin jusqu'à la case : des points le long des pas
      auto path = pathTo(actor, center);
      for (size_t i = 1; i < path.size(); i++) {
        Pt p0 = cellPos(path[i - 1]), p1 = cellPos(path[i]);
        for (int k = 1; k <= 3; k++) {
          float u = k / 4.f;
          g.ellipse(p0.x + (p1.x - p0.x) * u, p0.y + 13 + (p1.y - p0.y) * u, 1.5f, 1.2f, rgb(0xfff2a0, uint8_t(170 + 60 * pulse)));
        }
      }
    }
    if (pick_.move && center >= 0 && actor) {  // le combattant en transparence sur la case choisie
      Pt p = cellPos(center);
      g.alpha = .45f;
      drawFighterSprite(g, *actor, p.x, p.y, 1, boardX(center) < BOARD_W - 1, t);
      g.alpha = 1;
    }
    if (mv && mv->melee && center >= 0 && actor && boardDist(cellOf(actor), center) > reach(actor)) {
      int from = approachCell(actor, at(center));  // corps à corps : la case d'où il frappera
      if (from >= 0) {
        Pt p = cellPos(from);
        g.alpha = .35f;
        drawFighterSprite(g, *actor, p.x, p.y, 1, boardX(from) < boardX(center), t);
        g.alpha = 1;
        tile(g, p, rgb(0x8fd0ff, uint8_t(120 + 60 * pulse)), true);
      }
    }
  } else if (!preview_.empty() && previewDepth_ == G.menus.depth() && G.menus.active() && actor) {
    mv = &moveInfo(preview_);
    if (mv->target == Target::Enemy) {
      if (cursor && cursor->alive()) center = cellOf(cursor);
      // corps à corps : les ennemis hors d'atteinte sont barrés
      for (auto& f : alive(foes))
        if (!reachable(actor, *mv, f)) {
          Pt p = cellPos(cellOf(f));
          g.line(p.x - 6, p.y + 10, p.x + 6, p.y + 16, 2, rgb(0xff5a4a, 200));
          g.line(p.x + 6, p.y + 10, p.x - 6, p.y + 16, 2, rgb(0xff5a4a, 200));
        }
    } else if (mv->target == Target::Ally || mv->target == Target::AllyKO) {
      side = true;
      center = cellOf(actor);
    } else side = mv->target == Target::AllAllies;
  }
  if (!mv && center >= 0) {  // objet, allié, case : seulement la case choisie
    tile(g, cellPos(center), rgb(0xffd34d, uint8_t(150 + 80 * pulse)));
    return;
  }
  if (!mv) return;
  bool all = mv->zone == Area::All || mv->target == Target::AllEnemies || mv->target == Target::AllAllies;
  if (all) {  // tout un camp : les cases de ses combattants
    for (auto& f : alive(side ? allies : foes)) tile(g, cellPos(cellOf(f)), rgb(0xff9a3a, uint8_t(90 + 50 * pulse)));
    return;
  }
  for (int c = 0; c < BOARD_CELLS; c++) {
    bool main = c == center, in = center >= 0 && inBoardZone(mv->zone, center, c);
    if (!in) continue;
    tile(g, cellPos(c), main ? rgb(0xffd34d, uint8_t(150 + 80 * pulse)) : rgb(0xff9a3a, uint8_t(90 + 50 * pulse)));
    if (main) tile(g, cellPos(c), rgb(0xfff6c8, 230), true);
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
  // (2026-10-21) le panneau dans le style des fenêtres : dégradé bleu nuit, double filet clair en haut
  g.gradV(hx - 4, hy - 13, g.right() - hx + 4, SCREEN_H - hy + 13, rgb(0x22348a, 240), rgb(0x0a1040, 245));
  g.rect(hx - 4, hy - 13, g.right() - hx + 4, 1, rgb(0xd8def2, 170));
  g.rect(hx - 4, hy - 12, g.right() - hx + 4, 1, rgb(0x5a6aa8, 120));
  g.rect(hx - 4, hy - 13, 1, SCREEN_H - hy + 13, rgb(0xd8def2, 120));
  // une jauge : cadre sombre, remplissage avec un liseré clair en haut et plus sombre en bas
  auto gauge = [&](float x, float y, float w, float k, Color c) {
    g.rect(x - 1, y - 1, w + 2, 6, rgb(0x03051a));
    g.rect(x, y, w, 4, rgb(0x1a2050));
    float f = std::round(w * std::clamp(k, 0.f, 1.f));
    if (f <= 0) return;
    g.rect(x, y, f, 4, c);
    g.rect(x, y, f, 1, shade(c, -.35f));
    g.rect(x, y + 3, f, 1, shade(c, .25f));
  };
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
    if (a->alive() && guarding_.count(a.get()) && !ending_) {  // (2026-10-24) en garde : un petit bouclier (plus après la fin)
      float gx = hx + 56, gy = y;
      g.poly({{gx - 3, gy}, {gx + 3, gy}, {gx + 3, gy + 3}, {gx, gy + 6}, {gx - 3, gy + 3}}, rgb(0x8fd0ff));
      g.poly({{gx - 1.5f, gy + 1}, {gx + 1.5f, gy + 1}, {gx + 1.5f, gy + 3}, {gx, gy + 4.5f}, {gx - 1.5f, gy + 3}}, rgb(0x1c2a78));
    }
    if (a->alive() && aimedBy(a) > 0) {  // visé par une intention : le chevron orange, comme au-dessus de sa tête
      float cx = hx + 65, cy = y + 1;
      g.tri(cx - 3, cy, cx + 3, cy, cx, cy + 4, rgb(0xff9a3a));
    }
    float r = a->mhp ? float(a->hp) / a->mhp : 0;
    Color hc = r > .5f ? GREEN : r > .25f ? GOLD : RED;
    g.text(hx + 92, y - 2, std::to_string(std::max(0, a->hp)), a->alive() ? (r <= .25f ? hc : WHITE) : rgb(0xff7b6b), 2);
    gauge(hx + 95, y + 2, 34, r, hc);
    g.text(hx + 152, y - 2, std::to_string(a->mp), rgb(0x9fd0ff), 2);
    // Jauge ATB
    float bx = hx + 158, bw = 34;
    if (a->alive() && a->atb < 0) gauge(bx, y + 2, bw, -a->atb / 100, RED);
    else gauge(bx, y + 2, bw, a->alive() ? a->atb / 100 : 0, a->atb >= 100 ? GOLD : rgb(0xe8ecff));
    // Limite
    float lx = hx + 196, lw = 14, lr = std::clamp(a->lim / 100, 0.f, 1.f);
    gauge(lx, y + 2, lw, lr, lr >= 1 && int(t * 5) % 2 ? WHITE : rgb(0xff6fb0));
  }

}

void Battle::drawStatusTag(const Fighter& f, float x, float y) {
  if (f.status == Status::None || !f.alive()) return;
  Gfx& g = G.g;
  Color c = rgb(statusColor(f.status));
  x = std::clamp(x, 13.f, 307.f);  // l'allié du fond, tout à gauche : l'étiquette reste dans l'écran
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
  // la carte du lieu, quand le combat s'y passe (pas un décor imposé d'un autre thème)
  const MapDef* hm = G.mapId >= 0 && G.mapId < (int)maps().size() && G.M().theme == bg ? &G.M() : nullptr;
  auto share = [&](char ch) {  // (2026-10-25) part des cases `ch` sur cette carte : lave, glace, eau
    int n = 0, tot = 0;
    if (hm)
      for (auto& r : hm->rows)
        for (char c : r) n += c == ch, tot++;
    return tot ? float(n) / tot : 0.f;
  };
  auto hs = [](int i, int k) {  // hasard fixe (0 à 1) de l'élément i du décor
    uint32_t h = uint32_t(i) * 0x9E3779B1u ^ uint32_t(k + 1) * 0x85EBCA77u;
    h ^= h >> 16, h *= 0x85EBCA6Bu, h ^= h >> 13, h *= 0xC2B2AE35u, h ^= h >> 16;
    return (h & 1023) / 1023.f;
  };
  auto cap = [&](Pt a, Pt p, Pt b, float d, Color c) {  // neige sur le sommet p d'un relief (a, b : creux voisins), d de haut
    Pt l{p.x + (a.x - p.x) * d / (a.y - p.y), Y(p.y + d)}, r{p.x + (b.x - p.x) * d / (b.y - p.y), Y(p.y + d)};
    g.poly({l, {p.x, Y(p.y)}, r, {(p.x + r.x) / 2, Y(p.y + d * .6f)}, {p.x, Y(p.y + d)}, {(l.x + p.x) / 2, Y(p.y + d * .5f)}}, c);
  };
  // un combat dans un lieu au bord de la mer (le port, les îles) : la mer remplace les collines et les arbres du fond
  bool seaside = bg == Theme::Vallee && hm && G.coastal(*hm);
  bool ice = bg == Theme::Neige && share('i') > .08f;  // le glacier, le sanctuaire : des séracs au lieu des sapins
  bool fire = bg == Theme::Cendres && (!hm || share('l') > 0);  // de la lave sur la carte : un volcan au fond (sinon le col)
  if (bg == Theme::Vallee && !boss) {
    uint32_t lt = hm ? hm->light : 0;  // (2026-10-25) la lumière du lieu (Aube : l'aurore, Port-Écho : le violet) teinte le ciel, soleil bas
    g.gradV(L, 0, W, HZ, lt ? mix(rgb(0x8fc0db), rgb(lt), .3f) : rgb(0x8fc0db), lt ? mix(rgb(0xdfeac0), rgb(lt), .55f) : rgb(0xdfeac0));
    if (lt) g.ellipse(262, 24, 24, 15, mix(rgb(lt, 80), rgb(0xffffff, 80), .4f)), g.ellipse(262, 24, 7, 7, rgb(0xfff6e8));
    // montagnes lointaines (bleutées par la distance) et nuages qui passent lentement
    g.poly(ridge({{0, 112}, {40, 70}, {90, 96}, {150, 58}, {205, 92}, {255, 66}, {320, 98}, {320, 112}}, 84, 90), rgb(0xa9c6d8));
    g.poly(ridge({{0, 112}, {70, 88}, {120, 104}, {190, 82}, {250, 102}, {320, 90}, {320, 112}}, 96, 94), rgb(0xb8d2c4));
    for (int i = 0; i < 6 * more / 100 + 1; i++) {
      float cx = L + std::fmod(i * 97.f + t * 4, W + 140) - 70, cy = 7 + (i % 3) * 8.f;
      g.ellipse(cx, cy, 16, 4, rgb(0xffffff, 190)), g.ellipse(cx - 9, cy + 1, 9, 3, rgb(0xffffff, 170)), g.ellipse(cx + 8, cy - 2, 9, 4, rgb(0xffffff, 200));
    }
    if (seaside) {  // (2026-10-21) au bord de la mer : la mer à l'horizon, ses vagues, une voile au loin
      g.gradV(L, HZ - 16, W, 16, rgb(0x5a9ad0), rgb(0x3a78b8));
      for (int i = 0; i < 26 * more / 100; i++) {
        float x = L + std::fmod(i * 47.f + t * (6 + i % 3 * 3), W + 20) - 10, y = HZ - 14 + (i * 7) % 12;
        g.rect(x, y, 4 + i % 3 * 2, 1, rgb(0xd8ecff, uint8_t(120 + 60 * std::sin(t * 2 + i))));
      }
      for (int k = 0; lt && k < 7; k++) {  // le reflet du soleil bas sur l'eau
        float w = 3 + k * 1.5f;
        g.rect(262 - w / 2 + std::sin(t * 3 + k * 1.9f) * 2, HZ - 15 + k * 2, w, 1, rgb(0xfff2d8, uint8_t(110 + 70 * std::sin(t * 4 + k))));
      }
      float bx = L + std::fmod(t * 3 + 90, W + 60) - 30, by = HZ - 9;
      g.poly({{bx - 6, by + 2}, {bx + 6, by + 2}, {bx + 4, by + 4}, {bx - 4, by + 4}}, rgb(0x6a4a2a));  // la coque
      g.tri(bx, by - 9, bx, by + 1, bx + 6, by + 1, rgb(0xf4f0e6));                                // la voile
      g.rect(bx, by - 9, 1, 11, rgb(0x5a3a1a));
      float gx = L + std::fmod(t * 11 + 40, W + 40) - 20, gy = 14 + std::sin(t * .9f) * 4, f = std::sin(t * 9) * 2;
      g.line(gx - 4, gy - f, gx, gy, 1, rgb(0xffffff)), g.line(gx, gy, gx + 4, gy - f, 1, rgb(0xffffff));
      g.rect(L, HZ - 2, W, 2, rgb(0xe8f4ff, 160));  // l'écume du rivage
      ground(0xd8c690, 0xa8d488);  // le sable, puis l'herbe
    } else {
      if (L < 0) g.ellipse(L + 10, HZ, 70, 9, rgb(0xaed08c)), g.ellipse(R - 10, HZ, 70, 10, rgb(0xbcd89a));
      g.ellipse(70, HZ, 120, 11, rgb(0xbcd89a));
      g.ellipse(260, HZ + 1, 110, 9, rgb(0xaed08c));
      ground(0x8cba6c, 0xa8d488);
    }
    for (int i = 0; i < 22 * more / 100; i++) g.rect(L + std::fmod(i * 53 + 11.f, W), HZ + 6 + (i * 13) % 120, 8, 2, rgb(0xb6dc94));
  } else if (bg == Theme::Vallee) {  // (2026-10-25) un boss : le même pays sous un ciel d'orage, au couchant
    g.gradV(L, 0, W, HZ, rgb(0x1a1430), rgb(0x8a3e4a));
    g.ellipse(232, HZ - 6, 110, 20, rgb(0xff8a5a, 36)), g.ellipse(232, HZ - 3, 54, 9, rgb(0xffb070, 46));  // le soleil derrière l'orage
    float fl = std::fmod(t, 6.7f);  // un éclair au loin, de temps en temps
    if (fl < .3f && int(fl / .07f) % 2 == 0) {
      g.rect(L, 0, W, HZ, rgb(0xd8c8ff, 34));
      float bx = 70 + std::fmod(std::floor(t / 6.7f) * 97, 180.f), y = 10;
      for (int k = 0; k < 5; k++, y += 6) {
        float nx = bx + (hs(k, (int)(t / 6.7f)) - .5f) * 10;
        g.line(bx, y, nx, y + 6, 1, rgb(0xf4ecff)), bx = nx;
      }
    }
    for (int i = 0; i < 7 * more / 100 + 1; i++) {  // de lourds nuages, éclairés par en dessous
      float cx = L + std::fmod(i * 89.f + t * 5, W + 160) - 80, cy = 14 + (i % 3) * 7.f;
      g.ellipse(cx, cy + 3, 24, 6, rgb(0xc8707a, 120));
      g.ellipse(cx, cy, 24, 6, rgb(0x433457)), g.ellipse(cx - 13, cy + 1, 13, 5, rgb(0x3a2d4c)), g.ellipse(cx + 12, cy - 2, 14, 6, rgb(0x4c3c60));
    }
    g.poly(ridge({{0, 112}, {40, 70}, {90, 96}, {150, 58}, {205, 92}, {255, 66}, {320, 98}, {320, 112}}, 84, 90), rgb(0x3e2c4e));
    if (seaside) {  // une mer grise, des moutons d'écume
      g.gradV(L, HZ - 15, W, 15, rgb(0x34385a), rgb(0x22263e));
      for (int i = 0; i < 22 * more / 100; i++) {
        float x = L + std::fmod(i * 53.f + t * (9 + i % 3 * 4), W + 20) - 10, y = HZ - 13 + (i * 7) % 12;
        g.rect(x, y, 3 + i % 3 * 2, 1, rgb(0xc8d0f0, uint8_t(70 + 70 * std::sin(t * 2.5f + i))));
      }
      g.rect(L, HZ - 2, W, 2, rgb(0xb8c0e0, 110));
      ground(0x5a5044, 0x3c4a3c);
    } else {
      g.poly(ridge({{0, 112}, {70, 88}, {120, 104}, {190, 82}, {250, 102}, {320, 90}, {320, 112}}, 96, 94), rgb(0x2e2440));
      ground(0x26302e, 0x3a4c40);
    }
    for (int i = 0; i < 8 * more / 100; i++) g.ellipse(L + std::fmod(i * 70 + t * 12, W + 100) - 50, HZ + 4 + (i % 8) * 14, 60, 6, rgb(0xc8c0e8, 18));
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
  } else if (bg == Theme::Neige) {  // (2026-10-25) deux rangs de cimes enneigées, séracs sur la glace, aurore sur un boss
    g.gradV(L, 0, W, HZ, boss ? rgb(0x101a34) : rgb(0x9ab8d8), boss ? rgb(0x4a6a9c) : rgb(0xe6eef8));
    if (boss) {
      for (int i = 0; i < 24 * more / 100; i++)  // étoiles
        g.rect(L + hs(i, 1) * W, hs(i, 2) * (HZ - 14), 1, 1, rgb(0xffffff, uint8_t(130 + 90 * std::sin(t * 2 + i * 1.7f))));
      for (int b = 0; b < 2; b++)  // aurore : deux rubans qui ondulent
        for (float x = L; x < R; x += 2) {
          float y0 = 8 + b * 8 + 5 * std::sin(x * .021f + t * .35f + b * 2), h = 10 + 5 * std::sin(x * .047f - t * .5f + b);
          Color c = rgb(b ? 0x9a7aff : 0x6affc0, uint8_t(26 + 18 * std::sin(x * .03f + t + b * 3)));
          g.rect(x, y0, 2, h, c), g.rect(x, y0 + h * .5f, 2, h * .5f, c);
        }
    }
    std::vector<Pt> far{{0, 112}, {30, 70}, {75, 92}, {120, 52}, {185, 88}, {235, 46}, {290, 84}, {320, 70}, {320, 112}};
    g.poly(ridge(far, 76, 72), boss ? rgb(0x5a6c94) : rgb(0xdbe4f0));
    for (size_t k = 1; k + 2 < far.size(); k += 2) cap(far[k - 1], far[k], far[k + 1], 12, boss ? rgb(0x8a9cc4) : rgb(0xf6f9fd));
    std::vector<Pt> nr{{0, 112}, {50, 50}, {100, 90}, {160, 30}, {220, 86}, {270, 56}, {320, 80}, {320, 112}};
    g.poly(ridge(nr, 66, 58), boss ? rgb(0x3e4c70) : rgb(0xc8d6e8));
    for (size_t k = 1; k + 1 < nr.size(); k += 2) {  // le versant à l'ombre, puis la neige du sommet
      g.tri(nr[k].x, Y(nr[k].y), nr[k + 1].x, Y(nr[k + 1].y), nr[k].x + (nr[k + 1].x - nr[k].x) * .3f, HZ, boss ? rgb(0x34405e) : rgb(0xb4c4dc));
      cap(nr[k - 1], nr[k], nr[k + 1], 14, boss ? rgb(0x7a8cb4) : rgb(0xffffff));
    }
    if (boss) ground(0x8a9ab8, 0xb8c6da);  // la neige sous la nuit
    else ground(0xd6e0ec, 0xf2f6fb);
    if (ice) {  // le glacier : une bande de glace bleutée et des séracs qui scintillent
      g.rect(L, HZ - 3, W, 3, rgb(0xa8cce8));
      for (int i = (int)std::floor(L / 19) - 1; i * 19.f < R + 10; i++) {
        float x = i * 19.f + hs(i, 3) * 8, h = 7 + hs(i, 4) * 15, w = 4 + hs(i, 5) * 4;
        g.poly({{x - w, HZ + 1}, {x - w * .4f, HZ - h * .8f}, {x, HZ - h}, {x + w * .5f, HZ - h * .7f}, {x + w, HZ + 1}}, rgb(0xa4cbe8));
        g.poly({{x - w, HZ + 1}, {x - w * .4f, HZ - h * .8f}, {x, HZ - h}, {x - w * .1f, HZ + 1}}, rgb(0xe4f4ff));
        if (std::sin(t * 1.3f + i * 2.3f) > .95f) g.rect(x - 2, HZ - h + 2, 5, 1, rgb(0xffffff)), g.rect(x, HZ - h, 1, 5, rgb(0xffffff));
      }
    }
    for (int i = 0; i < 30 * more / 100; i++) {  // flocons ; un sur quatre plus gros (plus proche), qui tombe plus vite
      int s = 1 + (i % 4 == 0);
      g.rect(L + std::fmod(i * 41 + t * (5 + s * 3) + std::sin(t * .9f + i) * 5 + 10, W), std::fmod(i * 23 + t * (12 + s * 9), 180.f), s, s, rgb(0xffffff, 220));
    }
  } else if (bg == Theme::Cendres) {  // (2026-10-25) un volcan qui fume et des fissures de lave si la carte a de la lave ; sinon (le col) le vent
    if (fire) g.gradV(L, 0, W, HZ, boss ? rgb(0x2a0e08) : rgb(0x3a2420), boss ? rgb(0xa8321e) : rgb(0x8a4a2a));
    else g.gradV(L, 0, W, HZ, boss ? rgb(0x2a2c3a) : rgb(0x5c6476), boss ? rgb(0x7a7280) : rgb(0xb4aca2));
    std::vector<Pt> far{{0, 112}, {35, 74}, {85, 94}, {130, 64}, {195, 90}, {250, 60}, {320, 86}, {320, 112}};
    if (!fire) far = {{0, 112}, {35, 50}, {85, 86}, {130, 34}, {195, 84}, {250, 28}, {320, 70}, {320, 112}};  // plus hautes : les Pics Givrés
    g.poly(ridge(far, fire ? 80 : 60, fire ? 80 : 64), fire ? rgb(0x5e3a32) : rgb(0x9a98a6));
    if (!fire)
      for (size_t k = 1; k + 1 < far.size(); k += 2) cap(far[k - 1], far[k], far[k + 1], 18, rgb(0xeef2f8));
    if (fire) {  // le volcan : panache de fumée, cratère qui rougeoie, coulée de lave
      float vx = 240, top = 26, glow = .75f + .25f * std::sin(t * 2.3f);
      for (int k = 0; k < 10; k++) {
        float ph = std::fmod(t * .1f + k / 10.f, 1.f);
        g.ellipse(vx + ph * ph * 46 + std::sin(t * .5f + k) * 2, top - 1 - ph * 30, 3 + ph * 12, 2 + ph * 7, rgb(ph < .15f ? 0xb0583a : 0x8a7672, uint8_t(150 * (1 - ph))));
      }
      g.poly({{vx - 58, HZ}, {vx - 9, top + 1}, {vx - 4, top + 3}, {vx + 4, top + 3}, {vx + 9, top}, {vx + 62, HZ}}, rgb(0x4e322c));
      g.tri(vx + 9, top, vx + 62, HZ, vx + 16, HZ, rgb(0x3e2824));
      g.ellipse(vx, top + 1, 18, 7, rgb(0xff6a2a, uint8_t(70 * glow)));
      g.ellipse(vx, top + 2, 7, 2, rgb(0xffb050));
      float x = vx - 1, y = top + 3;  // une coulée qui serpente sur le flanc
      for (int k = 0; k < 7; k++) {
        float nx = x + (k % 2 ? 3.f : -4.f) + (hs(k, 40) - .5f) * 2, ny = y + 4;
        g.line(x, y, nx, ny, 3 - k * .2f, rgb(0xd8481a, uint8_t(220 - k * 22)));
        g.line(x, y, nx, ny, 1, rgb(0xffc060, uint8_t((170 - k * 20) * glow)));
        x = nx, y = ny;
      }
    }
    g.poly(ridge({{0, 112}, {60, 60}, {110, 100}, {170, 40}, {240, 96}, {290, 70}, {320, 90}, {320, 112}}, 70, 64), fire ? rgb(0x4a3a36) : rgb(0x5e5a60));
    if (fire) {
      ground(0x5c524c, 0x7c7068);
      g.rect(L, HZ - 2, W, 2, rgb(0xd9541e));
      for (int i = (int)std::floor(L / 47) - 1; i * 47.f < R; i++) {  // fissures de lave près de l'horizon, qui pulsent
        if (hs(i, 6) < .35f) continue;
        float x = i * 47.f + hs(i, 7) * 20, y = HZ + 3 + hs(i, 8) * 9, k = .6f + .4f * std::sin(t * 1.8f + i);
        g.ellipse(x + 8, y + .5f, 12, 3, rgb(0xff5a1a, uint8_t(34 * k)));
        Pt p[4] = {{x, y}, {x + 6, y + 1.5f}, {x + 10, y - .5f}, {x + 17, y + 1}};
        for (int j = 0; j < 3; j++) g.line(p[j].x, p[j].y, p[j + 1].x, p[j + 1].y, 1, rgb(0xff6a1a, uint8_t(210 * k)));
      }
      for (int i = 0; i < 22 * more / 100; i++) {  // braises qui montent du sol
        float ph = std::fmod(t * (9 + i % 5 * 3) + i * 37.f, 190.f), x = L + std::fmod(i * 41.f + 7, W) + std::sin(t * 1.3f + i) * 4;
        int s = 1 + (i % 4 == 0);
        g.rect(x, 172 - ph, s, s, rgb(i % 3 ? 0xffb347 : 0xff6a2a, uint8_t(220 * (1 - ph / 190))));
      }
    } else {
      ground(0x6e685c, 0x8a8474);
      for (int i = 0; i < 14 * more / 100; i++) {  // les rafales des Hurlevents
        float x = R + 40 - std::fmod(t * (80 + i % 4 * 30) + i * 71.f, W + 120), y = 4 + std::fmod(i * 29.f, 96.f);
        g.rect(x, y, 8 + i % 3 * 7, 1, rgb(0xffffff, uint8_t(i % 2 ? 50 : 80)));
      }
    }
  } else {
    // (2026-10-21) la roche du donjon où l'on se bat (« teinte_murs » de la carte) colore la grotte du combat
    uint32_t rock = hm && hm->wallTint ? hm->wallTint : 0x46405a;
    auto tone = [&](float k) {
      auto c = [&](int sh) { return uint32_t(std::min(255, int(((rock >> sh) & 255) * k))); };
      return c(16) << 16 | c(8) << 8 | c(0);
    };
    bool lake = share('~') > .02f;  // l'Épave, la Grotte des Marées : un lac sous terre à l'horizon
    g.gradV(L, 0, W, HZ, rgb(tone(.28f)), rgb(tone(.65f)));
    // (2026-10-25) la profondeur de la caverne : strates de la roche, piliers au loin, voûte déchiquetée, corniche au fond
    for (int k = 0; k < 3; k++)
      for (float x = L; x < R; x += 4) g.rect(x, 20 + k * 11 + 2 * std::sin(x * .05f + k * 2), 4, 1, rgb(tone(.18f), 60));
    for (int i = (int)std::floor(L / 96) - 1; i * 96.f < R + 20; i++) {
      float x = 30 + i * 96.f + hs(i, 1) * 30, w = 5 + hs(i, 2) * 5;
      g.poly({{x - w * 1.5f, 0}, {x + w * 1.5f, 0}, {x + w, HZ * .48f}, {x + w * 1.5f, HZ}, {x - w * 1.5f, HZ}, {x - w, HZ * .5f}}, rgb(tone(.34f)));
      g.line(x - w + 1, HZ * .5f, x - w * 1.5f + 1, HZ, 1, rgb(tone(.58f)));  // le bas du pilier prend la lumière
    }
    std::vector<Pt> roof{{L - 8, 0}};
    for (int i = (int)std::floor(L / 8) - 1; i * 8.f < R + 8; i++) roof.push_back({i * 8.f, 3 + hs(i, 9) * 5});
    roof.push_back({R + 8, 0});
    g.poly(roof, rgb(tone(.22f)));
    for (int i = (int)std::floor(L / 23) - 1; i * 23.f < R; i++) {  // stalactites, et une goutte qui tombe de temps en temps
      float x = i * 23.f + hs(i, 10) * 9, len = 9 + hs(i, 11) * 16;
      g.tri(x - 4, 0, x + 4, 0, x, len, rgb(tone(.52f)));
      g.tri(x - 2, 0, x + 1, 0, x - 1, len - 3, rgb(tone(.8f)));
      float ph = std::fmod(t * .7f + hs(i, 12) * 3, 3.f);
      if (ph < 1) g.rect(x, len + ph * (HZ - len), 1, 2, rgb(0x9fd8ff, 160));
    }
    ground(tone(.82f), tone(1.25f));
    std::vector<Pt> ledge{{L - 6, HZ + 1}};  // la corniche du fond, hérissée de stalagmites
    for (int i = (int)std::floor(L / 6) - 1; i * 6.f < R + 6; i++)
      ledge.push_back({i * 6.f, HZ - 2 - hs(i, 13) * 4 - (hs(i, 14) > .78f ? 6 + hs(i, 15) * 13 : 0)});
    ledge.push_back({R + 6, HZ + 1});
    g.poly(ledge, rgb(tone(.46f)));
    for (size_t k = 1; k + 2 < ledge.size(); k++)
      if (ledge[k + 1].y < ledge[k].y) g.line(ledge[k].x, ledge[k].y, ledge[k + 1].x, ledge[k + 1].y, 1, rgb(tone(.78f), 170));  // côté éclairé
    if (lake) {
      g.gradV(L, HZ - 3, W, 9, rgb(0x163452), rgb(0x2a5878));
      g.rect(L, HZ - 3, W, 1, rgb(0x9fd8ff, 90));
      for (int i = 0; i < 18 * more / 100; i++)
        g.rect(L + std::fmod(i * 61.f + t * 3, W), HZ - 1 + (i * 5) % 6, 3 + i % 3 * 2, 1, rgb(0x9fd8ff, uint8_t(60 + 50 * std::sin(t * 2 + i))));
      for (int i = (int)std::floor(L / 23) - 1; i * 23.f < R; i++) {  // les gouttes des stalactites y font des ronds
        float ph = std::fmod(t * .7f + hs(i, 12) * 3, 3.f), x = i * 23.f + hs(i, 10) * 9;
        if (ph >= 1 && ph < 1.6f) g.ellipse(x, HZ + 1, (ph - 1) * 14, (ph - 1) * 3, rgb(0xbfe8ff, uint8_t(150 * (1.6f - ph))));
      }
    }
    if (hm && hm->light)  // des rais de lumière tombent des fissures de la voûte (Citadelle)
      for (int i = (int)std::floor(L / 110) - 1; i * 110.f < R; i++) {
        float x = 40 + i * 110.f + hs(i, 16) * 30, a = 22 + 10 * std::sin(t * .7f + i * 2);
        g.poly({{x, 0}, {x + 7, 0}, {x + 30, HZ + 12}, {x + 14, HZ + 12}}, rgb(hm->light, uint8_t(a)));
        g.ellipse(x + 22, HZ + 12, 12, 3, rgb(hm->light, uint8_t(a)));
      }
    for (int i = (int)std::floor((L - 20) / 70) - 1; 20 + i * 70.f < R + 20; i++) {  // grappes de cristaux qui luisent
      if (hs(i, 22) < .25f) continue;
      float x = 20 + i * 70.f + hs(i, 17) * 30, pulse = .5f + .5f * std::sin(t * 2 + i), sc = .65f + hs(i, 23) * .55f;
      g.ellipse(x, HZ - 4, 15 * sc, 8 * sc, rgb(0x7fd6ff, uint8_t(20 + 26 * pulse)));
      for (int k : {0, 2, 1}) {
        float cx = x + (k - 1) * 4.5f * sc, h = (k == 1 ? 15 : 8 + hs(i, 18 + k) * 5) * sc, lean = (k - 1) * 2.5f, w = k == 1 ? 3.f : 2.f, y = HZ + 1;
        Pt b0{cx - w, y}, l{cx - w + lean * .7f, y - h + 3}, tip{cx + lean, y - h}, r{cx + w + lean * .7f, y - h + 3}, b1{cx + w, y};
        g.poly({b0, l, tip, r, b1}, rgb(0x3e8ac8));
        g.poly({b0, l, tip, {cx + lean * .4f, y}}, rgb(0x7fd6ff));
        g.line(cx - w * .3f, y - 2, tip.x - .5f, tip.y + 2, 1, rgb(0xdaf6ff));
      }
      if (std::fmod(t * .9f + hs(i, 21) * 4, 4.f) < .25f) g.rect(x - 2, HZ - 14 * sc, 5, 1, rgb(0xffffff)), g.rect(x, HZ - 14 * sc - 2, 1, 5, rgb(0xffffff));
    }
    for (int i = 0; i < 12 * more / 100; i++) {  // poussière qui flotte dans l'air immobile
      float x = L + std::fmod(i * 67.f + t * 2 + std::sin(t * .4f + i) * 6 + 10, W), y = 8 + std::fmod(i * 37.f + t * 1.5f, 130.f);
      g.rect(x, y, 1, 1, rgb(tone(2.2f), uint8_t(70 + 40 * std::sin(t + i))));
    }
  }
  {  // Profondeur (2026-10-21) : bandes du sol de plus en plus larges vers l'avant (perspective), brume à l'horizon
    bool bright = (bg == Theme::Vallee && !boss) || (bg == Theme::Neige && !boss);
    const int N = 14;
    for (int i = 1; i < N; i += 2) {
      float y0 = HZ + (SCREEN_H - HZ) * std::pow(i / float(N), 1.8f), y1 = HZ + (SCREEN_H - HZ) * std::pow((i + 1) / float(N), 1.8f);
      g.rect(L, std::round(y0), W, std::round(y1) - std::round(y0), rgb(0x000000, 9));
    }
    for (int i = 0; i < 16; i++) g.rect(L, HZ - 2 + i, W, 1, rgb(bright ? 0xffffff : 0x8a7ab0, uint8_t((16 - i) * (bright ? 4 : 2))));
  }
  {  // Décor de l'horizon (3.3) : arbres, sapins ou rochers de la planche, en rang au fond et quelques-uns plus près
    std::vector<const char*> back, near;
    Color far = rgb(0xb8c4d8);  // plus pâles : la distance
    if (bg == Theme::Vallee && !boss && seaside) near = {"touffe_1", "caillou", "touffe_3"};  // pas d'arbres devant la mer
    else if (bg == Theme::Vallee && !boss) back = {"arbre", "arbre_petit", "arbre", "buisson", "arbre_automne"}, near = {"touffe_1", "fleurette_1", "touffe_3"};
    else if (bg == Theme::Vallee && !seaside) back = {"arbre", "arbre_petit", "buisson", "arbre"}, far = rgb(0x30283e);  // boss : des silhouettes sous l'orage
    else if (bg == Theme::Foret) back = {"sapin", "arbre", "sapin_petit", "sapin"}, near = {"touffe_2", "touffe_4", "fleurette_2"};
    else if (bg == Theme::Neige && !boss && !ice) back = {"sapin_neige", "sapin_neige_petit"}, near = {};
    else if (bg == Theme::Cendres && !fire) back = {"rochers", "rocher"}, near = {"touffe_3", "caillou", "cailloux"};
    else if (bg == Theme::Cendres) back = {"rochers", "rocher"}, near = {"caillou", "cailloux"};
    else if (bg == Theme::Grotte) near = {"caillou", "cailloux"};
    // les arbres de la carte (« arbres » : pierre, lueur…) se retrouvent au fond du combat
    std::string kind = hm ? hm->trees : "";
    for (int i = 0; !back.empty() && L - 30 + i * 34.f < R + 30; i++) {  // au fond, posés sur l'horizon
      if (bg == Theme::Cendres && hs(i, 30) < .45f) continue;  // des rochers épars, pas une rangée
      std::string name = back[(size_t)(i * 7 + 3) % back.size()];
      const SpriteRect* r = kind.empty() ? nullptr : findSprite(name + "_" + kind);
      if (!r) r = findSprite(name);
      if (!r) break;
      float x = L - 30 + i * 34.f + (i * 13) % 11;
      g.sprite(*r, std::round(x - r->w / 2.f), std::round(HZ + 4 - r->h + (i % 3)), i % 2, 1, far);
    }
    for (int i = 0; !near.empty() && i < 9 * more / 100; i++) {  // touffes et cailloux devant, entre les deux damiers
      const SpriteRect* r = findSprite(near[(size_t)i % near.size()]);
      if (!r) break;
      float x = L + std::fmod(i * 97.f + 23, W), y = HZ + 20 + std::fmod(i * 41.f, 110.f);
      g.sprite(*r, std::round(x), std::round(y), i % 2);
    }
  }
  if (shown() && G.mapId >= 0 && G.mapId < (int)maps().size()) {  // (2026-10-22) comme sur la carte du lieu
    const MapDef& here = G.M();
    if (here.light) g.rect(L, 0, W, SCREEN_H, rgb(here.light, uint8_t(42 * (1 - G.nightAmount()))));  // sa lumière
    if (here.water == "marais" && bg == Theme::Foret)  // des flaques troubles et des quenouilles sur le sol du combat
      for (int i = 0; i < 9 * more / 100; i++) {
        float px0 = L + std::fmod(i * 83.f + 31, W), py0 = HZ + 18 + std::fmod(i * 47.f, 120.f), w2 = 8 + i % 3 * 4;
        g.ellipse(px0, py0, w2, 3, rgb(0x3e5a4a, 200));
        g.ellipse(px0 - w2 * .3f, py0 - 1, w2 * .3f, 1, rgb(0x8aa87a, 120));
        g.line(px0 + w2, py0, px0 + w2 + std::sin(t + i) * .8f, py0 - 9, 1, rgb(0x6a8a3a));
        g.rect(px0 + w2 - .5f + std::sin(t + i) * .8f, py0 - 11, 2, 3, rgb(0x7a4a28));
      }
  }
  if (float night = shown() ? G.nightAmount() : 0.f; night > 0)  // jour et nuit (2.6), comme sur la carte
    g.rect(L, 0, W, SCREEN_H, rgb(0x0a1440, uint8_t(80 * night)));
  for (int i = 0; i < 28; i++) {  // vignette (2026-10-21) : les bords de l'écran un peu plus sombres, le regard va au centre
    Color c = rgb(0x000000, uint8_t((28 - i) * 2));
    g.rect(L + i, 0, 1, SCREEN_H, c), g.rect(R - 1 - i, 0, 1, SCREEN_H, c);
  }
  // (2026-10-26) visible d'abord, pour que l'impact (éclair, écrasement, arrêt sur image) se voie
  float blinkE = blink ? since(blink.get(), blinkT) : 1;
  bool blinking = blink && blinkE < .42f && int(blinkE / .07f) % 2 == 1;
  // Échiquier (3.4) : une dalle par case, claires et foncées en alternance ; la moitié de chaque camp légèrement
  // teintée (bleu : le nôtre, rouge : l'adverse), la ligne de front au milieu
  {
    Pt a = cellPos(boardAt(0, GRID - 1)), b = cellPos(boardAt(0, GRID)), c2 = cellPos(boardAt(GRID - 1, GRID - 1)), d2 = cellPos(boardAt(GRID - 1, GRID));
    float x0 = (a.x + b.x) / 2, y0 = a.y + 6, x1 = (c2.x + d2.x) / 2, y1 = c2.y + 20;
    for (int i = 0; i < 12; i += 2) {  // tirets
      float u = i / 12.f, v = (i + 1) / 12.f;
      g.line(x0 + (x1 - x0) * u, y0 + (y1 - y0) * u, x0 + (x1 - x0) * v, y0 + (y1 - y0) * v, 1, rgb(0xfff0c0, 110));
    }
  }
  for (int c = 0; c < BOARD_CELLS; c++) {
      int side = inHalf(true, c) ? 1 : 0;
      Pt p = cellPos(c);
      bool dark = (boardX(c) + boardY(c)) % 2;
      // une dalle en losange, vue en perspective
      g.poly({{p.x - 15, p.y + 13}, {p.x, p.y + 7.5f}, {p.x + 15, p.y + 13}, {p.x, p.y + 18.5f}}, rgb(dark ? 0x000000 : 0xffffff, dark ? 44 : 70));
      g.poly({{p.x - 10, p.y + 13}, {p.x, p.y + 9.5f}, {p.x + 10, p.y + 13}, {p.x, p.y + 16.5f}}, rgb(side ? 0x6fb4ff : 0xff7a6a, 46));
      tile(g, Pt{p.x, p.y + .5f}, rgb(0xffffff, 40), true);  // liseré : le quadrillage se lit sur tous les sols
      if (const Rules::Terrain* tr = terrainOf(c)) {  // case de terrain (2.6) : couleur et petit décor animé
        Color col = rgb(tr->color);
        col.a = 120;
        g.poly({{p.x - 14, p.y + 13}, {p.x, p.y + 8}, {p.x + 14, p.y + 13}, {p.x, p.y + 18}}, col);
        float ph = t * 3 + c * 1.3f + side * 5;
        const std::string& k = tr->look;
        if (k == "epines")  // (2026-10-25) ronces acérées de la Ronce-Mère : de grandes épines dressées
          for (int i = 0; i < 4; i++)
            g.tri(p.x - 10 + i * 6, p.y + 15, p.x - 6 + i * 6, p.y + 15, p.x - 8 + i * 6 + std::sin(ph + i) * .6f, p.y + 7 - (i % 2) * 2, rgb(0x3a1a0c, 230));
        else if (k == "foudre")  // (2026-10-26) sol foudroyé du Seigneur des Orages : de petits éclairs qui crépitent
          for (int i = 0; i < 2; i++) {
            if (std::fmod(ph * 1.7f + i * 1.3f, 2.f) > 1.2f) continue;
            float x = p.x - 5 + i * 10;
            g.line(x + 1, p.y + 8, x - 2, p.y + 12, 1, rgb(0xfff6a0));
            g.line(x - 2, p.y + 12, x + 1, p.y + 13, 1, rgb(0xfff6a0));
            g.line(x + 1, p.y + 13, x - 1, p.y + 17, 1, rgb(0xfff6a0));
          }
        else if (k == "degats")  // braises qui rougeoient
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
    for (int c = 0; c < BOARD_CELLS; c++)
      if (inBoardZone(Area(zone), center, c)) {
        Pt p = cellPos(c);
        g.ellipse(p.x, p.y + 13, 14, 5, rgb(0xff3b30, uint8_t(120 + 90 * std::sin(t * 7))));
      }
  for (auto& w : warnings_)  // (2026-10-25) mécanique d'un boss : la charge montre où ils reculeront, les ronces percent le sol
    for (int c = 0; c < BOARD_CELLS && !w.kind.empty(); c++) {
      if (!inBoardZone(w.zone, w.center, c)) continue;
      Pt p = cellPos(c);
      if (w.kind == "charge" || w.kind == "attire") {  // flèches : où ils reculeront (charge), ou vers lui (attire, 2026-10-26)
        float d = (boardX(c) < boardX(cellOf(w.who)) ? -1.f : 1.f) * (w.kind == "attire" ? -1.f : 1.f), k = std::fmod(t * 1.5f + 10 - boardX(c) * .2f * d, 1.f);
        g.tri(p.x + d * 5, p.y + 13, p.x - d * 2, p.y + 9.5f, p.x - d * 2, p.y + 16.5f, rgb(0xffe0b0, uint8_t(120 + 120 * k)));
      } else if (w.kind == "ronces") {  // pointes de la couleur du terrain qui va pousser (ronces, braises, foudre)
        Color col = rgb(0x3a1a0c, 220);
        for (auto& x : rules().terrains)
          if (x.id == jget<std::string>(w.who->mech, "terrain", "") && x.look != "epines") col = rgb(x.color, 220);
        for (int i = 0; i < 3; i++) {
          float h = 2 + 2.5f * std::max(0.f, std::sin(t * 4 + i * 2 + c));
          g.tri(p.x - 7 + i * 7, p.y + 15, p.x - 5 + i * 7, p.y + 15, p.x - 6 + i * 7, p.y + 15 - h, col);
        }
      }
    }
  // Limite (3.4) : un cercle d'invocation (Godot) tourne au sol sous chaque cible, sous les combattants
  if (anim_.limit && t - anim_.t0 >= 0 && t - anim_.t0 < .8f)
    if (const SpriteRect* r = findSprite("gfx_cercle_" + std::to_string(std::clamp(int((t - anim_.t0) / .8f * 8), 0, 7))))
      for (auto& f : anim_.targets) {
        Pt p = pos(f);
        g.sprite(*r, std::round(p.x - r->w / 2.f), std::round(p.y + 23 - r->h));
      }
  // Obstacles (3.4) : posés sur leur case, avec leur ombre (sous les combattants : ils sont petits)
  for (auto& [c, spr] : obstacles_)
    if (const SpriteRect* r = findSprite(spr)) {
      Pt p = cellPos(c);
      g.ellipse(p.x, p.y + 14, r->w * .4f, 3, rgb(0, 60));
      g.sprite(*r, std::round(p.x - r->w / 2.f), std::round(p.y + 16 - r->h), c % 2 == 1);
    }
  // Combattants, du haut du damier vers le bas : ceux de devant passent par-dessus
  std::vector<FighterP> shown;
  for (auto& f : foes)
    if (f->alive() || (koT_.count(f.get()) && t - koT_[f.get()] < .6f)) shown.push_back(f);  // vaincu : il s'efface
  for (auto& a : allies) shown.push_back(a);
  std::stable_sort(shown.begin(), shown.end(), [&](const FighterP& a, const FighterP& b) {
    if (a->alive() != b->alive()) return !a->alive();  // les K.O. sont couchés au sol : dessinés avant les autres
    return pos(a).y < pos(b).y;
  });
  // Zone en cours (choix sur le damier, ou aperçu de la technique survolée) : les combattants du camp visé hors de
  // la zone sont estompés, pour que la cible ressorte même quand ils se chevauchent
  int zSide = -1, zCenter = -1;
  std::set<int> zCells;
  {
    const Move* zm = nullptr;
    bool side = false;
    if (boardPicking()) {
      Menu& m = G.menus.top();
      if (!pick_.move && m.sel >= 0 && m.sel < (int)pick_.cells.size()) zCenter = pick_.cells[(size_t)m.sel], side = at(zCenter) && isAlly(at(zCenter)), zm = aiming_;
    } else if (!preview_.empty() && previewDepth_ == G.menus.depth() && G.menus.active() && cursor && cursor->alive() && !isAlly(cursor) &&
               moveInfo(preview_).target == Target::Enemy) {
      zm = &moveInfo(preview_);
      zCenter = cellOf(cursor);
    }
    if (zCenter >= 0) {
      zSide = side;
      for (int c = 0; c < BOARD_CELLS; c++)
        if (c == zCenter || (zm && (zm->zone == Area::All || inBoardZone(zm->zone, zCenter, c)))) zCells.insert(c);
    }
  }
  // Le regard (2026-10-23) : une petite flèche au sol, devant les pieds, de la couleur du camp ; le dos est de l'autre côté
  for (auto& f : shown)
    if (f->alive() && cellOf(f) >= 0 && !ending_) {
      Pt p = pos(f);
      float d = facesRight(f) ? 1.f : -1.f, x0 = p.x + d * (f->boss ? 20 : 14), y0 = p.y + 14;
      g.tri(x0 - d, y0 - 4, x0 - d, y0 + 4, x0 + d * 6, y0, rgb(0x0a0f33, 140));  // contour sombre
      g.tri(x0, y0 - 2.8f, x0, y0 + 2.8f, x0 + d * 4.5f, y0, isAlly(f) ? rgb(0x8fd0ff, 230) : rgb(0xff8a7a, 230));
    }
  // Garde et intention (2026-10-24) : un petit bouclier, et l'icône de ce qu'un ennemi prépare (épée, orbe de la couleur
  // du type, croix verte pour un soin, sablier pour une garde) ; chez un ennemi, de part et d'autre de sa barre de PV
  for (auto* side : {&allies, &foes})  // (2026-10-25) depuis quand chacun est en garde : le bouclier se déplie
    for (auto& f : *side)
      if (guarding_.count(f.get())) guardT_.emplace(f.get(), t);
      else guardT_.erase(f.get());
  auto drawGuard = [&](const FighterP& f, float gx, float gy) {
    if (!guarding_.count(f.get()) || !f->alive() || ending_) return;  // combat fini : plus de garde à montrer
    // il se déplie en 0,2 s, de la tranche à toute sa largeur (un peu plus large un instant), puis son bord luit
    float u = (t - guardT_[f.get()]) / .2f, w = u >= 1 ? 1.f : std::max(.15f, std::sin(std::max(0.f, u) * 2) / std::sin(2.f));
    Color rim = mix(rgb(0xffffff), rgb(0x1c2a78), std::clamp((u - 1) * 1.5f, 0.f, 1.f));
    rim.a = 235;
    g.poly({{gx - 4 * w, gy}, {gx + 4 * w, gy}, {gx + 4 * w, gy + 4}, {gx, gy + 8}, {gx - 4 * w, gy + 4}}, rim);
    g.poly({{gx - 2.5f * w, gy + 1.5f}, {gx + 2.5f * w, gy + 1.5f}, {gx + 2.5f * w, gy + 4}, {gx, gy + 6.3f}, {gx - 2.5f * w, gy + 4}}, rgb(0x8fd0ff, 235));
  };
  auto intentCol = [&](const Intent& in) {  // la couleur de son icône (aussi celle des éclats quand elle se brise)
    if (in.wait) return in.posture == "cuirasse" ? rgb(0xc8d6f0) : in.posture == "voile" ? rgb(0xc8b8ff) : rgb(0xd8e4f4);
    const Move& m = moveInfo(in.move);
    if (m.kind == Kind::Heal || m.kind == Kind::Revive) return rgb(0x6ad87a);
    if (m.kind == Kind::Magic || m.kind == Kind::Status) return rgb(m.type >= 0 && m.type < (int)types().size() ? types()[(size_t)m.type].color : 0xc8a8ff);
    return rgb(0xf4f8ff);
  };
  auto intentIcon = [&](const Intent& in, float ix, float iy, float s) {  // s : échelle
    Color c = intentCol(in);
    g.ellipse(ix, iy, 6 * s, 6 * s, rgb(0x0a0f33, 210));
    if (in.wait && in.posture == "cuirasse") {  // (2026-10-25) un écu : il va se cuirasser
      g.poly({{ix - 3.5f * s, iy - 3.5f * s}, {ix + 3.5f * s, iy - 3.5f * s}, {ix + 3.5f * s, iy + .5f * s}, {ix, iy + 4 * s}, {ix - 3.5f * s, iy + .5f * s}}, c);
      g.rect(ix - .5f * s, iy - 2.5f * s, s, 5 * s, rgb(0x5a6070));
      return;
    }
    if (in.wait && in.posture == "voile") {  // trois volutes de brume : il va se voiler
      for (int i = 0; i < 3; i++) g.ellipse(ix + (i - 1) * 2.5f * s, iy + (i == 1 ? -1.5f : 1.f) * s, 2.2f * s, 1.4f * s, c);
      return;
    }
    if (in.wait) {  // sablier : il se met en garde
      g.tri(ix - 3 * s, iy - 3.5f * s, ix + 3 * s, iy - 3.5f * s, ix, iy, c), g.tri(ix - 3 * s, iy + 3.5f * s, ix + 3 * s, iy + 3.5f * s, ix, iy, c);
      return;
    }
    const Move& m = moveInfo(in.move);
    if (m.kind == Kind::Heal || m.kind == Kind::Revive) {  // croix verte : il va soigner un des siens
      g.rect(ix - s, iy - 4 * s, 2 * s, 8 * s, c), g.rect(ix - 4 * s, iy - s, 8 * s, 2 * s, c);
    } else if (m.kind == Kind::Magic || m.kind == Kind::Status) {  // orbe de la couleur du type
      g.ellipse(ix, iy, 3.5f * s, 3.5f * s, c);
      if (!typeMark(g, ix, iy, m.type)) g.ellipse(ix - 1.2f * s, iy - 1.2f * s, 1.2f * s, s, rgb(0xffffff, 200));
    } else if (const SpriteRect* ic = findSprite("ic_epee_1")) {  // un coup : l'épée du menu Attaquer
      float k = std::min(1.f, 9.f / std::max(ic->w, ic->h)) * s;
      g.sprite(*ic, ix - ic->w * k / 2, iy - ic->h * k / 2, false, k);
    } else {
      g.line(ix - 3 * s, iy + 3 * s, ix + 3 * s, iy - 3 * s, 2, c), g.line(ix - 3 * s, iy - s, ix - s, iy + s, 1, rgb(0xc08040));
    }
  };
  auto drawIntent = [&](const FighterP& f, float ix, float iy) {
    if (!f->alive()) return;
    iy += std::sin(t * 5) * 1.f;
    if (auto it = intents_.find(f.get()); it != intents_.end()) {
      if (it->second.wait || hasMove(it->second.move)) intentIcon(it->second, ix, iy, 1 + std::max(0.f, (.35f - (t - it->second.t0)) * 2));  // apparaît en grand, se pose en 0,35 s
      return;
    }
    // (2026-10-25) elle vient de partir : exécutée, l'icône grossit et s'efface (l'annonce, puis le coup) ; déjouée, elle
    // se fend et vole en éclats
    auto fx = intentFx_.find(f.get());
    if (fx == intentFx_.end() || !(fx->second.in.wait || hasMove(fx->second.in.move))) return;
    const Intent& in = fx->second.in;
    float k = (t - fx->second.t0) / .45f, a0 = g.alpha;
    if (k < 0 || k >= 1) return;
    Color c = intentCol(in);
    if (!fx->second.foiled) {
      g.alpha = a0 * (1 - k * k);
      intentIcon(in, ix, iy, 1 + .6f * std::sqrt(k));
      for (int i = 0; i < 12; i++) {  // un anneau qui s'ouvre
        float ang = i * .5236f, r = 7 + k * 8;
        g.rect(std::round(ix + std::cos(ang) * r), std::round(iy + std::sin(ang) * r), 1, 1, c);
      }
      g.alpha = a0;
      return;
    }
    if (k < .35f) {  // l'icône tremble, se fend (un éclair blanc la traverse) et pâlit
      float jx = ix + std::sin(k * 70) * 1.f;
      g.alpha = a0 * (1 - k / .35f);
      intentIcon(in, jx, iy, 1);
      g.line(jx - 5, iy - 5, jx, iy, 1.5f, rgb(0xffffff)), g.line(jx, iy, jx - 1, iy + 2, 1.5f, rgb(0xffffff)), g.line(jx - 1, iy + 2, jx + 5, iy + 5, 1.5f, rgb(0xffffff));
      g.alpha = a0;
    }
    for (int i = 0; i < 8; i++) {  // éclats qui s'écartent en tournant, puis retombent
      float ang = i * .785f + .4f, r = 3 + k * 13, x = ix + std::cos(ang) * r, y = iy + std::sin(ang) * r * .8f + k * k * 12;
      float sp = ang + k * 8, d = i % 3 ? 2.f : 3.f;
      Color col = i % 2 ? rgb(0x8fd0ff) : i % 4 ? c : rgb(0x2a3460);
      col.a = uint8_t(235 * (1 - k));
      g.tri(x + std::cos(sp) * d, y + std::sin(sp) * d, x + std::cos(sp + 2.1f) * d, y + std::sin(sp + 2.1f) * d, x + std::cos(sp + 4.2f) * d,
            y + std::sin(sp + 4.2f) * d, col);
    }
  };
  // (2026-10-26) avant de dessiner un combattant : son étirement ou écrasement, et l'éclair blanc d'un critique ou d'une
  // Brisure (une seule image) ; after() remet le dessin normal. Pas de traînée pendant le recul d'un coup reçu.
  auto before = [&](const FighterP& f, float x, float y) {
    squash(f, x, y);
    auto it = impacts_.find(f.get());
    if (it != impacts_.end() && it->second.flash) it->second.flash = false, g.spriteFlash = 1;
  };
  auto after = [&] { g.squashX = g.squashY = 1, g.spriteFlash = 0; };
  auto recoiling = [&](const FighterP& f) {
    auto it = impacts_.find(f.get());
    return it != impacts_.end() && since(f.get(), it->second.t0) < .25f;
  };
  for (auto& f : shown) {
    Pt p = pos(f);
    Pt o = offset(f);
    bool dim = zSide >= 0 && isAlly(f) == (zSide == 1) && !zCells.count(cellOf(f));
    if (anim_.actor == f && (anim_.fx == Fx::Magic || anim_.fx == Fx::Heal) && t - anim_.t0 >= 0 && t - anim_.t0 < .6f) {
      // incantation : un cercle de la couleur du sort s'ouvre sous le lanceur, avec des runes qui tournent
      float u = (t - anim_.t0) / .6f, r = 8 + u * 8, a = std::sin(u * 3.1416f);
      Color c = anim_.col;
      c.a = uint8_t(170 * a);
      g.ellipse(p.x, p.y + 13, r * 1.4f, r * .45f, Color{c.r, c.g, c.b, uint8_t(60 * a)});
      for (int i = 0; i < 8; i++) {
        float ang = i * .785f + u * 4;
        g.rect(std::round(p.x + std::cos(ang) * r * 1.4f), std::round(p.y + 13 + std::sin(ang) * r * .45f), 2, 1, c);
      }
    }
    if (std::abs(o.x) > 5 && f->alive() && !(blinking && blink == f) && !recoiling(f)) {  // élan : deux silhouettes pâles derrière l'attaquant
      float s = isAlly(f) ? 1.f : f->boss ? 1.45f : f->sp == "brumelin" ? .85f : 1.f, a0 = g.alpha;
      for (float k : {.3f, .6f}) {
        g.alpha = a0 * (.12f + .18f * k);
        drawFighterSprite(g, *f, p.x + o.x * k, p.y + o.y * k, s, facesRight(f), t);
      }
      g.alpha = a0;
    }
    if (!isAlly(f)) {  // ennemi : barre de PV au-dessus, nom sous le curseur
      float s = f->boss ? 1.45f : f->sp == "brumelin" ? .85f : 1.f;
      if (dim) g.alpha = .45f;
      if (!f->alive()) {  // vaincu : il pâlit et s'efface en s'enfonçant un peu
        float k = std::clamp(since(f.get(), koT_[f.get()]) / .6f, 0.f, 1.f);
        g.alpha = 1 - k;
        before(f, p.x + o.x, p.y);  // (2026-10-26) le coup qui l'abat le repousse et l'écrase encore
        drawFighterSprite(g, *f, p.x + o.x, p.y + k * 4, s, false, t);
        after();
        g.alpha = (1 - k) * .8f;
        g.ellipse(p.x, p.y - 4, 10 + k * 14, 8 + k * 10, rgb(0xffffff, 120));
        g.alpha = 1;
        continue;
      }
      // (2026-10-25) posture d'un boss : cuirassé, il prend la teinte de la pierre ; voilé, il pâlit dans des volutes de brume
      auto pz = posture_.find(f.get());
      bool veil = pz != posture_.end() && pz->second == "voile", armor = pz != posture_.end() && pz->second == "cuirasse";
      float a0 = g.alpha;
      if (veil) g.alpha = a0 * .45f;
      if (armor) g.spriteTint = rgb(0xa8b4c8);
      if (!(blinking && blink == f)) {
        before(f, p.x + o.x, p.y + o.y);
        drawFighterSprite(g, *f, p.x + o.x, p.y + o.y, s, facesRight(f), t + p.x * .01f);  // vers l'adversaire le plus proche
        after();
      }
      g.spriteTint = rgb(0xffffff), g.alpha = a0;
      if (veil)
        for (int i = 0; i < 6; i++) {
          float ang = t * 1.3f + i * 1.047f;
          g.ellipse(p.x + std::cos(ang) * 18, p.y - 8 + std::sin(ang) * 6, 9, 3.5f, rgb(0xd8d4e8, uint8_t(70 + 40 * std::sin(t * 2 + i))));
        }
      if (armor) {  // un anneau de pierre au sol, et des écailles qui luisent l'une après l'autre
        float top = fighterTop(*f, s), cy = p.y - top * .45f;
        g.ellipse(p.x, p.y + 13, 24, 7, rgb(0xc8d6f0, uint8_t(60 + 40 * std::sin(t * 4))));
        for (int i = 0; i < 6; i++) {
          float ang = i * 1.047f + .3f, x = p.x + std::cos(ang) * top * .4f, y = cy + std::sin(ang) * top * .35f;
          float glint = std::max(0.f, std::sin(t * 3 - i * 1.3f));
          g.poly({{x, y - 3}, {x + 2.5f, y}, {x, y + 3}, {x - 2.5f, y}}, rgb(0xe8f0ff, uint8_t(80 + 160 * glint)));
        }
      }
      float bw = f->boss ? 24 : 15, by = std::max(46.f, p.y - fighterTop(*f, s) - 6);
      g.rect(p.x - bw - 1, by - 1, bw * 2 + 2, 4, rgb(0x0a0f33));
      g.rect(p.x - bw, by, bw * 2 * f->hp / f->mhp, 2, rgb(f->broken ? 0xffe066 : 0xff8a7a));
      // Brisure : bouclier (points restants) à gauche de la barre, ou « BRISÉ » ; dessous, les faiblesses découvertes
      if (f->broken) {
        bool popping = std::any_of(pops.begin(), pops.end(), [&](const Pop& q) {  // « BRISÉ ! » jaillit encore : pas deux fois le mot
          return q.text == T("BRISÉ !") && G.time - q.t < 1 && std::fabs(q.x - p.x) < 24;
        });
        if (!popping) g.text(p.x, by - 20, T("BRISÉ"), rgb(0xffe066, uint8_t(180 + 70 * std::sin(t * 9))), 1);
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
      bool marks = options().typeMarks;  // (2026-10-25) types lisibles sans couleur : un carré de 7 px par faiblesse, son motif dedans
      for (int ty = 0; ty < (int)types().size(); ty++)
        if (G.weakKnown(f->sp, ty)) {
          if (marks) {
            float wx = std::round(p.x - bw) + nw * 7, wy = std::round(by) + 3;
            g.rect(wx, wy, 7, 7, rgb((types()[ty].color >> 1) & 0x7f7f7f)), typeMark(g, wx + 3.5f, wy + 3.5f, ty);  // bord : comme une pastille
          } else {
            g.rect(p.x - bw + nw * 5, by + 4, 4, 3, rgb(0x0a0f33));
            g.rect(p.x - bw + nw * 5, by + 4, 3, 2, rgb(types()[ty].color));
          }
          nw++;
        }
      drawStatusTag(*f, p.x, by - 10);
      drawGuard(f, p.x - bw + 2, by - 10);
      drawIntent(f, p.x + bw - 2, by - 10);
      g.alpha = 1;
      continue;
    }
    if (!f->alive()) g.alpha = .35f;
    else if (dim) g.alpha = .45f;
    // Poses (3.3) : lancer de la lanterne pendant une capture, à terre une fois K.O., victoire à la fin du combat
    std::string pose;
    float ce = t - anim_.t0;
    if (anim_.actor == f && anim_.fx == Fx::Capture && ce >= 0 && ce < .5f) pose = "lancer_" + std::to_string(std::min(4, int(ce / .09f)));
    else if (!f->alive()) pose = "ko";
    else if (summaryOn_) pose = "victoire";
    if (!f->alive()) {  // K.O. (demandé le 2026-10-19 : on doit le voir au premier coup d'œil) : couché, grisé, « K.O. »
      g.alpha = .9f;
      g.spriteTint = rgb(0x8a8aa0);
      g.ellipse(p.x, p.y + 13, 13, 3.5f, rgb(0, 70));
      if (!drawFighterPose(g, *f, "ko", p.x, p.y, facesRight(f))) {  // pas de pose dessinée : le sprite couché sur le côté
        g.spriteAngle = facesRight(f) ? -90.f : 90.f;
        drawFighterSprite(g, *f, p.x, p.y + 8, 1, facesRight(f), 0);
        g.spriteAngle = 0;
      }
      g.spriteTint = rgb(0xffffff);
      g.alpha = 1;
      continue;  // étoiles et « K.O. » : après tous les combattants (jamais cachés)
    }
    float hop = 0;  // victoire (2026-10-20) : sans pose dessinée, trois petits bonds de joie, décalés d'un allié à l'autre
    if (summaryOn_ && isAlly(f)) {
      float a = t - summaryT_ - (p.x + p.y) * .004f;
      if (a > 0 && a < 1.5f) hop = -std::round(std::abs(std::sin(a * 6.2832f)) * 5);
    }
    if (std::abs(o.x) > 3 && pose.empty() && !(blinking && blink == f) && !recoiling(f)) {  // (2026-10-21) l'élan laisse deux traînées
      float keepA = g.alpha;
      for (float k : {.35f, .65f}) {
        g.alpha = keepA * (k < .5f ? .18f : .3f);
        drawFighterSprite(g, *f, p.x + o.x * k, p.y + o.y * k + hop, 1, facesRight(f), t + p.x * .01f);
      }
      g.alpha = keepA;
    }
    if (!(blinking && blink == f)) {
      before(f, p.x + o.x, p.y + o.y + hop);
      if (!(!pose.empty() && drawFighterPose(g, *f, pose, p.x + o.x, p.y + o.y, facesRight(f))))
        drawFighterSprite(g, *f, p.x + o.x, p.y + o.y + hop, 1, facesRight(f), t + p.x * .01f);  // vers l'adversaire le plus proche
      after();
    }
    g.alpha = 1;
    drawStatusTag(*f, p.x - 24, p.y - 16);
    drawGuard(f, p.x - 13, std::max(44.f, p.y - fighterTop(*f, 1) - 4));  // en garde : à côté de la tête
    if (actor == f) {  // celui qui choisit son action : une flèche au-dessus de sa tête
      float ty = std::max(44.f, p.y - fighterTop(*f, 1) - 10);
      g.tri(p.x - 4, ty, p.x + 4, ty, p.x, ty + 6, rgb(0xffd34d));
    }
    if (cursor == f) g.cursor(p.x - 24, p.y - 4);
  }
  // Alliés K.O. : petites étoiles qui tournent et « K.O. », par-dessus tout le monde
  for (auto& f : allies)
    if (!f->alive()) {
      Pt p = pos(f);
      for (int i = 0; i < 3; i++) {
        float a = t * 3 + i * 2.094f;
        float sx = p.x + std::cos(a) * 8, sy = p.y + 2 + std::sin(a) * 2.5f;
        g.tri(sx - 2, sy, sx + 2, sy, sx, sy - 3, rgb(0xffe066, 210));
        g.tri(sx - 2, sy - 2, sx + 2, sy - 2, sx, sy + 1, rgb(0xffe066, 210));
      }
      g.rect(p.x - 11, p.y - 7, 22, 9, rgb(0x0a0f33, 200));
      g.text(p.x, p.y - 8, T("K.O."), rgb(0xff7b6b), 1, false);
    }
  // Intentions des ennemis (2026-10-24) : un chevron orange au-dessus de chaque allié visé, la zone en orange au sol
  {
    std::map<const Fighter*, int> aimed;
    std::map<const Fighter*, std::pair<int, int>> dmg;  // dégâts prévus, additionnés quand plusieurs le visent
    for (auto& [who, in] : intents_) {
      if (in.move.empty() || !hasMove(in.move)) continue;
      FighterP src;
      for (auto& e : foes)
        if (e.get() == who && e->alive()) src = e;
      if (!src) continue;
      const Move& m = moveInfo(in.move);
      for (auto& tg : in.targets) {
        if (!tg || !tg->alive() || !isAlly(tg)) continue;
        if (m.zone != Area::Single && m.zone != Area::All && cellOf(tg) >= 0)
          for (int c = 0; c < BOARD_CELLS; c++)
            if (inBoardZone(m.zone, cellOf(tg), c)) tile(g, cellPos(c), rgb(0xff9a3a, 60));
        aimed[tg.get()]++;
        if (m.damaging())
          if (Preview pv = preview(src, m, tg, in.cell); pv.eff > 0) dmg[tg.get()].first += pv.lo, dmg[tg.get()].second += pv.hi;
        if (cursor == src) {  // l'ennemi survolé : un pointillé vers sa cible
          Pt a = pos(src), b = pos(tg);
          float dx = b.x - a.x, dy = b.y - a.y, len = std::max(1.f, std::sqrt(dx * dx + dy * dy));
          for (float k = 8; k < len - 8; k += 6)
            g.rect(std::round(a.x + dx * k / len) - 1, std::round(a.y + dy * k / len) - 1, 2, 2, rgb(0xff9a3a, 210));
        }
      }
    }
    for (auto& a : allies)
      if (int n = aimed[a.get()]; n > 0 && a->alive()) {
        Pt p = pos(a);
        float y = std::max(44.f, p.y - fighterTop(*a, 1) - 9) + std::sin(t * 6) * 1.5f;
        int k2 = std::min(n, 3);
        for (int k = 0; k < k2; k++) {
          float x = p.x + (k - (k2 - 1) * .5f) * 7;
          g.tri(x - 4, y, x + 4, y, x, y + 5, rgb(0x2a1606, 220));
          g.tri(x - 3, y + .5f, x + 3, y + .5f, x, y + 4, rgb(0xff9a3a));
        }
        if (auto [lo, hi] = dmg[a.get()]; lo > 0)  // ce qu'il va prendre : de quoi décider (garde, soin, recul)
          g.text(p.x, y - 10, "≈ " + std::to_string(lo) + (hi > lo ? "-" + std::to_string(hi) : ""), rgb(0xff9a3a), 1);
      }
  }
  // Par-dessus les combattants : contour de la zone et flèche au-dessus de la cible (le sprite cache sa case)
  if (zSide >= 0) {
    for (int c : zCells) tile(g, cellPos(c), rgb(c == zCenter ? 0xfff2a0 : 0xffb050, 210), true);
    for (auto& f : zSide == 1 ? allies : foes)
      if (cellOf(f) == zCenter) {
        Pt p = cellPos(zCenter);
        float y = std::max(48.f, p.y - fighterTop(*f, 1) - 16) + std::sin(t * 8) * 1.5f;
        g.tri(p.x - 5, y, p.x + 5, y, p.x, y + 6, rgb(0xffe066));
        g.tri(p.x - 3, y + 1, p.x + 3, y + 1, p.x, y + 4, rgb(0xfff8d0));
      }
  }
  drawAnim();
  // Éclats (3.4, dessinés avec Godot) : Percée et Brisure, 8 images en une demi-seconde, posés sur le combattant
  bursts_.erase(std::remove_if(bursts_.begin(), bursts_.end(), [&](const Burst& b) { return t - b.t0 > .5f; }), bursts_.end());
  // (le bond : la poussière part un peu après, quand le combattant touche le sol)
  for (auto& b : bursts_) {
    if (t < b.t0) continue;  // pas encore (poussière à l'atterrissage)
    // (2026-10-25) dessinés en code : le bouclier qui encaisse, le coup de la riposte
    bool guardFx = !std::strcmp(b.fx, "garde"), riposteFx = !std::strcmp(b.fx, "riposte");
    if (guardFx || riposteFx) {
      FighterP from = anim_.actor && anim_.actor != b.f ? anim_.actor : nullptr;  // l'attaquant (son action est encore anim_)
      Pt p = pos(b.f), o = offset(b.f);
      float u = (t - b.t0) / .35f;
      if (guardFx && u < 1) {  // un arc bleu devant lui, du côté du coup, qui s'écarte et pâlit
        float dir = from && std::abs(pos(from).x - p.x) > 1 ? (pos(from).x > p.x ? 1.f : -1.f) : facesRight(b.f) ? 1.f : -1.f;
        float cx = p.x + o.x, cy = p.y - 6, rx = 13 + u * 3, ry = 17 + u * 2;
        for (int i = 0; i < 8; i++) {
          float a1 = -1.1f + i * .275f, a2 = a1 + .275f;
          auto arc = [&](float a, float r) { return Pt{cx + dir * std::cos(a) * r, cy + std::sin(a) * r * ry / rx}; };
          Pt q1 = arc(a1, rx), q2 = arc(a2, rx);
          g.poly({q1, q2, arc(a2, rx - 4), arc(a1, rx - 4)}, rgb(0x8fd0ff, uint8_t(80 * (1 - u))));  // un voile, comme du verre
          g.line(q1.x, q1.y, q2.x, q2.y, 2, rgb(0x8fd0ff, uint8_t(240 * (1 - u))));
          if (u < .5f) g.line(q1.x - dir, q1.y, q2.x - dir, q2.y, 1, rgb(0xffffff, uint8_t(255 * (1 - u * 2))));
        }
        for (int i = 0; i < 4; i++) {  // quelques étincelles rejaillissent vers l'attaquant
          float x = cx + dir * (rx + 1 + u * 10), y = cy - 6 + i * 4 + (i - 1.5f) * u * 6;
          g.line(x, y, x + dir * 2, y + (i - 1.5f), 1, rgb(0xe0f4ff, uint8_t(255 * (1 - u))));
        }
      }
      if (riposteFx && from && from->alive() && u > .2f && u < 1.2f) {  // deux entailles dorées sur l'attaquant
        Pt q = pos(from);
        float a = std::clamp((u - .2f) * 4, 0.f, 1.f), fade = std::clamp((1.2f - u) * 2.5f, 0.f, 1.f);
        q.y -= 6;
        g.line(q.x - 7, q.y - 7, q.x - 7 + 14 * a, q.y - 7 + 14 * a, 2, rgb(0xffd34d, uint8_t(240 * fade)));
        g.line(q.x + 7, q.y - 7, q.x + 7 - 14 * a, q.y - 7 + 14 * a, 2, rgb(0xffd34d, uint8_t(240 * fade)));
        g.line(q.x - 6, q.y - 7, q.x - 6 + 12 * a, q.y - 7 + 12 * a, 1, rgb(0xffffff, uint8_t(220 * fade)));
      }
      continue;
    }
    int k = std::clamp(int((t - b.t0) / .5f * 8), 0, 7);
    if (const SpriteRect* r = findSprite(std::string("gfx_") + b.fx + "_" + std::to_string(k))) {
      Pt p = pos(b.f);
      g.sprite(*r, std::round(p.x - r->w / 2.f), std::round(p.y + 14 - r->h));
    }
  }
  // Nombres flottants
  pops.erase(std::remove_if(pops.begin(), pops.end(), [&](const Pop& p) { return t - p.t > 1.f; }), pops.end());
  {  // nombres et mots qui jaillissent : un petit rebond, et jamais l'un sur l'autre (le plus récent monte au-dessus)
    std::vector<const Pop*> order;
    for (auto& p : pops) order.push_back(&p);
    std::sort(order.begin(), order.end(), [](const Pop* a, const Pop* b) { return a->t < b->t; });
    struct Placed {
      float x, w, top, bot;  // centre, demi-largeur, haut et bas des textes déjà posés
    };
    std::vector<Placed> placed;
    for (const Pop* p : order) {
      float u = t - p->t, y = p->y - std::min(1.f, u * 3) * 8 - (u < .3f ? std::sin(u / .3f * 3.1416f) * 3 : 0.f);
      // (2026-10-26) un nombre de dégâts jaillit plus grand puis se pose (un critique reste en double taille), posé sur le
      // même bas qu'un texte ordinaire ; les textes se rangent d'après leur taille posée (rien ne saute pendant le rebond)
      float size = p->style & PopCrit ? 2.f : 1.f, k = size;
      if (p->style & PopDamage && u < .16f) k *= 1 + .5f * (1 - u / .16f) * (1 - u / .16f);
      float w = Gfx::textW(p->text) * size / 2 + 2, x = std::clamp(p->x, w, 320 - w), bot = y + 11, top = bot - 11 * size;
      for (bool moved = true; moved;) {
        moved = false;
        for (auto& q : placed)
          if (std::abs(q.x - x) < w + q.w && top < q.bot - 2 && bot > q.top + 2) bot = q.top + 2, top = bot - 11 * size, moved = true;
      }
      if (size > 1 && top < popTop_ - 11) top = popTop_ - 11;  // en grand : pas plus haut qu'un texte ordinaire (l'ordre des tours)
      placed.push_back({x, w, top, top + 11 * size});
      // (2026-10-21) contour sombre et dégradé (clair en haut) : lisible sur tous les décors
      Color light = mix(p->col, rgb(0xffffff), .55f), dark = rgb(0x10081a);
      light.a = p->col.a, dark.a = p->col.a;
      float half = Gfx::textW(p->text) * k / 2 + 2;
      g.textBigFancy(std::clamp(x, half, 320 - half), top + 11 * (size - k), p->text, light, p->col, dark, k, 1);
    }
  }
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
  popTop_ = orderY + 22;  // sous les portraits (12 px), en comptant la montée du texte (8 px)
  float orderX = 6;
  if (weather_ >= 0 && !ending_ && !summaryOn_) {  // météo, puis l'ordre des tours sur la même ligne
    const std::string& wn = rules().weathers[(size_t)weather_].name;
    int ww = Gfx::textW(wn) + 6;  // sur une étiquette sombre comme les portraits : lisible sur le décor
    g.rect(4, orderY, ww, 12, rgb(0x0a0f33, 200));
    g.rect(4, orderY + 11, ww, 1, rgb(0x8fd0ff));
    g.text(7, orderY + 1, wn, rgb(0xe8ecff));
    orderX = 4 + ww + 3;
  }
  float orderMax = 316;
  if (!reserve.empty() && !ending_ && !summaryOn_) {  // renforts ennemis : à droite de la même ligne
    std::string rs = T("Renforts : ") + std::to_string(reserve.size());
    int rw = Gfx::textW(rs) + 6;
    g.rect(316 - rw, orderY, rw, 12, rgb(0x0a0f33, 200));
    g.rect(316 - rw, orderY + 11, rw, 1, rgb(0xff8a7a));
    g.text(313, orderY + 1, rs, rgb(0xd8dcef), 2);
    orderMax = 316 - rw - 3;
  }
  if (!ending_ && !summaryOn_ && !follower()) drawTurnOrder(orderY, orderX, orderMax);  // ordre des prochains tours, sous le message
  if (G.menus.shownDepth() <= 1) drawHud(t);  // un sous-menu ouvert prend sa place
  drawWeather();
  if (summaryOn_)  // niveau gagné : une colonne d'étincelles dorées monte autour de l'allié, dès que sa barre y arrive
    for (auto& r : gainRows_)
      for (auto& a : allies)
        if (a.get() == r.f && a->alive() && int(r.at(gainsFill(t))) > int(r.from)) {
          Pt p = pos(a);
          float age = t - summaryT_;
          for (int k = 0; k < 10; k++) {
            float ph = std::fmod(age * .9f + k * .1f, 1.f), x = p.x + std::sin(k * 2.4f + age * 3) * 10, y = p.y + 10 - ph * 46;
            float s = 1.5f * std::sin(ph * 3.14159f);
            g.rect(x - s, y, 2 * s + 1, 1, rgb(0xffe680, uint8_t(230 * (1 - ph)))), g.rect(x, y - s, 1, 2 * s + 1, rgb(0xffe680, uint8_t(230 * (1 - ph))));
          }
        }
  if (summaryOn_) {  // fenêtre des gains de la victoire
    // à droite, du côté des ennemis vaincus : l'équipe reste visible (pose de victoire). (2026-10-26) une ligne par
    // combattant (portrait, nom, barre d'expérience qui se remplit, « Niveau N ! »), puis l'or et le butin en icônes
    float fill = gainsFill(t);
    auto levelText = [](int n) {
      std::string s = T("Niveau {n} !");
      size_t at = s.find("{n}");
      return at == std::string::npos ? s : s.replace(at, 3, std::to_string(n));
    };
    struct Chip {
      std::string text;
      const SpriteRect* icon;
    };
    std::vector<Chip> chips;  // l'or (le premier, avec une pièce), puis chaque objet
    if (goldWon_ > 0) chips.push_back({T("Or : +") + std::to_string(goldWon_), nullptr});
    for (auto& [id, n] : loot_) chips.push_back({item(id).name + (n > 1 ? " ×" + std::to_string(n) : ""), item(id).icon.empty() ? nullptr : findSprite(item(id).icon)});
    int w = Gfx::textW(T("Entrée : continuer")) + 2, learned = 0;
    for (auto& r : gainRows_) {
      int right = Gfx::textW("+" + std::to_string(r.xp) + T(" exp"));
      if (int(r.to) > int(r.from)) right = std::max(right, Gfx::textW(levelText(int(r.to))));
      w = std::max(w, 22 + Gfx::textW(r.f->name()) + 8 + right);
      for (auto& [lv, mv] : r.learned) w = std::max(w, 28 + Gfx::textW(T("Apprend ") + mv)), learned++;
    }
    for (auto& n : gainNotes_) w = std::max(w, Gfx::textW(n));
    for (auto& c : chips) w = std::max(w, 13 + Gfx::textW(c.text));
    w = std::min(312, w + 16);
    std::vector<std::vector<size_t>> chipLines;  // les icônes à la suite, à la ligne quand la place manque
    for (int i = 0, used = 0; i < (int)chips.size(); i++) {
      int cw = 13 + Gfx::textW(chips[(size_t)i].text);
      if (chipLines.empty() || used + 8 + cw > w - 16) chipLines.push_back({}), used = -8;
      chipLines.back().push_back((size_t)i), used += 8 + cw;
    }
    int h = 18 + 15 * (int)gainRows_.size() + (gainNotes_.empty() ? 0 : 2 + 11 * (int)gainNotes_.size()) +
            (chipLines.empty() ? 0 : 2 + 12 * (int)chipLines.size()) + 14;
    learned = std::clamp((PANEL_Y - 6 - h) / 10, 0, learned);  // techniques apprises : autant de lignes que la place le permet
    h += 10 * learned;
    int x = std::max(4, 316 - w), y = std::max(top.empty() ? 4 : int(orderY), std::min(92 - h / 2, PANEL_Y - 2 - h));
    g.window(x, y, w, h);
    bool big = Gfx::textW(T("Victoire !")) * 2 <= x - 8;  // le grand titre a la place à gauche : la fenêtre dit « Gains »
    g.text(x + w / 2, y + 5, big ? T("Gains") : T("Victoire !"), GOLD, 1);
    {  // (2026-10-22) en grand, à gauche de la fenêtre : « Victoire ! » qui tombe en place, avec deux éclats
      std::string v = T("Victoire !");
      float vw = Gfx::textW(v) * 2.f, cx = (4 + x) / 2.f, age = t - summaryT_;
      if (vw <= x - 8) {
        float drop = std::max(0.f, 1 - age * 4) * 18;
        g.textBigFancy(cx, 36 - drop, v, rgb(0xfff0a0), rgb(0xe8902a), rgb(0x2a1606), 2, 1);
        for (int k = 0; k < 2; k++) {
          float ph = std::fmod(age * 1.3f + k * .5f, 1.f), sx2 = cx + (k ? vw / 2 - 4 : -vw / 2 + 4), sy2 = 40.f, r = 3 * std::sin(ph * 3.14159f);
          g.rect(sx2 - r, sy2, 2 * r + 1, 1, rgb(0xffffff, 220)), g.rect(sx2, sy2 - r, 1, 2 * r + 1, rgb(0xffffff, 220));
        }
      }
    }
    float ly = y + 18;
    for (auto& r : gainRows_) {
      float v = r.at(fill);  // la barre en niveaux : la partie entière est le niveau atteint
      int lv = int(v), lv0 = int(r.from);
      drawFighterIcon(g, *r.f, x + 18, ly + 5, 20, 10, true);
      g.text(x + 30, ly, r.f->name(), WHITE);
      if (lv > lv0) g.text(x + w - 8, ly, levelText(lv), GOLD, 2);
      else g.text(x + w - 8, ly, "+" + std::to_string(int(r.xp * fill + .5f)) + T(" exp"), rgb(0xd8def2), 2);
      float bx = x + 30, bw = x + w - 8 - bx, part = v - lv;
      float flash = lv > lv0 && fill < 1 ? std::max(0.f, 1 - part * 5) : 0;  // éclair blanc au passage d'un niveau
      g.rect(bx, ly + 10, bw, 3, rgb(0x03051a, 200));
      g.rect(bx, ly + 10, bw * part, 3, mix(rgb(0xc89af0), rgb(0xffffff), flash));
      g.rect(bx, ly + 10, bw * part, 1, mix(rgb(0xe8d4ff), rgb(0xffffff), flash));  // reflet
      ly += 15;
      for (auto& [l, mv] : r.learned)
        if (learned > 0) {
          if (lv >= l) g.text(x + 36, ly, T("Apprend ") + mv, BLUE);  // quand la barre arrive à ce niveau
          ly += 10, learned--;
        }
    }
    if (!gainNotes_.empty()) ly += 2;
    for (auto& n : gainNotes_) g.text(x + 8, ly, n, WHITE), ly += 11;
    if (!chipLines.empty()) ly += 2;
    for (auto& line : chipLines) {
      float cx = x + 8;
      for (size_t i : line) {
        const Chip& c = chips[i];
        bool coin = goldWon_ > 0 && i == 0;
        if (coin) menuGlyph(g, "piece", cx, ly - 1.5f);
        else if (c.icon) {
          float k = std::min(1.f, 11.f / std::max(c.icon->w, c.icon->h));
          g.sprite(*c.icon, cx + (11 - c.icon->w * k) / 2, ly - 1.5f + (11 - c.icon->h * k) / 2, false, k);
        }
        g.text(cx + 13, ly, c.text, coin ? rgb(0xffd34d) : WHITE);
        cx += 13 + Gfx::textW(c.text) + 8;
      }
      ly += 12;
    }
    g.text(x + w / 2, y + h - 12, T("Entrée : continuer"), rgb(0xaab3d8), 1);
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
    // (2026-10-22) un bandeau plus large, et le nom en grandes lettres rouge et or qui glissent en place
    bool big = Gfx::textW(bossName) * 2 <= 290;
    float y = 80, h = (big ? 32 : 22) * a;
    g.rect(g.left(), y - h / 2, g.fullW, h, rgb(0x1a0610, uint8_t(225 * a)));
    g.rect(g.left(), y - h / 2, g.fullW, 1, rgb(0xffe066, uint8_t(255 * a)));
    g.rect(g.left(), y - h / 2 + 2, g.fullW, 1, rgb(0xb8402a, uint8_t(200 * a)));
    g.rect(g.left(), y + h / 2 - 3, g.fullW, 1, rgb(0xb8402a, uint8_t(200 * a)));
    g.rect(g.left(), y + h / 2 - 1, g.fullW, 1, rgb(0xffe066, uint8_t(255 * a)));
    float slide = (1 - std::min(1.f, kb * 4)) * 60;
    if (a > .5f) {
      if (big) g.textBigFancy(160 + slide, y - 11, bossName, rgb(0xffe880), rgb(0xd8402a), rgb(0x200408), 2, 1);
      else g.text(160 + slide, y - 4, bossName, rgb(0xffe066), 1);
    }
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
  weather_ = -1;  // ni météo ni terrain ni obstacles : le duel reste le même chez les deux joueurs
  terrain_.fill(-1);
  obstacles_.clear();
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
      a.push_back(Json::array({f->hp, f->mp, (int)std::lround(f->atb), (int)std::lround(f->lim), (int)f->status, st, netCell(cellOf(f)), f->shield, f->broken ? 1 : 0, facingOf(f),
                               guarding_.count(f.get()) ? 1 : 0}));
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
  for (auto& w : warnings_) warn.push_back(Json::array({netCell(w.center), (int)w.zone}));
  Json ter = Json::array();
  for (int v : terrain_) ter.push_back(v);
  Json obs = Json::array();
  for (auto& [c, spr] : obstacles_) obs.push_back(Json::array({netCell(c), spr}));
  // (2026-10-24) les intentions des ennemis, pour que les autres joueurs du gardien les voient aussi : ennemi (indice),
  // technique, case annoncée, alliés visés (indices)
  Json intents = Json::array();
  for (size_t i = 0; i < foes.size(); i++)
    if (auto it = intents_.find(foes[i].get()); it != intents_.end() && foes[i]->alive()) {
      Json tg = Json::array();
      for (auto& t : it->second.targets)
        for (size_t j = 0; j < allies.size(); j++)
          if (allies[j] == t) tg.push_back((int)j);
      intents.push_back(Json{{"f", (int)i}, {"m", it->second.move}, {"c", netCell(it->second.cell)}, {"t", tg}, {"w", it->second.wait}});
    }
  return Json{{"t", "etat"}, {"a", pack(allies)}, {"b", pack(foes)}, {"msg", sc.showingMessage() ? sc.text() : std::string()}, {"attente", wait},
              {"w", warn}, {"meteo", weather_}, {"terrain", ter}, {"obstacles", obs}, {"i", intents}};
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
          if (x.size() > 6) {  // la case, décidée chez l'hôte (en duel, le plateau est vu en miroir)
            int c = netCell(std::clamp(x.at(6).get<int>(), -1, BOARD_CELLS - 1));
            if (cells_.count(&f) && cells_[&f] != c && cells_[&f] >= 0 && shown()) slide_[&f] = {cellPos(cells_[&f]), G.time};
            cells_[&f] = c;
          }
          if (x.size() > 8) f.shield = x.at(7).get<int>(), f.broken = x.at(8).get<int>() != 0;  // Brisure
          if (x.size() > 9) facing_[&f] = (x.at(9).get<int>() < 0 ? -1 : 1) * (net_ == Net::Guest ? -1 : 1);  // regard (miroir chez l'invité)
          if (x.size() > 10) {  // en garde (2026-10-24)
            if (x.at(10).get<int>()) guarding_.insert(&f);
            else guarding_.erase(&f);
          }
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
        if (w.is_array() && w.size() == 2) netWarn_.push_back({netCell(std::clamp(w[0].get<int>(), 0, BOARD_CELLS - 1)), std::clamp(w[1].get<int>(), 0, 4)});
      netWait_ = jget<std::string>(m, "attente", "");
      int wi = jget(m, "meteo", -1);
      weather_ = wi >= 0 && wi < (int)rules().weathers.size() ? wi : -1;
      Json ter = m.value("terrain", Json::array());
      if (ter.size() == BOARD_CELLS)
        for (size_t i = 0; i < ter.size(); i++) {
          int v = ter[i].is_number_integer() ? ter[i].get<int>() : -1;
          terrain_[(size_t)netCell((int)i)] = v >= 0 && v < (int)rules().terrains.size() ? v : -1;
        }
      obstacles_.clear();
      for (auto& o : m.value("obstacles", Json::array()))
        if (o.is_array() && o.size() == 2 && o[0].is_number_integer() && o[1].is_string())
          obstacles_[netCell(std::clamp(o[0].get<int>(), 0, BOARD_CELLS - 1))] = o[1].get<std::string>();
      if (m.contains("i") && m["i"].is_array()) {  // intentions des ennemis (2026-10-24), dessinées comme chez le chef
        std::map<const Fighter*, Intent> seen;
        for (auto& o : m["i"]) {
          int fi = jget(o, "f", -1);
          if (!o.is_object() || fi < 0 || fi >= (int)foes.size()) continue;
          FighterP e = foes[(size_t)fi];
          Intent in;
          in.move = jget<std::string>(o, "m", "");
          if (!in.move.empty() && !hasMove(in.move)) continue;
          int c = jget(o, "c", -1);
          in.cell = c >= 0 && c < BOARD_CELLS ? netCell(c) : -1;
          in.wait = jget(o, "w", false);
          for (auto& t : o.value("t", Json::array()))
            if (t.is_number_integer() && t.get<int>() >= 0 && t.get<int>() < (int)allies.size()) in.targets.push_back(allies[(size_t)t.get<int>()]);
          auto old = intents_.find(e.get());  // la même annonce garde son heure (l'icône ne se repose pas à chaque état)
          in.t0 = old != intents_.end() && old->second.move == in.move ? old->second.t0 : G.time;
          seen[e.get()] = in;
        }
        intents_ = seen;
      }
    } else if (k == "tour") {
      int i = jget(m, "i", -1);
      if (i < 0 || i >= (int)allies.size() || !allies[(size_t)i]->alive() || !mineToCommand(allies[(size_t)i])) return;
      FighterP a = allies[(size_t)i];
      actor = a;
      if (jget(m, "bouge", false)) moved_.insert(a.get());  // il s'est déjà déplacé : son action maintenant
      else moved_.erase(a.get());
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
        pop(f, jget<std::string>(m, "txt", ""), col, jget(m, "s", 0));  // (2026-10-26) style : critique, faiblesse…
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
    bool mineF = i >= 0 && i < (int)side.size() && side[(size_t)i] == f;
    if (m.contains("deplacer")) {  // se déplacer : vers une case où il peut aller ce tour
      int c = netCell(jget(m, "deplacer", -1));
      if (mineF && !moved_.count(f.get()) && walkable(f).count(c)) return moveTo(f, c);
      if (net_ == Net::Lead) return autoCommand(f);
      return enemyTurn(f);
    }
    if (m.contains("attendre") && mineF) return waitTurn(f);
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
      if (ok && moveInfo(id).target == Target::Enemy) ok = reachable(f, moveInfo(id), targets[0]);  // corps à corps : à portée
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
