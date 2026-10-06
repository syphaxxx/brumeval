// Mode test (brumeval --test DOSSIER) : joue automatiquement quelques scènes,
// vérifie les données, simule des combats pour l'équilibrage et enregistre
// des captures d'écran (.bmp) dans DOSSIER. Renvoie 0 si tout va bien.
#include <cstdio>

#include <filesystem>

#include "battle.hpp"
#include "events.hpp"
#include "game.hpp"
#include "sprites.hpp"

int Game::selfTest(SDL_Surface* target, const std::string& out) {
  std::filesystem::create_directories(out);
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
  check(fillText("Lior part avec {compagnon}.") == "Lior part avec Braisenard.", "les textes remplacent {compagnon} par son nom");

  in.menu = true;
  frame();
  snap("04_menu");
  in.confirm = true;
  frame();
  snap("05_equipe");
  menus.clear();
  panelMode_ = 0;

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

  // --- Combat piloté par les menus ---
  startBattle({makeFighter("mulotin", 4), makeFighter("champichou", 5), makeFighter("piafouine", 4)}, false, nullptr);
  for (int i = 0; i < 1500 && !menus.active(); i++) frame();
  check(menus.active(), "le menu de commande s'ouvre quand une jauge ATB est pleine");
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

  // --- Simulation d'équilibrage (IA simple, sans objets) ---
  std::printf("\nÉquilibrage (combats simulés, IA automatique) :\n");
  auto simSetup = [&](const char* name, std::vector<std::pair<std::string, int>> party, std::function<BattleSetup()> mk, int n) {
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
                 int n) {
    simSetup(name, party, [mk, boss] {
      BattleSetup s;
      s.foes = mk();
      s.boss = boss, s.canFlee = !boss, s.canCapture = !boss;
      return s;
    }, n);
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
    s.foes = {makeFighter("givrelin", 27), bossF("givrecorne", 31, 4.5f), makeFighter("givrelin", 27)};
    s.reserve = {makeFighter("cristallin", 26)};
    s.boss = true;
    s.canFlee = s.canCapture = false;
    return s;
  }, 40);

  std::printf("\n%s (%d échec%s)\n", fails ? "TESTS EN ÉCHEC" : "TOUS LES TESTS PASSENT", fails, fails > 1 ? "s" : "");
  return fails ? 1 : 0;
}
