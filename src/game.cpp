#include "game.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

#include "arena.hpp"
#include "battle.hpp"
#include "expedition.hpp"
#include "mapedit.hpp"
#include "online.hpp"
#include "settings.hpp"
#include "storyedit.hpp"
#include "sprites.hpp"
#include "tactics.hpp"
#include "version.hpp"

static const Color WHITE = rgb(0xffffff), GOLD = rgb(0xffd34d), MUTED = rgb(0xaab3d8);

Game::Game(SDL_Renderer* r) : g(r) { titleMenu(); }
Game::~Game() = default;

// ---------------------------------------------------------------------------
// Clavier : flèches ou ZQSD (WASD en QWERTY), Entrée/Espace pour valider,
// Échap pour annuler et ouvrir le menu.
// ---------------------------------------------------------------------------
void Game::onMouse(int x, int y, int button, bool down) {
  if (in.mx != x || in.my != y) in.moved = true;
  in.mx = x, in.my = y;
  in.mouseOn = true;
  if (button < 0 || button > 2) return;
  in.mdown[button] = down;
  if (down) in.mclick[button] = true;
}
void Game::onWheel(int dy) {
  in.wheel += dy > 0 ? 1 : dy < 0 ? -1 : 0;
  in.mouseOn = true;
}

void Game::onKey(SDL_Scancode k, bool down, bool repeat) {
  in.ctrl = (SDL_GetModState() & KMOD_CTRL) != 0;
  in.shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
  SDL_Keycode kc = SDL_GetKeyFromScancode(k);  // touche selon la disposition du clavier (AZERTY…)
  if (!textOn_ && down && in.ctrl) {
    if (kc == SDLK_z) in.undo = true;
    if (kc == SDLK_y) in.redo = true;
    if (kc == SDLK_s) in.saveKey = true;
    if (kc == SDLK_z || kc == SDLK_y || kc == SDLK_s) return;
  }
  if (textOn_) {  // saisie de texte : les touches servent à écrire
    if (!down) return;
    if (k == SDL_SCANCODE_BACKSPACE) {
      while (!textValue_.empty() && (textValue_.back() & 0xC0) == 0x80) textValue_.pop_back();
      if (!textValue_.empty()) textValue_.pop_back();
    } else if ((k == SDL_SCANCODE_RETURN || k == SDL_SCANCODE_KP_ENTER) && !repeat) {
      textOn_ = false;
      SDL_StopTextInput();
      auto done = textDone_;
      if (done) done(textValue_);
    } else if (k == SDL_SCANCODE_ESCAPE && !repeat) {
      textOn_ = false;
      SDL_StopTextInput();
    }
    return;
  }
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
  if (down && k == SDL_SCANCODE_PAGEUP) in.prev = true;
  if (down && k == SDL_SCANCODE_PAGEDOWN) in.next = true;
  if (down && k == SDL_SCANCODE_DELETE) in.del = true;
  if (!down || repeat) return;
  if (k == SDL_SCANCODE_RETURN || k == SDL_SCANCODE_KP_ENTER || k == SDL_SCANCODE_SPACE) in.confirm = true;
  if (k == SDL_SCANCODE_ESCAPE || k == SDL_SCANCODE_BACKSPACE) in.cancel = in.menu = true;
  if (k == SDL_SCANCODE_TAB) in.menu = in.tab = true;
}

// ---------------------------------------------------------------------------
// Saisie de texte
// ---------------------------------------------------------------------------
static int utf8Len(const std::string& s) {
  int n = 0;
  for (unsigned char c : s)
    if ((c & 0xC0) != 0x80) n++;
  return n;
}
void Game::editText(const std::string& title, const std::string& initial, int maxChars, std::function<void(const std::string&)> done) {
  textOn_ = true;
  textTitle_ = title;
  textValue_ = initial;
  textMax_ = maxChars;
  textDone_ = std::move(done);
  in.endFrame();
  SDL_StartTextInput();
}
void Game::onText(const char* utf8) {
  if (!textOn_ || !utf8) return;
  std::string add = utf8;
  if (add == "\t" || add == "\r" || add == "\n") return;
  textValue_ += add;
  if (utf8Len(textValue_) > textMax_) textValue_ = utf8Prefix(textValue_, textMax_);
}
void Game::drawTextEdit() {
  auto title = Gfx::wrap(textTitle_, 284);
  int lines = std::max(1, (int)Gfx::wrap(textValue_ + "_", 284).size());
  int top = 8 + 11 * (int)title.size();  // place du titre
  int h = top + 22 + lines * 11;
  int y = 120 - h / 2;
  g.rect(g.left(), 0, g.fullW, SCREEN_H, rgb(0x000010, 120));
  g.newLayer();  // fenêtre modale : le reste est assombri derrière
  g.window(10, y, 300, h);
  for (size_t i = 0; i < title.size(); i++) g.text(18, y + 5 + i * 11, title[i], rgb(0xffd34d));
  auto wl = Gfx::wrap(textValue_, 284);
  if (wl.empty()) wl.push_back("");
  for (size_t i = 0; i < wl.size(); i++) g.text(18, y + top + i * 11, wl[i], WHITE);
  if (int(time * 3) % 2 == 0) g.rect(18 + Gfx::textW(wl.back()), y + top + (wl.size() - 1) * 11 + 1, 5, 9, rgb(0xffd34d));
  g.text(160, y + h - 13, "Entrée : valider · Échap : annuler · " + std::to_string(utf8Len(textValue_)) + "/" + std::to_string(textMax_),
         MUTED, 1);
}

// ---------------------------------------------------------------------------
// Boucle principale
// ---------------------------------------------------------------------------
void Game::update(float dt) {
  time += dt;
  if (online_) online_->update(dt);  // réseau du multijoueur, dans tous les écrans
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
    case Mode::Arena:
      if (arena_) arena_->update(dt);
      break;
    case Mode::Settings:
      if (settings_) settings_->update(dt);
      break;
    case Mode::Editor:
      if (editor_) editor_->update(dt);
      break;
    case Mode::Story:
      if (story_) story_->update(dt);
      break;
    case Mode::Battle: {
      battle_->update(dt);
      if (battle_->finished()) {
        BattleResult r = battle_->result();
        if (duelBattle_ && online_) {  // duel en ligne : retour au salon, sans conséquence sur la partie
          battle_.reset();
          duelBattle_ = false;
          menus.clear();
          mode = Mode::Title;
          online_->onBattleEnd(r);
          break;
        }
        if (arenaBattle_ && arena_) {  // retour à l'Arène, sans les conséquences d'une vraie défaite
          auto log = battle_->log;
          battle_.reset();
          arenaBattle_ = false;
          mode = Mode::Arena;
          arena_->onBattleEnd(r, log);
          break;
        }
        battle_.reset();
        mode = Mode::Map;
        menus.clear();
        steps = 0;
        auto after = afterBattle_;
        afterBattle_ = nullptr;
        if (r == BattleResult::Lose) {
          sc.clear();  // la défaite interrompt l'événement en cours
          defeat();
        }
        if (after) sc.runNow([&] { after(r); });
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
    if (f->alive() && (int)v.size() < rules().frontSize) v.push_back(f);
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

void Game::startBattle(BattleSetup setup, std::function<void(BattleResult)> after) {
  afterBattle_ = std::move(after);
  arenaBattle_ = mode == Mode::Arena;
  Theme th = setup.theme >= 0 ? Theme(setup.theme) : M().theme;
  battle_ = std::make_unique<Battle>(*this, std::move(setup), th);
  mode = Mode::Battle;
  menus.clear();
}
void Game::startBattle(std::vector<FighterP> foes, bool boss, std::function<void(BattleResult)> after, bool canFlee, bool canCapture) {
  BattleSetup s;
  s.foes = std::move(foes);
  s.boss = boss, s.canFlee = canFlee, s.canCapture = canCapture;
  startBattle(std::move(s), std::move(after));
}

bool Game::inExpedition() const { return expedition_ && expedition_->active(); }

void Game::defeat() {
  if (inExpedition()) return expedition_->onDefeat();
  healAll();
  changeMap(respawnMap, respawnX, respawnY, DOWN);
  sc.say("Vous reprenez connaissance chez la guérisseuse. Toute l'équipe est soignée.");
}

// ---------------------------------------------------------------------------
// Carte
// ---------------------------------------------------------------------------
bool Game::npcVisible(const Npc& n) const { return n.hideIf.empty() || !has(n.hideIf); }
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
  for (auto& w : m.warps)  // passage fermé : on ne peut pas y marcher
    if (w.x == x && w.y == y && !w.condition.is_null() && !checkCond(w.condition)) return true;
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
      if (nx == b.doorX() && ny == b.doorY()) return runEvent(b.event);
    for (auto& b : M().bosses)
      if (bossAlive(b) && nx >= b.x && nx <= b.x + 1 && ny >= b.y && ny <= b.y + 1) return runEvent(b.event);
    for (auto& w : M().warps)
      if (w.x == nx && w.y == ny && !w.condition.is_null() && !checkCond(w.condition))
        return sc.say(w.message.empty() ? "Le passage est fermé." : fillText(w.message));
    return;
  }
  fromX = px, fromY = py;
  px = nx, py = ny;
  moving = true;
  moveT = 0;
}

void Game::arrive() {
  steps++;
  if (inExpedition() && expedition_->atExit(px, py)) return expedition_->nextRegion();
  for (auto& w : M().warps)
    if (w.x == px && w.y == py) {
      int m = mapIndex(w.map);
      if (m >= 0) return changeMap(m, w.tx, w.ty, w.dir);
    }
  for (auto& t : M().triggers)
    if (px >= t.x && py >= t.y && px < t.x + t.w && py < t.y + t.h && (t.until.empty() || !has(t.until))) {
      steps = 0;
      return runEvent(t.event);
    }
  if (checkSight()) return;
  char c = M().rows[py][px];
  double rate = M().encounterRate > 0 ? M().encounterRate : rules().encounterRate;
  if (tileEncounter(c) && steps > rules().minSteps && frand() < rate) encounter();
}

// Dresseurs : un habitant avec « vue » repère le joueur dans sa ligne de regard
bool Game::checkSight() {
  const MapDef& m = M();
  for (size_t i = 0; i < m.npcs.size(); i++) {
    const Npc& n = m.npcs[i];
    if (n.sight <= 0 || n.event.empty() || !npcVisible(n) || (!n.sightUntil.empty() && has(n.sightUntil))) continue;
    int dx = n.dir == LEFT ? -1 : n.dir == RIGHT ? 1 : 0, dy = n.dir == UP ? -1 : n.dir == DOWN ? 1 : 0;
    for (int k = 1; k <= n.sight; k++) {
      int x = n.x + dx * k, y = n.y + dy * k;
      if (x == px && y == py) {
        exclaimNpc_ = (int)i;
        exclaimT_ = time;
        dir = n.dir == UP ? DOWN : n.dir == DOWN ? UP : n.dir == LEFT ? RIGHT : LEFT;  // le joueur se tourne vers lui
        steps = 0;
        std::string ev = n.event;
        sc.wait(.7f);
        sc.call([this, ev] { runEvent(ev); });
        return true;
      }
      if (blocked(x, y)) break;
    }
  }
  return false;
}

void Game::encounter() {
  for (auto& z : M().zones) {
    if (px < z.x || py < z.y || px >= z.x + z.w || py >= z.y + z.h) continue;
    int n = 1 + (frand() < rules().second) + (frand() < rules().third);
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
  exclaimNpc_ = -1;
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
    if (bossAlive(b) && fx >= b.x && fx <= b.x + 1 && fy >= b.y && fy <= b.y + 1) return runEvent(b.event);
  for (auto& b : m.buildings)
    if (fx == b.doorX() && fy == b.doorY()) return runEvent(b.event);
}

// Drapeau d'un coffre ouvert : carte et position (reste valable si on ajoute des coffres)
std::string Game::chestFlag(int i) const {
  const Chest& c = M().chests[i];
  return "coffre:" + M().id + ":" + std::to_string(c.x) + ":" + std::to_string(c.y);
}

void Game::openChest(int i) {
  std::string key = chestFlag(i);
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
  if (!n.event.empty()) return runEvent(n.event);
  for (auto& l : n.lines) sc.say(fillText(l));
}

void Game::ask(const std::string& q, std::function<void()> yes, std::function<void()> no) {
  askText_ = q;
  sc.halt();
  Menu m;
  m.x = 236, m.y = 152, m.w = 76, m.rows = 2;  // sous la fenêtre de la question (y 84 à 150)
  auto close = [this] {
    askText_.clear();
    menus.clear();
    sc.resume();
  };
  // La réponse passe avant les étapes déjà en attente dans la file
  m.items.push_back({"Oui", "", "", true, [this, close, yes] {
                       close();
                       sc.runNow(yes);
                     }});
  m.items.push_back({"Non", "", "", true, [this, close, no] {
                       close();
                       sc.runNow(no);
                     }});
  m.onCancel = m.items[1].act;
  menus.push(m);
}

// ---------------------------------------------------------------------------
// Menus
// ---------------------------------------------------------------------------
void Game::titleMenu() {
  if (expedition_) expedition_->leave();  // remet les données du jeu à la place de celles de l'expédition
  if (online_) online_->leave();          // ferme les connexions du multijoueur
  mode = Mode::Title;
  editorTest_ = storyTest_ = false;
  menus.clear();
  Menu m;
  m.x = 102, m.y = 128, m.w = 116, m.rows = 6, m.cancelable = false;
  m.items.push_back({"Nouvelle partie", "", "", true, [this] { starterMenu(); }});
  bool can = saveExists();
  m.items.push_back({"Continuer", "", "", can, [this] {
                       if (loadGame()) {
                         menus.clear();
                         mode = Mode::Map;
                         showRegionBanner();
                       }
                     }});
  m.items.push_back({"Expédition", "", "Des régions générées sans fin, façon roguelite.", true, [this] {
                       if (!expedition_) expedition_ = std::make_unique<Expedition>(*this);
                       expedition_->menu();
                     }});
  m.items.push_back({"Multijoueur", "", "Duel en ligne contre un ami.", true, [this] {
                       if (!online_) online_ = std::make_unique<Online>(*this);
                       online_->menu();
                     }});
  m.items.push_back({"Outils", "", "Arène, réglages et éditeurs.", true, [this] { toolsMenu(); }});
  m.items.push_back({"Quitter", "", "", true, [this] { quit = true; }});
  if (can) m.sel = 1;
  menus.push(m);
}

void Game::toolsMenu() {
  Menu m;
  m.title = "Outils";
  m.x = 92, m.y = 92, m.w = 136, m.rows = 6;
  m.items.push_back({"Arène de combat", "", "Composer deux équipes, combattre ou simuler des combats.", true, [this] {
                       if (!arena_) arena_ = std::make_unique<Arena>(*this);
                       mode = Mode::Arena;
                       arena_->open();
                     }});
  m.items.push_back({"Réglages", "", "Modifier les règles, espèces, techniques, types et objets.", true, [this] {
                       if (!settings_) settings_ = std::make_unique<Settings>(*this);
                       mode = Mode::Settings;
                       settings_->open();
                     }});
  m.items.push_back({"Éditeur de cartes", "", "Peindre les cartes, placer habitants, coffres, passages, zones… et tester.", true, [this] {
                       if (!editor_) editor_ = std::make_unique<MapEditor>(*this);
                       mode = Mode::Editor;
                       editor_->open();
                     }});
  m.items.push_back({"Éditeur d'histoire", "", "Dialogues et événements : conditions, questions, combats, récompenses… et les jouer.", true, [this] {
                       if (!story_) story_ = std::make_unique<StoryEditor>(*this);
                       mode = Mode::Story;
                       story_->open();
                     }});
  m.items.push_back({"Retour", "", "", true, [this] { menus.pop(); }});
  menus.push(m);
}

void Game::starterMenu() {
  Menu m;
  m.title = "Premier compagnon";
  m.x = 90, m.y = 132, m.w = 140, m.rows = 3;
  for (const std::string& id : rules().starters) {
    const Species& s = species(id);
    std::string lim = s.limit.empty() ? "aucune" : moveInfo(s.limit).name;
    m.items.push_back({s.name, typesName(s), "Type " + typesName(s) + " · Limite : " + lim, true,
                       [this, id] { newGame(id); }});
  }
  menus.push(m);
}

void Game::newGame(const std::string& starter) {
  const Rules& r = rules();
  menus.clear();
  sc.clear();
  team = {makeFighter(r.hero, r.startLevel), makeFighter(starter, r.startLevel)};
  items.clear();
  for (auto& [id, n] : r.startItems) items[id] = n;
  gold = r.startGold;
  flags.clear();
  respawnMap = std::max(0, mapIndex(r.respawnMap)), respawnX = r.respawnX, respawnY = r.respawnY;
  mapId = -1;
  changeMap(std::max(0, mapIndex(r.startMap)), r.startX, r.startY, r.startDir);
  mode = Mode::Map;
  if (!r.startEvent.empty()) runEvent(r.startEvent);
}

void Game::pauseMenu() {
  panelMode_ = 1;
  Menu m;
  m.x = 8, m.y = 8, m.w = 100, m.rows = 8;
  if (editorTest_ && editor_)
    m.items.push_back({"Fin du test", "", "Revenir à l'éditeur de cartes, à l'endroit où vous êtes.", true, [this] {
                         panelMode_ = 0;
                         editor_->returnFromTest();
                       }});
  if (storyTest_ && story_)
    m.items.push_back({"Fin du test", "", "Revenir à l'éditeur d'histoire.", true, [this] {
                         panelMode_ = 0;
                         story_->returnFromTest();
                       }});
  m.items.push_back({"Équipe", "", "Ordre de combat et fiches.", true, [this] { teamMenu(); }});
  m.items.push_back({"Tactiques", "", "Ce que chaque membre fait tout seul en combat (mode auto : touche Tab).", true, [this] { tacticsMenu(); }});
  m.items.push_back({"Objets", "", "Utiliser un objet.", true, [this] { itemMenu(); }});
  m.items.push_back({"Magie", "", "Lancer un sort de soin.", true, [this] { magicMenu(); }});
  if (inExpedition()) {
    m.items.push_back({"Sauvegarder", "", "Expédition : " + expedition_->status() + ".", true,
                       [this] { notice(saveGame() ? "Expédition sauvegardée." : "Impossible de sauvegarder."); }});
    m.items.push_back({"Abandonner", "", "Arrêter l'expédition ici et recevoir les éclats gagnés.", true, [this] {
                         ask("Abandonner l'expédition ?", [this] { expedition_->onDefeat(true); });
                       }});
    m.items.push_back({"Écran titre", "", "Sauvegarder et revenir à l'écran titre.", true, [this] {
                         saveGame();
                         titleMenu();
                       }});
  } else {
    m.items.push_back({"Sauvegarder", "", "", true, [this] { notice(saveGame() ? "Partie sauvegardée." : "Impossible de sauvegarder."); }});
    m.items.push_back({"Écran titre", "", "", true, [this] {
                         ask("Revenir à l'écran titre ? (pensez à sauvegarder)", [this] { titleMenu(); });
                       }});
  }
  m.items.push_back({"Reprendre", "", "", true, [this] { menus.clear(); }});
  menus.push(m);
}

// Tactiques : mode auto, puis la liste des règles de chaque membre (tactics.hpp)
void Game::tacticsMenu(int sel) {
  panelMode_ = 0;
  Menu m;
  m.title = "Tactiques";
  m.x = 112, m.y = 8, m.w = 200, m.rows = 11;
  m.sel = sel;
  {
    MenuItem it{"Mode auto en combat", "", "Oui : quand sa jauge est pleine, chaque membre joue sa première tactique possible. En combat, Tab l'active ou le coupe.",
                true, [this] { tacticsAuto = !tacticsAuto; }};
    it.adjust = [this](int) { tacticsAuto = !tacticsAuto; };
    it.rightFn = [this] { return std::string(tacticsAuto ? "Oui" : "Non"); };
    m.items.push_back(it);
  }
  m.items.push_back(menuHeader("Règles de chaque membre"));
  for (size_t i = 0; i < team.size(); i++) {
    FighterP f = team[i];
    MenuItem it{f->name(), "", "Entrée : modifier ses tactiques (" + std::to_string(tacticSlots(f->lvl)) + " lignes au niveau " +
                                   std::to_string(f->lvl) + ").",
                true, [this, f] {
                  TacticsTarget t;
                  t.list = &f->tactics;
                  t.on = &f->tacticsOn;
                  t.fighter = f;
                  t.title = "Tactiques de " + f->name();
                  t.slots = tacticSlots(f->lvl);
                  openTacticsEditor(*this, t);
                }};
    it.rightFn = [f] {
      if (!f->tacticsOn) return std::string("manuel");
      int n = 0, slots = tacticSlots(f->lvl);
      for (int k = 0; k < (int)f->tactics.size() && k < slots; k++) n += f->tactics[k].on;
      return std::to_string(n) + (n > 1 ? " règles" : " règle");
    };
    m.items.push_back(it);
  }
  m.onCancel = [this] {
    panelMode_ = 1;
    menus.pop();
  };
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
  panelMode_ = 0;  // la liste prend la place du résumé de l'équipe
  Menu m;
  m.title = "Objets  ·  " + std::to_string(gold) + " or";
  m.x = 8, m.y = 8, m.w = 170, m.rows = 7;
  m.sel = sel;
  m.onCancel = [this] {
    panelMode_ = 1;
    menus.pop();
  };
  int idx = 0;
  for (auto& d : allItems()) {
    int n = items.count(d.id) ? items[d.id] : 0;
    if (n <= 0) continue;
    std::string id = d.id;
    int myIdx = idx++;
    m.items.push_back({d.name, "×" + std::to_string(n), d.desc + (d.field ? "" : d.key ? " (objet important)" : " (en combat)"), d.field,
                       [this, id, myIdx] {
                         pickMember("Sur qui ?", [id](const Fighter& f) {
                           const ItemDef& d = item(id);
                           if (d.revive) return !f.alive();
                           return f.alive() && ((d.healHp && f.hp < f.mhp) || (d.healMp && f.mp < f.mmp));
                         }, [this, id, myIdx](Fighter& f) {
                           const ItemDef& d = item(id);
                           items[id]--;
                           if (d.revive) f.hp = std::max(1, f.mhp * d.revive / 100);
                           else {
                             f.hp = std::min(f.mhp, f.hp + d.healHp);
                             f.mp = std::min(f.mmp, f.mp + d.healMp);
                           }
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
  panelMode_ = 0;  // la liste prend la place du résumé de l'équipe
  Menu m;
  m.title = "Qui lance le sort ?";
  m.x = 8, m.y = 8, m.w = 170, m.rows = 8;
  m.onCancel = [this] {
    panelMode_ = 1;
    menus.pop();
  };
  for (auto& f : team) {
    FighterP caster = f;
    std::vector<std::string> heal;
    for (auto& id : f->spells())
      if ((moveInfo(id).kind == Kind::Heal && moveInfo(id).power > 0) || moveInfo(id).kind == Kind::Revive) heal.push_back(id);
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
  panelMode_ = 0;
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
  return s + (inExpedition() ? expedition_->saveName : saveName_);
}
bool Game::saveExists() const {
  std::ifstream f(savePath());
  return f.good();
}
bool Game::saveGame() {
  std::ofstream f(savePath());
  if (!f) return false;
  f << "BRUMEVAL 1\n";
  if (inExpedition()) expedition_->writeSave(f);  // en premier : le chargement génère d'abord le monde
  f << "carte " << M().id << ' ' << px << ' ' << py << ' ' << dir << '\n';
  f << "reveil " << maps()[respawnMap].id << ' ' << respawnX << ' ' << respawnY << '\n';
  f << "or " << gold << '\n';
  for (auto& fl : flags) f << "drapeau " << fl << '\n';
  for (auto& [id, n] : items)
    if (n > 0) f << "objet " << id << ' ' << n << '\n';
  f << "auto " << tacticsAuto << '\n';
  static const char KIND[] = {'a', 't', 'o'};  // action automatique, technique, objet
  for (auto& m : team) {
    f << "membre " << m->sp << ' ' << m->lvl << ' ' << m->xp << ' ' << m->hp << ' ' << m->mp << ' ' << m->lim << '\n';
    f << "tactiques " << m->tacticsOn << '\n';
    for (auto& t : m->tactics) f << "tactique " << t.on << ' ' << t.cond << ' ' << t.value << ' ' << KIND[(int)t.kind] << ' ' << t.act << '\n';
  }
  return true;
}
bool Game::loadGame() {
  std::ifstream f(savePath());
  std::string head;
  if (!f || !std::getline(f, head) || head.rfind("BRUMEVAL", 0) != 0) return false;
  team.clear();
  items.clear();
  flags.clear();
  // Anciennes sauvegardes : cartes désignées par leur numéro
  static const char* OLD_MAPS[] = {"vallee", "cendrelune", "grotte"};
  auto mapOf = [&](const std::string& v) {
    int m = mapIndex(v);
    if (m < 0 && v.size() == 1 && v[0] >= '0' && v[0] <= '2') m = mapIndex(OLD_MAPS[v[0] - '0']);
    return std::max(0, m);
  };
  std::vector<std::string> oldChests;
  std::string line;
  mapId = 0;
  while (std::getline(f, line)) {
    std::istringstream s(line);
    std::string k, v;
    s >> k;
    if (k == "expedition" || k == "graine") {
      std::string rest;
      std::getline(s, rest);
      if (!expedition_ || !expedition_->readSave(k, rest)) return false;
    } else if (k == "carte") {
      s >> v >> px >> py >> dir;
      mapId = mapOf(v);
    } else if (k == "reveil") {
      s >> v >> respawnX >> respawnY;
      respawnMap = mapOf(v);
    } else if (k == "or") s >> gold;
    else if (k == "drapeau") {
      s >> v;
      // Ancien format des coffres : coffre:<numéro de carte>:<numéro du coffre>
      if (v.rfind("coffre:", 0) == 0 && std::count(v.begin(), v.end(), ':') == 2) oldChests.push_back(v);
      else flags.insert(v);
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
    } else if (k == "auto") s >> tacticsAuto;
    else if (k == "tactiques" && !team.empty()) {  // les anciennes sauvegardes gardent les tactiques de départ
      s >> team.back()->tacticsOn;
      team.back()->tactics.clear();
    } else if (k == "tactique" && !team.empty()) {
      Tactic t;
      char kind = 'a';
      s >> t.on >> t.cond >> t.value >> kind >> t.act;
      t.kind = kind == 't' ? Tactic::Act::Move : kind == 'o' ? Tactic::Act::Item : Tactic::Act::Auto;
      if (s && findTacticCond(t.cond)) team.back()->tactics.push_back(t);
    }
  }
  for (auto& v : oldChests) {
    int m = mapOf(v.substr(7, v.find(':', 7) - 7));
    int i = std::atoi(v.substr(v.rfind(':') + 1).c_str());
    if (i >= 0 && i < (int)maps()[m].chests.size()) {
      auto& c = maps()[m].chests[i];
      flags.insert("coffre:" + maps()[m].id + ":" + std::to_string(c.x) + ":" + std::to_string(c.y));
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
    case Mode::Arena:
      if (arena_) arena_->draw();
      break;
    case Mode::Settings:
      if (settings_) settings_->draw();
      break;
    case Mode::Editor:
      if (editor_) editor_->draw();
      break;
    case Mode::Story:
      if (story_) story_->draw();
      break;
  }
  if (textOn_) drawTextEdit();
}

void Game::drawTitle() {
  if (expedition_ && expedition_->onScreen()) return expedition_->draw(g);
  if (online_ && online_->onScreen()) return online_->draw(g);
  float L = g.left(), W = (float)g.fullW;
  g.gradV(L, 0, W, SCREEN_H, rgb(0x141a3c), rgb(0x3c4278));
  g.poly({{L, 150}, {L, 120}, {0, 150}, {50, 96}, {100, 136}, {160, 80}, {220, 128}, {270, 100}, {320, 126}, {L + W, 104}, {L + W, 240}, {L, 240}},
         rgb(0x262a52));
  g.rect(L, 160, W, 80, rgb(0x1d2148));
  for (int i = 0; i < 6 * W / SCREEN_W; i++) g.ellipse(L + std::fmod(i * 70 + time * 18, W + 100) - 50, 112 + (i % 6) * 10, 90, 7, rgb(0xe6ebff, 26));
  g.textBig(160, 26, "BRUMEVAL", GOLD, 4, 1);
  g.text(160, 76, "Les gardiens de la vallée", rgb(0xc9cbe0), 1);
  drawHuman(g, look(0), 144, 92, 2, DOWN, 0, false);
  drawCreature(g, "braisenard", 92, 124, 1, true, time);
  drawCreature(g, "gouttelin", 232, 124, 1, false, time + .5f);
  if (menus.active()) {
    menus.draw(g, time);
    auto lines = Gfx::wrap(menus.help(), 284);
    if (!lines.empty() && !lines[0].empty()) {
      int h = 9 + 11 * (int)lines.size();
      g.window(14, 236 - h, 292, h);
      for (size_t i = 0; i < lines.size(); i++) g.text(160, 236 - h + 4 + i * 11, lines[i], WHITE, 1);
    }
  }
  if (!menus.active() || menus.help().empty()) {
    g.text(g.left() + 4, 227, "F11 : plein écran", rgb(0x8a92b8), 0, false);
    g.text(g.right() - 4, 227, "v" BRUMEVAL_VERSION, rgb(0x8a92b8), 2, false);
  }
}

void Game::drawMap() {
  const MapDef& m = M();
  float k = moving ? moveT : 1;
  float ppx = (fromX + (px - fromX) * k) * 16, ppy = (fromY + (py - fromY) * k) * 16;
  if (!moving) ppx = px * 16.f, ppy = py * 16.f;
  // camX : position dans la carte du bord gauche de la zone du milieu ; l'écran montre de camX + left() à camX + right()
  float lo = -g.left(), hi = m.w() * 16 - g.right();
  int camX = hi >= lo ? (int)std::clamp(ppx - SCREEN_W / 2 + 8, lo, hi) : (int)((m.w() * 16 - SCREEN_W) / 2);
  int camY = (int)std::clamp(ppy - SCREEN_H / 2 + 8, 0.f, float(m.h() * 16 - SCREEN_H));
  int tx0 = std::max(0, (int)std::floor((camX + g.left()) / 16)), tx1 = (int)((camX + g.right()) / 16), ty0 = camY / 16;
  for (int y = ty0; y <= ty0 + SCREEN_H / 16 && y < m.h(); y++)
    for (int x = tx0; x <= tx1 && x < m.w(); x++) drawTile(g, m, x, y, x * 16 - camX, y * 16 - camY, time);
  for (auto& b : m.buildings) drawBuilding(g, b, b.x * 16 - camX, b.y * 16 - camY);
  for (size_t i = 0; i < m.chests.size(); i++)
    drawChest(g, m.chests[i].x * 16 - camX, m.chests[i].y * 16 - camY, has(chestFlag((int)i)));
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
  if (exclaimNpc_ >= 0 && exclaimNpc_ < (int)m.npcs.size() && time - exclaimT_ < 1.2f) {
    const Npc& n = m.npcs[exclaimNpc_];
    float bx = n.x * 16 - camX + 4, by = n.y * 16 - camY - 16;
    actors.push_back({1e9f, [&, bx, by] {
                        g.rect(bx, by, 9, 11, rgb(0xffffff));
                        g.frame(bx, by, 9, 11, rgb(0x1a1a24));
                        g.text(bx + 2, by - 1, "!", rgb(0xd2493f), 0, false);
                      }});
  }
  int step = moving ? (moveT < .5f ? 1 : 2) : 0;
  // Le joueur a l'apparence de Lior ; en Expédition, celle du premier personnage de l'équipe (le héros choisi)
  int playerLook = 0;
  if (inExpedition())
    for (auto& f : team)
      if (f->S().human) {
        playerLook = f->S().look;
        break;
      }
  actors.push_back({ppy, [&] { drawHuman(g, look(playerLook), ppx - camX, ppy - camY - 2, 1, dir, step, false); }});
  std::sort(actors.begin(), actors.end(), [](const Actor& a, const Actor& b) { return a.y < b.y; });
  for (auto& a : actors) a.draw();
  // Ambiance
  bool amb = m.ambianceUntil.empty() || !has(m.ambianceUntil);
  if (m.ambiance == "neige" && amb)
    for (int i = 0; i < 40 * g.fullW / SCREEN_W; i++) {
      float x = g.left() + std::fmod(i * 47.f + time * (6 + i % 4) + std::sin(time + i) * 6, g.fullW + 10.f) - 5;
      float y = std::fmod(i * 29.f + time * (18 + i % 6), 250.f) - 5;
      g.rect(x, y, i % 3 ? 1 : 2, i % 3 ? 1 : 2, rgb(0xffffff, 200));
    }
  if (m.ambiance == "lucioles" && amb)
    for (int i = 0; i < 14 * g.fullW / SCREEN_W; i++) {
      float x = g.left() + std::fmod(i * 61.f + std::sin(time * .7f + i) * 20 + 40, (float)g.fullW), y = std::fmod(i * 37.f + std::cos(time * .5f + i * 2) * 14, 240.f);
      g.ellipse(x, y, 2, 2, rgb(0xe8ff9a, uint8_t(90 + 80 * std::sin(time * 3 + i))));
    }
  if (m.ambiance == "brume" && amb)
    for (int i = 0; i < 5 * g.fullW / SCREEN_W; i++)
      g.ellipse(g.left() + std::fmod(i * 97 + time * 22, g.fullW + 140.f) - 70, 30 + (i % 5) * 46, 90, 10, rgb(0xe6ebff, 22));
  if (m.ambiance == "cendres" && amb)
    for (int i = 0; i < 30 * g.fullW / SCREEN_W; i++) {
      float x = g.left() + std::fmod(i * 53.f + time * (8 + i % 5), g.fullW + 10.f) - 5, y = std::fmod(i * 31.f + time * (14 + i % 7), 250.f) - 5;
      g.rect(x, y, 1, 1, rgb(0xcfc6c0, 170));
    }
  if (m.ambiance == "obscurite" && amb) {
    float cx = ppx - camX + 8, cy = ppy - camY + 8;
    for (int ring = 0; ring < 2; ring++) {
      float R = ring == 0 ? 92 : 68;
      Color c = rgb(0x05030a, ring == 0 ? 200 : 90);
      for (int y = 0; y < SCREEN_H; y++) {
        float dy = y + .5f - cy;
        float span = R * R - dy * dy;
        if (span <= 0) {
          if (ring == 0) g.rect(g.left(), y, g.fullW, 1, c);
          continue;
        }
        float w = std::sqrt(span);
        float R0 = ring == 0 ? 0 : 92;
        float w0 = R0 * R0 - dy * dy > 0 ? std::sqrt(R0 * R0 - dy * dy) : 0;
        if (ring == 0) {
          g.rect(g.left(), y, cx - w - g.left(), 1, c);
          g.rect(cx + w, y, g.right() - cx - w, 1, c);
        } else {
          g.rect(cx - w0, y, w0 - w, 1, c);
          g.rect(cx + w, y, w0 - w, 1, c);
        }
      }
    }
  }
  // Bandeau du nom de la région
  if (banner > 0 && !menus.active()) {  // le nom de la région s'efface quand un menu s'ouvre
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
    int rowH = team.size() > 6 ? 20 : 22;  // plus serré avec une grande équipe, pour laisser la place à l'aide
    int h = 22 + (int)team.size() * rowH + 6;
    g.window(x, y, 320 - x - 8, h);
    g.text(x + 8, y + 5, "Équipe", GOLD);
    g.text(320 - 16, y + 5, std::to_string(gold) + " or", WHITE, 2);
    auto fr = front();
    for (size_t i = 0; i < team.size(); i++) {
      auto& f = team[i];
      float ry = y + 20 + i * rowH;
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
  std::string t, s;
  for (auto& id : f.techs()) t += (t.empty() ? "" : ", ") + moveInfo(id).name;
  for (auto& id : f.spells()) s += (s.empty() ? "" : ", ") + moveInfo(id).name;
  auto techLines = Gfx::wrap("Techniques : " + t, w - 16), magicLines = s.empty() ? std::vector<std::string>{} : Gfx::wrap("Magie : " + s, w - 16);
  int lines = (int)techLines.size() + (int)magicLines.size() + (f.S().limit.empty() ? 0 : 1);
  g.window(x, y, w, std::min(SCREEN_H - y - 2, 34 + 4 * 11 + 3 + 4 * 11 + 3 + lines * 11 + 6));
  g.text(x + 8, y + 6, f.name(), GOLD);
  g.text(x + w - 8, y + 6, "Niveau " + std::to_string(f.lvl), WHITE, 2);
  std::string role = f.S().human ? f.S().role : std::string("Créature de type ") + typeName(f.S().type);
  g.text(x + 8, y + 19, role.size() > 40 ? typeName(f.S().type) : role, MUTED);
  if (f.S().human) drawHuman(g, look(f.S().look), x + w - 40, y + 32, 2, DOWN, 0, false);  // sous la ligne du rôle
  else drawCreature(g, f.sp, x + w - 26, y + 48, .9f, false, time);
  int ly = y + 34;
  auto row = [&](const std::string& a, const std::string& b) {
    g.text(x + 8, ly, a, MUTED);
    g.text(x + 54, ly, b, WHITE);
    ly += 11;
  };
  row("Type", typesName(f.S()));
  row("PV", std::to_string(f.hp) + "/" + std::to_string(f.mhp));
  row("PM", std::to_string(f.mp) + "/" + std::to_string(f.mmp));
  row("Exp.", std::to_string(f.xp) + "/" + std::to_string(f.need()));
  ly += 3;
  // Statistiques sur deux colonnes
  auto pair = [&](const char* a, int va, const char* b, const std::string& vb) {
    g.text(x + 8, ly, a, MUTED);
    g.text(x + 76, ly, std::to_string(va), WHITE, 2);
    g.text(x + 84, ly, b, MUTED);
    g.text(x + w - 8, ly, vb, WHITE, 2);
    ly += 11;
  };
  pair("Attaque", f.atk, "Défense", std::to_string(f.def));
  pair("Magie", f.mag, "Résist.", std::to_string(f.res));
  pair("Vitesse", f.spd, "Précis.", std::to_string(f.acc) + "%");
  pair("Esquive", f.eva, "Critique", std::to_string(f.crit) + "%");
  ly += 3;
  for (auto& l : techLines) g.text(x + 8, ly, l, WHITE), ly += 11;
  for (auto& l : magicLines) g.text(x + 8, ly, l, rgb(0x9fd8ff)), ly += 11;
  if (!f.S().limit.empty()) g.text(x + 8, ly, "Limite : " + moveInfo(f.S().limit).name, rgb(0xff9ad0));
}

void Game::drawEnding() {
  float L = g.left(), W = (float)g.fullW;
  g.gradV(L, 0, W, SCREEN_H, rgb(0x0e1550), rgb(0x3a6fb0));
  g.ellipse(250, 60, 22, 22, rgb(0xfff4c0));
  g.poly({{L, 200}, {L, 170}, {0, 200}, {80, 150}, {150, 190}, {230, 140}, {320, 190}, {L + W, 160}, {L + W, 240}, {L, 240}}, rgb(0x2e6b3a));
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
