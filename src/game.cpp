#include "game.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

#include "arena.hpp"
#include "audio.hpp"
#include "battle.hpp"
#include "expedition.hpp"
#include "mapedit.hpp"
#include "online.hpp"
#include "pilot.hpp"
#include "options.hpp"
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

// Manette : croix ou stick pour se déplacer, A valider, B annuler, Start menu, Select mode auto (Tab),
// gâchettes hautes page précédente / suivante (éditeurs)
void Game::onPad(int button, bool down) {
  if (textOn_ || bindKey_ >= 0) return;
  switch (button) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return padDirection(UP, down);
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return padDirection(DOWN, down);
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return padDirection(LEFT, down);
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return padDirection(RIGHT, down);
    default: break;
  }
  if (!down) return;
  in.mouseOn = false;
  switch (button) {
    case SDL_CONTROLLER_BUTTON_A: in.confirm = true; break;
    case SDL_CONTROLLER_BUTTON_B: in.cancel = in.menu = true; break;
    case SDL_CONTROLLER_BUTTON_START: in.menu = true; break;
    case SDL_CONTROLLER_BUTTON_BACK:
    case SDL_CONTROLLER_BUTTON_Y: in.menu = in.tab = true; break;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: in.prev = true; break;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: in.next = true; break;
    default: break;
  }
}
void Game::onStick(int axis, int value) {
  if (axis != SDL_CONTROLLER_AXIS_LEFTX && axis != SDL_CONTROLLER_AXIS_LEFTY) return;
  int neg = axis == SDL_CONTROLLER_AXIS_LEFTX ? LEFT : UP, pos = axis == SDL_CONTROLLER_AXIS_LEFTX ? RIGHT : DOWN;
  // Seuils différents pour pousser et relâcher : pas de tremblement autour de la limite
  for (int d : {neg, pos}) {
    bool push = d == neg ? value < -16000 : value > 16000, keep = d == neg ? value < -8000 : value > 8000;
    bool on = stickDir_[d] ? keep : push;
    if (on != stickDir_[d]) {
      stickDir_[d] = on;
      padDirection(d, on || padDir_[d]);
    }
  }
}
void Game::padDirection(int d, bool on) {
  if (on && !in.hold[d]) {
    in.press[d] = true;
    padRepeat_[d] = .35f;  // premier délai avant la répétition
  }
  in.hold[d] = on;
  if (!on) padDir_[d] = stickDir_[d] = false;
  if (on && !stickDir_[d]) padDir_[d] = true;
  in.mouseOn = false;
}

void Game::onKey(SDL_Scancode k, bool down, bool repeat) {
  if (pilot_ && pilot_->demo() && down) {  // démo : une touche rend la main au joueur
    pilot_.reset();
    for (bool& h : in.hold) h = false;
    notice("Vous reprenez la main (sauvegarde à part : " + saveName_ + ").");
    return;
  }
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
  if (bindKey_ >= 0) {  // Options > Touches : la prochaine touche appuyée remplace celle de l'action
    if (down && !repeat) bindKey(k);
    return;
  }
  // Touches choisies dans les options ; les flèches, Entrée et Échap marchent toujours
  const auto& K = options().keys;
  int d = -1;
  if (k == SDL_SCANCODE_UP || k == K[K_UP]) d = UP;
  if (k == SDL_SCANCODE_DOWN || k == K[K_DOWN]) d = DOWN;
  if (k == SDL_SCANCODE_LEFT || k == K[K_LEFT]) d = LEFT;
  if (k == SDL_SCANCODE_RIGHT || k == K[K_RIGHT]) d = RIGHT;
  if (d >= 0) {
    if (!repeat) in.hold[d] = down;
    if (down) in.press[d] = true;
    return;
  }
  if (down && k == SDL_SCANCODE_PAGEUP) in.prev = true;
  if (down && k == SDL_SCANCODE_PAGEDOWN) in.next = true;
  if (down && k == SDL_SCANCODE_DELETE) in.del = true;
  if (!down || repeat) return;
  if (k == SDL_SCANCODE_RETURN || k == SDL_SCANCODE_KP_ENTER || k == K[K_OK]) in.confirm = true;
  if (k == SDL_SCANCODE_ESCAPE || k == K[K_BACK]) in.cancel = in.menu = true;
  if (k == K[K_AUTO]) in.menu = in.tab = true;
}

// ---------------------------------------------------------------------------
// Options : volumes, plein écran, touches (options.hpp)
// ---------------------------------------------------------------------------
void Game::optionsMenu(int sel) {
  int panel = panelMode_;
  panelMode_ = 0;  // le menu prend la place du résumé de l'équipe
  Menu m;
  m.title = "Options";
  m.x = 60, m.y = 40, m.w = 200, m.rows = 5, m.sel = sel;
  auto volume = [this](const std::string& label, int* v, const std::string& help) {
    MenuItem it{label, "", help, true, nullptr};
    it.adjust = [v](int d) {
      *v = std::clamp(*v + d, 0, 10);
      applyVolumes();
      saveOptions();
    };
    it.rightFn = [v] { return *v ? std::to_string(*v) + " / 10" : std::string("coupé"); };
    return it;
  };
  m.items.push_back(volume("Musique", &options().music, "Gauche / droite : volume de la musique."));
  m.items.push_back(volume("Effets sonores", &options().effects, "Gauche / droite : volume des bruitages."));
  auto full = [] {
    options().fullscreen = !options().fullscreen;
    saveOptions();
  };
  MenuItem fs{"Plein écran", "", "Aussi avec F11 ou Alt+Entrée.", true, full};
  fs.adjust = [full](int) { full(); };
  fs.rightFn = [] { return std::string(options().fullscreen ? "Oui" : "Non"); };
  m.items.push_back(fs);
  m.items.push_back({"Touches", "", "Changer les touches du clavier. La manette se branche toute seule.", true, [this] { keysMenu(); }});
  m.items.push_back({"Retour", "", "", true, [this, panel] {
                       panelMode_ = panel;
                       menus.pop();
                     }});
  m.onCancel = m.items.back().act;
  menus.push(m);
}

void Game::keysMenu(int sel) {
  Menu m;
  m.title = "Touches";
  m.x = 72, m.y = 52, m.w = 210, m.rows = 9, m.sel = sel;
  const std::string help = "Entrée : appuyer ensuite sur la nouvelle touche. Les flèches, Entrée et Échap marchent toujours.";
  for (int a = 0; a < N_KEYS; a++) {
    MenuItem it{keyActionName(a), "", help, true, [this, a] { bindKey_ = a; }};
    it.rightFn = [a] { return keyName(options().keys[a]); };
    m.items.push_back(it);
  }
  m.items.push_back({"Touches par défaut", "", "ZQSD (WASD en QWERTY), Espace, Retour arrière et Tab.", true, [this] {
                       options().keys = Options::defaultKeys();
                       saveOptions();
                       notice("Touches par défaut remises.");
                     }});
  m.items.push_back({"Retour", "", "", true, [this] { menus.pop(); }});
  menus.push(m);
}

// Nouvelle touche pour l'action bindKey_ (Échap : annuler). Si une autre action l'avait, elle prend l'ancienne.
void Game::bindKey(SDL_Scancode k) {
  int a = bindKey_;
  bindKey_ = -1;
  in.endFrame();
  if (k == SDL_SCANCODE_ESCAPE) return;
  if (k == SDL_SCANCODE_RETURN || k == SDL_SCANCODE_KP_ENTER || k == SDL_SCANCODE_F11 || k == SDL_SCANCODE_UP || k == SDL_SCANCODE_DOWN ||
      k == SDL_SCANCODE_LEFT || k == SDL_SCANCODE_RIGHT)
    return notice("Cette touche marche déjà toute seule.");
  auto& K = options().keys;
  for (auto& other : K)
    if (other == k) other = K[a];
  K[a] = k;
  saveOptions();
  notice(std::string(keyActionName(a)) + " : " + keyName(k) + ".");
}

void Game::drawKeyPrompt() {
  g.rect(g.left(), 0, g.fullW, SCREEN_H, rgb(0x000010, 120));
  g.newLayer();  // fenêtre modale : le reste est assombri derrière
  g.window(40, 96, 240, 46);
  g.text(160, 102, "Nouvelle touche pour « " + std::string(keyActionName(bindKey_)) + " »", rgb(0xffd34d), 1);
  g.text(160, 116, "Appuyez sur la touche voulue…", WHITE, 1);
  g.text(160, 128, "Échap : annuler", MUTED, 1);
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
  // Manette : une direction tenue se répète (comme une touche du clavier), pour parcourir les menus
  for (int d = 0; d < 4; d++)
    if (padDir_[d] || stickDir_[d]) {
      padRepeat_[d] -= dt;
      if (padRepeat_[d] <= 0) {
        in.press[d] = true;
        padRepeat_[d] = .09f;
      }
    }
  if (online_) online_->update(dt);  // réseau du multijoueur, dans tous les écrans
  if (pilot_) pilot_->update(dt);    // joueur automatique : il choisit les touches de cette image
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
        groupBattle_ = false;
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
  updateMusic();
  in.endFrame();
}

// Musique selon l'écran : celle du lieu (thème de la carte), du combat ou du boss ; silence
// pendant la fin d'un combat (la fanfare de victoire ou de défaite se joue seule)
void Game::updateMusic() {
  std::string t = "titre";
  if (mode == Mode::Battle && battle_) t = battle_->ending_ ? "" : battle_->boss ? "boss" : "combat";
  else if (mode == Mode::Map && mapId >= 0 && mapId < (int)maps().size()) t = audio::musicForPlace(themeName(M().theme));
  audio::music(t);
}

// ---------------------------------------------------------------------------
// Équipe
// ---------------------------------------------------------------------------
std::vector<FighterP> Game::front() const { return frontOf(team, rules().frontSize); }
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
  for (auto& f : setup.foes) see(*f);  // bestiaire (les renforts : quand ils entrent, battle.cpp)
  afterBattle_ = std::move(after);
  arenaBattle_ = mode == Mode::Arena;
  Theme th = setup.theme >= 0 ? Theme(setup.theme) : M().theme;
  battle_ = std::make_unique<Battle>(*this, std::move(setup), th);
  mode = Mode::Battle;
  menus.clear();
  audio::play("rencontre");
}
void Game::startBattle(std::vector<FighterP> foes, bool boss, std::function<void(BattleResult)> after, bool canFlee, bool canCapture) {
  BattleSetup s;
  s.foes = std::move(foes);
  s.boss = boss, s.canFlee = canFlee, s.canCapture = canCapture;
  startBattle(std::move(s), std::move(after));
}

bool Game::inExpedition() const { return expedition_ && expedition_->active(); }

void Game::defeat() {
  if (inExpedition()) {
    // À plusieurs, une défaite en solo ramène au village ; seule celle contre le gardien, ensemble, arrête l'expédition
    if (expedition_->group() && !groupBattle_) return expedition_->knockedOut();
    return expedition_->onDefeat();
  }
  defeats_++;
  healAll();
  changeMap(respawnMap, respawnX, respawnY, DOWN);
  sc.say("Vous reprenez connaissance chez la guérisseuse. Toute l'équipe est soignée.");
}

// ---------------------------------------------------------------------------
// Carte
// ---------------------------------------------------------------------------
bool Game::npcVisible(const Npc& n) const { return n.hideIf.empty() || !has(n.hideIf); }
bool Game::bossAlive(const BossSpot& b) const { return !has(b.flag); }

bool Game::blocked(int x, int y) const { return blockedOn(M(), x, y); }
bool Game::blockedOn(const MapDef& m, int x, int y) const {
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
      if (nx == b.doorX() && ny == b.doorY()) return enterDoor(b);
    for (auto& b : M().bosses)
      if (bossAlive(b) && nx >= b.x && nx <= b.x + 1 && ny >= b.y && ny <= b.y + 1) return bossTouched(b);
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

// Gardien d'une expédition à plusieurs : il ne s'affronte qu'avec tout le groupe (coop.cpp)
void Game::bossTouched(const BossSpot& b) {
  if (online_ && online_->inGroup() && inExpedition() && b.flag == expedition_->current().bossFlag) return online_->guardian(b.event);
  runEvent(b.event);
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
    if (bossAlive(b) && fx >= b.x && fx <= b.x + 1 && fy >= b.y && fy <= b.y + 1) return bossTouched(b);
  for (auto& b : m.buildings)
    if (fx == b.doorX() && fy == b.doorY()) return enterDoor(b);
}

// Porte d'un bâtiment (Entrée devant elle ou marcher dedans) : on entre s'il a un intérieur, sinon son événement
void Game::enterDoor(const Building& b) {
  audio::play("porte");
  int in = mapIndex(b.interior), ex, ey;
  if (in >= 0 && interiorEntry(maps()[in], M().id, ex, ey)) return changeMap(in, ex, ey, UP);
  runEvent(b.event);
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
  audio::play("coffre");
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
  m.x = 102, m.y = 122, m.w = 116, m.rows = 7, m.cancelable = false;
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
  m.items.push_back({"Multijoueur", "", "Avec des amis : duel, expédition, échanges.", true, [this] {
                       if (!online_) online_ = std::make_unique<Online>(*this);
                       online_->menu();
                     }});
  m.items.push_back({"Outils", "", "Arène, réglages et éditeurs.", true, [this] { toolsMenu(); }});
  m.items.push_back({"Options", "", "Volumes, plein écran et touches.", true, [this] { optionsMenu(); }});
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
  m.items.push_back({"Démo", "", "Le jeu joue tout seul une nouvelle partie. Une touche pour reprendre la main.", true,
                     [this] { startPilot(true); }});
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
  reserve.clear();
  seen.clear(), caught.clear();
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
  m.x = 8, m.y = 8, m.w = 100, m.rows = 9;
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
  m.items.push_back({"Compagnons", "", "La créature qui combat aux côtés de chaque héros.", true, [this] { companionsMenu(); }});
  m.items.push_back({"Réserve", reserve.empty() ? "" : std::to_string(reserve.size()), "Les créatures en plus de l'équipe (" +
                     std::to_string(rules().maxTeam) + " membres au plus).", true, [this] { reserveMenu(); }});
  m.items.push_back({"Bestiaire", "", "Les créatures rencontrées et obtenues, et leur fiche.", true, [this] { bestiaryMenu(); }});
  m.items.push_back({"Tactiques", "", "Ce que chaque membre fait tout seul en combat (mode auto : touche Tab).", true, [this] { tacticsMenu(); }});
  m.items.push_back({"Équipement", "", "Armes, armures et accessoires.", true, [this] { gearMenu(); }});
  m.items.push_back({"Journal", "", "Les quêtes en cours et terminées.", true, [this] { journalMenu(); }});
  m.items.push_back({"Objets", "", "Utiliser un objet.", true, [this] { itemMenu(); }});
  m.items.push_back({"Magie", "", "Lancer un sort de soin.", true, [this] { magicMenu(); }});
  if (inExpedition()) {
    bool group = online_ && online_->inGroup();
    m.items.push_back({"Sauvegarder", "", "Expédition : " + expedition_->status() + ".", true,
                       [this] { notice(saveGame() ? "Expédition sauvegardée." : "Impossible de sauvegarder."); }});
    if (group) m.items.push_back({"Groupe", "", "Où en sont les autres joueurs.", true, [this] { online_->groupMenu(); }});
    m.items.push_back({"Abandonner", "", "Arrêter l'expédition ici et recevoir les éclats gagnés.", true, [this] {
                         ask("Abandonner l'expédition ?", [this] { expedition_->onDefeat(true); });
                       }});
    m.items.push_back({"Écran titre", "", group ? "Sauvegarder, quitter le groupe et revenir à l'écran titre." : "Sauvegarder et revenir à l'écran titre.",
                       true, [this] {
                         saveGame();
                         titleMenu();
                       }});
  } else {
    m.items.push_back({"Sauvegarder", "", "", true, [this] { notice(saveGame() ? "Partie sauvegardée." : "Impossible de sauvegarder."); }});
    m.items.push_back({"Écran titre", "", "", true, [this] {
                         ask("Revenir à l'écran titre ? (pensez à sauvegarder)", [this] { titleMenu(); });
                       }});
  }
  m.items.push_back({"Options", "", "Volumes, plein écran et touches.", true, [this] { optionsMenu(); }});
  m.items.push_back({"Reprendre", "", "", true, [this] { menus.clear(); }});
  m.rows = (int)m.items.size();
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

// ---------------------------------------------------------------------------
// Compagnons : chaque héros choisit la créature qui combat à ses côtés (sinon : la première
// créature libre de l'équipe). En combat, le compagnon agit seul, avec ses tactiques.
// ---------------------------------------------------------------------------
// Réserve : les créatures en plus de l'équipe. On y dépose une créature de l'équipe, on en reprend
// une (s'il reste de la place), ou on en échange une contre un membre de l'équipe.
void Game::reserveMenu(int sel) {
  panelMode_ = 0;
  Menu m;
  m.title = "Réserve  ·  " + std::to_string(reserve.size()) + " créature" + (reserve.size() > 1 ? "s" : "");
  m.x = 8, m.y = 8, m.w = 176, m.rows = 9;
  bool full = (int)team.size() >= rules().maxTeam;
  std::vector<FighterP> creatures;
  for (auto& f : team)
    if (!f->S().human) creatures.push_back(f);
  auto pick = [this](const std::string& title, const std::vector<FighterP>& who, std::function<void(FighterP)> done) {
    Menu p;
    p.title = title;
    p.x = 20, p.y = 20, p.w = 176, p.rows = 8;
    for (auto& c : who) {
      MenuItem it{c->name(), "N." + std::to_string(c->lvl), typesName(c->S()) + " · " + std::to_string(c->hp) + "/" + std::to_string(c->mhp) + " PV",
                  true, [done, c] { done(c); }};
      it.shrink = true;
      p.items.push_back(it);
    }
    menus.push(p);
  };
  m.items.push_back({"Déposer une créature…", "", creatures.empty() ? "Aucune créature dans l'équipe." : "Envoyer une créature de l'équipe dans la réserve.",
                     !creatures.empty(), [this, creatures, pick] {
                       pick("Déposer", creatures, [this](FighterP c) {
                         swapReserve(c, nullptr);
                         notice(c->name() + " va dans la réserve.");
                         menus.pop();
                         menus.pop();
                         reserveMenu(0);
                       });
                     }});
  for (size_t i = 0; i < reserve.size(); i++) {
    FighterP r = reserve[i];
    int at = (int)i + 1;
    std::string help = typesName(r->S()) + " · " + std::to_string(r->hp) + "/" + std::to_string(r->mhp) + " PV. " +
                       (full ? "Entrée : l'échanger contre une créature de l'équipe." : "Entrée : la prendre dans l'équipe.");
    MenuItem it{r->name(), "N." + std::to_string(r->lvl), help, !full || !creatures.empty(), [this, r, at, full, creatures, pick] {
                  if (!full) {
                    swapReserve(nullptr, r);
                    notice(r->name() + " rejoint l'équipe.");
                    menus.pop();
                    return reserveMenu(at);
                  }
                  pick("Échanger contre", creatures, [this, r, at](FighterP c) {
                    swapReserve(c, r);
                    notice(r->name() + " prend la place de " + c->name() + ".");
                    menus.pop();
                    menus.pop();
                    reserveMenu(at);
                  });
                }};
    it.shrink = true;
    m.items.push_back(it);
  }
  if (reserve.empty()) m.items.push_back({"(réserve vide)", "", "Les créatures capturées quand l'équipe est complète arrivent ici.", false, nullptr});
  m.sel = std::clamp(sel, 0, (int)m.items.size() - 1);
  menus.push(m);
}

void Game::noteBestiary() {
  for (auto* v : {&team, &reserve})
    for (auto& f : *v)
      if (f && hasSpecies(f->sp) && !f->S().human) seen.insert(f->sp), caught.insert(f->sp);
}

// Bestiaire : toutes les créatures des données, dans leur ordre ; une créature jamais vue reste « ??? »
void Game::bestiaryMenu(int sel) {
  noteBestiary();
  panelMode_ = 3;
  std::vector<const Species*> all;
  for (auto& s : allSpecies())
    if (!s.human) all.push_back(&s);
  int nSeen = 0, nCaught = 0;
  for (auto* s : all) nSeen += seen.count(s->id) > 0, nCaught += caught.count(s->id) > 0;
  Menu m;
  m.title = "Bestiaire  " + std::to_string(nSeen) + "/" + std::to_string(all.size());
  m.x = 8, m.y = 8, m.w = 132, m.rows = 12;
  for (size_t i = 0; i < all.size(); i++) {
    const Species& s = *all[i];
    bool v = seen.count(s.id), c = caught.count(s.id);
    char num[8];
    std::snprintf(num, sizeof num, "%02d ", int(i + 1));
    std::string id = s.id;
    MenuItem it{num + (v ? s.name : std::string("???")), c ? "★" : v ? "vu" : "",
                c ? "Obtenue." : v ? "Rencontrée, pas encore obtenue." : "Pas encore rencontrée.", true, nullptr, [this, id] { bestiarySel_ = id; }};
    it.shrink = true;
    m.items.push_back(it);
  }
  if (!all.empty()) bestiarySel_ = all[(size_t)std::clamp(sel, 0, (int)all.size() - 1)]->id;
  m.sel = std::clamp(sel, 0, (int)m.items.size() - 1);
  m.items.insert(m.items.begin(), menuHeader("Obtenues : " + std::to_string(nCaught)));
  m.sel++;
  menus.push(m);
}

// Fiche du bestiaire : dessin, types, statistiques de base et lieux où vivre la rencontre
void Game::drawBestiaryPanel(int x, int y) {
  if (!hasSpecies(bestiarySel_)) return;
  const Species& s = species(bestiarySel_);
  int w = 320 - x - 8;
  bool v = seen.count(s.id) > 0, c = caught.count(s.id) > 0;
  std::vector<std::string> places;
  for (auto& mp : maps()) {
    bool here = false;
    for (auto& z : mp.zones) here = here || std::find(z.pool.begin(), z.pool.end(), s.id) != z.pool.end();
    for (auto& b : mp.bosses) here = here || b.id == s.id;
    if (here && mp.theme != Theme::Interieur) places.push_back(mp.name);
  }
  std::string where;
  for (auto& p : places) where += (where.empty() ? "" : ", ") + p;
  auto lines = Gfx::wrap("Lieux : " + (where.empty() ? std::string("inconnus") : where), w - 16);
  int h = 104 + (c ? 42 : 0) + (v ? 11 * (int)lines.size() : 0);
  g.window(x, y, w, h);
  if (!v) {
    g.text(x + w / 2, y + 40, "???", MUTED, 1);
    g.text(x + w / 2, y + 56, "Pas encore rencontrée", MUTED, 1);
    return;
  }
  g.text(x + 8, y + 6, s.name, GOLD);
  g.text(x + 8, y + 18, typesName(s), MUTED);
  drawCreature(g, s.id, x + w / 2, y + 74, 1.2f, false, time);
  int ly = y + 96;
  if (c) {  // statistiques de base, seulement pour une créature obtenue
    const char* names[] = {"PV", "PM", "Att", "Déf", "Mag", "Rés", "Vit"};
    for (int i = 0; i < N_BASE; i++) g.text(x + 8 + (i % 3) * 50, ly + (i / 3) * 11, std::string(names[i]) + " " + std::to_string(s.base[i]), WHITE);
    ly += 36;
  } else {
    g.text(x + 8, ly, "Pas encore obtenue.", MUTED);
    ly += 12;
  }
  for (size_t i = 0; i < lines.size(); i++) g.text(x + 8, ly + 6 + i * 11, lines[i], MUTED);
}

void Game::swapReserve(FighterP out, FighterP in) {
  if (out) {
    auto it = std::find(team.begin(), team.end(), out);
    if (it == team.end()) return;
    if (in) {
      *it = in;  // à sa place dans l'équipe ; le héros qu'elle accompagnait prend la nouvelle venue
      for (auto& h : team)
        if (h->companion.lock() == out) h->companion = in;
    } else
      team.erase(it);
    reserve.push_back(out);
  } else if (in)
    team.push_back(in);
  if (in) reserve.erase(std::remove(reserve.begin(), reserve.end(), in), reserve.end());
}

void Game::companionsMenu(int sel) {
  panelMode_ = 2;  // fiche à droite
  Menu m;
  m.title = "Compagnons";
  m.x = 8, m.y = 8, m.w = 132, m.rows = 8, m.sel = sel;
  int row = 0;
  for (size_t i = 0; i < team.size(); i++) {
    FighterP h = team[i];
    if (!h->S().human) continue;
    FighterP c = companionOf(team, h);
    int at = row++;
    MenuItem it{h->name(), c ? utf8Prefix(c->name(), 10) : "—",
                c ? (h->companion.lock() == c ? "Choisi par vous. " : "Choisi tout seul (première créature libre). ") + std::string("Entrée : changer.")
                  : "Aucune créature libre. Entrée : en choisir une.",
                true, [this, h, at] {
                  Menu p;
                  p.title = "Compagnon de " + utf8Prefix(h->name(), 9);
                  p.x = 20, p.y = 20, p.w = 132, p.rows = 8;
                  p.items.push_back({"Automatique", "", "La première créature libre de l'équipe.", true, [this, h, at] {
                                       h->companion.reset();
                                       menus.pop();
                                       menus.pop();
                                       companionsMenu(at);
                                     }});
                  for (size_t k = 0; k < team.size(); k++) {
                    FighterP c = team[k];
                    if (c->S().human) continue;
                    std::string with;  // le héros qui l'a choisie
                    for (auto& o : team)
                      if (o != h && o->S().human && o->companion.lock() == c) with = o->name();
                    MenuItem ci{c->name(), "N." + std::to_string(c->lvl), with.empty() ? typesName(c->S()) : "Choisie par " + with + " : elle changera de héros.",
                                true, [this, h, c, at] {
                                  for (auto& o : team)  // une créature n'accompagne qu'un seul héros
                                    if (o->companion.lock() == c) o->companion.reset();
                                  h->companion = c;
                                  menus.pop();
                                  menus.pop();
                                  companionsMenu(at);
                                },
                                [this, k] { teamPanelSel_ = (int)k; }};
                    ci.shrink = true;
                    p.items.push_back(ci);
                  }
                  menus.push(p);
                },
                [this, i] { teamPanelSel_ = (int)i; }};
    it.shrink = true;
    m.items.push_back(it);
  }
  m.onCancel = [this] {
    panelMode_ = 1;
    menus.pop();
  };
  for (size_t i = 0; i < team.size(); i++)
    if (team[i]->S().human && sel-- == 0) teamPanelSel_ = (int)i;
  menus.push(m);
}

// ---------------------------------------------------------------------------
// Équipement : qui, puis quel emplacement, puis quel objet du sac
// ---------------------------------------------------------------------------
void Game::gearMenu(int sel) {
  panelMode_ = 2;  // fiche détaillée à droite : les statistiques changent en direct
  Menu m;
  m.title = "Équipement";
  m.x = 8, m.y = 8, m.w = 132, m.rows = 9, m.sel = sel;
  for (size_t i = 0; i < team.size(); i++) {
    FighterP f = team[i];
    int n = 0;
    for (auto& g : f->gear) n += !g.empty();
    m.items.push_back({f->name(), n ? std::to_string(n) + "/3" : "", "Entrée : changer son équipement.", true,
                       [this, f, i] { gearSlots(f, (int)i); }, [this, i] { teamPanelSel_ = (int)i; }});
  }
  m.onCancel = [this] {
    panelMode_ = 1;
    menus.pop();
  };
  if (sel < (int)team.size()) teamPanelSel_ = sel;
  menus.push(m);
}

void Game::gearSlots(FighterP f, int who, int sel) {
  Menu m;
  m.title = f->name();
  m.x = 8, m.y = 8, m.w = 132, m.rows = 7;
  for (int s = 0; s < N_GEAR; s++) {  // chaque emplacement : son titre, puis l'objet porté
    const std::string& id = f->gear[(size_t)s];
    bool ok = s == G_ACCESSORY || f->S().human;
    m.items.push_back(menuHeader(gearSlotName(s)));
    MenuItem it{ok ? (id.empty() ? "(rien)" : item(id).name) : "(héros seulement)", "",
                ok ? (id.empty() ? "Entrée : choisir dans le sac." : item(id).bonusText() + ". Entrée : changer ou retirer.")
                   : "Seuls les héros portent armes et armures.",
                ok, [this, f, who, s] { gearPick(f, who, s); }};
    it.shrink = true;
    m.items.push_back(it);
  }
  m.sel = 1 + 2 * std::clamp(sel, 0, N_GEAR - 1);
  m.items.push_back({"Retour", "", "", true, [this] { menus.pop(); }});
  teamPanelSel_ = who;
  menus.push(m);
}

// Choix d'un objet du sac pour l'emplacement s (ou retirer celui qui est porté)
void Game::gearPick(FighterP f, int who, int s) {
  auto equip = [this, f, who, s](const std::string& id) {
    std::string& cur = f->gear[(size_t)s];
    if (!cur.empty()) items[cur]++;
    if (!id.empty()) items[id]--;
    int lostHp = f->mhp - f->hp, lostMp = f->mmp - f->mp;
    cur = id;
    f->recalc();
    f->hp = std::clamp(f->mhp - lostHp, f->alive() ? 1 : 0, f->mhp);  // les PV perdus restent perdus
    f->mp = std::clamp(f->mmp - lostMp, 0, f->mmp);
    audio::play("achat");
    menus.pop();
    menus.pop();
    gearSlots(f, who, s);
  };
  Menu m;
  m.title = gearSlotName(s);
  m.x = 8, m.y = 8, m.w = 132, m.rows = 8;
  for (auto& d : allItems()) {
    int n = items.count(d.id) ? items[d.id] : 0;
    if (d.slot != s || n <= 0 || !canEquip(*f, d)) continue;
    std::string id = d.id;
    MenuItem it{d.name, "×" + std::to_string(n), d.bonusText() + ". " + d.desc, true, [equip, id] { equip(id); }};
    it.shrink = true;
    m.items.push_back(it);
  }
  if (!f->gear[(size_t)s].empty()) m.items.push_back({"Retirer", "", "Remettre " + item(f->gear[(size_t)s]).name + " dans le sac.", true, [equip] { equip(""); }});
  if (m.items.empty()) m.items.push_back({"(rien dans le sac)", "", "Les boutiques vendent armes, armures et accessoires.", false, nullptr});
  menus.push(m);
}

// Journal des quêtes : celles qui ont commencé ; l'aide montre l'étape où l'on en est
void Game::journalMenu() {
  panelMode_ = 0;
  Menu m;
  m.title = "Journal";
  m.x = 8, m.y = 8, m.w = 220, m.rows = 9;
  for (auto& q : quests()) {
    std::string id = jget<std::string>(q, "id", "");
    if (!has(questFlag(id))) continue;
    bool done = has(questFlag(id, true));
    std::string help = jget<std::string>(q, "fin_texte", "Quête terminée.");
    if (!done) {
      Json steps = q.value("etapes", Json::array());
      for (auto& s : steps)  // la dernière étape dont la condition est vraie
        if (!s.contains("si") || checkCond(s["si"])) help = jget<std::string>(s, "texte", "");
    }
    MenuItem it{jget<std::string>(q, "nom", id), done ? "terminée" : "en cours", jget<std::string>(q, "lieu", "") + " — " + help, true, nullptr};
    it.color = done ? 0xaab3d8 : 0;
    it.shrink = true;
    m.items.push_back(it);
  }
  if (m.items.empty()) m.items.push_back({"(aucune quête)", "", "Parlez aux habitants : certains ont besoin d'aide.", false, nullptr});
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
    std::string note = d.field ? "" : d.slot >= 0 ? " (équipement : Échap > Équipement)" : d.key ? " (objet important)" : " (en combat)";
    m.items.push_back({d.name, "×" + std::to_string(n), d.desc + note, d.field,
                       [this, id, myIdx] {
                         pickMember("Sur qui ?", [id](const Fighter& f) {
                           const ItemDef& d = item(id);
                           if (d.revive) return !f.alive();
                           return f.alive() && ((d.healHp && f.hp < f.mhp) || (d.healMp && f.mp < f.mmp));
                         }, [this, id, myIdx](Fighter& f) {
                           const ItemDef& d = item(id);
                           audio::play("soin");
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
                                                  audio::play("soin");
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
                           audio::play("refus");
                           notice("Pas assez d'or.");
                           return;
                         }
                         audio::play("achat");
                         gold -= d.price;
                         items[id]++;
                         notice("Acheté : " + d.name + ".");
                         menus.pop();
                         shopMenuAt(stock, (int)i);
                       }});
  }
  m.items.push_back({"Vendre…", "", "Vendre vos objets à la moitié de leur prix.", true, [this, stock] { sellMenu(stock, 0); }});
  m.items.push_back({"Quitter", "", "", true, [this] { menus.clear(); }});
  m.sel = std::clamp(sel, 0, (int)m.items.size() - 1);
  menus.push(m);
}

// Vendre : chaque objet du sac (sauf les objets de quête) rapporte la moitié de son prix
void Game::sellMenu(const std::vector<std::string>& stock, int sel) {
  Menu m;
  m.title = "Vendre  ·  " + std::to_string(gold) + " or";
  m.x = 60, m.y = 20, m.w = 200, m.rows = 7;
  for (auto& d : allItems()) {
    auto have = items.find(d.id);
    if (have == items.end() || have->second <= 0 || d.key || d.price / 2 <= 0) continue;
    std::string id = d.id;
    int at = (int)m.items.size();
    MenuItem it{d.name, "+" + std::to_string(d.price / 2) + " or", "Vous en avez " + std::to_string(have->second) + ".", true, [this, stock, id, at] {
                  items[id]--;
                  gold += item(id).price / 2;
                  audio::play("achat");
                  notice("Vendu : " + item(id).name + ".");
                  menus.pop();
                  sellMenu(stock, at);
                }};
    it.shrink = true;
    m.items.push_back(it);
  }
  if (m.items.empty()) m.items.push_back({"(rien à vendre)", "", "", false, nullptr});
  m.sel = std::clamp(sel, 0, (int)m.items.size() - 1);
  m.onCancel = [this, stock] {  // retour à la boutique (avec l'or à jour)
    menus.pop();
    menus.pop();
    shopMenuAt(stock, (int)stock.size());
  };
  menus.push(m);
}

// ---------------------------------------------------------------------------
// Sauvegarde (fichier texte dans le dossier de l'utilisateur)
// ---------------------------------------------------------------------------
std::string Game::savePath() const { return userFile(inExpedition() ? expedition_->saveFile() : saveName_); }
bool Game::saveExists() const {
  std::ifstream f(savePath());
  return f.good();
}
bool Game::saveGame() {
  std::ostringstream f;
  f << "BRUMEVAL 1\n";
  if (inExpedition()) expedition_->writeSave(f);  // en premier : le chargement génère d'abord le monde
  f << "carte " << M().id << ' ' << px << ' ' << py << ' ' << dir << '\n';
  f << "reveil " << maps()[respawnMap].id << ' ' << respawnX << ' ' << respawnY << '\n';
  f << "or " << gold << '\n';
  for (auto& fl : flags) f << "drapeau " << fl << '\n';
  for (auto& [id, n] : items)
    if (n > 0) f << "objet " << id << ' ' << n << '\n';
  f << "auto " << tacticsAuto << '\n';
  noteBestiary();
  for (auto& sp : seen)
    if (hasSpecies(sp)) f << "vu " << sp << '\n';
  for (auto& sp : caught)
    if (hasSpecies(sp)) f << "pris " << sp << '\n';
  static const char KIND[] = {'a', 't', 'o'};  // action automatique, technique, objet
  auto member = [&](const FighterP& m) {
    f << "membre " << m->sp << ' ' << m->lvl << ' ' << m->xp << ' ' << m->hp << ' ' << m->mp << ' ' << m->lim << '\n';
    if (m->gear != decltype(m->gear){}) {
      f << "equipement";
      for (auto& g : m->gear) f << ' ' << (g.empty() ? "-" : g);
      f << '\n';
    }
    auto c = std::find(team.begin(), team.end(), m->companion.lock());
    if (c != team.end() && hasSpecies(m->sp) && m->S().human) f << "compagnon " << (c - team.begin()) << '\n';
    f << "tactiques " << m->tacticsOn << '\n';
    for (auto& t : m->tactics) f << "tactique " << t.on << ' ' << t.cond << ' ' << t.value << ' ' << KIND[(int)t.kind] << ' ' << t.act << '\n';
  };
  for (auto& m : team) member(m);
  if (!reserve.empty()) {  // la réserve après l'équipe : ses lignes « membre » suivent la ligne « reserve »
    f << "reserve\n";
    for (auto& m : reserve) member(m);
  }
  return writeUserFile(inExpedition() ? expedition_->saveFile() : saveName_, f.str());
}
// Lignes d'un membre de l'équipe : « membre » (espèce, niveau, expérience, PV, PM, Limite), puis
// « equipement », « tactiques » et ses lignes « tactique ». Une espèce inconnue ajoute nullptr :
// les lignes qui la suivent sont ignorées (à retirer ensuite).
bool readMemberLine(const std::string& k, std::istream& s, std::vector<FighterP>& team, std::vector<std::pair<size_t, int>>* links) {
  if (k == "membre") {
    std::string sp;
    int lvl = 1, xp = 0, hp = 1, mp = 0;
    float lim = 0;
    s >> sp >> lvl >> xp >> hp >> mp >> lim;
    if (!hasSpecies(sp)) {
      team.push_back(nullptr);
      return true;
    }
    auto m = makeFighter(sp, lvl);
    m->xp = xp, m->hp = hp, m->mp = mp, m->lim = lim;  // à borner une fois l'équipement lu
    team.push_back(m);
    return true;
  }
  if (k != "equipement" && k != "tactiques" && k != "tactique" && k != "compagnon") return false;
  if (team.empty() || !team.back()) return true;
  Fighter& m = *team.back();
  if (k == "compagnon") {  // rang de sa créature compagnon dans l'équipe (relié une fois tout lu)
    int i = -1;
    if (s >> i && links) links->push_back({team.size() - 1, i});
    return true;
  }
  if (k == "equipement") {
    for (auto& g : m.gear) {
      std::string id;
      s >> id;
      g = id != "-" && hasItem(id) && canEquip(m, item(id)) ? id : "";
    }
    m.recalc();
  } else if (k == "tactiques") {  // les anciennes sauvegardes gardent les tactiques de départ
    s >> m.tacticsOn;
    m.tactics.clear();
  } else {
    Tactic t;
    char kind = 'a';
    s >> t.on >> t.cond >> t.value >> kind >> t.act;
    t.kind = kind == 't' ? Tactic::Act::Move : kind == 'o' ? Tactic::Act::Item : Tactic::Act::Auto;
    if (s && findTacticCond(t.cond)) m.tactics.push_back(t);
  }
  return true;
}

bool Game::loadGame() {
  std::ifstream f(savePath());
  std::string head;
  if (!f || !std::getline(f, head) || head.rfind("BRUMEVAL", 0) != 0) return false;
  team.clear();
  reserve.clear();
  seen.clear(), caught.clear();
  bool inReserve = false;                      // après la ligne « reserve »
  std::vector<std::pair<size_t, int>> links;  // lignes « compagnon » : reliées à la fin
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
    } else if (k == "auto") s >> tacticsAuto;
    else if (k == "vu" || k == "pris") {
      s >> v;
      if (hasSpecies(v)) (k == "vu" ? seen : caught).insert(v);
    } else if (k == "reserve") inReserve = true;
    else if (inReserve) readMemberLine(k, s, reserve, nullptr);
    else readMemberLine(k, s, team, &links);
  }
  reserve.erase(std::remove(reserve.begin(), reserve.end(), nullptr), reserve.end());
  for (auto& m : reserve) m->hp = std::min(m->hp, m->mhp), m->mp = std::min(m->mp, m->mmp);
  noteBestiary();  // anciennes sauvegardes : l'équipe compte déjà
  for (auto& [h, c] : links)  // compagnons : avant de retirer les espèces disparues (les rangs changeraient)
    if (c >= 0 && c < (int)team.size() && team[h] && team[(size_t)c] && !team[(size_t)c]->S().human) team[h]->companion = team[(size_t)c];
  team.erase(std::remove(team.begin(), team.end(), nullptr), team.end());  // espèces qui n'existent plus
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
  for (auto& m : team) m->hp = std::min(m->hp, m->mhp), m->mp = std::min(m->mp, m->mmp);
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
  if (bindKey_ >= 0) drawKeyPrompt();
  if (pilot_ && pilot_->demo()) {  // bandeau de la démo, en haut de l'écran
    std::string s = "DÉMO · " + pilot_->status;
    if (s.size() > 60) s = utf8Prefix(s, 44) + "…";
    g.rect(g.left(), 0, g.fullW, 11, rgb(0x000010, 170));
    g.text(160, 2, s, rgb(0xffd34d), 1);
  }
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
  // Carte moins haute que l'écran (intérieur) : centrée, comme une carte trop étroite
  float vhi = float(m.h() * 16 - SCREEN_H);
  int camY = vhi >= 0 ? (int)std::clamp(ppy - SCREEN_H / 2 + 8, 0.f, vhi) : (int)(vhi / 2);
  int tx0 = std::max(0, (int)std::floor((camX + g.left()) / 16)), tx1 = (int)((camX + g.right()) / 16), ty0 = std::max(0, camY / 16);
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
  // Expédition à plusieurs : les autres joueurs de la région, avec leur pseudo (doré : prêt devant le gardien)
  if (online_ && online_->inGroup())
    for (auto& a : online_->avatars())
      actors.push_back({a.y * 16, [&, a] {
                          float x = a.x * 16 - camX, y = a.y * 16 - camY;
                          drawHuman(g, look(a.look), x, y - 2, 1, a.dir, a.step, false);
                          int w = Gfx::textW(a.name);
                          g.rect(x + 8 - w / 2 - 2, y - 23, w + 4, 10, rgb(0x0a0f33, 150));
                          g.text(x + 8, y - 22, a.name, a.ready ? GOLD : a.busy ? rgb(0xff8a7a) : WHITE, 1);
                        }});
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
  if (panelMode_ == 3 && menus.active()) drawBestiaryPanel(146, 8);
  else if (panelMode_ && menus.active()) drawTeamPanel(panelMode_ == 1 ? 114 : 146, 8, panelMode_ == 2 ? teamPanelSel_ : -1);
  if (menus.active()) menus.draw(g, time);
  drawDialogue();
  int noticeY = 190;
  if (menus.active() && askText_.empty() && !menus.help().empty()) {
    auto lines = Gfx::wrap(menus.help(), 300);
    int h = 10 + 11 * (int)lines.size();
    g.window(4, 236 - h, 312, h);
    for (size_t i = 0; i < lines.size(); i++) g.text(10, 236 - h + 4 + i * 11, lines[i], MUTED);
    noticeY = std::min(noticeY, 236 - h - 22);  // le petit message passe au-dessus de l'aide
  }
  if (noticeT_ > 0) {  // un message trop long passe sur deux lignes (le bas de la fenêtre ne bouge pas)
    auto lines = Gfx::wrap(notice_, 290);
    int w = 20, h = 9 + 11 * (int)lines.size(), top = noticeY + 20 - h;
    for (auto& l : lines) w = std::max(w, Gfx::textW(l) + 20);
    g.window(160 - w / 2, top, w, h);
    for (size_t i = 0; i < lines.size(); i++) g.text(160, top + 4 + i * 11, lines[i], WHITE, 1);
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
      g.text(320 - 16, ry, isFront ? "front" : "repos", isFront ? GOLD : MUTED, 2);
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
  g.ellipse(272, 36, 18, 18, rgb(0xfff4c0));  // au-dessus du texte
  g.poly({{L, 200}, {L, 170}, {0, 200}, {80, 150}, {150, 190}, {230, 140}, {320, 190}, {L + W, 160}, {L + W, 240}, {L, 240}}, rgb(0x2e6b3a));
  g.textBig(160, 30, "FIN", GOLD, 3, 1);
  auto lines = Gfx::wrap(endingText_.empty() ? "La brume se lève enfin sur Brumeval." : endingText_, 280);
  for (const char* l : {"", "Un jeu créé avec Claude", "C++ et SDL2", "", "Appuyez sur Entrée pour continuer"}) lines.push_back(l);
  for (size_t i = 0; i < lines.size(); i++) g.text(160, 74 + i * 13, lines[i], WHITE, 1);
  auto team = front();  // jusqu'à six combattants (héros et compagnons) : l'écart s'adapte
  float gap = team.size() > 1 ? std::min(100.f, 260.f / (team.size() - 1)) : 0, x = 160 - gap * (team.size() - 1) / 2;
  for (auto& f : team) {
    drawFighterSprite(g, *f, x, 205, 1, true, time);
    x += gap;
  }
}
