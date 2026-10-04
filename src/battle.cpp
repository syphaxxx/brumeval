#include "battle.hpp"

#include <algorithm>
#include <cmath>
#include <map>

#include "game.hpp"
#include "sprites.hpp"

static const Color WHITE = rgb(0xffffff), GOLD = rgb(0xffe066), GREY = rgb(0xb9c0de), GREEN = rgb(0x7dffa8), BLUE = rgb(0x8fd0ff);

Battle::Battle(Game& game, std::vector<FighterP> f, bool boss_, Theme bg_, bool flee, bool cap)
    : G(game), foes(std::move(f)), boss(boss_), canFlee(flee), canCapture(cap), bg(bg_) {
  allies = G.front();
  start = G.time;
  std::map<std::string, int> cnt, seen;
  for (auto& e : foes) cnt[e->sp]++;
  for (auto& e : foes)
    if (cnt[e->sp] > 1) e->tag = std::string(1, char('A' + seen[e->sp]++));
  for (auto& a : allies) a->atb = 20 + frand() * 50;
  for (auto& e : foes) e->atb = frand() * 30;
  G.menus.clear();
  std::string intro;
  FighterP leader = foes[0];
  for (auto& e : foes)
    if (e->boss) leader = e;
  if (leader->S().human) intro = leader->name() + " engage le combat !";
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
void Battle::pop(const FighterP& f, const std::string& t, Color c, float dy) {
  Pt p = pos(f);
  pops.push_back({p.x, p.y - 22 - dy, t, c, G.time});
}

// ---------------------------------------------------------------------------
void Battle::update(float dt) {
  if (finished_) return;
  if (sc.busy()) {
    sc.update(dt, G.in);
    return;
  }
  if (ending_) return;
  if (G.menus.active()) {
    G.menus.update(G.in);
    return;
  }
  tickATB(dt);
}

void Battle::tickATB(float dt) {
  std::vector<FighterP> all = alive(allies);
  for (auto& e : alive(foes)) all.push_back(e);
  for (auto& f : all) {
    f->atb += dt * (20 + f->spd) * 1.1f;
    if (f->atb >= 100) {
      f->atb = 100;
      if (isAlly(f)) {
        if (autoPlay) autoCommand(f);
        else command(f);
      } else enemyTurn(f);
      return;
    }
  }
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
                         t.items.push_back({mv.name, typeName(mv.type), "Puissance " + std::to_string(mv.power) + " · " + typeName(mv.type), true,
                                            [this, a, id] { moveTarget(a, id); }});
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
                           t.items.push_back({mv.name, std::to_string(mv.cost) + " PM", mv.desc, a->mp >= mv.cost,
                                              [this, a, id] { moveTarget(a, id); }});
                         }
                         G.menus.push(t);
                       }});
  const Move& lim = moveInfo(a->S().limit);
  m.items.push_back({"Limite", "", a->lim >= 100 ? lim.name + " : technique ultime !" : "La jauge Limite se remplit quand on reçoit des coups.",
                     a->lim >= 100, [this, a, lim] {
                       G.menus.clear();
                       useMove(a, lim.id, lim.target == Target::AllAllies ? alive(allies) : alive(foes), true);
                     }});
  m.items.push_back({"Objet", "", "Utiliser un objet du sac.", true, [this, a] {
                       Menu t;
                       t.title = "Objets";
                       t.x = 4, t.y = 82, t.w = 176, t.rows = 4;
                       for (auto& d : allItems()) {
                         int n = G.items.count(d.id) ? G.items[d.id] : 0;
                         if (!d.battle || n <= 0) continue;
                         std::string id = d.id;
                         bool ok = true;
                         bool lantern = id.rfind("lanterne", 0) == 0;
                         if (lantern) ok = canCapture && (int)G.team.size() < Game::MAX_TEAM;
                         if (id == "plume") ok = alive(allies).size() < allies.size();
                         std::string help = d.desc;
                         if (lantern && !canCapture) help = "Impossible de capturer ici.";
                         else if (lantern && !ok) help = "L'équipe est complète (8 membres).";
                         t.items.push_back({d.name, "×" + std::to_string(n), help, ok, [this, a, id, lantern] {
                                              if (lantern) {
                                                float mult = id == "lanterne_argent" ? 1.6f : 1.f;
                                                pickFoe("Capturer", [mult](const Fighter& f) {
                                                  float c = std::min(.95f, (.15f + .75f * (1 - float(f.hp) / f.mhp)) * mult);
                                                  return "Chance de capture : " + std::to_string(int(c * 100)) + " %";
                                                }, [this, a, id](FighterP t) { useItem(a, id, t); });
                                              } else pickAlly("Sur qui ?", id == "plume", [this, a, id](FighterP t) { useItem(a, id, t); });
                                            }});
                       }
                       if (t.items.empty()) t.items.push_back({"(sac vide)", "", "", false, nullptr});
                       G.menus.push(t);
                     }});
  std::vector<FighterP> reserve;
  for (auto& r : G.team)
    if (r->alive() && !isAlly(r)) reserve.push_back(r);
  m.items.push_back({"Changer", "", reserve.empty() ? "Personne en réserve." : "Faire entrer un membre de la réserve.", !reserve.empty(),
                     [this, a, reserve] {
                       Menu t;
                       t.title = "Réserve";
                       t.x = 4, t.y = 82, t.w = 176, t.rows = 4;
                       for (auto& r : reserve)
                         t.items.push_back({r->name() + " N." + std::to_string(r->lvl), std::to_string(r->hp) + "/" + std::to_string(r->mhp),
                                            std::string(typeName(r->S().type)) + " · " + std::to_string(r->mp) + " PM", true,
                                            [this, a, r] { swapIn(a, r); }});
                       G.menus.push(t);
                     }});
  if (canFlee) m.items.push_back({"Fuir", "", "Tenter de fuir avec toute l'équipe.", true, [this, a] { tryFlee(a); }});
  G.menus.push(m);
}

void Battle::pickFoe(const std::string& title, std::function<std::string(const Fighter&)> info, std::function<void(FighterP)> done) {
  auto L = alive(foes);
  Menu t;
  t.title = title;
  t.x = 4, t.y = 94, t.w = 150, t.rows = 3;
  t.onCancel = [this] {
    cursor = nullptr;
    G.menus.pop();
  };
  for (auto& f : L)
    t.items.push_back({f->name(), "N." + std::to_string(f->lvl), info(*f), true, [done, f] { done(f); }, [this, f] { cursor = f; }});
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
    t.items.push_back({f->name(), std::to_string(f->hp) + "/" + std::to_string(f->mhp),
                       std::to_string(f->mp) + "/" + std::to_string(f->mmp) + " PM" + (f->alive() ? "" : " · K.O."), ok, [done, f] { done(f); },
                       [this, f] { cursor = f; }});
  }
  G.menus.push(t);
}

void Battle::moveTarget(FighterP a, const std::string& id) {
  const Move& m = moveInfo(id);
  switch (m.target) {
    case Target::Enemy:
      pickFoe("Cible", [m](const Fighter& f) {
        float e = typeEff(m.type, f.S().type);
        std::string s = std::string(typeName(f.S().type)) + " · " + std::to_string(f.hp) + "/" + std::to_string(f.mhp) + " PV";
        if (e > 1) s += " · super efficace";
        if (e < 1) s += " · peu efficace";
        return s;
      }, [this, a, id](FighterP t) { useMove(a, id, {t}); });
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

void Battle::autoCommand(FighterP a) {
  actor = a;
  if (a->lim >= 100) {
    const Move& lim = moveInfo(a->S().limit);
    useMove(a, lim.id, lim.target == Target::AllAllies ? alive(allies) : alive(foes), true);
    return;
  }
  for (auto& id : a->spells()) {
    const Move& m = moveInfo(id);
    if (m.kind == Kind::Heal && a->mp >= m.cost)
      for (auto& x : alive(allies))
        if (x->hp < x->mhp / 3) {
          useMove(a, id, m.target == Target::AllAllies ? alive(allies) : std::vector<FighterP>{x});
          return;
        }
  }
  auto L = alive(foes);
  FighterP t = L[irand(0, (int)L.size() - 1)];
  std::string best;
  float score = -1;
  for (auto& id : a->techs()) {
    const Move& m = moveInfo(id);
    float s = m.power * typeEff(m.type, t->S().type);
    if (s > score) score = s, best = id;
  }
  useMove(a, best, {t});
}

// ---------------------------------------------------------------------------
// Résolution des actions
// ---------------------------------------------------------------------------
void Battle::enemyTurn(FighterP e) {
  actor = e;
  auto techs = e->techs();
  std::string mv = techs[irand(0, (int)techs.size() - 1)];
  if (e->boss)
    for (auto& id : techs)
      if (moveInfo(id).target == Target::AllEnemies && frand() < .35f) mv = id;
  const Move& m = moveInfo(mv);
  auto L = alive(allies);
  std::vector<FighterP> t = m.target == Target::AllEnemies ? L : std::vector<FighterP>{L[irand(0, (int)L.size() - 1)]};
  useMove(e, mv, t);
}

int Battle::applyHit(const FighterP& a, const FighterP& d, const Move& m) {
  bool magic = m.kind == Kind::Magic;
  float A = float(magic ? a->mag : a->atk);
  float D = magic ? (d->def + d->mag) / 2.f : float(d->def);
  float eff = typeEff(m.type, d->S().type);
  float stab = m.type == a->S().type ? 1.25f : 1.f;
  int dmg = std::max(1, int(((2 * a->lvl / 5.f + 2) * m.power * A / std::max(1.f, D) / 40.f + 2) * stab * eff * (.85f + frand() * .15f)));
  d->hp = std::max(0, d->hp - dmg);
  blink = d;
  blinkT = G.time;
  pop(d, std::to_string(dmg), eff > 1 ? GOLD : eff < 1 ? GREY : WHITE);
  if (eff > 1) pop(d, "Efficace !", GOLD, 11);
  else if (eff < 1) pop(d, "Résiste", GREY, 11);
  if (isAlly(d)) d->lim = std::min(100.f, d->lim + dmg * 110.f / d->mhp);
  if (!d->alive() && !isAlly(d)) defeated.push_back(d);
  return dmg;
}

void Battle::useMove(FighterP a, const std::string& id, std::vector<FighterP> targets, bool isLimit) {
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
    }
    for (auto& t : targets) {
      if (m.kind == Kind::Heal) {
        if (!t->alive()) continue;
        int amt = std::max(1, m.power * a->mag / 20 + irand(0, 4));
        amt = std::min(amt, t->mhp - t->hp);
        t->hp += amt;
        pop(t, "+" + std::to_string(amt), GREEN);
      } else if (m.kind == Kind::Revive) {
        if (t->alive()) continue;
        t->hp = std::max(1, t->mhp * m.power / 100);
        t->atb = 0;
        pop(t, "+" + std::to_string(t->hp), GREEN);
      } else {
        if (!t->alive()) continue;
        applyHit(a, t, m);
        if (!t->alive()) ko->push_back(t);
      }
    }
  });
  sc.wait(.75f);
  sc.call([this, ko] {
    for (auto& f : *ko) sc.say(f->name() + (isAlly(f) ? " est K.O. !" : " est vaincu !"), .75f);
  });
  sc.call([this, a] { afterAction(a); });
}

void Battle::useItem(FighterP a, const std::string& id, FighterP t) {
  G.menus.clear();
  cursor = nullptr;
  G.items[id]--;
  const ItemDef& d = item(id);
  sc.say(a->name() + " utilise : " + d.name, .7f);
  sc.call([this, a, id, t] {
    if (id == "potion" || id == "superpotion") {
      int amt = std::min(id == "potion" ? 40 : 120, t->mhp - t->hp);
      t->hp += amt;
      pop(t, "+" + std::to_string(amt), GREEN);
    } else if (id == "ether") {
      int amt = std::min(25, t->mmp - t->mp);
      t->mp += amt;
      pop(t, "+" + std::to_string(amt) + " PM", BLUE);
    } else if (id == "plume") {
      t->hp = std::max(1, t->mhp / 2);
      t->atb = 0;
      pop(t, "+" + std::to_string(t->hp), GREEN);
      sc.say(t->name() + " se relève !", .9f);
    } else {
      float mult = id == "lanterne_argent" ? 1.6f : 1.f;
      float c = std::min(.95f, (.15f + .75f * (1 - float(t->hp) / t->mhp)) * mult);
      flashT = G.time;
      flashCol = rgb(0xfff0a0);
      if (frand() < c) {
        foes.erase(std::find(foes.begin(), foes.end(), t));
        t->tag.clear();
        t->hp = t->mhp;
        t->mp = t->mmp;
        t->lim = 0;
        t->atb = 0;
        G.team.push_back(t);
        sc.say("Capturé ! " + t->name() + " rejoint l'équipe" + (G.team.size() > 3 ? " (en réserve)." : "."), 1.5f);
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
    if (frand() < .7f) {
      sc.say("L'équipe prend la fuite !", 1.0f);
      sc.call([this] { finish(BattleResult::Fled); });
    } else {
      sc.say("Impossible de fuir !", .9f);
      sc.call([this, a] { afterAction(a); });
    }
  });
}

void Battle::afterAction(FighterP a) {
  a->atb = 0;
  actor = nullptr;
  if (alive(foes).empty()) victory();
  else if (alive(allies).empty()) {
    ending_ = true;
    sc.say("Toute l'équipe est à terre…", 1.6f);
    sc.call([this] { finish(BattleResult::Lose); });
  }
}

void Battle::victory() {
  ending_ = true;
  int total = 0, goldGain = 0;
  for (auto& d : defeated) {
    total += d->lvl * 9 * (d->boss ? 3 : 1);
    goldGain += d->lvl * 4 * (d->boss ? 8 : 1);
  }
  sc.say("Victoire !", .9f);
  if (total > 0) {
    int share = total * 7 / 10, res = total * 3 / 10;
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
    float r = frand();
    if (r < .25f) {
      G.items["potion"]++;
      sc.say("Vous ramassez une Potion.", 1.0f);
    } else if (r < .33f) {
      G.items["ether"]++;
      sc.say("Vous ramassez un Éther.", 1.0f);
    }
  }
  sc.call([this] { finish(BattleResult::Win); });
}

void Battle::finish(BattleResult r) {
  result_ = r;
  finished_ = true;
  G.menus.clear();
  for (auto& f : G.team) f->atb = 0;
}

// ---------------------------------------------------------------------------
// Dessin
// ---------------------------------------------------------------------------
void Battle::draw() {
  Gfx& g = G.g;
  float t = G.time;
  // Décor
  if (bg == Theme::Vallee && !boss) {
    g.gradV(0, 0, SCREEN_W, 112, rgb(0x8fc0db), rgb(0xdfeac0));
    g.ellipse(70, 112, 120, 22, rgb(0xbcd89a));
    g.ellipse(260, 114, 110, 18, rgb(0xaed08c));
    g.rect(0, 108, SCREEN_W, 60, rgb(0x9cc77a));
    for (int i = 0; i < 14; i++) g.rect((i * 53 + 11) % 320, 114 + (i * 13) % 50, 10, 2, rgb(0xa9d186));
  } else if (bg == Theme::Vallee) {
    g.gradV(0, 0, SCREEN_W, 112, rgb(0x1d1838), rgb(0x5a4d80));
    g.rect(0, 108, SCREEN_W, 60, rgb(0x3c3360));
    for (int i = 0; i < 6; i++) g.ellipse(std::fmod(i * 70 + t * 12, 420.f) - 50, 90 + i * 9, 60, 6, rgb(0xd8d0ff, 26));
  } else if (bg == Theme::Cendres) {
    g.gradV(0, 0, SCREEN_W, 112, boss ? rgb(0x2a0e08) : rgb(0x3a2420), boss ? rgb(0xa8321e) : rgb(0x8a4a2a));
    g.poly({{0, 112}, {60, 60}, {110, 100}, {170, 40}, {240, 96}, {290, 70}, {320, 90}, {320, 112}}, rgb(0x4a3a36));
    g.rect(0, 108, SCREEN_W, 60, rgb(0x6e625c));
    g.rect(0, 108, SCREEN_W, 2, rgb(0xd9541e));
    for (int i = 0; i < 18; i++)
      g.rect(std::fmod(i * 41 + t * 6, 320.f), std::fmod(i * 23 + t * 14, 108.f), 1, 1, rgb(0xffb347, 160));
  } else {
    g.gradV(0, 0, SCREEN_W, 112, rgb(0x120e18), rgb(0x2e2838));
    g.rect(0, 108, SCREEN_W, 60, rgb(0x4a4252));
    for (int i = 0; i < 5; i++) {
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
    if (actor == a) g.tri(p.x - 4, p.y - 34, p.x + 4, p.y - 34, p.x, p.y - 28, rgb(0xffd34d));
    if (cursor == a) g.cursor(p.x - 24, p.y - 4);
  }
  // Nombres flottants
  pops.erase(std::remove_if(pops.begin(), pops.end(), [&](const Pop& p) { return t - p.t > 1.f; }), pops.end());
  for (auto& p : pops) g.text(p.x, p.y - std::min(1.f, (t - p.t) * 3) * 8, p.text, p.col, 1);
  if (flashT >= 0 && t - flashT < .5f) {
    Color c = flashCol;
    c.a = uint8_t(130 * (1 - (t - flashT) / .5f));
    g.rect(0, 0, SCREEN_W, 168, c);
  }
  // Fenêtre du haut : message ou aide
  std::string top;
  if (G.menus.active()) top = G.menus.help();
  else if (sc.showingMessage()) top = utf8Prefix(sc.text(), sc.visibleChars());
  if (!top.empty()) {
    auto lines = Gfx::wrap(top, 300);
    g.window(4, 2, 312, 7 + 11 * (int)lines.size());
    for (size_t i = 0; i < lines.size(); i++) g.text(160, 5 + i * 11, lines[i], WHITE, 1);
  }
  // Fenêtre de gauche : noms des ennemis
  g.window(4, 168, 96, 70);
  int row = 0;
  for (auto& f : foes) {
    if (!f->alive() || row > 4) continue;
    g.text(10, 173 + row * 12, f->name(), WHITE);
    row++;
  }
  // Fenêtre d'état de l'équipe
  g.window(102, 168, 214, 70);
  Color lab = rgb(0xaab3d8);
  g.text(194, 171, "PV", lab, 1);
  g.text(231, 171, "PM", lab, 1);
  g.text(261, 171, "ATB", lab, 1);
  g.text(297, 171, "LIMITE", lab, 1);
  for (size_t i = 0; i < allies.size(); i++) {
    auto& a = allies[i];
    float y = 184 + i * 17;
    if (actor == a) {
      g.rect(105, y - 2, 208, 15, rgb(0xffffff, 34));
      g.cursor(106, y);
    }
    std::string n = utf8Prefix(a->name(), 9);
    g.text(113, y, n, a->alive() ? WHITE : rgb(0xff7b6b));
    Color hc = a->hp * 4 < a->mhp ? GOLD : WHITE;
    g.text(216, y, std::to_string(a->hp) + "/" + std::to_string(a->mhp), hc, 2);
    g.rect(172, y + 10, 44, 1, rgb(0x2a3270));
    g.rect(172, y + 10, 44.f * a->hp / a->mhp, 1, GREEN);
    g.text(240, y, std::to_string(a->mp), WHITE, 2);
    auto bar = [&](float x, float r, Color c) {
      g.rect(x - 1, y + 2, 32, 6, rgb(0x0a0f33));
      g.rect(x, y + 3, 30, 4, rgb(0x2a3270));
      g.rect(x, y + 3, 30 * std::clamp(r, 0.f, 1.f), 4, c);
    };
    bar(246, a->alive() ? a->atb / 100 : 0, a->atb >= 100 ? GOLD : rgb(0x7fd6ff));
    bool full = a->lim >= 100;
    bar(282, a->lim / 100, full && int(t * 5) % 2 ? WHITE : rgb(0xff6fb0));
  }
  G.menus.draw(g, t);
  float k = (t - start) / .35f;
  if (k < 1) g.rect(0, 0, SCREEN_W, SCREEN_H, rgb(0xffffff, uint8_t(255 * (1 - k))));
}
