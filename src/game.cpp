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
#include "primes.hpp"
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
  if (button == SDL_CONTROLLER_BUTTON_X) in.runPad = down;  // courir : suivi même pendant une saisie (relâché = arrêté)
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
    case SDL_CONTROLLER_BUTTON_X: return;  // maintenu : courir (plus haut)
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
  if (mode == Mode::Map && !sc.busy() && !menus.active() && !fishing_.on && !cards_.on && (talkCheckT_ -= dt) < 0) {  // une nouvelle discussion ?
    talkCheckT_ = 1;
    checkDiscussions();
    checkExploits();
  }
  speedT_ -= dt / speed();  // même durée à l'écran quelle que soit la vitesse
  if (worldMapOn_) {  // carte du monde : flèches pour choisir où aller (voyage rapide), Entrée pour y aller, Échap pour fermer
    auto targets = travelTargets();
    if (!targets.empty() && (in.press[UP] || in.press[DOWN] || in.press[LEFT] || in.press[RIGHT])) {
      auto it = std::find(targets.begin(), targets.end(), worldSel_);
      int k = it == targets.end() ? -1 : int(it - targets.begin());
      k = (in.press[UP] || in.press[LEFT]) ? (k <= 0 ? (int)targets.size() - 1 : k - 1) : (k + 1) % (int)targets.size();
      worldSel_ = targets[(size_t)k];
      audio::play("curseur");
    }
    bool go = in.confirm && std::find(targets.begin(), targets.end(), worldSel_) != targets.end();
    if (go) {  // devant la maison de soin du village choisi
      worldMapOn_ = false;
      menus.clear();
      panelMode_ = 0;
      for (auto& b : maps()[(size_t)worldSel_].buildings)
        if (b.kind == "soin") {
          audio::play("valider");
          changeMap(worldSel_, b.doorX(), b.doorY() + 1, DOWN);
          autoSave();
          break;
        }
      worldSel_ = -1;
    } else if (in.confirm || in.cancel || in.menu || in.mclick[0] || in.mclick[2])
      worldMapOn_ = false, worldSel_ = -1, audio::play("retour");
    updateMusic();
    in.endFrame();
    return;
  }
  if (helpPage_ >= 0) {  // aide ouverte : le reste du jeu attend
    lastHelp_ = helpPage_;
    updateHelp();
    updateMusic();
    in.endFrame();
    return;
  }
  if (cards_.on && mode == Mode::Map && !sc.busy() && !menus.active()) {  // cartes des gardiens : le reste du jeu attend
    updateCards(dt);
    updateMusic();
    in.endFrame();
    return;
  }
  if (fishing_.on && mode == Mode::Map && !sc.busy() && !menus.active()) {  // pêche : le reste du jeu attend
    updateFishing(dt);
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
        bool training = battle_->training_;
        battle_.reset();
        mode = Mode::Map;
        menus.clear();
        steps = 0;
        auto after = afterBattle_;
        afterBattle_ = nullptr;
        if (training && trainingHp_.size() == team.size())
          for (size_t i = 0; i < team.size(); i++) team[i]->hp = trainingHp_[i].first, team[i]->mp = trainingHp_[i].second;
        trainingHp_.clear();
        if (r == BattleResult::Lose && !training) {
          sc.clear();  // la défaite interrompt l'événement en cours
          defeat();
        }
        groupBattle_ = false;
        if (after) sc.runNow([&] { after(r); });
        if (r == BattleResult::Win && !inExpedition() && !training) sc.call([this] { offerEvolutions(); });
      }
      break;
    }
    case Mode::Map: {
      if (menus.active()) {
        if (formationPick_ && panelMode_ == 4 && in.mouseOn) {  // Formation : la souris sur le dessin du plateau
          int c = formationCellAt(in.mx, in.my);
          if (c >= 0) {
            int idx = (BACK_COL - cellCol(c)) * GRID + cellRow(c);
            Menu& fm = menus.top();
            if (in.moved && idx < (int)fm.items.size()) fm.sel = idx, formationCell_ = c, fm.top = std::clamp(fm.top, std::max(0, idx - fm.rows + 1), idx);
            if (in.mclick[0] && idx < (int)fm.items.size()) in.mclick[0] = false, fm.sel = idx, in.confirm = true;
          }
        }
        if (panelMode_ == 3 && seen.count(bestiarySel_) &&
            (in.press[LEFT] || in.press[RIGHT] || (in.mouseOn && in.mclick[0] && in.mx >= 154))) {  // Bestiaire : l'autre page de la fiche
          bestiaryPage_ ^= 1;
          in.press[LEFT] = in.press[RIGHT] = in.mclick[0] = false;
          audio::play("curseur");
        }
        menus.update(in);
        if (!menus.active()) formationPick_ = false;
        if (!menus.active()) panelMode_ = 0;
        break;
      }
      if (sc.busy()) {
        sc.update(dt, in);
        break;
      }
      updateWalkers(dt);
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
  if (mode == Mode::Battle && battle_) {
    t = battle_->ending_ ? "" : !battle_->music.empty() ? battle_->music : battle_->boss ? "boss" : "combat";
    if (t == "combat" && mapId >= 0 && mapId < (int)maps().size()) {  // « lieux » : « combat:<carte> » change la musique des combats
      std::string c = audio::musicForPlace("combat:" + M().id);
      if (!c.empty()) t = c;
    }
  }
  else if (mode == Mode::Map && mapId >= 0 && mapId < (int)maps().size()) {
    t = audio::musicForPlace(M().id);  // « lieux » : une carte peut avoir son morceau (port, tour…), sinon celui de son thème
    if (t.empty()) t = audio::musicForPlace(themeName(M().theme));
    if (cards_.on) t = "cartes";  // une partie de Cartes des gardiens : l'air de taverne
  }
  audio::music(t);
  // Bruit de pluie : l'averse de la carte, ou en combat la météo du combat (elle ne change plus jusqu'à la fin)
  audio::ambience(mode == Mode::Battle && battle_ ? (battle_->raining() ? 1.f : 0.f) : mode == Mode::Map ? rainAmount() : 0.f);
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
  flags.insert(id);
  if (hasSpecies(id) && species(id).human) {  // un héros déjà là (Nouvelle partie +) ne revient pas en double
    for (auto& m : team)
      if (m->sp == id) return (void)sc.say(m->name() + T(" est déjà dans l'équipe."));
    for (auto& m : reserve)
      if (m->sp == id) return (void)sc.say(m->name() + T(" est déjà dans l'équipe."));
  }
  auto f = makeFighter(id, std::max(minLvl, avgLevel()));
  team.push_back(f);  // à la suite : il ne prend la place de personne
  auto fr = front();
  bool fights = std::find(fr.begin(), fr.end(), f) != fr.end();
  sc.say(f->name() + (fights ? T(" rejoint l'équipe et combat à vos côtés !") : T(" rejoint l'équipe !")));
  sc.say(std::string(fights ? "(" : T("(Il attend son tour : ")) + T("Échap > Équipe pour choisir qui combat. ") + f->S().role + ".)");
}

void Game::startBattle(BattleSetup setup, std::function<void(BattleResult)> after) {
  fishing_.on = false;  // une pêche en cours s'arrête (sans rien perdre)
  for (auto& f : setup.foes) see(*f);  // bestiaire (les renforts : quand ils entrent, battle.cpp)
  afterBattle_ = std::move(after);
  arenaBattle_ = mode == Mode::Arena;
  trainingHp_.clear();
  if (setup.training)  // entraînement : l'équipe retrouvera ses PV et PM, quoi qu'il arrive
    for (auto& f : team) trainingHp_.push_back({f->hp, f->mp});
  Theme th = setup.theme >= 0 ? Theme(setup.theme) : M().theme;
  if (setup.theme < 0 && mode == Mode::Map && rainAmount() > 0) setup.rain = true;
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
bool Game::npcVisible(const Npc& n) const { return (n.hideIf.empty() || !has(n.hideIf)) && (n.cond.is_null() || checkCond(n.cond)); }
bool Game::chestShown(const MapDef& m, const Chest& c) const {
  return c.cond.is_null() || has("coffre:" + m.id + ":" + std::to_string(c.x) + ":" + std::to_string(c.y)) || checkCond(c.cond);
}
bool Game::bossAlive(const BossSpot& b) const { return !has(b.flag) && (b.cond.is_null() || checkCond(b.cond)); }

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
  for (size_t i = 0; i < m.npcs.size(); i++) {
    if (!npcVisible(m.npcs[i])) continue;
    if (npcCell(m, i) == std::make_pair(x, y)) return true;
    if (&m == &M() && walkMap_ == mapId && i < walk_.size() && walk_[i].t < 1 && walk_[i].fx == x && walk_[i].fy == y) return true;
  }
  for (auto& c : m.chests)
    if (c.x == x && c.y == y && chestShown(m, c)) return true;
  for (auto& s : m.signs)
    if (s.x == x && s.y == y) return true;
  for (auto& b : m.bosses)
    if (bossAlive(b) && x >= b.x && x <= b.x + 1 && y >= b.y && y <= b.y + 1) return true;
  for (auto& w : m.warps)  // passage fermé : on ne peut pas y marcher
    if (w.x == x && w.y == y && !w.condition.is_null() && !checkCond(w.condition)) return true;
  return false;
}

// ---------------------------------------------------------------------------
// Promeneurs (3.3) : les habitants avec « promenade » font un pas de temps en temps, sans s'éloigner de leur place
// ---------------------------------------------------------------------------
std::pair<int, int> Game::npcCell(const MapDef& m, size_t i) const {
  if (&m == &M() && walkMap_ == mapId && i < walk_.size() && walk_[i].hx == m.npcs[i].x && walk_[i].hy == m.npcs[i].y)
    return {walk_[i].x, walk_[i].y};
  return {m.npcs[i].x, m.npcs[i].y};
}
void Game::syncWalkers() {
  const MapDef& m = M();
  if (walkMap_ != mapId || walk_.size() != m.npcs.size()) walk_.assign(m.npcs.size(), NpcWalk{}), walkMap_ = mapId;
  for (size_t i = 0; i < m.npcs.size(); i++) {
    NpcWalk& w = walk_[i];
    const Npc& n = m.npcs[i];
    if (w.hx == n.x && w.hy == n.y) continue;
    w = NpcWalk{};
    w.hx = w.x = w.fx = n.x, w.hy = w.y = w.fy = n.y, w.dir = n.dir;
    w.side = n.dir == LEFT || n.dir == RIGHT ? n.dir : (n.x + n.y) % 2 ? LEFT : RIGHT;
    w.wait = 1 + float((n.x * 7 + n.y * 13) % 30) / 10;
  }
}
void Game::updateWalkers(float dt) {
  syncWalkers();
  if (talkNpc_ >= 0 && talkNpc_ < (int)walk_.size()) walk_[talkNpc_].faceUntil = time + 1.5f;  // la conversation est finie
  talkNpc_ = -1;
  const MapDef& m = M();
  auto rnd = [this](int k) {
    walkSeed_ = walkSeed_ * 1664525u + 1013904223u;
    return int((walkSeed_ >> 8) % (uint32_t)k);
  };
  for (size_t i = 0; i < m.npcs.size(); i++) {
    const Npc& n = m.npcs[i];
    NpcWalk& w = walk_[i];
    if (n.roam <= 0 || n.sight > 0 || !npcVisible(n)) continue;  // un dresseur garde sa place (son regard compte)
    if (w.t < 1) {  // un pas lent : un tiers de seconde par case
      w.t = std::min(1.f, w.t + dt * 3);
      continue;
    }
    if (time < w.faceUntil || (w.wait -= dt) > 0) continue;  // on lui parle, ou il se repose
    w.wait = 1.2f + rnd(25) / 10.f;
    int d = rnd(4), nx = w.x + (d == LEFT ? -1 : d == RIGHT ? 1 : 0), ny = w.y + (d == UP ? -1 : d == DOWN ? 1 : 0);
    w.dir = d;
    if (d == LEFT || d == RIGHT) w.side = d;
    if (std::abs(nx - n.x) > n.roam || std::abs(ny - n.y) > n.roam) continue;  // trop loin de sa place : il regarde seulement
    bool bad = blocked(nx, ny) || (nx == px && ny == py) || (moving && nx == fromX && ny == fromY);
    for (auto& wp : m.warps) bad = bad || (wp.x == nx && wp.y == ny);
    for (auto& b : m.buildings) bad = bad || (nx == b.doorX() && ny == b.doorY() + 1);  // pas devant une porte
    if (bad) continue;
    w.fx = w.x, w.fy = w.y, w.x = nx, w.y = ny, w.t = 0;
  }
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
  trail_.push_back({px, py});  // le compagnon suivra par ici
  if (trail_.size() > 3) trail_.erase(trail_.begin());
  if (M().theme == Theme::Neige && (M().rows[py][px] == '.' || M().rows[py][px] == '=')) {  // une empreinte dans la neige
    footprints_.push_back({px, py, d, time});
    if (footprints_.size() > 40) footprints_.erase(footprints_.begin());
  }
  if (in.run() && tileWalkable(M().rows[py][px]) && M().rows[py][px] != 'B') puffs_.push_back({px * 16 + 8.f, py * 16 + 14.f, time, 1});  // on court
  if (puffs_.size() > 24) puffs_.erase(puffs_.begin());
  px = nx, py = ny;
  moving = true;
  moveT = 0;
}

void Game::arrive() {
  steps++;
  pensionStep();
  if (inExpedition() && expedition_->atExit(px, py)) return expedition_->nextRegion();
  for (auto& w : M().warps)
    if (w.x == px && w.y == py && !(w.map == M().id && w.tx == px && w.ty == py)) {  // vers sa propre case : une barrière
      int m = mapIndex(w.map);
      if (m >= 0) {
        changeMap(m, w.tx, w.ty, w.dir);
        return autoSave();
      }
    }
  for (auto& t : M().triggers)
    if (px >= t.x && py >= t.y && px < t.x + t.w && py < t.y + t.h && (t.until.empty() || !has(t.until))) {
      steps = 0;
      return runEvent(t.event);
    }
  if (checkSight()) return;
  char c = M().rows[py][px];
  if (tileEncounter(c)) puffs_.push_back({px * 16 + 8.f, py * 16 + 10.f, time, 0});  // les herbes s'écartent
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
    // Meute : au plus profond de la nature, des créatures en plus ; (2026-10-24) jamais plus de 2 + une par tranche de
    // meute_par_niveau niveaux de l'équipe : les premières heures se jouent à deux ou trois contre deux (chaque partie
    // automatique perdait dans ses dix premières minutes contre trois créatures)
    float deep = std::min(1.f, wildDepth(px, py) / float(std::max(1.0, rules().packDistance)));
    auto fr = front();
    int avg = 0;
    for (auto& f : fr) avg += f->lvl;
    avg = fr.empty() ? 0 : avg / (int)fr.size();
    int most = z.packCap > 0 ? z.packCap : rules().packMax;
    if (!inExpedition() && rules().packPerLevels > 0 && !fr.empty()) most = std::min(most, 2 + avg / rules().packPerLevels);
    n = std::min(n, std::max(1, most));
    for (int k = n; k < most; k++)
      if (frand() < .5f * deep) n++;
    // (2026-10-23) à la mesure de l'équipe : une équipe de six ne croise plus deux créatures seules
    if (!inExpedition() && rules().perAlly > 0) n = std::clamp((int)std::lround(fr.size() * rules().perAlly), n, std::max(n, most));
    int floorLvl = -1;  // les créatures sauvages ne restent pas trop loin sous l'équipe
    if (!inExpedition() && rules().levelGap > 0 && !fr.empty()) floorLvl = std::min(avg - rules().levelGap, z.hi + rules().levelCap);
    std::vector<FighterP> foes;
    bool night = !z.night.empty() && nightAmount() > .5f && frand() < rules().nightShare;  // la nuit, ses créatures à elle
    const auto& pool = night ? z.night : z.pool;
    for (int i = 0; i < n; i++) {
      int lv = irand(z.lo, z.hi);
      foes.push_back(makeFighter(pool[irand(0, (int)pool.size() - 1)], std::max(lv, floorLvl) + foeLevelBonus));
      if (floorLvl > lv) foes.back()->rewardLvl = lv + foeLevelBonus;
      if (!inExpedition() && frand() < rules().chroma) foes.back()->chroma = true;  // couleurs rares
    }
    if (n >= 2 && !inExpedition() && frand() < rules().alpha) {  // une meute menée par un alpha : plus fort, traité comme un boss
      bool chroma = foes[0]->chroma;
      int reward = foes[0]->rewardLvl;  // relevée : ses gains restent ceux de sa zone (plus les niveaux de l'alpha)
      foes[0] = makeFighter(foes[0]->sp, foes[0]->lvl + rules().alphaLevels);
      foes[0]->boss = true, foes[0]->chroma = chroma;
      foes[0]->rewardLvl = reward > 0 ? reward + rules().alphaLevels : 0;
    }
    BattleSetup s;
    s.foes = foes;
    s.ambush = !inExpedition() && n >= 2 && frand() < rules().ambush;  // (2026-10-23) certaines surgissent derrière l'équipe
    startBattle(std::move(s), nullptr);
    return;
  }
}

// Gardien d'une expédition à plusieurs : il ne s'affronte qu'avec tout le groupe (coop.cpp)
void Game::bossTouched(const BossSpot& b) {
  if (online_ && online_->inGroup() && inExpedition() && b.flag == expedition_->current().bossFlag) return online_->guardian(b.event);
  runEvent(b.event);
}

void Game::changeMap(int m, int x, int y, int d) {
  fishing_.on = false;
  trail_.clear();  // le compagnon réapparaît au premier pas
  footprints_.clear();
  puffs_.clear();
  talkNpc_ = -1;
  mapFadeT_ = time;  // fondu depuis le noir
  bool changed = m != mapId;
  mapId = m;
  if (changed) resetBoulders();
  if (!inExpedition() && M().worldX >= 0) visited.insert(M().id);  // voyage rapide
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
  syncWalkers();
  for (size_t i = 0; i < m.npcs.size(); i++)
    if (npcCell(m, i) == std::make_pair(fx, fy) && npcVisible(m.npcs[i])) {
      if (m.npcs[i].sight == 0) {  // il se tourne vers le joueur (un dresseur garde son regard)
        walk_[i].dir = dir == UP ? DOWN : dir == DOWN ? UP : dir == LEFT ? RIGHT : LEFT;
        if (dir == LEFT || dir == RIGHT) walk_[i].side = walk_[i].dir;
        walk_[i].faceUntil = time + 2.5f;
        walk_[i].talkT = time;
        talkNpc_ = (int)i;
      }
      return talk(m.npcs[i]);
    }
  for (auto& s : m.signs)
    if (s.x == fx && s.y == fy) return sc.say(s.text);
  for (size_t i = 0; i < m.chests.size(); i++)
    if (m.chests[i].x == fx && m.chests[i].y == fy && chestShown(m, m.chests[i])) return openChest((int)i);
  for (auto& b : m.bosses)
    if (bossAlive(b) && fx >= b.x && fx <= b.x + 1 && fy >= b.y && fy <= b.y + 1) return bossTouched(b);
  for (auto& b : m.buildings)
    if (fx == b.doorX() && fy == b.doorY()) return enterDoor(b);
  if (fx >= 0 && fy >= 0 && fx < m.w() && fy < m.h() && m.rows[fy][fx] == 'G' && !inExpedition() && !bounties().empty())
    return bountyMenu();  // tableau d'annonces : les primes
  if (fx >= 0 && fy >= 0 && fx < m.w() && fy < m.h() && m.rows[fy][fx] == '~' && items.count("canne") && items.at("canne") > 0 &&
      !inExpedition() && hasItem("canne"))
    return startFishing(fx, fy);  // face à l'eau, avec la canne : pêcher
}

// Porte d'un bâtiment (Entrée devant elle ou marcher dedans) : on entre s'il a un intérieur, sinon son événement
void Game::enterDoor(const Building& b) {
  if (b.decor()) return;  // décor : pas de porte
  audio::play("porte");
  int in = mapIndex(b.interior), ex, ey;
  if (in >= 0 && interiorEntry(maps()[in], M().id, ex, ey)) {
    changeMap(in, ex, ey, UP);
    return autoSave();
  }
  runEvent(b.event);
}

// Sauvegarde automatique (2026-10-20) : à chaque passage et porte, dans l'histoire seulement (ni Expédition, ni en ligne,
// ni mode test ou démo, qui ont leur propre fichier), si l'option est active
void Game::autoSave() {
  if (!options().autosave || inExpedition() || (online_ && online_->connected()) || saveName_ != autoSaveFile_ || pilot_ ||
      editorTest_ || storyTest_ || mode != Mode::Map || team.empty())
    return;
  if (saveGame()) autoSaveT_ = time;
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
  chestFx_ = {c.x, c.y, time, c.item.empty() ? std::string() : item(c.item).icon, mapId};  // lumière et objet qui monte
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
  fishing_.on = false;
  foeLevelBonus = 0;  // l'Arène, les outils et l'Expédition n'ont pas de bonus
  pilot_.reset();  // fin de la démo : la partie reprend son fichier de sauvegarde habituel
  demoPaused_ = false;
  if (!saveBeforePilot_.empty()) saveName_ = saveBeforePilot_, saveBeforePilot_.clear();
  if (expedition_) expedition_->leave();  // remet les données du jeu à la place de celles de l'expédition
  if (online_) online_->leave();          // ferme les connexions du multijoueur
  mode = Mode::Title;
  editorTest_ = storyTest_ = false;
  menus.clear();
  Menu m;
  m.x = 100, m.y = 110, m.w = 120, m.rows = 8, m.cancelable = false;  // (la place des icônes)
  bool can = saveExists();
  m.items.push_back({T("Nouvelle partie"), "", "", true, [this, can] {
                       if (!can) return starterMenu();
                       Menu c;  // une partie existe : la sauvegarde automatique la remplacerait
                       c.title = T("Nouvelle partie ?");
                       c.x = 70, c.y = 120, c.w = 180, c.rows = 2;
                       c.items.push_back({T("Annuler"), "", T("Garder la partie en cours."), true, [this] { menus.pop(); }});
                       c.items.push_back({T("Commencer"), "", T("Une partie est déjà sauvegardée : elle sera remplacée dès la première sauvegarde."), true,
                                          [this] {
                                            menus.pop();
                                            starterMenu();
                                          }});
                       c.onCancel = [this] { menus.pop(); };
                       menus.push(c);
                     }});
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
  setIcons(m, {{T("Nouvelle partie"), "etoile"}, {T("Continuer"), "reprendre"}, {T("Expédition"), "carte"}, {T("Multijoueur"), "duo"},
               {T("Outils"), "ic_marteau_1"}, {T("Options"), "engrenage"}, {T("Aide"), "aide"}, {T("Quitter"), "porte"}});
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

int Game::newGamePlus() const {
  int n = 0;
  while (has("ngp" + std::to_string(n + 1))) n++;
  return n;
}

// Nouvelle partie + : l'histoire recommence au départ avec l'équipe, l'équipement, l'or, le Bestiaire et le carnet de
// pêche ; les objets importants (prix 0, ni équipement ni gemme : clés, lettres, trident…) sont rendus, car ils
// ouvriraient des passages trop tôt ; tous les drapeaux de l'histoire sont effacés ; les ennemis gagnent des niveaux
void Game::startNewGamePlus() {
  const Rules& r = rules();
  int n = newGamePlus() + 1;
  std::set<std::string> keep;
  for (auto& f : flags)
    if (f.rfind("ngp", 0) == 0 || f.rfind("peche:", 0) == 0) keep.insert(f);
  keep.insert("ngp" + std::to_string(n));
  flags = keep;
  visited.clear();  // le voyage rapide repart de zéro : les passages (pont, bateaux…) sont refermés
  for (auto it = items.begin(); it != items.end();) {
    const bool key = hasItem(it->first) && item(it->first).price == 0 && item(it->first).slot < 0 && !item(it->first).gem;
    it = key ? items.erase(it) : std::next(it);
  }
  healAll(true);
  sc.clear();
  menus.clear();
  respawnMap = std::max(0, mapIndex(r.respawnMap)), respawnX = r.respawnX, respawnY = r.respawnY;
  mapId = -1;
  walkMap_ = -1;
  applyNewGamePlus();
  changeMap(std::max(0, mapIndex(r.startMap)), r.startX, r.startY, r.startDir);
  mode = Mode::Map;
  sc.say(T("Nouvelle partie + : l'aventure recommence, et les créatures sont plus fortes (+") + std::to_string(n * NGP_LEVELS) +
         T(" niveaux). Votre équipe, votre équipement et votre Bestiaire vous suivent."));
}

void Game::newGame(const std::string& starter) {
  const Rules& r = rules();
  menus.clear();
  sc.clear();
  team = {makeFighter(r.hero, r.startLevel), makeFighter(starter, r.startLevel)};
  reserve.clear();
  seen.clear(), caught.clear(), weak.clear(), tipsSeen.clear();
  visited.clear(), cardHand_.clear();
  pension_.clear(), pensionLvl_.clear(), eggs_.clear(), pensionSteps_ = 0, pensionEgg_ = false;
  items.clear();
  for (auto& [id, n] : r.startItems) items[id] = n;
  gold = r.startGold;
  flags.clear();
  foeLevelBonus = 0;
  respawnMap = std::max(0, mapIndex(r.respawnMap)), respawnX = r.respawnX, respawnY = r.respawnY;
  mapId = -1;
  walkMap_ = -1;  // les habitants repartent de leur place
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
    case Mode::Map:
      drawMap();
      if (float a = time - autoSaveT_; a >= 0 && a < 1.6f && !sc.busy() && !menus.active())  // « Sauvegarde… » en bas à droite
        g.text(g.right() - 6, SCREEN_H - 14, T("Sauvegarde…"), rgb(0xe8ecff, uint8_t(220 * std::min(1.f, (1.6f - a) * 2))), 2, false);
      break;
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
  if (fishing_.on && mode == Mode::Map && !sc.busy() && !menus.active()) drawFishing();
  if (cards_.on && mode == Mode::Map && !sc.busy() && !menus.active()) drawCards();
  if (!evoTo_.empty()) drawEvolution();
  if (worldMapOn_) drawWorldMap();
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
    bool keepCheck = g.checkLayout;  // le bandeau se superpose à tout (fenêtre des messages du combat) : pas un défaut
    g.checkLayout = false;
    g.rect(g.left(), 0, g.fullW, 11, rgb(0x000010, 170));
    g.text(160, 2, s, rgb(0xffd34d), 1);
    g.checkLayout = keepCheck;
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
  // Paysage de nuit (2026-10-20) : ciel, lune, montagnes enneigées, village au loin, brume, prairie au premier plan
  g.gradV(L, 0, W, 140, rgb(0x0c1230), rgb(0x3a3a70));
  g.gradV(L, 110, W, 50, rgb(0x3a3a70, 0), rgb(0x7a5a8a, 90));  // lueur à l'horizon
  float mx = std::max(292.f, L + W - 44), my = 26;                                 // la lune, et son halo
  g.ellipse(mx, my, 34, 34, rgb(0xfff4c8, 14)), g.ellipse(mx, my, 22, 22, rgb(0xfff4c8, 26));
  g.ellipse(mx, my, 12, 12, rgb(0xf6f0d6));
  g.ellipse(mx - 4, my - 3, 2.5f, 2, rgb(0xd8d2b8)), g.ellipse(mx + 4, my + 4, 2, 1.5f, rgb(0xd8d2b8)), g.ellipse(mx + 3, my - 5, 1.2f, 1, rgb(0xd8d2b8));
  std::vector<Pt> far = {{L, 140}, {L, 104}};  // montagnes lointaines, sommets enneigés
  for (float x = L, k = 0; x <= L + W + 40; x += 34, k++) {
    float peak = 70 + std::fmod(k * 37.f, 30.f);
    far.push_back({x + 17, peak}), far.push_back({x + 34, 100 + std::fmod(k * 13.f, 12.f)});
    g.tri(x + 17, peak, x + 11, peak + 9, x + 23, peak + 9, rgb(0xc8d0f0, 120));
  }
  far.push_back({L + W + 40, 140});
  g.poly(far, rgb(0x1c2048));
  for (float x = L, k = 0; x <= L + W + 40; x += 34, k++) {  // neige redessinée par-dessus le massif
    float peak = 70 + std::fmod(k * 37.f, 30.f);
    g.tri(x + 17, peak, x + 13, peak + 6, x + 21, peak + 6, rgb(0xdfe6ff, 170));
  }
  for (int i = 0; i < 4 * W / SCREEN_W; i++) g.ellipse(L + std::fmod(i * 97 + time * 10, W + 140) - 70, 122 + (i % 3) * 7, 100, 6, rgb(0xe6ebff, 20));
  g.poly({{L, 160}, {L, 122}, {40, 114}, {110, 124}, {170, 116}, {230, 112}, {290, 110}, {L + W, 116}, {L + W, 160}}, rgb(0x161a3c));  // collines
  for (int i = 0; i < 6; i++) {  // le village au loin : maisons, fenêtres allumées, fumée
    float hx = 228 + i * 11 + (i % 2) * 3, hy = 106 - (i % 3);
    g.rect(hx, hy, 9, 6, rgb(0x0e1130));
    g.tri(hx - 1, hy, hx + 10, hy, hx + 4.5f, hy - 5, rgb(0x0e1130));
    if ((i * 7 + int(time * .3f)) % 5) g.rect(hx + 3, hy + 2, 2, 2, rgb(0xffcf6a, 220));
    if (i % 2 == 0)
      for (int s = 0; s < 3; s++) {
        float ph = std::fmod(time * .4f + s / 3.f + i * .2f, 1.f);
        g.ellipse(hx + 7 + ph * 6, hy - 6 - ph * 16, 1.5f + ph * 3, 1 + ph * 2, rgb(0xb8bcd8, uint8_t(60 * (1 - ph))));
      }
  }
  g.rect(262, 93, 3, 12, rgb(0x0e1130)), g.tri(260, 94, 267, 94, 263.5f, 86, rgb(0x0e1130));  // le clocher
  g.gradV(L, 156, W, 84, rgb(0x1a2d3a), rgb(0x0e1a22));  // la prairie, où se tiennent les héros
  for (int i = 0; i < 70 * W / SCREEN_W; i++) {  // herbes qui ondulent
    uint32_t h = uint32_t(i) * 2246822519u;
    float x = L + float(h % 997) / 997.f * W, y = 158 + float((h >> 11) % 80), sway = std::sin(time * 1.6f + x * .05f) * 1.2f;
    g.line(x, y, x + sway, y - 3 - float(h % 3), 1, rgb(i % 3 ? 0x2c4a48 : 0x3a5c50));
    if (h % 11 == 0) g.rect(x + sway, y - 4, 1, 1, rgb(i % 2 ? 0xe8d0f0 : 0xfff0a0, 200));  // fleurs
  }
  for (float x : {L + 6.f, L + 22.f, L + W - 26.f, L + W - 10.f}) {  // sapins sombres sur les bords
    for (int t = 0; t < 3; t++) g.tri(x - 9 + t * 2, 168 - t * 9, x + 9 - t * 2, 168 - t * 9, x, 150 - t * 9, rgb(0x101a24));
    g.rect(x - 1, 168, 3, 5, rgb(0x0c141c));
  }
  for (int i = 0; i < 6 * W / SCREEN_W; i++) g.ellipse(L + std::fmod(i * 70 + time * 18, W + 100) - 50, 150 + (i % 4) * 12, 90, 6, rgb(0xe6ebff, 18));
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
  g.textBigFancy(160, 26, T("BRUMEVAL"), rgb(0xfff0a0), rgb(0xe8902a), rgb(0x3a1e08), 4, 1);  // or, de clair à ambré
  {  // reflet qui passe sur le titre
    float k = std::fmod(time, 4.f) / 1.2f;
    if (k < 1) {
      float x = 70 + 180 * k;
      g.poly({{x, 22}, {x + 6, 22}, {x - 4, 58}, {x - 10, 58}}, rgb(0xffffff, uint8_t(110 * std::sin(k * 3.14159f))));
    }
  }
  g.text(160, 76, T("Les gardiens de la vallée"), rgb(0xc9cbe0), 1);
  // de part et d'autre du menu (3.3) : Lior et Braisenard à gauche, Maëlle et Gouttelin à droite, face à face
  drawHuman(g, look(0), 40, 118, 2, RIGHT, 0, false);
  drawCreature(g, "braisenard", 76, 150, 1, true, time);
  drawHuman(g, look(std::min(1, (int)allLooks().size() - 1)), 248, 118, 2, LEFT, 0, false);
  drawCreature(g, "gouttelin", 248, 152, 1, false, time + .5f);
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

// Un lieu au bord de la mer : beaucoup d'eau sur la carte (calculé une fois par carte)
bool Game::coastal(const MapDef& m) const {
  static std::map<std::string, bool> known;
  auto it = known.find(m.id);
  if (it != known.end()) return it->second;
  int n = 0, tot = 0;
  for (auto& r : m.rows)
    for (char c : r) n += c == '~', tot++;
  return known[m.id] = tot > 0 && n * 100 > tot * 15;
}
bool Game::isNight() const {
  float p = std::fmod(time / 720.f + .85f, 1.f);
  return -std::cos(p * 6.2831853f) - .15f > .3f;  // nightAmount() > 0,5
}
float Game::nightAmount() const {
  if (!outdoors()) return 0;
  float p = std::fmod(time / 720.f + .85f, 1.f);  // 0 : midi ; la partie commence en matinée
  float c = std::cos(p * 6.2831853f);
  return std::clamp((-c - .15f) / .6f, 0.f, 1.f);
}
// Pluie (2026-10-20, dessin et météo des combats) : de temps en temps, sur les cartes de la vallée et de la forêt
// sans ambiance ; des averses de 210 s choisies d'après leur numéro (jamais la première), arrivée et départ en 15 s
float Game::rainAmount() const {
  if (!outdoors()) return 0;
  const MapDef& m = M();
  if (m.theme != Theme::Vallee && m.theme != Theme::Foret) return 0;
  if (!m.ambiance.empty() && (m.ambianceUntil.empty() || !has(m.ambianceUntil))) return 0;
  const float SEG = 210;
  int s = int(time / SEG);
  uint32_t h = uint32_t(s) * 2654435761u;
  h ^= h >> 15;
  if (s == 0 || h % 5 != 1) return 0;
  float t = time - s * SEG;
  return std::clamp(std::min(t, SEG - t) / 15.f, 0.f, 1.f);
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
  camX_ = camX, camY_ = camY;
  int tx0 = std::max(0, (int)std::floor((camX + g.left()) / 16)), tx1 = (int)((camX + g.right()) / 16), ty0 = std::max(0, camY / 16);
  setTileNight(m.theme == Theme::Interieur && isNight());  // fenêtres des intérieurs sombres la nuit
  if (m.theme == Theme::Interieur) {  // (2026-10-21) autour d'une petite pièce : un halo sombre au lieu d'un noir plat
    float rx = -camX, ry = -camY, rw = m.w() * 16.f, rh = m.h() * 16.f;
    for (int i = 10; i >= 1; i--) {
      float d = i * 6.f;
      g.rect(rx - d, ry - d, rw + 2 * d, rh + 2 * d, rgb(0x2a2040, uint8_t(14)));
    }
  }
  for (int y = ty0; y <= ty0 + SCREEN_H / 16 && y < m.h(); y++)
    for (int x = tx0; x <= tx1 && x < m.w(); x++) drawTile(g, m, x, y, x * 16 - camX, y * 16 - camY, time);
  for (auto& f : footprints_) {  // empreintes dans la neige : deux pas par case, qui s'effacent en 30 s
    float a = 1 - (time - f.t) / 30;
    if (a <= 0) continue;
    float sx = f.x * 16 - camX, sy = f.y * 16 - camY;
    bool vert = f.dir == UP || f.dir == DOWN;
    for (int k = 0; k < 2; k++) {
      float ox = vert ? 5 + k * 5 : 3 + k * 8, oy = vert ? 3 + k * 7 : 8 + (k ? 2 : -1);
      g.ellipse(sx + ox, sy + oy, vert ? 1.5f : 2, vert ? 2 : 1.5f, rgb(0x9fb2cc, uint8_t(150 * a)));
    }
  }
  // Ombres portées (2026-10-25) : le jour, dehors, elles suivent le soleil ; longues le matin vers l'ouest, courtes à
  // midi, longues le soir vers l'est ; elles pâlissent au crépuscule et sous la pluie
  if (float sun = (1 - nightAmount()) * (1 - rainAmount()); outdoors() && sun > 0) {
    float p = std::fmod(time / 720.f + .85f, 1.f) * 6.2831853f;  // comme nightAmount : 0 à midi
    float low = 1 - std::clamp((std::cos(p) + .15f) / 1.15f, 0.f, 1.f);  // 0 : soleil haut, 1 : à l'horizon
    float kx = std::sin(p) * (.15f + .6f * low), ky = .1f;
    for (auto& b : m.buildings) drawBuildingShadow(g, b, b.x * 16 - camX, b.y * 16 - camY, kx, ky, sun);
    for (int y = std::max(0, ty0 - 1); y <= ty0 + SCREEN_H / 16 + 3 && y < m.h(); y++)
      for (int x = std::max(0, tx0 - 3); x <= tx1 + 3 && x < m.w(); x++)
        if (hasProp(m, x, y)) drawPropShadow(g, m, x, y, x * 16 - camX, y * 16 - camY, kx, ky, sun);
  }
  // Énigme (3.0) : grilles (fermées : barreaux), plaques allumées, rochers
  if (!m.puzzleFlag.empty()) {
    bool open = puzzleSolved(m);
    for (auto& [x, y] : puzzleTiles(m, 'D')) {
      float sx = x * 16 - camX, sy = y * 16 - camY;
      if (open) {  // (2026-10-21) la herse relevée : on n'en voit que les pointes en haut
        g.rect(sx, sy, 16, 3, rgb(0x2a2420));
        for (int i = 0; i < 4; i++) g.tri(sx + 1 + i * 4, sy + 3, sx + 3 + i * 4, sy + 3, sx + 2 + i * 4, sy + 6, rgb(0x8a8078));
      } else {  // une herse de fer : barreaux éclairés d'un côté, deux traverses rivetées, des pointes en bas
        g.rect(sx, sy, 16, 16, rgb(0x1a1612));
        for (int i = 0; i < 4; i++) {
          float bx = sx + 1 + i * 4;
          g.rect(bx, sy, 2, 14, rgb(0x6a6058)), g.rect(bx, sy, 1, 14, rgb(0xa89e94));
          g.tri(bx, sy + 14, bx + 2, sy + 14, bx + 1, sy + 16, rgb(0x8a8078));
        }
        for (float ty : {sy + 3.f, sy + 10.f}) {
          g.rect(sx, ty, 16, 2, rgb(0x4a4038)), g.rect(sx, ty, 16, 1, rgb(0x7a7068));
          for (int i = 0; i < 4; i++) g.rect(sx + 1.5f + i * 4, ty, 1, 1, rgb(0xc8beb4));  // les rivets
        }
      }
    }
    for (auto& [x, y] : puzzleTiles(m, 'P'))
      if (boulderAt(x, y)) {  // une plaque enfoncée : ses runes s'allument
        float sx = x * 16 - camX, sy = y * 16 - camY, k = .7f + .3f * std::sin(time * 3 + x);
        g.ellipse(sx + 8, sy + 8, 9, 8, rgb(0xffd34d, uint8_t(60 * k)));
        g.rect(sx + 1, sy + 13, 14, 2, rgb(0xffd34d, uint8_t(200 * k)));
      }
    for (auto& [x, y] : boulders_) {  // (2026-10-21) un rocher rond et ombré, fendu, un peu de mousse
      float sx = x * 16 - camX, sy = y * 16 - camY;
      g.ellipse(sx + 8, sy + 14, 7.5f, 2.5f, rgb(0x000000, 70));
      g.ellipse(sx + 8, sy + 8.5f, 7.5f, 7, rgb(0x4e4a54));
      g.ellipse(sx + 7.5f, sy + 7.5f, 6.5f, 6, rgb(0x76727c));
      g.ellipse(sx + 6.5f, sy + 6, 4.5f, 3.5f, rgb(0x96929c));
      g.ellipse(sx + 5.5f, sy + 4.5f, 2, 1.5f, rgb(0xc4c0ca));                           // le reflet
      g.rect(sx + 9, sy + 8, 1, 3, rgb(0x3e3a44)), g.rect(sx + 10, sy + 10, 2, 1, rgb(0x3e3a44));  // une fente
      g.ellipse(sx + 11, sy + 4.5f, 2.5f, 1.2f, rgb(0x5f8a46));                           // la mousse
    }
  }
  for (size_t i = 0; i < m.chests.size(); i++)
    if (chestShown(m, m.chests[i])) drawChest(g, m.chests[i].x * 16 - camX, m.chests[i].y * 16 - camY, has(chestFlag((int)i)), time);
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
  std::set<std::pair<int, int>> behind;  // cases qu'un objet haut plus bas ne doit pas cacher
  for (auto& c : m.chests)
    if (chestShown(m, c)) behind.insert({c.x, c.y});
  for (auto& sg : m.signs) behind.insert({sg.x, sg.y});
  for (size_t i = 0; i < m.npcs.size(); i++)
    if (npcVisible(m.npcs[i])) behind.insert(npcCell(m, i));
  behind.insert({px, py});
  if (moving) behind.insert({fromX, fromY});
  for (int y = ty0; y <= ty0 + SCREEN_H / 16 + 3 && y < m.h(); y++)  // + 3 : un arbre plus bas dépasse vers le haut
    for (int x = std::max(0, tx0 - 1); x <= tx1 + 1 && x < m.w(); x++)
      if (hasProp(m, x, y)) {
        // Un arbre (ou un objet haut) devant le joueur, un coffre, un panneau ou un habitant : à demi transparent (3.3)
        bool see = false;
        for (int dy = 1; dy <= 2 && !see; dy++)
          see = behind.count({x, y - dy}) > 0;
        actors.push_back({float(y * 16 + 1), [&, x, y, see] {
                            if (see) g.alpha = .5f;
                            drawProp(g, m, x, y, x * 16 - camX, y * 16 - camY, time);
                            g.alpha = 1;
                          }});
      }
  for (size_t i = 0; i < m.npcs.size(); i++)
    if (const Npc& n = m.npcs[i]; npcVisible(n)) {
      NpcWalk w;  // l'habitant à sa place, sauf si la carte affichée suit déjà ses pas
      if (walkMap_ == mapId && i < walk_.size() && walk_[i].hx == n.x && walk_[i].hy == n.y) w = walk_[i];
      else w.x = w.fx = n.x, w.y = w.fy = n.y, w.dir = n.dir, w.side = n.dir == RIGHT ? RIGHT : LEFT;
      float nx = (w.fx + (w.x - w.fx) * w.t) * 16, ny = (w.fy + (w.y - w.fy) * w.t) * 16;
      actors.push_back({ny, [&, i, n, w, nx, ny] {
                          // Habitants vivants (3.3, dessin seul) : de temps en temps, ils regardent ailleurs un instant (pas les
                          // dresseurs : leur regard compte) ; les animaux sautillent et changent de côté ; les promeneurs
                          // marchent, et chacun se tourne vers le joueur qui lui parle
                          bool facing = time < w.faceUntil || (int)i == talkNpc_;
                          int d = n.roam > 0 || facing ? w.dir : n.dir, h = n.x * 7 + n.y * 13;
                          float bob = 0;
                          const Look& lk = look(n.look);
                          bool animal = !lk.sprite.empty() && findSprite(lk.sprite) && !findSprite(lk.sprite + "_bas_0");
                          if (animal && (n.roam > 0 || facing)) d = w.side;  // un animal se voit de profil
                          if (n.sight == 0 && n.roam == 0 && !facing) {
                            float period = 5.f + h % 4, ph = std::fmod(time + h * .37f, period);
                            if (ph > period - 1.4f) d = animal ? (d == LEFT ? RIGHT : LEFT) : (h % 3 == 0 ? LEFT : h % 3 == 1 ? RIGHT : DOWN);
                          }
                          int step = w.t < 1 ? (w.t < .5f ? 1 : 2) : 0;
                          if (animal) bob = -std::fabs(std::sin(time * (w.t < 1 ? 9 : 3) + h)) * 1.5f;
                          // on lui parle : un petit sursaut (seul signe pour ceux qui n'ont qu'une pose de face)
                          if (float k = (time - w.talkT) / .3f; k >= 0 && k < 1) bob -= std::sin(k * 3.14159f) * 3;
                          drawHuman(g, lk, nx - camX, ny - camY - 2 + std::round(bob), 1, d, step, false);
                        }});
      // Quête (3.3) : une bulle « ! » (à prendre) ou « ? » dorée (à rendre) au-dessus de l'habitant, par-dessus tout
      if (int mk = n.event.empty() || (int)i == exclaimNpc_ ? 0 : questMark(n.event)) {
        float bx = nx - camX + 4, by = ny - camY - 19 + std::round(std::sin(time * 4 + (float)i) * 1.5f);
        actors.push_back({1e9f, [&, bx, by, mk] {
                            Color c = mk == 2 ? rgb(0xffd34d) : rgb(0xffffff);
                            g.rect(bx, by, 9, 11, c);
                            g.frame(bx, by, 9, 11, rgb(0x1a1a24));
                            g.rect(bx + 3, by + 11, 3, 1, rgb(0x1a1a24)), g.rect(bx + 4, by + 12, 1, 1, rgb(0x1a1a24));  // pointe
                            if (mk == 3) {  // combat proposé (revanche, défi) : deux épées croisées
                              g.line(bx + 2, by + 2, bx + 7, by + 8, 1, rgb(0x3a4a6a)), g.line(bx + 7, by + 2, bx + 2, by + 8, 1, rgb(0x3a4a6a));
                              g.rect(bx + 1, by + 7, 3, 1, rgb(0x9a6a2a)), g.rect(bx + 6, by + 7, 3, 1, rgb(0x9a6a2a));
                            } else
                              g.text(bx + 2, by - 1, mk == 2 ? "?" : "!", mk == 2 ? rgb(0x7a4a00) : rgb(0xd2493f), 0, false);
                          }});
      } else if (n.sight == 0 && (int)i != exclaimNpc_ && (int)i != talkNpc_) {
        // Bulles d'humeur (2026-10-20, dessin seul) : de temps en temps, un habitant fredonne, rêvasse ou sourit
        int h = n.x * 7 + n.y * 13;
        float period = 9.f + h % 7, ph = std::fmod(time + h * 1.3f, period);
        if (ph < 1.6f) {
          float bx = nx - camX + 5, by = ny - camY - 17 - std::min(1.f, ph * 4) * 2, a = std::min(1.f, (1.6f - ph) * 3);
          int kind = h % 3;
          actors.push_back({1e9f, [&, bx, by, a, kind] {
                              g.ellipse(bx + 4, by + 4, 6, 5, rgb(0xffffff, uint8_t(220 * a)));
                              g.rect(bx + 1, by + 9, 2, 2, rgb(0xffffff, uint8_t(220 * a)));
                              Color ink = rgb(kind == 2 ? 0xd2493f : 0x3a4a6a, uint8_t(255 * a));
                              if (kind == 0) {  // une note de musique
                                g.rect(bx + 5, by + 1, 1, 5, ink), g.rect(bx + 5, by + 1, 2, 1, ink), g.ellipse(bx + 4, by + 6, 1.5f, 1.2f, ink);
                              } else if (kind == 1) {  // « … »
                                for (int k = 0; k < 3; k++) g.rect(bx + 1 + k * 3, by + 4, 1, 1, ink);
                              } else {  // un petit cœur
                                g.ellipse(bx + 3, by + 3, 1.5f, 1.5f, ink), g.ellipse(bx + 5, by + 3, 1.5f, 1.5f, ink);
                                g.tri(bx + 1.5f, by + 3.5f, bx + 6.5f, by + 3.5f, bx + 4, by + 7, ink);
                              }
                            }});
        }
      }
    }
  for (auto& b : m.bosses)
    if (bossAlive(b))
      actors.push_back({float(b.y * 16 + 16), [&, b] {
                          drawCreature(g, b.id, (b.x + 1) * 16 - camX, (b.y + 1) * 16 - camY - 2, .6f, false, time);
                          if (b.bounty) {  // monstre d'une prime : bulle rouge « ! » au-dessus
                            float bx = (b.x + 1) * 16 - camX - 4, by = b.y * 16 - camY - 14 + std::sin(time * 4) * 1.5f;
                            g.rect(bx - 1, by - 1, 10, 11, rgb(0x2a1010));
                            g.rect(bx, by, 8, 9, rgb(0xd2493f));
                            g.text(bx + 2, by - 1, "!", WHITE, 0, false);
                          }
                        }});
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
    auto [ex, ey] = npcCell(m, (size_t)exclaimNpc_);
    float bx = ex * 16 - camX + 4, by = ey * 16 - camY - 16;
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
  // Le compagnon suit le joueur (la première créature de l'équipe), une case derrière lui
  const Fighter* buddy = nullptr;  // la première créature en forme, sinon la première tout court
  for (auto& f : team)
    if (!f->S().human && (f->alive() || !buddy)) {
      buddy = f.get();
      if (f->alive()) break;
    }
  if (buddy && !trail_.empty() && !(online_ && online_->inGroup())) {
    std::pair<int, int> to = trail_.back(), from = trail_.size() >= 2 && moving ? trail_[trail_.size() - 2] : to;
    float k2 = moving ? moveT : 1;
    float bx = (from.first + (to.first - from.first) * k2) * 16, by = (from.second + (to.second - from.second) * k2) * 16;
    if (to.first != from.first) followDir_ = to.first > from.first ? RIGHT : LEFT;
    if (!(bx == ppx && by == ppy)) {  // pas sur le joueur (premier pas après un changement de carte)
      std::string sp = buddy->sp;
      bool right = followDir_ == RIGHT, chroma = buddy->chroma;
      // il trottine : un petit bond à chaque pas (deux par case en courant), sinon il respire sur place
      float hop = moving && from != to ? -std::fabs(std::sin(moveT * 3.14159f * (running_ ? 2 : 1))) * 3.5f : 0;
      actors.push_back({by - .5f, [&, bx, by, sp, right, hop, chroma] {
                          drawCreature(g, sp, bx - camX + 8, by - camY + 4 + std::round(hop), .62f, right, time, chroma);
                        }});
    }
  }
  actors.push_back({ppy, [&] {
                       drawHuman(g, look(playerLook), ppx - camX, ppy - camY - 2, 1, dir, step, false, run);
                       if (!moving) tallGrass(g, m, px, py, px * 16 - camX, py * 16 - camY, time, true);  // pieds dans l'herbe
                     }});
  std::sort(actors.begin(), actors.end(), [](const Actor& a, const Actor& b) { return a.y < b.y; });
  for (auto& a : actors) a.draw();
  if (float a = time - chestFx_.t; chestFx_.map == mapId && a >= 0 && a < 1.4f) {  // un coffre s'ouvre
    float cx = chestFx_.x * 16 - camX + 8, cy = chestFx_.y * 16 - camY + 4, f = 1 - a / 1.4f;
    for (int k = 0; k < 8; k++) {  // rayons de lumière
      float ang = k * .785f + a * 1.5f, len = 6 + 14 * std::min(1.f, a * 3);
      g.line(cx, cy, cx + std::cos(ang) * len, cy + std::sin(ang) * len * .7f, 1, rgb(0xfff2b0, uint8_t(160 * f)));
    }
    g.ellipse(cx, cy, 6 + a * 4, 4 + a * 3, rgb(0xfff6d0, uint8_t(110 * f)));
    float iy = cy - 6 - std::min(1.f, a * 2.5f) * 12;
    if (const SpriteRect* r = chestFx_.icon.empty() ? nullptr : findSprite(chestFx_.icon)) g.sprite(*r, cx - r->w / 2.f, iy - r->h / 2.f);
    else
      for (int k = 0; k < 3; k++) g.ellipse(cx - 4 + k * 4, iy + (k % 2), 2.5f, 2.5f, rgb(0xffd34d)), g.ellipse(cx - 4.5f + k * 4, iy - .5f + (k % 2), 1, 1, rgb(0xfff6c0));
  }
  for (auto& p : puffs_) {  // brins d'herbe qui volent, poussière de course
    float a = time - p.t, sx = p.x - camX, sy = p.y - camY;
    if (p.kind == 0 && a < .5f)
      for (int k = 0; k < 5; k++) {
        float ang = k * 1.26f + p.x * .13f, vx = std::cos(ang) * 20, vy = -26 + k * 4.f;
        g.rect(sx + vx * a, sy + vy * a + 70 * a * a, 2, 1, rgb(k % 2 ? 0x6fbf4a : 0x4f9a35, uint8_t(230 * (1 - a / .5f))));
      }
    if (p.kind == 1 && a < .4f) {
      Color c = rgb(m.theme == Theme::Neige ? 0xf4f8ff : 0xd8cbb0, uint8_t(110 * (1 - a / .4f)));
      g.ellipse(sx - 3 - a * 8, sy - a * 6, 2 + a * 8, 1.5f + a * 5, c), g.ellipse(sx + 3 + a * 8, sy - a * 5, 2 + a * 7, 1.5f + a * 4, c);
    }
  }
  // Ombres de nuages qui glissent sur la campagne, le jour, par beau temps
  if (outdoors() && (m.theme == Theme::Vallee || m.theme == Theme::Foret || m.theme == Theme::Neige) &&
      (m.ambiance.empty() || (!m.ambianceUntil.empty() && has(m.ambianceUntil)))) {
    float clear = (1 - nightAmount()) * (1 - rainAmount());
    float W = m.w() * 16 + 400.f, H = m.h() * 16 + 200.f;
    for (int i = 0; clear > 0 && i < 1 + m.w() * m.h() / 500; i++) {
      float wx = std::fmod(i * 331.f + time * (6 + i % 3), W) - 200 - camX, wy = std::fmod(i * 197.f + i * i * 13.f, H) - 100 - camY;
      Color c = rgb(0x0a1030, uint8_t(16 * clear));
      g.ellipse(wx, wy, 64, 30, c), g.ellipse(wx + 40, wy + 10, 46, 24, c), g.ellipse(wx - 36, wy + 8, 40, 20, c);
    }
  }
  drawMapCritters(g, m, tx0, tx1, ty0, ty0 + SCREEN_H / 16 + 1, camX, camY, time, nightAmount());
  // Jour et nuit : la carte s'assombrit (crépuscule orangé), les fenêtres s'allument, le héros porte une lanterne
  float night = nightAmount();
  if (m.light && night < 1)  // (2026-10-22) la lumière propre au lieu : l'aube rose d'Aube, la lumière froide des Échos
    g.rect(g.left(), 0, g.fullW, SCREEN_H, rgb(m.light, uint8_t(42 * (1 - night))));
  if (m.theme == Theme::Interieur && isNight())  // (2026-10-21) la nuit, dedans : la lumière chaude des bougies
    g.rect(g.left(), 0, g.fullW, SCREEN_H, rgb(0x4a2a08, 46));
  if (night > 0) {
    g.rect(g.left(), 0, g.fullW, SCREEN_H, rgb(0x0a1440, uint8_t(155 * night)));
    float dusk = night * (1 - night) * 4;
    if (dusk > 0) g.rect(g.left(), 0, g.fullW, SCREEN_H, rgb(0xff8a3a, uint8_t(30 * dusk)));
    for (auto& b : m.buildings) drawBuildingLights(g, b, b.x * 16 - camX, b.y * 16 - camY, night);
    // Réverbères et feux de camp allumés (3.3) : un halo chaud qui vacille un peu
    for (int y = ty0; y <= ty0 + SCREEN_H / 16 + 2 && y < m.h(); y++)
      for (int x = std::max(0, tx0 - 1); x <= tx1 + 1 && x < m.w(); x++) {
        char c = m.rows[y][x];
        if (c != 'L' && c != 'X') continue;
        float fl = .85f + .15f * std::sin(time * (c == 'X' ? 11.f : 3.f) + x * 1.7f);
        float lx = x * 16 - camX + 8, ly = y * 16 - camY + (c == 'L' ? -10 : 8);
        g.ellipse(lx, ly, 26 * fl, 18 * fl, rgb(0xffc86a, uint8_t(26 * night)));
        g.ellipse(lx, ly, 12 * fl, 9 * fl, rgb(0xffd890, uint8_t(60 * night)));
        if (c == 'L') g.ellipse(lx, ly, 3, 3, rgb(0xfff2c0, uint8_t(220 * night)));
      }
    float lx = ppx - camX + 8, ly = ppy - camY + 6;
    g.ellipse(lx, ly, 46, 34, rgb(0xffe8b0, uint8_t(14 * night)));
    g.ellipse(lx, ly, 26, 20, rgb(0xffe8b0, uint8_t(22 * night)));
    if (m.ambiance.empty() && (m.theme == Theme::Vallee || m.theme == Theme::Foret))  // lucioles la nuit
      for (int i = 0; i < 12 * g.fullW / SCREEN_W; i++) {
        float x = g.left() + std::fmod(i * 61.f + std::sin(time * .7f + i) * 20 + 40, (float)g.fullW), y = std::fmod(i * 37.f + std::cos(time * .5f + i * 2) * 14, 240.f);
        g.ellipse(x, y, 1.5f, 1.5f, rgb(0xe8ff9a, uint8_t(night * (90 + 80 * std::sin(time * 3 + i)))));
      }
  }
  // Fête des lanternes (champ « fete » de la carte) : la nuit, des lanternes de papier montent doucement dans le ciel
  if (night > .3f && !m.festival.is_null() && checkCond(m.festival))
    for (int i = 0; i < 14 * g.fullW / SCREEN_W; i++) {
      float sp = 6 + i % 5 * 1.5f, y = 250 - std::fmod(time * sp + i * 71.f, 270.f);
      float x = g.left() + std::fmod(i * 83.f + std::sin(time * .5f + i) * 8, (float)g.fullW);
      uint8_t a = uint8_t(std::min(1.f, (night - .3f) * 2) * 255 * std::clamp(y / 60.f, 0.f, 1.f));
      g.ellipse(x, y, 6, 6, rgb(0xffb04a, uint8_t(a / 6)));
      g.rect(x - 2, y - 3, 5, 6, rgb(0xff9a3a, a));
      g.rect(x - 1, y - 2, 3, 4, rgb(0xffe08a, a));
    }
  // Mouettes (2026-10-21) : le jour, au-dessus des lieux au bord de la mer, elles planent et battent des ailes de temps
  // en temps ; leur ombre glisse sur le sol, plus bas
  if (m.ambiance.empty() && m.theme == Theme::Vallee && night < .4f && coastal(m))
    for (int i = 0; i < 4 * g.fullW / SCREEN_W; i++) {
      float sp = 14 + i % 3 * 5, x = g.left() + std::fmod(i * 131.f + time * sp, g.fullW + 60.f) - 30;
      float y = 30 + std::fmod(i * 67.f, 120.f) + std::sin(time * .8f + i * 2) * 6;
      float flap = std::fmod(time * 1.3f + i * .7f, 3.f) < .9f ? std::sin(time * 14 + i) : .25f;  // bat des ailes, puis plane
      Color c = rgb(0xffffff, uint8_t(230 * (1 - night * 2)));
      g.ellipse(x + 10, y + 34, 4, 1.5f, rgb(0x000000, 40));  // l'ombre
      Color under = rgb(0x8a96a4, c.a);  // le dessous des ailes, plus sombre : deux pixels d'épaisseur
      g.line(x - 7, y - flap * 4 + 1, x, y + 1, 1, under), g.line(x, y + 1, x + 7, y - flap * 4 + 1, 1, under);
      g.line(x - 7, y - flap * 4, x, y, 1, c), g.line(x, y, x + 7, y - flap * 4, 1, c);
      g.rect(x - 1, y, 3, 2, c);
      g.rect(x - 7, y - flap * 4, 1, 1, rgb(0x3a3a44, c.a)), g.rect(x + 7, y - flap * 4, 1, 1, rgb(0x3a3a44, c.a));  // bouts noirs
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
  // Pluie : ciel assombri, gouttes en biais, éclaboussures au sol
  if (float rain = rainAmount(); rain > 0) {
    // (2026-10-21) des flaques sur les chemins et les pavés, où les gouttes font des ronds
    for (int y = ty0; y <= ty0 + SCREEN_H / 16 + 1 && y < m.h(); y++)
      for (int x = std::max(0, tx0); x <= tx1 && x < m.w(); x++) {
        char c = m.rows[(size_t)y][(size_t)x];
        uint32_t h = uint32_t(x) * 73856093u ^ uint32_t(y) * 19349663u;
        if ((c != '=' && c != 'q') || h % 9) continue;
        float px0 = x * 16 - camX + 3 + (h >> 5) % 6, py0 = y * 16 - camY + 6 + (h >> 9) % 6, w = 4 + (h >> 13) % 3;
        g.ellipse(px0 + w, py0 + 2, w + 1, 2.5f, rgb(0x4a5a78, uint8_t(120 * rain)));
        g.rect(px0 + 2, py0 + 1, 2, 1, rgb(0xd8e4f8, uint8_t(110 * rain)));  // le reflet du ciel
        float k = std::fmod(time * 1.8f + (h >> 3) % 10 * .1f, 1.f);
        g.ellipse(px0 + w, py0 + 2, 1 + k * w, .5f + k * 2, rgb(0xd8e4f8, uint8_t(150 * rain * (1 - k))));
      }
    g.rect(g.left(), 0, g.fullW, SCREEN_H, rgb(0x2a3450, uint8_t(70 * rain)));
    for (int i = 0, n = int(90 * rain) * g.fullW / SCREEN_W; i < n; i++) {
      float x = g.left() + std::fmod(i * 37.f + i * i * .7f + time * 60, g.fullW + 20.f) - 10;
      float y = std::fmod(i * 53.f + time * (260 + (i % 7) * 20), 260.f) - 10;
      g.line(x, y, x - 3, y + 11, 1, rgb(0xd8e4f8, uint8_t(175 * rain)));
    }
    for (int i = 0, n = int(30 * rain) * g.fullW / SCREEN_W; i < n; i++) {
      float k = time * 3 + i * .37f, ph = k - std::floor(k);
      uint32_t h = uint32_t(int(k)) * 7919u + uint32_t(i) * 104729u;
      h ^= h >> 13, h *= 0x5bd1e995u, h ^= h >> 15;
      float x = g.left() + float(h % uint32_t(g.fullW)), y = float((h >> 12) % uint32_t(SCREEN_H));
      g.ellipse(x, y, 1 + ph * 3, .5f + ph * 1.2f, rgb(0xe4eeff, uint8_t(190 * rain * (1 - ph))));
    }
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
    // La lanterne (2026-10-21) : pleine lumière près du héros, puis un fondu régulier vers le noir (douze anneaux), qui
    // vacille un peu comme une flamme ; les cristaux brillent dans le noir
    float cx = ppx - camX + 8, cy = ppy - camY + 8;
    float flick = std::sin(time * 7.3f) * 1.2f + std::sin(time * 12.1f) * .6f;
    float rin = 44 + flick, rout = 98 + flick;
    const int N = 12;
    const uint8_t A = 212;
    auto half = [](float r, float dy) { float s = r * r - dy * dy; return s > 0 ? std::sqrt(s) : 0.f; };
    for (int y = 0; y < SCREEN_H; y++) {
      float dy = y + .5f - cy;
      float wo = half(rout, dy);
      if (wo <= 0) {
        g.rect(g.left(), y, g.fullW, 1, rgb(0x05030a, A));
        continue;
      }
      // bornes entières : les anneaux se touchent sans trou ni recouvrement (sinon un pointillé dessine le cercle)
      int L = (int)std::lround(cx - wo), Rr = (int)std::lround(cx + wo);
      g.rect(g.left(), y, L - g.left(), 1, rgb(0x05030a, A));
      g.rect(Rr, y, g.right() - Rr, 1, rgb(0x05030a, A));
      for (int k = N - 1; k >= 0; k--) {
        float w1 = half(rin + (rout - rin) * k / N, dy);
        int l1 = (int)std::lround(cx - w1), r1 = (int)std::lround(cx + w1);
        Color c = rgb(0x05030a, uint8_t(A * std::pow((k + .5f) / N, 1.4f)));
        if (l1 > L) g.rect(L, y, l1 - L, 1, c);
        if (Rr > r1) g.rect(r1, y, Rr - r1, 1, c);
        L = std::max(L, l1), Rr = std::min(Rr, r1);
      }
    }
    g.ellipse(cx, cy, rin, rin * .9f, rgb(0xffb050, 14));  // la lumière chaude de la lanterne
    for (int y = std::max(0, ty0 - 1); y <= ty0 + SCREEN_H / 16 + 2 && y < m.h(); y++)
      for (int x = std::max(0, tx0 - 1); x <= tx1 + 1 && x < m.w(); x++)
        if (m.rows[(size_t)y][(size_t)x] == 'x') {
          float gx = x * 16 - camX + 8, gy = y * 16 - camY + 9, p = .8f + .2f * std::sin(time * 2 + x);
          g.ellipse(gx, gy, 22 * p, 18 * p, rgb(0x7fd6ff, 22));
          g.ellipse(gx, gy, 11 * p, 9 * p, rgb(0x9fe4ff, 40));
        }
  }
  // Bandeau du nom de la région
  if (float k = (time - mapFadeT_) / .3f; k >= 0 && k < 1)  // changement de carte (3.3) : on sort du noir
    g.rect(g.left(), 0, g.fullW, SCREEN_H, rgb(0x000000, uint8_t(255 * (1 - k) * (1 - k))));
  if (banner > 0 && !menus.active()) {  // le nom de la région s'efface quand un menu s'ouvre
    // (2026-10-21) un ruban : bande sombre qui s'efface vers les bords, deux filets dorés, le nom en grand s'il tient
    float a = std::min(1.f, banner / .5f);
    bool big = Gfx::textW(bannerText) * 2 <= 280;
    float tw = float(big ? Gfx::textW(bannerText) * 2 : Gfx::textW(bannerText)), half = tw / 2 + 36, cy = 22;
    for (int i = 0; i < (int)half; i++) {
      float f = 1 - float(i) / half;
      Color band = rgb(0x0a0c1e, uint8_t(a * 205 * std::min(1.f, f * 1.7f))), line = rgb(0xffd34d, uint8_t(a * 210 * f));
      for (float x : {159.f - i, 160.f + i}) {
        g.rect(x, cy - 11, 1, 22, band);
        g.rect(x, cy - 12, 1, 1, line), g.rect(x, cy + 11, 1, 1, line);
      }
    }
    g.alpha = a;
    if (big) g.textBig(160, cy - 11, bannerText, GOLD, 2, 1);  // le corps des lettres (lignes 2 à 8) au milieu du ruban
    else g.text(160, cy - 5, bannerText, GOLD, 1);
    g.alpha = 1;
  }
  // Fenêtres
  if (panelMode_ == 3 && menus.active()) drawBestiaryPanel(154, 8);  // à droite de la liste (146 px avec ses icônes)
  else if (panelMode_ == 4 && menus.active()) drawFormationPanel(146, 8);
  else if (panelMode_ == 5 && menus.active()) drawShopPanel(60, 128);
  else if (panelMode_ && menus.active()) drawTeamPanel(panelMode_ == 1 ? 120 : 146, 8, panelMode_ == 2 ? teamPanelSel_ : -1);
  if (menus.active()) menus.draw(g, time);
  drawDialogue();
  int noticeY = 190;
  if (menus.active() && askText_.empty() && !menus.help().empty()) {
    auto lines = Gfx::wrap(menus.help(), 300);
    // trois lignes au plus : la fenêtre ne monte pas sur les panneaux (textes anglais plus longs) ; sans panneau, jusqu'à
    // six tant qu'elle ne touche aucun menu (2026-10-26 : le conseil du prochain gardien, dans le Journal, était coupé)
    size_t most = panelMode_ ? 3 : (size_t)std::clamp((236 - 10 - 2 - menus.maxBottom()) / 11, 3, 6);
    if (lines.size() > most) {
      lines.resize(most);
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
  // (2026-10-22) « Nom : réplique » : le nom passe dans un onglet au-dessus de la fenêtre, la réplique seule dedans
  std::string speaker, body = askText_.empty() ? sc.text() : askText_;
  if (askText_.empty()) {
    auto c = body.find(": ");
    if (c != std::string::npos && c > 0 && c <= 30) {
      std::string who = body.substr(0, c);
      while (!who.empty() && who.back() == ' ') who.pop_back();
      bool sentence = who.find_first_of(".!?,\"") != std::string::npos || who.find("«") != std::string::npos || who.find("»") != std::string::npos;
      if (!who.empty() && !sentence) {  // (comparer « » en entier : leurs octets se retrouvent dans « ë »)
        speaker = who;
        body = body.substr(c + 2);
        text = text.size() > c + 2 ? text.substr(c + 2) : "";  // le texte visible commence par le même préfixe
      }
    }
  }
  bool portrait = false;
  if (const Look* lk = askText_.empty() && mode != Mode::Battle ? speakerLook(sc.text()) : nullptr) {  // portrait au-dessus de la fenêtre
    portrait = true;
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
  if (!speaker.empty()) {  // l'onglet du nom, à côté du portrait
    int tw = Gfx::textW(speaker) + 14, tx = portrait ? 52 : 8;
    g.window(tx, y - 15, tw, 15);
    g.text(tx + 7, y - 12, speaker, GOLD);
  }
  g.window(4, y, 312, 66);
  auto full = Gfx::wrap(body, 296);
  if (full.size() >= 4 && Gfx::textW(full[3]) > 284)  // la 4e ligne irait sous la flèche « ▼ » : lignes un peu plus courtes
    full = Gfx::wrap(body, 284);
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
  // (2026-10-22) l'aube sur Brumeval : le ciel passe de la nuit à l'orange, le soleil se lève derrière les montagnes,
  // les dernières étoiles s'effacent, des lanternes montent
  g.gradV(L, 0, W, 150, rgb(0x0e1550), rgb(0xe89a6a));
  g.gradV(L, 150, W, SCREEN_H - 150, rgb(0xe89a6a), rgb(0xf4c890));
  for (int i = 0; i < 40 * W / SCREEN_W; i++) {  // les étoiles, seulement en haut, qui s'effacent
    float x = L + std::fmod(i * 61.f + i * i * .3f, W), y = std::fmod(i * 37.f, 70.f);
    g.rect(x, y, 1, 1, rgb(0xffffff, uint8_t((70 - y) * 2.6f * (.6f + .4f * std::sin(time * 2 + i)))));
  }
  float rise = std::min(1.f, time * .02f + .6f);  // le soleil monte doucement
  float sy = 168 - rise * 30;
  for (int r = 5; r >= 1; r--) g.ellipse(250, sy, 18.f + r * 9, 18.f + r * 9, rgb(0xffe0a0, uint8_t(18 + (5 - r) * 6)));  // sa lueur
  g.ellipse(250, sy, 18, 18, rgb(0xfff4c0));
  g.poly({{L, 240}, {L, 150}, {0, 165}, {60, 128}, {120, 160}, {190, 120}, {260, 158}, {320, 132}, {L + W, 150}, {L + W, 240}}, rgb(0x8a6a8a));  // au loin
  g.poly({{L, 240}, {L, 178}, {0, 182}, {70, 150}, {140, 182}, {210, 152}, {290, 184}, {L + W, 166}, {L + W, 240}}, rgb(0x4a5a6a));
  g.poly({{L, 240}, {L, 200}, {0, 202}, {80, 182}, {160, 200}, {240, 186}, {320, 202}, {L + W, 192}, {L + W, 240}}, rgb(0x2e6b3a));  // la colline
  for (int i = 0; i < 9 * W / SCREEN_W; i++) {  // des lanternes qui montent vers le ciel
    float x = L + std::fmod(i * 97.f + std::sin(time * .4f + i) * 10, W), y = 230 - std::fmod(time * (7 + i % 3 * 2) + i * 53.f, 240.f);
    g.ellipse(x, y, 5, 5, rgb(0xffb04a, 40));
    g.rect(x - 2, y - 3, 5, 6, rgb(0xff9a3a, 220)), g.rect(x - 1, y - 2, 3, 4, rgb(0xffe08a, 230));
  }
  g.textBigFancy(160, 26, T("FIN"), rgb(0xfff0a0), rgb(0xe8902a), rgb(0x2a1606), 3, 1);
  auto lines = Gfx::wrap(endingText_.empty() ? T("La brume se lève enfin sur Brumeval.") : endingText_, 280);
  for (const char* l : {"", T("Un jeu créé avec Claude"), T("C++ et SDL2"), "", T("Appuyez sur Entrée pour continuer")}) lines.push_back(l);
  for (size_t i = 0; i < lines.size(); i++) g.text(160, 74 + i * 13, lines[i], WHITE, 1);  // (le texte garde son ombre : lisible sur l'aube)
  auto team = front();  // jusqu'à six combattants (héros et compagnons) : l'écart s'adapte
  float gap = team.size() > 1 ? std::min(100.f, 260.f / (team.size() - 1)) : 0, x = 160 - gap * (team.size() - 1) / 2;
  for (auto& f : team) {
    drawFighterSprite(g, *f, x, 205, 1, true, time);
    x += gap;
  }
}
