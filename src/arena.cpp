#include "arena.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "events.hpp"
#include "game.hpp"
#include "sprites.hpp"
#include "tactics.hpp"

static const Color WHITE = rgb(0xffffff), GOLD = rgb(0xffd34d), MUTED = rgb(0xaab3d8), GREEN = rgb(0x7dffa8), RED = rgb(0xff8a7a);
static const char* THEME_NAMES[] = {"Vallée", "Cendres", "Grotte", "Forêt", "Neige"};
static const int N_THEMES = 5;

// Menu principal : position des éléments
enum { I_ALLY = 1, I_FOE = 8, I_LEVEL = 15, I_THEME, I_ITEMS, I_AI, I_SIM, I_FIGHT, I_PRESETS, I_LOG, I_QUIT };

// « 3 », « 2,5 »
static std::string fmtMult(float v) {
  char b[16];
  if (std::fabs(v - std::round(v)) < 1e-4f) std::snprintf(b, sizeof b, "%d", (int)std::lround(v));
  else std::snprintf(b, sizeof b, "%.1f", v);
  std::string s = b;
  for (auto& c : s)
    if (c == '.') c = ',';
  return s;
}
static std::string pct(double a, double b) { return b > 0 ? std::to_string((int)std::lround(100 * a / b)) + " %" : "-"; }

Arena::Arena(Game& g) : G(g) {
  allies[0] = {"lior", 11};
  allies[1] = {"maelle", 11};
  allies[2] = {"braisenard", 11};
  loadPresets();
  int p = findPreset("Sylvarque");
  if (p >= 0) applyPreset(p);
}

// ---------------------------------------------------------------------------
// Modèles : combats des événements et zones de rencontres
// ---------------------------------------------------------------------------
static ArenaSlot slotFromJson(const Json& e) {
  ArenaSlot s;
  s.sp = jget<std::string>(e, "espece", "");
  s.lvl = jget(e, "niveau", 10);
  s.hpMult = jget(e, "pv", 1.f);
  s.boss = jget(e, "boss", false);
  return s;
}

void Arena::loadPresets() {
  presets_.clear();
  // Thème de la carte où un événement est déclenché
  auto themeOfEvent = [](const std::string& id) {
    for (auto& m : maps()) {
      for (auto& n : m.npcs)
        if (n.event == id) return (int)m.theme;
      for (auto& b : m.bosses)
        if (b.event == id) return (int)m.theme;
      for (auto& b : m.buildings)
        if (b.event == id) return (int)m.theme;
      for (auto& t : m.triggers)
        if (t.event == id) return (int)m.theme;
    }
    return -1;
  };
  std::function<void(const Json&, const std::string&)> walk = [&](const Json& list, const std::string& id) {
    for (auto& a : list) {
      if (jget<std::string>(a, "action", "") == "combat") {
        Preset p;
        for (auto& e : a.value("ennemis", Json::array())) p.foes.push_back(slotFromJson(e));
        for (auto& e : a.value("renforts", Json::array())) p.reserve.push_back(slotFromJson(e));
        if (p.foes.empty() || !hasSpecies(p.foes[0].sp)) continue;
        std::string leader = species(p.foes[0].sp).name;
        for (auto& f : p.foes)
          if (f.boss && hasSpecies(f.sp)) leader = species(f.sp).name;
        p.name = jget<std::string>(a, "nom", leader);
        for (auto& q : presets_)
          if (q.name == p.name) p.name += " (" + id + ")";
        p.info = "Événement « " + id + " »";
        p.theme = themeOfEvent(id);
        presets_.push_back(p);
      }
      for (const char* k : {"oui", "non", "alors", "sinon", "victoire", "defaite"})
        if (a.contains(k)) walk(a[k], id);
    }
  };
  for (auto& [id, ev] : events().items()) {
    if (ev.contains("pages"))
      for (auto& pg : ev["pages"]) walk(pg.value("actions", Json::array()), id);
    else walk(ev.value("actions", Json::array()), id);
  }
  for (auto& m : maps())
    for (auto& z : m.zones) {
      Preset p;
      p.name = m.name + " (N." + std::to_string(z.lo) + "-" + std::to_string(z.hi) + ")";
      p.info = "Rencontres sauvages";
      for (size_t k = 0; k < z.pool.size() && (int)k < std::min(3, z.maxN); k++)
        if (hasSpecies(z.pool[k])) p.foes.push_back({z.pool[k], z.hi});
      p.theme = (int)m.theme;
      if (!p.foes.empty()) presets_.push_back(p);
    }
}

int Arena::findPreset(const std::string& name) const {
  for (size_t i = 0; i < presets_.size(); i++)
    if (presets_[i].name.find(name) != std::string::npos) return (int)i;
  return -1;
}

void Arena::setFoes(const Json& combat) {
  for (auto& s : foes) s = ArenaSlot{"", 10};
  int k = 0;
  for (auto& e : combat.value("ennemis", Json::array()))
    if (k < 3 && hasSpecies(jget<std::string>(e, "espece", ""))) foes[k++] = slotFromJson(e);
  k = 3;
  for (auto& e : combat.value("renforts", Json::array()))
    if (k < 6 && hasSpecies(jget<std::string>(e, "espece", ""))) foes[k++] = slotFromJson(e);
  int top = 1;
  bool boss = false;
  for (auto& f : foes)
    if (!f.sp.empty()) top = std::max(top, f.lvl), boss = boss || f.boss;
  for (auto& a : allies)
    if (!a.sp.empty()) a.lvl = std::max(1, top - (boss ? 1 : 0));
}

void Arena::applyPreset(int i) {
  if (i < 0 || i >= (int)presets_.size()) return;
  const Preset& p = presets_[i];
  for (auto& s : foes) s = ArenaSlot{"", 10};
  for (size_t k = 0; k < p.foes.size() && k < 3; k++) foes[k] = p.foes[k];
  for (size_t k = 0; k < p.reserve.size() && k < 3; k++) foes[3 + k] = p.reserve[k];
  // Les alliés prennent le niveau des adversaires (un de moins contre un boss)
  int top = 1;
  bool boss = false;
  for (auto& f : foes)
    if (!f.sp.empty()) top = std::max(top, f.lvl), boss = boss || f.boss;
  for (auto& a : allies)
    if (!a.sp.empty()) a.lvl = std::max(1, top - (boss ? 1 : 0));
  if (p.theme >= 0) theme = p.theme;
}

// ---------------------------------------------------------------------------
// Préparation des combats
// ---------------------------------------------------------------------------
FighterP Arena::make(const ArenaSlot& s) const {
  auto f = makeFighter(s.sp, s.lvl);
  if (s.hpMult != 1.f) f->mhp = std::max(1, int(f->mhp * s.hpMult + 1e-4f));
  f->hp = f->mhp;
  f->boss = s.boss;
  if (s.customTactics) f->tactics = s.tactics;
  f->tacticsOn = s.tacticsOn;
  return f;
}

bool Arena::ready() const {
  bool a = false, f = false;
  for (auto& s : allies) a = a || !s.sp.empty();
  for (int i = 0; i < 3; i++) f = f || !foes[i].sp.empty();
  return a && f;
}

BattleSetup Arena::makeSetup() const {
  BattleSetup s;
  for (int i = 0; i < 6; i++) {
    if (foes[i].sp.empty()) continue;
    (i < 3 ? s.foes : s.reserve).push_back(make(foes[i]));
    s.boss = s.boss || foes[i].boss;
  }
  s.canFlee = true;
  s.canCapture = false;
  s.theme = theme;
  return s;
}

void Arena::prepareTeam() {
  G.team.clear();
  teamSlot_.clear();
  for (int i = 0; i < 6; i++)
    if (!allies[i].sp.empty()) {
      G.team.push_back(make(allies[i]));
      teamSlot_.push_back(i);
    }
  G.items.clear();
  if (withItems)
    for (auto [id, n] : std::vector<std::pair<std::string, int>>{{"potion", 5}, {"superpotion", 3}, {"ether", 3}, {"remede", 3}, {"plume", 2}, {"elixir", 1}})
      if (hasItem(id)) G.items[id] = n;
}

void Arena::open() {
  savedTeam_ = G.team;
  savedItems_ = G.items;
  savedGold_ = G.gold;
  loadPresets();
  menuMain();
}

void Arena::restore() {
  G.team = savedTeam_;
  G.items = savedItems_;
  G.gold = savedGold_;
}

void Arena::startManual() {
  if (!ready()) return;
  prepareTeam();
  G.menus.clear();
  t0_ = G.time;
  G.startBattle(makeSetup(), nullptr);
}

void Arena::onBattleEnd(BattleResult r, const std::vector<std::string>& log) {
  int s = (int)std::lround(G.time - t0_);
  lastResult = std::string(r == BattleResult::Win ? "Victoire" : r == BattleResult::Lose ? "Défaite" : "Fuite") + " en " + std::to_string(s) + " s";
  lastLog = log;
  G.menus.clear();
  menuMain(I_FIGHT);
}

// ---------------------------------------------------------------------------
// Simulation : les deux camps jouent tout seuls, plusieurs combats à la suite
// ---------------------------------------------------------------------------
void Arena::startSim(int n) {
  if (!ready()) return;
  results = Results{};
  simTarget_ = n;
  simRunning_ = true;
  G.menus.clear();
  nextSim();
}

void Arena::nextSim() {
  prepareTeam();
  G.items.clear();  // l'ordinateur n'utilise pas d'objets
  sim_ = std::make_unique<Battle>(G, makeSetup(), Theme(theme));
  sim_->autoPlay = true;
  sim_->simTactics = simTactics;
  simTime_ = 0;
}

void Arena::stepSim() {
  Uint32 start = SDL_GetTicks();
  do {
    for (int k = 0; k < 40 && !sim_->finished() && simTime_ < 900; k++) {
      G.in.confirm = true;
      sim_->update(.05f);
      simTime_ += .05f;
    }
    if (!sim_->finished() && simTime_ < 900) continue;
    Results& R = results;
    R.total++;
    R.time += simTime_;
    if (!sim_->finished()) R.timeouts++;
    else if (sim_->result() == BattleResult::Win) {
      R.wins++;
      int hp = 0, mhp = 0;
      for (auto& f : G.team) hp += f->hp, mhp += f->mhp;
      R.hpLeft += mhp ? double(hp) / mhp : 0;
    } else R.losses++;
    for (size_t j = 0; j < G.team.size(); j++)
      if (!G.team[j]->alive()) R.ko[teamSlot_[j]]++;
    lastLog = sim_->log;
    sim_.reset();
    if (R.total >= simTarget_) {
      simRunning_ = false;
      G.in.confirm = false;
      lastResult = "Simulation terminée";
      menuMain(I_SIM);
      return;
    }
    nextSim();
  } while (SDL_GetTicks() - start < 15);
}

void Arena::update(float) {
  if (simRunning_) {
    if (G.in.cancel) {  // Échap : arrête la simulation
      simRunning_ = false;
      sim_.reset();
      G.in.cancel = false;
      menuMain(I_SIM);
      return;
    }
    stepSim();
    return;
  }
  if (!G.menus.active()) menuMain();
  G.menus.update(G.in);
}

// ---------------------------------------------------------------------------
// Menus
// ---------------------------------------------------------------------------
void Arena::menuMain(int sel) {
  G.menus.clear();
  hover_ = -1;
  previewSp_.clear();
  Menu m;
  m.x = 4, m.y = 18, m.w = 152, m.rows = 14;
  m.sel = sel;
  m.onCancel = [this] {
    restore();
    G.titleMenu();
  };
  auto slotItems = [&](bool foe) {
    for (int i = 0; i < 6; i++) {
      ArenaSlot& s = slot(foe, i);
      std::string label = s.sp.empty() ? (i < 3 ? "—" : "— (renfort)") : species(s.sp).name;
      if (s.sp.empty() && !foe && i >= 3) label = "— (remplaçant)";
      std::string help = foe ? (i < 3 ? "Ennemi en première ligne." : "Renfort : entre quand un ennemi tombe.")
                             : (i < 3 ? "Allié en première ligne." : "Remplaçant : la commande « Changer » le fait entrer.");
      MenuItem it{label, "", help + " Entrée : modifier. B = boss.", true, [this, foe, i] { menuSlot(foe, i); },
                  [this, foe, i] {
                    hover_ = foe ? 6 + i : i;
                    previewSp_.clear();
                  }};
      it.rightFn = [this, foe, i] {
        ArenaSlot& s = slot(foe, i);
        if (s.sp.empty()) return std::string();
        return "N." + std::to_string(s.lvl) + (s.hpMult != 1.f ? " ×" + fmtMult(s.hpMult) : "") + (s.boss ? " B" : "");
      };
      m.items.push_back(it);
    }
  };
  auto unhover = [this] {
    hover_ = -1;
    previewSp_.clear();
  };
  m.items.push_back(menuHeader("Équipe"));
  slotItems(false);
  m.items.push_back(menuHeader("Adversaires"));
  slotItems(true);
  m.items.push_back(menuHeader("Options"));
  {
    MenuItem it{"Niveau équipe", "", "Gauche/droite : monte ou baisse le niveau de tous les alliés.", true, nullptr, unhover};
    it.adjust = [this](int d) {
      for (auto& a : allies)
        if (!a.sp.empty()) a.lvl = std::clamp(a.lvl + d, 1, 100);
    };
    it.rightFn = [this] {
      for (auto& a : allies)
        if (!a.sp.empty()) return "< " + std::to_string(a.lvl) + " >";
      return std::string("-");
    };
    m.items.push_back(it);
  }
  {
    MenuItem it{"Décor", "", "Gauche/droite : décor du combat.", true, nullptr, unhover};
    it.adjust = [this](int d) { theme = (theme + d + N_THEMES) % N_THEMES; };
    it.rightFn = [this] { return std::string("< ") + THEME_NAMES[theme] + " >"; };
    m.items.push_back(it);
  }
  {
    MenuItem it{"Objets", "", "Sac d'entraînement pour les combats à la main : potions, éthers, remèdes, plumes, élixir.", true,
                [this] { withItems = !withItems; }, unhover};
    it.adjust = [this](int) { withItems = !withItems; };
    it.rightFn = [this] { return std::string(withItems ? "Oui" : "Non"); };
    m.items.push_back(it);
  }
  {
    MenuItem it{"Alliés", "",
                "Simulations : les alliés suivent leurs tactiques (allié > Tactiques…), ou l'IA de l'ordinateur. À la main : mode auto avec Tab.",
                true, [this] { simTactics = !simTactics; }, unhover};
    it.adjust = [this](int) { simTactics = !simTactics; };
    it.rightFn = [this] { return std::string(simTactics ? "Tactiques" : "IA"); };
    m.items.push_back(it);
  }
  {
    MenuItem it{"Simuler", "", "Gauche/droite : nombre de combats. Entrée : les deux camps jouent tout seuls (Échap pour arrêter).",
                ready(), [this] { startSim(simN); }, unhover};
    it.adjust = [this](int d) { simN = std::clamp(simN + d * 10, 10, 500); };
    it.rightFn = [this] { return "< " + std::to_string(simN) + " >"; };
    m.items.push_back(it);
  }
  m.items.push_back({"Combattre !", "", "Vous jouez l'équipe, l'ordinateur joue les adversaires.", ready(), [this] { startManual(); }, unhover});
  m.items.push_back({"Modèles…", "", "Reprendre un combat de l'histoire ou une zone de rencontres.", true, [this] { menuPresets(); }, unhover});
  m.items.push_back({"Journal", "", "Détail du dernier combat : coups, dégâts, critiques, états.", !lastLog.empty(), [this] { menuLog(); }, unhover});
  m.items.push_back({"Quitter l'arène", "", "", true, [this] {
                       restore();
                       G.titleMenu();
                     }, unhover});
  G.menus.push(m);
}

void Arena::menuSlot(bool foe, int i, int sel) {
  ArenaSlot& s = slot(foe, i);
  hover_ = foe ? 6 + i : i;
  previewSp_.clear();
  auto refresh = [this, foe, i](int sel) {
    menuMain(foe ? I_FOE + i : I_ALLY + i);
    menuSlot(foe, i, sel);
  };
  Menu m;
  m.title = std::string(foe ? (i < 3 ? "Ennemi " : "Renfort ") : (i < 3 ? "Allié " : "Remplaçant ")) + std::to_string(i % 3 + 1);
  m.x = 70, m.y = 56, m.w = 170, m.rows = 6;
  m.sel = sel;
  m.items.push_back({"Espèce", s.sp.empty() ? "—" : species(s.sp).name, "Choisir le combattant.", true, [this, foe, i] {
                       G.menus.pop();
                       pickSpecies(foe, i);
                     }});
  {
    MenuItem it{"Niveau", "", "Gauche/droite pour changer le niveau.", !s.sp.empty()};
    it.adjust = [this, foe, i](int d) { slot(foe, i).lvl = std::clamp(slot(foe, i).lvl + d, 1, 100); };
    it.rightFn = [this, foe, i] { return "< " + std::to_string(slot(foe, i).lvl) + " >"; };
    m.items.push_back(it);
  }
  if (foe) {
    MenuItem it{"PV ×", "", "Multiplicateur de PV (les boss de l'histoire ont ×3 à ×5).", !s.sp.empty()};
    it.adjust = [this, foe, i](int d) { slot(foe, i).hpMult = std::clamp(slot(foe, i).hpMult + d * .5f, .5f, 20.f); };
    it.rightFn = [this, foe, i] { return "< " + fmtMult(slot(foe, i).hpMult) + " >"; };
    m.items.push_back(it);
    MenuItem b{"Boss", "", "Un boss rapporte plus d'expérience et d'or.", !s.sp.empty(), [this, foe, i] { slot(foe, i).boss = !slot(foe, i).boss; }};
    b.adjust = [this, foe, i](int) { slot(foe, i).boss = !slot(foe, i).boss; };
    b.rightFn = [this, foe, i] { return std::string(slot(foe, i).boss ? "Oui" : "Non"); };
    m.items.push_back(b);
  } else {
    int row = (int)m.items.size();
    MenuItem it{"Tactiques…", "", "Règles de combat de cet allié, pour les simulations et le mode auto.", !s.sp.empty(), [this, i, row] {
                  ArenaSlot& s = allies[i];
                  if (!s.customTactics) s.tactics = defaultTactics(species(s.sp));
                  TacticsTarget t;
                  t.list = &s.tactics;
                  t.on = &s.tacticsOn;
                  t.fighter = make(s);
                  t.title = "Tactiques de " + species(s.sp).name + " (Arène)";
                  t.slots = tacticSlots(s.lvl);
                  t.changed = [this, i] { allies[i].customTactics = true; };
                  t.closed = [this, i, row] {
                    menuMain(I_ALLY + i);
                    menuSlot(false, i, row);
                  };
                  openTacticsEditor(G, t);
                }};
    it.rightFn = [this, i] { return std::string(allies[i].customTactics ? "modifiées" : ""); };
    m.items.push_back(it);
  }
  m.items.push_back({"Vider", "", "Libère cet emplacement.", !s.sp.empty(), [this, foe, i, refresh] {
                       slot(foe, i) = ArenaSlot{"", slot(foe, i).lvl};
                       refresh(0);
                     }});
  m.items.push_back({"Retour", "", "", true, [this, foe, i] { menuMain(foe ? I_FOE + i : I_ALLY + i); }});
  m.onCancel = m.items.back().act;
  G.menus.push(m);
}

void Arena::pickSpecies(bool foe, int i) {
  Menu m;
  m.title = "Espèce";
  m.x = 4, m.y = 18, m.w = 152, m.rows = 14;
  int lvl = slot(foe, i).lvl;
  auto group = [&](const char* title, auto keep) {
    bool any = false;
    for (auto& s : allSpecies()) {
      if (!keep(s)) continue;
      if (!any) m.items.push_back(menuHeader(title));
      any = true;
      std::string id = s.id;
      if (id == slot(foe, i).sp) m.sel = (int)m.items.size();
      std::string help = s.human ? s.role : s.limit.empty() ? "Boss" : "Créature";
      help += " · " + typesName(s);
      m.items.push_back({s.name, "", help, true,
                         [this, foe, i, id] {
                           if (slot(foe, i).sp != id) slot(foe, i).customTactics = false, slot(foe, i).tactics.clear();
                           slot(foe, i).sp = id;
                           menuMain(foe ? I_FOE + i : I_ALLY + i);
                           menuSlot(foe, i, 0);
                         },
                         [this, id, lvl] {
                           previewSp_ = id;
                           previewLvl_ = lvl;
                         }});
    }
  };
  group("Héros", [](const Species& s) { return s.human && !s.limit.empty(); });
  group("Créatures", [](const Species& s) { return !s.human && !s.limit.empty(); });
  group("Boss", [](const Species& s) { return !s.human && s.limit.empty(); });
  group("Adversaires", [](const Species& s) { return s.human && s.limit.empty(); });
  m.onCancel = [this, foe, i] {
    previewSp_.clear();
    G.menus.pop();
    menuSlot(foe, i, 0);
  };
  G.menus.push(m);
}

void Arena::menuPresets() {
  Menu m;
  m.title = "Modèles de combat";
  m.x = 4, m.y = 18, m.w = 236, m.rows = 14;
  bool zones = false;
  m.items.push_back(menuHeader("Histoire"));
  for (size_t k = 0; k < presets_.size(); k++) {
    const Preset& p = presets_[k];
    if (!zones && p.info == "Rencontres sauvages") {
      m.items.push_back(menuHeader("Zones de rencontres"));
      zones = true;
    }
    std::string comp;
    for (auto* v : {&p.foes, &p.reserve})
      for (auto& f : *v) {
        if (!hasSpecies(f.sp)) continue;
        comp += (comp.empty() ? "" : v == &p.reserve ? " + renfort " : ", ") + species(f.sp).name + " N." + std::to_string(f.lvl) +
                (f.hpMult != 1.f ? " ×" + fmtMult(f.hpMult) : "");
      }
    m.items.push_back({utf8Prefix(p.name, 36), "", p.info + " : " + comp, true, [this, k] {
                         applyPreset((int)k);
                         menuMain(I_FOE);
                       }});
  }
  G.menus.push(m);
}

void Arena::menuLog() {
  Menu m;
  m.title = "Journal du dernier combat";
  m.x = 4, m.y = 4, m.w = 312, m.rows = 16;
  for (auto& l : lastLog) m.items.push_back({utf8Prefix(l, 47), "", l, true, nullptr});
  if (m.items.empty()) m.items.push_back({"(vide)", "", "", false, nullptr});
  G.menus.push(m);
}

// ---------------------------------------------------------------------------
// Dessin
// ---------------------------------------------------------------------------
static std::string multStr(float e) {
  if (e == 0) return "×0";
  return "×" + fmtMult(e);
}

void drawFighterCard(Gfx& g, const Fighter& f, int x, int y, int w, int h, float t) {
  const Species& s = f.S();
  g.window(x, y, w, h);
  g.text(x + 8, y + 5, s.name, GOLD);
  g.text(x + w - 8, y + 5, "N." + std::to_string(f.lvl), WHITE, 2);
  // Types en couleur
  int tx = x + 8;
  for (Type t : {s.type, s.type2}) {
    if (t < 0) continue;
    if (tx > x + 8) g.text(tx, y + 17, "/", MUTED), tx += 6;
    g.text(tx, y + 17, typeName(t), rgb(types()[t].color));
    tx += Gfx::textW(typeName(t));
  }
  drawFighterSprite(g, f, x + w - 24, y + 44, .7f, false, t);
  int ly = y + 30;
  g.text(x + 8, ly, "PV " + std::to_string(f.mhp), WHITE), ly += 11;
  g.text(x + 8, ly, "PM " + std::to_string(f.mmp), WHITE), ly += 13;
  auto pair = [&](const char* a, int va, const char* b, const std::string& vb) {
    g.text(x + 8, ly, a, MUTED);
    g.text(x + 68, ly, std::to_string(va), WHITE, 2);
    g.text(x + 76, ly, b, MUTED);
    g.text(x + w - 8, ly, vb, WHITE, 2);
    ly += 11;
  };
  pair("Attaque", f.atk, "Défense", std::to_string(f.def));
  pair("Magie", f.mag, "Résist.", std::to_string(f.res));
  pair("Vitesse", f.spd, "Précis.", std::to_string(f.acc) + "%");
  pair("Esquive", f.eva, "Critique", std::to_string(f.crit) + "%");
  ly += 2;
  // Faiblesses et résistances (types, coups physiques, magie, états)
  std::string weak, strong, cat, imm;
  for (Type t = 0; t < (Type)types().size(); t++) {
    Move probe{"", "", t, 0, Target::Enemy, Kind::Status, 0, ""};
    float e = moveEff(probe, s);
    if (e > 1) weak += (weak.empty() ? "" : ", ") + std::string(typeName(t)) + " " + multStr(e);
    if (e < 1) strong += (strong.empty() ? "" : ", ") + std::string(typeName(t)) + " " + multStr(e);
  }
  for (auto& [k, v] : s.resist)
    if (k == "physique" || k == "magique") cat += (cat.empty() ? "" : ", ") + std::string(k == "physique" ? "coups" : "magie") + " " + multStr(v);
  for (Status st : {Status::Poison, Status::Burn, Status::Paralysis, Status::Sleep})
    if (immuneTo(s, st)) imm += (imm.empty() ? "" : ", ") + std::string(statusName(st));
  auto para = [&](const std::string& t, Color c) {
    for (auto& l : Gfx::wrap(t, w - 16)) {
      if (ly > y + h - 14) return;
      g.text(x + 8, ly, l, c);
      ly += 10;
    }
  };
  if (!weak.empty()) para("Faible : " + weak, RED);
  if (!strong.empty()) para("Résiste : " + strong, GREEN);
  if (!cat.empty()) para("Dégâts reçus : " + cat, MUTED);
  if (!imm.empty()) para("Insensible : " + imm, MUTED);
  std::string tech;
  for (auto& id : f.techs()) tech += (tech.empty() ? "" : ", ") + moveInfo(id).name;
  for (auto& id : f.spells()) tech += (tech.empty() ? "" : ", ") + moveInfo(id).name;
  para("Tech. : " + tech, WHITE);
}

void Arena::drawSummary(int x, int y, int w) {
  Gfx& g = G.g;
  g.window(x, y, w, 190);
  g.text(x + 8, y + 5, "Équipe", GOLD);
  int k = 0;
  for (int i = 0; i < 3; i++)
    if (!allies[i].sp.empty()) {
      auto f = make(allies[i]);
      drawFighterSprite(g, *f, x + 28 + k * 48.f, y + 40, .55f, false, G.time + k);
      k++;
    }
  g.text(x + 8, y + 62, "Adversaires", GOLD);
  k = 0;
  for (int i = 0; i < 3; i++)
    if (!foes[i].sp.empty()) {
      auto f = make(foes[i]);
      drawFighterSprite(g, *f, x + 28 + k * 48.f, y + 98, f->boss ? .7f : .55f, true, G.time + k);
      k++;
    }
  int ly = y + 122;
  if (!lastResult.empty()) g.text(x + 8, ly, lastResult, WHITE), ly += 12;
  const Results& R = results;
  if (R.total > 0) {
    g.text(x + 8, ly, "Gagnés : " + pct(R.wins, R.total) + " (" + std::to_string(R.wins) + "/" + std::to_string(R.total) + ")",
           R.wins * 2 >= R.total ? GREEN : RED);
    ly += 11;
    g.text(x + 8, ly, "Durée moyenne : " + std::to_string((int)std::lround(R.time / R.total)) + " s", WHITE), ly += 11;
    if (R.wins) g.text(x + 8, ly, "PV restants : " + pct(R.hpLeft, R.wins), WHITE), ly += 11;
    std::string ko;
    for (int i = 0; i < 6; i++)
      if (!allies[i].sp.empty() && R.ko[i]) ko += (ko.empty() ? "" : ", ") + utf8Prefix(species(allies[i].sp).name, 6) + " " + pct(R.ko[i], R.total);
    if (!ko.empty())
      for (auto& l : Gfx::wrap("K.O. : " + ko, w - 16)) g.text(x + 8, ly, l, MUTED), ly += 10;
  } else if (lastResult.empty())
    for (auto& l : Gfx::wrap("Composez les équipes, puis « Simuler » ou « Combattre ! ».", w - 16)) g.text(x + 8, ly, l, MUTED), ly += 10;
}

void Arena::draw() {
  Gfx& g = G.g;
  g.gradV(0, 0, SCREEN_W, SCREEN_H, rgb(0x231638), rgb(0x47305e));
  g.ellipse(160, 250, 210, 60, rgb(0x2d1f46));
  g.ellipse(160, 250, 170, 40, rgb(0x3a2a52));
  g.text(160, 4, "ARÈNE DE COMBAT", GOLD, 1);
  if (simRunning_) {
    g.window(60, 90, 200, 60);
    g.text(160, 98, "Simulation en cours…", WHITE, 1);
    float r = simTarget_ ? float(results.total) / simTarget_ : 0;
    g.rect(80, 116, 160, 6, rgb(0x0a0f33));
    g.rect(80, 116, 160 * r, 6, GOLD);
    g.text(160, 128, std::to_string(results.total) + " / " + std::to_string(simTarget_) + "  ·  " + std::to_string(results.wins) + " victoires",
           MUTED, 1);
    g.text(160, 156, "Échap : arrêter", MUTED, 1);
    return;
  }
  // Panneau de droite : fiche du combattant survolé, ou résumé
  if (!previewSp_.empty() && hasSpecies(previewSp_)) drawFighterCard(g, *makeFighter(previewSp_, previewLvl_), 160, 18, 156, 190, G.time);
  else if (hover_ >= 0 && !slot(hover_ >= 6, hover_ % 6).sp.empty())
    drawFighterCard(g, *make(slot(hover_ >= 6, hover_ % 6)), 160, 18, 156, 190, G.time);
  else drawSummary(160, 18, 156);
  G.menus.draw(g, G.time);
  std::string h = G.menus.help();
  if (!h.empty()) {
    auto lines = Gfx::wrap(h, 300);
    int hh = 8 + 11 * (int)lines.size();
    g.window(4, 238 - hh, 312, hh);
    for (size_t i = 0; i < lines.size(); i++) g.text(10, 238 - hh + 4 + i * 11, lines[i], MUTED);
  }
}
