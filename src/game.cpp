#include "i18n.hpp"
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
  if (pilot_ && pilot_->demo()) return;  // la souris ne dérange pas la démo
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
  if (pilot_ && pilot_->demo()) {  // démo : Start met en pause ou reprend
    if (down && button == SDL_CONTROLLER_BUTTON_START) demoPaused_ = !demoPaused_;
    return;
  }
  if (textOn_ || bindKey_ >= 0) return;
  switch (button) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return padDirection(UP, down);
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return padDirection(DOWN, down);
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return padDirection(LEFT, down);
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return padDirection(RIGHT, down);
    case SDL_CONTROLLER_BUTTON_X: in.runPad = down; return;  // maintenu : courir
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
  if (pilot_ && pilot_->demo()) return;
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
  bool shortcut = k == SDL_SCANCODE_F1 || k == SDL_SCANCODE_F2 || k == SDL_SCANCODE_F11;  // aide, vitesse, plein écran
  if (pilot_ && pilot_->demo() && !shortcut) {  // démo : Espace met en pause ou reprend, Échap la quitte ; rien d'autre
    if (down && !repeat && k == SDL_SCANCODE_SPACE) demoPaused_ = !demoPaused_;
    if (down && !repeat && k == SDL_SCANCODE_ESCAPE) titleMenu();
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
  if (k == SDL_SCANCODE_F2) {
    if (down && !repeat) cycleSpeed();
    return;
  }
  if (k == SDL_SCANCODE_F1) {  // aide : ouvre la dernière page vue (ou la première), ou la ferme
    if (down && !repeat) {
      if (helpPage_ >= 0) helpPage_ = -1;
      else openHelp(std::max(0, lastHelp_));
    }
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
  g.text(160, y + h - 13, T("Entrée : valider · Échap : annuler · ") + std::to_string(utf8Len(textValue_)) + "/" + std::to_string(textMax_),
         MUTED, 1);
}

// ---------------------------------------------------------------------------
// Boucle principale
// ---------------------------------------------------------------------------
int Game::speed() const {
  if (online_ && online_->connected()) return 1;
  return std::clamp(options().speed, 1, 3);
}

void Game::cycleSpeed() {
  if (online_ && online_->connected()) return notice(T("Vitesse normale en multijoueur."));
  options().speed = options().speed % 3 + 1;
  saveOptions();
  speedT_ = 1.2f;
}

void Game::update(float dt) {
  if (mode == Mode::Map && bouldersMap_ != mapId) resetBoulders();  // après un chargement, une nouvelle partie…
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
  if (pilot_ && pilot_->demo() && demoPaused_) {  // démo en pause : tout attend
    updateMusic();
    in.endFrame();
    return;
  }
  if (pilot_) pilot_->update(dt);    // joueur automatique : il choisit les touches de cette image
  banner -= dt;
  noticeT_ -= dt;
  speedT_ -= dt / speed();  // même durée à l'écran quelle que soit la vitesse
  if (helpPage_ >= 0) {  // aide ouverte : le reste du jeu attend
    lastHelp_ = helpPage_;
    updateHelp();
    updateMusic();
    in.endFrame();
    return;
  }
  switch (mode) {
    case Mode::Title: menus.update(in); break;
    case Mode::Ending:
      if (in.confirm) {
        mode = Mode::Map;
        notice(T("Merci d'avoir joué ! Vous pouvez continuer l'exploration."));
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
        if (r == BattleResult::Win && !inExpedition()) sc.call([this] { offerEvolutions(); });
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
      if (!moving && !M().puzzleFlag.empty() && !puzzleSolved(M()) && tip("enigme", sc)) break;  // énigme (3.0)
      if (!tipsSeen.count("gemme") && !moving)  // astuce : la première gemme reçue (boss, coffre, boutique)
        for (auto& [id, n] : items)
          if (n > 0 && hasItem(id) && item(id).gem && tip("gemme", sc)) break;
      if (moving) {
        running_ = in.run();
        moveT += dt * (running_ ? 13.f : 7.5f);  // Maj ou X maintenu : on court
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
  team.push_back(f);  // à la suite : il ne prend la place de personne
  auto fr = front();
  bool fights = std::find(fr.begin(), fr.end(), f) != fr.end();
  sc.say(f->name() + (fights ? T(" rejoint l'équipe et combat à vos côtés !") : T(" rejoint l'équipe !")));
  sc.say(std::string(fights ? "(" : T("(Il attend son tour : ")) + T("Échap > Équipe pour choisir qui combat. ") + f->S().role + ".)");
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
  sc.say(T("Vous reprenez connaissance chez la guérisseuse. Toute l'équipe est soignée."));
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
  if (m.rows[y][x] == 'D' && !puzzleSolved(m)) return true;  // grille fermée
  if (&m == &M() && bouldersMap_ == mapId ? boulderAt(x, y)
                                           : (m.rows[y][x] == 'O' && !puzzleSolved(m)) || (m.rows[y][x] == 'P' && puzzleSolved(m) && !m.puzzleFlag.empty()))
    return true;  // rocher (sur la carte affichée : sa place actuelle)
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

void Game::resetBoulders() {
  const MapDef& m = M();
  boulders_ = puzzleTiles(m, puzzleSolved(m) && !m.puzzleFlag.empty() ? 'P' : 'O');
  bouldersMap_ = mapId;
}
bool Game::boulderAt(int x, int y) const { return std::find(boulders_.begin(), boulders_.end(), std::make_pair(x, y)) != boulders_.end(); }

void Game::pushBoulder(int x, int y, int dx, int dy) {
  const MapDef& m = M();
  int bx = x + dx, by = y + dy;
  bool warp = false;
  for (auto& w : m.warps) warp = warp || (w.x == bx && w.y == by);
  if (puzzleSolved(m) || blocked(bx, by) || warp) {
    audio::play("refus");
    return;
  }
  *std::find(boulders_.begin(), boulders_.end(), std::make_pair(x, y)) = {bx, by};
  audio::play("coup_lourd");
  bool all = true;
  for (auto& p : puzzleTiles(m, 'P')) all = all && boulderAt(p.first, p.second);
  if (all) {
    flags.insert(m.puzzleFlag);
    audio::play("bonus");
    sc.say(m.puzzleMsg.empty() ? T("Un grondement sourd : quelque part, une grille s'ouvre !") : fillText(m.puzzleMsg));
  }
}

void Game::tryMove(int d) {
  bool fresh = in.press[d];
  dir = d;
  int dx = d == LEFT ? -1 : d == RIGHT ? 1 : 0, dy = d == UP ? -1 : d == DOWN ? 1 : 0;
  int nx = px + dx, ny = py + dy;
  if (boulderAt(nx, ny) && !puzzleSolved(M())) {  // énigme : pousser le rocher, puis avancer à sa place
    pushBoulder(nx, ny, dx, dy);
    if (boulderAt(nx, ny)) return;
  }
  if (blocked(nx, ny)) {
    if (!fresh) return;
    for (auto& b : M().buildings)
      if (nx == b.doorX() && ny == b.doorY()) return enterDoor(b);
    for (auto& b : M().bosses)
      if (bossAlive(b) && nx >= b.x && nx <= b.x + 1 && ny >= b.y && ny <= b.y + 1) return bossTouched(b);
    for (auto& w : M().warps)
      if (w.x == nx && w.y == ny && !w.condition.is_null() && !checkCond(w.condition))
        return sc.say(w.message.empty() ? T("Le passage est fermé.") : fillText(w.message));
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
  if (tileEncounter(c)) rate *= 1 + rules().packDense * std::min(1.0, wildDepth(px, py) / std::max(1.0, rules().packDistance));
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

// Profondeur d'une case : distance à pied jusqu'au plus proche bâtiment ou passage de la carte
// (villes, entrées). Plus on s'enfonce, plus la nature est dense : rencontres plus fréquentes
// et meutes plus grandes (règles, rencontres).
int Game::wildDepth(int x, int y) const {
  const MapDef& m = M();
  int W = m.w(), H = m.h();
  if (x < 0 || y < 0 || x >= W || y >= H) return 0;
  std::vector<int> d((size_t)W * H, -1);
  std::vector<int> q;
  auto seed = [&](int sx, int sy) {
    if (sx >= 0 && sy >= 0 && sx < W && sy < H && d[(size_t)sy * W + sx] < 0) d[(size_t)sy * W + sx] = 0, q.push_back(sy * W + sx);
  };
  for (auto& b : m.buildings) seed(b.doorX(), b.doorY() + 1);
  for (auto& w : m.warps) seed(w.x, w.y);
  for (size_t k = 0; k < q.size(); k++) {
    int i = q[k], cx = i % W, cy = i / W;
    if (cx == x && cy == y) return d[(size_t)i];
    const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
    for (int s = 0; s < 4; s++) {
      int nx = cx + DX[s], ny = cy + DY[s];
      if (nx < 0 || ny < 0 || nx >= W || ny >= H || d[(size_t)ny * W + nx] >= 0 || !tileWalkable(m.rows[ny][nx])) continue;
      d[(size_t)ny * W + nx] = d[(size_t)i] + 1;
      q.push_back(ny * W + nx);
    }
  }
  return 0;
}

void Game::encounter() {
  for (auto& z : M().zones) {
    if (px < z.x || py < z.y || px >= z.x + z.w || py >= z.y + z.h) continue;
    int n = 1 + (frand() < rules().second) + (frand() < rules().third);
    n = std::min(n, z.maxN);
    // Meute : au plus profond de la nature, des créatures en plus
    float deep = std::min(1.f, wildDepth(px, py) / float(std::max(1.0, rules().packDistance)));
    for (int k = n; k < rules().packMax; k++)
      if (frand() < .5f * deep) n++;
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
  if (changed) resetBoulders();
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
  if (b.decor()) return;  // décor : pas de porte
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
  if (has(key)) return sc.say(T("Le coffre est vide."));
  flags.insert(key);
  audio::play("coffre");
  const Chest& c = M().chests[i];
  if (c.item.empty()) {
    gold += c.qty;
    sc.say(T("Vous trouvez ") + std::to_string(c.qty) + T(" pièces d'or !"));
  } else {
    items[c.item] += c.qty;
    sc.say(T("Vous trouvez : ") + item(c.item).name + (c.qty > 1 ? T(" ×") + std::to_string(c.qty) : "") + " !");
    if (c.item == "herbe") sc.say(T("Elle luit d'une douce lumière. Maëlle, à la chapelle, en avait besoin."));
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
  m.items.push_back({T("Oui"), "", "", true, [this, close, yes] {
                       close();
                       sc.runNow(yes);
                     }});
  m.items.push_back({T("Non"), "", "", true, [this, close, no] {
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
  pilot_.reset();  // fin de la démo : la partie reprend son fichier de sauvegarde habituel
  demoPaused_ = false;
  if (!saveBeforePilot_.empty()) saveName_ = saveBeforePilot_, saveBeforePilot_.clear();
  if (expedition_) expedition_->leave();  // remet les données du jeu à la place de celles de l'expédition
  if (online_) online_->leave();          // ferme les connexions du multijoueur
  mode = Mode::Title;
  editorTest_ = storyTest_ = false;
  menus.clear();
  Menu m;
  m.x = 102, m.y = 110, m.w = 116, m.rows = 8, m.cancelable = false;
  m.items.push_back({T("Nouvelle partie"), "", "", true, [this] { starterMenu(); }});
  bool can = saveExists();
  m.items.push_back({T("Continuer"), "", "", can, [this] {
                       if (loadGame()) {
                         menus.clear();
                         mode = Mode::Map;
                         showRegionBanner();
                       }
                     }});
  m.items.push_back({T("Expédition"), "", T("Des régions générées sans fin, façon roguelite."), true, [this] {
                       if (!expedition_) expedition_ = std::make_unique<Expedition>(*this);
                       expedition_->menu();
                     }});
  m.items.push_back({T("Multijoueur"), "", T("Avec des amis : duel, expédition, échanges."), true, [this] {
                       if (!online_) online_ = std::make_unique<Online>(*this);
                       online_->menu();
                     }});
  m.items.push_back({T("Outils"), "", T("Arène, réglages et éditeurs."), true, [this] { toolsMenu(); }});
  m.items.push_back({T("Options"), "", T("Volumes, plein écran, vitesse du jeu et touches."), true, [this] { optionsMenu(); }});
  m.items.push_back({T("Aide"), "", T("Statistiques, dégâts, damier, zones, états, tableau des types, touches (F1)."), true, [this] { helpMenu(); }});
  m.items.push_back({T("Quitter"), "", "", true, [this] { quit = true; }});
  if (can) m.sel = 1;
  menus.push(m);
}

void Game::toolsMenu() {
  Menu m;
  m.title = T("Outils");
  m.x = 92, m.y = 92, m.w = 136, m.rows = 6;
  m.items.push_back({T("Arène de combat"), "", T("Composer deux équipes, combattre ou simuler des combats."), true, [this] {
                       if (!arena_) arena_ = std::make_unique<Arena>(*this);
                       mode = Mode::Arena;
                       arena_->open();
                     }});
  m.items.push_back({T("Réglages"), "", translating() ? T("Les éditeurs sont en français : Options > Langue > Français.") : T("Modifier les règles, espèces, techniques, types et objets."), !translating(), [this] {
                       if (!settings_) settings_ = std::make_unique<Settings>(*this);
                       mode = Mode::Settings;
                       settings_->open();
                     }});
  m.items.push_back({T("Éditeur de cartes"), "", translating() ? T("Les éditeurs sont en français : Options > Langue > Français.") : T("Peindre les cartes, placer habitants, coffres, passages, zones… et tester."), !translating(), [this] {
                       if (!editor_) editor_ = std::make_unique<MapEditor>(*this);
                       mode = Mode::Editor;
                       editor_->open();
                     }});
  m.items.push_back({T("Éditeur d'histoire"), "", translating() ? T("Les éditeurs sont en français : Options > Langue > Français.") : T("Dialogues et événements : conditions, questions, combats, récompenses… et les jouer."), !translating(), [this] {
                       if (!story_) story_ = std::make_unique<StoryEditor>(*this);
                       mode = Mode::Story;
                       story_->open();
                     }});
  m.items.push_back({T("Démo"), "", T("Le jeu joue tout seul une nouvelle partie. Espace : pause, Échap : quitter."), true,
                     [this] { startPilot(true); }});
  m.items.push_back({T("Retour"), "", "", true, [this] { menus.pop(); }});
  menus.push(m);
}

void Game::starterMenu() {
  Menu m;
  m.title = T("Premier compagnon");
  m.x = 90, m.y = 132, m.w = 140, m.rows = 3;
  for (const std::string& id : rules().starters) {
    const Species& s = species(id);
    std::string lim = s.limit.empty() ? "aucune" : moveInfo(s.limit).name;
    m.items.push_back({s.name, typesName(s), T("Type ") + typesName(s) + T(" · Limite : ") + lim, true,
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
  seen.clear(), caught.clear(), weak.clear(), tipsSeen.clear();
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
  if (!evoTo_.empty()) drawEvolution();
  if (helpPage_ >= 0) drawHelp();
  if (textOn_) drawTextEdit();
  if (bindKey_ >= 0) drawKeyPrompt();
  if (int v = speed(); v > 1 || speedT_ > 0) {  // vitesse : en grand juste après F2, puis discrète dans le coin
    std::string s = T("×") + std::to_string(v);
    if (speedT_ > 0) {
      g.newLayer();  // par-dessus les menus éventuels
      g.window(160 - 46, 30, 92, 26);
      g.text(160, 36, T("Vitesse ") + s, GOLD, 1);
    } else {
      int y = pilot_ && pilot_->demo() ? 12 : 1;  // sous le bandeau de la démo
      g.rect(g.right() - 20, y, 18, 10, rgb(0x000010, 140));
      g.text(g.right() - 11, y + 1, s, GOLD, 1, false);
    }
  }
  if (pilot_ && pilot_->demo()) {  // bandeau de la démo, en haut de l'écran ; en pause, les commandes
    std::string s = T("DÉMO · ") + pilot_->status;
    if (s.size() > 60) s = utf8Prefix(s, 44) + "…";
    g.rect(g.left(), 0, g.fullW, 11, rgb(0x000010, 170));
    g.text(160, 2, s, rgb(0xffd34d), 1);
    if (demoPaused_) {
      g.rect(g.left(), 11, g.fullW, SCREEN_H - 11, rgb(0x000010, 110));
      g.newLayer();
      g.window(70, 96, 180, 50);
      g.text(160, 102, T("Démo en pause"), GOLD, 1);
      g.text(160, 116, T("Espace : reprendre"), WHITE, 1);
      g.text(160, 128, T("Échap : quitter · F2 : vitesse"), MUTED, 1);
    }
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
  // Étoiles qui scintillent, étoile filante, lucioles (2.7)
  for (int i = 0; i < 40 * W / SCREEN_W; i++) {
    uint32_t h = uint32_t(i) * 2654435761u;
    float x = L + float(h % 1000) / 1000.f * W, y = float((h >> 10) % 70);
    float tw = .5f + .5f * std::sin(time * (1.5f + (h % 7) * .4f) + i);
    g.rect(x, y, 1, 1, rgb(0xffffff, uint8_t(60 + 150 * tw)));
  }
  float shoot = std::fmod(time, 7.f);
  if (shoot < .8f) {
    float k = shoot / .8f, sx = L + W * (.15f + .6f * k), sy = 10 + 40 * k;
    g.line(sx, sy, sx - 18, sy - 12, 1, rgb(0xffffff, uint8_t(220 * (1 - k))));
  }
  for (int i = 0; i < 10 * W / SCREEN_W; i++) {
    float x = L + std::fmod(i * 61.f + std::sin(time * .7f + i) * 20 + 40, W), y = 150 + std::fmod(i * 23.f + std::cos(time * .5f + i * 2) * 10, 60.f);
    g.ellipse(x, y, 1.5f, 1.5f, rgb(0xe8ff9a, uint8_t(90 + 80 * std::sin(time * 3 + i))));
  }
  g.textBig(160, 26, T("BRUMEVAL"), GOLD, 4, 1);
  {  // reflet qui passe sur le titre
    float k = std::fmod(time, 4.f) / 1.2f;
    if (k < 1) {
      float x = 70 + 180 * k;
      g.poly({{x, 22}, {x + 6, 22}, {x - 4, 58}, {x - 10, 58}}, rgb(0xffffff, uint8_t(110 * std::sin(k * 3.14159f))));
    }
  }
  g.text(160, 76, T("Les gardiens de la vallée"), rgb(0xc9cbe0), 1);
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
    g.text(g.left() + 4, 227, T("F1 : aide · F2 : vitesse · F11 : plein écran"), rgb(0x8a92b8), 0, false);
    g.text(g.right() - 4, 227, "v" BRUMEVAL_VERSION, rgb(0x8a92b8), 2, false);
  }
}

float Game::nightAmount() const {
  if (!outdoors()) return 0;
  float p = std::fmod(time / 720.f + .85f, 1.f);  // 0 : midi ; la partie commence en matinée
  float c = std::cos(p * 6.2831853f);
  return std::clamp((-c - .15f) / .6f, 0.f, 1.f);
}
bool Game::outdoors() const {
  if (mode != Mode::Map && mode != Mode::Battle) return false;
  const MapDef& m = M();
  return m.theme != Theme::Interieur && m.theme != Theme::Grotte && m.ambiance != "obscurite";
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
  // Énigme (3.0) : grilles (fermées : barreaux), plaques allumées, rochers
  if (!m.puzzleFlag.empty()) {
    bool open = puzzleSolved(m);
    for (auto& [x, y] : puzzleTiles(m, 'D')) {
      float sx = x * 16 - camX, sy = y * 16 - camY;
      if (open) g.rect(sx + 1, sy + 13, 14, 2, rgb(0x5a5048, 120));
      else {
        g.rect(sx, sy, 16, 16, rgb(0x3a3430));
        for (int i = 0; i < 4; i++) g.rect(sx + 1 + i * 4, sy, 2, 16, rgb(0x8a8078));
        g.rect(sx, sy + 3, 16, 2, rgb(0x6a6058));
      }
    }
    for (auto& [x, y] : puzzleTiles(m, 'P'))
      if (boulderAt(x, y)) g.rect(x * 16 - camX + 2, y * 16 - camY + 13, 12, 2, rgb(0xffd34d, 200));
    for (auto& [x, y] : boulders_) {
      float sx = x * 16 - camX, sy = y * 16 - camY;
      g.ellipse(sx + 8, sy + 13, 7, 3, rgb(0x000000, 60));
      g.ellipse(sx + 8, sy + 8, 7, 7, rgb(0x6e6a72));
      g.ellipse(sx + 7, sy + 6, 5, 4, rgb(0x8e8a94));
      g.rect(sx + 9, sy + 9, 3, 1, rgb(0x504c56));
    }
  }
  for (size_t i = 0; i < m.chests.size(); i++)
    drawChest(g, m.chests[i].x * 16 - camX, m.chests[i].y * 16 - camY, has(chestFlag((int)i)));
  for (auto& s : m.signs) drawSign(g, s.x * 16 - camX, s.y * 16 - camY);
  // Personnages, bâtiments et objets hauts (arbres, réverbères…) triés de haut en bas : on passe derrière un toit
  // ou un arbre plus bas que soi
  struct Actor {
    float y;
    std::function<void()> draw;
  };
  std::vector<Actor> actors;
  for (auto& b : m.buildings)
    actors.push_back({float((b.y + b.h) * 16 - 8), [&, b] {
                        drawBuilding(g, b, b.x * 16 - camX, b.y * 16 - camY);
                        if (m.theme != Theme::Interieur) drawChimneySmoke(g, b, b.x * 16 - camX, b.y * 16 - camY, time);
                      }});
  for (int y = ty0; y <= ty0 + SCREEN_H / 16 + 3 && y < m.h(); y++)  // + 3 : un arbre plus bas dépasse vers le haut
    for (int x = std::max(0, tx0 - 1); x <= tx1 + 1 && x < m.w(); x++)
      if (hasProp(m, x, y)) actors.push_back({float(y * 16 + 1), [&, x, y] { drawProp(g, m, x, y, x * 16 - camX, y * 16 - camY, time); }});
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
  bool run = moving && running_;
  // Le joueur a l'apparence de Lior ; en Expédition, celle du premier personnage de l'équipe (le héros choisi)
  int playerLook = 0;
  if (inExpedition())
    for (auto& f : team)
      if (f->S().human) {
        playerLook = f->S().look;
        break;
      }
  actors.push_back({ppy, [&] {
                       drawHuman(g, look(playerLook), ppx - camX, ppy - camY - 2, 1, dir, step, false, run);
                       if (!moving) tallGrass(g, m, px, py, px * 16 - camX, py * 16 - camY, time, true);  // pieds dans l'herbe
                     }});
  std::sort(actors.begin(), actors.end(), [](const Actor& a, const Actor& b) { return a.y < b.y; });
  for (auto& a : actors) a.draw();
  // Jour et nuit : la carte s'assombrit (crépuscule orangé), les fenêtres s'allument, le héros porte une lanterne
  float night = nightAmount();
  if (night > 0) {
    g.rect(g.left(), 0, g.fullW, SCREEN_H, rgb(0x0a1440, uint8_t(155 * night)));
    float dusk = night * (1 - night) * 4;
    if (dusk > 0) g.rect(g.left(), 0, g.fullW, SCREEN_H, rgb(0xff8a3a, uint8_t(30 * dusk)));
    for (auto& b : m.buildings) drawBuildingLights(g, b, b.x * 16 - camX, b.y * 16 - camY, night);
    float lx = ppx - camX + 8, ly = ppy - camY + 6;
    g.ellipse(lx, ly, 46, 34, rgb(0xffe8b0, uint8_t(14 * night)));
    g.ellipse(lx, ly, 26, 20, rgb(0xffe8b0, uint8_t(22 * night)));
    if (m.ambiance.empty() && (m.theme == Theme::Vallee || m.theme == Theme::Foret))  // lucioles la nuit
      for (int i = 0; i < 12 * g.fullW / SCREEN_W; i++) {
        float x = g.left() + std::fmod(i * 61.f + std::sin(time * .7f + i) * 20 + 40, (float)g.fullW), y = std::fmod(i * 37.f + std::cos(time * .5f + i * 2) * 14, 240.f);
        g.ellipse(x, y, 1.5f, 1.5f, rgb(0xe8ff9a, uint8_t(night * (90 + 80 * std::sin(time * 3 + i)))));
      }
  }
  // Feuilles qui tombent en forêt, papillons dans la vallée le jour
  if (m.ambiance.empty() && m.theme == Theme::Foret)
    for (int i = 0; i < 10 * g.fullW / SCREEN_W; i++) {
      float x = g.left() + std::fmod(i * 71.f + time * (9 + i % 4) + std::sin(time * 1.5f + i) * 10, g.fullW + 10.f) - 5;
      float y = std::fmod(i * 43.f + time * (12 + i % 5), 250.f) - 5, w = std::sin(time * 4 + i) * 2;
      g.poly({{x - 2, y}, {x, y - 1 + w * .3f}, {x + 2, y}, {x, y + 1}}, rgb(i % 3 ? 0xc9a13a : 0x9a6a2a, 200));
    }
  if (m.ambiance.empty() && m.theme == Theme::Vallee && night < .5f)
    for (int i = 0; i < 4 * g.fullW / SCREEN_W; i++) {
      float x = g.left() + std::fmod(i * 97.f + std::sin(time * .4f + i * 3) * 40 + time * 6, (float)g.fullW);
      float y = 40 + std::fmod(i * 53.f + std::cos(time * .6f + i) * 20, 160.f), f = std::abs(std::sin(time * 9 + i)) * 2.5f;
      Color c = rgb(i % 2 ? 0xffffff : 0xffe066, uint8_t(220 * (1 - night * 2)));
      g.ellipse(x - f / 2 - 1, y, f / 2 + .5f, 1.5f, c), g.ellipse(x + f / 2 + 1, y, f / 2 + .5f, 1.5f, c);
    }
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
  else if (panelMode_ == 4 && menus.active()) drawFormationPanel(146, 8);
  else if (panelMode_ && menus.active()) drawTeamPanel(panelMode_ == 1 ? 114 : 146, 8, panelMode_ == 2 ? teamPanelSel_ : -1);
  if (menus.active()) menus.draw(g, time);
  drawDialogue();
  int noticeY = 190;
  if (menus.active() && askText_.empty() && !menus.help().empty()) {
    auto lines = Gfx::wrap(menus.help(), 300);
    if (lines.size() > 3) {  // trois lignes au plus : la fenêtre ne monte pas sur les panneaux (textes anglais plus longs)
      lines.resize(3);
      lines.back() = utf8Prefix(lines.back(), 49) + "…";
    }
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

// Portrait de celui qui parle (2.7) : « Maëlle : … » ; son apparence par son nom, ses autres noms
// (« aussi » dans apparences.json) ou le début de son nom (« Kael (ligoté) »). nullptr : pas de portrait
static const Look* speakerLook(const std::string& text) {
  auto c = text.find(": ");  // « Maëlle : … » en français, « Maëlle: … » en anglais
  if (c == std::string::npos || c > 40) return nullptr;
  std::string who = text.substr(0, c);
  while (!who.empty() && who.back() == ' ') who.pop_back();
  for (auto& l : allLooks()) {
    if (l.name == who) return &l;
    for (auto& a : l.aliases)
      if (a == who) return &l;
  }
  for (auto& l : allLooks())
    if (who.rfind(l.name + " ", 0) == 0) return &l;
  return nullptr;
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
  if (const Look* lk = askText_.empty() && mode != Mode::Battle ? speakerLook(sc.text()) : nullptr) {  // portrait au-dessus de la fenêtre
    g.window(4, y - 50, 44, 48);
    g.rect(8, y - 46, 36, 40, rgb(0x1a2058));
    if (const SpriteRect* face = lk->sprite.empty() ? nullptr : findSprite(lk->sprite + "_visage_0")) {
      // Visage dessiné (Lior) : l'expression suit la réplique (joie, surprise, gêne)
      const std::string& s = sc.text();
      auto has = [&](const char* w) { return s.find(w) != std::string::npos; };
      int e = has("…") || has("...") ? 4 : has("?!") || has("Quoi") || has("What") ? 2 : has("Merci") || has("Thank") || has("Génial") || has("Super") ? 1 : 0;
      if (const SpriteRect* f = findSprite(lk->sprite + "_visage_" + std::to_string(e))) face = f;
      g.sprite(*face, 26 - face->w / 2, y - 6 - face->h);
    } else drawHuman(g, *lk, 10, y - 44, 2, DOWN, 0, false);
  }
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

void Game::drawEnding() {
  float L = g.left(), W = (float)g.fullW;
  g.gradV(L, 0, W, SCREEN_H, rgb(0x0e1550), rgb(0x3a6fb0));
  g.ellipse(272, 36, 18, 18, rgb(0xfff4c0));  // au-dessus du texte
  g.poly({{L, 200}, {L, 170}, {0, 200}, {80, 150}, {150, 190}, {230, 140}, {320, 190}, {L + W, 160}, {L + W, 240}, {L, 240}}, rgb(0x2e6b3a));
  g.textBig(160, 30, T("FIN"), GOLD, 3, 1);
  auto lines = Gfx::wrap(endingText_.empty() ? T("La brume se lève enfin sur Brumeval.") : endingText_, 280);
  for (const char* l : {"", T("Un jeu créé avec Claude"), T("C++ et SDL2"), "", T("Appuyez sur Entrée pour continuer")}) lines.push_back(l);
  for (size_t i = 0; i < lines.size(); i++) g.text(160, 74 + i * 13, lines[i], WHITE, 1);
  auto team = front();  // jusqu'à six combattants (héros et compagnons) : l'écart s'adapte
  float gap = team.size() > 1 ? std::min(100.f, 260.f / (team.size() - 1)) : 0, x = 160 - gap * (team.size() - 1) / 2;
  for (auto& f : team) {
    drawFighterSprite(g, *f, x, 205, 1, true, time);
    x += gap;
  }
}
