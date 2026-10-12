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
  std::string who = e.self ? "du lanceur" : m.target == Target::AllAllies ? "de l'équipe" : onAllies ? "de l'allié"
                    : m.target == Target::AllEnemies || m.zone != Area::Single ? "des ennemis touchés" : "de la cible";
  std::string s;
  if (e.cure) s = "guérit les états";
  if (e.status != Status::None) s = statusName(e.status);
  if (e.stat >= 0) s = std::string(stageName(e.stat)) + (e.stages > 0 ? " +" : " −") + std::to_string(std::abs(e.stages)) + " " + who;
  if (e.chance < 100) s += " (" + std::to_string(e.chance) + " % de chance)";
  return s;
}

// Tout ce que fait une technique, en clair : genre, type, puissance, coût, cible et zone, portée,
// effets, rythme. user : le lanceur, pour chiffrer un soin.
std::string moveDetails(const Move& m, const Fighter* user) {
  const Rules& R = rules();
  static const char* KIND[] = {"Physique", "Magique", "Soin", "Réanimation", "Statut"};
  std::string s = std::string(KIND[(int)m.kind]) + " · " + typeName(m.type);
  if (m.damaging()) s += " · Puissance " + std::to_string(m.power);
  s += m.cost > 0 ? " · " + std::to_string(m.cost) + " PM" : std::string(" · Gratuite");
  if (m.acc < 100) s += " · Précision " + std::to_string(m.acc) + " %";
  if (m.critBonus > 0) s += " · Critique +" + std::to_string(m.critBonus) + " %";
  // Cible et zone, portée
  std::string side = m.damaging() ? ", " + pct(R.zoneSide) + " autour" : "";
  switch (m.target) {
    case Target::Enemy:
      switch (m.zone) {
        case Area::Line: s += ". Un ennemi et toute sa ligne (de l'avant à l'arrière" + side + ")"; break;
        case Area::Column: s += ". Un ennemi et toute sa colonne (de haut en bas" + side + ")"; break;
        case Area::Cross: s += ". Un ennemi et ses voisins en croix" + (side.empty() ? "" : " (" + side.substr(2) + ")"); break;
        case Area::All: s += ". Tous les ennemis"; break;
        case Area::Single: s += ". Un ennemi"; break;
      }
      break;
    case Target::AllEnemies: s += ". Tous les ennemis, sans viser"; break;
    case Target::Ally: s += ". Un allié"; break;
    case Target::AllAllies: s += ". Toute l'équipe"; break;
    case Target::AllyKO: s += ". Un allié K.O."; break;
  }
  if (m.damaging() && m.target == Target::Enemy) s += m.melee ? ", corps à corps (colonne ennemie la plus avancée)" : ", à distance (toute case)";
  // Soins
  if (m.kind == Kind::Heal && m.power > 0)
    s += user ? ". Rend environ " + std::to_string(int(m.power * user->eMag() / 20) + 2) + " PV (Magie " + std::to_string(int(user->eMag())) + ")"
              : ". Rend Puissance " + std::to_string(m.power) + " × Magie ÷ 20 PV";
  if (m.kind == Kind::Revive) s += ". Relève avec " + std::to_string(m.power) + " % des PV";
  if (!m.effects.empty()) {
    s += ". Effet : ";
    for (size_t i = 0; i < m.effects.size(); i++) s += (i ? ", " : "") + effectText(m.effects[i], m);
  }
  if (m.pace == Pace::Quick) s += ". Rapide : rejoue plus tôt (jauge à " + std::to_string(R.quickGauge) + " %)";
  if (m.pace == Pace::Heavy) s += ". Lourde : rejoue plus tard (jauge à −" + std::to_string(R.heavyDelay) + " %)";
  return s + ".";
}

std::string moveHelp(const Move& m, const Fighter* user) { return (m.desc.empty() ? "" : m.desc + " ") + moveDetails(m, user); }

// Ce que fait un objet, chiffré : soins, réanimation, capture, bonus d'équipement, où l'utiliser
std::string itemDetails(const ItemDef& d) {
  const Rules& R = rules();
  std::vector<std::string> v;
  if (d.slot >= 0) {
    std::string b = d.bonusText();
    v.push_back(std::string(gearSlotName(d.slot)) + (b.empty() ? "" : " : " + b) + (d.slot != G_ACCESSORY ? " (héros seulement)" : ""));
  }
  if (d.healHp) v.push_back(d.healHp >= 999 ? "Rend tous les PV" : "Rend " + std::to_string(d.healHp) + " PV");
  if (d.healMp) v.push_back(d.healMp >= 999 ? "Rend tous les PM" : "Rend " + std::to_string(d.healMp) + " PM");
  if (d.revive) v.push_back("Relève un allié K.O. avec " + std::to_string(d.revive) + " % de ses PV");
  if (d.cure) v.push_back("Guérit poison, brûlure, paralysie et sommeil");
  if (d.capture > 0) {
    double lo = std::min(R.capMax, R.capBase * d.capture), hi = std::min(R.capMax, (R.capBase + R.capHp) * d.capture);
    v.push_back("Capture : " + pct(lo) + " de chance sur une créature en pleine forme, jusqu'à " + pct(hi) + " presque K.O.");
  }
  for (size_t L = 0; L < R.forge.size(); L++)  // matériau de la forge
    for (auto& [id, n] : R.forge[L].items)
      if (id == d.id || (id == "trophee" && d.trophy))
        v.push_back("Forge de Forgeroc : " + std::to_string(n) + " pour un objet +" + std::to_string(L + 1));
  for (auto& td : R.typeDrops)
    if (td.item == d.id) {
      std::string tn = td.type;
      for (auto& t : types())
        if (t.id == td.type) tn = t.name;
      v.push_back("Butin des créatures " + tn + " (" + pct(td.chance) + ")");
    }
  if (d.slot >= 0 && forgeable(d)) v.push_back("Peut être renforcé à la forge de Forgeroc (+" + std::to_string(d.forged + 1) + ")");
  if (d.key && d.trophy) v.push_back("Trophée");
  else if (d.key) v.push_back("Objet important");
  else if (d.slot < 0 && (d.battle || d.field))
    v.push_back(d.battle && d.field ? "En combat et dans le menu" : d.battle ? "En combat seulement" : "Dans le menu seulement");
  std::string s;
  for (auto& x : v) s += (s.empty() ? "" : ". ") + x;
  return s.empty() ? s : s + ".";
}

// Forces et faiblesses d'une espèce : efficacité de chaque type d'attaque (ses deux types et ses
// résistances propres), résistances aux coups physiques ou à la magie, états impossibles
std::string matchupText(const Species& sp) {
  std::map<float, std::string, std::greater<float>> by;
  auto r = [&](const std::string& k) {
    auto it = sp.resist.find(k);
    return it == sp.resist.end() ? 1.f : it->second;
  };
  for (Type t = 0; t < (Type)types().size(); t++) {
    float e = typeEff(t, sp.type) * (sp.type2 >= 0 ? typeEff(t, sp.type2) : 1.f) * r(types()[t].id);
    if (e != 1) by[e] += (by[e].empty() ? "" : ", ") + types()[t].name;
  }
  std::string weak, strong, none;
  for (auto& [e, names] : by) {
    std::string mult = " (×" + numFr(e) + ")";
    if (e == 0) none = names;
    else if (e > 1) weak += (weak.empty() ? "" : ", ") + names + mult;
    else strong += (strong.empty() ? "" : ", ") + names + mult;
  }
  std::string s = "Faible contre : " + (weak.empty() ? std::string("rien") : weak) + ".";
  if (!strong.empty()) s += " Résiste : " + strong + ".";
  if (!none.empty()) s += " Sans effet : " + none + ".";
  if (r("physique") != 1) s += std::string(r("physique") < 1 ? " Encaisse" : " Craint") + " les coups physiques (×" + numFr(r("physique")) + ").";
  if (r("magique") != 1) s += std::string(r("magique") < 1 ? " Encaisse" : " Craint") + " la magie (×" + numFr(r("magique")) + ").";
  std::string imm;
  for (Status st : {Status::Poison, Status::Burn, Status::Paralysis, Status::Sleep})
    if (immuneTo(sp, st)) imm += (imm.empty() ? "" : ", ") + std::string(statusName(st));
  if (!imm.empty()) s += " Insensible : " + imm + ".";
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
  for (auto& a : allies) a->atb = 20 + frand() * 50;
  for (auto& e : foes) e->atb = frand() * 30;
  G.menus.clear();
  std::string intro;
  FighterP leader = foes[0];
  for (auto& e : foes)
    if (e->boss) leader = e;
  if (!foeName.empty()) intro = foeName + " vous défie !";
  else if (leader->S().human) intro = leader->name() + " engage le combat !";
  else if (boss) intro = leader->name() + " surgit devant vous !";
  else if (foes.size() > 1) intro = std::to_string(foes.size()) + " créatures sauvages surgissent !";
  else intro = leader->name() + " sauvage apparaît !";
  sc.say(intro, 1.3f);
}

bool Battle::isAlly(const FighterP& f) const { return std::find(allies.begin(), allies.end(), f) != allies.end(); }
std::vector<FighterP> Battle::alive(const std::vector<FighterP>& v) const {
  std::vector<FighterP> o;
  for (auto& f : v)
    if (f->alive()) o.push_back(f);
  return o;
}

// Damier à l'écran : les alliés à droite, les ennemis à gauche ; l'avant de chaque camp est du
// côté du milieu de l'écran. Les lignes du bas sont un peu plus près (léger effet de profondeur).
Pt Battle::cellPos(bool allySide, int c) const {
  if (c < 0 || c >= N_CELLS) c = cellAt(1, 1);
  float col = (float)cellCol(c), row = (float)cellRow(c);
  float x = 196 + col * 30 + row * 4;  // 4 colonnes de 30 px, chaque ligne un peu décalée (perspective)
  return {allySide ? x : 320 - x, 58 + row * 26};
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
enum class Dock { Command, Above, AboveLeft };
static void dock(Menu& t, Dock d = Dock::Above) {
  int th = t.title.empty() ? 0 : 13, h = std::min(t.rows, (int)t.items.size()) * 12 + 8 + th;
  if (d == Dock::Command) t.x = 4, t.w = 96, t.y = 238 - h;
  else t.x = d == Dock::Above ? 102 : 4, t.w = 214, t.y = 184 - h;
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

void Battle::startAnim(const FighterP& a, const std::vector<FighterP>& targets, Fx fx, Color col, bool limit) {
  anim_ = Anim{a, targets, fx, col, limit, G.time};
  if (net_ == Net::Host || net_ == Net::Lead) {
    Json c = Json::array();
    for (auto& f : targets) c.push_back(mine(f));
    send_(-1, Json{{"t", "anim"}, {"a", mine(a)}, {"c", c}, {"fx", (int)fx}, {"col", Json::array({col.r, col.g, col.b})}, {"l", limit}});
  }
}

// Élan : le lanceur avance vers ses adversaires (coup) ou fait un petit bond (sort, soin) ; le
// combattant touché tremble un instant
Pt Battle::offset(const FighterP& f) const {
  Pt o{0, 0};
  float e = G.time - anim_.t0;
  if (anim_.actor == f && e >= 0 && e < .4f) {
    float k = std::sin(3.14159f * e / .4f);
    if (anim_.fx == Fx::Hit) o.x = (isAlly(f) ? -16.f : 16.f) * k;
    else o.y = -6 * k;
  }
  if (blink == f && G.time - blinkT < .3f) o.x += std::sin((G.time - blinkT) * 70) * 2.5f;
  return o;
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
    if (anim_.limit)  // Limite : grande étoile dorée
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
void Battle::update(float dt) {
  if (finished_) return;
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
    G.menus.update(G.in);
    return;
  }
  if (follower() || remoteTurn_) return;  // invité : l'hôte fait avancer le temps ; hôte : ordre attendu
  tickATB(dt);
}

void Battle::tickATB(float dt) {
  const Rules& R = rules();
  std::vector<FighterP> all = alive(allies);
  for (auto& e : alive(foes)) all.push_back(e);
  for (auto& f : all) {
    f->atb += dt * float((R.atbBase + f->eSpd()) * R.atbSpeed * (companion(f) ? R.companionAtb : 1));
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
  if (f->status == Status::Sleep) {
    actor = f;
    log.push_back("  " + f->name() + " dort");
    if (--f->statusTurns <= 0) {
      f->status = Status::None;
      sc.say(f->name() + " se réveille !", .8f);
    } else sc.say(f->name() + " dort profondément…", .7f);
    sc.call([this, f] { afterAction(f); });
    return true;
  }
  if (f->status == Status::Paralysis && frand() < rules().paraSkip) {
    actor = f;
    log.push_back("  " + f->name() + " paralysé");
    sc.say(f->name() + " est paralysé et ne peut pas bouger !", .8f);
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
  m.items.push_back({"Attaquer", "", "Utiliser une technique.", true, [this, a] {
                       Menu t;
                       t.title = "Techniques";
                       t.rows = 5;
                       for (auto& id : a->techs()) {
                         const Move& mv = moveInfo(id);
                         t.items.push_back({mv.name, typeName(mv.type), moveHelp(mv, a.get()), true, [this, a, id] { moveTarget(a, id); }});
                       }
                       dock(t);
                       G.menus.push(t);
                     }});
  auto sp = a->spells();
  if (!sp.empty())
    m.items.push_back({"Magie", "", "Lancer un sort (coûte des PM).", true, [this, a, sp] {
                         Menu t;
                         t.title = "Magie  " + std::to_string(a->mp) + "/" + std::to_string(a->mmp) + " PM";
                         t.rows = 5;
                         for (auto& id : sp) {
                           const Move& mv = moveInfo(id);
                           t.items.push_back({mv.name, std::to_string(mv.cost) + " PM", moveHelp(mv, a.get()), a->mp >= mv.cost,
                                              [this, a, id] { moveTarget(a, id); }});
                         }
                         dock(t);
                         G.menus.push(t);
                       }});
  if (!a->S().limit.empty()) {
    const Move& lim = moveInfo(a->S().limit);
    m.items.push_back({"Limite", "", (a->lim >= 100 ? lim.name + " : technique ultime ! " : "La jauge Limite (rose) se remplit quand on reçoit des coups. " + lim.name + " : ") +
                           moveDetails(lim, a.get()),
                       a->lim >= 100, [this, a, lim] {
                         G.menus.clear();
                         useMove(a, lim.id, lim.target == Target::AllAllies ? alive(allies) : alive(foes), true);
                       }});
  }
  m.items.push_back({"Objet", "", "Utiliser un objet du sac.", true, [this, a] {
                       Menu t;
                       t.title = "Objets";
                       t.rows = 5;
                       bool creature = false;
                       for (auto& f : alive(foes)) creature = creature || !f->S().human;
                       for (auto& d : allItems()) {
                         int n = G.items.count(d.id) ? G.items[d.id] : 0;
                         if (!d.battle || n <= 0) continue;
                         std::string id = d.id;
                         bool ok = true;
                         bool lantern = d.capture > 0;
                         std::string help = itemHelp(d);
                         if (lantern) {
                           ok = canCapture && creature;
                           if (!canCapture) help = "Impossible de capturer ici.";
                           else if (!creature) help = "On ne capture pas une personne !";
                           else if ((int)G.team.size() >= rules().maxTeam) help += " Équipe complète : la créature ira dans la réserve.";
                         }
                         if (d.revive) ok = alive(allies).size() < allies.size();
                         t.items.push_back({d.name, "×" + std::to_string(n), help, ok, [this, a, id, lantern] {
                                              if (lantern) {
                                                pickFoe("Capturer", [id](const Fighter& f) {
                                                  return "Chance de capture : " + std::to_string(int(captureChance(f, item(id).capture) * 100)) + " %";
                                                }, [this, a, id](FighterP t) { useItem(a, id, t); }, true);
                                              } else pickAlly("Sur qui ?", item(id).revive > 0, [this, a, id](FighterP t) { useItem(a, id, t); });
                                            }});
                       }
                       if (t.items.empty()) t.items.push_back({"(sac vide)", "", "", false, nullptr});
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
  m.items.push_back({"Se déplacer", "", freeCells.empty() ? "Aucune case libre." : "Changer de case sur le damier (action rapide).", !freeCells.empty(),
                     [this, a, freeCells] {
                       Menu t;
                       t.title = "Se déplacer";
                       t.rows = 5;
                       t.onCancel = [this] {
                         moveCell_ = -1;
                         G.menus.pop();
                       };
                       for (int c : freeCells)
                         t.items.push_back({cellName(c), "", "Avant : frappe et reçoit plus fort au corps à corps. Arrière : protégé.", true,
                                            [this, a, c] { moveTo(a, c); }, [this, c] { moveCell_ = c; }});
                       dock(t, Dock::AboveLeft);
                       G.menus.push(t);
                     }});
  std::vector<FighterP> bench;  // avec des héros : un héros n'est remplacé que par un héros
  for (auto& r : G.team)
    if (r->alive() && !isAlly(r) && (!hasHero_ || r->S().human == a->S().human)) bench.push_back(r);
  m.items.push_back({"Changer", "", bench.empty() ? "Personne pour vous remplacer." : "Faire entrer un remplaçant de l'équipe.", !bench.empty(),
                     [this, a, bench] {
                       Menu t;
                       t.title = "Remplaçants";
                       t.rows = 5;
                       for (auto& r : bench)
                         t.items.push_back({r->name() + " N." + std::to_string(r->lvl), std::to_string(r->hp) + "/" + std::to_string(r->mhp),
                                            typesName(r->S()) + " · " + std::to_string(r->mp) + " PM", true, [this, a, r] { swapIn(a, r); }});
                       dock(t);
                       G.menus.push(t);
                     }});
  if (canFlee) m.items.push_back({"Fuir", "", "Tenter de fuir avec toute l'équipe.", true, [this, a] { tryFlee(a); }});
  // Duel : ni objets ni remplaçants (l'équipement compte) ; gardien à plusieurs : pas de remplaçants
  if (net_ != Net::None) {
    bool d = duel();
    m.items.erase(std::remove_if(m.items.begin(), m.items.end(),
                                 [d](const MenuItem& it) { return (d && it.label == "Objet") || it.label == "Changer"; }),
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
  for (auto& f : alive(foes)) {
    if (creaturesOnly && f->S().human) continue;
    if (aiming_ && !reachable(actor, *aiming_, f)) continue;  // corps à corps : seulement l'avant
    t.items.push_back({f->name(), "N." + std::to_string(f->lvl), info(*f), true, [done, f] { done(f); }, [this, f] { cursor = f; }});
  }
  dock(t);
  G.menus.push(t);
}
void Battle::pickAlly(const std::string& title, bool ko, std::function<void(FighterP)> done) {
  Menu t;
  t.title = title;
  t.rows = 6;
  t.onCancel = [this] {
    cursor = nullptr;
    G.menus.pop();
  };
  for (auto& f : allies) {
    bool ok = ko ? !f->alive() : f->alive();
    std::string st = f->status != Status::None ? std::string(" · ") + statusName(f->status) : "";
    t.items.push_back({f->name(), std::to_string(f->hp) + "/" + std::to_string(f->mhp),
                       std::to_string(f->mp) + "/" + std::to_string(f->mmp) + " PM" + (f->alive() ? st : " · K.O."), ok, [done, f] { done(f); },
                       [this, f] { cursor = f; }});
  }
  dock(t, Dock::AboveLeft);
  G.menus.push(t);
}

// Fiche rapide d'un ennemi visé : types, PV, état, bonus/malus et efficacité
static std::string foeInfo(const Fighter& f, const Move* m) {
  std::string s = typesName(f.S()) + " · " + std::to_string(f.hp) + "/" + std::to_string(f.mhp) + " PV";
  if (f.status != Status::None) s += " · " + std::string(statusName(f.status));
  for (int st = 0; st < N_STAGES; st++)
    if (f.stage[st]) s += " · " + std::string(stageName(st)) + (f.stage[st] > 0 ? " +" : " ") + std::to_string(f.stage[st]);
  if (m && m->damaging()) {
    float e = moveEff(*m, f.S());
    if (e == 0) s += " · aucun effet";
    else if (e > 1) s += " · super efficace";
    else if (e < 1) s += " · peu efficace";
  }
  return s;
}

void Battle::moveTarget(FighterP a, const std::string& id) {
  const Move& m = moveInfo(id);
  switch (m.target) {
    case Target::Enemy:
      aiming_ = &m;
      pickFoe("Cible", [&m](const Fighter& f) { return foeInfo(f, &m); }, [this, a, id](FighterP t) { useMove(a, id, {t}); });
      break;
    case Target::AllEnemies: useMove(a, id, alive(foes)); break;
    case Target::Ally: pickAlly("Sur qui ?", false, [this, a, id](FighterP t) { useMove(a, id, {t}); }); break;
    case Target::AllAllies: useMove(a, id, alive(allies)); break;
    case Target::AllyKO:
      if (alive(allies).size() == allies.size()) {
        sc.say("Personne n'est K.O.", .8f);
        return;
      }
      pickAlly("Qui relever ?", true, [this, a, id](FighterP t) { useMove(a, id, {t}); });
      break;
  }
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
  auto consider = [&](const std::string& id, std::vector<FighterP> tg, float score) {
    score *= .85f + frand() * .3f;
    if (!tg.empty() && score > bestScore) bestScore = score, best = {id, tg, false};
  };
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
  float score = m.power * moveEff(m, t->S()) * (hasType(a->S(), m.type) ? float(R.stab) : 1.f) * A / D * hit;
  if (m.melee) {  // depuis l'arrière, ou sur une cible à l'arrière : moins fort
    if (cellCol(std::max(0, cellOf(a))) == BACK_COL) score *= float(R.backRow);
    if (cellCol(std::max(0, cellOf(t))) == BACK_COL) score *= float(R.backRow);
  }
  if (!ally) return score / paceTime(m);
  score *= 1 + std::min(1.f, (a->crit + m.critBonus) / 100.f) * float(R.critMult - 1);
  if (score >= t->hp * 1.2f) score += 15;  // achever un adversaire affaibli
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
  useMove(a, p.move, p.targets, p.limit);
}

void Battle::enemyTurn(FighterP e) {
  actor = e;
  if (bossPlan(e) || enemyMove(e)) return;
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
        log.push_back(e->name() + " : " + moveInfo(w.move).name + " (annoncée), personne dans la zone");
        sc.say(e->name() + " déchaîne " + moveInfo(w.move).name + "… mais tout le monde s'est écarté !", 1.2f);
        return false;  // il joue tout de même une attaque normale
      }
      log.push_back(e->name() + " : " + moveInfo(w.move).name + " (annoncée) sur " + std::to_string(hit.size()));
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
  log.push_back(e->name() + " annonce " + moveInfo(best).name + " (" + zoneName(zone) + ", " + cellName(center) + ")");
  sound("statut");
  sc.say(e->name() + " prépare " + moveInfo(best).name + " ! Écartez-vous de la zone rouge.", 1.4f);
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
  log.push_back(a->name() + " : tactique " + std::to_string(p.tactic + 1));
  if (p.cell >= 0) {  // se mettre à l'abri
    moveTo(a, p.cell);
    return true;
  }
  if (!p.item.empty()) useItem(a, p.item, p.targets.at(0));
  else useMove(a, p.move, p.targets, p.limit);
  return true;
}

bool Battle::tacticPlan(FighterP a, Plan& out) {
  struct Act {
    const Move* m = nullptr;  // technique, sort ou Limite
    std::string item;         // ou objet du sac
    bool limit = false;
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
        if (x.m) score -= x.m->cost * .3f;
        if (found && (key > bestKey + 1e-4f || (key > bestKey - 1e-4f && score <= bestScore))) continue;
        found = true;
        bestKey = key, bestScore = score;
        out = Plan{};
        out.tactic = i;
        if (x.m) out.move = x.m->id, out.limit = x.limit;
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
  if (frand() >= hit) {
    pop(d, "Raté", GREY);
    sound("rate");
    log.push_back(a->name() + " > " + d->name() + " : " + m.name + " ratée");
    return -1;
  }
  float eff = moveEff(m, d->S());
  if (eff <= 0) {
    pop(d, "Aucun effet", GREY);
    sound("rate");
    return 0;
  }
  float A = magic ? a->eMag() : a->eAtk();
  float D = magic ? d->eRes() : d->eDef();
  float stab = hasType(a->S(), m.type) ? float(R.stab) : 1.f;
  float spread = float(R.spreadMin) + frand() * float(1 - R.spreadMin);
  bool crit = frand() * 100 < a->crit + m.critBonus;
  float k = crit ? float(R.critMult) : 1.f;
  if (m.melee) {  // damier : l'arrière frappe et reçoit moins fort au corps à corps
    if (cellCol(std::max(0, cellOf(a))) == BACK_COL) k *= float(R.backRow);
    if (cellCol(std::max(0, cellOf(d))) == BACK_COL) k *= float(R.backRow);
  }
  k *= mult;
  int dmg = std::max(1, int(((2 * a->lvl / 5.f + 2) * m.power * A / std::max(1.f, D) / float(R.dmgDivisor) + 2) * stab * eff * spread * k));
  d->hp = std::max(0, d->hp - dmg);
  setBlink(d);
  // Bruitage : sort selon l'élément (« sort_feu »…), coup selon le rythme (rapide, normal, lourd)
  std::string fx = crit ? "critique" : magic ? "sort_" + types()[m.type].id : m.pace == Pace::Quick ? "coup_rapide" : m.pace == Pace::Heavy ? "coup_lourd" : "coup";
  if (!audio::has(fx)) fx = magic ? "sort" : "coup";
  sound(fx);
  pop(d, std::to_string(dmg), eff > 1 ? GOLD : eff < 1 ? GREY : WHITE);
  if (crit) pop(d, "Critique !", RED);
  if (eff > 1) pop(d, "Efficace !", GOLD);
  else if (eff < 1) pop(d, "Résiste", GREY);
  if (isAlly(d) || duel()) d->lim = std::min(100.f, d->lim + dmg * float(R.limitGain) / d->mhp);  // en duel, les deux équipes
  log.push_back(a->name() + " > " + d->name() + " : " + m.name + " " + std::to_string(dmg) + (crit ? " critique" : "") +
                (eff > 1 ? " efficace" : eff < 1 ? " résiste" : "") + " (reste " + std::to_string(d->hp) + "/" + std::to_string(d->mhp) + ")");
  return dmg;
}

void Battle::applyEffect(const FighterP& t, const Effect& e) {
  if (!t->alive() || frand() * 100 >= e.chance) return;
  if (e.cure && t->status != Status::None) {
    t->status = Status::None;
    pop(t, "Guéri", GREEN);
  }
  if (e.status != Status::None) {
    if (immuneTo(t->S(), e.status)) {
      if (e.chance >= 100) pop(t, "Insensible", GREY);
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
  log.push_back("  " + f->name() + " K.O.");
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
  startAnim(a, targets, fxOf(m), rgb(types()[m.type].color), isLimit);
  sc.say(isLimit ? "LIMITE — " + a->name() + " : " + m.name + " !" : a->name() + " : " + m.name, isLimit ? .9f : .55f);
  auto ko = std::make_shared<std::vector<FighterP>>();
  sc.call([this, a, targets, center, &m, isLimit, ko, boost] {
    if (isLimit) sound("limite");
    if (m.kind == Kind::Heal || m.kind == Kind::Revive) sound("soin");
    if (isLimit || m.target == Target::AllEnemies) {
      flashT = G.time;
      flashCol = m.kind == Kind::Heal ? rgb(0xfff4c0) : isLimit ? rgb(0xff8ac8) : rgb(0xffffff);
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
          pop(t, "Raté", GREY);
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
        else
          for (auto& e : m.effects)
            if (!e.self) applyEffect(t, e);
      }
    }
    if (landed || m.kind == Kind::Status)
      for (auto& e : m.effects)
        if (e.self) applyEffect(a, e);
    for (auto& f : *ko) knockOut(f);
  });
  sc.wait(.75f);
  sc.call([this, ko] {
    if (!ko->empty()) sound("ko");
    for (auto& f : *ko) sc.say(f->name() + (isAlly(f) || duel() ? " est K.O. !" : " est vaincu !"), .75f);
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
  sc.say(a->name() + " utilise : " + d.name, .7f);
  sc.call([this, a, id, t] {
    const ItemDef& d = item(id);
    sound(d.capture > 0 ? "lancer" : "soin");
    if (d.revive) {
      t->clearBattle();
      t->hp = std::max(1, t->mhp * d.revive / 100);
      t->atb = 0;
      pop(t, "+" + std::to_string(t->hp), GREEN);
      sc.say(t->name() + " se relève !", .9f);
    } else if (d.capture <= 0) {
      if (d.healHp) {
        int amt = std::min(d.healHp, t->mhp - t->hp);
        t->hp += amt;
        pop(t, "+" + std::to_string(amt), GREEN);
      }
      if (d.healMp) {
        int amt = std::min(d.healMp, t->mmp - t->mp);
        t->mp += amt;
        pop(t, "+" + std::to_string(amt) + " PM", BLUE);
      }
      if (d.cure && t->status != Status::None) {
        t->status = Status::None;
        pop(t, "Guéri", GREEN);
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
        sc.say("Capturé ! " + t->name() + (full ? " est envoyé dans la réserve (Échap > Réserve)." : " rejoint l'équipe."), 1.5f);
      } else {
        sc.call([this] { sound("capture_rate"); });
        sc.say(t->name() + " s'échappe de la lumière !", 1.0f);
      }
    }
  });
  sc.wait(.4f);
  sc.call([this, a] { afterAction(a); });
}

void Battle::moveTo(FighterP a, int cell) {
  G.menus.clear();
  moveCell_ = -1;
  if (follower()) {  // l'invité envoie son ordre ; l'hôte (ou le chef) vérifie et déplace
    send_(net_ == Net::Follow ? leader_ : -1, Json{{"t", "ordre"}, {"i", mine(a)[1]}, {"deplacer", cell}});
    actor = nullptr;
    return;
  }
  cells_[a.get()] = cell;
  log.push_back(a->name() + " se déplace : " + cellName(cell));
  sc.say(a->name() + " se déplace.", .45f);
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
  sc.say(a->name() + " recule. " + r->name() + " entre au combat !", 1.1f);
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
      sc.say("L'équipe prend la fuite !", 1.0f);
      sc.call([this] { finish(BattleResult::Fled); });
    } else {
      sc.say("Impossible de fuir !", .9f);
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
    sc.say(a->name() + (poison ? " souffre du poison." : " souffre de sa brûlure."), .6f);
    if (!a->alive()) {
      knockOut(a);
      sc.say(a->name() + (isAlly(a) || duel() ? " est K.O. !" : " est vaincu !"), .75f);
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
    sc.say(names_[won ? 0 : 1] + " remporte le duel !", 1.8f);
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
      sc.say(foeName.empty() ? n->name() + " entre dans le combat !" : foeName + " envoie " + n->name() + " !", 1.1f);
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
    sc.say("Toute l'équipe est à terre…", 1.6f);
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
        summary_.push_back(a->name() + " : +" + std::to_string(share) + " exp" + (a->lvl > before ? " · niveau " + std::to_string(a->lvl) + " !" : ""));
        for (auto& m : msgs)
          if (m.find(" apprend ") != std::string::npos) summary_.push_back("   " + m);
        up = up || a->lvl > before;
      }
      bool others = false;
      for (auto& m : G.team)
        if (!isAlly(m) && m->alive()) gainXp(*m, res), others = true;
      if (others && res > 0) summary_.push_back("Le reste de l'équipe : +" + std::to_string(res) + " exp");
    }
    if (goldGain > 0) {
      G.gold += goldGain;
      summary_.push_back("Or : +" + std::to_string(goldGain));
    }
    if (!boss) {
      // Un seul objet au plus : les chances s'additionnent dans l'ordre de la liste
      float r = frand(), acc = 0;
      for (auto& [id, chance] : R.drops) {
        acc += float(chance);
        if (r < acc) {
          G.items[id]++;
          summary_.push_back("Objet trouvé : " + item(id).name);
          break;
        }
      }
      // Matériaux de la forge : selon le type de chaque créature vaincue
      for (auto& f : roster_)
        if (!f->S().human)
          for (auto& td : R.typeDrops)
            if (hasType(f->S(), typeOf(td.type)) && frand() < float(td.chance)) {
              G.items[td.item]++;
              summary_.push_back("Objet trouvé : " + item(td.item).name);
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
  bool blinking = blink && t - blinkT < .42f && int((t - blinkT) / .07f) % 2 == 0;
  // Damier : une marque au sol par case ; la zone de la technique visée et la case choisie pour se déplacer en or
  for (int side = 0; side < 2; side++)
    for (int c = 0; c < N_CELLS; c++) {
      Pt p = cellPos(side == 1, c);
      // une dalle claire en losange, vue en perspective
      g.poly({{p.x - 14, p.y + 13}, {p.x, p.y + 8}, {p.x + 14, p.y + 13}, {p.x, p.y + 18}}, rgb(0xffffff, 34));
      g.poly({{p.x - 10, p.y + 13}, {p.x, p.y + 9.5f}, {p.x + 10, p.y + 13}, {p.x, p.y + 16.5f}}, rgb(0x000000, 18));
    }
  if (aiming_ && cursor && cursor->alive())
    for (int c = 0; c < N_CELLS; c++)
      if (inZone(aiming_->zone, cellOf(cursor), c)) {
        Pt p = cellPos(isAlly(cursor), c);
        g.ellipse(p.x, p.y + 13, 13, 5, rgb(0xffd34d, uint8_t(c == cellOf(cursor) ? 190 : 110)));
      }
  if (moveCell_ >= 0) {
    Pt p = cellPos(true, moveCell_);
    g.ellipse(p.x, p.y + 13, 13, 5, rgb(0xffd34d, uint8_t(150 + 60 * std::sin(t * 8))));
  }
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
  for (auto& f : shown) {
    Pt p = pos(f);
    Pt o = offset(f);
    if (!isAlly(f)) {  // ennemi : barre de PV au-dessus, nom sous le curseur
      float s = f->boss ? 1.45f : f->sp == "brumelin" ? .85f : 1.f;
      if (!(blinking && blink == f)) drawFighterSprite(g, *f, p.x + o.x, p.y + o.y, s, true, t + p.x * .01f);
      float bw = f->boss ? 24 : 15, by = p.y - (f->boss ? 42 : 26);
      g.rect(p.x - bw - 1, by - 1, bw * 2 + 2, 4, rgb(0x0a0f33));
      g.rect(p.x - bw, by, bw * 2 * f->hp / f->mhp, 2, rgb(0xff8a7a));
      drawStatusTag(*f, p.x, by - 10);
      if (cursor == f) g.cursor(p.x - 3, by - 12 + std::sin(t * 8));  // son nom est dans le menu de la cible
      continue;
    }
    if (!f->alive()) g.alpha = .35f;
    if (!(blinking && blink == f)) drawFighterSprite(g, *f, p.x + o.x, p.y + o.y, 1, false, t + p.x * .01f);
    g.alpha = 1;
    drawStatusTag(*f, p.x - 26, p.y - 18);
    if (actor == f) g.tri(p.x - 4, p.y - 34, p.x + 4, p.y - 34, p.x, p.y - 28, rgb(0xffd34d));
    if (cursor == f) g.cursor(p.x - 24, p.y - 4);
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
  else if (!choosing().empty()) top = choosing() + " choisit…";
  if (!top.empty()) {
    auto lines = Gfx::wrap(top, 300);
    g.window(4, 2, 312, 7 + 11 * (int)lines.size());
    for (size_t i = 0; i < lines.size(); i++) g.text(160, 5 + i * 11, lines[i], WHITE, 1);
  }
  // ATH discret, à droite du bas de l'écran (à gauche : le menu de commande) : deux colonnes de
  // trois alliés ; pour chacun, nom et PV, puis quatre traits : PV (vert), PM (bleu), jauge ATB
  // (blanche, or quand elle est pleine) et Limite (rose). Au-dessus : renforts et mode auto.
  Color lab = rgb(0xaab3d8);
  if (!reserve.empty()) g.text(6, 176, "Renforts : " + std::to_string(reserve.size()), GREY);
  bool flash = t - toggledT_ < 1.2f && int(t * 8) % 2;  // Tab : mode auto activé ou coupé
  if (G.menus.depth() <= 1)  // caché par un sous-menu (au-dessus de l'ATH)
    g.text(314, 176, G.tacticsAuto ? "Auto (Tab)" : "Manuel (Tab)", flash ? WHITE : G.tacticsAuto ? GOLD : lab, 2);
  const float hx = 104, hy = 189;
  g.rect(hx - 4, hy - 3, g.right() - hx + 4, SCREEN_H - hy + 3, rgb(0x060a24, 165));
  for (size_t i = 0; i < allies.size() && i < 6; i++) {
    auto& a = allies[i];
    float x = hx + (i / 3) * 108.f, y = hy + (i % 3) * 16.f;
    if (actor == a) g.rect(x - 2, y - 2, 106, 16, rgb(0xffffff, 30));
    // Gardien à plusieurs : les combattants des autres joueurs en bleu ; compagnon : en retrait
    Color nc = !a->alive() ? rgb(0xff7b6b) : a->status != Status::None ? rgb(statusColor(a->status)) : !mineToCommand(a) ? BLUE : actor == a ? GOLD : WHITE;
    g.text(x + (companion(a) ? 4 : 0), y, utf8Prefix(a->name(), companion(a) ? 6 : 7), nc);
    g.text(x + 102, y, std::to_string(a->hp) + "/" + std::to_string(a->mhp), a->hp * 4 < a->mhp ? GOLD : WHITE, 2);
    float by = y + 11;
    auto line = [&](float bx, float w, float r, Color c) {
      g.rect(bx, by, w, 2, rgb(0x2a3270));
      g.rect(bx, by, w * std::clamp(r, 0.f, 1.f), 2, c);
    };
    line(x, 28, float(a->hp) / a->mhp, GREEN);
    line(x + 32, 18, a->mmp ? float(a->mp) / a->mmp : 0, BLUE);
    // Après une technique lourde, la jauge part en dessous de zéro : trait rouge qui se vide
    if (a->alive() && a->atb < 0) line(x + 54, 24, -a->atb / 100, RED);
    else line(x + 54, 24, a->alive() ? a->atb / 100 : 0, a->atb >= 100 ? GOLD : rgb(0xe8ecff));
    line(x + 82, 18, a->lim / 100, a->lim >= 100 && int(t * 5) % 2 ? WHITE : rgb(0xff6fb0));
  }
  if (summaryOn_) {  // fenêtre des gains de la victoire
    int h = 36 + 11 * (int)summary_.size(), y = std::max(26, 92 - h / 2);
    g.window(30, y, 260, h);
    g.text(160, y + 5, "Victoire !", GOLD, 1);
    for (size_t i = 0; i < summary_.size(); i++) g.text(38, y + 19 + i * 11, summary_[i], WHITE);
    g.text(160, y + h - 12, "Entrée : continuer", lab, 1);
  }
  G.menus.draw(g, t);
  float k = (t - start) / .35f;
  if (k < 1) g.rect(g.left(), 0, g.fullW, SCREEN_H, rgb(0xffffff, uint8_t(255 * (1 - k))));
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
  sc.clear();
  if (mode == Net::Host) sc.say("Duel : " + me + " contre " + other + " !", 1.3f);
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
      a.push_back(Json::array({f->hp, f->mp, (int)std::lround(f->atb), (int)std::lround(f->lim), (int)f->status, st, cellOf(f)}));
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
  return Json{{"t", "etat"}, {"a", pack(allies)}, {"b", pack(foes)}, {"msg", sc.showingMessage() ? sc.text() : std::string()}, {"attente", wait},
              {"w", warn}};
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
        anim_ = Anim{a, c, Fx(std::clamp(jget(m, "fx", 0), 0, 4)), cc, jget(m, "l", false), G.time};
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
        sc.say("Toute l'équipe est à terre…", 1.6f);
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
