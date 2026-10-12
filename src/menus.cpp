// Menus du jeu : options et touches, menu de pause (équipe, compagnons, réserve, bestiaire,
// tactiques, équipement, journal, objets, magie), boutique (achat et vente), et la fiche de
// l'équipe dessinée à droite des menus. Méthodes de Game (game.hpp).
#include <algorithm>
#include <cmath>

#include "audio.hpp"
#include "battle.hpp"
#include "expedition.hpp"
#include "game.hpp"
#include "mapedit.hpp"
#include "online.hpp"
#include "options.hpp"
#include "sprites.hpp"
#include "storyedit.hpp"
#include "tactics.hpp"

static const Color WHITE = rgb(0xffffff), GOLD = rgb(0xffd34d), MUTED = rgb(0xaab3d8);

// ---------------------------------------------------------------------------
// Options : volumes, plein écran, touches (options.hpp)
// ---------------------------------------------------------------------------
void Game::optionsMenu(int sel) {
  int panel = panelMode_;
  panelMode_ = 0;  // le menu prend la place du résumé de l'équipe
  Menu m;
  m.title = "Options";
  m.x = 60, m.y = 40, m.w = 200, m.rows = 6, m.sel = sel;
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
  {
    MenuItem sp{"Vitesse du jeu", "", "Gauche / droite : ×1, ×2 ou ×3 (marche, combats, textes). Aussi avec F2. Toujours ×1 en multijoueur.", true,
                [] {
                  options().speed = options().speed % 3 + 1;
                  saveOptions();
                }};
    sp.adjust = [](int d) {
      options().speed = std::clamp(options().speed + d, 1, 3);
      saveOptions();
    };
    sp.rightFn = [] { return "×" + std::to_string(options().speed); };
    m.items.push_back(sp);
  }
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
  if (k == SDL_SCANCODE_RETURN || k == SDL_SCANCODE_KP_ENTER || k == SDL_SCANCODE_F11 || k == SDL_SCANCODE_F2 || k == SDL_SCANCODE_UP || k == SDL_SCANCODE_DOWN ||
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
  m.items.push_back({"Formation", "", "La place de chacun sur le damier de combat (avant, milieu, arrière).", true, [this] { formationMenu(); }});
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
  m.items.push_back({"Options", "", "Volumes, plein écran, vitesse du jeu et touches.", true, [this] { optionsMenu(); }});
  m.items.push_back({"Aide", "", "Statistiques, dégâts, damier, zones, états, tableau des types, touches (F1).", true, [this] { helpMenu(); }});
  m.items.push_back({"Reprendre", "", "", true, [this] { menus.clear(); }});
  m.rows = std::min(13, (int)m.items.size());  // au-delà, la liste défile (la bande d'aide reste visible)
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
// créature libre de l'équipe). En combat, le joueur le commande comme un héros.
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
                c ? "Obtenue. " + matchupText(s) : v ? "Rencontrée, pas encore obtenue. " + matchupText(s) : "Pas encore rencontrée.", true, nullptr, [this, id] { bestiarySel_ = id; }};
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

// Formation : la case de chaque combattant sur le damier (data.hpp), gardée dans la sauvegarde.
// L'avant frappe et reçoit plus fort au corps à corps ; l'arrière est protégé (et seules les
// attaques à distance l'atteignent tant que l'avant tient).
void Game::formationMenu(int sel) {
  panelMode_ = 4;
  auto fr = front();
  auto cells = formation(fr, true);
  for (size_t i = 0; i < fr.size(); i++) fr[i]->cell = cells[i];  // le placement automatique devient visible et modifiable
  Menu m;
  m.title = "Formation";
  m.x = 8, m.y = 8, m.w = 132, m.rows = 7;
  for (size_t i = 0; i < fr.size(); i++) {
    FighterP f = fr[i];
    int at = (int)i;
    MenuItem it{f->name(), "", cellName(f->cell) + ". Entrée : changer de case.", true, [this, f, at] {
                  Menu p;
                  p.title = "Case de " + utf8Prefix(f->name(), 10);
                  p.x = 8, p.y = 24, p.w = 132, p.rows = 9;  // à gauche du dessin du damier
                  for (int c = 0; c < N_CELLS; c++) {
                    std::string who;
                    for (auto& o : front())
                      if (o != f && o->cell == c) who = o->name();
                    MenuItem ci{cellName(c), who.empty() ? "" : "·", who.empty() ? "Case libre." : "Occupée par " + who + " : échanger.", true,
                                [this, f, c, at] {
                                  for (auto& o : front())
                                    if (o != f && o->cell == c) o->cell = f->cell;  // échange de cases
                                  f->cell = c;
                                  menus.pop();
                                  menus.pop();
                                  formationMenu(at);
                                },
                                [this, c] { formationCell_ = c; }};
                    ci.shrink = true;
                    p.items.push_back(ci);
                  }
                  p.sel = std::clamp(f->cell, 0, N_CELLS - 1);
                  menus.push(p);
                },
                [this, f] { formationCell_ = f->cell; }};
    it.shrink = true;
    m.items.push_back(it);
  }
  if (m.items.empty()) m.items.push_back({"(personne)", "", "", false, nullptr});
  m.sel = std::clamp(sel, 0, (int)m.items.size() - 1);
  menus.push(m);
}

// Dessin du damier de la formation : l'avant à gauche, face aux ennemis (comme en combat)
void Game::drawFormationPanel(int x, int y) {
  int w = 320 - x - 8, cw = (w - 16) / 3, ch = 32, h = 34 + 3 * ch + 16;
  g.window(x, y, w, h);
  g.text(x + 8, y + 5, "Damier", GOLD);
  g.text(x + w - 8, y + 5, "ennemis à gauche", MUTED, 2);
  auto fr = front();
  for (int c = 0; c < N_CELLS; c++) {
    int rx = x + 8 + cellCol(c) * cw, ry = y + 20 + cellRow(c) * ch;
    bool lit = c == formationCell_;
    g.rect(rx + 1, ry + 1, cw - 2, ch - 2, lit ? rgb(0xffd34d, 70) : rgb(0x000010, 60));
    for (auto& f : fr)
      if (f->cell == c) {
        g.text(rx + cw / 2, ry + 6, utf8Prefix(f->name(), cw / 6 - 1), f->S().human ? WHITE : rgb(0xb8f0c8), 1);
        g.text(rx + cw / 2, ry + 17, "N." + std::to_string(f->lvl), MUTED, 1);
      }
  }
  const char* COLS[] = {"Avant", "Milieu", "Arrière"};
  for (int k = 0; k < 3; k++) g.text(x + 8 + k * cw + cw / 2, y + 22 + 3 * ch, COLS[k], MUTED, 1);
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
    MenuItem it{d.name, "×" + std::to_string(n), itemHelp(d), true, [equip, id] { equip(id); }};
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
    std::string note = d.slot >= 0 ? " (à porter : Échap > Équipement)" : "";
    m.items.push_back({d.name, "×" + std::to_string(n), itemHelp(d) + note, d.field,
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
                           s.items.push_back({mv.name, std::to_string(mv.cost) + " PM", moveHelp(mv, caster.get()), caster->mp >= mv.cost, [this, caster, id] {
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
    m.items.push_back({d.name, std::to_string(d.price) + " or", itemHelp(d) + " (vous en avez " + std::to_string(have) + ")", true,
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
    MenuItem it{d.name, "+" + std::to_string(d.price / 2) + " or", "Vous en avez " + std::to_string(have->second) + ". " + itemHelp(d), true, [this, stock, id, at] {
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
