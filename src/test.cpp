// Mode test (brumeval --test DOSSIER) : joue automatiquement quelques scènes,
// vérifie les données, simule des combats pour l'équilibrage et enregistre
// des captures d'écran (.bmp) dans DOSSIER. Renvoie 0 si tout va bien.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "arena.hpp"
#include "audio.hpp"
#include "battle.hpp"
#include "events.hpp"
#include "expedition.hpp"
#include "game.hpp"
#include "mapedit.hpp"
#include "online.hpp"
#include "options.hpp"
#include "settings.hpp"
#include "sprites.hpp"
#include "storyedit.hpp"
#include "tactics.hpp"

int Game::selfTest(SDL_Surface* target, const std::string& out) {
  std::filesystem::create_directories(out);
  saveName_ = "sauvegarde_test.txt";  // ne jamais toucher à la vraie sauvegarde du joueur
  tacticsAuto = false;                 // les combats des tests passent par les menus (les tactiques ont leurs tests)
  g.checkLayout = true;                // chaque image dessinée vérifie la mise en page (gfx.cpp)
  int fails = 0;
  auto check = [&](bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "[ok]    " : "[ÉCHEC] ", what.c_str());
    if (!ok) fails++;
  };
  auto frame = [&](float dt = 1 / 60.f) {
    update(dt);
    draw();
  };
  auto snap = [&](const std::string& name) {
    draw();
    SDL_RenderPresent(g.renderer());
    std::string p = out + "/" + name + ".bmp";
    SDL_SaveBMP(target, p.c_str());
    std::printf("capture  %s\n", p.c_str());
    g.layoutScene = name;
  };
  auto run = [&](float sec) {
    for (int i = 0; i < int(sec * 60); i++) frame();
  };
  auto skipScript = [&] {
    for (int i = 0; i < 3000 && sc.busy() && !menus.active(); i++) {
      in.confirm = true;
      frame();
    }
  };

  // --- Données (dossier data/) ---
  std::printf("Données lues dans %s\n", dataDir().c_str());
  auto report = [&](const std::vector<std::string>& errs, const std::string& what) {
    for (auto& e : errs) check(false, e);
    if (errs.empty()) check(true, what);
  };
  report(checkData(), "types, techniques, espèces, objets et règles cohérents");
  report(checkMaps(), std::to_string(maps().size()) + " cartes valides, tous les lieux sont accessibles à pied");
  report(checkEvents(), std::to_string(events().size()) + " événements valides");
  auto mi = [](const char* id) { return std::max(0, mapIndex(id)); };
  // Relecture : écrire puis relire une carte doit redonner la même carte
  {
    bool same = true;
    for (auto& m : maps()) same = same && mapToJson(mapFromJson(mapToJson(m))) == mapToJson(m);
    check(same, "les cartes se relisent à l'identique après écriture");
  }

  // --- Son (data/sons.json) : chaque musique et chaque effet donne un son audible, sans saturer ---
  report(audio::checkSounds(), std::to_string(audio::trackNames().size()) + " musiques et " + std::to_string(audio::effectNames().size()) +
                                   " effets sonores décrits");
  {
    std::string bad;
    auto audible = [&](const std::string& name, bool isMusic) {
      auto v = audio::render(name, isMusic, isMusic ? 4.f : 2.f);
      float peak = 0;
      for (float x : v) peak = std::max(peak, std::fabs(x));
      if (peak < .02f || peak >= 1.f) bad += " " + name + "(" + std::to_string(peak) + ")";
    };
    for (auto& t : audio::trackNames()) audible(t, true);
    for (auto& e : audio::effectNames()) audible(e, false);
    bool places = true;
    for (int t = 0; t < N_THEMES; t++) places = places && !audio::musicForPlace(themeName(Theme(t))).empty();
    check(bad.empty() && places, "son : chaque morceau et chaque effet s'entend sans saturer, chaque thème de carte a sa musique" +
                                     (bad.empty() ? std::string() : " (problème :" + bad + ")"));
  }

  // --- Options (fichier à part) : volume, nouvelle touche, relecture ---
  {
    std::string realFile = optionsFile;
    optionsFile = "options_test.txt";
    loadOptions();
    titleMenu();
    menus.top().sel = 5;  // Options
    in.confirm = true;
    frame();
    bool opened = menus.top().title == "Options";
    int vol = options().music;
    in.press[RIGHT] = true;  // Musique : plus fort
    frame();
    snap("86_options");
    bool louder = options().music == std::min(10, vol + 1);
    menus.top().sel = 3;  // Touches
    in.confirm = true;
    frame();
    snap("87_touches");
    in.confirm = true;  // Haut : attend la nouvelle touche
    frame();
    snap("88_touche_attente");
    bool waiting = bindKey_ == K_UP;
    onKey(SDL_SCANCODE_I, true, false);
    onKey(SDL_SCANCODE_I, false, false);
    onKey(SDL_SCANCODE_I, true, false);
    bool bound = bindKey_ < 0 && options().keys[K_UP] == SDL_SCANCODE_I && in.press[UP];
    onKey(SDL_SCANCODE_I, false, false);
    in.endFrame();
    options() = Options{};
    loadOptions();
    bool reread = options().keys[K_UP] == SDL_SCANCODE_I && options().music == std::min(10, vol + 1);
    check(opened && louder && waiting && bound && reread,
          "options : écran titre > Options, volume de la musique, nouvelle touche pour « Haut », relues depuis le fichier");
    char* pp = SDL_GetPrefPath("Brumeval", "Brumeval");
    std::remove((std::string(pp ? pp : "") + optionsFile).c_str());
    SDL_free(pp);
    optionsFile = realFile;
    options() = Options{};
    titleMenu();
    // Manette : croix vers le bas (tenue : elle se répète), A valide, B revient
    menus.top().sel = 0;
    onPad(SDL_CONTROLLER_BUTTON_DPAD_DOWN, true);
    frame();
    int one = menus.top().sel;
    run(.5f);
    int more = menus.top().sel;
    onPad(SDL_CONTROLLER_BUTTON_DPAD_DOWN, false);
    frame();
    menus.top().sel = 4;  // Outils
    onPad(SDL_CONTROLLER_BUTTON_A, true);
    frame();
    bool tools = menus.top().title == "Outils";
    onPad(SDL_CONTROLLER_BUTTON_B, true);
    frame();
    onStick(SDL_CONTROLLER_AXIS_LEFTY, -30000);  // stick vers le haut
    frame();
    int up = menus.top().sel;
    onStick(SDL_CONTROLLER_AXIS_LEFTY, 0);
    frame();
    check(one == 1 && more > 2 && tools && menus.depth() == 1 && up == 3 && !in.hold[UP],
          "manette : la croix déplace le curseur (et se répète), A valide, B revient, le stick marche aussi");
    titleMenu();
  }

  // --- Écran titre et début de partie ---
  run(.5f);
  snap("01_titre");
  newGame("braisenard");
  run(.6f);
  snap("02_intro");
  skipScript();
  run(.3f);
  snap("03_village");
  check(mode == Mode::Map && team.size() == 2, "nouvelle partie : Lior et son compagnon sur la carte");
  check(audio::currentMusic() == audio::musicForPlace("vallee"), "son : la musique de la vallée joue sur la carte (« " + audio::currentMusic() + " »)");
  check(fillText("Lior part avec {compagnon}.") == "Lior part avec Braisenard.", "les textes remplacent {compagnon} par son nom");

  in.menu = true;
  frame();
  snap("04_menu");
  in.confirm = true;
  frame();
  snap("05_equipe");
  menus.clear();
  panelMode_ = 0;

  // --- Équipement : une épée pour Lior (par les menus), gardée par la sauvegarde ; pas d'arme pour une créature ---
  {
    items["epee_fer"] = 1;
    int atk0 = team.at(0)->atk;
    pauseMenu();
    gearMenu();
    in.confirm = true;  // Lior
    frame();
    in.confirm = true;  // Arme
    frame();
    in.confirm = true;  // Épée de fer
    frame();
    snap("92_equipement");
    bool equipped = team[0]->gear[G_WEAPON] == "epee_fer" && team[0]->atk == atk0 + 3 && items["epee_fer"] == 0;
    bool noWeapon = !canEquip(*team.at(1), item("epee_fer")) && canEquip(*team.at(1), item("amulette_vie"));
    menus.clear();
    panelMode_ = 0;
    saveGame();
    team.clear();
    loadGame();
    bool kept = team.at(0)->gear[G_WEAPON] == "epee_fer" && team[0]->atk == atk0 + 3;
    check(equipped && noWeapon && kept, "équipement : Échap > Équipement, l'épée donne +3 en Attaque, gardée par la sauvegarde ; pas d'arme pour une créature");
    team[0]->gear[G_WEAPON].clear();
    team[0]->recalc();
    items.erase("epee_fer");
  }

  // --- Dialogue et recrutement de Maëlle ---
  changeMap(mi("vallee"), 7, 16, UP);
  interact();
  run(1.f);
  snap("06_dialogue");
  skipScript();
  items["herbe"] = 1;
  interact();
  skipScript();
  check(has("maelle") && team.size() == 3, "Maëlle rejoint l'équipe avec l'herbe lunaire");

  // --- Intérieurs : la porte de la maison de soin fait entrer, la guérisseuse soigne, le tapis fait sortir ---
  {
    int village = mi("vallee");
    const Building* soin = nullptr;
    for (auto& b : maps()[village].buildings)
      if (b.kind == "soin") soin = &b;
    changeMap(village, soin->doorX(), soin->doorY() + 1, UP);
    team.at(0)->hp = 1;
    interact();
    run(.3f);
    bool inside = M().id == soin->interior && M().theme == Theme::Interieur;
    snap("91_interieur_soin");
    bool music = audio::currentMusic() == audio::musicForPlace("interieur");
    const Npc& healer = M().npcs.at(0);
    px = healer.x, py = healer.y + 1, dir = UP;
    interact();
    skipScript();
    bool healed = team.at(0)->hp == team.at(0)->mhp;
    int dx = 0, dy = 0;
    for (auto& w : M().warps) dx = w.x, dy = w.y;
    px = dx, py = dy - 1, dir = DOWN;
    in.press[DOWN] = true;
    tryMove(DOWN);
    for (int i = 0; i < 30 && moving; i++) frame();
    run(.2f);
    bool out = M().id == "vallee" && px == soin->doorX() && py == soin->doorY() + 1;
    check(inside && music && healed && out,
          "intérieurs : la porte fait entrer dans la maison de soin (musique de l'intérieur), la guérisseuse soigne, le tapis fait ressortir");
  }

  // --- Quête annexe : le médaillon (accepter, journal, coffre du Bois Murmurant, récompense) ---
  {
    auto answer = [&] {  // Entrée sur chaque message ; « Oui » (premier choix) aux questions
      for (int i = 0; i < 3000 && (sc.busy() || menus.active()); i++) {
        in.confirm = true;
        frame();
      }
    };
    int gold0 = gold;
    runEvent("quete_medaillon");
    answer();
    bool started = has(questFlag("medaillon")) && !has(questFlag("medaillon", true));
    pauseMenu();
    journalMenu();
    run(.1f);
    snap("93_journal");
    bool listed = menus.top().title == "Journal" && menus.top().items.at(0).right == "en cours" &&
                  menus.top().items.at(0).help.find("Bois Murmurant") != std::string::npos;
    menus.clear();
    panelMode_ = 0;
    int village = mi("vallee"), chest = -1;
    changeMap(village, 12, 25, UP);
    for (size_t i = 0; i < M().chests.size(); i++)
      if (M().chests[i].item == "medaillon") chest = (int)i;
    if (chest >= 0) openChest(chest);
    answer();
    runEvent("quete_medaillon");
    answer();
    bool done = has(questFlag("medaillon", true)) && items["medaillon"] == 0 && items["amulette_vie"] == 1 && gold == gold0 + 200;
    check(started && listed && chest >= 0 && done,
          "quête annexe : le médaillon (acceptée, inscrite au journal, trouvée dans un coffre, rendue contre une récompense)");
    items.erase("amulette_vie");
  }

  // --- Combat piloté par les menus ---
  startBattle({makeFighter("mulotin", 4), makeFighter("champichou", 5), makeFighter("piafouine", 4)}, false, nullptr);
  for (int i = 0; i < 1500 && !menus.active(); i++) frame();
  check(menus.active(), "le menu de commande s'ouvre quand une jauge ATB est pleine");
  check(audio::currentMusic() == "combat" && audio::lastEffect != "", "son : musique de combat, bruitages (dernier : « " + audio::lastEffect + " »)");
  snap("07_combat_commande");
  if (battle_ && battle_->actor && !battle_->actor->spells().empty()) {
    in.press[DOWN] = true;
    frame();
    in.confirm = true;
    frame();
    snap("08_combat_magie");
    in.cancel = true;
    frame();
    in.press[UP] = true;
    frame();
  }
  in.confirm = true;
  frame();
  snap("09_combat_techniques");
  in.confirm = true;
  frame();
  snap("10_combat_cible");
  in.confirm = true;
  frame();
  run(.45f);
  snap("11_combat_degats");
  if (battle_) {  // animations : sort (anneaux de la couleur du type) et soin (étincelles)
    Battle& B = *battle_;
    B.startAnim(B.allies[0], {B.foes[1]}, Battle::Fx::Magic, rgb(0x5aa0ff));
    for (int i = 0; i < 22; i++) frame();
    snap("89_anim_sort");
    B.startAnim(B.allies[2], {B.allies[0], B.allies[1]}, Battle::Fx::Heal, rgb(0x7dffa8));
    for (int i = 0; i < 8; i++) frame();
    Pt lunge = B.offset(B.allies[2]);
    bool two = B.anim_.targets.size() == 2;
    for (int i = 0; i < 18; i++) frame();
    snap("90_anim_soin");
    check(lunge.y < 0 && two, "combat : animations (élan du lanceur, effet sur chaque cible)");
  }
  for (int i = 0; i < 60 * 300 && mode == Mode::Battle; i++) {
    if (menus.active() || sc.busy() || (battle_ && battle_->sc.busy())) in.confirm = true;
    frame();
  }
  check(mode == Mode::Map, "le combat se termine et ramène sur la carte");
  skipScript();

  // --- Cartes ---
  changeMap(mi("cendrelune"), 12, 34, DOWN);
  run(.4f);
  snap("12_forgeroc");
  changeMap(mi("cendrelune"), 46, 12, UP);
  banner = 0;
  run(.2f);
  snap("13_cendrelune_nord");
  changeMap(mi("grotte"), 15, 18, UP);
  run(.4f);
  snap("14_grotte");
  changeMap(mi("vallee"), 56, 32, RIGHT);
  banner = 0;
  run(.2f);
  snap("15_col_sylvarque");
  changeMap(mi("vallee"), 30, 12, DOWN);
  run(.2f);
  snap("16_prairie");
  shopMenu({"potion", "superpotion", "ether", "plume", "lanterne", "lanterne_argent"});
  frame();
  snap("17_boutique");
  menus.clear();

  // --- Combats spéciaux (captures) ---
  team = {makeFighter("lior", 22), makeFighter("maelle", 22), makeFighter("isra", 22)};
  changeMap(mi("cendrelune"), 48, 4, UP);
  startBattle({makeFighter("tisonnel", 19), makeFighter("ignarok", 23), makeFighter("tisonnel", 19)}, true, nullptr, false, false);
  battle_->foes[1]->boss = true;
  for (int i = 0; i < 1500 && !menus.active(); i++) frame();
  snap("18_boss_ignarok");
  check(audio::currentMusic() == "boss", "son : musique du boss");
  battle_->autoPlay = true;
  menus.clear();
  battle_->actor->atb = 0;
  battle_->actor = nullptr;
  for (int i = 0; i < 60 * 30 && mode == Mode::Battle; i++) {
    in.confirm = true;
    frame();
    if (i == 400) snap("19_boss_en_cours");
  }
  mode = Mode::Ending;
  snap("20_fin");
  mode = Mode::Map;
  sc.clear();

  // --- Sauvegarde ---
  team = {makeFighter("lior", 9), makeFighter("ronceau", 8)};
  gold = 321;
  flags = {"maelle", "boss1"};
  items = {{"potion", 7}};
  changeMap(mi("cendrelune"), 12, 34, DOWN);
  flags.insert(chestFlag(0));
  bool saved = saveGame();
  team.clear();
  gold = 0;
  bool loaded = loadGame();
  check(saved && loaded && team.size() == 2 && team[0]->lvl == 9 && gold == 321 && has("boss1") && items["potion"] == 7 &&
            M().id == "cendrelune" && px == 12 && has(chestFlag(0)),
        "sauvegarde puis chargement");

  // --- Événements : le pêcheur donne deux plumes une seule fois ---
  {
    items.clear();
    flags.clear();
    changeMap(mi("vallee"), 26, 38, LEFT);
    interact();
    skipScript();
    interact();
    skipScript();
    check(items["plume"] == 2 && has("pecheur"), "événement du pêcheur (drapeau, objet donné une seule fois)");
  }
  // --- Événements : question puis combat, la suite passe avant le reste ---
  {
    team = {makeFighter("lior", 40), makeFighter("maelle", 40), makeFighter("isra", 40)};
    flags.clear();
    mode = Mode::Map;
    sc.clear();
    menus.clear();
    runEvent("brann");
    for (int i = 0; i < 4000 && mode != Mode::Battle; i++) {
      in.confirm = true;  // fait défiler et répond « Oui »
      frame();
    }
    bool fought = mode == Mode::Battle;
    if (battle_) battle_->autoPlay = true;
    for (int i = 0; i < 60 * 120 && mode == Mode::Battle; i++) {
      in.confirm = true;
      frame();
    }
    skipScript();
    check(fought && has("brann") && team.size() == 4, "événement de Brann : question, duel, victoire puis recrutement");
  }

  // --- Mécaniques du combat ---
  {
    const Species& golem = species("golem");
    check(std::abs(moveEff(moveInfo("lame"), golem) - .75f) < 1e-4f && std::abs(moveEff(moveInfo("feu"), golem) - 1.25f) < 1e-4f,
          "résistances propres : le Golem encaisse les coups physiques, craint la magie");
    check(moveEff(moveInfo("lumiere"), species("sylvarque")) == 2.f && moveEff(moveInfo("feu"), species("ronceau")) == 2.f &&
              moveEff(moveInfo("lame"), species("lior")) == 1.f,
          "efficacité des types");
    check(immuneTo(species("braisenard"), Status::Burn) && immuneTo(golem, Status::Poison) && !immuneTo(species("lior"), Status::Poison),
          "immunités aux états (par type et par espèce)");
    auto f = makeFighter("lior", 20);
    int def = f->def;
    f->stage[S_DEF] = 1;
    float up = f->eDef();
    f->stage[S_DEF] = -1;
    float down = f->eDef();
    check(std::abs(up - def * 1.25f) < .01f && std::abs(down - def * .8f) < .01f, "bonus et malus : +1 = ×1,25 ; -1 = ×0,8");
    check(f->res > 0 && f->acc == 100 && f->crit == 8, "nouvelles statistiques : Résistance, Précision, Critique");

    // Poison : 10 % des PV max à la fin du tour ; sommeil : le tour est perdu
    team = {makeFighter("lior", 20), makeFighter("maelle", 20)};
    mode = Mode::Map;
    sc.clear();
    menus.clear();
    BattleSetup setup;
    setup.foes = {makeFighter("mulotin", 10)};
    setup.reserve = {makeFighter("piafouine", 10)};
    setup.foeName = "Le dresseur";
    startBattle(setup, nullptr);
    Battle& B = *battle_;
    B.sc.clear();
    auto foe = B.foes[0];
    foe->status = Status::Poison;
    B.allies[1]->status = Status::Paralysis;
    B.allies[1]->stage[S_DEF] = 1;
    B.start = time - 1;  // pas d'éclair blanc d'entrée en combat sur la capture
    snap("22_etats");
    B.allies[1]->status = Status::None;
    int hp0 = foe->hp;
    B.afterAction(foe);
    check(hp0 - foe->hp == std::max(1, int(foe->mhp * rules().poisonDmg)), "poison : perte de PV à la fin du tour");
    B.sc.clear();
    auto ally = B.allies[0];
    ally->status = Status::Sleep;
    ally->statusTurns = 2;
    bool skipped = B.skipTurn(ally);
    check(skipped && ally->status == Status::Sleep && ally->statusTurns == 1, "sommeil : le combattant perd son tour");
    B.sc.clear();
    foe->hp = 0;
    B.knockOut(foe);
    B.checkEnd();
    check(B.foes[0]->sp == "piafouine" && B.reserve.empty() && !B.ending_, "renforts : un nouvel ennemi entre quand le premier tombe");
    battle_.reset();
    mode = Mode::Map;
    sc.clear();
  }

  // --- Dresseur : le braconnier repère le joueur sur le chemin ---
  {
    team = {makeFighter("lior", 30), makeFighter("maelle", 30), makeFighter("braisenard", 30)};
    flags.clear();
    items.clear();
    sc.clear();
    menus.clear();
    mode = Mode::Map;
    changeMap(mi("vallee"), 43, 9, RIGHT);
    in.hold[RIGHT] = true;
    for (int i = 0; i < 40 && px < 45; i++) frame();
    in.hold[RIGHT] = false;
    for (int i = 0; i < 60 && exclaimNpc_ < 0; i++) frame();
    run(.3f);
    snap("23_dresseur_repere");
    for (int i = 0; i < 2000 && mode != Mode::Battle; i++) {
      in.confirm = true;
      frame();
    }
    bool spotted = mode == Mode::Battle && battle_ && battle_->foeName == "Le braconnier";
    if (battle_) battle_->autoPlay = true;
    for (int i = 0; i < 2000 && mode == Mode::Battle; i++) {
      in.confirm = i > 60;  // laisse le temps de voir l'adversaire et ses renforts
      frame();
      if (i == 50) snap("21_dresseur");
    }
    skipScript();
    check(spotted && has("braconnier") && items["remede"] == 2, "dresseur : repère le joueur, combat avec renfort, récompense");
  }

  // --- Passages fermés et zones déclencheuses ---
  {
    team = {makeFighter("lior", 30), makeFighter("maelle", 30)};
    items.clear();
    sc.clear();
    menus.clear();
    mode = Mode::Map;
    auto walk = [&](int d, int n) {
      for (int k = 0; k < n; k++) {
        in.press[d] = true;
        frame();
        for (int i = 0; i < 20 && moving; i++) frame();
      }
    };
    flags.clear();
    changeMap(mi("vallee"), 1, 9, LEFT);
    walk(LEFT, 1);
    bool locked = M().id == "vallee" && px == 1 && sc.busy();
    skipScript();
    flags = {"boss1"};
    changeMap(mi("vallee"), 1, 9, LEFT);
    walk(LEFT, 1);
    bool open = M().id == "sylvenoire";
    check(locked && open, "passage fermé : la forêt s'ouvre seulement après Sylvarque");
    // Zone déclencheuse à l'entrée de la forêt : une seule fois
    for (int i = 0; i < 30 && !sc.busy(); i++) walk(LEFT, 1);
    bool fired = sc.busy() || has("foret_vue");
    skipScript();
    bool once = has("foret_vue");
    changeMap(mi("sylvenoire"), 55, 20, LEFT);
    walk(LEFT, 3);
    check(fired && once && !sc.busy(), "zone déclencheuse : la scène d'entrée de la forêt ne se joue qu'une fois");
    skipScript();
    // Temple : la porte du sanctuaire demande la clé de givre
    flags = {"boss1", "boss2"};
    changeMap(mi("temple"), 19, 1, UP);
    walk(UP, 1);
    bool sealed = M().id == "temple";
    skipScript();
    items["cle_givre"] = 1;
    changeMap(mi("temple"), 19, 1, UP);
    walk(UP, 1);
    check(sealed && M().id == "sanctuaire", "porte scellée : le sanctuaire s'ouvre avec la clé de givre");
    skipScript();
    items.clear();
    flags.clear();
  }

  // --- Nouvelles régions : captures des cartes et des décors de combat ---
  {
    team = {makeFighter("lior", 25), makeFighter("maelle", 25), makeFighter("kael", 25)};
    flags = {"boss1", "boss2", "foret_vue", "pics_vu"};
    sc.clear();
    menus.clear();
    mode = Mode::Map;
    auto view = [&](const char* map, int x, int y, int d, const char* name) {
      changeMap(mi(map), x, y, d);
      banner = 0;
      run(.3f);
      snap(name);
    };
    view("sylvenoire", 14, 20, LEFT, "24_sylvenoire_camp");
    view("sylvenoire", 40, 20, LEFT, "25_sylvenoire");
    view("pics", 30, 35, UP, "26_givreval");
    view("pics", 30, 12, UP, "27_pics");
    view("temple", 20, 27, UP, "28_temple");
    view("sanctuaire", 10, 12, UP, "29_sanctuaire");
    auto fight = [&](const char* map, std::vector<FighterP> foes, const char* name) {
      changeMap(mi(map), 2, 2, DOWN);
      startBattle(foes, false, nullptr);
      battle_->start = time - 1;
      battle_->sc.clear();
      run(.2f);
      snap(name);
      battle_.reset();
      mode = Mode::Map;
    };
    fight("sylvenoire", {makeFighter("serpentin", 12), makeFighter("chauvenuit", 12), makeFighter("esprillon", 12)}, "30_combat_foret");
    fight("pics", {makeFighter("givrelin", 24), makeFighter("ferrolin", 24), makeFighter("cristallin", 24)}, "31_combat_neige");
    // Planches : toutes les créatures, puis tous les personnages
    auto raw = [&](const std::string& name) {
      SDL_RenderPresent(g.renderer());
      std::string p = out + "/" + name + ".bmp";
      SDL_SaveBMP(target, p.c_str());
      std::printf("capture  %s\n", p.c_str());
    };
    std::vector<const Species*> cs;
    for (auto& sp : allSpecies())
      if (!sp.human) cs.push_back(&sp);
    g.clear(rgb(0x1d2148));
    for (size_t i = 0; i < cs.size(); i++) {
      float x = 22 + (i % 7) * 46.f, y = 26 + (i / 7) * 46.f;
      drawCreature(g, cs[i]->id, x, y, .62f, false, 0);
      g.text(x, y + 13, utf8Prefix(cs[i]->name, 7), rgb(0xffffff), 1);
    }
    raw("32_galerie_creatures");
    g.clear(rgb(0x1d2148));
    for (size_t i = 0; i < allLooks().size(); i++) {
      float x = 14 + (i % 9) * 34.f, y = 20 + (i / 9) * 60.f;
      drawHuman(g, look((int)i), x, y, 1.5f, DOWN, 0, true);
      g.text(x + 12, y + 30, utf8Prefix(look((int)i).name, 5), rgb(0xffffff), 1);
    }
    raw("33_galerie_personnages");
    team.clear();
    flags.clear();
  }

  // --- Arène de combat ---
  {
    sc.clear();
    titleMenu();
    menus.top().sel = 4;  // Outils
    in.confirm = true;
    frame();
    in.confirm = true;  // Arène de combat
    frame();
    check(mode == Mode::Arena && arena_ != nullptr, "écran titre > Outils > Arène de combat");
    if (arena_) {
      Arena& A = *arena_;
      check(A.presetCount() >= 15 && A.findPreset("Givrecorne") >= 0 && A.findPreset("Le braconnier") >= 0,
            "arène : modèles repris des événements et des zones (" + std::to_string(A.presetCount()) + ")");
      run(.3f);
      snap("34_arene");
      in.press[DOWN] = true;
      frame();
      snap("35_arene_fiche");
      in.confirm = true;  // modifier l'allié
      frame();
      in.confirm = true;  // choisir l'espèce
      frame();
      run(.2f);
      snap("36_arene_especes");
      menus.clear();
      A.applyPreset(A.findPreset("Sylvarque"));
      A.startSim(10);
      for (int i = 0; i < 20000 && A.simulating(); i++) frame();
      check(!A.simulating() && A.results.total == 10, "arène : simulation de 10 combats (" + std::to_string(A.results.wins) + " victoires)");
      run(.2f);
      snap("37_arene_resultats");
      A.startManual();
      if (battle_) battle_->autoPlay = true;
      for (int i = 0; i < 60 * 300 && mode == Mode::Battle; i++) {
        in.confirm = true;
        frame();
      }
      check(mode == Mode::Arena && !A.lastLog.empty() && !A.lastResult.empty(), "arène : combat à la main puis retour avec le journal");
      in.press[DOWN] = true;  // Modèles…
      frame();
      in.press[DOWN] = true;  // Journal
      frame();
      in.confirm = true;
      frame();
      run(.2f);
      snap("38_arene_journal");
      menus.clear();
    }
    mode = Mode::Title;
    titleMenu();
  }

  // --- Réglages ---
  {
    // Enregistrer sans rien changer doit redonner exactement les mêmes fichiers
    bool same = true;
    for (int f = 0; f < N_DATAFILES; f++) {
      std::ifstream file(std::filesystem::u8path(dataDir()) / dataFileName(DataFile(f)), std::ios::binary);
      std::stringstream ss;
      ss << file.rdbuf();
      std::string disk = ss.str();
      disk.erase(std::remove(disk.begin(), disk.end(), '\r'), disk.end());
      if (disk != prettyJson(dataDoc(DataFile(f)))) {
        same = false;
        std::printf("         différent : %s\n", dataFileName(DataFile(f)));
      }
    }
    check(same, "réglages : les fichiers de data/ se réécrivent à l'identique");

    sc.clear();
    titleMenu();
    menus.top().sel = 4;  // Outils
    in.confirm = true;
    frame();
    menus.top().sel = 1;  // Réglages
    in.confirm = true;
    frame();
    check(mode == Mode::Settings && settings_ != nullptr, "écran titre > Outils > Réglages");
    if (settings_) {
      Settings& S = *settings_;
      run(.2f);
      snap("39_reglages");
      in.press[DOWN] = true;  // Espèces
      frame();
      in.confirm = true;
      frame();
      in.confirm = true;  // Lior
      frame();
      int atk0 = species("lior").base[B_ATK];
      menus.top().sel = 7;  // Attaque
      in.press[RIGHT] = true;
      frame();
      check(species("lior").base[B_ATK] == atk0 + 1 && S.dirtyCount() == 1 && makeFighter("lior", 20)->atk > 0,
            "réglages : Attaque de Lior +1, effet immédiat, 1 fichier modifié");
      run(.2f);
      snap("40_reglages_espece");
      // Saisie de texte : renommer
      menus.top().sel = 0;
      in.confirm = true;
      frame();
      bool typing = editingText();
      for (int i = 0; i < 10; i++) onKey(SDL_SCANCODE_BACKSPACE, true, false);
      onText("Lior le Brave");
      run(.1f);
      snap("41_saisie_texte");
      onKey(SDL_SCANCODE_RETURN, true, false);
      check(typing && !editingText() && species("lior").name == "Lior le Brave", "saisie de texte : renommer une espèce");
      // Grille des types
      S.menuTypes(0);
      in.confirm = true;
      frame();
      float e0 = typeEff(typeOf("normal"), typeOf("normal"));
      in.confirm = true;
      frame();
      check(S.inGrid() && e0 == 1.f && typeEff(typeOf("normal"), typeOf("normal")) == 2.f, "réglages : grille d'efficacité des types");
      run(.1f);
      snap("42_reglages_types");
      S.menuRules(0);
      run(.1f);
      snap("43_reglages_regles");
      S.editMove("flam");
      run(.1f);
      snap("44_reglages_technique");
      S.revert();
      check(S.dirtyCount() == 0 && species("lior").base[B_ATK] == atk0 && species("lior").name == "Lior" &&
                typeEff(typeOf("normal"), typeOf("normal")) == 1.f,
            "réglages : « Annuler les changements » recharge les fichiers");
    }
    menus.clear();
    mode = Mode::Title;
    titleMenu();
  }

  // --- Éditeur de cartes ---
  {
    sc.clear();
    titleMenu();
    menus.top().sel = 4;  // Outils
    in.confirm = true;
    frame();
    menus.top().sel = 2;  // Éditeur de cartes
    in.confirm = true;
    frame();
    check(mode == Mode::Editor && editor_ != nullptr, "écran titre > Outils > Éditeur de cartes");
    if (editor_) {
      MapEditor& E = *editor_;
      int vi = mi("vallee");
      E.open("vallee");
      Json before = mapToJson(maps()[vi]);
      run(.2f);
      snap("45_editeur");
      // Pinceau, annuler, rétablir
      E.setLayer(MapEditor::Layer::Tiles);
      E.setTool(MapEditor::Tool::Brush);
      E.setTile('F');
      E.setCursor(10, 5);
      char old = maps()[vi].rows[5][10];
      E.apply();
      bool painted = maps()[vi].rows[5][10] == 'F';
      E.undo();
      bool undone = maps()[vi].rows[5][10] == old;
      E.redo();
      check(painted && undone && maps()[vi].rows[5][10] == 'F' && E.dirty(vi), "éditeur : peindre, annuler, rétablir");
      // Rectangle
      E.setTool(MapEditor::Tool::Rect);
      E.setTile('R');
      E.setCursor(2, 2);
      E.apply();
      E.setCursor(4, 3);
      E.apply();
      int nR = 0;
      for (int y = 2; y <= 3; y++)
        for (int x = 2; x <= 4; x++) nR += maps()[vi].rows[y][x] == 'R';
      check(nR == 6, "éditeur : rectangle de 3 x 2 cases");
      // Souris : un clic gauche peint sous la souris
      E.setTool(MapEditor::Tool::Brush);
      E.setTile('~');
      onMouse(150, 100, -1, false);
      onMouse(150, 100, 0, true);
      frame();
      onMouse(150, 100, 0, false);
      frame();
      check(maps()[vi].rows[E.cursorY()][E.cursorX()] == '~', "éditeur : peindre à la souris");
      in.mouseOn = false;
      // Calque des objets : ajouter un habitant
      E.setLayer(MapEditor::Layer::Objects);
      size_t npcs = maps()[vi].npcs.size();
      E.setCursor(24, 6);
      E.apply();  // menu « Ajouter »
      in.confirm = true;  // Habitant
      frame();
      run(.1f);
      snap("46_editeur_habitant");
      check(maps()[vi].npcs.size() == npcs + 1 && menus.active(), "éditeur : ajouter un habitant et ouvrir sa fiche");
      menus.clear();
      E.setLayer(MapEditor::Layer::Zones);
      E.setCursor(30, 12);
      run(.1f);
      snap("47_editeur_zones");
      // Vue éloignée (menu Échap > Vue)
      in.cancel = true;
      frame();
      menus.top().sel = 3;
      in.press[RIGHT] = true;
      frame();
      menus.clear();
      run(.1f);
      snap("48_editeur_vue_eloignee");
      // Tester ici, puis revenir à l'éditeur
      E.setCursor(12, 9);
      E.testHere();
      bool playing = mode == Mode::Map && M().id == "vallee" && px == 12 && py == 9;
      in.menu = true;
      frame();
      in.confirm = true;  // « Fin du test »
      frame();
      check(playing && mode == Mode::Editor, "éditeur : « Tester ici » puis « Fin du test »");
      // On remet la carte telle qu'elle était (rien n'est enregistré)
      maps()[vi] = mapFromJson(before);
      check(!E.dirty(vi) && mapToJson(maps()[vi]) == before, "éditeur : la carte d'origine est intacte");
    }
    menus.clear();
    team.clear();
    mode = Mode::Title;
    titleMenu();
  }

  // --- Éditeur d'histoire ---
  {
    sc.clear();
    titleMenu();
    menus.top().sel = 4;  // Outils
    in.confirm = true;
    frame();
    menus.top().sel = 3;  // Éditeur d'histoire
    in.confirm = true;
    frame();
    check(mode == Mode::Story && story_ != nullptr, "écran titre > Outils > Éditeur d'histoire");
    if (story_) {
      StoryEditor& S = *story_;
      Json before = events();
      run(.2f);
      snap("49_histoire");
      // Ajouter une action « Donner » à la première page du pêcheur
      S.goTo("/pecheur/pages/0/actions");
      run(.1f);
      snap("50_histoire_actions");
      size_t n0 = events()["pecheur"]["pages"][0]["actions"].size();
      menus.top().sel = (int)n0;  // « + Ajouter une action »
      in.confirm = true;
      frame();
      menus.top().sel = 1;  // Donner un objet
      in.confirm = true;
      frame();
      run(.1f);
      snap("51_histoire_action");
      check(events()["pecheur"]["pages"][0]["actions"].size() == n0 + 1 && checkEvents().empty(), "histoire : ajouter une action « Donner »");
      // Actions imbriquées : le duel de Brann
      S.goTo("/brann/actions/3/oui/0");
      run(.1f);
      snap("52_histoire_combat");
      S.goTo("/brann/actions/3/oui/0/victoire");
      bool nested = menus.active() && menus.top().items.size() == events()["brann"]["actions"][3]["oui"][0]["victoire"].size() + 1;
      check(nested, "histoire : ouvrir une sous-liste (victoire du duel de Brann)");
      // Modifier un message au clavier, puis annuler et rétablir
      std::string old = events()["ancien"]["pages"][0]["actions"][0]["texte"].get<std::string>();
      S.goTo("/ancien/pages/0/actions/0");
      menus.top().sel = 0;
      in.confirm = true;
      frame();
      bool typing = editingText();
      for (int i = 0; i < 200; i++) onKey(SDL_SCANCODE_BACKSPACE, true, true);
      onText("Ancien : Bonjour, voyageur !");
      onKey(SDL_SCANCODE_RETURN, true, false);
      std::string now = events()["ancien"]["pages"][0]["actions"][0]["texte"].get<std::string>();
      S.undo();
      std::string undone = events()["ancien"]["pages"][0]["actions"][0]["texte"].get<std::string>();
      S.redo();
      check(typing && now == "Ancien : Bonjour, voyageur !" && undone == old &&
                events()["ancien"]["pages"][0]["actions"][0]["texte"].get<std::string>() == now,
            "histoire : modifier un message, annuler, rétablir");
      S.goTo("/ancien/pages/0/si");
      run(.1f);
      snap("53_histoire_condition");
      S.revert();
      check(events() == before && !S.dirty(), "histoire : « Tout annuler » rend le fichier d'origine");
      // Jouer l'événement du pêcheur, là où il se trouve
      S.testFlags = "";
      S.play("pecheur");
      bool played = mode == Mode::Map && M().id == "vallee" && sc.busy();
      skipScript();
      check(played && has("pecheur") && items["plume"] == 2, "histoire : jouer l'événement du pêcheur sur sa carte");
      in.menu = true;
      frame();
      in.confirm = true;  // « Fin du test »
      frame();
      check(mode == Mode::Story, "histoire : retour à l'éditeur après le test");
    }
    menus.clear();
    flags.clear();
    team.clear();
    mode = Mode::Title;
    titleMenu();
  }

  // --- Rythme des techniques : rapide (la jauge repart plus haut) ou lourde (en dessous de zéro) ---
  {
    check(moveInfo("lame").pace == Pace::Quick && moveInfo("estoc").pace == Pace::Normal && moveInfo("eclair").pace == Pace::Heavy,
          "techniques : Lame d'acier rapide, Estoc normale, Taillade éclair lourde");
    team = {makeFighter("lior", 20), makeFighter("maelle", 20)};
    FighterP lior = team[0];
    flags.clear();
    sc.clear();
    menus.clear();
    mode = Mode::Map;
    changeMap(mi("vallee"), 30, 12, DOWN);
    tacticsAuto = false;
    auto foe = makeFighter("golem", 20);
    foe->mhp = foe->hp = 9999;
    startBattle({foe}, false, nullptr);
    if (battle_) {
      Battle& B = *battle_;
      // Jauge de Lior juste après chaque technique (le script fini, avant que le temps ne repasse)
      auto gaugeAfter = [&](const std::string& mv) {
        menus.clear();
        B.sc.clear();
        B.useMove(lior, mv, {foe});
        for (int i = 0; i < 600 && B.sc.busy(); i++) frame();
        return lior->atb;
      };
      float q = gaugeAfter("lame"), n = gaugeAfter("estoc"), h = gaugeAfter("eclair");
      check(q == float(rules().quickGauge) && n == 0 && h == -float(rules().heavyDelay),
            "techniques : la jauge repart à " + std::to_string((int)q) + " % (rapide), " + std::to_string((int)n) + " % (normale), " +
                std::to_string((int)h) + " % (lourde)");
      snap("60_combat_jauge_lourde");
      foe->hp = 1;
      B.autoPlay = true;
      for (int i = 0; i < 60 * 60 && mode == Mode::Battle; i++) {
        in.confirm = true;
        frame();
      }
    }
    check(mode == Mode::Map, "techniques : fin du combat de test du rythme");
    skipScript();
  }

  // --- Tactiques (règles de combat automatiques) ---
  {
    auto selectLabel = [&](const std::string& start) {
      auto& it = menus.top().items;
      for (size_t k = 0; k < it.size(); k++)
        if (!it[k].header && it[k].label.rfind(start, 0) == 0) {
          menus.top().sel = (int)k;
          return true;
        }
      return false;
    };
    check(rules().tactics.size() == 4 && tacticSlots(5) == 4 && tacticSlots(12) == 6 && tacticSlots(200) == rules().tacticMax &&
              makeFighter("braisenard", 5)->tactics.size() == 4,
          "tactiques : règles de départ, lignes selon le niveau");
    team = {makeFighter("lior", 20), makeFighter("maelle", 20), makeFighter("isra", 20)};
    FighterP lior = team[0], maelle = team[1], isra = team[2];
    items = {{"potion", 3}};
    flags.clear();
    sc.clear();
    menus.clear();
    mode = Mode::Map;
    changeMap(mi("vallee"), 30, 12, DOWN);
    tacticsAuto = false;
    startBattle({makeFighter("ronceau", 18), makeFighter("gouttelin", 18)}, false, nullptr);
    if (battle_) {
      Battle& B = *battle_;
      Battle::Plan p;
      lior->hp = lior->mhp / 5;
      bool heal = B.tacticPlan(maelle, p) && p.move == "soin" && p.targets.size() == 1 && p.targets[0] == lior;
      lior->hp = 0;
      bool revive = B.tacticPlan(maelle, p) && p.move == "reveil" && p.targets[0] == lior;
      lior->hp = lior->mhp;
      check(heal && revive, "tactiques : Maëlle soigne Lior blessé, puis le relève quand il est K.O.");
      isra->tactics = {Tactic{true, "ennemi_faible", 0, Tactic::Act::Auto, "attaque"}};
      bool weak = B.tacticPlan(isra, p) && std::any_of(p.targets.begin(), p.targets.end(), [&](const FighterP& t) {
                    return moveEff(moveInfo(p.move), t->S()) > 1;
                  });
      check(weak, "tactiques : « Ennemi : faible à l'action » choisit une attaque super efficace (" + (p.move.empty() ? "?" : p.move) + ")");
      lior->tactics = {Tactic{true, "allie_pv_moins", 50, Tactic::Act::Item, "potion"}};
      maelle->hp = maelle->mhp * 3 / 10;
      bool potion = B.tacticPlan(lior, p) && p.item == "potion" && p.targets[0] == maelle;
      lior->tactics[0].on = false;
      bool off = !B.tacticPlan(lior, p);
      lior->tactics = {Tactic{true, "allie_pv_moins", 50, Tactic::Act::Auto, "attaque"}};  // attaque sur un allié : impossible
      bool wrong = !B.tacticPlan(lior, p) && !tacticProblem(lior->tactics[0], nullptr).empty();
      maelle->hp = maelle->mhp;
      check(potion && off && wrong, "tactiques : objet du sac, règle désactivée, règle impossible sautée");
      for (auto& f : team) f->tactics = defaultTactics(f->S());
      // Le menu de commande attend un ordre : Tab active le mode auto et joue ce tour
      for (int i = 0; i < 1500 && !menus.active(); i++) frame();
      bool waiting = menus.active();
      onKey(SDL_SCANCODE_TAB, true, false);
      frame();
      check(waiting && tacticsAuto && !menus.active(), "tactiques : Tab en combat active le mode auto et joue le tour en attente");
      run(.4f);
      snap("54_combat_auto");
      bool menu = false;
      for (int i = 0; i < 60 * 300 && mode == Mode::Battle; i++) {
        frame();
        menu = menu || menus.active();
      }
      check(mode == Mode::Map && !menu, "tactiques : le combat se joue tout seul en mode auto, sans ouvrir de menu");
    }
    // Éditeur des tactiques : menu de pause > Tactiques > Lior
    skipScript();
    menus.clear();
    sc.clear();
    in.menu = true;
    frame();
    bool inMenu = selectLabel("Tactiques");
    in.confirm = true;
    frame();
    run(.1f);
    snap("55_tactiques_membres");
    menus.top().sel = 2;  // Lior
    in.confirm = true;
    frame();
    run(.1f);
    snap("56_tactiques_regles");
    size_t n0 = lior->tactics.size();
    menus.top().sel = 2 + (int)n0;  // première ligne libre
    in.confirm = true;
    frame();
    bool found = selectLabel("Soi : PV");
    run(.1f);
    snap("57_tactiques_condition");
    in.confirm = true;
    frame();
    found = selectLabel("Potion") && found;
    run(.1f);
    snap("58_tactiques_action");
    in.confirm = true;
    frame();
    bool added = lior->tactics.size() == n0 + 1 && lior->tactics.back().cond == "soi_pv_moins" && lior->tactics.back().act == "potion" &&
                 lior->tactics.back().kind == Tactic::Act::Item && lior->tactics.back().value == 30;
    check(inMenu && found && added, "tactiques : ajouter « Soi : PV < 30 % > Potion » depuis le menu de pause");
    in.confirm = true;  // la nouvelle règle est sous le curseur
    frame();
    bool up = selectLabel("Monter");
    in.confirm = true;
    frame();
    up = selectLabel("Active") && up;
    in.confirm = true;
    frame();
    check(up && lior->tactics[n0 - 1].cond == "soi_pv_moins" && !lior->tactics[n0 - 1].on, "tactiques : monter puis désactiver une règle");
    for (int i = 0; i < 5 && menus.active(); i++) {
      in.cancel = true;
      frame();
    }
    // Sauvegarde : les règles et le mode auto sont gardés
    tacticsAuto = false;
    bool saved = saveGame();
    tacticsAuto = true;
    team.clear();
    bool loaded = loadGame();
    check(saved && loaded && !tacticsAuto && team.size() == 3 && team[0]->tactics.size() == n0 + 1 && team[0]->tactics[n0 - 1].act == "potion" &&
              !team[0]->tactics[n0 - 1].on && team[0]->tactics[n0 - 1].value == 30 && team[1]->tactics.size() == 4,
          "tactiques : sauvegarde puis chargement des règles et du mode auto");
    // Réglages : tactiques de départ
    if (settings_) {
      mode = Mode::Settings;
      settings_->open();
      settings_->menuRules(0);
      bool opened = selectLabel("Tactiques de départ");
      in.confirm = true;
      frame();
      menus.top().sel = 2;  // règle 2 : Allié : PV < 40 %
      in.confirm = true;
      frame();
      run(.1f);
      snap("59_reglages_tactiques");
      opened = selectLabel("Seuil") && opened;
      in.press[RIGHT] = true;
      frame();
      bool changed = rules().tactics.size() == 4 && rules().tactics[1].value == 50 && dataDoc(DF_RULES)["tactiques"]["defaut"][1]["valeur"] == 50 &&
                     settings_->dirtyCount() == 1;
      settings_->revert();
      check(opened && changed && rules().tactics[1].value == 40, "réglages : seuil d'une tactique de départ, puis annulation");
    }
    menus.clear();
    team.clear();
    tacticsAuto = false;
    mode = Mode::Title;
    titleMenu();
  }

  // --- Tour des menus : chaque écran est dessiné une fois pour vérifier sa mise en page ---
  {
    auto show = [&](const std::string& name) {
      g.layoutScene = "tour : " + name;
      frame();
    };
    auto back = [&](size_t depth) {
      for (int i = 0; i < 10 && menus.depth() > depth; i++) {
        in.cancel = true;
        frame();
      }
    };
    team = {makeFighter("lior", 40), makeFighter("ronceau", 40), makeFighter("isra", 40), makeFighter("maelle", 40), makeFighter("kael", 40),
            makeFighter("selene", 40), makeFighter("brann", 40), makeFighter("braisenard", 40)};
    items.clear();
    for (auto& d : allItems()) items[d.id] = 99;
    gold = 99999;
    sc.clear();
    menus.clear();
    mode = Mode::Map;
    changeMap(mi("vallee"), 30, 12, DOWN);
    banner = 0;
    pauseMenu();
    show("pause");
    teamMenuAt(1);
    show("équipe");
    menus.clear();
    itemMenu();
    show("objets");
    menus.clear();
    magicMenu();
    show("magie");
    menus.top().sel = 3;  // Maëlle
    in.confirm = true;
    show("magie : sorts");
    menus.clear();
    std::vector<std::string> stock;
    for (auto& d : allItems())
      if (d.price > 0) stock.push_back(d.id);
    shopMenu(stock);
    show("boutique");
    menus.clear();
    tacticsMenu();
    show("tactiques");
    for (int k = 0; k < 3; k++) {  // éditeur de Lior, Ronceau et Isra : liste, condition, action
      menus.top().sel = 2 + k;
      in.confirm = true;
      show("tactiques " + team[k]->name());
      menus.top().sel = 2 + (int)team[k]->tactics.size();
      in.confirm = true;
      show("tactiques : condition");
      in.confirm = true;
      show("tactiques : action");
      back(1);
    }
    menus.clear();
    // Combat : chaque commande et ses sous-menus
    tacticsAuto = false;
    startBattle({makeFighter("chevalier", 30), makeFighter("chef_bandit", 30), makeFighter("mage_givre", 30)}, false, nullptr, false, true);
    for (int i = 0; i < 1500 && !menus.active(); i++) frame();
    if (battle_ && menus.active()) {
      show("combat : commande");
      for (int c = 0; c < (int)menus.top().items.size(); c++) {
        std::string cmd = menus.top().items[c].label;
        menus.top().sel = c;
        in.confirm = true;
        show("combat : " + cmd);
        if (menus.depth() > 1) {
          in.confirm = true;  // première entrée du sous-menu, puis le choix de la cible
          show("combat : " + cmd + " > cible");
        }
        back(1);
        if (mode != Mode::Battle || !menus.active()) break;  // une action est partie (Changer)
      }
    }
    battle_.reset();
    mode = Mode::Map;
    menus.clear();
    sc.clear();
    // Arène : emplacement d'ennemi, modèles, tactiques d'un allié
    if (arena_) {
      Arena& A = *arena_;
      mode = Mode::Arena;
      A.open();
      A.foes[0] = {"chevalier", 30, 2.5f, true};
      menus.top().sel = 8;  // premier ennemi
      in.confirm = true;
      show("arène : ennemi");
      back(1);
      menus.top().sel = (int)menus.top().items.size() - 3;  // Modèles…
      in.confirm = true;
      show("arène : modèles");
      back(1);
      menus.top().sel = 1;  // premier allié
      in.confirm = true;
      menus.top().sel = 2;  // Tactiques…
      in.confirm = true;
      show("arène : tactiques");
      back(1);
      menus.clear();
    }
    // Réglages : listes et fiches avec les noms les plus longs
    if (settings_) {
      Settings& S = *settings_;
      mode = Mode::Settings;
      S.open();
      S.menuSpeciesList("chevalier");
      show("réglages : espèces");
      S.editSpecies("chevalier");
      show("réglages : Chevalier du givre");
      S.menuLearn("chevalier");
      show("réglages : techniques apprises");
      S.editSpecies("ronce_mere");
      S.menuResist("ronce_mere");
      show("réglages : résistances");
      S.editSpecies("golem");
      S.menuImmune("golem");
      show("réglages : immunités");
      S.menuMovesList("fleche");
      show("réglages : techniques");
      S.editMove("fleche");
      show("réglages : Fléchette empoisonnée");
      S.menuEffect("fleche");
      show("réglages : effet");
      S.menuItemsList("lanterne_argent");
      show("réglages : objets");
      S.editItem("lanterne_argent");
      show("réglages : Lanterne d'argent");
      S.menuRules(0);
      S.menuStartItems();
      show("réglages : objets de départ");
      S.menuRules(0);
      S.menuDrops();
      show("réglages : butin");
      menus.clear();
    }
    team.clear();
    items.clear();
    mode = Mode::Title;
    titleMenu();
    show("titre");
    menus.top().sel = 4;  // Outils
    in.confirm = true;
    show("outils");
    menus.clear();
    titleMenu();
  }

  // --- Expédition : monde généré (procgen.hpp) et mode roguelite (expedition.hpp) ---
  {
    expedition_ = std::make_unique<Expedition>(*this);
    Expedition& X = *expedition_;
    X.saveName = "expedition_test.txt";  // ne jamais toucher à l'expédition ni à la progression du joueur
    X.progressName = "expedition_progres_test.txt";
    X.removeSave();
    std::remove(X.path(X.progressName).c_str());
    X.loadProgress();
    auto u8 = [](const std::string& s) {
      int n = 0;
      for (unsigned char c : s) n += (c & 0xC0) != 0x80;
      return n;
    };
    // Plusieurs graines, huit régions chacune : données, cartes (tout accessible à pied) et événements valides
    int bad = 0, checked = 0;
    std::string firstErr;
    size_t nMoves = 0, nSpecies = 0;
    for (uint64_t seed : {1ull, 2ull, 7ull, 42ull, 2026ull, 123456789ull}) {
      try {
        X.generate(seed, 1);
        for (int k = 1; k <= 8; k++) {
          if (k > 1) X.current_ = procgen::generateRegion(seed, k, X.content_), X.region_ = k;
          X.install();
          std::vector<std::string> errs = checkData();
          for (auto& e : checkMaps()) errs.push_back(e);
          for (auto& e : checkEvents()) errs.push_back(e);
          for (auto& m : allMoves())
            if (u8(m.name) > 18) errs.push_back("nom de technique trop long : " + m.name);
          for (auto& sp : allSpecies())
            if (u8(sp.name) > 10) errs.push_back("nom d'espèce trop long : " + sp.name);
          checked++;
          if (!errs.empty() && bad++ == 0) firstErr = "graine " + std::to_string(seed) + ", région " + std::to_string(k) + " : " + errs[0];
        }
        nMoves = allMoves().size(), nSpecies = allSpecies().size();
      } catch (std::exception& e) {
        if (bad++ == 0) firstErr = "graine " + std::to_string(seed) + " : " + e.what();
      }
      X.leave();
    }
    check(bad == 0, "expédition : " + std::to_string(checked) + " régions générées (6 graines) : " + std::to_string(nMoves) + " techniques, " +
                        std::to_string(nSpecies) + " espèces, cartes et événements valides" + (firstErr.empty() ? "" : " — " + firstErr));
    check(hasSpecies("lior") && mapIndex("vallee") >= 0 && findEvent("sylvarque"), "expédition : les données du jeu sont remises en place ensuite");
    // BRUMEVAL_MONDE=brume : affiche le monde de cette graine (noms, types, rôles des trois premières régions)
    if (const char* w = SDL_getenv("BRUMEVAL_MONDE")) {
      uint64_t seed = procgen::seedFromText(w);
      X.generate(seed, 3);
      X.install();
      std::printf("\nMonde de la graine « %s » :\n", w);
      for (auto& id : X.content_.heroes) std::printf("  héros     %-10s %s\n", species(id).name.c_str(), species(id).role.c_str());
      for (auto& id : X.content_.starters) std::printf("  départ    %-10s %s\n", species(id).name.c_str(), typesName(species(id)).c_str());
      for (int k = 1; k <= 3; k++) {
        std::string px = "x" + std::to_string(k);
        std::printf("  région %d  %s (gardien : %s)\n", k, procgen::regionName(seed, k).c_str(), species(px + "b").name.c_str());
        for (auto& s : allSpecies())
          if (s.id.rfind(px, 0) == 0) std::printf("            %-10s %-14s %s\n", s.name.c_str(), typesName(s).c_str(), s.role.c_str());
      }
      for (auto& m : allMoves()) std::printf("  technique %-18s %-8s %s\n", m.name.c_str(), typeName(m.type), moveDetails(m).c_str());
      X.leave();
    }
    {
      procgen::Content a = procgen::generateBase(99), b = procgen::generateBase(99);
      procgen::Region ra = procgen::generateRegion(99, 1, a), rb = procgen::generateRegion(99, 1, b);
      procgen::Region ra2 = procgen::generateRegion(99, 2, a), rb2 = procgen::generateRegion(99, 2, b);
      check(a.moves == b.moves && a.species == b.species && a.looks == b.looks && ra.map == rb.map && ra.events == rb.events && ra2.map == rb2.map &&
                procgen::seedFromText("1234") == 1234 && procgen::seedFromText("brume") == procgen::seedFromText("brume"),
            "expédition : la même graine redonne exactement le même monde");
      // À plusieurs, chaque joueur génère le monde chez lui : l'empreinte doit être la même sous Windows, Mac et Linux
      std::string wh = Expedition::worldHash(2026, 3);
      check(wh == "536d1b3aad5321ce", "expédition : la graine 2026 donne le même monde sur tous les systèmes (empreinte " + wh + ")");
    }
    // Écran titre > Expédition, choix du héros puis de la créature
    titleMenu();
    menus.top().sel = 2;  // Expédition
    in.confirm = true;
    frame();
    run(.2f);
    snap("61_expedition_menu");
    bool onMenu = X.onScreen() && menus.active() && menus.top().title == "Expédition";
    X.start(424242, "");
    run(.2f);
    snap("62_expedition_heros");
    int nHeroes = (int)menus.top().items.size();
    in.confirm = true;
    frame();
    run(.2f);
    snap("63_expedition_creature");
    in.confirm = true;
    frame();
    skipScript();
    run(.3f);
    snap("64_expedition_region");
    check(onMenu && nHeroes == 3 && inExpedition() && mode == Mode::Map && team.size() == 2 && M().id == "expedition_1" && X.savedRegion() == 1,
          "expédition : écran titre > Expédition, choix du héros (3) et de la créature, région 1 (sauvegardée)");
    // Combat contre une créature générée
    startBattle({makeFighter(X.current_.wild.at(0), 3)}, false, nullptr);
    run(.8f);
    snap("65_expedition_combat");
    if (battle_) battle_->autoPlay = true;
    for (int i = 0; i < 60 * 120 && mode == Mode::Battle; i++) {
      in.confirm = true;
      frame();
    }
    skipScript();
    check(mode == Mode::Map && inExpedition() && !team.empty(), "expédition : combat contre une créature générée");
    // Le gardien bloque la sortie ; vaincu, la sortie mène à la région 2 (sauvegarde automatique)
    {
      const BossSpot b = M().bosses.at(0);
      bool wall = blocked(b.x, b.y) && blocked(b.x, b.y + 1);
      flags.insert(b.flag);
      bool open = !blocked(b.x, b.y) && !blocked(b.x + 1, b.y + 1);
      px = X.current_.exitX - 1, py = X.current_.exitY;
      moving = false;
      in.press[RIGHT] = true;
      tryMove(RIGHT);
      for (int i = 0; i < 30 && moving; i++) frame();
      run(.5f);
      snap("66_expedition_region2");
      check(wall && open && X.region() == 2 && M().id == "expedition_2" && X.savedRegion() == 2,
            "expédition : le gardien bloque la route ; vaincu, la sortie mène à la région 2 (sauvegarde automatique)");
    }
    // Sauvegarde, retour au titre (données du jeu remises), puis reprise identique
    {
      std::vector<std::pair<std::string, int>> before, after;
      for (auto& f : team) before.push_back({f->sp, f->lvl});
      int gold0 = gold;
      bool saved = saveGame();
      titleMenu();
      bool restored = !inExpedition() && hasSpecies("lior") && !hasSpecies(before[0].first);
      X.menu();
      in.confirm = true;  // Continuer
      frame();
      for (auto& f : team) after.push_back({f->sp, f->lvl});
      check(saved && restored && inExpedition() && X.region() == 2 && M().id == "expedition_2" && after == before && gold == gold0,
            "expédition : sauvegarde, retour au titre (données du jeu remises), puis reprise à l'identique");
    }
    // Défaite : l'expédition s'arrête, éclats et record gardés, sauvegarde effacée
    {
      int shards0 = X.progress.shards;
      for (auto& f : team) f->hp = 0;
      defeat();
      skipScript();
      run(.2f);
      snap("67_expedition_bilan");
      check(!inExpedition() && mode == Mode::Title && X.onScreen() && X.progress.shards > shards0 && X.progress.record >= 2 && !X.saveExists() &&
                hasSpecies("lior"),
            "expédition : la défaite y met fin (" + std::to_string(X.progress.shards - shards0) + " éclats, record : région " +
                std::to_string(X.progress.record) + "), sauvegarde effacée");
    }
    // Camp : une amélioration achetée propose un héros de plus
    {
      X.progress.shards += 100;
      X.camp(0);
      run(.1f);
      snap("68_expedition_camp");
      int lv = X.progress.up[U_HEROES];
      in.confirm = true;
      frame();
      bool bought = X.progress.up[U_HEROES] == lv + 1;
      X.start(7, "");
      bool four = menus.top().items.size() == 4;
      X.cancelSetup();
      check(bought && four && !inExpedition(), "expédition : le Camp vend une amélioration (4 héros proposés)");
    }
    menus.clear();
    X.leave();
    X.removeSave();
    std::remove(X.path(X.progressName).c_str());
    titleMenu();
  }

  // --- Multijoueur : deux jeux sur cet ordinateur (vrai réseau, 127.0.0.1) : duel, puis expédition à plusieurs ---
  {
    Game guest(g.renderer());
    guest.g.checkLayout = true;
    guest.saveName_ = "sans_sauvegarde_test.txt";  // pas de partie : l'invité prend l'équipe de départ
    online_ = std::make_unique<Online>(*this);
    guest.online_ = std::make_unique<Online>(guest);
    Online &H = *online_, &V = *guest.online_;
    for (Online* o : {&H, &V}) o->settingsName = "multijoueur_test.txt", o->port = 47475;
    H.pseudo = "Hôte", V.pseudo = "Invité";
    H.loopback = true;
    auto both = [&](int frames) {
      for (int i = 0; i < frames; i++) {
        frame();
        guest.update(1 / 60.f);
      }
    };
    // Les deux appuient sur Entrée (messages) jusqu'à ce que leurs scènes soient finies
    auto bothSkip = [&] {
      for (int i = 0; i < 3000 && ((sc.busy() && !menus.active()) || (guest.sc.busy() && !guest.menus.active())); i++) {
        in.confirm = sc.busy() && !menus.active();  // jamais dans un menu ouvert (il choisirait)
        guest.in.confirm = guest.sc.busy() && !guest.menus.active();
        both(1);
      }
    };
    auto snapGuest = [&](const std::string& name) {
      guest.draw();
      SDL_RenderPresent(g.renderer());
      std::string p = out + "/" + name + ".bmp";
      SDL_SaveBMP(target, p.c_str());
      std::printf("capture  %s\n", p.c_str());
      guest.g.layoutScene = name;
    };
    auto connect = [&] {
      V.menu();
      V.join("127.0.0.1");
      for (int i = 0; i < 600 && !(H.players_.size() == 1 && V.state_ == Online::State::Salon); i++) both(1);
    };
    titleMenu();
    menus.top().sel = 3;  // Multijoueur
    in.confirm = true;
    frame();
    run(.1f);
    snap("69_multijoueur");
    bool menuOk = H.onScreen() && menus.top().title == "Multijoueur";
    in.confirm = true;  // Héberger une partie
    frame();
    run(.1f);
    snap("70_multijoueur_attente");
    bool hosting = H.state_ == Online::State::Salon && H.host_ && H.players_.empty();
    connect();
    run(.1f);
    snap("71_multijoueur_salon");
    check(menuOk && hosting && H.state_ == Online::State::Salon && V.state_ == Online::State::Salon && H.players_.size() == 1 &&
              H.players_[0].name == "Invité" && V.nameOf(0) == "Hôte" && V.myId_ == 1,
          "multijoueur : écran titre > Multijoueur, héberger une partie, rejoindre (127.0.0.1), les deux joueurs dans le salon");
    // Duel : niveaux égaux ; l'hôte laisse jouer l'ordinateur, l'invité ses tactiques (mode auto)
    menus.top().sel = 2;  // Niveaux : égaux
    in.confirm = true;
    frame();
    menus.top().sel = 1;  // Duel
    in.confirm = true;
    frame();
    both(5);
    bool started = mode == Mode::Battle && guest.mode == Mode::Battle && battle_ && guest.battle_ && battle_->online() == Battle::Net::Host &&
                   guest.battle_->online() == Battle::Net::Guest && V.sameLevel_ && team.at(0)->lvl == 50;
    if (battle_) battle_->autoPlay = true;
    guest.tacticsAuto = true;
    both(240);
    snap("72_duel_hote");
    snapGuest("73_duel_invite");
    // L'invité voit les mêmes PV que l'hôte (état reçu dix fois par seconde)
    bool mirrored = false;
    for (int i = 0; i < 120 && !mirrored && battle_ && guest.battle_; i++) {
      both(1);
      mirrored = battle_->allies.size() == guest.battle_->foes.size() && battle_->foes.size() == guest.battle_->allies.size();
      for (size_t k = 0; mirrored && k < battle_->allies.size(); k++) mirrored = battle_->allies[k]->hp == guest.battle_->foes[k]->hp;
      for (size_t k = 0; mirrored && k < battle_->foes.size(); k++) mirrored = battle_->foes[k]->hp == guest.battle_->allies[k]->hp;
    }
    size_t guestTurns = 0;
    for (int i = 0; i < 60 * 900 && (mode == Mode::Battle || guest.mode == Mode::Battle); i++) {
      if (guest.battle_) guestTurns = std::max(guestTurns, guest.battle_->log.size());
      both(1);
    }
    both(10);
    bool hostWon = H.status_.find("Victoire") != std::string::npos, guestWon = V.status_.find("Victoire") != std::string::npos;
    check(started && mirrored && guestTurns > 0 && mode == Mode::Title && guest.mode == Mode::Title && H.state_ == Online::State::Salon &&
              V.state_ == Online::State::Salon && hostWon != guestWon,
          "multijoueur : duel complet (niveaux égaux), l'invité voit les mêmes PV et joue ses tours ; " + std::string(hostWon ? "l'hôte" : "l'invité") +
              " gagne, les deux reviennent au salon");
    snapGuest("74_duel_fin_invite");
    // --- Échange de créatures dans le salon (parties principales, fichiers à part) ---
    {
      std::string hostSave = saveName_;
      saveName_ = "echange_hote_test.txt", guest.saveName_ = "echange_invite_test.txt";
      const Rules& r = rules();
      auto makeSave = [&](Game& p, const std::vector<std::pair<std::string, int>>& members) {
        p.team.clear();
        for (auto& [sp, lvl] : members) p.team.push_back(makeFighter(sp, lvl));
        p.mapId = p.respawnMap = 0;
        p.saveGame();
      };
      auto saved = [&](Game& p) {
        p.loadGame();
        std::string s;
        for (auto& f : p.team) s += f->sp + ":" + std::to_string(f->lvl) + " ";
        return s;
      };
      const std::string hero = r.hero, a = r.starters.at(0), b = r.starters.at(1), c = r.starters.at(2);
      makeSave(*this, {{hero, 10}, {a, 8}, {b, 9}});
      makeSave(guest, {{hero, 10}, {c, 7}});
      // Une contre une : l'hôte propose a, l'invité donne c en retour, l'hôte accepte
      H.offerTrade(1, 1, false);
      both(10);
      bool asked = guest.menus.active() && guest.menus.top().title == "Échange : avec Hôte" && V.trade_.step == Online::Trade::Step::Answering;
      snapGuest("83_echange_invite");
      V.answerTrade(1);
      both(10);
      bool countered = menus.active() && menus.top().title == "Échange : avec Invité" && H.trade_.step == Online::Trade::Step::Countered;
      snap("84_echange_hote");
      in.confirm = true;  // Accepter
      frame();
      both(10);
      std::string h1 = saved(*this), v1 = saved(guest);
      check(asked && countered && h1 == hero + ":10 " + c + ":7 " + b + ":9 " && v1 == hero + ":10 " + a + ":8 " &&
                H.status_.rfind("Échange fait", 0) == 0 && V.status_.rfind("Échange fait", 0) == 0,
            "échange (salon) : une créature contre une, chacun la reçoit dans la sauvegarde de sa partie principale");
      // Cadeau : l'invité offre sa créature, l'hôte l'accepte ; l'invité n'a plus que son héros
      V.offerTrade(0, 1, true);
      both(10);
      bool giftAsked = menus.active() && menus.top().title == "Échange : cadeau de Invité";
      in.confirm = true;  // Accepter
      frame();
      both(10);
      std::string h2 = saved(*this), v2 = saved(guest);
      check(giftAsked && h2 == h1 + a + ":8 " && v2 == hero + ":10 " && H.status_.rfind("Cadeau reçu", 0) == 0 &&
                V.status_.rfind("Cadeau envoyé", 0) == 0,
            "échange (salon) : un cadeau, sans rien en retour");
      // Une contre une avec un joueur qui n'a plus de créature : refusé tout de suite
      H.offerTrade(1, 1, false);
      both(10);
      check(H.trade_.step == Online::Trade::Step::None && H.status_.find("aucune créature") != std::string::npos && saved(*this) == h2,
            "échange (salon) : refusé si l'autre n'a aucune créature à donner (« " + H.status_ + " »)");
      for (Game* p : {this, &guest}) std::remove(H.file(p->saveName_).c_str());
      saveName_ = hostSave, guest.saveName_ = "sans_sauvegarde_test.txt";
      H.salon(), V.salon();
    }
    // L'invité s'en va : l'hôte reste dans le salon, prévenu
    V.leave();
    both(30);
    check(H.state_ == Online::State::Salon && H.players_.empty() && H.peers_.empty() && H.status_.find("parti") != std::string::npos,
          "multijoueur : l'invité quitte, l'hôte le voit et attend d'autres joueurs (« " + H.status_ + " »)");
    // Version différente : refusée
    H.onHello(9, Json{{"t", "bonjour"}, {"version", "0.1"}, {"donnees", H.hash_}, {"pseudo", "Ancien"}});
    check(H.status_.find("Versions différentes") != std::string::npos, "multijoueur : une autre version du jeu est refusée");

    // --- Expédition à plusieurs : même monde, chacun de son côté, le gardien ensemble ---
    Expedition& XH = *expedition_;  // fichiers « _test » (tests de l'expédition)
    guest.expedition_ = std::make_unique<Expedition>(guest);
    Expedition& XV = *guest.expedition_;
    XH.groupName = "expedition_groupe_test.txt", XH.groupTag = "hote";
    XV.saveName = "expedition_invite_test.txt", XV.groupName = "expedition_groupe_test.txt", XV.groupTag = "invite";
    XV.progressName = "expedition_progres_invite_test.txt";
    for (Expedition* x : {&XH, &XV}) {
      x->removeSave(true);
      std::remove(x->path(x->progressName).c_str());
    }
    connect();
    menus.top().sel = 0;  // Expédition
    in.confirm = true;
    frame();
    run(.1f);
    snap("75_groupe_menu");
    bool groupMenuOk = menus.top().title == "Expédition à plusieurs" && menus.top().items.size() == 4;
    uint64_t seed = procgen::seedFromText("groupe");
    menus.clear();
    // Les deux jeux partagent ici les mêmes données en mémoire : l'invité met de côté les vraies données
    // (celles que l'hôte a mises de côté), pas le monde déjà installé par l'hôte
    auto shareBackup = [&] {
      XV.docs_ = XH.docs_, XV.maps_ = XH.maps_, XV.events_ = XH.events_;
      XV.backedUp_ = true;
    };
    H.launch(seed, "groupe", true);
    shareBackup();
    both(10);
    snapGuest("76_groupe_heros_invite");
    bool choosing = H.inGroup() && V.inGroup() && XH.onScreen() && XV.onScreen() && XH.seed() == seed && XV.seed() == seed &&
                    XH.content_.heroes == XV.content_.heroes && XV.seedText() == "groupe";
    XH.begin(XH.content_.heroes.at(0), XH.content_.starters.at(0));
    XV.begin(XV.content_.heroes.at(1), XV.content_.starters.at(1));
    bothSkip();
    // L'invité fait deux pas : l'hôte le voit glisser jusqu'à sa case
    for (int i = 0; i < 20; i++) {
      guest.in.hold[RIGHT] = true;
      both(1);
    }
    guest.in.hold[RIGHT] = false;
    both(30);
    snap("77_groupe_carte_hote");
    auto seenH = H.avatars(), seenV = V.avatars();
    bool seen = seenH.size() == 1 && seenV.size() == 1 && seenH[0].x == (float)guest.px && seenH[0].y == (float)guest.py && guest.px != px &&
                seenH[0].name == "Invité" && seenV[0].name == "Hôte" && guest.mode == Mode::Map;
    check(groupMenuOk && choosing && seen,
          "expédition à plusieurs : lancée depuis le salon, chacun choisit son héros dans le même monde et voit l'autre se déplacer");
    // Menu de pause > Groupe : où en sont les autres
    pauseMenu();
    bool hasGroup = false;
    for (auto& it : menus.top().items) hasGroup = hasGroup || it.label == "Groupe";
    H.groupMenu();
    run(.1f);
    snap("78_groupe_pause");
    check(hasGroup && menus.top().title == "Groupe" && menus.top().items.size() == 4, "expédition à plusieurs : menu de pause > Groupe");
    menus.clear();
    // Gardien : l'hôte attend devant lui, l'invité arrive, le combat commence chez les deux (deux combattants chacun)
    for (Game* p : {this, &guest})
      for (auto& f : p->team) {
        f->lvl = 30;
        f->recalc();
        f->hp = f->mhp, f->mp = f->mmp;
      }
    const BossSpot boss = M().bosses.at(0);
    auto toGuardian = [&](Game& p) {
      p.px = boss.x - 1, p.py = boss.y, p.dir = RIGHT;
      p.moving = false;
    };
    toGuardian(*this);
    interact();
    skipScript();
    both(10);
    snap("79_groupe_attente_gardien");
    bool waiting = H.ready_ == 1 && menus.active() && menus.top().title == "Gardien : le groupe" && mode == Mode::Map && !H.missing(1).empty() &&
                   V.players_.at(0).ready == 1;
    toGuardian(guest);
    guest.interact();
    for (int i = 0; i < 600 && !(mode == Mode::Battle && guest.mode == Mode::Battle); i++) {
      guest.in.confirm = guest.sc.busy() && !guest.menus.active();
      both(1);
    }
    bool fought = mode == Mode::Battle && guest.mode == Mode::Battle && battle_ && guest.battle_ && battle_->online() == Battle::Net::Lead &&
                  guest.battle_->online() == Battle::Net::Follow && battle_->allies.size() == 4 && guest.battle_->allies.size() == 4 &&
                  battle_->foes.size() == guest.battle_->foes.size() && battle_->foes.at(1)->mhp == guest.battle_->foes.at(1)->mhp;
    if (battle_) {
      battle_->autoPlay = true;
      // Les combattants de l'invité (les deux derniers alliés) jouent d'abord : sans ça, l'équipe de
      // l'hôte, au niveau 30, gagne parfois avant que l'invité ait eu un seul tour
      for (size_t k = 0; k < battle_->allies.size(); k++) battle_->allies[k]->atb = k >= 2 ? 99.f : 0.f;
    }
    both(150);
    snap("80_groupe_gardien_hote");
    snapGuest("81_groupe_gardien_invite");
    bool same = false;
    for (int i = 0; i < 120 && !same && battle_ && guest.battle_; i++) {
      both(1);
      same = true;
      for (size_t k = 0; same && k < battle_->allies.size(); k++) same = battle_->allies[k]->hp == guest.battle_->allies[k]->hp;
      for (size_t k = 0; same && k < battle_->foes.size(); k++) same = battle_->foes[k]->hp == guest.battle_->foes[k]->hp;
    }
    int gold0 = guest.gold, xp0 = guest.team.at(0)->xp, lvl0 = guest.team.at(0)->lvl;
    size_t turns = 0;
    for (int i = 0; i < 60 * 900 && (mode == Mode::Battle || guest.mode == Mode::Battle); i++) {
      if (guest.battle_) turns = std::max(turns, guest.battle_->log.size());
      both(1);
    }
    bothSkip();
    bool won = XH.beaten() && XV.beaten() && guest.gold > gold0 && (guest.team.at(0)->xp != xp0 || guest.team.at(0)->lvl > lvl0);
    check(waiting && fought && same && turns > 0 && won,
          "expédition à plusieurs : le gardien attend tout le groupe, puis combat à quatre alliés (tours de l'invité : " + std::to_string(turns) +
              ", mêmes PV chez les deux) ; gagné, chacun reçoit or et expérience");
    // Défaite en solo : réveil au village, moitié de l'or ; l'expédition continue
    guest.gold = 100;
    guest.defeat();
    bothSkip();
    check(XV.active() && V.inGroup() && guest.gold == 50 && guest.px == XV.current().startX && guest.mode == Mode::Map,
          "expédition à plusieurs : une défaite en solo ramène au village (moitié de l'or), l'expédition continue");
    // L'hôte passe en région 2 : l'invité ne le voit plus sur sa carte ; le gardien 2 attendra l'invité
    px = XH.current().exitX - 1, py = XH.current().exitY;
    moving = false;
    in.press[RIGHT] = true;
    tryMove(RIGHT);
    for (int i = 0; i < 30 && moving; i++) both(1);
    both(30);
    check(XH.region() == 2 && V.players_.at(0).region == 2 && V.avatars().empty() && H.session_.region == 2 && !H.missing(2).empty(),
          "expédition à plusieurs : l'hôte passe en région 2 (sauvegardée), l'invité le sait ; le gardien 2 l'attendra");
    // L'hôte s'en va : l'invité revient au menu, son expédition sauvegardée ; puis tous deux la reprennent
    int lvlV = guest.team.at(0)->lvl;
    titleMenu();  // sauvegarde déjà faite au passage de région ; ferme la partie et les connexions
    for (int i = 0; i < 120 && V.inGroup(); i++) guest.update(1 / 60.f);
    snapGuest("82_groupe_hote_parti");
    bool backToMenu = !V.inGroup() && V.state_ == Online::State::Menu && !XV.active() && XV.savedRegion(true) == 1 &&
                      V.status_.find("sauvegardée") != std::string::npos && hasSpecies("lior");
    H.menu();
    H.host();
    connect();
    uint64_t saved = 0;
    XH.savedRegion(true, &saved);
    H.launch(saved, "", false);
    shareBackup();
    both(10);
    bothSkip();
    check(backToMenu && saved == seed && H.inGroup() && V.inGroup() && XH.region() == 2 && !XH.onScreen() && XV.region() == 1 && !XV.onScreen() &&
              XV.beaten() && guest.team.at(0)->lvl == lvlV && guest.mode == Mode::Map,
          "expédition à plusieurs : l'hôte part (l'invité garde sa sauvegarde), puis chacun reprend la sienne dans le même monde");
    // Échange pendant l'expédition : l'hôte (région 2) offre sa créature à l'invité (région 1)
    {
      int slot = -1;
      for (size_t i = 0; i < team.size() && slot < 0; i++)
        if (!team[i]->S().human) slot = (int)i;
      size_t nH = team.size(), nV = guest.team.size();
      std::string sp = slot >= 0 ? team[(size_t)slot]->sp : "";
      int lvl = slot >= 0 ? team[(size_t)slot]->lvl : 0;
      both(10);
      H.offerTrade(1, slot, true);
      both(10);
      bool asked = guest.menus.active() && guest.menus.top().title == "Échange : cadeau de Hôte";
      snapGuest("85_groupe_cadeau_invite");
      guest.in.confirm = true;  // Accepter
      both(1);
      both(10);
      check(slot >= 0 && asked && team.size() == nH - 1 && guest.team.size() == nV + 1 && guest.team.back()->sp == sp &&
                guest.team.back()->lvl == lvl && guest.notice_.rfind("Cadeau reçu", 0) == 0 && !guest.menus.active() && guest.mode == Mode::Map,
            "échange (expédition) : l'hôte offre une créature à l'invité, qui la reçoit dans son expédition");
    }
    for (auto& s : guest.g.layoutIssues) std::printf("         (invité) %s\n", s.c_str());
    check(guest.g.layoutIssues.empty(), "multijoueur : mise en page de l'écran de l'invité");
    V.leave();
    H.leave();
    guest.expedition_->leave();
    XH.leave();
    for (Expedition* x : {&XH, &XV}) {
      x->removeSave(true);
      x->removeSave();
      std::remove(x->path(x->progressName).c_str());
    }
    std::remove(H.file(H.settingsName).c_str());
    online_.reset();
    titleMenu();
  }

  // --- Mise en page de toutes les images dessinées pendant les tests ---
  for (auto& s : g.layoutIssues) std::printf("         %s\n", s.c_str());
  check(g.layoutIssues.empty(), "mise en page : aucun texte ne dépasse ni ne se chevauche, aucune fenêtre de travers (" +
                                    std::to_string(g.layoutIssues.size()) + " problème(s))");
  g.checkLayout = false;

  // --- Simulation d'équilibrage (IA simple, sans objets) ---
  std::printf("\nÉquilibrage (combats simulés, IA automatique) :\n");
  // BRUMEVAL_SIMULATIONS=5 : cinq fois plus de combats par ligne (mesure plus précise, plus longue)
  const char* simEnv = SDL_getenv("BRUMEVAL_SIMULATIONS");
  int simFactor = simEnv ? std::max(1, std::atoi(simEnv)) : 1;
  auto simSetup = [&](const char* name, std::vector<std::pair<std::string, int>> party, std::function<BattleSetup()> mk, int n,
                      bool tac = false) {
    n *= simFactor;
    int wins = 0;
    float total = 0;
    for (int k = 0; k < n; k++) {
      team.clear();
      for (auto& [id, l] : party) team.push_back(makeFighter(id, l));
      menus.clear();
      sc.clear();
      mode = Mode::Map;
      BattleResult res = BattleResult::Lose;
      bool done = false;
      startBattle(mk(), [&](BattleResult r) {
        res = r;
        done = true;
      });
      battle_->autoPlay = true;
      battle_->simTactics = tac;  // alliés guidés par leurs tactiques de départ plutôt que par l'IA
      float t = 0;
      std::vector<std::string> lastLog;
      const char* want = SDL_getenv("BRUMEVAL_JOURNAL");  // ex. BRUMEVAL_JOURNAL=Sylvarque
      bool keep = k == 0 && want && *want && std::string(name).find(want) != std::string::npos;
      for (int i = 0; i < 20 * 900 && !done; i++) {
        in.confirm = true;
        if (keep && battle_) lastLog = battle_->log;
        update(1 / 20.f);
        t += 1 / 20.f;
      }
      if (res == BattleResult::Win) wins++;
      total += t;
      if (keep) {
        for (auto& l : lastLog) std::printf("      %s\n", l.c_str());
        std::printf("      => %s\n", res == BattleResult::Win ? "victoire" : "défaite");
      }
    }
    std::printf("  %-34s %3d %% de victoires   %4.0f s en moyenne\n", name, wins * 100 / n, total / n);
  };
  auto sim = [&](const char* name, std::vector<std::pair<std::string, int>> party, std::function<std::vector<FighterP>()> mk, bool boss,
                 int n, bool tac = false) {
    simSetup(name, party, [mk, boss] {
      BattleSetup s;
      s.foes = mk();
      s.boss = boss, s.canFlee = !boss, s.canCapture = !boss;
      return s;
    }, n, tac);
  };
  auto group = [](std::vector<std::string> pool, int lo, int hi, int n) {
    return [=] {
      std::vector<FighterP> v;
      for (int i = 0; i < n; i++) v.push_back(makeFighter(pool[irand(0, (int)pool.size() - 1)], irand(lo, hi)));
      return v;
    };
  };
  auto bossF = [](const std::string& sp, int lvl, float mult) {
    auto f = makeFighter(sp, lvl);
    f->mhp = int(f->mhp * mult + 1e-4f);
    f->hp = f->mhp;
    f->boss = true;
    return f;
  };
  sim("Début (N.5 contre N.2-4)", {{"lior", 5}, {"braisenard", 5}}, group({"piafouine", "mulotin", "champichou"}, 2, 4, 2), false, 30);
  simSetup("Braconnier (équipe N.6)", {{"lior", 6}, {"braisenard", 6}}, [] {
    BattleSetup s;
    s.foes = {makeFighter("braconnier", 6), makeFighter("mulotin", 5)};
    s.reserve = {makeFighter("piafouine", 5)};
    s.foeName = "Le braconnier";
    s.canFlee = s.canCapture = false;
    return s;
  }, 30);
  sim("Bois (N.8 contre 3 x N.4-7)", {{"lior", 8}, {"maelle", 8}, {"braisenard", 8}},
      group({"champichou", "mulotin", "lucioline", "piafouine"}, 4, 7, 3), false, 30);
  sim("Bosquet du col (N.10 contre N.7-10)", {{"lior", 10}, {"maelle", 10}, {"gouttelin", 10}},
      group({"rocaillou", "grenouillon", "lucioline", "brumelin"}, 7, 10, 3), false, 30);
  sim("Boss Sylvarque (équipe N.11)", {{"lior", 11}, {"maelle", 11}, {"gouttelin", 11}},
      [&] { return std::vector<FighterP>{makeFighter("brumelin", 9), bossF("sylvarque", 12, 3.5f), makeFighter("brumelin", 9)}; }, true, 40);
  sim("Duel contre Brann (équipe N.14)", {{"lior", 14}, {"maelle", 14}, {"ronceau", 14}}, [&] {
    auto b = makeFighter("brann", 14);
    b->mhp = b->mhp * 5 / 2;
    b->hp = b->mhp;
    b->boss = true;
    return std::vector<FighterP>{b};
  }, true, 20);
  sim("Cendrelune (N.15 contre N.12-15)", {{"lior", 15}, {"maelle", 15}, {"brann", 15}},
      group({"tisonnel", "galetor", "voltigeon", "glaconnet"}, 12, 15, 3), false, 30);
  sim("Golem de suie (équipe N.16)", {{"lior", 16}, {"maelle", 16}, {"brann", 16}},
      [&] { return std::vector<FighterP>{bossF("golem", 16, 3)}; }, true, 20);
  simSetup("Bandit de la forêt (équipe N.12)", {{"lior", 12}, {"maelle", 12}, {"braisenard", 12}}, [] {
    BattleSetup s;
    s.foes = {makeFighter("bandit", 12), makeFighter("louvet", 11)};
    s.foeName = "Le bandit";
    s.canFlee = s.canCapture = false;
    return s;
  }, 30);
  sim("Sylve-Noire (N.13 contre N.10-13)", {{"lior", 13}, {"maelle", 13}, {"kael", 13}},
      group({"louvet", "serpentin", "chauvenuit", "esprillon", "champichou"}, 10, 13, 3), false, 30);
  simSetup("Chef des bandits (équipe N.13)", {{"lior", 13}, {"maelle", 13}, {"gouttelin", 13}}, [&] {
    BattleSetup s;
    s.foes = {bossF("chef_bandit", 13, 2), makeFighter("louvet", 12)};
    s.reserve = {makeFighter("bandit", 12), makeFighter("louvet", 12)};
    s.foeName = "Le chef des bandits";
    s.boss = true;
    s.canFlee = s.canCapture = false;
    return s;
  }, 30);
  simSetup("Ronce-Mère (équipe N.16)", {{"lior", 16}, {"maelle", 16}, {"kael", 16}}, [&] {
    BattleSetup s;
    s.foes = {makeFighter("serpentin", 14), bossF("ronce_mere", 18, 4), makeFighter("serpentin", 14)};
    s.reserve = {makeFighter("crapoison", 13)};
    s.boss = true;
    s.canFlee = s.canCapture = false;
    return s;
  }, 30);
  sim("Boss Ignarok (équipe N.22)", {{"lior", 22}, {"maelle", 22}, {"isra", 22}},
      [&] { return std::vector<FighterP>{makeFighter("tisonnel", 19), bossF("ignarok", 23, 4), makeFighter("tisonnel", 19)}; }, true, 40);
  sim("Pics Givrés (N.25 contre N.21-24)", {{"lior", 25}, {"maelle", 25}, {"isra", 25}},
      group({"givrelin", "zephyrin", "ferrolin", "cristallin", "etincelot"}, 21, 24, 3), false, 30);
  simSetup("Chevalier du givre (équipe N.25)", {{"lior", 25}, {"maelle", 25}, {"isra", 25}}, [] {
    BattleSetup s;
    s.foes = {makeFighter("chevalier", 26), makeFighter("givrelin", 25)};
    s.reserve = {makeFighter("mage_givre", 26)};
    s.foeName = "Le chevalier du givre";
    s.canFlee = s.canCapture = false;
    return s;
  }, 30);
  simSetup("Duel contre Sélène (équipe N.26)", {{"lior", 26}, {"maelle", 26}, {"isra", 26}}, [&] {
    BattleSetup s;
    s.foes = {bossF("selene", 26, 3)};
    s.foeName = "Sélène";
    s.boss = true;
    s.canFlee = s.canCapture = false;
    return s;
  }, 30);
  simSetup("Givrecorne (équipe N.30)", {{"lior", 30}, {"maelle", 30}, {"selene", 30}}, [&] {
    BattleSetup s;
    s.foes = {makeFighter("givrelin", 27), bossF("givrecorne", 31, 4), makeFighter("givrelin", 27)};
    s.reserve = {makeFighter("cristallin", 26)};
    s.boss = true;
    s.canFlee = s.canCapture = false;
    return s;
  }, 40);

  std::printf("\nMêmes combats, alliés guidés par les tactiques de départ :\n");
  sim("Bois (N.8 contre 3 x N.4-7)", {{"lior", 8}, {"maelle", 8}, {"braisenard", 8}},
      group({"champichou", "mulotin", "lucioline", "piafouine"}, 4, 7, 3), false, 30, true);
  sim("Boss Sylvarque (équipe N.11)", {{"lior", 11}, {"maelle", 11}, {"gouttelin", 11}},
      [&] { return std::vector<FighterP>{makeFighter("brumelin", 9), bossF("sylvarque", 12, 3.5f), makeFighter("brumelin", 9)}; }, true, 30, true);
  sim("Boss Ignarok (équipe N.22)", {{"lior", 22}, {"maelle", 22}, {"isra", 22}},
      [&] { return std::vector<FighterP>{makeFighter("tisonnel", 19), bossF("ignarok", 23, 4), makeFighter("tisonnel", 19)}; }, true, 30, true);
  simSetup("Givrecorne (équipe N.30)", {{"lior", 30}, {"maelle", 30}, {"selene", 30}}, [&] {
    BattleSetup s;
    s.foes = {makeFighter("givrelin", 27), bossF("givrecorne", 31, 4), makeFighter("givrelin", 27)};
    s.reserve = {makeFighter("cristallin", 26)};
    s.boss = true;
    s.canFlee = s.canCapture = false;
    return s;
  }, 30, true);

  // Expédition : gardien de chaque région contre une équipe générée (héros, créature de départ,
  // recrue de la région 1) arrivée deux niveaux au-dessus des créatures sauvages
  std::printf("\nExpédition (gardiens, équipe générée au niveau de la région + 2) :\n");
  if (expedition_) {
    Expedition& X = *expedition_;
    std::function<const Json*(const Json&)> findCombat = [&](const Json& list) -> const Json* {
      for (auto& a : list) {
        if (jget<std::string>(a, "action", "") == "combat") return &a;
        for (const char* key : {"oui", "non", "alors", "sinon"})
          if (a.contains(key))
            if (const Json* c = findCombat(a[key])) return c;
      }
      return nullptr;
    };
    for (int k : {1, 2, 3, 5, 8})
      for (uint64_t seed : {11ull, 22ull, 33ull}) {
        X.generate(seed, k);
        X.install();
        mapId = 0;
        int L = procgen::regionLevel(k), lvl = L + 2;
        const Json* fight = findCombat(X.current_.events["x" + std::to_string(k) + "_gardien"]["actions"]);
        if (!fight) continue;
        Json f = *fight;
        auto mk = [f, L] {
          BattleSetup s;
          auto make = [&](const Json& e) {
            auto x = makeFighter(e.at("espece").get<std::string>(), jget(e, "niveau", L));
            float mult = jget(e, "pv", 1.f);
            if (mult != 1.f) x->mhp = std::max(1, int(x->mhp * mult + 1e-4f));
            x->hp = x->mhp;
            x->boss = jget(e, "boss", false);
            return x;
          };
          for (auto& e : f.at("ennemis")) s.foes.push_back(make(e));
          Json res = f.value("renforts", Json::array());  // variable : la boucle doit la garder en vie
          for (auto& e : res) s.reserve.push_back(make(e));
          s.boss = true;
          s.canFlee = s.canCapture = false;
          return s;
        };
        std::vector<std::pair<std::string, int>> party = {{X.content_.heroes[0], lvl}, {X.content_.starters[0], lvl}, {"x1r", lvl}};
        std::string name = "Région " + std::to_string(k) + " (graine " + std::to_string(seed) + ", N." + std::to_string(lvl) + ")";
        simSetup(name.c_str(), party, mk, 15);
        // Gardien à plusieurs (coop.cpp) : 2 combattants chacun à deux, 1 chacun à trois ou quatre ;
        // PV des ennemis × alliés / 3 au-delà de trois alliés. Chaque joueur a son héros et sa créature.
        if (k != 2 && k != 8)
          for (int players : {2, 3, 4}) {
            size_t per = players == 2 ? 2 : 1;
            auto& H = X.content_.heroes;
            auto& S = X.content_.starters;
            auto coop = [mk, players, per, H, S, lvl] {
              BattleSetup s = mk();
              for (int p = 0; p < players; p++) {
                s.allies.push_back(makeFighter(H[(size_t)p % H.size()], lvl));
                if (per > 1) s.allies.push_back(makeFighter(S[(size_t)p % S.size()], lvl));
              }
              float scale = std::max(1.f, s.allies.size() / 3.f);
              for (auto* v : {&s.foes, &s.reserve})
                for (auto& f : *v) {
                  f->mhp = std::max(1, int(f->mhp * scale + 1e-4f));
                  f->hp = f->mhp;
                }
              return s;
            };
            std::string cn = "  à " + std::to_string(players) + " (" + std::to_string(players * (int)per) + " alliés)";
            simSetup(cn.c_str(), party, coop, 15);
          }
        if (seed == 11) {
          // Combats ordinaires de la même région : un dresseur, puis un groupe sauvage de la zone la plus forte
          for (auto& [id, ev] : X.current_.events.items())
            if (id.find("_dresseur") != std::string::npos) {
              if (const Json* c = findCombat(ev["pages"][0]["actions"])) {
                Json tf = *c;
                std::string tn = "  dresseur, région " + std::to_string(k);
                simSetup(tn.c_str(), party, [tf, L] {
                  BattleSetup s;
                  for (auto& e : tf.at("ennemis")) s.foes.push_back(makeFighter(e.at("espece").get<std::string>(), jget(e, "niveau", L)));
                  Json res = tf.value("renforts", Json::array());
                  for (auto& e : res) s.reserve.push_back(makeFighter(e.at("espece").get<std::string>(), jget(e, "niveau", L)));
                  s.canFlee = s.canCapture = false;
                  return s;
                }, 10);
              }
              break;
            }
          const Zone& z = M().zones.back();
          std::string wn = "  sauvages, région " + std::to_string(k);
          sim(wn.c_str(), party, group(z.pool, z.lo, z.hi, z.maxN), false, 10);
        }
        X.leave();
      }
  }

  std::remove(savePath().c_str());
  std::printf("\n%s (%d échec%s)\n", fails ? "TESTS EN ÉCHEC" : "TOUS LES TESTS PASSENT", fails, fails > 1 ? "s" : "");
  return fails ? 1 : 0;
}
