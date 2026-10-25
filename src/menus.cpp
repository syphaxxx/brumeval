// Menus du jeu : options et touches, menu de pause (équipe, compagnons, réserve, bestiaire,
// tactiques, équipement, journal, objets, magie), boutique (achat et vente), et la fiche de
// l'équipe dessinée à droite des menus. Méthodes de Game (game.hpp).
#include <algorithm>
#include <cmath>

#include "i18n.hpp"
#include "audio.hpp"
#include "battle.hpp"
#include "expedition.hpp"
#include "game.hpp"
#include "mapedit.hpp"
#include "online.hpp"
#include "options.hpp"
#include "primes.hpp"
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
  m.title = T("Options");
  m.x = 60, m.y = 40, m.w = 200, m.rows = 8, m.sel = sel;
  auto volume = [this](const std::string& label, int* v, const std::string& help) {
    MenuItem it{label, "", help, true, nullptr};
    it.adjust = [v](int d) {
      *v = std::clamp(*v + d, 0, 10);
      applyVolumes();
      saveOptions();
    };
    it.notches = [v] { return *v; };  // des crans plutôt que « 7 / 10 »
    return it;
  };
  m.items.push_back(volume(T("Musique"), &options().music, T("Gauche / droite : volume de la musique.")));
  m.items.push_back(volume(T("Effets sonores"), &options().effects, T("Gauche / droite : volume des bruitages.")));
  auto full = [] {
    options().fullscreen = !options().fullscreen;
    saveOptions();
  };
  MenuItem fs{T("Plein écran"), "", T("Aussi avec F11 ou Alt+Entrée."), true, full};
  fs.adjust = [full](int) { full(); };
  fs.rightFn = [] { return std::string(options().fullscreen ? T("Oui") : T("Non")); };
  m.items.push_back(fs);
  {
    MenuItem sp{T("Vitesse du jeu"), "", T("Gauche / droite : ×1, ×2 ou ×3 (marche, combats, textes). Aussi avec F2. Toujours ×1 en multijoueur."), true,
                [] {
                  options().speed = options().speed % 3 + 1;
                  saveOptions();
                }};
    sp.adjust = [](int d) {
      options().speed = std::clamp(options().speed + d, 1, 3);
      saveOptions();
    };
    sp.rightFn = [] { return T("×") + std::to_string(options().speed); };
    m.items.push_back(sp);
  }
  {
    auto lang = [] {  // langue (3.1) : tout se traduit aussitôt (les noms des données sont reconstruits)
      options().english = !options().english;
      setLanguage(options().english ? "en" : "fr");
      rebuildData();
      loadMaps();  // noms des lieux, panneaux, dialogues des habitants
      saveOptions();
    };
    MenuItem lg{T("Langue"), "", T("Français ou anglais (Language: French or English)."), true, lang};
    lg.adjust = [lang](int) { lang(); };
    lg.rightFn = [] { return std::string(options().english ? T("English") : T("Français")); };
    m.items.push_back(lg);
  }
  {
    auto flip = [] {
      options().tips = !options().tips;
      saveOptions();
    };
    MenuItem tp{T("Astuces"), "", T("Expliquer chaque nouvelle règle la première fois qu'on la rencontre (F1 : en savoir plus)."), true, flip};
    tp.adjust = [flip](int) { flip(); };
    tp.rightFn = [] { return std::string(options().tips ? T("Oui") : T("Non")); };
    m.items.push_back(tp);
  }
  {
    auto flip = [] {
      options().autosave = !options().autosave;
      saveOptions();
    };
    MenuItem as{T("Sauvegarde auto"), "", T("Sauvegarder la partie à chaque passage d'une carte à l'autre et à chaque porte."), true, flip};
    as.adjust = [flip](int) { flip(); };
    as.rightFn = [] { return std::string(options().autosave ? T("Oui") : T("Non")); };
    m.items.push_back(as);
  }
  m.items.push_back({T("Touches"), "", T("Changer les touches du clavier. La manette se branche toute seule."), true, [this] { keysMenu(); }});
  m.items.push_back({T("Retour"), "", "", true, [this, panel] {
                       panelMode_ = panel;
                       menus.pop();
                     }});
  m.onCancel = m.items.back().act;
  menus.push(m);
}

void Game::keysMenu(int sel) {
  Menu m;
  m.title = T("Touches");
  m.x = 72, m.y = 52, m.w = 210, m.rows = 9, m.sel = sel;
  const std::string help = T("Entrée : appuyer ensuite sur la nouvelle touche. Les flèches, Entrée et Échap marchent toujours.");
  for (int a = 0; a < N_KEYS; a++) {
    MenuItem it{keyActionName(a), "", help, true, [this, a] { bindKey_ = a; }};
    it.rightFn = [a] { return keyName(options().keys[a]); };
    m.items.push_back(it);
  }
  m.items.push_back({T("Touches par défaut"), "", T("ZQSD (WASD en QWERTY), Espace, Retour arrière et Tab."), true, [this] {
                       options().keys = Options::defaultKeys();
                       saveOptions();
                       notice(T("Touches par défaut remises."));
                     }});
  m.items.push_back({T("Retour"), "", "", true, [this] { menus.pop(); }});
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
    return notice(T("Cette touche marche déjà toute seule."));
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
  g.text(160, 102, T("Nouvelle touche pour « ") + std::string(keyActionName(bindKey_)) + " »", rgb(0xffd34d), 1);
  g.text(160, 116, T("Appuyez sur la touche voulue…"), WHITE, 1);
  g.text(160, 128, T("Échap : annuler"), MUTED, 1);
}

void Game::pauseMenu() {
  panelMode_ = 1;
  Menu m;
  m.x = 8, m.y = 8, m.w = 110, m.rows = 9;  // (la place des icônes ; le panneau de l'équipe à droite, à x = 120)
  if (editorTest_ && editor_)
    m.items.push_back({T("Fin du test"), "", T("Revenir à l'éditeur de cartes, à l'endroit où vous êtes."), true, [this] {
                         panelMode_ = 0;
                         editor_->returnFromTest();
                       }});
  if (storyTest_ && story_)
    m.items.push_back({T("Fin du test"), "", T("Revenir à l'éditeur d'histoire."), true, [this] {
                         panelMode_ = 0;
                         story_->returnFromTest();
                       }});
  if (!M().puzzleFlag.empty() && !puzzleSolved(M()) && M().puzzleX >= 0)  // énigme bloquée : tout remettre en place (3.0)
    m.items.push_back({T("Recommencer l'énigme"), "", T("Les rochers reviennent à leur place, et vous au début de l'énigme."), true, [this] {
                         menus.clear();
                         panelMode_ = 0;
                         resetBoulders();
                         px = M().puzzleX, py = M().puzzleY, moving = false;
                       }});
  m.items.push_back({T("Équipe"), "", T("Ordre de combat et fiches."), true, [this] { teamMenu(); }});
  m.items.push_back({T("Préparation"), "", T("Formation, compagnons, tactiques et réserve : préparer l'équipe aux combats."), true, [this] { prepMenu(); }});
  m.items.push_back({T("Équipement"), "", T("Armes, armures et accessoires."), true, [this] { gearMenu(); }});
  {
    int waiting = 0;
    for (auto& f : team) waiting += canEvolve(*f) && f->evolveLater;
    if (waiting)
      m.items.push_back({T("Évolution"), std::to_string(waiting), T("Des créatures peuvent évoluer (vous aviez répondu Non)."), true,
                         [this] { evolutionMenu(); }});
  }
  m.items.push_back({T("Objets"), "", T("Utiliser un objet."), true, [this] { itemMenu(); }});
  m.items.push_back({T("Magie"), "", T("Lancer un sort de soin."), true, [this] { magicMenu(); }});
  {
    int n = inExpedition() ? 0 : newDiscussions();  // une nouvelle discussion se voit dès le menu de pause
    m.items.push_back({T("Journal"), n ? std::to_string(n) : "", T("Quêtes, discussions des héros, Bestiaire, progression et exploits."), true,
                       [this] { journalMenu(); }});
  }
  if (!inExpedition())
    m.items.push_back({T("Carte"), "", T("La carte du monde : les lieux où vous pouvez aller, et où vous êtes."), true, [this] {
                         worldMapOn_ = true;
                         audio::play("valider");
                       }});
  if (inExpedition()) {
    bool group = online_ && online_->inGroup();
    m.items.push_back({T("Sauvegarder"), "", T("Expédition : ") + expedition_->status() + ".", true,
                       [this] { notice(saveGame() ? T("Expédition sauvegardée.") : T("Impossible de sauvegarder.")); }});
    if (group) m.items.push_back({T("Groupe"), "", T("Où en sont les autres joueurs."), true, [this] { online_->groupMenu(); }});
    m.items.push_back({T("Abandonner"), "", T("Arrêter l'expédition ici et recevoir les éclats gagnés."), true, [this] {
                         ask(T("Abandonner l'expédition ?"), [this] { expedition_->onDefeat(true); });
                       }});
    m.items.push_back({T("Écran titre"), "", group ? T("Sauvegarder, quitter le groupe et revenir à l'écran titre.") : T("Sauvegarder et revenir à l'écran titre."),
                       true, [this] {
                         saveGame();
                         titleMenu();
                       }});
  } else {
    m.items.push_back({T("Sauvegarder"), "", "", true, [this] { notice(saveGame() ? T("Partie sauvegardée.") : T("Impossible de sauvegarder.")); }});
    m.items.push_back({T("Écran titre"), "", "", true, [this] {
                         ask(T("Revenir à l'écran titre ? (pensez à sauvegarder)"), [this] { titleMenu(); });
                       }});
  }
  m.items.push_back({T("Options"), "", T("Volumes, plein écran, vitesse du jeu et touches."), true, [this] { optionsMenu(); }});
  m.items.push_back({T("Aide"), "", T("Statistiques, dégâts, damier, zones, états, tableau des types, touches (F1)."), true, [this] { helpMenu(); }});
  m.items.push_back({T("Reprendre"), "", "", true, [this] { menus.clear(); }});
  // (2026-10-23) une petite icône devant chaque entrée : la liste se lit d'un coup d'œil
  setIcons(m, {
      {T("Fin du test"), "porte"}, {T("Recommencer l'énigme"), "caillou"}, {T("Équipe"), "ic_casque_1"}, {T("Préparation"), "damier"},
      {T("Évolution"), "etoile"}, {T("Équipement"), "ic_epee_1"}, {T("Journal"), "parchemin"},
      {T("Carte"), "carte"}, {T("Objets"), "ic_sac"}, {T("Magie"), "ic_baton_1"}, {T("Sauvegarder"), "ic_gemme_cristal"},
      {T("Groupe"), "ic_casque_2"}, {T("Abandonner"), "drapeau"}, {T("Écran titre"), "porte"}, {T("Options"), "engrenage"},
      {T("Aide"), "aide"}, {T("Reprendre"), "reprendre"}});
  // (2026-10-23) l'ordre et les groupes : l'équipe, le sac, l'aventure, la partie
  for (size_t i = 0; i < m.items.size(); i++)
    if (m.items[i].label == T("Écran titre") && i + 1 < m.items.size()) {  // juste avant « Reprendre »
      MenuItem it = m.items[i];
      m.items.erase(m.items.begin() + (long)i);
      m.items.insert(m.items.end() - 1, it);
      break;
    }
  for (auto& it : m.items)
    it.sep = it.label == T("Objets") || it.label == T("Journal") || it.label == T("Sauvegarder") || it.label == T("Reprendre");
  m.rows = std::min(13, (int)m.items.size());  // au-delà, la liste défile (la bande d'aide reste visible)
  menus.push(m);
}

// Échap > Préparation (2026-10-23) : ce qui prépare l'équipe aux combats, pour alléger le menu de pause
void Game::prepMenu() {
  Menu m;
  m.title = T("Préparation");
  m.x = 8, m.y = 8, m.w = 110, m.rows = 6;
  m.items.push_back({T("Formation"), "", T("La place de chacun sur le damier de combat (avant, milieu, arrière)."), true, [this] { formationMenu(); }});
  m.items.push_back({T("Compagnons"), "", T("La créature qui combat aux côtés de chaque héros."), true, [this] { companionsMenu(); }});
  m.items.push_back({T("Tactiques"), "", T("Ce que chaque membre fait tout seul en combat (mode auto : touche Tab)."), true, [this] { tacticsMenu(); }});
  m.items.push_back({T("Réserve"), reserve.empty() ? "" : std::to_string(reserve.size()), T("Les créatures en plus de l'équipe (") +
                     std::to_string(rules().maxTeam) + T(" membres au plus)."), true, [this] { reserveMenu(); }});
  setIcons(m, {{T("Formation"), "damier"}, {T("Compagnons"), "patte"}, {T("Tactiques"), "viser"}, {T("Réserve"), "ic_lanterne"}});
  menus.push(m);
}

// Tactiques : mode auto, puis la liste des règles de chaque membre (tactics.hpp)
void Game::tacticsMenu(int sel) {
  panelMode_ = 0;
  Menu m;
  m.title = T("Tactiques");
  m.x = 112, m.y = 8, m.w = 200, m.rows = 11;
  m.sel = sel;
  {
    MenuItem it{T("Mode auto en combat"), "", T("Oui : quand sa jauge est pleine, chaque membre joue sa première tactique possible. En combat, Tab l'active ou le coupe."),
                true, [this] { tacticsAuto = !tacticsAuto; }};
    it.adjust = [this](int) { tacticsAuto = !tacticsAuto; };
    it.rightFn = [this] { return std::string(tacticsAuto ? T("Oui") : T("Non")); };
    m.items.push_back(it);
  }
  m.items.push_back(menuHeader(T("Règles de chaque membre")));
  for (size_t i = 0; i < team.size(); i++) {
    FighterP f = team[i];
    MenuItem it{f->name(), "", T("Entrée : modifier ses tactiques (") + std::to_string(tacticSlots(f->lvl)) + T(" lignes au niveau ") +
                                   std::to_string(f->lvl) + ").",
                true, [this, f] {
                  TacticsTarget t;
                  t.list = &f->tactics;
                  t.on = &f->tacticsOn;
                  t.fighter = f;
                  t.title = T("Tactiques de ") + f->name();
                  t.slots = tacticSlots(f->lvl);
                  openTacticsEditor(*this, t);
                }};
    it.rightFn = [f] {
      if (!f->tacticsOn) return std::string(T("manuel"));
      int n = 0, slots = tacticSlots(f->lvl);
      for (int k = 0; k < (int)f->tactics.size() && k < slots; k++) n += f->tactics[k].on;
      return std::to_string(n) + (n > 1 ? T(" règles") : T(" règle"));
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
  m.title = T("Réserve  ·  ") + std::to_string(reserve.size()) + T(" créature") + (reserve.size() > 1 ? "s" : "");
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
      MenuItem it{c->name(), T("N.") + std::to_string(c->lvl), typesName(c->S()) + " · " + std::to_string(c->hp) + "/" + std::to_string(c->mhp) + T(" PV"),
                  true, [done, c] { done(c); }};
      it.shrink = true;
      it.icon = typeDot(c->S().type);
      p.items.push_back(it);
    }
    menus.push(p);
  };
  m.items.push_back({T("Déposer une créature…"), "", creatures.empty() ? T("Aucune créature dans l'équipe.") : T("Envoyer une créature de l'équipe dans la réserve."),
                     !creatures.empty(), [this, creatures, pick] {
                       pick(T("Déposer"), creatures, [this](FighterP c) {
                         swapReserve(c, nullptr);
                         notice(c->name() + T(" va dans la réserve."));
                         menus.pop();
                         menus.pop();
                         reserveMenu(0);
                       });
                     }});
  for (size_t i = 0; i < reserve.size(); i++) {
    FighterP r = reserve[i];
    int at = (int)i + 1;
    std::string help = typesName(r->S()) + " · " + std::to_string(r->hp) + "/" + std::to_string(r->mhp) + T(" PV. ") +
                       (full ? T("Entrée : l'échanger contre une créature de l'équipe.") : T("Entrée : la prendre dans l'équipe."));
    MenuItem it{r->name(), T("N.") + std::to_string(r->lvl), help, !full || !creatures.empty(), [this, r, at, full, creatures, pick] {
                  if (!full) {
                    swapReserve(nullptr, r);
                    notice(r->name() + T(" rejoint l'équipe."));
                    menus.pop();
                    return reserveMenu(at);
                  }
                  pick(T("Échanger contre"), creatures, [this, r, at](FighterP c) {
                    swapReserve(c, r);
                    notice(r->name() + T(" prend la place de ") + c->name() + ".");
                    menus.pop();
                    menus.pop();
                    reserveMenu(at);
                  });
                }};
    it.shrink = true;
    it.icon = typeDot(r->S().type);
    m.items.push_back(it);
  }
  if (reserve.empty()) m.items.push_back({T("(réserve vide)"), "", T("Les créatures capturées quand l'équipe est complète arrivent ici."), false, nullptr});
  m.sel = std::clamp(sel, 0, (int)m.items.size() - 1);
  menus.push(m);
}

void Game::noteBestiary() {
  for (auto* v : {&team, &reserve})
    for (auto& f : *v)
      if (f && hasSpecies(f->sp) && !f->S().human) seen.insert(f->sp), caught.insert(f->sp);
}

// Bestiaire : toutes les créatures des données, dans leur ordre ; une créature jamais vue reste « ??? »
// « Évolue en Ronceflore au niveau 16. » / « Évolution de Ronceau (niveau 16). »
static std::string evolutionText(const Species& s) {
  std::string t;
  if (const Rules::Talent* tl = talentOf(s)) t += T("Talent : ") + tl->name + ". ";
  if (!s.evolveTo.empty() && hasSpecies(s.evolveTo))
    t += T("Évolue en ") + species(s.evolveTo).name + T(" au niveau ") + std::to_string(s.evolveLvl) + ". ";
  for (auto& o : allSpecies())
    if (o.evolveTo == s.id) t += T("Évolution de ") + o.name + T(" (niveau ") + std::to_string(o.evolveLvl) + "). ";
  return t;
}

void Game::bestiaryMenu(int sel) {
  noteBestiary();
  panelMode_ = 3;
  bestiaryPage_ = 0;
  std::vector<const Species*> all;
  for (auto& s : allSpecies())
    if (!s.human) all.push_back(&s);
  int nSeen = 0, nCaught = 0;
  for (auto* s : all) nSeen += seen.count(s->id) > 0, nCaught += caught.count(s->id) > 0;
  Menu m;
  m.title = T("Bestiaire  ") + std::to_string(nSeen) + "/" + std::to_string(all.size());
  m.x = 8, m.y = 8, m.w = 142, m.rows = 12;
  for (size_t i = 0; i < all.size(); i++) {
    const Species& s = *all[i];
    bool v = seen.count(s.id), c = caught.count(s.id);
    char num[8];
    std::snprintf(num, sizeof num, T("%02d "), int(i + 1));
    std::string id = s.id;
    MenuItem it{num + (v ? s.name : std::string("???")), c ? "★" : v ? "vu" : "",
                c ? T("Obtenue. ") + evolutionText(s) + matchupText(s) : v ? T("Rencontrée, pas encore obtenue. ") + evolutionText(s) + matchupText(s, &weak) : T("Pas encore rencontrée."), true, nullptr, [this, id] { bestiarySel_ = id; }};
    it.shrink = true;
    it.icon = v ? typeDot(s.type) : "inconnu";  // (2026-10-23) la couleur de son type, une fois rencontrée
    m.items.push_back(it);
  }
  if (!all.empty()) bestiarySel_ = all[(size_t)std::clamp(sel, 0, (int)all.size() - 1)]->id;
  m.sel = std::clamp(sel, 0, (int)m.items.size() - 1);
  m.items.insert(m.items.begin(), menuHeader(T("Obtenues : ") + std::to_string(nCaught)));
  m.sel++;
  menus.push(m);
}

Matchup knownMatchup(const Species& s, const std::set<std::string>* known) {
  Matchup m;
  m.full = !known;
  auto r = [&](const std::string& k) {
    auto it = s.resist.find(k);
    return it == s.resist.end() ? 1.f : it->second;
  };
  for (Type t = 0; t < (Type)types().size(); t++) {
    float e = typeEff(t, s.type) * (s.type2 >= 0 ? typeEff(t, s.type2) : 1.f) * r(types()[t].id);
    bool found = m.full || known->count(s.id + ":" + std::to_string(t));  // Brisure : découverte en la frappant
    if (e > 1) found ? m.weak.push_back({t, e}) : (void)m.hidden++;
    else if (e < 0 && found) m.absorb.push_back({t, e});
    else if (e >= 0 && e < 1 && m.full) m.resist.push_back({t, e});
  }
  m.phys = r("physique") * float(1 - talentValue(s, "resiste_physique"));  // comme applyHit : Carapace, Écorce runique
  m.mag = r("magique") * float(1 - talentValue(s, "resiste_magie"));
  return m;
}

// « ×0,5 », « ×2 »
static std::string multText(float e) {
  char b[16];
  std::snprintf(b, sizeof b, "%.2f", e);
  std::string s = b;
  while (s.back() == '0') s.pop_back();
  if (s.back() == '.') s.pop_back();
  std::replace(s.begin(), s.end(), '.', ',');
  return "×" + s;
}

// Fiche du bestiaire, page 2 : ce que l'équipe a appris pour la combattre (rien qui n'ait été découvert)
static void drawBestiaryTactics(Gfx& g, int x, int y, int w, const Species& s, const Matchup& mu) {
  int bottom = 184, ly = y + 32;  // au-dessus de l'aide
  g.window(x, y, w, bottom - y);
  g.text(x + 8, y + 6, s.name, GOLD);
  g.text(x + w - 8, y + 6, "<2/2", MUTED, 2);
  g.text(x + 8, y + 18, typesName(s), MUTED);
  auto dot = [&](float cx, float cy, Type t) {
    uint32_t c = types()[t].color & 0xffffff;
    g.ellipse(cx, cy, 3.5f, 3.5f, rgb((c >> 1) & 0x7f7f7f)), g.ellipse(cx, cy, 2.6f, 2.6f, rgb(c));
  };
  bool cut = false;
  auto room = [&] {
    if (ly + 11 > bottom - 4) cut = true;
    return !cut;
  };
  // Rôle : celui qu'on voit en combat (icône au-dessus de lui)
  const std::string& role = s.combatRole;
  if (role.empty())
    g.text(x + 8, ly, T("Rôle : aucun"), MUTED), ly += 13;
  else {
    uint32_t col = role == "soigneur" ? 0x6ad87a : role == "protecteur" ? 0x8ad8d8 : 0xf0e06a;
    g.text(x + 8, ly, std::string(T("Rôle : ")) + (role == "soigneur" ? T("Soigneur") : role == "protecteur" ? T("Protecteur") : T("Chef")), rgb(col));
    const char* why = role == "soigneur" ? T("Soigne les siens : abattez-le en premier.")
                    : role == "protecteur" ? T("Ses voisins en croix encaissent moins : isolez-les.")
                                           : T("Ses alliés agissent plus vite tant qu'il tient.");
    for (auto& l : Gfx::wrap(why, w - 16)) ly += 11, g.text(x + 8, ly, l, rgb(0xd8dcf0));
    ly += 13;
  }
  // Efficacités : une ligne par catégorie, pastille du type et son nom (names faux : pastilles groupées par
  // multiplicateur, pour tenir) ; « ? » tant que ce n'est pas découvert
  auto row = [&](const char* label, const std::vector<std::pair<Type, float>>& list, bool names, bool showMult, std::string tail, Color tc) {
    if (!room()) return;
    g.text(x + 8, ly, label, WHITE);
    int cx = x + 56;
    if (list.empty() && tail.empty()) tail = T("rien");
    std::vector<std::pair<std::vector<Type>, std::string>> segs;
    if (names)
      for (auto& [t, e] : list) segs.push_back({{t}, std::string(types()[t].name) + (showMult ? " " + multText(e) : "")});
    else {
      std::map<float, std::vector<Type>> by;
      for (auto& [t, e] : list) by[e].push_back(t);
      for (auto& [e, ts] : by) segs.push_back({ts, showMult ? multText(e) : ""});
    }
    for (auto& [ts, txt] : segs) {
      int ew = 9 * (int)ts.size() + 1 + Gfx::textW(txt);
      if (cx > x + 56 && cx + ew > x + w - 8) {
        if ((ly += 11, !room())) return;
        cx = x + 56;
      }
      for (size_t k = 0; k < ts.size(); k++) dot(cx + 3.5f + 9 * k, ly + 3.5f, ts[k]);
      if (!txt.empty()) g.text(cx + 9 * (int)ts.size(), ly, txt, WHITE);
      cx += ew + 5;
    }
    if (!tail.empty()) {
      if (cx > x + 56 && cx + Gfx::textW(tail) > x + w - 8) {
        if ((ly += 11, !room())) return;
        cx = x + 56;
      }
      g.text(cx, ly, tail, tc);
    }
    ly += 12;
  };
  Color red = rgb(0xff8a7a), muted = MUTED;
  row(T("Faible"), mu.weak, true, true, mu.hidden ? (mu.weak.empty() ? "?" : "+?") : !mu.full && mu.weak.empty() ? "?" : "", muted);
  row(T("Absorbe"), mu.absorb, true, false, mu.full ? "" : "?", muted);
  row(T("Résiste"), mu.resist, false, true, mu.full ? "" : "?", muted);
  if (room()) {
    g.text(x + 8, ly, T("Coups"), WHITE);
    auto hit = [&](const char* what, float k) {
      return std::string(what) + (mu.full ? multText(k) : std::string("?"));
    };
    bool hard = mu.full && (mu.phys < 1 || mu.mag < 1);
    g.text(x + 56, ly, hit(T("phys."), mu.phys) + " " + hit(T("mag."), mu.mag), hard ? red : mu.full ? WHITE : muted);
    ly += 14;
  }
  // Attaques de zone qu'elle apprend : un petit damier 3x3 montre la forme de la zone
  std::vector<const Move*> zones;
  for (auto& l : s.learn)
    if (hasMove(l.move)) {
      const Move& m = moveInfo(l.move);
      if (m.damaging() && m.zone != Area::Single && std::find(zones.begin(), zones.end(), &m) == zones.end()) zones.push_back(&m);
    }
  if (!room()) return;
  g.text(x + 8, ly, T("Attaques de zone"), GOLD);
  ly += 11;
  if (zones.empty() && room()) g.text(x + 8, ly, T("aucune"), MUTED), ly += 11;
  for (auto* m : zones) {
    if (!room()) break;
    for (int r = 0; r < 3; r++)
      for (int q = 0; q < 3; q++) {
        bool lit = m->zone == Area::All || (m->zone == Area::Line && r == 1) || (m->zone == Area::Column && q == 1) ||
                   (m->zone == Area::Cross && (r == 1 || q == 1));
        g.rect(x + 8 + q * 3, ly + 1 + r * 3, 2, 2, lit ? rgb(0xff9a4a) : rgb(0x3a4478));
      }
    dot(x + 22.5f, ly + 4.5f, m->type);
    g.text(x + 28, ly, m->name, WHITE);
    ly += 11;
  }
}

// Fiche du bestiaire : dessin, types, statistiques de base et lieux où vivre la rencontre
void Game::drawBestiaryPanel(int x, int y) {
  if (!hasSpecies(bestiarySel_)) return;
  const Species& s = species(bestiarySel_);
  int w = 320 - x - 8;
  bool v = seen.count(s.id) > 0, c = caught.count(s.id) > 0;
  if (v && bestiaryPage_ == 1) return drawBestiaryTactics(g, x, y, w, s, knownMatchup(s, c ? nullptr : &weak));
  std::vector<std::string> places;
  for (auto& mp : maps()) {
    bool here = false;
    for (auto& z : mp.zones)
      here = here || std::find(z.pool.begin(), z.pool.end(), s.id) != z.pool.end() || std::find(z.night.begin(), z.night.end(), s.id) != z.night.end();
    for (auto& b : mp.bosses) here = here || b.id == s.id;
    if (here && mp.theme != Theme::Interieur) places.push_back(mp.name);
  }
  std::string where;
  for (auto& p : places) where += (where.empty() ? "" : ", ") + p;
  auto lines = Gfx::wrap(T("Lieux : ") + (where.empty() ? std::string("inconnus") : where), w - 16);
  auto about = c && !s.desc.empty() ? Gfx::wrap(s.desc, w - 16) : std::vector<std::string>{};  // le mot du Bestiaire, une fois obtenue
  int top = about.empty() ? 104 : 88;  // avec la description : un dessin un peu plus petit
  int room = (184 - y - top - (c ? 42 : 0) - (about.empty() ? 0 : 6 + 11 * (int)about.size())) / 11;  // au-dessus de l'aide
  if (room <= 0) lines.clear();  // longue description : pas de place pour les lieux
  else if ((int)lines.size() > room) {  // trop de lieux : les premiers, puis « … »
    lines.resize((size_t)room);
    lines.back() += "…";
  }
  int h = top + (c ? 42 : 0) + (v ? 11 * (int)lines.size() : 0) + (about.empty() ? 0 : 6 + 11 * (int)about.size());
  g.window(x, y, w, h);
  if (!v) {
    g.text(x + w / 2, y + 40, "???", MUTED, 1);
    g.text(x + w / 2, y + 56, T("Pas encore rencontrée"), MUTED, 1);
    return;
  }
  g.text(x + 8, y + 6, s.name, GOLD);
  g.text(x + w - 8, y + 6, "1/2>", MUTED, 2);  // Gauche/Droite (ou un clic) : la page tactique
  g.text(x + 8, y + 18, typesName(s), MUTED);
  float k = creatureFit(s.id, top == 104 ? 60 : 46);  // les petites créatures deux fois plus grandes
  drawCreature(g, s.id, x + w / 2, y + top - 14 - 16 * k * s.size, k, false, time);
  int ly = y + top - 8;
  if (c) {  // statistiques de base, seulement pour une créature obtenue
    const char* names[] = {"PV", "PM", "Att", "Déf", "Mag", "Rés", "Vit"};
    const float most[] = {140, 80, 100, 100, 100, 100, 100};  // (2026-10-23) une jauge sous chaque chiffre : forte, faible d'un coup d'œil
    const uint32_t col[] = {0x6ad87a, 0x6aa8f0, 0xf08a6a, 0xd8b86a, 0xb88af0, 0x8ad8d8, 0xf0e06a};
    for (int i = 0; i < N_BASE; i++) {
      float bx = x + 8 + (i % 3) * 50, by = ly + (i / 3) * 11;
      g.text(bx, by, std::string(T(names[i])) + " " + std::to_string(s.base[i]), WHITE);
      g.rect(bx, by + 9.5f, 44, 1.5f, rgb(0x03051a, 200));
      g.rect(bx, by + 9.5f, 44 * std::min(1.f, s.base[i] / most[i]), 1.5f, rgb(col[i]));
    }
    ly += 36;
  } else {
    g.text(x + 8, ly, T("Pas encore obtenue."), MUTED);
    ly += 12;
  }
  for (size_t i = 0; i < lines.size(); i++) g.text(x + 8, ly + 6 + i * 11, lines[i], MUTED);
  ly += 6 + 11 * (int)lines.size();
  for (size_t i = 0; i < about.size(); i++) g.text(x + 8, ly + 6 + i * 11, about[i], rgb(0xd8dcf0));
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

// Formation : la case de départ de chaque combattant dans notre moitié de l'échiquier (data.hpp), gardée dans la
// sauvegarde. Devant, on arrive vite au contact ; derrière, une ligne d'alliés protège des ennemis qui avancent.
void Game::formationMenu(int sel) {
  panelMode_ = 4;
  auto fr = front();
  auto cells = formation(fr, true);
  for (size_t i = 0; i < fr.size(); i++) fr[i]->cell = cells[i];  // le placement automatique devient visible et modifiable
  Menu m;
  m.title = T("Formation");
  m.x = 8, m.y = 8, m.w = 132, m.rows = 7;
  for (size_t i = 0; i < fr.size(); i++) {
    FighterP f = fr[i];
    int at = (int)i;
    MenuItem it{f->name(), "", cellName(f->cell) + T(". Entrée : changer de case."), true, [this, f, at] {
                  Menu p;
                  p.title = T("Case de ") + utf8Prefix(f->name(), 10);
                  p.x = 8, p.y = 24, p.w = 132, p.rows = 9;  // à gauche du dessin du damier
                  // Dans l'ordre du dessin (3.4) : colonne par colonne de gauche à droite, de haut en bas ; Haut et Bas
                  // changent de ligne, Gauche et Droite de colonne (adjust), la souris choisit sur le dessin
                  formationPick_ = true;
                  p.onCancel = [this] {
                    formationPick_ = false;
                    menus.pop();
                  };
                  for (int vx = 0; vx < GRID; vx++)
                    for (int row = 0; row < GRID; row++) {
                    int c = cellAt(row, BACK_COL - vx);
                    std::string who;
                    for (auto& o : front())
                      if (o != f && o->cell == c) who = o->name();
                    MenuItem ci{cellName(c), who.empty() ? "" : "·", who.empty() ? T("Case libre.") : T("Occupée par ") + who + T(" : échanger."), true,
                                [this, f, c, at] {
                                  for (auto& o : front())
                                    if (o != f && o->cell == c) o->cell = f->cell;  // échange de cases
                                  f->cell = c;
                                  formationPick_ = false;
                                  menus.pop();
                                  menus.pop();
                                  formationMenu(at);
                                },
                                [this, c] { formationCell_ = c; }};
                    ci.shrink = true;
                    ci.adjust = [this, vx, row](int d) {  // Gauche / Droite : la colonne voisine sur le dessin
                      int nx = std::clamp(vx + d, 0, GRID - 1);
                      Menu& mm = menus.top();
                      mm.sel = nx * GRID + row;
                      mm.top = std::clamp(mm.top, std::max(0, mm.sel - mm.rows + 1), mm.sel);  // la ligne reste visible
                      formationCell_ = cellAt(row, BACK_COL - nx);
                    };
                    p.items.push_back(ci);
                  }
                  p.sel = std::clamp((BACK_COL - cellCol(std::max(0, f->cell))) * GRID + cellRow(std::max(0, f->cell)), 0, N_CELLS - 1);
                  menus.push(p);
                },
                [this, f] { formationCell_ = f->cell; }};
    it.shrink = true;
    m.items.push_back(it);
  }
  if (m.items.empty()) m.items.push_back({T("(personne)"), "", "", false, nullptr});
  m.sel = std::clamp(sel, 0, (int)m.items.size() - 1);
  menus.push(m);
}

// Géométrie du dessin de la formation (drawFormationPanel, posé en 146, 8)
int Game::formationCellAt(int mx, int my) const {
  const int x = 146, y = 8, w = 320 - x - 8, cw = (w - 16) / GRID, ch = 26;
  int vx = (mx - x - 8) / cw, row = (my - y - 20) / ch;
  if (mx < x + 8 || my < y + 20 || vx < 0 || vx >= GRID || row < 0 || row >= GRID) return -1;
  return cellAt(row, BACK_COL - vx);
}

// Dessin de notre moitié de l'échiquier : l'avant à droite, face aux ennemis (comme en combat, 3.4)
void Game::drawFormationPanel(int x, int y) {
  int w = 320 - x - 8, cw = (w - 16) / GRID, ch = 26, h = 34 + GRID * ch + 16;
  g.window(x, y, w, h);
  g.text(x + 8, y + 5, T("Notre camp"), GOLD);
  g.text(x + w - 8, y + 5, T("avant à droite"), MUTED, 2);
  auto fr = front();
  for (int c = 0; c < N_CELLS; c++) {
    int rx = x + 8 + (BACK_COL - cellCol(c)) * cw, ry = y + 20 + cellRow(c) * ch;
    bool lit = c == formationCell_;
    g.rect(rx + 1, ry + 1, cw - 2, ch - 2, lit ? rgb(0xffd34d, 70) : rgb(0x000010, 60));
    for (auto& f : fr)
      if (f->cell == c) {
        g.text(rx + cw / 2, ry + 6, utf8Prefix(f->name(), cw / 6 - 1), f->S().human ? WHITE : rgb(0xb8f0c8), 1);
        g.text(rx + cw / 2, ry + 17, T("N.") + std::to_string(f->lvl), MUTED, 1);
      }
  }
  const char* COLS[GRID] = {T("Arrière"), "", "", T("Avant")};
  for (int k = 0; k < GRID; k++) g.text(x + 8 + k * cw + cw / 2, y + 22 + GRID * ch, COLS[k], MUTED, 1);
}

void Game::companionsMenu(int sel) {
  panelMode_ = 2;  // fiche à droite
  Menu m;
  m.title = T("Compagnons");
  m.x = 8, m.y = 8, m.w = 132, m.rows = 8, m.sel = sel;
  int row = 0;
  for (size_t i = 0; i < team.size(); i++) {
    FighterP h = team[i];
    if (!h->S().human) continue;
    FighterP c = companionOf(team, h);
    int at = row++;
    MenuItem it{h->name(), c ? utf8Prefix(c->name(), 10) : "—",
                c ? (h->companion.lock() == c ? T("Choisi par vous. ") : T("Choisi tout seul (première créature libre). ")) + std::string(T("Entrée : changer."))
                  : T("Aucune créature libre. Entrée : en choisir une."),
                true, [this, h, at] {
                  Menu p;
                  p.title = T("Compagnon de ") + utf8Prefix(h->name(), 9);
                  p.x = 20, p.y = 20, p.w = 132, p.rows = 8;
                  p.items.push_back({T("Automatique"), "", T("La première créature libre de l'équipe."), true, [this, h, at] {
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
                    MenuItem ci{c->name(), T("N.") + std::to_string(c->lvl), with.empty() ? typesName(c->S()) : T("Choisie par ") + with + T(" : elle changera de héros."),
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
  m.title = T("Équipement");
  m.x = 8, m.y = 8, m.w = 132, m.rows = 9, m.sel = sel;
  for (size_t i = 0; i < team.size(); i++) {
    FighterP f = team[i];
    int n = 0;
    for (auto& g : f->gear) n += !g.empty();
    m.items.push_back({f->name(), n ? std::to_string(n) + "/3" : "", T("Entrée : changer son équipement."), true,
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
    MenuItem it{ok ? (id.empty() ? std::string(T("(rien)")) : item(id).name) : T("(héros seulement)"), "",
                ok ? (id.empty() ? T("Entrée : choisir dans le sac.") : item(id).bonusText() + T(". Entrée : changer ou retirer."))
                   : T("Seuls les héros portent armes et armures."),
                ok, [this, f, who, s] { gearPick(f, who, s); }};
    it.shrink = true;
    if (id.empty()) it.icon = "vide";  // (2026-10-23) une case vide en pointillés ; un objet porté a son icône
    m.items.push_back(it);
  }
  m.sel = 1 + 2 * std::clamp(sel, 0, N_GEAR - 1);
  int sockets = 0, set = 0;
  for (int s = 0; s < N_GEAR; s++) {
    const std::string& id = f->gear[(size_t)s];
    if (!id.empty() && hasItem(id)) sockets += gemSockets(item(id));
    set += (int)f->gems[(size_t)s].size();
  }
  m.items.push_back({T("Gemmes"), sockets ? std::to_string(set) + "/" + std::to_string(sockets) : "",
                     sockets ? T("Sertir des gemmes dans les chatons de l'équipement porté.") : T("Il faut porter un équipement pour sertir des gemmes."), sockets > 0,
                     [this, f, who] { gemMenu(f, who); }});
  m.items.push_back({T("Retour"), "", "", true, [this] { menus.pop(); }});
  setIcons(m, {{T("Gemmes"), "ic_gemme_cristal"}, {T("Retour"), "retour"}});
  m.items[m.items.size() - 2].sep = true;
  teamPanelSel_ = who;
  menus.push(m);
}

// Gemmes (2.7) : chaque chaton de l'équipement porté, puis la gemme du sac à y mettre (ou la retirer)
void Game::gemMenu(FighterP f, int who, int sel) {
  Menu m;
  m.title = T("Gemmes");
  m.x = 8, m.y = 8, m.w = 132, m.rows = 8;
  auto reopen = [this, f, who](int at) {
    menus.pop();
    menus.pop();
    gemMenu(f, who, at);
  };
  int row = 0;
  for (int s = 0; s < N_GEAR; s++) {
    const std::string& gid = f->gear[(size_t)s];
    if (gid.empty() || !hasItem(gid)) continue;
    int n = gemSockets(item(gid));
    for (int k = 0; k < n; k++, row++) {
      auto& slot = f->gems[(size_t)s];
      std::string cur = k < (int)slot.size() ? slot[(size_t)k] : "";
      MenuItem it{std::string(gearSlotName(s)) + " : " + (cur.empty() ? std::string(T("(vide)")) : item(cur).name), "",
                  cur.empty() ? T("Entrée : choisir une gemme du sac.") : itemHelp(item(cur)), true, [this, f, s, k, cur, reopen, row] {
                    Menu p;
                    p.title = T("Sertir");
                    p.x = 8, p.y = 8, p.w = 132, p.rows = 7;
                    auto put = [this, f, s, k, cur, reopen, row](const std::string& id) {
                      auto& v = f->gems[(size_t)s];
                      if (!cur.empty()) {
                        items[cur]++;
                        v.erase(v.begin() + k);
                      }
                      if (!id.empty()) {
                        items[id]--;
                        v.insert(v.begin() + std::min(k, (int)v.size()), id);
                      }
                      int lostHp = f->mhp - f->hp, lostMp = f->mmp - f->mp;
                      f->recalc();
                      f->hp = std::clamp(f->mhp - lostHp, f->alive() ? 1 : 0, f->mhp);
                      f->mp = std::clamp(f->mmp - lostMp, 0, f->mmp);
                      audio::play("achat");
                      reopen(row);
                    };
                    for (auto& d : allItems()) {
                      int have = items.count(d.id) ? items[d.id] : 0;
                      if (!d.gem || have <= 0) continue;
                      std::string id = d.id;
                      MenuItem g{d.name, T("×") + std::to_string(have), itemHelp(d), true, [put, id] { put(id); }};
                      g.shrink = true;
                      p.items.push_back(g);
                    }
                    if (!cur.empty()) p.items.push_back({T("Retirer"), "", T("Remettre ") + item(cur).name + T(" dans le sac."), true, [put] { put(""); }});
                    if (p.items.empty()) p.items.push_back({T("(aucune gemme)"), "", T("Les gemmes se trouvent dans les boutiques, les coffres et sur les boss."), false, nullptr});
                    menus.push(p);
                  }};
      it.shrink = true;
      m.items.push_back(it);
    }
  }
  m.sel = std::clamp(sel, 0, std::max(0, (int)m.items.size() - 1));
  m.items.push_back({T("Retour"), "", "", true, [this] { menus.pop(); }});
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
    for (auto& g : trimGems(*f)) items[g]++;  // gemmes en trop (moins de chatons) : rendues au sac
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
    // Comparaison (2.8) : statistiques gagnées ou perdues par rapport à l'objet porté, et chatons à gemmes
    std::string cmp;
    const std::string& cur = f->gear[(size_t)s];
    for (int k = 0; k < N_GEAR_STATS; k++) {
      int diff = d.bonus[k] - (cur.empty() ? 0 : item(cur).bonus[k]);
      if (diff) cmp += (cmp.empty() ? "" : ", ") + std::string(diff > 0 ? "+" : "") + std::to_string(diff) + (k >= GS_ACC ? " % " : " ") + gearStatName(k);
    }
    int sockets = gemSockets(d), now = cur.empty() ? 0 : gemSockets(item(cur));
    std::string help = (cmp.empty() ? std::string(T("Mêmes statistiques")) : T("Par rapport à l'actuel : ") + cmp) + ". " +
                       std::to_string(sockets) + (sockets > 1 ? T(" chatons") : T(" chaton")) + (sockets < now ? T(" (moins qu'avant : des gemmes reviendront au sac)") : "") +
                       ". " + itemHelp(d);
    MenuItem it{d.name, T("×") + std::to_string(n), help, true, [equip, id] { equip(id); }};
    it.shrink = true;
    m.items.push_back(it);
  }
  if (!f->gear[(size_t)s].empty()) m.items.push_back({T("Retirer"), "", T("Remettre ") + item(f->gear[(size_t)s]).name + T(" dans le sac."), true, [equip] { equip(""); }});
  if (m.items.empty()) m.items.push_back({T("(rien dans le sac)"), "", T("Les boutiques vendent armes, armures et accessoires."), false, nullptr});
  menus.push(m);
}

// Journal des quêtes : celles qui ont commencé ; l'aide montre l'étape où l'on en est
void Game::journalMenu() {
  panelMode_ = 0;
  Menu m;
  m.title = T("Journal");
  m.x = 8, m.y = 8, m.w = 236, m.rows = 9;  // (236 : la place des icônes)
  if (std::string adv = adviceText(); !adv.empty()) m.items.push_back({T("Prochain gardien"), "", adv, true, nullptr});  // conseil (2.8)
  if (!inExpedition())
    m.items.push_back({T("Progression"), "", T("Ce que vous avez déjà accompli : quêtes, primes, dresseurs, coffres, Bestiaire…"), true, [this] { progressMenu(); }});
  if (!inExpedition())
    m.items.push_back({T("Exploits"), std::to_string(exploitsDone()) + "/" + std::to_string(exploitCount()),
                       T("Des défis pour ceux qui veulent tout faire : finir les actes, remplir le Bestiaire, devenir champion…"), true,
                       [this] { exploitsMenu(); }});
  for (auto& q : quests()) {
    std::string id = jget<std::string>(q, "id", "");
    if (!has(questFlag(id))) continue;
    bool done = has(questFlag(id, true));
    std::string help = tr(jget<std::string>(q, "fin_texte", "Quête terminée."));
    if (!done) {
      Json steps = q.value("etapes", Json::array());
      for (auto& s : steps)  // la dernière étape dont la condition est vraie
        if (!s.contains("si") || checkCond(s["si"])) help = tr(jget<std::string>(s, "texte", ""));
    }
    MenuItem it{tr(jget<std::string>(q, "nom", id)), done ? T("terminée") : T("en cours"), tr(jget<std::string>(q, "lieu", "")) + " — " + help, true, nullptr};
    it.color = done ? 0xaab3d8 : 0;
    it.icon = done ? "coche" : "parchemin";
    it.shrink = true;
    m.items.push_back(it);
  }
  for (auto& b : bounties()) {  // primes acceptées, pas encore touchées
    std::string id = jget<std::string>(b, "id", "");
    if (!has(bountyFlag(id, 0)) || has(bountyFlag(id, 2))) continue;
    bool beaten = has(bountyFlag(id, 1));
    MenuItem it{T("Prime : ") + tr(jget<std::string>(b, "nom", id)), beaten ? T("Prime !") : T("en chasse"),
                tr(jget<std::string>(b, "lieu", "")) + " — " +
                    (beaten ? std::string(T("Vaincu : toucher la prime à un tableau d'annonces.")) : tr(jget<std::string>(b, "description", ""))),
                true, nullptr};
    it.icon = "viser";
    it.shrink = true;
    m.items.push_back(it);
  }
  if (!inExpedition()) {  // (2026-10-23) venus du menu de pause ; au retour, le Journal retrouve son affichage
    auto back = [this] {
      panelMode_ = 0;
      menus.pop();
    };
    int n = newDiscussions();
    MenuItem talk{T("Discussions"), n ? std::to_string(n) + T(" nouv.") : "", T("Ce que se disent les héros au fil de l'aventure."), true, [this, back] {
                    discussionsMenu();
                    menus.top().onCancel = back;
                  }};
    MenuItem best{T("Bestiaire"), "", T("Les créatures rencontrées et obtenues, et leur fiche."), true, [this, back] {
                    bestiaryMenu();
                    menus.top().onCancel = back;
                  }};
    size_t at = 0;  // après Prochain gardien, Progression et Exploits, avant les quêtes
    while (at < m.items.size() && (m.items[at].label == T("Prochain gardien") || m.items[at].label == T("Progression") || m.items[at].label == T("Exploits"))) at++;
    m.items.insert(m.items.begin() + (long)at, {talk, best});
    if (m.items.size() > at + 2) m.items.insert(m.items.begin() + (long)at + 2, menuHeader(T("Quêtes")));  // la liste des quêtes à part
  }
  setIcons(m, {{T("Prochain gardien"), "carte"}, {T("Progression"), "etoile"}, {T("Exploits"), "coupe"}, {T("Discussions"), "bulle"},
               {T("Bestiaire"), "ic_livre"}});
  if (m.items.empty()) m.items.push_back({T("(aucune quête)"), "", T("Parlez aux habitants : certains ont besoin d'aide."), false, nullptr});
  m.onCancel = [this] {
    panelMode_ = 1;
    menus.pop();
  };
  menus.push(m);
}

// Progression (2026-10-20) : des compteurs, tous calculés d'après les données et les drapeaux (rien d'enregistré en plus)
void Game::progressMenu() {
  Menu m;
  m.title = T("Progression");
  m.x = 8, m.y = 8, m.w = 230, m.rows = 10;
  auto line = [&](const char* name, int done, int total, const char* help) {
    if (total <= 0) return;
    MenuItem it{name, std::to_string(done) + " / " + std::to_string(total), help, true, nullptr};
    if (done >= total) it.color = 0xffe066;  // tout est fait : en or
    it.bar = float(done) / total;
    m.items.push_back(it);
  };
  int q = 0, qd = 0, b = 0, bd = 0, trn = 0, trd = 0, rv = 0, rvd = 0, ch = 0, chd = 0, hid = 0, hidd = 0, sp = 0, cr = 0, talk = 0;
  for (auto& x : quests()) {
    std::string id = jget<std::string>(x, "id", "");
    q++, qd += has(questFlag(id, true));
  }
  for (auto& x : bounties()) b++, bd += has(bountyFlag(jget<std::string>(x, "id", ""), 2));
  for (auto& map : maps()) {
    for (auto& n : map.npcs)
      if (n.sight > 0 && !n.sightUntil.empty()) {
        trn++, trd += has(n.sightUntil);
        const Json* ev = findEvent(n.event);
        if (ev && ev->dump().find(n.sightUntil + "_revanche") != std::string::npos) rv++, rvd += has(n.sightUntil + "_revanche");
      }
    for (auto& c : map.chests) {
      bool open = has("coffre:" + map.id + ":" + std::to_string(c.x) + ":" + std::to_string(c.y));
      if (c.cond.is_null()) ch++, chd += open;
      else hid++, hidd += open;  // trésors cachés (cartes au trésor)
    }
  }
  for (auto& s : allSpecies()) sp += !s.human && s.shape != Shape::Boss;
  noteBestiary();
  int caughtN = 0;
  for (auto& id : caught) caughtN += hasSpecies(id) && !species(id).human && species(id).shape != Shape::Boss;
  for (auto* v : {&team, &reserve})
    for (auto& f : *v) cr += f->chroma;
  for (auto& f : flags) talk += f.rfind("discussion:", 0) == 0;
  line(T("Quêtes terminées"), qd, q, T("Les quêtes du Journal, rendues à ceux qui les ont données."));
  line(T("Primes touchées"), bd, b, T("Les monstres des tableaux d'annonces, vaincus et payés."));
  line(T("Dresseurs battus"), trd, trn, T("Les dresseurs qui vous défient quand vous passez devant eux."));
  line(T("Revanches gagnées"), rvd, rv, T("Après le Seigneur des Orages, les dresseurs battus proposent une revanche."));
  line(T("Coffres ouverts"), chd, ch, T("Tous les coffres du monde, éclats de brume compris."));
  line(T("Créatures obtenues"), caughtN, sp, T("Le Bestiaire : les créatures capturées ou reçues (sans les humains ni les boss)."));
  line(T("Poissons pêchés"), fishCaught(), fishSpecies(), T("Le carnet du maître pêcheur du port."));
  line(T("Discussions écoutées"), talk, discussionCount(), T("Échap > Journal > Discussions : ce que se disent les héros."));
  line(T("Trésors cachés"), hidd, hid, T("Les trésors des cartes de Corbin, au port : chacun contient la carte du suivant."));
  line(T("Joueurs de cartes battus"), cardsWon(), cardFoeCount(), T("Les joueurs de Cartes des gardiens, dans les auberges et les maisons."));
  line(T("Cartes des gardiens"), (int)cardCollection().size(), cardTotal(), T("Chaque créature obtenue donne sa carte ; les cartes des boss se gagnent aux joueurs."));
  line(T("Exploits"), exploitsDone(), exploitCount(), T("Journal > Exploits : les défis pour ceux qui veulent tout faire."));
  MenuItem c{T("Créatures chromatiques"), std::to_string(cr), T("Les créatures aux couleurs rares de votre équipe et de votre réserve."), true, nullptr};
  m.items.push_back(c);
  m.onCancel = [this] { menus.pop(); };
  menus.push(m);
}

void Game::teamMenu() { teamMenuAt(0); }
void Game::teamMenuAt(int sel) {
  panelMode_ = 2;
  Menu m;
  m.title = swapFrom_ >= 0 ? T("Échanger avec…") : T("Équipe");
  m.x = 8, m.y = 8, m.w = 132, m.rows = 9;
  m.sel = sel;
  auto fv = front();
  for (size_t i = 0; i < team.size(); i++) {
    auto& f = team[i];
    bool fr = std::find(fv.begin(), fv.end(), f) != fv.end();
    m.items.push_back({(int)i == swapFrom_ ? "» " + f->name() : f->name(), fr ? T("Front") : f->alive() ? T("Rés.") : T("K.O."),
                       T("Entrée sur deux membres pour les échanger. Les 3 premiers valides combattent."), true,
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
    m.items.push_back({f->name(), std::to_string(f->hp) + "/" + std::to_string(f->mhp) + "  " + std::to_string(f->mp) + T("PM"), "",
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
  m.title = T("Objets  ·  ") + std::to_string(gold) + T(" or");
  m.x = 8, m.y = 8, m.w = 170, m.rows = 7;
  m.sel = sel;
  m.onCancel = [this] {
    panelMode_ = 1;
    menus.pop();
  };
  // Rangé par catégories (2.8) : soins, capture, gemmes, équipement, matériaux, objets importants
  auto category = [](const ItemDef& d) {
    if (d.key) return 5;
    if (d.gem) return 2;
    if (d.slot >= 0) return 3;
    if (d.capture > 0) return 1;
    if (d.healHp || d.healMp || d.revive || d.cure) return 0;
    return 4;
  };
  static const char* CATS[] = {"Soins", "Capture", "Gemmes", "Équipement", "Matériaux et trophées", "Objets importants"};
  std::vector<const ItemDef*> order;
  for (auto& d : allItems())
    if (items.count(d.id) && items[d.id] > 0) order.push_back(&d);
  std::stable_sort(order.begin(), order.end(), [&](const ItemDef* a, const ItemDef* b) { return category(*a) < category(*b); });
  int lastCat = -1;
  for (const ItemDef* dp : order) {
    const ItemDef& d = *dp;
    int n = items[d.id];
    if (category(d) != lastCat) m.items.push_back(menuHeader(T(CATS[lastCat = category(d)])));
    std::string id = d.id;
    int myIdx = (int)m.items.size();
    std::string note = d.slot >= 0 ? T(" (à porter : Échap > Équipement)") : "";
    m.items.push_back({d.name, T("×") + std::to_string(n), itemHelp(d) + note, d.field,
                       [this, id, myIdx] {
                         pickMember(T("Sur qui ?"), [id](const Fighter& f) {
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
                           notice(item(id).name + T(" utilisé sur ") + f.name() + ".");
                           menus.pop();
                           itemMenuAt(myIdx);
                         });
                       }});
  }
  if (m.items.empty()) m.items.push_back({T("(sac vide)"), "", "", false, nullptr});
  if (m.sel < (int)m.items.size() && m.items[(size_t)m.sel].header) m.sel++;  // jamais sur un titre de catégorie
  m.sel = std::clamp(m.sel, 0, (int)m.items.size() - 1);
  menus.push(m);
}

void Game::magicMenu() {
  panelMode_ = 0;  // la liste prend la place du résumé de l'équipe
  Menu m;
  m.title = T("Qui lance le sort ?");
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
    m.items.push_back({f->name(), std::to_string(f->mp) + T(" PM"), heal.empty() ? T("Aucun sort utilisable hors combat.") : "", f->alive() && !heal.empty(),
                       [this, caster, heal] {
                         Menu s;
                         s.title = T("Sorts de ") + caster->name();
                         s.x = 24, s.y = 40, s.w = 170, s.rows = 5;
                         for (auto& id : heal) {
                           const Move& mv = moveInfo(id);
                           s.items.push_back({mv.name, std::to_string(mv.cost) + T(" PM"), moveHelp(mv, caster.get()), caster->mp >= mv.cost, [this, caster, id] {
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
                                                  notice(caster->name() + T(" lance ") + mv.name + ".");
                                                  menus.clear();
                                                  panelMode_ = 0;
                                                };
                                                if (mv.target == Target::AllAllies) cast(nullptr);
                                                else
                                                  pickMember(T("Sur qui ?"), [&mv](const Fighter& f) {
                                                    return mv.kind == Kind::Revive ? !f.alive() : f.alive() && f.hp < f.mhp;
                                                  }, [cast](Fighter& f) { cast(&f); });
                                              }});
                         }
                         for (size_t k = 0; k < heal.size() && k < s.items.size(); k++) s.items[k].icon = typeDot(moveInfo(heal[k]).type);
                         menus.push(s);
                       }});
  }
  menus.push(m);
}

void Game::shopMenu(const std::vector<std::string>& stock) { shopMenuAt(stock, 0); }
// Boutique : pour un équipement survolé, chaque membre qui peut le porter et ce qu'il gagnerait (ou perdrait) par
// rapport à ce qu'il porte ; rien pour un objet ordinaire
void Game::drawShopPanel(int x, int y) {
  if (shopHover_.empty() || !hasItem(shopHover_)) return;
  const ItemDef& d = item(shopHover_);
  if (d.slot < 0) return;
  std::vector<std::pair<std::string, std::vector<std::pair<std::string, int>>>> rows;  // nom, (statistique, écart)
  for (auto& f : team) {
    if (!canEquip(*f, d) || rows.size() >= 3) continue;  // trois au plus : l'avis « Acheté » s'affiche juste dessous
    const std::string& cur = f->gear[(size_t)d.slot];
    std::vector<std::pair<std::string, int>> diff;
    for (int k = 0; k < N_GEAR_STATS; k++)
      if (int v = d.bonus[k] - (cur.empty() ? 0 : item(cur).bonus[k])) diff.push_back({gearStatName(k), v});
    rows.push_back({cur == d.id ? f->name() + T(" (le porte)") : f->name(), diff});
  }
  if (rows.empty()) return;
  int w = 200, h = 8 + 11 * (int)rows.size();
  g.window(x, y, w, h);
  for (size_t i = 0; i < rows.size(); i++) {
    float ly = y + 4 + i * 11;
    std::string name = utf8Prefix(rows[i].first, 14);
    g.text(x + 8, ly, name, rgb(0xd8def2));
    float rx = x + w - 8, stop = x + 8 + Gfx::textW(name) + 6;  // les écarts, de droite à gauche, sans toucher le nom
    if (rows[i].second.empty()) g.text(rx, ly, "=", rgb(0xaab3d8), 2);
    for (auto it = rows[i].second.rbegin(); it != rows[i].second.rend(); ++it) {
      std::string s = (it->second > 0 ? "+" : "") + std::to_string(it->second) + " " + utf8Prefix(it->first, 3);
      if (rx - Gfx::textW(s) < stop) break;
      g.text(rx, ly, s, it->second > 0 ? rgb(0x6ad87a) : rgb(0xf06a5a), 2);
      rx -= Gfx::textW(s) + 6;
    }
  }
}

void Game::shopMenuAt(const std::vector<std::string>& stock, int sel) {
  panelMode_ = 5;  // (2026-10-23) sous la liste : ce que l'équipement survolé changerait pour chacun
  shopHover_.clear();
  Menu m;
  m.title = T("Boutique  ·  ") + std::to_string(gold) + T(" or");
  m.x = 60, m.y = 20, m.w = 200, m.rows = 7;
  m.sel = sel;
  for (size_t i = 0; i < stock.size(); i++) {
    const ItemDef& d = item(stock[i]);
    std::string id = d.id;
    int have = items.count(id) ? items[id] : 0;
    m.items.push_back({d.name, std::to_string(d.price) + T(" or"), itemHelp(d) + T(" (vous en avez ") + std::to_string(have) + ")", true,
                       [this, stock, id, i] {
                         const ItemDef& d = item(id);
                         if (gold < d.price) {
                           audio::play("refus");
                           notice(T("Pas assez d'or."));
                           return;
                         }
                         audio::play("achat");
                         gold -= d.price;
                         items[id]++;
                         notice(T("Acheté : ") + d.name + ".");
                         menus.pop();
                         shopMenuAt(stock, (int)i);
                       }});
    m.items.back().hover = [this, id] { shopHover_ = id; };
  }
  m.items.push_back({T("Vendre…"), "", T("Vendre vos objets à la moitié de leur prix."), true, [this, stock] { sellMenu(stock, 0); }});
  m.items.push_back({T("Quitter"), "", "", true, [this] { menus.clear(); }});
  setIcons(m, {{T("Vendre…"), "piece"}, {T("Quitter"), "porte"}});  // alignés sur les icônes des objets
  m.sel = std::clamp(sel, 0, (int)m.items.size() - 1);
  menus.push(m);
}

// Vendre : chaque objet du sac (sauf les objets de quête) rapporte la moitié de son prix
void Game::sellMenu(const std::vector<std::string>& stock, int sel) {
  panelMode_ = 0;  // pas de comparaison en vendant (le retour reconstruit la boutique)
  Menu m;
  m.title = T("Vendre  ·  ") + std::to_string(gold) + T(" or");
  m.x = 60, m.y = 20, m.w = 200, m.rows = 7;
  for (auto& d : allItems()) {
    auto have = items.find(d.id);
    if (have == items.end() || have->second <= 0 || d.key || d.price / 2 <= 0) continue;
    std::string id = d.id;
    int at = (int)m.items.size();
    MenuItem it{d.name, "+" + std::to_string(d.price / 2) + T(" or"), T("Vous en avez ") + std::to_string(have->second) + ". " + itemHelp(d), true, [this, stock, id, at] {
                  items[id]--;
                  gold += item(id).price / 2;
                  audio::play("achat");
                  notice(T("Vendu : ") + item(id).name + ".");
                  menus.pop();
                  sellMenu(stock, at);
                }};
    it.shrink = true;
    m.items.push_back(it);
  }
  if (m.items.empty()) m.items.push_back({T("(rien à vendre)"), "", "", false, nullptr});
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
    // Grande équipe (plus de 6) : une ligne par membre (le front en or), pour laisser la place à l'aide en bas
    bool compact = team.size() > 6;
    int rowH = compact ? 11 : 22;
    int h = 22 + (int)team.size() * rowH + (compact ? 2 : 6);
    g.window(x, y, 320 - x - 8, h);
    g.text(x + 8, y + 5, T("Équipe"), GOLD);
    g.text(320 - 16, y + 5, std::to_string(gold) + T(" or"), WHITE, 2);
    auto fr = front();
    for (size_t i = 0; i < team.size(); i++) {
      auto& f = team[i];
      float ry = y + 20 + i * rowH;
      bool isFront = std::find(fr.begin(), fr.end(), f) != fr.end();
      if (compact) {
        g.text(x + 8, ry, utf8Prefix(f->name(), 12), !f->alive() ? rgb(0xff7b6b) : isFront ? GOLD : WHITE);
        g.text(x + 86, ry, T("N.") + std::to_string(f->lvl), MUTED);
        g.text(320 - 16, ry, std::to_string(f->hp) + "/" + std::to_string(f->mhp) + T(" PV"), WHITE, 2);
        continue;
      }
      g.text(x + 8, ry, f->name(), f->alive() ? WHITE : rgb(0xff7b6b));
      g.text(x + 86, ry, T("N.") + std::to_string(f->lvl), MUTED);
      g.text(320 - 16, ry, isFront ? T("Front") : T("Rés."), isFront ? GOLD : MUTED, 2);
      g.text(x + 8, ry + 10, T("PV ") + std::to_string(f->hp) + "/" + std::to_string(f->mhp), WHITE);
      g.text(x + 86, ry + 10, T("PM ") + std::to_string(f->mp) + "/" + std::to_string(f->mmp), WHITE);
    }
    return;
  }
  if (sel >= (int)team.size()) return;
  const Fighter& f = *team[sel];
  int w = 320 - x - 8;
  std::string t, s;
  for (auto& id : f.techs()) t += (t.empty() ? "" : ", ") + moveInfo(id).name;
  for (auto& id : f.spells()) s += (s.empty() ? "" : ", ") + moveInfo(id).name;
  auto techLines = Gfx::wrap(T("Techniques : ") + t, w - 16), magicLines = s.empty() ? std::vector<std::string>{} : Gfx::wrap(T("Magie : ") + s, w - 16);
  for (auto* v : {&techLines, &magicLines})  // deux lignes au plus chacune (en anglais, les noms sont parfois plus longs)
    if (v->size() > 2) {
      v->resize(2);
      v->back() = utf8Prefix(v->back(), std::max(0, (w - 16) / 6 - 1)) + "…";
    }
  auto limitLines = f.S().limit.empty() ? std::vector<std::string>{} : Gfx::wrap(T("Limite : ") + moveInfo(f.S().limit).name, w - 16);
  int lines = (int)techLines.size() + (int)magicLines.size() + (int)limitLines.size();
  g.window(x, y, w, std::min(SCREEN_H - y - 2, 34 + 5 * 11 + 3 + 4 * 11 + 3 + lines * 11 + 6));
  g.text(x + 8, y + 6, f.name(), GOLD);
  g.text(x + w - 8, y + 6, T("Niveau ") + std::to_string(f.lvl), WHITE, 2);
  if (const Rules::Talent* tl = talentOf(f.S())) g.text(x + 8, y + 19, T("Talent : ") + tl->name, rgb(0xffd34d));  // (Aide > Talents)
  else g.text(x + 8, y + 19, f.S().human ? utf8Prefix(f.S().role, 24) : typeName(f.S().type), MUTED);
  if (f.S().human) drawHuman(g, look(f.S().look), x + w - 40, y + 32, 2, DOWN, 0, false);  // sous la ligne du rôle
  else {  // à la taille de la place à droite des premières lignes (les petites créatures deux fois plus grandes)
    float k = creatureFit(f.sp, 44, 56);
    drawCreature(g, f.sp, x + w - 34, y + 74 - 16 * k * f.S().size, k, false, time, f.chroma);
  }
  int ly = y + 34;
  auto row = [&](const std::string& a, const std::string& b) {
    g.text(x + 8, ly, a, MUTED);
    g.text(x + 54, ly, b, WHITE);
    ly += 11;
  };
  row(T("Type"), typesName(f.S()));
  auto gauge = [&](float part, uint32_t c) {  // (2026-10-23) une fine jauge sous le nombre de la ligne d'avant
    part = std::clamp(part, 0.f, 1.f);
    g.rect(x + 54, ly - 1, 48, 2, rgb(0x03051a, 200));
    g.rect(x + 54, ly - 1, 48 * part, 2, rgb(c));
  };
  float hpPart = f.mhp ? (float)f.hp / f.mhp : 0;
  row(T("PV"), std::to_string(f.hp) + "/" + std::to_string(f.mhp));
  gauge(hpPart, hpPart > .5f ? 0x6ad87a : hpPart > .25f ? 0xffd34d : 0xf06a5a);
  row(T("PM"), std::to_string(f.mp) + "/" + std::to_string(f.mmp));
  gauge(f.mmp ? (float)f.mp / f.mmp : 0, 0x6aa8f0);
  row(T("Exp."), std::to_string(f.xp) + "/" + std::to_string(f.need()));
  gauge(f.need() ? (float)f.xp / f.need() : 0, 0xc89af0);
  row(T("Pas"), std::to_string(fighterMoveRange(f)) + T(" cases") + (speciesFlies(f.S()) ? T(", vole") : ""));  // échiquier
  ly += 3;
  // Statistiques sur deux colonnes
  auto pair = [&](const char* a, int va, const char* b, const std::string& vb) {
    g.text(x + 8, ly, a, MUTED);
    g.text(x + 76, ly, std::to_string(va), WHITE, 2);
    g.text(x + 84, ly, b, MUTED);
    g.text(x + w - 8, ly, vb, WHITE, 2);
    ly += 11;
  };
  pair(T("Attaque"), f.atk, T("Défense"), std::to_string(f.def));
  pair(T("Magie"), f.mag, T("Résist."), std::to_string(f.res));
  pair(T("Vitesse"), f.spd, T("Précis."), std::to_string(f.acc) + "%");
  pair(T("Esquive"), f.eva, T("Critique"), std::to_string(f.crit) + "%");
  ly += 3;
  for (auto& l : techLines) g.text(x + 8, ly, l, WHITE), ly += 11;
  for (auto& l : magicLines) g.text(x + 8, ly, l, rgb(0x9fd8ff)), ly += 11;
  for (auto& l : limitLines) g.text(x + 8, ly, l, rgb(0xff9ad0)), ly += 11;  // un nom long passe à la ligne
}

// ---------------------------------------------------------------------------
// Carte du monde (3.3) : les lieux extérieurs (champ « carte_monde » des cartes) qu'on peut atteindre d'ici par des
// passages ouverts ; l'acte 2 reste caché tant que le bateau ne traverse pas. Rien n'est enregistré : tout se déduit
// des passages et des drapeaux.
// ---------------------------------------------------------------------------
std::vector<bool> Game::worldOpen() const {
  int here = mapId;
  if (M().theme == Theme::Interieur)
    for (auto& w : M().warps)
      if (int t = mapIndex(w.map); t >= 0 && maps()[t].worldX >= 0) here = t;
  std::vector<bool> open(maps().size(), false);
  std::vector<int> todo;
  if (here >= 0 && maps()[here].worldX >= 0) open[here] = true, todo.push_back(here);
  while (!todo.empty()) {
    int c = todo.back();
    todo.pop_back();
    for (auto& w : maps()[c].warps) {
      int t = mapIndex(w.map);
      if (t < 0 || open[t] || maps()[t].worldX < 0) continue;
      if (!w.condition.is_null() && !checkCond(w.condition)) continue;
      open[t] = true;
      todo.push_back(t);
    }
  }
  return open;
}

std::vector<int> Game::travelTargets() const {
  std::vector<int> out;
  if (inExpedition() || (online_ && online_->connected()) || pilot_) return out;
  auto open = worldOpen();
  for (size_t i = 0; i < maps().size(); i++) {
    if (!open[i] || (int)i == mapId || !visited.count(maps()[i].id)) continue;
    for (auto& b : maps()[i].buildings)
      if (b.kind == "soin") {
        out.push_back((int)i);
        break;
      }
  }
  return out;
}

void Game::drawWorldMap() {
  // Carte illustrée (2026-10-21) : parchemin taché et bordé, rose des vents ; sous chaque lieu découvert, un petit
  // dessin de son paysage (arbres, sommets, neige, entrée de grotte, vagues pour un lieu au bord de l'eau) ; les
  // chemins en pointillés ; les noms des villages, du lieu où l'on est et de la destination choisie, placés sans se
  // chevaucher (les autres lieux n'ont que leur dessin).
  int here = mapId;
  if (M().theme == Theme::Interieur)
    for (auto& w : M().warps)
      if (int t = mapIndex(w.map); t >= 0 && maps()[t].worldX >= 0) here = t;
  auto targets = travelTargets();
  auto open = worldOpen();  // passage fermé : on ne voit pas encore ce qu'il y a derrière
  const float X0 = 22, Y0 = 34, WW = 276, HH = 168;
  auto P = [&](const MapDef& m) { return Pt{X0 + m.worldX / 100 * WW, Y0 + m.worldY / 100 * HH}; };
  static std::vector<float> wet;  // part d'eau de chaque carte (calculée une fois)
  if (wet.size() != maps().size()) {
    wet.assign(maps().size(), 0.f);
    for (size_t i = 0; i < maps().size(); i++) {
      int n = 0, tot = 0;
      for (auto& r : maps()[i].rows)
        for (char c : r) n += c == '~', tot++;
      wet[i] = tot ? float(n) / tot : 0.f;
    }
  }
  const Color INK = rgb(0x4a321c), INK2 = rgb(0x6a4a2a);
  g.newLayer();
  g.rect(g.left(), 0, g.fullW, SCREEN_H, rgb(0x050816, 200));
  g.rect(8, 8, 304, 224, rgb(0x3a2a1a));
  g.gradV(10, 10, 300, 220, rgb(0xf0e4c4), rgb(0xd6bf8a));  // parchemin
  for (int i = 0; i < 9; i++) {                              // taches et usure
    float x = 20 + (i * 97) % 270, y = 22 + (i * 61) % 190;
    g.ellipse(x, y, 10 + i % 4 * 6, 6 + i % 3 * 4, rgb(0xb89a64, 26));
  }
  g.frame(13, 13, 294, 214, rgb(0x8a6a44));  // double filet
  g.frame(15, 15, 290, 210, rgb(0xa8885a));
  for (float cx : {15.f, 305.f})
    for (float cy : {15.f, 225.f}) g.ellipse(cx, cy, 3, 3, rgb(0x8a6a44));  // coins
  g.text(160, 18, T("Carte du monde"), INK, 1, false);
  // Rose des vents (en bas à droite)
  {
    float cx = 284, cy = 196;
    g.ellipse(cx, cy, 11, 11, rgb(0xb89a64, 90));
    g.poly({{cx, cy - 13}, {cx + 3, cy}, {cx, cy + 13}, {cx - 3, cy}}, INK2);
    g.poly({{cx - 13, cy}, {cx, cy - 3}, {cx + 13, cy}, {cx, cy + 3}}, rgb(0x8a6a44));
    g.poly({{cx, cy - 13}, {cx + 3, cy}, {cx - 3, cy}}, rgb(0xb8402a));
    g.text(cx, cy - 23, "N", INK, 1, false);
  }
  // Paysages des lieux découverts
  for (size_t i = 0; i < maps().size(); i++) {
    if (!open[i]) continue;
    const MapDef& m = maps()[i];
    Pt p = P(m);
    int h = int(i * 37 + 11);
    if (wet[i] > .12f)  // au bord de l'eau : une tache bleue et des vaguelettes
      for (int k = 0; k < 3; k++) {
        float wx = p.x + (k - 1) * 9 + (h >> k) % 5 - 2, wy = p.y + 7 + (k % 2) * 3;
        g.ellipse(wx, wy, 11, 5, rgb(0x7fb0d8, 120));
        g.rect(wx - 3, wy - 1, 3, 1, rgb(0x4a80b0, 200)), g.rect(wx, wy, 3, 1, rgb(0x4a80b0, 200));
      }
    if (m.theme == Theme::Foret)  // de petits arbres
      for (int k = 0; k < 5; k++) {
        float tx = p.x - 12 + k * 6, ty = p.y - 3 + (k % 2) * 4 - (k == 2) * 2;
        g.rect(tx, ty + 2, 1, 3, rgb(0x5a3a1a));
        g.ellipse(tx + .5f, ty, 3, 3.5f, rgb(k % 2 ? 0x2f6a3a : 0x3e7f44));
      }
    else if (m.theme == Theme::Cendres || m.theme == Theme::Neige) {  // des sommets (enneigés dans le froid)
      bool snow = m.theme == Theme::Neige;
      for (int k = 0; k < 3; k++) {
        float mx = p.x - 10 + k * 10, my = p.y + 2 - (k == 1) * 3, s = k == 1 ? 9.f : 7.f;
        g.tri(mx - s, my, mx, my - s * 1.3f, mx + s, my, rgb(snow ? 0x9ab4cc : 0xa08468));
        g.tri(mx, my, mx, my - s * 1.3f, mx + s, my, rgb(snow ? 0x7a94b0 : 0x806448));  // le versant à l'ombre
        g.tri(mx - s * .35f, my - s * .85f, mx, my - s * 1.3f, mx + s * .35f, my - s * .85f, rgb(snow ? 0xffffff : 0xc8b090));
      }
      if (m.id == "cendrelune" || m.id == "cratere") g.ellipse(p.x, p.y - 13, 3, 2, rgb(0xd9541e, 200));  // le volcan fume
    } else if (m.theme == Theme::Grotte) {  // l'entrée d'une grotte
      g.ellipse(p.x, p.y - 1, 9, 7, rgb(0x8a7a66));
      g.ellipse(p.x, p.y + 1, 5, 5, rgb(0x2a2018));
    } else if (wet[i] <= .12f)  // prairie : des touffes d'herbe
      for (int k = 0; k < 4; k++) {
        float gx = p.x - 10 + k * 7, gy = p.y + 5 + (k % 2) * 3;
        g.rect(gx, gy, 1, 2, rgb(0x5f9a52)), g.rect(gx + 2, gy - 1, 1, 3, rgb(0x5f9a52));
      }
  }
  // Chemins entre les lieux ouverts : des pointillés
  for (size_t i = 0; i < maps().size(); i++) {
    if (!open[i]) continue;
    for (auto& w : maps()[i].warps) {
      int t = mapIndex(w.map);
      if (t <= (int)i || !open[(size_t)t]) continue;
      Pt a = P(maps()[i]), b = P(maps()[(size_t)t]);
      float d = std::hypot(b.x - a.x, b.y - a.y);
      for (float k = 6; k < d - 6; k += 4) g.rect(std::round(a.x + (b.x - a.x) * k / d), std::round(a.y + (b.y - a.y) * k / d), 2, 1, rgb(0x8a6a44));
    }
  }
  // Les lieux, puis leurs noms (sans chevauchement : sous le point, sinon au-dessus, à droite ou à gauche)
  std::vector<std::array<float, 4>> taken;  // rectangles déjà occupés par un nom
  for (size_t i = 0; i < maps().size(); i++) {
    if (!open[i]) continue;
    const MapDef& m = maps()[i];
    Pt p = P(m);
    uint32_t col = m.theme == Theme::Foret ? 0x2f6a3a : m.theme == Theme::Cendres ? 0xc0502a : m.theme == Theme::Neige ? 0x8ab8e0
                   : m.theme == Theme::Grotte ? 0x6a5a8a : 0x5fa84f;
    bool village = false;
    for (auto& b : m.buildings) village = village || b.kind == "soin";
    g.ellipse(p.x, p.y + 1, 5, 4, rgb(0x000000, 60));
    if (village) {  // un village : une petite maison
      g.rect(p.x - 4, p.y - 2, 8, 5, rgb(0xe8d8b0));
      g.tri(p.x - 5, p.y - 2, p.x, p.y - 6, p.x + 5, p.y - 2, rgb(0xb8402a));
      g.rect(p.x - 1, p.y, 2, 3, rgb(0x5a3a1a));
    } else {
      g.ellipse(p.x, p.y, 4, 3.5f, rgb(0x3a2a1a));
      g.ellipse(p.x, p.y, 3, 2.5f, rgb(col));
    }
    if ((int)i == here) {  // vous êtes ici : un anneau doré qui pulse, et Lior à côté
      float k = .5f + .5f * std::sin(time * 5);
      g.ellipse(p.x, p.y, 8 + k * 2, 7 + k * 2, rgb(0xffd34d, uint8_t(70 + 70 * k)));
      if (const SpriteRect* r = findSprite("lior_bas_0")) {  // Lior, debout au-dessus du lieu
        g.sprite(*r, std::round(p.x - r->w * .35f), std::round(p.y - 6 - r->h * .7f), false, .7f);
        taken.push_back({p.x - r->w * .35f, p.y - 6 - r->h * .7f, r->w * .7f, r->h * .7f});
      }
    }
    taken.push_back({p.x - 8, p.y - 7, 16, 13});  // le dessin du lieu : aucun nom par-dessus
    bool sel = (int)i == worldSel_, can = std::find(targets.begin(), targets.end(), (int)i) != targets.end();
    if (sel) {
      float k = .5f + .5f * std::sin(time * 6);
      g.ellipse(p.x, p.y, 9 + k * 2, 8 + k * 2, rgb(0x2a7ad8, uint8_t(110 + 80 * k)));
    } else if (can)
      g.ellipse(p.x + 6, p.y - 5, 1.5f, 1.5f, rgb(0x2a7ad8));
  }
  for (int pass = 0; pass < 2; pass++)  // d'abord la destination et le lieu actuel (leur nom passe avant les autres)
    for (size_t i = 0; i < maps().size(); i++) {
      if (!open[i]) continue;
      const MapDef& m = maps()[i];
      bool village = false;
      for (auto& b : m.buildings) village = village || b.kind == "soin";
      bool sel = (int)i == worldSel_, me = (int)i == here;
      if ((pass == 0) != (sel || me) || !(village || sel || me)) continue;
      Pt p = P(m);
      float w = (float)Gfx::textW(m.name), hgt = 9;
      std::array<std::array<float, 2>, 4> spots = {{{p.x - w / 2, p.y + 6}, {p.x - w / 2, p.y - 17}, {p.x + 9, p.y - 4}, {p.x - 9 - w, p.y - 4}}};
      std::array<float, 2> at = spots[0];
      float best = 1e9f;  // la place la moins encombrée (surface recouverte)
      for (auto& sp : spots) {
        float cost = (sp[0] < 16 || sp[0] + w > 304 || sp[1] < 26 || sp[1] + hgt > 210) ? 1e6f : 0.f;
        for (auto& r : taken)
          cost += std::max(0.f, std::min(sp[0] + w + 2, r[0] + r[2]) - std::max(sp[0] - 2, r[0])) *
                  std::max(0.f, std::min(sp[1] + hgt, r[1] + r[3]) - std::max(sp[1], r[1]));
        if (cost < best - .5f) best = cost, at = sp;
      }
      taken.push_back({at[0], at[1], w, hgt});
      g.rect(at[0] - 2, at[1] - 1, w + 4, hgt + 1, rgb(0xf2e6c6, 215));  // une petite étiquette de parchemin
      g.text(at[0], at[1], m.name, sel ? rgb(0x1a4a9a) : me ? rgb(0x8a2a10) : INK, 0, false);
    }
  g.text(160, 213, targets.empty() ? T("Entrée ou Échap : fermer")
                   : worldSel_ >= 0 ? T("Entrée : y aller · Échap : fermer") : T("Flèches : choisir un village · Échap : fermer"),
         INK2, 1, false);
}


