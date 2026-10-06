// Mode test (brumeval --test DOSSIER) : joue automatiquement quelques scènes,
// vérifie les données, simule des combats pour l'équilibrage et enregistre
// des captures d'écran (.bmp) dans DOSSIER. Renvoie 0 si tout va bien.
#include <cstdio>
#include <filesystem>

#include "battle.hpp"
#include "events.hpp"
#include "game.hpp"

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

  // --- Simulation d'équilibrage (IA simple, sans objets) ---
  std::printf("\nÉquilibrage (combats simulés, IA automatique) :\n");
  auto sim = [&](const char* name, std::vector<std::pair<std::string, int>> party, std::function<std::vector<FighterP>()> mk, bool boss,
                 int n) {
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
      startBattle(mk(), boss, [&](BattleResult r) {
        res = r;
        done = true;
      }, !boss, !boss);
      battle_->autoPlay = true;
      float t = 0;
      for (int i = 0; i < 20 * 900 && !done; i++) {
        in.confirm = true;
        update(1 / 20.f);
        t += 1 / 20.f;
      }
      if (res == BattleResult::Win) wins++;
      total += t;
    }
    std::printf("  %-34s %3d %% de victoires   %4.0f s en moyenne\n", name, wins * 100 / n, total / n);
  };
  auto group = [](std::vector<std::string> pool, int lo, int hi, int n) {
    return [=] {
      std::vector<FighterP> v;
      for (int i = 0; i < n; i++) v.push_back(makeFighter(pool[irand(0, (int)pool.size() - 1)], irand(lo, hi)));
      return v;
    };
  };
  auto bossF = [](const std::string& sp, int lvl, int mult) {
    auto f = makeFighter(sp, lvl);
    f->mhp *= mult;
    f->hp = f->mhp;
    f->boss = true;
    return f;
  };
  sim("Début (N.5 contre N.2-4)", {{"lior", 5}, {"braisenard", 5}}, group({"piafouine", "mulotin", "champichou"}, 2, 4, 2), false, 30);
  sim("Bois (N.8 contre 3 x N.4-7)", {{"lior", 8}, {"maelle", 8}, {"braisenard", 8}},
      group({"champichou", "mulotin", "lucioline", "piafouine"}, 4, 7, 3), false, 30);
  sim("Bosquet du col (N.10 contre N.7-10)", {{"lior", 10}, {"maelle", 10}, {"gouttelin", 10}},
      group({"rocaillou", "grenouillon", "lucioline", "brumelin"}, 7, 10, 3), false, 30);
  sim("Boss Sylvarque (équipe N.11)", {{"lior", 11}, {"maelle", 11}, {"gouttelin", 11}},
      [&] { return std::vector<FighterP>{makeFighter("brumelin", 9), bossF("sylvarque", 12, 3), makeFighter("brumelin", 9)}; }, true, 20);
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
  sim("Boss Ignarok (équipe N.22)", {{"lior", 22}, {"maelle", 22}, {"isra", 22}},
      [&] { return std::vector<FighterP>{makeFighter("tisonnel", 19), bossF("ignarok", 23, 4), makeFighter("tisonnel", 19)}; }, true, 20);

  std::printf("\n%s (%d échec%s)\n", fails ? "TESTS EN ÉCHEC" : "TOUS LES TESTS PASSENT", fails, fails > 1 ? "s" : "");
  return fails ? 1 : 0;
}
