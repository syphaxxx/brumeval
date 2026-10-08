#include "battle.hpp"

#include <algorithm>
#include <cmath>
#include <map>

#include "game.hpp"
#include "sprites.hpp"
#include "tactics.hpp"

static const Color WHITE = rgb(0xffffff), GOLD = rgb(0xffe066), GREY = rgb(0xb9c0de), GREEN = rgb(0x7dffa8), BLUE = rgb(0x8fd0ff),
                   RED = rgb(0xff8a7a);

// ---------------------------------------------------------------------------
// Textes d'aide
// ---------------------------------------------------------------------------
static std::string effectText(const Effect& e) {
  std::string s;
  if (e.cure) s = "guérit les états";
  if (e.status != Status::None) s = statusName(e.status);
  if (e.stat >= 0) s = std::string(stageName(e.stat)) + (e.stages > 0 ? " +" : " ") + std::to_string(e.stages) + (e.self ? " (lanceur)" : "");
  if (e.chance < 100) s += " " + std::to_string(e.chance) + " %";
  return s;
}

std::string moveDetails(const Move& m) {
  static const char* KIND[] = {"Physique", "Magique", "Soin", "Réanimation", "Statut"};
  std::string s = std::string(KIND[(int)m.kind]) + " · " + typeName(m.type);
  if (m.damaging()) s += " · Puissance " + std::to_string(m.power);
  if (m.acc < 100) s += " · Précision " + std::to_string(m.acc) + " %";
  if (m.critBonus > 0) s += " · Critique +" + std::to_string(m.critBonus) + " %";
  for (auto& e : m.effects) s += " · " + effectText(e);
  if (m.pace == Pace::Quick) s += " · Rapide : rejoue plus tôt";
  if (m.pace == Pace::Heavy) s += " · Lourde : rejoue plus tard";
  return s;
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
  allies = G.front();
  start = G.time;
  std::map<std::string, int> cnt, seen;
  for (auto* v : {&foes, &reserve})
    for (auto& e : *v) cnt[e->sp]++;
  for (auto* v : {&foes, &reserve})
    for (auto& e : *v)
      if (cnt[e->sp] > 1) e->tag = std::string(1, char('A' + seen[e->sp]++));
  for (auto& a : G.team) a->clearBattle();
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

Pt Battle::pos(const FighterP& f) const {
  static const Pt FOE[3][3] = {{{92, 96}}, {{72, 72}, {112, 122}}, {{58, 58}, {112, 96}, {58, 134}}};
  static const Pt ALLY[3] = {{238, 64}, {264, 100}, {238, 136}};
  for (size_t i = 0; i < foes.size(); i++)
    if (foes[i] == f) return FOE[std::min<size_t>(foes.size(), 3) - 1][std::min<size_t>(i, 2)];
  for (size_t i = 0; i < allies.size(); i++)
    if (allies[i] == f) return ALLY[std::min<size_t>(i, 2)];
  return {160, 100};
}
// Nombre ou texte flottant au-dessus d'un combattant ; ils s'empilent s'ils apparaissent ensemble
void Battle::pop(const FighterP& f, const std::string& t, Color c) {
  Pt p = pos(f);
  int stack = 0;
  for (auto& q : pops)
    if (q.x == p.x && G.time - q.t < .3f) stack++;
  pops.push_back({p.x, p.y - 22 - 11.f * stack, t, c, G.time});
  if (net_ == Net::Host) send_(Json{{"t", "pop"}, {"f", mine(f)}, {"txt", t}, {"c", Json::array({c.r, c.g, c.b})}});
}

// ---------------------------------------------------------------------------
void Battle::update(float dt) {
  if (finished_) return;
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
  if (net_ == Net::Guest || remoteTurn_) return;  // invité : l'hôte fait avancer le temps ; hôte : ordre attendu
  tickATB(dt);
}

void Battle::tickATB(float dt) {
  const Rules& R = rules();
  std::vector<FighterP> all = alive(allies);
  for (auto& e : alive(foes)) all.push_back(e);
  for (auto& f : all) {
    f->atb += dt * float((R.atbBase + f->eSpd()) * R.atbSpeed);
    if (f->atb >= 100) {
      f->atb = 100;
      if (skipTurn(f)) return;
      if (isAlly(f)) {
        if (tacticsActive() && tacticTurn(f)) return;
        if (autoPlay) autoCommand(f);
        else command(f);
      } else if (net_ == Net::Host) {  // combattant de l'invité : on attend son ordre
        remoteTurn_ = f;
        actor = f;
        send_(Json{{"t", "tour"}, {"i", mine(f)[1]}});
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
  m.x = 4, m.y = 160, m.w = 96, m.rows = 6, m.cancelable = false;
  m.items.push_back({"Attaquer", "", "Utiliser une technique.", true, [this, a] {
                       Menu t;
                       t.title = "Techniques";
                       t.x = 4, t.y = 82, t.w = 176, t.rows = 4;
                       for (auto& id : a->techs()) {
                         const Move& mv = moveInfo(id);
                         t.items.push_back({mv.name, typeName(mv.type), moveDetails(mv), true, [this, a, id] { moveTarget(a, id); }});
                       }
                       G.menus.push(t);
                     }});
  auto sp = a->spells();
  if (!sp.empty())
    m.items.push_back({"Magie", "", "Lancer un sort (coûte des PM).", true, [this, a, sp] {
                         Menu t;
                         t.title = "Magie  " + std::to_string(a->mp) + "/" + std::to_string(a->mmp) + " PM";
                         t.x = 4, t.y = 70, t.w = 190, t.rows = 5;
                         for (auto& id : sp) {
                           const Move& mv = moveInfo(id);
                           std::string help = mv.desc.empty() ? moveDetails(mv) : mv.desc + " (" + moveDetails(mv) + ")";
                           t.items.push_back({mv.name, std::to_string(mv.cost) + " PM", help, a->mp >= mv.cost,
                                              [this, a, id] { moveTarget(a, id); }});
                         }
                         G.menus.push(t);
                       }});
  if (!a->S().limit.empty()) {
    const Move& lim = moveInfo(a->S().limit);
    m.items.push_back({"Limite", "", a->lim >= 100 ? lim.name + " : technique ultime !" : "La jauge Limite se remplit quand on reçoit des coups.",
                       a->lim >= 100, [this, a, lim] {
                         G.menus.clear();
                         useMove(a, lim.id, lim.target == Target::AllAllies ? alive(allies) : alive(foes), true);
                       }});
  }
  m.items.push_back({"Objet", "", "Utiliser un objet du sac.", true, [this, a] {
                       Menu t;
                       t.title = "Objets";
                       t.x = 4, t.y = 82, t.w = 176, t.rows = 4;
                       bool creature = false;
                       for (auto& f : alive(foes)) creature = creature || !f->S().human;
                       for (auto& d : allItems()) {
                         int n = G.items.count(d.id) ? G.items[d.id] : 0;
                         if (!d.battle || n <= 0) continue;
                         std::string id = d.id;
                         bool ok = true;
                         bool lantern = d.capture > 0;
                         std::string help = d.desc;
                         if (lantern) {
                           ok = canCapture && creature && (int)G.team.size() < rules().maxTeam;
                           if (!canCapture) help = "Impossible de capturer ici.";
                           else if (!creature) help = "On ne capture pas une personne !";
                           else if (!ok) help = "L'équipe est complète (" + std::to_string(rules().maxTeam) + " membres).";
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
                       G.menus.push(t);
                     }});
  std::vector<FighterP> bench;
  for (auto& r : G.team)
    if (r->alive() && !isAlly(r)) bench.push_back(r);
  m.items.push_back({"Changer", "", bench.empty() ? "Personne en réserve." : "Faire entrer un membre de la réserve.", !bench.empty(),
                     [this, a, bench] {
                       Menu t;
                       t.title = "Réserve";
                       t.x = 4, t.y = 82, t.w = 176, t.rows = 4;
                       for (auto& r : bench)
                         t.items.push_back({r->name() + " N." + std::to_string(r->lvl), std::to_string(r->hp) + "/" + std::to_string(r->mhp),
                                            typesName(r->S()) + " · " + std::to_string(r->mp) + " PM", true, [this, a, r] { swapIn(a, r); }});
                       G.menus.push(t);
                     }});
  if (canFlee) m.items.push_back({"Fuir", "", "Tenter de fuir avec toute l'équipe.", true, [this, a] { tryFlee(a); }});
  if (net_ != Net::None)  // duel : ni objets ni remplaçants, à armes égales
    m.items.erase(std::remove_if(m.items.begin(), m.items.end(), [](const MenuItem& it) { return it.label == "Objet" || it.label == "Changer"; }),
                  m.items.end());
  G.menus.push(m);
}

void Battle::pickFoe(const std::string& title, std::function<std::string(const Fighter&)> info, std::function<void(FighterP)> done,
                     bool creaturesOnly) {
  Menu t;
  t.title = title;
  t.x = 4, t.y = 94, t.w = 166, t.rows = 3;  // assez large pour « Chevalier du givre »
  t.onCancel = [this] {
    cursor = nullptr;
    G.menus.pop();
  };
  for (auto& f : alive(foes)) {
    if (creaturesOnly && f->S().human) continue;
    t.items.push_back({f->name(), "N." + std::to_string(f->lvl), info(*f), true, [done, f] { done(f); }, [this, f] { cursor = f; }});
  }
  G.menus.push(t);
}
void Battle::pickAlly(const std::string& title, bool ko, std::function<void(FighterP)> done) {
  Menu t;
  t.title = title;
  t.x = 4, t.y = 94, t.w = 176, t.rows = 3;
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
            if (!focus || t == focus) consider(id, {t}, attackScore(a, m, t));
        break;
    }
  }
  if (best.move.empty()) best = {a->techs()[0], {theirs[irand(0, (int)theirs.size() - 1)]}, false};
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
  float A = magic ? a->eMag() : a->eAtk(), D = std::max(1.f, magic ? t->eRes() : t->eDef());
  float hit = std::clamp(m.acc / 100.f * a->acc / 100.f - (magic ? 0 : t->eva / 100.f), 0.f, 1.f);
  float score = m.power * moveEff(m, t->S()) * (hasType(a->S(), m.type) ? float(R.stab) : 1.f) * A / D * hit;
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
  Plan p = think(a);
  useMove(a, p.move, p.targets, p.limit);
}

void Battle::enemyTurn(FighterP e) {
  actor = e;
  Plan p = think(e);
  useMove(e, p.move, p.targets);
}

// ---------------------------------------------------------------------------
// Tactiques des alliés (voir tactics.hpp) : de haut en bas, la première règle
// dont la condition vise quelqu'un et dont l'action est possible est jouée.
// ---------------------------------------------------------------------------
bool Battle::tacticsActive() const { return autoPlay ? simTactics : G.tacticsAuto; }

bool Battle::tacticTurn(FighterP a) {
  if (!a->tacticsOn) return false;
  Plan p;
  if (!tacticPlan(a, p)) return false;
  actor = a;
  log.push_back(a->name() + " : tactique " + std::to_string(p.tactic + 1));
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
    if (d.capture > 0) return canCapture && !t->S().human && (int)G.team.size() < rules().maxTeam;
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
          else score = attackScore(a, *x.m, t);
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
int Battle::applyHit(const FighterP& a, const FighterP& d, const Move& m) {
  const Rules& R = rules();
  bool magic = m.kind == Kind::Magic;
  // Précision : l'esquive ne joue que contre les attaques physiques
  float hit = m.acc / 100.f * a->acc / 100.f - (magic ? 0 : d->eva / 100.f);
  if (frand() >= hit) {
    pop(d, "Raté", GREY);
    log.push_back(a->name() + " > " + d->name() + " : " + m.name + " ratée");
    return -1;
  }
  float eff = moveEff(m, d->S());
  if (eff <= 0) {
    pop(d, "Aucun effet", GREY);
    return 0;
  }
  float A = magic ? a->eMag() : a->eAtk();
  float D = magic ? d->eRes() : d->eDef();
  float stab = hasType(a->S(), m.type) ? float(R.stab) : 1.f;
  float spread = float(R.spreadMin) + frand() * float(1 - R.spreadMin);
  bool crit = frand() * 100 < a->crit + m.critBonus;
  float k = crit ? float(R.critMult) : 1.f;
  int dmg = std::max(1, int(((2 * a->lvl / 5.f + 2) * m.power * A / std::max(1.f, D) / float(R.dmgDivisor) + 2) * stab * eff * spread * k));
  d->hp = std::max(0, d->hp - dmg);
  setBlink(d);
  pop(d, std::to_string(dmg), eff > 1 ? GOLD : eff < 1 ? GREY : WHITE);
  if (crit) pop(d, "Critique !", RED);
  if (eff > 1) pop(d, "Efficace !", GOLD);
  else if (eff < 1) pop(d, "Résiste", GREY);
  if (isAlly(d) || net_ != Net::None) d->lim = std::min(100.f, d->lim + dmg * float(R.limitGain) / d->mhp);  // en duel, les deux équipes
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
      log.push_back("  " + t->name() + " : " + statusName(e.status));
    }
  }
  if (e.stat >= 0) {
    int old = t->stage[e.stat];
    t->stage[e.stat] = std::clamp(old + e.stages, -3, 3);
    if (t->stage[e.stat] != old) {
      pop(t, std::string(stageName(e.stat)) + (e.stages > 0 ? " +" : " -"), e.stages > 0 ? BLUE : GREY);
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

void Battle::useMove(FighterP a, const std::string& id, std::vector<FighterP> targets, bool isLimit) {
  if (net_ == Net::Guest) {  // l'invité envoie son ordre ; l'hôte le joue et renvoie le résultat
    Json t = Json::array();
    for (auto& f : targets) t.push_back(mine(f));
    send_(Json{{"t", "ordre"}, {"i", mine(a)[1]}, {"move", id}, {"limite", isLimit}, {"cibles", t}});
    G.menus.clear();
    cursor = nullptr;
    actor = nullptr;
    return;
  }
  G.menus.clear();
  cursor = nullptr;
  const Move& m = moveInfo(id);
  if (m.cost > 0) a->mp = std::max(0, a->mp - m.cost);
  if (isLimit) a->lim = 0;
  sc.say(isLimit ? "LIMITE — " + a->name() + " : " + m.name + " !" : a->name() + " : " + m.name, isLimit ? .9f : .55f);
  auto ko = std::make_shared<std::vector<FighterP>>();
  sc.call([this, a, targets, &m, isLimit, ko] {
    if (isLimit || m.target == Target::AllEnemies) {
      flashT = G.time;
      flashCol = m.kind == Kind::Heal ? rgb(0xfff4c0) : isLimit ? rgb(0xff8ac8) : rgb(0xffffff);
      if (net_ == Net::Host) send_(Json{{"t", "eclair"}, {"c", Json::array({flashCol.r, flashCol.g, flashCol.b})}});
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
        int dmg = applyHit(a, t, m);
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
    for (auto& f : *ko) sc.say(f->name() + (isAlly(f) || net_ != Net::None ? " est K.O. !" : " est vaincu !"), .75f);
  });
  float gauge = isLimit ? 0.f : paceGauge(m);
  sc.call([this, a, gauge] { afterAction(a, gauge); });
}

void Battle::useItem(FighterP a, const std::string& id, FighterP t) {
  if (net_ != Net::None) return;  // pas d'objets en duel
  G.menus.clear();
  cursor = nullptr;
  G.items[id]--;
  const ItemDef& d = item(id);
  sc.say(a->name() + " utilise : " + d.name, .7f);
  sc.call([this, a, id, t] {
    const ItemDef& d = item(id);
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
        G.team.push_back(t);
        sc.say("Capturé ! " + t->name() + " rejoint l'équipe" + ((int)G.team.size() > rules().frontSize ? " (en réserve)." : "."), 1.5f);
      } else sc.say(t->name() + " s'échappe de la lumière !", 1.0f);
    }
  });
  sc.wait(.4f);
  sc.call([this, a] { afterAction(a); });
}

void Battle::swapIn(FighterP a, FighterP r) {
  G.menus.clear();
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
    log.push_back("  " + a->name() + " : " + statusName(a->status) + " " + std::to_string(dmg));
    sc.say(a->name() + (poison ? " souffre du poison." : " souffre de sa brûlure."), .6f);
    if (!a->alive()) {
      knockOut(a);
      sc.say(a->name() + (isAlly(a) || net_ != Net::None ? " est K.O. !" : " est vaincu !"), .75f);
    }
  }
  sc.call([this] { checkEnd(); });
}

void Battle::checkEnd() {
  if (net_ != Net::None) {
    if (!alive(foes).empty() && !alive(allies).empty()) return;
    bool won = alive(foes).empty();
    ending_ = true;
    sc.say(names_[won ? 0 : 1] + " remporte le duel !", 1.8f);
    sc.call([this, won] { finish(won ? BattleResult::Win : BattleResult::Lose); });
    return;
  }
  // Renforts ennemis : remplacent ceux qui sont tombés
  for (auto& f : foes)
    if (!f->alive() && !reserve.empty()) {
      FighterP n = reserve.front();
      reserve.erase(reserve.begin());
      f = n;
      n->atb = frand() * 30;
      sc.say(foeName.empty() ? n->name() + " entre dans le combat !" : foeName + " envoie " + n->name() + " !", 1.1f);
    }
  if (alive(foes).empty()) victory();
  else if (alive(allies).empty()) {
    ending_ = true;
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
  sc.say("Victoire !", .9f);
  if (total > 0) {
    int share = int(total * R.xpFront + 1e-6), res = int(total * R.xpReserve + 1e-6);
    sc.say("Chaque combattant gagne " + std::to_string(share) + " points d'expérience.", 1.4f);
    sc.call([this, share, res] {
      for (auto& a : alive(allies))
        for (auto& msg : gainXp(*a, share)) sc.say(msg, 1.3f);
      for (auto& m : G.team)
        if (!isAlly(m) && m->alive()) gainXp(*m, res);
    });
  }
  if (goldGain > 0) {
    G.gold += goldGain;
    sc.say("Vous trouvez " + std::to_string(goldGain) + " pièces d'or.", 1.1f);
  }
  if (!boss) {
    // Un seul objet au plus : les chances s'additionnent dans l'ordre de la liste
    float r = frand(), acc = 0;
    for (auto& [id, chance] : R.drops) {
      acc += float(chance);
      if (r < acc) {
        G.items[id]++;
        sc.say("Vous ramassez : " + item(id).name + ".", 1.0f);
        break;
      }
    }
  }
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
  // Relief : prolongé par un sommet de chaque côté quand l'écran est plus large que la scène
  auto ridge = [&](std::vector<Pt> p, float yl, float yr) {
    if (L < 0) {
      p.insert(p.begin(), {{L, 112}, {L, yl}});
      p.push_back({R, yr});
      p.push_back({R, 112});
    }
    return p;
  };
  if (bg == Theme::Vallee && !boss) {
    g.gradV(L, 0, W, 112, rgb(0x8fc0db), rgb(0xdfeac0));
    if (L < 0) g.ellipse(L + 10, 113, 70, 18, rgb(0xaed08c)), g.ellipse(R - 10, 112, 70, 20, rgb(0xbcd89a));
    g.ellipse(70, 112, 120, 22, rgb(0xbcd89a));
    g.ellipse(260, 114, 110, 18, rgb(0xaed08c));
    g.rect(L, 108, W, SCREEN_H - 108, rgb(0x9cc77a));  // le sol descend jusqu'en bas, derrière les fenêtres
    for (int i = 0; i < 14 * more / 100; i++) g.rect(L + std::fmod(i * 53 + 11.f, W), 114 + (i * 13) % 50, 10, 2, rgb(0xa9d186));
  } else if (bg == Theme::Vallee) {
    g.gradV(L, 0, W, 112, rgb(0x1d1838), rgb(0x5a4d80));
    g.rect(L, 108, W, SCREEN_H - 108, rgb(0x3c3360));
    for (int i = 0; i < 6 * more / 100; i++) g.ellipse(L + std::fmod(i * 70 + t * 12, W + 100) - 50, 90 + (i % 6) * 9, 60, 6, rgb(0xd8d0ff, 26));
  } else if (bg == Theme::Foret) {
    g.gradV(L, 0, W, 112, rgb(0x1e3a2a), rgb(0x4a7a4a));
    for (int i = L < 0 ? (int)std::floor(L / 40) - 1 : 0; i * 40.f - 10 < R + 30; i++) {
      int odd = i & 1, row = ((i % 3) + 3) % 3;
      float x = i * 40.f - 10 + odd * 12;
      g.rect(x + 8, 30 + row * 8, 6, 80, rgb(0x2a1e16));
      g.ellipse(x + 11, 34 + row * 8, 22, 26, rgb(odd ? 0x173a22 : 0x1f4a2a));
    }
    g.rect(L, 108, W, SCREEN_H - 108, rgb(0x3f6a3a));
    for (int i = 0; i < 12 * more / 100; i++)
      g.ellipse(L + std::fmod(i * 53.f + std::sin(t + i) * 8 + 10, W), 40 + std::fmod(i * 23.f, 70.f), 1.5f, 1.5f,
                rgb(0xe8ff9a, uint8_t(100 + 80 * std::sin(t * 3 + i))));
  } else if (bg == Theme::Neige) {
    g.gradV(L, 0, W, 112, boss ? rgb(0x2a3a5a) : rgb(0x9ab8d8), boss ? rgb(0x7a9ac8) : rgb(0xe6eef8));
    g.poly(ridge({{0, 112}, {50, 50}, {100, 90}, {160, 30}, {220, 86}, {270, 56}, {320, 80}, {320, 112}}, 66, 58), rgb(0xc8d6e8));
    g.poly({{150, 42}, {160, 30}, {171, 42}}, rgb(0xffffff));
    g.rect(L, 108, W, SCREEN_H - 108, rgb(0xe8eef6));
    for (int i = 0; i < 30 * more / 100; i++)
      g.rect(L + std::fmod(i * 41 + t * 8 + std::sin(t + i) * 4 + 10, W), std::fmod(i * 23 + t * 20, 168.f), 1, 1, rgb(0xffffff, 220));
  } else if (bg == Theme::Cendres) {
    g.gradV(L, 0, W, 112, boss ? rgb(0x2a0e08) : rgb(0x3a2420), boss ? rgb(0xa8321e) : rgb(0x8a4a2a));
    g.poly(ridge({{0, 112}, {60, 60}, {110, 100}, {170, 40}, {240, 96}, {290, 70}, {320, 90}, {320, 112}}, 70, 64), rgb(0x4a3a36));
    g.rect(L, 108, W, SCREEN_H - 108, rgb(0x6e625c));
    g.rect(L, 108, W, 2, rgb(0xd9541e));
    for (int i = 0; i < 18 * more / 100; i++)
      g.rect(L + std::fmod(i * 41 + t * 6, W), std::fmod(i * 23 + t * 14, 108.f), 1, 1, rgb(0xffb347, 160));
  } else {
    g.gradV(L, 0, W, 112, rgb(0x120e18), rgb(0x2e2838));
    g.rect(L, 108, W, SCREEN_H - 108, rgb(0x4a4252));
    for (int i = L < 0 ? (int)std::floor((L - 20) / 70) : 0; 20 + i * 70.f < R; i++) {
      float x = 20 + i * 70.f;
      g.ellipse(x, 104, 10, 8, rgb(0x7fd6ff, uint8_t(40 + 30 * std::sin(t * 2 + i))));
      g.tri(x - 4, 108, x, 90, x + 4, 108, rgb(0x7fd6ff));
    }
  }
  bool blinking = blink && t - blinkT < .42f && int((t - blinkT) / .07f) % 2 == 0;
  // Ennemis
  for (auto& f : foes) {
    if (!f->alive()) continue;
    Pt p = pos(f);
    float s = f->boss ? 1.45f : f->sp == "brumelin" ? .85f : 1.f;
    if (!(blinking && blink == f)) drawFighterSprite(g, *f, p.x, p.y, s, true, t + p.x * .01f);
    float bw = f->boss ? 24 : 15, by = p.y - (f->boss ? 42 : 26);
    g.rect(p.x - bw - 1, by - 1, bw * 2 + 2, 4, rgb(0x0a0f33));
    g.rect(p.x - bw, by, bw * 2 * f->hp / f->mhp, 2, rgb(0xff8a7a));
    drawStatusTag(*f, p.x, by - 10);
    if (cursor == f) {
      g.cursor(p.x - 3, by - 12 + std::sin(t * 8));
      g.text(p.x, p.y + (f->boss ? 30 : 20), f->name(), WHITE, 1);
    }
  }
  // Alliés
  for (auto& a : allies) {
    Pt p = pos(a);
    if (!a->alive()) g.alpha = .35f;
    if (!(blinking && blink == a)) drawFighterSprite(g, *a, p.x, p.y, 1, false, t + p.x * .01f);
    g.alpha = 1;
    drawStatusTag(*a, p.x - 26, p.y - 18);
    if (actor == a) g.tri(p.x - 4, p.y - 34, p.x + 4, p.y - 34, p.x, p.y - 28, rgb(0xffd34d));
    if (cursor == a) g.cursor(p.x - 24, p.y - 4);
  }
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
  else if (net_ == Net::Guest && !netMsg_.empty()) top = utf8Prefix(netMsg_, int((t - netMsgT_) * 60));
  else if ((net_ == Net::Host && remoteTurn_) || (net_ == Net::Guest && netWait_ == "hote")) top = names_[1] + " choisit…";
  if (!top.empty()) {
    auto lines = Gfx::wrap(top, 300);
    g.window(4, 2, 312, 7 + 11 * (int)lines.size());
    for (size_t i = 0; i < lines.size(); i++) g.text(160, 5 + i * 11, lines[i], WHITE, 1);
  }
  // Fenêtre de gauche : noms des ennemis (et renforts restants), cachée par le menu de commande
  if (!G.menus.active()) {
    g.window(4, 168, 96, 70);
    int row = 0;
    for (auto& f : foes) {
      if (!f->alive() || row > 3) continue;
      g.text(10, 173 + row * 12, utf8Prefix(f->name(), f->status != Status::None ? 10 : 14),
             f->status != Status::None ? rgb(statusColor(f->status)) : WHITE);
      if (f->status != Status::None) g.text(94, 173 + row * 12, statusTag(f->status), rgb(statusColor(f->status)), 2);
      row++;
    }
    if (!reserve.empty()) g.text(10, 222, "Renforts : " + std::to_string(reserve.size()), GREY);
  }
  // Fenêtre d'état de l'équipe
  g.window(102, 168, 214, 70);
  Color lab = rgb(0xaab3d8);
  g.text(201, 171, "PV", lab, 1);
  g.text(238, 171, "PM", lab, 1);
  g.text(263, 171, "ATB", lab, 1);
  g.text(296, 171, "LIMITE", lab, 1);
  // Mode auto : les tactiques jouent (touche Tab pour l'activer ou le couper)
  bool flash = t - toggledT_ < 1.2f && int(t * 8) % 2;
  g.text(108, 171, G.tacticsAuto ? "Auto (Tab)" : "Manuel (Tab)", flash ? WHITE : G.tacticsAuto ? GOLD : lab);
  for (size_t i = 0; i < allies.size(); i++) {
    auto& a = allies[i];
    float y = 184 + i * 17;
    if (actor == a) {
      g.rect(105, y - 2, 208, 15, rgb(0xffffff, 34));
      g.cursor(105, y);
    }
    Color nc = !a->alive() ? rgb(0xff7b6b) : a->status != Status::None ? rgb(statusColor(a->status)) : WHITE;
    g.text(112, y, utf8Prefix(a->name(), 10), nc);
    Color hc = a->hp * 4 < a->mhp ? GOLD : WHITE;
    g.text(223, y, std::to_string(a->hp) + "/" + std::to_string(a->mhp), hc, 2);
    g.rect(179, y + 10, 44, 1, rgb(0x2a3270));
    g.rect(179, y + 10, 44.f * a->hp / a->mhp, 1, GREEN);
    g.text(245, y, std::to_string(a->mp), WHITE, 2);
    auto bar = [&](float x, float r, Color c) {
      g.rect(x - 1, y + 2, 28, 6, rgb(0x0a0f33));
      g.rect(x, y + 3, 26, 4, rgb(0x2a3270));
      g.rect(x, y + 3, 26 * std::clamp(r, 0.f, 1.f), 4, c);
    };
    // Après une technique lourde, la jauge part en dessous de zéro : barre rouge qui se vide
    if (a->alive() && a->atb < 0) bar(250, -a->atb / 100, RED);
    else bar(250, a->alive() ? a->atb / 100 : 0, a->atb >= 100 ? GOLD : rgb(0x7fd6ff));
    bool full = a->lim >= 100;
    bar(283, a->lim / 100, full && int(t * 5) % 2 ? WHITE : rgb(0xff6fb0));
  }
  G.menus.draw(g, t);
  float k = (t - start) / .35f;
  if (k < 1) g.rect(g.left(), 0, g.fullW, SCREEN_H, rgb(0xffffff, uint8_t(255 * (1 - k))));
}

// ---------------------------------------------------------------------------
// Duel en ligne (online.hpp) : l'hôte joue tout le combat et envoie son état ;
// l'invité l'affiche et répond quand un de ses combattants doit agir.
// ---------------------------------------------------------------------------
void Battle::goOnline(Net mode, std::function<void(const Json&)> send, const std::string& me, const std::string& other) {
  net_ = mode;
  send_ = std::move(send);
  names_[0] = me, names_[1] = other;
  sc.clear();
  if (mode == Net::Host) sc.say("Duel : " + me + " contre " + other + " !", 1.3f);
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
  const auto& v = side == 0 ? foes : allies;  // l'équipe de l'autre joueur, ce sont mes « ennemis »
  return side >= 0 && side <= 1 && i >= 0 && i < (int)v.size() ? v[(size_t)i] : nullptr;
}
void Battle::setBlink(const FighterP& f) {
  blink = f;
  blinkT = G.time;
  if (net_ == Net::Host) send_(Json{{"t", "clignote"}, {"f", mine(f)}});
}

Json Battle::netState() const {
  auto pack = [](const std::vector<FighterP>& v) {
    Json a = Json::array();
    for (auto& f : v) {
      Json st = Json::array();
      for (int k = 0; k < N_STAGES; k++) st.push_back(f->stage[k]);
      a.push_back(Json::array({f->hp, f->mp, (int)std::lround(f->atb), (int)std::lround(f->lim), (int)f->status, st}));
    }
    return a;
  };
  return Json{{"t", "etat"},
              {"a", pack(allies)},
              {"b", pack(foes)},
              {"msg", sc.showingMessage() ? sc.text() : std::string()},
              {"attente", remoteTurn_ ? "invite" : G.menus.active() ? "hote" : ""}};
}

void Battle::netMessage(const Json& m) {
  std::string k = jget<std::string>(m, "t", "");
  if (net_ == Net::Guest) {
    if (k == "etat") {
      // « a » : l'équipe de l'hôte (mes ennemis) ; « b » : la mienne
      auto apply = [](std::vector<FighterP>& v, const Json& a) {
        for (size_t i = 0; i < v.size() && i < a.size(); i++) {
          const Json& x = a[i];
          Fighter& f = *v[i];
          f.hp = x[0].get<int>(), f.mp = x[1].get<int>(), f.atb = x[2].get<float>(), f.lim = x[3].get<float>();
          f.status = Status(std::clamp(x[4].get<int>(), 0, 4));
          for (int s = 0; s < N_STAGES && s < (int)x[5].size(); s++) f.stage[s] = x[5][(size_t)s].get<int>();
        }
      };
      apply(foes, m.value("a", Json::array()));
      apply(allies, m.value("b", Json::array()));
      std::string msg = jget<std::string>(m, "msg", "");
      if (msg != netMsg_) netMsg_ = msg, netMsgT_ = G.time;
      netWait_ = jget<std::string>(m, "attente", "");
    } else if (k == "tour") {
      int i = jget(m, "i", -1);
      if (i < 0 || i >= (int)allies.size() || !allies[(size_t)i]->alive()) return;
      FighterP a = allies[(size_t)i];
      actor = a;
      if (G.tacticsAuto && tacticTurn(a)) return;  // mode auto : les tactiques choisissent
      command(a);
    } else if (k == "pop") {
      if (FighterP f = other(m.value("f", Json()))) {
        Json c = m.value("c", Json::array({255, 255, 255}));
        Color col;
        col.r = c[0].get<uint8_t>(), col.g = c[1].get<uint8_t>(), col.b = c[2].get<uint8_t>();
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
      flashCol = rgb(uint32_t(c[0].get<int>()) << 16 | uint32_t(c[1].get<int>()) << 8 | uint32_t(c[2].get<int>()));
    }
    return;
  }
  // Hôte : l'ordre de l'invité pour son combattant en attente (vérifié : technique connue, PM, cibles)
  if (net_ == Net::Host && k == "ordre" && remoteTurn_) {
    FighterP f = remoteTurn_;
    remoteTurn_ = nullptr;
    int i = jget(m, "i", -1);
    std::string id = jget<std::string>(m, "move", "");
    bool lim = jget(m, "limite", false);
    std::vector<FighterP> targets;
    Json cs = m.value("cibles", Json::array());
    for (auto& c : cs)
      if (FighterP t = other(c)) targets.push_back(t);
    bool ok = i >= 0 && i < (int)foes.size() && foes[(size_t)i] == f && hasMove(id) && !targets.empty();
    if (ok && lim) ok = f->S().limit == id && f->lim >= 100;
    else if (ok) {
      auto known = f->techs();
      for (auto& s : f->spells()) known.push_back(s);
      ok = std::find(known.begin(), known.end(), id) != known.end() && f->mp >= moveInfo(id).cost;
    }
    if (ok) useMove(f, id, targets, lim);
    else enemyTurn(f);  // ordre impossible : l'ordinateur joue à sa place
  }
}

void Battle::netEnd(bool won) {
  if (finished_) return;
  G.menus.clear();
  finish(won ? BattleResult::Win : BattleResult::Lose);
}
