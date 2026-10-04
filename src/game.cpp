#include "game.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

#include "battle.hpp"
#include "sprites.hpp"

static const Color WHITE = rgb(0xffffff), GOLD = rgb(0xffd34d), MUTED = rgb(0xaab3d8);

Game::Game(SDL_Renderer* r) : g(r) { titleMenu(); }
Game::~Game() = default;

// ---------------------------------------------------------------------------
// Clavier : flèches ou ZQSD (WASD en QWERTY), Entrée/Espace pour valider,
// Échap pour annuler et ouvrir le menu.
// ---------------------------------------------------------------------------
void Game::onKey(SDL_Scancode k, bool down, bool repeat) {
  int d = -1;
  if (k == SDL_SCANCODE_UP || k == SDL_SCANCODE_W) d = UP;
  if (k == SDL_SCANCODE_DOWN || k == SDL_SCANCODE_S) d = DOWN;
  if (k == SDL_SCANCODE_LEFT || k == SDL_SCANCODE_A) d = LEFT;
  if (k == SDL_SCANCODE_RIGHT || k == SDL_SCANCODE_D) d = RIGHT;
  if (d >= 0) {
    if (!repeat) in.hold[d] = down;
    if (down) in.press[d] = true;
    return;
  }
  if (!down || repeat) return;
  if (k == SDL_SCANCODE_RETURN || k == SDL_SCANCODE_KP_ENTER || k == SDL_SCANCODE_SPACE) in.confirm = true;
  if (k == SDL_SCANCODE_ESCAPE || k == SDL_SCANCODE_BACKSPACE) in.cancel = in.menu = true;
  if (k == SDL_SCANCODE_TAB) in.menu = true;
}

// ---------------------------------------------------------------------------
// Boucle principale
// ---------------------------------------------------------------------------
void Game::update(float dt) {
  time += dt;
  banner -= dt;
  noticeT_ -= dt;
  switch (mode) {
    case Mode::Title: menus.update(in); break;
    case Mode::Ending:
      if (in.confirm) {
        mode = Mode::Map;
        notice("Merci d'avoir joué ! Vous pouvez continuer l'exploration.");
      }
      break;
    case Mode::Battle: {
      battle_->update(dt);
      if (battle_->finished()) {
        BattleResult r = battle_->result();
        battle_.reset();
        mode = Mode::Map;
        menus.clear();
        steps = 0;
        auto after = afterBattle_;
        afterBattle_ = nullptr;
        if (r == BattleResult::Lose) defeat();
        if (after) after(r);
      }
      break;
    }
    case Mode::Map: {
      if (menus.active()) {
        menus.update(in);
        if (!menus.active()) panelMode_ = 0;
        break;
      }
      if (sc.busy()) {
        sc.update(dt, in);
        break;
      }
      if (moving) {
        moveT += dt * 7.5f;
        if (moveT >= 1) {
          moving = false;
          arrive();
        }
        break;
      }
      if (in.menu) {
        pauseMenu();
        break;
      }
      if (in.confirm) {
        interact();
        break;
      }
      for (int d : {UP, DOWN, LEFT, RIGHT})
        if (in.hold[d] || in.press[d]) {
          tryMove(d);
          break;
        }
      break;
    }
  }
  in.endFrame();
}

// ---------------------------------------------------------------------------
// Équipe
// ---------------------------------------------------------------------------
std::vector<FighterP> Game::front() const {
  std::vector<FighterP> v;
  for (auto& f : team)
    if (f->alive() && v.size() < 3) v.push_back(f);
  return v;
}
int Game::avgLevel() const {
  if (team.empty()) return 1;
  int s = 0, n = 0;
  for (auto& f : front()) s += f->lvl, n++;
  return n ? (s + n / 2) / n : 1;
}
void Game::healAll(bool mpToo) {
  for (auto& f : team) {
    f->hp = f->mhp;
    if (mpToo) f->mp = f->mmp;
  }
}
void Game::recruit(const std::string& id, int minLvl) {
  auto f = makeFighter(id, std::max(minLvl, avgLevel()));
  flags.insert(id);
  size_t at = std::min<size_t>(2, team.size());
  team.insert(team.begin() + at, f);
  sc.say(f->name() + " rejoint l'équipe et prend place en première ligne !");
  sc.say("(Échap > Équipe pour choisir qui combat. " + f->S().role + ".)");
}

void Game::startBattle(std::vector<FighterP> foes, bool boss, std::function<void(BattleResult)> after, bool canFlee, bool canCapture) {
  afterBattle_ = std::move(after);
  battle_ = std::make_unique<Battle>(*this, std::move(foes), boss, M().theme, canFlee, canCapture);
  mode = Mode::Battle;
  menus.clear();
}

void Game::defeat() {
  healAll();
  changeMap(respawnMap, respawnX, respawnY, DOWN);
  sc.say("Vous reprenez connaissance chez la guérisseuse. Toute l'équipe est soignée.");
}

// ---------------------------------------------------------------------------
// Carte
// ---------------------------------------------------------------------------
bool Game::npcVisible(const Npc& n) const {
  if (n.script == "maelle" || n.script == "brann" || n.script == "isra") return !has(n.script);
  return true;
}
bool Game::bossAlive(const BossSpot& b) const { return !has(b.flag); }

bool Game::blocked(int x, int y) const {
  const MapDef& m = M();
  if (x < 0 || y < 0 || x >= m.w() || y >= m.h()) return true;
  if (!tileWalkable(m.rows[y][x])) return true;
  for (auto& b : m.buildings)
    if (x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h) return true;
  for (auto& n : m.npcs)
    if (n.x == x && n.y == y && npcVisible(n)) return true;
  for (auto& c : m.chests)
    if (c.x == x && c.y == y) return true;
  for (auto& s : m.signs)
    if (s.x == x && s.y == y) return true;
  for (auto& b : m.bosses)
    if (bossAlive(b) && x >= b.x && x <= b.x + 1 && y >= b.y && y <= b.y + 1) return true;
  return false;
}

void Game::tryMove(int d) {
  bool fresh = in.press[d];
  dir = d;
  int dx = d == LEFT ? -1 : d == RIGHT ? 1 : 0, dy = d == UP ? -1 : d == DOWN ? 1 : 0;
  int nx = px + dx, ny = py + dy;
  if (blocked(nx, ny)) {
    if (!fresh) return;
    for (auto& b : M().buildings)
      if (nx == b.doorX() && ny == b.doorY()) return door(b);
    for (auto& b : M().bosses)
      if (bossAlive(b) && nx >= b.x && nx <= b.x + 1 && ny >= b.y && ny <= b.y + 1) return bossEvent(b);
    return;
  }
  fromX = px, fromY = py;
  px = nx, py = ny;
  moving = true;
  moveT = 0;
}

void Game::arrive() {
  steps++;
  for (auto& w : M().warps)
    if (w.x == px && w.y == py) return changeMap(w.map, w.tx, w.ty, w.dir);
  char c = M().rows[py][px];
  float rate = M().theme == Theme::Grotte ? .08f : .11f;
  if (tileEncounter(c) && steps > 3 && frand() < rate) encounter();
}

void Game::encounter() {
  for (auto& z : M().zones) {
    if (px < z.x || py < z.y || px >= z.x + z.w || py >= z.y + z.h) continue;
    int n = 1 + (frand() < .55f) + (frand() < .35f);
    n = std::min(n, z.maxN);
    std::vector<FighterP> foes;
    for (int i = 0; i < n; i++) foes.push_back(makeFighter(z.pool[irand(0, (int)z.pool.size() - 1)], irand(z.lo, z.hi)));
    startBattle(foes, false, nullptr);
    return;
  }
}

void Game::changeMap(int m, int x, int y, int d) {
  bool changed = m != mapId;
  mapId = m;
  px = x, py = y, dir = d;
  moving = false;
  steps = 0;
  if (changed) showRegionBanner();
}
void Game::showRegionBanner() {
  bannerText = M().name;
  banner = 2.6f;
}

void Game::interact() {
  int dx = dir == LEFT ? -1 : dir == RIGHT ? 1 : 0, dy = dir == UP ? -1 : dir == DOWN ? 1 : 0;
  int fx = px + dx, fy = py + dy;
  const MapDef& m = M();
  for (auto& n : m.npcs)
    if (n.x == fx && n.y == fy && npcVisible(n)) return talk(n);
  for (auto& s : m.signs)
    if (s.x == fx && s.y == fy) return sc.say(s.text);
  for (size_t i = 0; i < m.chests.size(); i++)
    if (m.chests[i].x == fx && m.chests[i].y == fy) return openChest((int)i);
  for (auto& b : m.bosses)
    if (bossAlive(b) && fx >= b.x && fx <= b.x + 1 && fy >= b.y && fy <= b.y + 1) return bossEvent(b);
  for (auto& b : m.buildings)
    if (fx == b.doorX() && fy == b.doorY()) return door(b);
}

void Game::openChest(int i) {
  std::string key = "coffre:" + std::to_string(mapId) + ":" + std::to_string(i);
  if (has(key)) return sc.say("Le coffre est vide.");
  flags.insert(key);
  const Chest& c = M().chests[i];
  if (c.item.empty()) {
    gold += c.qty;
    sc.say("Vous trouvez " + std::to_string(c.qty) + " pièces d'or !");
  } else {
    items[c.item] += c.qty;
    sc.say("Vous trouvez : " + item(c.item).name + (c.qty > 1 ? " ×" + std::to_string(c.qty) : "") + " !");
    if (c.item == "herbe") sc.say("Elle luit d'une douce lumière. Maëlle, à la chapelle, en avait besoin.");
  }
}

// ---------------------------------------------------------------------------
// Habitants et événements
// ---------------------------------------------------------------------------
void Game::talk(const Npc& n) {
  const std::string& s = n.script;
  if (s.empty()) {
    for (auto& l : n.lines) sc.say(l);
    return;
  }
  if (s == "ancien") {
    if (!has("boss1")) {
      sc.say("Ancien : Cette brume n'est pas naturelle. Elle vient du gardien Sylvarque, qui bloque le col à l'est.");
      sc.say("Ancien : Il est accompagné de Brumelins. Renforcez votre équipe avant de l'affronter.");
      sc.say("Ancien : Et passez voir Maëlle, à la chapelle. Sa magie de Lumière vous serait précieuse.");
    } else {
      sc.say("Ancien : La brume s'est levée ! Le col mène aux Monts Cendrelune, où gronde un volcan.");
      sc.say("Ancien : Là-bas, la ville de Forgeroc pourra vous accueillir.");
    }
  } else if (s == "maelle") {
    if (items["herbe"] > 0) {
      items["herbe"] = 0;
      sc.say("Maëlle : L'herbe lunaire ! Merci, je vais pouvoir terminer mon remède.");
      sc.say("Maëlle : Laissez-moi vous accompagner. Ma magie de Lumière vous protégera.");
      sc.call([this] { recruit("maelle", 6); });
      sc.say("Maëlle connaît Soin et Lumière. En combat, choisissez « Magie ».");
    } else {
      sc.say("Maëlle : Bonjour, je suis Maëlle, la mage de la chapelle.");
      sc.say("Maëlle : J'aimerais combattre la brume avec vous, mais je dois d'abord finir un remède pour le village.");
      sc.say("Maëlle : Il me manque une herbe lunaire. Elle pousse tout au fond du Bois Murmurant, au sud-ouest.");
    }
  } else if (s == "pecheur") {
    if (!has("pecheur")) {
      flags.insert("pecheur");
      items["plume"] += 2;
      sc.say("Pêcheur : Rien ne mord avec cette brume… Tenez, j'ai repêché ces plumes ce matin.");
      sc.say("Vous obtenez : Plume ravivante ×2 !");
    } else sc.say("Pêcheur : Le lac est calme. Un jour, la brume partira, et les poissons reviendront.");
  } else if (s == "garde_col") {
    if (has("boss1")) sc.say("Garde : Le col est libre ! Les Monts Cendrelune vous attendent de l'autre côté.");
    else {
      sc.say("Garde : Halte ! Le gardien Sylvarque bloque le col, accompagné de deux Brumelins.");
      sc.say("Garde : Il est de type Ombre et de niveau 12. La Lumière le blesse beaucoup.");
    }
  } else if (s == "brann") {
    sc.say("Brann : Alors c'est vous qui avez chassé la brume de la vallée ?");
    sc.say("Brann : Ignarok, le monstre du volcan, menace Forgeroc. Je veux bien vous aider…");
    sc.say("Brann : … mais seulement si vous me battez en duel !");
    sc.call([this] {
      ask("Affronter Brann en duel ?", [this] {
        auto b = makeFighter("brann", 14);
        b->mhp = b->mhp * 5 / 2;
        b->hp = b->mhp;
        b->boss = true;
        startBattle({b}, true, [this](BattleResult r) {
          if (r != BattleResult::Win) return;
          sc.say("Brann : Ha ! Belle bagarre. Vous avez gagné un compagnon.");
          sc.call([this] { recruit("brann", 14); });
        }, false, false);
      });
    });
  } else if (s == "garde_volcan") {
    if (has("boss2")) sc.say("Garde : Le volcan s'est calmé. Merci, héros !");
    else {
      sc.say("Garde : Le cratère est au bout de ce chemin. Ignarok y règne, entouré de Tisonnels.");
      sc.say("Garde : C'est une créature de Feu de niveau 23. L'Eau est sa faiblesse.");
    }
  } else if (s == "isra") {
    sc.say("Isra : Vous avez abattu le Golem ? J'étais coincée ici depuis des jours !");
    sc.say("Isra : Je suis Isra, mage noire. Feu, Givre, Foudre… je maîtrise les éléments.");
    sc.say("Isra : Si vous allez au volcan, comptez sur moi. Ignarok ne supportera pas mon Givre.");
    sc.call([this] { recruit("isra", 15); });
  }
}

void Game::door(const Building& b) {
  if (b.kind == "soin") {
    healAll();
    respawnMap = mapId, respawnX = b.doorX(), respawnY = b.doorY() + 1;
    sc.say("Guérisseuse : Reposez-vous… Voilà, toute votre équipe est en pleine forme !");
  } else if (b.kind == "boutique1") {
    shopMenu({"potion", "ether", "plume", "lanterne"});
  } else if (b.kind == "boutique2") {
    shopMenu({"potion", "superpotion", "ether", "plume", "lanterne", "lanterne_argent"});
  } else if (b.kind == "auberge") {
    sc.say("Aubergiste : Une nuit ici coûte 20 pièces d'or. Le repos rend tous les PV et PM.");
    sc.call([this] {
      ask("Passer la nuit (20 or) ?", [this] {
        if (gold < 20) return sc.say("Aubergiste : Vous n'avez pas assez d'or.");
        gold -= 20;
        healAll();
        respawnMap = mapId, respawnX = px, respawnY = py;
        sc.say("Vous dormez d'un sommeil profond… L'équipe est en pleine forme !");
      });
    });
  } else if (b.kind == "chapelle") {
    for (auto& f : team) f->mp = f->mmp;
    sc.say("Une douce lumière emplit la chapelle. Les PM de toute l'équipe sont restaurés.");
  } else if (b.kind == "forge") {
    sc.say(has("brann") ? "L'enclume est encore chaude. Brann y forgeait sa hache." : "La forge est fermée. Brann se tient juste à côté.");
  } else {
    sc.say("Personne ne répond.");
  }
}

void Game::bossEvent(const BossSpot& b) {
  std::string id = b.id, flag = b.flag;
  if (id == "sylvarque") sc.say("Une silhouette immense se dresse dans la brume. Les yeux de Sylvarque s'allument…");
  if (id == "golem") sc.say("Un amas de roches noires se redresse : le Golem de suie barre le passage !");
  if (id == "ignarok") sc.say("Le magma bouillonne. Ignarok, le cœur du volcan, ouvre les yeux.");
  sc.call([this, id, flag] {
    ask("Engager le combat ?", [this, id, flag] {
      std::vector<FighterP> foes;
      auto bossF = [](const std::string& sp, int lvl, int mult) {
        auto f = makeFighter(sp, lvl);
        f->mhp *= mult;
        f->hp = f->mhp;
        f->boss = true;
        return f;
      };
      if (id == "sylvarque") foes = {makeFighter("brumelin", 9), bossF("sylvarque", 12, 3), makeFighter("brumelin", 9)};
      if (id == "golem") foes = {bossF("golem", 16, 3)};
      if (id == "ignarok") foes = {makeFighter("tisonnel", 19), bossF("ignarok", 23, 4), makeFighter("tisonnel", 19)};
      startBattle(foes, true, [this, id, flag](BattleResult r) {
        if (r != BattleResult::Win) return;
        flags.insert(flag);
        if (id == "sylvarque") {
          sc.say("Sylvarque se dissout dans un souffle de vent. La brume se lève sur toute la vallée !");
          sc.say("Le col de la Brume est ouvert. Les Monts Cendrelune vous attendent à l'est.");
        } else if (id == "golem") {
          sc.say("Le Golem s'effondre en un tas de cailloux. Une voix vous appelle au fond de la grotte…");
        } else {
          ending();
        }
      }, false, false);
    });
  });
}

void Game::ending() {
  sc.say("Ignarok rugit une dernière fois, puis le volcan se tait.");
  sc.say("La lave refroidit. Sur Forgeroc, le ciel redevient bleu pour la première fois depuis des années.");
  sc.say("Lior et ses compagnons ont ramené la paix à Brumeval et aux Monts Cendrelune.");
  sc.call([this] { mode = Mode::Ending; });
}

void Game::ask(const std::string& q, std::function<void()> yes, std::function<void()> no) {
  askText_ = q;
  sc.halt();
  Menu m;
  m.x = 236, m.y = 128, m.w = 76, m.rows = 2;
  auto close = [this] {
    askText_.clear();
    menus.clear();
    sc.resume();
  };
  m.items.push_back({"Oui", "", "", true, [close, yes] {
                       close();
                       if (yes) yes();
                     }});
  m.items.push_back({"Non", "", "", true, [close, no] {
                       close();
                       if (no) no();
                     }});
  m.onCancel = m.items[1].act;
  menus.push(m);
}

// ---------------------------------------------------------------------------
// Menus
// ---------------------------------------------------------------------------
void Game::titleMenu() {
  mode = Mode::Title;
  menus.clear();
  Menu m;
  m.x = 110, m.y = 146, m.w = 100, m.rows = 3, m.cancelable = false;
  m.items.push_back({"Nouvelle partie", "", "", true, [this] { starterMenu(); }});
  bool can = saveExists();
  m.items.push_back({"Continuer", "", "", can, [this] {
                       if (loadGame()) {
                         menus.clear();
                         mode = Mode::Map;
                         showRegionBanner();
                       }
                     }});
  m.items.push_back({"Quitter", "", "", true, [this] { quit = true; }});
  if (can) m.sel = 1;
  menus.push(m);
}

void Game::starterMenu() {
  Menu m;
  m.title = "Premier compagnon";
  m.x = 90, m.y = 132, m.w = 140, m.rows = 3;
  for (std::string id : {"braisenard", "gouttelin", "ronceau"}) {
    const Species& s = species(id);
    m.items.push_back({s.name, typeName(s.type), "Type " + std::string(typeName(s.type)) + " · Limite : " + moveInfo(s.limit).name, true,
                       [this, id] { newGame(id); }});
  }
  menus.push(m);
}

void Game::newGame(const std::string& starter) {
  menus.clear();
  sc.clear();
  team = {makeFighter("lior", 5), makeFighter(starter, 5)};
  items = {{"potion", 4}, {"ether", 1}, {"plume", 1}, {"lanterne", 5}};
  gold = 100;
  flags.clear();
  respawnMap = 0, respawnX = 7, respawnY = 7;
  mapId = 0;
  changeMap(0, 12, 9, DOWN);
  showRegionBanner();
  mode = Mode::Map;
  sc.say("Ce matin encore, une brume épaisse a recouvert la vallée de Brumeval.");
  sc.say("Elle rend les créatures sauvages. Lior, apprenti gardien, part avec " + species(starter).name + " en trouver la source.");
  sc.say("Entrée : parler et valider. Échap : menu. Flèches ou ZQSD : se déplacer.");
  sc.say("Les hautes herbes cachent des créatures. Capturez-les avec des lanternes pour agrandir l'équipe !");
}

void Game::pauseMenu() {
  panelMode_ = 1;
  Menu m;
  m.x = 8, m.y = 8, m.w = 100, m.rows = 6;
  m.items.push_back({"Équipe", "", "Ordre de combat et fiches.", true, [this] { teamMenu(); }});
  m.items.push_back({"Objets", "", "Utiliser un objet.", true, [this] { itemMenu(); }});
  m.items.push_back({"Magie", "", "Lancer un sort de soin.", true, [this] { magicMenu(); }});
  m.items.push_back({"Sauvegarder", "", "", true, [this] { notice(saveGame() ? "Partie sauvegardée." : "Impossible de sauvegarder."); }});
  m.items.push_back({"Écran titre", "", "", true, [this] {
                       ask("Revenir à l'écran titre ? (pensez à sauvegarder)", [this] { titleMenu(); });
                     }});
  m.items.push_back({"Reprendre", "", "", true, [this] { menus.clear(); }});
  menus.push(m);
}

void Game::teamMenu() { teamMenuAt(0); }
void Game::teamMenuAt(int sel) {
  panelMode_ = 2;
  Menu m;
  m.title = swapFrom_ >= 0 ? "Échanger avec…" : "Équipe";
  m.x = 8, m.y = 8, m.w = 132, m.rows = 9;
  m.sel = sel;
  auto fv = front();
  for (size_t i = 0; i < team.size(); i++) {
    auto& f = team[i];
    bool fr = std::find(fv.begin(), fv.end(), f) != fv.end();
    m.items.push_back({(int)i == swapFrom_ ? "» " + f->name() : f->name(), fr ? "Front" : f->alive() ? "Rés." : "K.O.",
                       "Entrée sur deux membres pour les échanger. Les 3 premiers valides combattent.", true,
                       [this, i] {
                         if (swapFrom_ < 0) swapFrom_ = (int)i;
                         else {
                           std::swap(team[swapFrom_], team[i]);
                           swapFrom_ = -1;
                         }
                         menus.pop();
                         teamMenuAt((int)i);
                       },
                       [this, i] { teamPanelSel_ = (int)i; }});
  }
  m.onCancel = [this] {
    swapFrom_ = -1;
    panelMode_ = 1;
    menus.pop();
  };
  menus.push(m);
}

void Game::pickMember(const std::string& title, std::function<bool(const Fighter&)> ok, std::function<void(Fighter&)> use) {
  Menu m;
  m.title = title;
  m.x = 116, m.y = 8, m.w = 196, m.rows = 8;
  for (auto& f : team) {
    FighterP fp = f;
    m.items.push_back({f->name(), std::to_string(f->hp) + "/" + std::to_string(f->mhp) + "  " + std::to_string(f->mp) + "PM", "",
                       ok(*f), [this, fp, use] {
                         menus.pop();
                         use(*fp);
                       }});
  }
  menus.push(m);
}

void Game::itemMenu() { itemMenuAt(0); }
void Game::itemMenuAt(int sel) {
  Menu m;
  m.title = "Objets  ·  " + std::to_string(gold) + " or";
  m.x = 8, m.y = 8, m.w = 170, m.rows = 7;
  m.sel = sel;
  int idx = 0;
  for (auto& d : allItems()) {
    int n = items.count(d.id) ? items[d.id] : 0;
    if (n <= 0) continue;
    std::string id = d.id;
    int myIdx = idx++;
    m.items.push_back({d.name, "×" + std::to_string(n), d.desc + (d.field ? "" : d.key ? " (objet important)" : " (en combat)"), d.field,
                       [this, id, myIdx] {
                         pickMember("Sur qui ?", [id](const Fighter& f) {
                           if (id == "plume") return !f.alive();
                           if (id == "ether") return f.alive() && f.mp < f.mmp;
                           return f.alive() && f.hp < f.mhp;
                         }, [this, id, myIdx](Fighter& f) {
                           items[id]--;
                           if (id == "potion") f.hp = std::min(f.mhp, f.hp + 40);
                           if (id == "superpotion") f.hp = std::min(f.mhp, f.hp + 120);
                           if (id == "ether") f.mp = std::min(f.mmp, f.mp + 25);
                           if (id == "plume") f.hp = std::max(1, f.mhp / 2);
                           notice(item(id).name + " utilisé sur " + f.name() + ".");
                           menus.pop();
                           itemMenuAt(myIdx);
                         });
                       }});
  }
  if (m.items.empty()) m.items.push_back({"(sac vide)", "", "", false, nullptr});
  menus.push(m);
}

void Game::magicMenu() {
  Menu m;
  m.title = "Qui lance le sort ?";
  m.x = 8, m.y = 8, m.w = 170, m.rows = 8;
  for (auto& f : team) {
    FighterP caster = f;
    std::vector<std::string> heal;
    for (auto& id : f->spells())
      if (moveInfo(id).kind == Kind::Heal || moveInfo(id).kind == Kind::Revive) heal.push_back(id);
    m.items.push_back({f->name(), std::to_string(f->mp) + " PM", heal.empty() ? "Aucun sort utilisable hors combat." : "", f->alive() && !heal.empty(),
                       [this, caster, heal] {
                         Menu s;
                         s.title = "Sorts de " + caster->name();
                         s.x = 24, s.y = 40, s.w = 170, s.rows = 5;
                         for (auto& id : heal) {
                           const Move& mv = moveInfo(id);
                           s.items.push_back({mv.name, std::to_string(mv.cost) + " PM", mv.desc, caster->mp >= mv.cost, [this, caster, id] {
                                                const Move& mv = moveInfo(id);
                                                auto cast = [this, caster, id](Fighter* target) {
                                                  const Move& mv = moveInfo(id);
                                                  caster->mp -= mv.cost;
                                                  auto healOne = [&](Fighter& t) {
                                                    if (mv.kind == Kind::Revive) {
                                                      if (!t.alive()) t.hp = std::max(1, t.mhp * mv.power / 100);
                                                    } else if (t.alive())
                                                      t.hp = std::min(t.mhp, t.hp + mv.power * caster->mag / 20 + 2);
                                                  };
                                                  if (target) healOne(*target);
                                                  else
                                                    for (auto& t : team) healOne(*t);
                                                  notice(caster->name() + " lance " + mv.name + ".");
                                                  menus.clear();
                                                  panelMode_ = 0;
                                                };
                                                if (mv.target == Target::AllAllies) cast(nullptr);
                                                else
                                                  pickMember("Sur qui ?", [&mv](const Fighter& f) {
                                                    return mv.kind == Kind::Revive ? !f.alive() : f.alive() && f.hp < f.mhp;
                                                  }, [cast](Fighter& f) { cast(&f); });
                                              }});
                         }
                         menus.push(s);
                       }});
  }
  menus.push(m);
}

void Game::shopMenu(const std::vector<std::string>& stock) { shopMenuAt(stock, 0); }
void Game::shopMenuAt(const std::vector<std::string>& stock, int sel) {
  Menu m;
  m.title = "Boutique  ·  " + std::to_string(gold) + " or";
  m.x = 60, m.y = 20, m.w = 200, m.rows = 7;
  m.sel = sel;
  for (size_t i = 0; i < stock.size(); i++) {
    const ItemDef& d = item(stock[i]);
    std::string id = d.id;
    int have = items.count(id) ? items[id] : 0;
    m.items.push_back({d.name, std::to_string(d.price) + " or", d.desc + " (vous en avez " + std::to_string(have) + ")", true,
                       [this, stock, id, i] {
                         const ItemDef& d = item(id);
                         if (gold < d.price) {
                           notice("Pas assez d'or.");
                           return;
                         }
                         gold -= d.price;
                         items[id]++;
                         notice("Acheté : " + d.name + ".");
                         menus.pop();
                         shopMenuAt(stock, (int)i);
                       }});
  }
  m.items.push_back({"Quitter", "", "", true, [this] { menus.clear(); }});
  menus.push(m);
}

// ---------------------------------------------------------------------------
// Sauvegarde (fichier texte dans le dossier de l'utilisateur)
// ---------------------------------------------------------------------------
std::string Game::savePath() const {
  char* p = SDL_GetPrefPath("Brumeval", "Brumeval");
  std::string s = p ? p : "";
  SDL_free(p);
  return s + "sauvegarde.txt";
}
bool Game::saveExists() const {
  std::ifstream f(savePath());
  return f.good();
}
bool Game::saveGame() {
  std::ofstream f(savePath());
  if (!f) return false;
  f << "BRUMEVAL 1\n";
  f << "carte " << mapId << ' ' << px << ' ' << py << ' ' << dir << '\n';
  f << "reveil " << respawnMap << ' ' << respawnX << ' ' << respawnY << '\n';
  f << "or " << gold << '\n';
  for (auto& fl : flags) f << "drapeau " << fl << '\n';
  for (auto& [id, n] : items)
    if (n > 0) f << "objet " << id << ' ' << n << '\n';
  for (auto& m : team) f << "membre " << m->sp << ' ' << m->lvl << ' ' << m->xp << ' ' << m->hp << ' ' << m->mp << ' ' << m->lim << '\n';
  return true;
}
bool Game::loadGame() {
  std::ifstream f(savePath());
  std::string head;
  if (!f || !std::getline(f, head) || head.rfind("BRUMEVAL", 0) != 0) return false;
  team.clear();
  items.clear();
  flags.clear();
  std::string line;
  while (std::getline(f, line)) {
    std::istringstream s(line);
    std::string k;
    s >> k;
    if (k == "carte") s >> mapId >> px >> py >> dir;
    else if (k == "reveil") s >> respawnMap >> respawnX >> respawnY;
    else if (k == "or") s >> gold;
    else if (k == "drapeau") {
      std::string v;
      s >> v;
      flags.insert(v);
    } else if (k == "objet") {
      std::string id;
      int n;
      s >> id >> n;
      items[id] = n;
    } else if (k == "membre") {
      std::string sp;
      int lvl, xp, hp, mp;
      float lim;
      s >> sp >> lvl >> xp >> hp >> mp >> lim;
      auto m = makeFighter(sp, lvl);
      m->xp = xp, m->hp = std::min(hp, m->mhp), m->mp = std::min(mp, m->mmp), m->lim = lim;
      team.push_back(m);
    }
  }
  moving = false;
  steps = 0;
  sc.clear();
  return !team.empty();
}

// ---------------------------------------------------------------------------
// Dessin
// ---------------------------------------------------------------------------
void Game::draw() {
  g.clear(rgb(0x070a1c));
  switch (mode) {
    case Mode::Title: drawTitle(); break;
    case Mode::Map: drawMap(); break;
    case Mode::Battle: battle_->draw(); break;
    case Mode::Ending: drawEnding(); break;
  }
}

void Game::drawTitle() {
  g.gradV(0, 0, SCREEN_W, SCREEN_H, rgb(0x141a3c), rgb(0x3c4278));
  g.poly({{0, 150}, {50, 96}, {100, 136}, {160, 80}, {220, 128}, {270, 100}, {320, 126}, {320, 240}, {0, 240}}, rgb(0x262a52));
  g.rect(0, 160, SCREEN_W, 80, rgb(0x1d2148));
  for (int i = 0; i < 6; i++) g.ellipse(std::fmod(i * 70 + time * 18, 420.f) - 50, 112 + i * 10, 90, 7, rgb(0xe6ebff, 26));
  g.textBig(160, 26, "BRUMEVAL", GOLD, 4, 1);
  g.text(160, 76, "Les gardiens de la vallée", rgb(0xc9cbe0), 1);
  drawHuman(g, look(0), 144, 92, 2, DOWN, 0, false);
  drawCreature(g, "braisenard", 92, 124, 1, true, time);
  drawCreature(g, "gouttelin", 232, 124, 1, false, time + .5f);
  if (menus.active()) {
    menus.draw(g, time);
    std::string h = menus.help();
    if (!h.empty()) {
      g.window(20, 214, 280, 20);
      g.text(160, 218, h, WHITE, 1);
    }
  }
  g.text(316, 230, "v1.0", rgb(0x8a92b8), 2, false);
}

void Game::drawMap() {
  const MapDef& m = M();
  float k = moving ? moveT : 1;
  float ppx = (fromX + (px - fromX) * k) * 16, ppy = (fromY + (py - fromY) * k) * 16;
  if (!moving) ppx = px * 16.f, ppy = py * 16.f;
  int camX = (int)std::clamp(ppx - SCREEN_W / 2 + 8, 0.f, float(m.w() * 16 - SCREEN_W));
  int camY = (int)std::clamp(ppy - SCREEN_H / 2 + 8, 0.f, float(m.h() * 16 - SCREEN_H));
  int tx0 = camX / 16, ty0 = camY / 16;
  for (int y = ty0; y <= ty0 + SCREEN_H / 16 && y < m.h(); y++)
    for (int x = tx0; x <= tx0 + SCREEN_W / 16 && x < m.w(); x++) drawTile(g, m, x, y, x * 16 - camX, y * 16 - camY, time);
  for (auto& b : m.buildings) drawBuilding(g, b, b.x * 16 - camX, b.y * 16 - camY);
  for (size_t i = 0; i < m.chests.size(); i++)
    drawChest(g, m.chests[i].x * 16 - camX, m.chests[i].y * 16 - camY, has("coffre:" + std::to_string(mapId) + ":" + std::to_string(i)));
  for (auto& s : m.signs) drawSign(g, s.x * 16 - camX, s.y * 16 - camY);
  // Personnages triés de haut en bas
  struct Actor {
    float y;
    std::function<void()> draw;
  };
  std::vector<Actor> actors;
  for (auto& n : m.npcs)
    if (npcVisible(n)) actors.push_back({float(n.y * 16), [&, n] { drawHuman(g, look(n.look), n.x * 16 - camX, n.y * 16 - camY - 2, 1, n.dir, 0, false); }});
  for (auto& b : m.bosses)
    if (bossAlive(b))
      actors.push_back({float(b.y * 16 + 16), [&, b] { drawCreature(g, b.id, (b.x + 1) * 16 - camX, (b.y + 1) * 16 - camY - 2, .6f, false, time); }});
  int step = moving ? (moveT < .5f ? 1 : 2) : 0;
  actors.push_back({ppy, [&] { drawHuman(g, look(0), ppx - camX, ppy - camY - 2, 1, dir, step, false); }});
  std::sort(actors.begin(), actors.end(), [](const Actor& a, const Actor& b) { return a.y < b.y; });
  for (auto& a : actors) a.draw();
  // Ambiance
  if (m.theme == Theme::Vallee && !has("boss1"))
    for (int i = 0; i < 5; i++) g.ellipse(std::fmod(i * 97 + time * 22, 460.f) - 70, 30 + i * 46, 90, 10, rgb(0xe6ebff, 22));
  if (m.theme == Theme::Cendres && !has("boss2"))
    for (int i = 0; i < 30; i++) {
      float x = std::fmod(i * 53.f + time * (8 + i % 5), 330.f) - 5, y = std::fmod(i * 31.f + time * (14 + i % 7), 250.f) - 5;
      g.rect(x, y, 1, 1, rgb(0xcfc6c0, 170));
    }
  if (m.theme == Theme::Grotte) {
    float cx = ppx - camX + 8, cy = ppy - camY + 8;
    for (int ring = 0; ring < 2; ring++) {
      float R = ring == 0 ? 92 : 68;
      Color c = rgb(0x05030a, ring == 0 ? 200 : 90);
      for (int y = 0; y < SCREEN_H; y++) {
        float dy = y + .5f - cy;
        float span = R * R - dy * dy;
        if (span <= 0) {
          if (ring == 0) g.rect(0, y, SCREEN_W, 1, c);
          continue;
        }
        float w = std::sqrt(span);
        float R0 = ring == 0 ? 0 : 92;
        float w0 = R0 * R0 - dy * dy > 0 ? std::sqrt(R0 * R0 - dy * dy) : 0;
        if (ring == 0) {
          g.rect(0, y, cx - w, 1, c);
          g.rect(cx + w, y, SCREEN_W - cx - w, 1, c);
        } else {
          g.rect(cx - w0, y, w0 - w, 1, c);
          g.rect(cx + w, y, w0 - w, 1, c);
        }
      }
    }
  }
  // Bandeau du nom de la région
  if (banner > 0) {
    float a = std::min(1.f, banner / .5f);
    g.alpha = a;
    int w = Gfx::textW(bannerText) + 24;
    g.window(160 - w / 2, 10, w, 20);
    g.text(160, 14, bannerText, GOLD, 1);
    g.alpha = 1;
  }
  // Fenêtres
  if (panelMode_ && menus.active()) drawTeamPanel(panelMode_ == 1 ? 114 : 146, 8, panelMode_ == 2 ? teamPanelSel_ : -1);
  if (menus.active()) menus.draw(g, time);
  drawDialogue();
  if (menus.active() && askText_.empty() && !menus.help().empty()) {
    auto lines = Gfx::wrap(menus.help(), 300);
    int h = 10 + 11 * (int)lines.size();
    g.window(4, 236 - h, 312, h);
    for (size_t i = 0; i < lines.size(); i++) g.text(10, 236 - h + 4 + i * 11, lines[i], MUTED);
  }
  if (noticeT_ > 0) {
    int w = Gfx::textW(notice_) + 20;
    g.window(160 - w / 2, 190, w, 20);
    g.text(160, 194, notice_, WHITE, 1);
  }
}

void Game::drawDialogue() {
  std::string text;
  bool done = true;
  if (!askText_.empty()) text = askText_;
  else if (sc.showingMessage()) {
    text = utf8Prefix(sc.text(), sc.visibleChars());
    done = sc.messageDone();
  } else return;
  int y = askText_.empty() ? 170 : 84;
  g.window(4, y, 312, 66);
  auto full = Gfx::wrap(askText_.empty() ? sc.text() : askText_, 296);
  // Découpe le texte visible selon les mêmes lignes que le texte complet
  int remaining = 0;
  for (unsigned char c : text)
    if ((c & 0xC0) != 0x80) remaining++;
  for (size_t i = 0; i < full.size() && i < 4; i++) {
    int n = 0;
    for (unsigned char c : full[i])
      if ((c & 0xC0) != 0x80) n++;
    g.text(12, y + 8 + i * 13, utf8Prefix(full[i], std::min(n, remaining)), WHITE);
    remaining -= n + 1;
    if (remaining <= 0) break;
  }
  if (done && askText_.empty() && int(time * 3) % 2 == 0) g.text(302, y + 52, "▼", GOLD);
}

void Game::drawTeamPanel(int x, int y, int sel) {
  if (sel < 0) {
    int h = 22 + (int)team.size() * 22 + 14;
    g.window(x, y, 320 - x - 8, h);
    g.text(x + 8, y + 5, "Équipe", GOLD);
    g.text(320 - 16, y + 5, std::to_string(gold) + " or", WHITE, 2);
    auto fr = front();
    for (size_t i = 0; i < team.size(); i++) {
      auto& f = team[i];
      float ry = y + 20 + i * 22;
      bool isFront = std::find(fr.begin(), fr.end(), f) != fr.end();
      g.text(x + 8, ry, f->name(), f->alive() ? WHITE : rgb(0xff7b6b));
      g.text(x + 86, ry, "N." + std::to_string(f->lvl), MUTED);
      g.text(320 - 16, ry, isFront ? "front" : "réserve", isFront ? GOLD : MUTED, 2);
      g.text(x + 8, ry + 10, "PV " + std::to_string(f->hp) + "/" + std::to_string(f->mhp), WHITE);
      g.text(x + 86, ry + 10, "PM " + std::to_string(f->mp) + "/" + std::to_string(f->mmp), WHITE);
    }
    return;
  }
  if (sel >= (int)team.size()) return;
  const Fighter& f = *team[sel];
  int w = 320 - x - 8;
  g.window(x, y, w, 200);
  g.text(x + 8, y + 6, f.name(), GOLD);
  g.text(x + w - 8, y + 6, "Niveau " + std::to_string(f.lvl), WHITE, 2);
  std::string role = f.S().human ? f.S().role : std::string("Créature de type ") + typeName(f.S().type);
  g.text(x + 8, y + 19, role.size() > 40 ? typeName(f.S().type) : role, MUTED);
  if (f.S().human) drawHuman(g, look(f.S().look), x + w - 40, y + 24, 2, DOWN, 0, false);
  else drawCreature(g, f.sp, x + w - 26, y + 48, .9f, false, time);
  int ly = y + 34;
  auto row = [&](const std::string& a, const std::string& b) {
    g.text(x + 8, ly, a, MUTED);
    g.text(x + 54, ly, b, WHITE);
    ly += 11;
  };
  row("Type", typeName(f.S().type));
  row("PV", std::to_string(f.hp) + "/" + std::to_string(f.mhp));
  row("PM", std::to_string(f.mp) + "/" + std::to_string(f.mmp));
  row("Attaque", std::to_string(f.atk));
  row("Défense", std::to_string(f.def));
  row("Magie", std::to_string(f.mag));
  row("Vitesse", std::to_string(f.spd));
  row("Exp.", std::to_string(f.xp) + "/" + std::to_string(f.need()));
  ly += 3;
  std::string t;
  for (auto& id : f.techs()) t += (t.empty() ? "" : ", ") + moveInfo(id).name;
  for (auto& l : Gfx::wrap("Techniques : " + t, w - 16)) g.text(x + 8, ly, l, WHITE), ly += 11;
  std::string s;
  for (auto& id : f.spells()) s += (s.empty() ? "" : ", ") + moveInfo(id).name;
  if (!s.empty())
    for (auto& l : Gfx::wrap("Magie : " + s, w - 16)) g.text(x + 8, ly, l, rgb(0x9fd8ff)), ly += 11;
  g.text(x + 8, ly, "Limite : " + moveInfo(f.S().limit).name, rgb(0xff9ad0));
}

void Game::drawEnding() {
  g.gradV(0, 0, SCREEN_W, SCREEN_H, rgb(0x0e1550), rgb(0x3a6fb0));
  g.ellipse(250, 60, 22, 22, rgb(0xfff4c0));
  g.poly({{0, 200}, {80, 150}, {150, 190}, {230, 140}, {320, 190}, {320, 240}, {0, 240}}, rgb(0x2e6b3a));
  g.textBig(160, 30, "FIN", GOLD, 3, 1);
  const char* lines[] = {"Brumeval et les Monts Cendrelune", "sont libérés de la brume et du feu.", "",
                         "Un jeu créé avec Claude", "C++ et SDL2", "", "Appuyez sur Entrée pour continuer"};
  for (int i = 0; i < 7; i++) g.text(160, 80 + i * 13, lines[i], WHITE, 1);
  float x = 60;
  for (auto& f : front()) {
    drawFighterSprite(g, *f, x, 205, 1, true, time);
    x += 100;
  }
}
