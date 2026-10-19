#include "i18n.hpp"
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
static const int N_BATTLE_THEMES = 5;  // décors de combat (pas d'intérieur)

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
  int p = findPreset(T("Sylvarque"));
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
        p.name = tr(jget<std::string>(a, "nom", leader));
        for (auto& q : presets_)
          if (q.name == p.name) p.name += " (" + id + ")";
        p.info = T("Événement « ") + id + " »";
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
      p.name = m.name + T(" (N.") + std::to_string(z.lo) + "-" + std::to_string(z.hi) + ")";
      p.info = T("Rencontres sauvages");
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
  lastResult = std::string(r == BattleResult::Win ? T("Victoire") : r == BattleResult::Lose ? T("Défaite") : T("Fuite")) + T(" en ") + std::to_string(s) + T(" s");
  lastLog = log;
  G.menus.clear();
  if (challenge_ >= 0) {  // défi : retour à la liste des défis (réussi : coché)
    int i = challenge_;
    challenge_ = -1;
    Json d = readJson("defis.json");
    if (r == BattleResult::Win && i < (int)d.size()) challengesWon_.insert(jget<std::string>(d[(size_t)i], "id", ""));
    menuMain(I_FIGHT);
    menuChallenges(i);
    return;
  }
  menuMain(I_FIGHT);
}

int Arena::challengeCount() const {
  Json d = readJson("defis.json");
  return d.is_array() ? (int)d.size() : 0;
}

void Arena::menuChallenges(int sel) {
  Json d = readJson("defis.json");
  Menu m;
  m.title = T("Défis de l'échiquier");
  m.x = 4, m.y = 18, m.w = 236, m.rows = 10;
  for (size_t i = 0; i < d.size(); i++) {
    std::string id = jget<std::string>(d[i], "id", "");
    std::string help = tr(jget<std::string>(d[i], "description", ""));
    m.items.push_back({tr(jget<std::string>(d[i], "nom", id)), challengesWon_.count(id) ? T("réussi") : "", help, true,
                       [this, i] { startChallenge((int)i); }});
  }
  m.items.push_back({T("Retour"), "", "", true, [this] { G.menus.pop(); }});
  m.sel = std::clamp(sel, 0, (int)m.items.size() - 1);
  G.menus.push(m);
}

void Arena::startChallenge(int i) {
  Json d = readJson("defis.json");
  if (i < 0 || i >= (int)d.size()) return;
  const Json& c = d[(size_t)i];
  BattleSetup s;
  auto cell = [](const Json& e) {
    Json p = e.value("case", Json::array({1, 1}));
    return boardAt(std::clamp(p.at(0).get<int>(), 0, GRID - 1), std::clamp(p.at(1).get<int>(), 0, BOARD_W - 1));
  };
  for (auto& e : c.value("allies", Json::array())) {
    s.allies.push_back(makeFighter(e.at("espece").get<std::string>(), jget(e, "niveau", 10)));
    s.allyCells.push_back(cell(e));
  }
  for (auto& e : c.value("ennemis", Json::array())) {
    s.foes.push_back(makeFoe(e, 10));
    s.foeCells.push_back(cell(e));
  }
  for (auto& o : c.value("obstacles", Json::array())) {  // seulement sur le plateau, sur une case libre
    int y = o.at(0).get<int>(), x = o.at(1).get<int>(), b = boardAt(y, x);
    bool free = y >= 0 && y < GRID && x >= 0 && x < BOARD_W;
    for (int t : s.allyCells) free = free && t != b;
    for (int t : s.foeCells) free = free && t != b;
    if (free) s.obstacleCells.push_back(b);
  }
  s.fixedBoard = true;
  s.canCapture = false;
  s.theme = (int)themeOf(jget<std::string>(c, "decor", "vallee"));
  s.intro = T("Défi : ") + tr(jget<std::string>(c, "nom", "")) + ". " + tr(jget<std::string>(c, "description", ""));
  G.team = s.allies;  // personne en réserve, pas d'objets : la position fait tout
  teamSlot_.clear();
  G.items.clear();
  challenge_ = i;
  G.menus.clear();
  t0_ = G.time;
  G.startBattle(std::move(s), nullptr);
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
      lastResult = T("Simulation terminée");
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
      std::string label = s.sp.empty() ? (i < 3 ? "—" : T("— (renfort)")) : species(s.sp).name;
      if (s.sp.empty() && !foe && i >= 3) label = T("— (remplaçant)");
      std::string help = foe ? (i < 3 ? T("Ennemi en première ligne.") : T("Renfort : entre quand un ennemi tombe."))
                             : (i < 3 ? T("Allié en première ligne.") : T("Remplaçant : la commande « Changer » le fait entrer."));
      MenuItem it{label, "", help + T(" Entrée : modifier. B = boss."), true, [this, foe, i] { menuSlot(foe, i); },
                  [this, foe, i] {
                    hover_ = foe ? 6 + i : i;
                    previewSp_.clear();
                  }};
      it.rightFn = [this, foe, i] {
        ArenaSlot& s = slot(foe, i);
        if (s.sp.empty()) return std::string();
        return T("N.") + std::to_string(s.lvl) + (s.hpMult != 1.f ? T(" ×") + fmtMult(s.hpMult) : "") + (s.boss ? T(" B") : "");
      };
      it.shrink = true;  // nom complet sur la fiche de droite
      m.items.push_back(it);
    }
  };
  auto unhover = [this] {
    hover_ = -1;
    previewSp_.clear();
  };
  m.items.push_back(menuHeader(T("Équipe")));
  slotItems(false);
  m.items.push_back(menuHeader(T("Adversaires")));
  slotItems(true);
  m.items.push_back(menuHeader(T("Options")));
  {
    MenuItem it{T("Niveau équipe"), "", T("Gauche/droite : monte ou baisse le niveau de tous les alliés."), true, nullptr, unhover};
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
    MenuItem it{T("Décor"), "", T("Gauche/droite : décor du combat."), true, nullptr, unhover};
    it.adjust = [this](int d) { theme = (theme + d + N_BATTLE_THEMES) % N_BATTLE_THEMES; };
    it.rightFn = [this] { return std::string("< ") + T(THEME_NAMES[theme]) + " >"; };
    m.items.push_back(it);
  }
  {
    MenuItem it{T("Objets"), "", T("Sac d'entraînement pour les combats à la main : potions, éthers, remèdes, plumes, élixir."), true,
                [this] { withItems = !withItems; }, unhover};
    it.adjust = [this](int) { withItems = !withItems; };
    it.rightFn = [this] { return std::string(withItems ? T("Oui") : T("Non")); };
    m.items.push_back(it);
  }
  {
    MenuItem it{T("Alliés"), "",
                T("Simulations : les alliés suivent leurs tactiques (allié > Tactiques…), ou l'IA de l'ordinateur. À la main : mode auto avec Tab."),
                true, [this] { simTactics = !simTactics; }, unhover};
    it.adjust = [this](int) { simTactics = !simTactics; };
    it.rightFn = [this] { return std::string(simTactics ? T("Tactiques") : T("IA")); };
    m.items.push_back(it);
  }
  {
    MenuItem it{T("Simuler"), "", T("Gauche/droite : nombre de combats. Entrée : les deux camps jouent tout seuls (Échap pour arrêter)."),
                ready(), [this] { startSim(simN); }, unhover};
    it.adjust = [this](int d) { simN = std::clamp(simN + d * 10, 10, 500); };
    it.rightFn = [this] { return "< " + std::to_string(simN) + " >"; };
    m.items.push_back(it);
  }
  m.items.push_back({T("Combattre !"), "", T("Vous jouez l'équipe, l'ordinateur joue les adversaires."), ready(), [this] { startManual(); }, unhover});
  m.items.push_back({T("Modèles…"), "", T("Reprendre un combat de l'histoire ou une zone de rencontres."), true, [this] { menuPresets(); }, unhover});
  m.items.push_back({T("Défis de l'échiquier…"), "", T("Des positions imposées, une leçon chacune : dans le dos, Percée, tenir une ligne, pousser."),
                     challengeCount() > 0, [this] { menuChallenges(); }, unhover});
  m.items.push_back({T("Journal"), "", T("Détail du dernier combat : coups, dégâts, critiques, états."), !lastLog.empty(), [this] { menuLog(); }, unhover});
  m.items.push_back({T("Quitter l'arène"), "", "", true, [this] {
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
  m.title = std::string(foe ? (i < 3 ? T("Ennemi ") : T("Renfort ")) : (i < 3 ? T("Allié ") : T("Remplaçant "))) + std::to_string(i % 3 + 1);
  m.x = 10, m.y = 56, m.w = 140, m.rows = 7;  // à l'intérieur de la liste, sans toucher la fiche de droite
  m.sel = sel;
  m.items.push_back({s.sp.empty() ? T("Choisir l'espèce") : species(s.sp).name, ">", T("Choisir le combattant."), true, [this, foe, i] {
                       G.menus.pop();
                       pickSpecies(foe, i);
                     }});
  {
    MenuItem it{T("Niveau"), "", T("Gauche/droite pour changer le niveau."), !s.sp.empty()};
    it.adjust = [this, foe, i](int d) { slot(foe, i).lvl = std::clamp(slot(foe, i).lvl + d, 1, 100); };
    it.rightFn = [this, foe, i] { return "< " + std::to_string(slot(foe, i).lvl) + " >"; };
    m.items.push_back(it);
  }
  if (foe) {
    MenuItem it{T("PV ×"), "", T("Multiplicateur de PV (les boss de l'histoire ont ×3 à ×5)."), !s.sp.empty()};
    it.adjust = [this, foe, i](int d) { slot(foe, i).hpMult = std::clamp(slot(foe, i).hpMult + d * .5f, .5f, 20.f); };
    it.rightFn = [this, foe, i] { return "< " + fmtMult(slot(foe, i).hpMult) + " >"; };
    m.items.push_back(it);
    MenuItem b{T("Boss"), "", T("Un boss rapporte plus d'expérience et d'or."), !s.sp.empty(), [this, foe, i] { slot(foe, i).boss = !slot(foe, i).boss; }};
    b.adjust = [this, foe, i](int) { slot(foe, i).boss = !slot(foe, i).boss; };
    b.rightFn = [this, foe, i] { return std::string(slot(foe, i).boss ? T("Oui") : T("Non")); };
    m.items.push_back(b);
  } else {
    int row = (int)m.items.size();
    MenuItem it{T("Tactiques…"), "", T("Règles de combat de cet allié, pour les simulations et le mode auto."), !s.sp.empty(), [this, i, row] {
                  ArenaSlot& s = allies[i];
                  if (!s.customTactics) s.tactics = defaultTactics(species(s.sp));
                  TacticsTarget t;
                  t.list = &s.tactics;
                  t.on = &s.tacticsOn;
                  t.fighter = make(s);
                  t.title = T("Tactiques de ") + species(s.sp).name + T(" (Arène)");
                  t.slots = tacticSlots(s.lvl);
                  t.changed = [this, i] { allies[i].customTactics = true; };
                  t.closed = [this, i, row] {
                    menuMain(I_ALLY + i);
                    menuSlot(false, i, row);
                  };
                  openTacticsEditor(G, t);
                }};
    it.rightFn = [this, i] { return std::string(allies[i].customTactics ? T("modifiées") : ""); };
    m.items.push_back(it);
  }
  m.items.push_back({T("Vider"), "", T("Libère cet emplacement."), !s.sp.empty(), [this, foe, i, refresh] {
                       slot(foe, i) = ArenaSlot{"", slot(foe, i).lvl};
                       refresh(0);
                     }});
  m.items.push_back({T("Retour"), "", "", true, [this, foe, i] { menuMain(foe ? I_FOE + i : I_ALLY + i); }});
  m.onCancel = m.items.back().act;
  G.menus.push(m);
}

void Arena::pickSpecies(bool foe, int i) {
  Menu m;
  m.title = T("Espèce");
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
      std::string help = s.human ? s.role : s.limit.empty() ? T("Boss") : T("Créature");
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
  group(T("Héros"), [](const Species& s) { return s.human && !s.limit.empty(); });
  group(T("Créatures"), [](const Species& s) { return !s.human && !s.limit.empty(); });
  group(T("Boss"), [](const Species& s) { return !s.human && s.limit.empty(); });
  group(T("Adversaires"), [](const Species& s) { return s.human && s.limit.empty(); });
  m.onCancel = [this, foe, i] {
    previewSp_.clear();
    G.menus.pop();
    menuSlot(foe, i, 0);
  };
  G.menus.push(m);
}

void Arena::menuPresets() {
  Menu m;
  m.title = T("Modèles de combat");
  m.x = 4, m.y = 18, m.w = 236, m.rows = 14;
  bool zones = false;
  m.items.push_back(menuHeader(T("Histoire")));
  for (size_t k = 0; k < presets_.size(); k++) {
    const Preset& p = presets_[k];
    if (!zones && p.info == T("Rencontres sauvages")) {
      m.items.push_back(menuHeader(T("Zones de rencontres")));
      zones = true;
    }
    std::string comp;
    for (auto* v : {&p.foes, &p.reserve})
      for (auto& f : *v) {
        if (!hasSpecies(f.sp)) continue;
        comp += (comp.empty() ? "" : v == &p.reserve ? T(" + renfort ") : ", ") + species(f.sp).name + T(" N.") + std::to_string(f.lvl) +
                (f.hpMult != 1.f ? T(" ×") + fmtMult(f.hpMult) : "");
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
  m.title = T("Journal du dernier combat");
  m.x = 4, m.y = 4, m.w = 312, m.rows = 15;  // laisse la place à une aide sur deux lignes
  for (auto& l : lastLog) m.items.push_back({utf8Prefix(l, 47), "", l, true, nullptr});
  if (m.items.empty()) m.items.push_back({T("(vide)"), "", "", false, nullptr});
  G.menus.push(m);
}

// ---------------------------------------------------------------------------
// Dessin
// ---------------------------------------------------------------------------
static std::string multStr(float e) {
  if (e == 0) return T("×0");
  return T("×") + fmtMult(e);
}

void drawFighterCard(Gfx& g, const Fighter& f, int x, int y, int w, int h, float t) {
  const Species& s = f.S();
  g.window(x, y, w, h);
  g.text(x + 8, y + 5, s.name, GOLD);
  g.text(x + w - 8, y + 5, T("N.") + std::to_string(f.lvl), WHITE, 2);
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
  g.text(x + 8, ly, T("PV ") + std::to_string(f.mhp), WHITE), ly += 11;
  g.text(x + 8, ly, T("PM ") + std::to_string(f.mmp), WHITE), ly += 13;
  auto pair = [&](const char* a, int va, const char* b, const std::string& vb) {
    g.text(x + 8, ly, a, MUTED);
    g.text(x + 68, ly, std::to_string(va), WHITE, 2);
    g.text(x + 76, ly, b, MUTED);
    g.text(x + w - 8, ly, vb, WHITE, 2);
    ly += 11;
  };
  pair(T("Attaque"), f.atk, T("Défense"), std::to_string(f.def));
  pair(T("Magie"), f.mag, T("Résist."), std::to_string(f.res));
  pair(T("Vitesse"), f.spd, T("Précis."), std::to_string(f.acc) + "%");
  pair(T("Esquive"), f.eva, T("Critique"), std::to_string(f.crit) + "%");
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
  if (!weak.empty()) para(T("Faible : ") + weak, RED);
  if (!strong.empty()) para(T("Résiste : ") + strong, GREEN);
  if (!cat.empty()) para(T("Dégâts reçus : ") + cat, MUTED);
  if (!imm.empty()) para(T("Insensible : ") + imm, MUTED);
  para(T("Échiquier : ") + std::to_string(fighterMoveRange(f)) + T(" cases par tour") + (speciesFlies(s) ? T(", vole") : ""), MUTED);
  std::string tech;
  for (auto& id : f.techs()) tech += (tech.empty() ? "" : ", ") + moveInfo(id).name;
  for (auto& id : f.spells()) tech += (tech.empty() ? "" : ", ") + moveInfo(id).name;
  para(T("Tech. : ") + tech, WHITE);
}

void Arena::drawSummary(int x, int y, int w) {
  Gfx& g = G.g;
  g.window(x, y, w, 190);
  g.text(x + 8, y + 5, T("Équipe"), GOLD);
  int k = 0;
  for (int i = 0; i < 3; i++)
    if (!allies[i].sp.empty()) {
      auto f = make(allies[i]);
      drawFighterSprite(g, *f, x + 28 + k * 48.f, y + 40, .55f, false, G.time + k);
      k++;
    }
  g.text(x + 8, y + 62, T("Adversaires"), GOLD);
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
    g.text(x + 8, ly, T("Gagnés : ") + pct(R.wins, R.total) + " (" + std::to_string(R.wins) + "/" + std::to_string(R.total) + ")",
           R.wins * 2 >= R.total ? GREEN : RED);
    ly += 11;
    g.text(x + 8, ly, T("Durée moyenne : ") + std::to_string((int)std::lround(R.time / R.total)) + T(" s"), WHITE), ly += 11;
    if (R.wins) g.text(x + 8, ly, T("PV restants : ") + pct(R.hpLeft, R.wins), WHITE), ly += 11;
    std::string ko;
    for (int i = 0; i < 6; i++)
      if (!allies[i].sp.empty() && R.ko[i]) ko += (ko.empty() ? "" : ", ") + utf8Prefix(species(allies[i].sp).name, 6) + " " + pct(R.ko[i], R.total);
    if (!ko.empty())
      for (auto& l : Gfx::wrap(T("K.O. : ") + ko, w - 16)) g.text(x + 8, ly, l, MUTED), ly += 10;
  } else if (lastResult.empty())
    for (auto& l : Gfx::wrap(T("Composez les équipes, puis « Simuler » ou « Combattre ! »."), w - 16)) g.text(x + 8, ly, l, MUTED), ly += 10;
}

void Arena::draw() {
  Gfx& g = G.g;
  g.gradV(g.left(), 0, g.fullW, SCREEN_H, rgb(0x231638), rgb(0x47305e));
  g.ellipse(160, 250, 210 + g.ox, 60, rgb(0x2d1f46));
  g.ellipse(160, 250, 170 + g.ox, 40, rgb(0x3a2a52));
  g.text(160, 4, T("ARÈNE DE COMBAT"), GOLD, 1);
  if (simRunning_) {
    g.window(60, 90, 200, 60);
    g.text(160, 98, T("Simulation en cours…"), WHITE, 1);
    float r = simTarget_ ? float(results.total) / simTarget_ : 0;
    g.rect(80, 116, 160, 6, rgb(0x0a0f33));
    g.rect(80, 116, 160 * r, 6, GOLD);
    g.text(160, 128, std::to_string(results.total) + " / " + std::to_string(simTarget_) + "  ·  " + std::to_string(results.wins) + T(" victoires"),
           MUTED, 1);
    g.text(160, 156, T("Échap : arrêter"), MUTED, 1);
    return;
  }
  // Panneau de droite : fiche du combattant survolé, ou résumé (caché par un menu large)
  bool wide = G.menus.maxRight() > 158;
  if (!wide) {
    if (!previewSp_.empty() && hasSpecies(previewSp_)) drawFighterCard(g, *makeFighter(previewSp_, previewLvl_), 160, 18, 156, 190, G.time);
    else if (hover_ >= 0 && !slot(hover_ >= 6, hover_ % 6).sp.empty())
      drawFighterCard(g, *make(slot(hover_ >= 6, hover_ % 6)), 160, 18, 156, 190, G.time);
    else drawSummary(160, 18, 156);
  }
  G.menus.draw(g, G.time);
  std::string h = G.menus.help();
  if (!h.empty()) {
    auto lines = Gfx::wrap(h, 300);
    int hh = 8 + 11 * (int)lines.size();
    g.window(4, 238 - hh, 312, hh);
    for (size_t i = 0; i < lines.size(); i++) g.text(10, 238 - hh + 4 + i * 11, lines[i], MUTED);
  }
}
