// Fichier généré : cartes du jeu. Vous pouvez le modifier à la main.
// Chaque ligne de "rows" est une rangée de tuiles (voir la légende dans world.hpp).
#include "world.hpp"

bool tileWalkable(char c) { return c=='.'||c==','||c=='='||c=='F'||c=='B'||c=='a'||c=='g'||c=='b'||c=='k'||c=='c'; }
bool tileEncounter(char c) { return c==','||c=='g'||c=='c'; }

static std::vector<MapDef> build() {
  std::vector<MapDef> v;
  {
    MapDef m;
    m.name = "Vallée de Brumeval";
    m.theme = Theme::Vallee;
    m.rows = {
        "TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT~~TTTTTTTTTTTTTTTTTTTTTTTTTTT",
        "T......F....................F......~~......F...................T",
        "T..........F.=..............,T,,,..~~..,,,,,......,,,T,,,R.....T",
        "T...........F=............,,,,,,,,,~~,,,,,,,,,T.,,,,,,T,,,,....T",
        "T............=............,R,,,,,,,~~,RR,,,,,,R,,,,,,,,,,,,T..TT",
        "T............=............,,,RR,,,,~~,T,,,,,,,,,,,,,,T,,,,,,...T",
        "T............=.WW...........,,,,,..~~,,R,,,,,,,,,,,,,T,,,,,,...T",
        "T......=.....=.WW...=..............~~,,,,,,,,T..,,,,,TT,,,,....T",
        "T......=.....=......=.....TT.......~~..,,,,,...T..,,,,,,,T.....T",
        "T.=================================BB==========....T...........T",
        "T............=................T..R.~~.........=..........,T,...T",
        "T............=............TT.......~~.......FT=.........,,TR,..T",
        "T.F..........=............R,,,,T...~~......T,,=T,,,....,,,,,,,.T",
        "T............=............R,,,,,,..~~TR...,,,,=,,,,,,..,,,,,,,.T",
        "T............=............,,,,,R,..~~,,,,,,,,,=,,,,,,.T,,,,,,,.T",
        "T.....=......=.....=......,,,,,,,..~~,,,,,,,,,=,,,,,,...,,,,,.TT",
        "T..F..==================...,,,,,...~~,,,,,,,,,=,,,,T.....,,,...T",
        "T............=.....=...........R...~~,,,,,,,..=.....T..........T",
        "T.........====.....=..,,,,,.......T~~,,,,,....=...F.F.....#####T",
        "TTT.TTTTTT=TTT.TT..=T,,,,,,,....TT.~~........T=...T...T.T.#####T",
        "TT.TTTT.T.=TT,,,,,T=TT,,,,,,.......~~......F..=....TT.T.T.#####T",
        "TF.T.T..T.=T,,,,,,,=T,,,,,,,.......~~....T....=....T..T...#####T",
        "T..T..T.T.=.,,,,,,,=T.,T,,,.F.~~~~~~~....F....=,,,T,.T..#######T",
        "TTTT.TTT..=T,,,,,,,=T...TT....~~~~~~~.TF......=,,,,TTT.T#######T",
        "TTT.=======T.,,,,,T=TTT.......~~...F.........T=T,,,,,...#######T",
        "TTTT=T,,,,,.TTTTT..=TT.F.....T~~....FT........=,,TT,TF.T..#####T",
        "TTTT=,,,,,,,,...TTT=T....~~~~~~~.F...........T=,,,TTTF.T..#####T",
        "TTT.=,,,,,,,,TTT..T=...~~~~~~~~~........,,,T,.=TT.TTT.....#####T",
        "TTTT=,,,,,,,,....TT=T.~~~~~~~~~~~T..F..,,,,,,,=.TT.T...TTT#####T",
        "T.TT=.,,,,,T...TTTT=F~~~~~~~~~~~~~.....,,,,T,,=..FTT....T.#####T",
        "T.T.=TTTT.TTT.TT..T=T~~~~~~~~~~~~~..F..,,,,,,,=..TTT......#####T",
        "TTTT=T.TT..TT..T.TT=T~~~~~~~~~~~~~......,,T,,.=T.T...TT..T#####T",
        "T..T===========T...=TT~~~~~~~~~~~......F....FT==================",
        "TTTT.,,,..T..T=,,,,=TT.~~~~~~~~~......TT.....T=...T...T..T======",
        "TTTT,,,,,TFTT,=,,,,=TT...~~~~~.....,,T,,.....T=..T...T,,.T#####T",
        "TT..,,,,,TT.T,=,,,,=.T.....~~....,,T,,,,,,...T=T..TT,,,T,T#####T",
        "TT..,,,,,TTTT,=,,,,=TT.....~~.T..,T,,,,,,,....=TT..,,,,,T,#####T",
        "T.FTT,,,TT.TT.=,,,,======..~~....,,,,,,,,T....=....TTT,,T,#####T",
        "TTT.TTT...TTTT=TT..TT...=..~~T.....,,,,,.....T=.,,T,,,T,,######T",
        "TTTTTTT.=======TT.T....F=..~~........T........=,TTT,T,,,T######T",
        "T.TT.T..T...F.TTT.......===BB==================,,TT,,,,TT######T",
        "T..TTTTT.....TT..TT.T......~~......T...........,.,,,T,...######T",
        "TTTT.T..TTT....TTT.T.T....T~~........F..T.......,TT,TTT..######T",
        "TTTTTTTTTTTTTTTTTTTTTTTTTTT~~TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT",
    };
    m.buildings.push_back({5, 3, 5, 4, "soin", "Maison de soin", 0xb8483e});
    m.buildings.push_back({18, 3, 5, 4, "boutique1", "Boutique", 0x3f6fb0});
    m.buildings.push_back({4, 11, 5, 4, "chapelle", "Chapelle", 0x8a6fb8});
    m.buildings.push_back({17, 11, 5, 4, "maison", "Maison de l'ancien", 0x7a8a4a});
    m.npcs.push_back({11, 8, 4, 1, "", {"Bienvenue à Brumeval !", "Appuyez sur Échap pour ouvrir le menu : équipe, objets, magie et sauvegarde."}});
    m.npcs.push_back({17, 10, 8, 1, "", {"La maison au toit rouge, c'est celle de la guérisseuse. Elle soigne gratuitement.", "La boutique, juste à côté, vend potions, éthers et lanternes."}});
    m.npcs.push_back({21, 15, 5, 1, "ancien", {}});
    m.npcs.push_back({7, 15, 1, 1, "maelle", {}});
    m.npcs.push_back({25, 8, 6, 2, "", {"Les créatures affaiblies se capturent beaucoup plus facilement avec une lanterne !", "Moi, j'attends qu'il leur reste presque plus de PV."}});
    m.npcs.push_back({25, 38, 9, 3, "pecheur", {}});
    m.npcs.push_back({55, 31, 7, 1, "garde_col", {}});
    m.npcs.push_back({44, 8, 4, 2, "", {"Les sorts coûtent des PM.", "Un éther en rend 25, et la guérisseuse les restaure aussi."}});
    m.npcs.push_back({41, 24, 8, 1, "", {"Contre les créatures d'Ombre, la magie de Lumière fait des merveilles.", "La mage de la chapelle en connaît un rayon…"}});
    m.chests.push_back({7, 39, "herbe", 1});
    m.chests.push_back({17, 24, "potion", 2});
    m.chests.push_back({61, 2, "ether", 1});
    m.chests.push_back({55, 21, "plume", 1});
    m.chests.push_back({48, 41, "lanterne", 3});
    m.chests.push_back({3, 24, "", 80});
    m.chests.push_back({31, 41, "potion", 3});
    m.chests.push_back({2, 2, "lanterne", 2});
    m.signs.push_back({12, 10, "Village de Brumeval. Maison de soin au nord-ouest, boutique au nord-est."});
    m.signs.push_back({11, 19, "Bois Murmurant. On dit qu'une herbe lunaire pousse tout au fond des bois."});
    m.signs.push_back({55, 33, "Col de la Brume. Au-delà s'étendent les Monts Cendrelune."});
    m.signs.push_back({34, 10, "Pont du Ru-Clair."});
    m.warps.push_back({63, 32, 1, 1, 33, 3});
    m.warps.push_back({63, 33, 1, 1, 34, 3});
    m.zones.push_back({25, 1, 16, 18, 2, 4, 2, {"piafouine", "mulotin", "champichou"}});
    m.zones.push_back({41, 1, 22, 18, 4, 7, 3, {"lucioline", "grenouillon", "piafouine", "champichou"}});
    m.zones.push_back({1, 19, 22, 24, 4, 7, 3, {"champichou", "mulotin", "lucioline", "piafouine"}});
    m.zones.push_back({41, 19, 17, 24, 7, 10, 3, {"rocaillou", "grenouillon", "lucioline", "brumelin"}});
    m.zones.push_back({0, 0, 64, 44, 3, 6, 2, {"piafouine", "grenouillon", "mulotin"}});
    m.bosses.push_back({59, 32, "sylvarque", "boss1"});
    v.push_back(m);
  }
  {
    MapDef m;
    m.name = "Monts Cendrelune";
    m.theme = Theme::Cendres;
    m.rows = {
        "mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm",
        "maamaaaaaaaaaaaaaaaaaadaaaadaallaaaaaammmmmmmmmmmmmmmmmmmmmmmmmm",
        "maaaaaaaaaaaaaaaaaaagggggaamaallaaaaaammmmaaaaaaaaaaallammmmmmmm",
        "maaaaaaaaaaaaaadaaagggggggaaaallaaadaammmmaaaaaaaaaaaalammmmmmmm",
        "maaaaaaaaaadaaRaaaagggggggaaaallaaaaaammmmaaaaaaaaaaaaaammmmmmmm",
        "maadaadagggggaaaaaagggggggaaaallaaaaaammmmmmmm===mmmmmmmmmmmmmmm",
        "maaaaagggggggggRaaaagggggaaaaallaaaaaammmmmmmm=mmmmmmmmmmmmmmmmm",
        "maaaagggggggggggRaaaaaaaaaaaaallaaaaaammmmmmmm=mmmmmmmmmmmmmmmmm",
        "maaaagggggggggggaaaaaRaaaadaamllaaaadammmmmmmm=mmmmmmmmmmmmmmmmm",
        "mdaaagggggggggggaadaaaaaaaaaaallaaaaaaaaaaaaaa=aaaaaaaaaaadaaaam",
        "maaaaagggggggggaaaaaaaaaaa====bb===============aaaamamaaamaaRaam",
        "maaaaaaagggggaaaaaaaaaaaaa=aaallaagggggaaaaaaaagggggggaaaaaaaaam",
        "maaaaaaaRaaaaaaagggggaaaaa=aaallgggggggggaaaamggggggggggaaaaaRam",
        "maaaaaaaaaaaaaagggggggaaaa=aaallgggggggggaaaggggggggggmggaaaaaam",
        "maaRaaaaRaadaagggggggggaaa=aaallgggggggggaaagggggggggggggaaaaaam",
        "maaaaaaaaaaaaagggggggggaaR=aaallaagggggaaaaagggggggggggggaaaaaam",
        "maaagggggaaaaagggggggggaaa=aaallaaaRaaadaaaaaggggggggmggRaaaaaRm",
        "maagggggggaRaaamggggggaaaa=aaallaaaaaaaaaaaaaaagggggggaagggggaam",
        "maagggggggaaaaaagggggaaaaR=aaallaaaaaagggggaaaaaaaaaaRagggggggam",
        "maagggggggaaaaaaaadaaaaaaa=aaallaaaagggggggggaamaaaaaaagggggggam",
        "maaagggggaagggggaaaRaaaaaa=aaallaaaagggggggggaaaaaaaaaagggggggam",
        "maaaaaaaaagggggggaaamaaaaa=adallaaaagggggggggamaaaaRaaaagggggaam",
        "maaaaaaaaaagggggaaaaaaaaaa=aaallaaaaaagggggaaaaaaaaadaaaaaaaaaam",
        "mdaaaamaaaaaaaaaaaaaaaaaaa=aaallaaadamaaaRaaaaaaaaaaaaaaaaaaaaam",
        "maaaaaaaaRaaaaaaaaaaaaaaaa=aaallllllllllllllllllllllllllllaaaadm",
        "maaaaaaRaaaaaaaRaRaaaaaadm=aaalllllllllllllllllllllllllllladamam",
        "maaaaaaaaaaaaaaaaaaaaaaaaa=aaaamaaaaaaaaRaaaaaaaadaaaaaaagggaaam",
        "maaaaaaaaaaaaaaaaaaaaaaaaa=aaaaaaadaaaaammmmmmmmaaaaaaaagggggaam",
        "maaaaaaaaaaaaaaaaaaaaaaaaa=aaaaaaaaaaaaaammmmmmmaaaaaaagggggggam",
        "maaaaaaaaaaaaaaaaaaaaaaaaa=aaaaaaaaaamaaammmkmmmRaaaaaagggggggam",
        "maaaaaaaaaaaaaaaaaaaaaaaaa=aaaadaaadaaaaaaaa=aaaaaaaaaagggggggam",
        "maaaaaa=aaaaaa=aaaaaa=aaaa=aaaaaaaaaaaaaaaaa=aaaaaaaaaaagggggaam",
        "maaaaaa=aaaaaa=aaaaaa=aaaa=aaaaaaaaadmaadaaa=aaaaaaaaRaaagggaaam",
        "=============================================aaaagggggggaaaaaaam",
        "===aaaaaaaaaaaaa=aaaaaaaaaaadaaaaaaRaaaaaaaaRaaggggmggggggaaaaam",
        "maaaaaaaaaaaaaaa=aaaaaaaaaaaRaaaaaaaaaaaaaaaaagggggggggggggdaaam",
        "maaaaaaaaaaaaaaa=aaaaaaaaaaaaaaRaagggggaaaaaaagggggggggggggaaaam",
        "maaaaaaaaaaaaaaa=aaaaaaaaaaaaaaagggggggggaaaaagggggggggmgggaaaam",
        "maaaaaaaaaaaaaaa=aaaaaaaaaaaaaaagggggggggaaaaaagggggggggggadaaam",
        "maaaaaaaa=aaaaaa=aaa=aaaaaaaaaaagggggggggaadaaaaagmgggggaRaaaRam",
        "maaaaaaaa============aaaaaaaaaaaaagggggamaaaagggggaRaaaaaaaaaaam",
        "maaaaaaaaaaaaaaaaaaaaaaaaaaaaaRaaaaaaaaaaaaagggggggaaaaaaaaaaaam",
        "maaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaadaaaaagggggaaaaaaaaaaaamm",
        "mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm",
    };
    m.buildings.push_back({5, 27, 5, 4, "soin", "Maison de soin", 0xb8483e});
    m.buildings.push_back({12, 27, 5, 4, "boutique2", "Boutique", 0x3f6fb0});
    m.buildings.push_back({19, 27, 5, 4, "auberge", "Auberge du Tison", 0x9a5a2a});
    m.buildings.push_back({6, 35, 6, 4, "forge", "Forge de Brann", 0x4a4a52});
    m.buildings.push_back({18, 35, 5, 4, "maison", "Maison", 0x7a6a4a});
    m.npcs.push_back({4, 35, 7, 1, "", {"Une jeune mage est partie explorer la Grotte des Échos, à l'est.", "Elle n'est jamais revenue… On entend des grondements là-dedans."}});
    m.npcs.push_back({11, 32, 9, 1, "", {"Ignarok, le cœur du volcan, est une créature de Feu.", "L'Eau est sa grande faiblesse. Les sorts Givre et Blizzard devraient le faire vaciller."}});
    m.npcs.push_back({24, 35, 6, 2, "", {"Brann, c'est le plus fort de Forgeroc !", "Il ne suit que ceux qui le battent en duel."}});
    m.npcs.push_back({11, 40, 2, 2, "brann", {}});
    m.npcs.push_back({47, 10, 7, 1, "garde_volcan", {}});
    m.npcs.push_back({33, 35, 8, 1, "", {"Ici, les créatures sont bien plus fortes que dans la vallée.", "Pensez à faire tourner votre équipe pour que tout le monde progresse."}});
    m.npcs.push_back({26, 30, 4, 2, "", {"Les lanternes d'argent de la boutique attrapent presque tout.", "Pratique pour les créatures têtues des montagnes."}});
    m.chests.push_back({60, 41, "superpotion", 2});
    m.chests.push_back({12, 4, "ether", 2});
    m.chests.push_back({60, 11, "plume", 2});
    m.chests.push_back({34, 41, "", 300});
    m.chests.push_back({3, 22, "lanterne_argent", 3});
    m.chests.push_back({55, 30, "ether", 1});
    m.chests.push_back({24, 2, "superpotion", 1});
    m.signs.push_back({3, 32, "Forgeroc, la ville des forges."});
    m.signs.push_back({27, 11, "Vers le cratère. Ignarok y dort, dit-on."});
    m.signs.push_back({45, 31, "Grotte des Échos."});
    m.warps.push_back({0, 33, 0, 62, 32, 2});
    m.warps.push_back({0, 34, 0, 62, 33, 2});
    m.warps.push_back({44, 29, 2, 15, 21, 0});
    m.zones.push_back({1, 1, 29, 23, 11, 14, 3, {"tisonnel", "galetor", "voltigeon"}});
    m.zones.push_back({32, 9, 31, 15, 14, 17, 3, {"voltigeon", "glaconnet", "fumerole", "tisonnel"}});
    m.zones.push_back({28, 26, 35, 17, 12, 15, 3, {"galetor", "tisonnel", "voltigeon", "glaconnet"}});
    m.zones.push_back({0, 0, 64, 44, 12, 15, 3, {"tisonnel", "galetor"}});
    m.bosses.push_back({48, 2, "ignarok", "boss2"});
    v.push_back(m);
  }
  {
    MapDef m;
    m.name = "Grotte des Échos";
    m.theme = Theme::Grotte;
    m.rows = {
        "################################",
        "################################",
        "########cc#################cccc#",
        "########cc#####cccccccccccccccc#",
        "########cc#####cccccccccccccccc#",
        "########ccccccccc##########cccc#",
        "########ccccccccc##########cccx#",
        "###############cc###############",
        "###############cc###############",
        "###############cc###############",
        "####ccccccccccccc###############",
        "####ccccccccccccc###############",
        "####cc#########cc###############",
        "####cc#########cccccccccc#######",
        "####cc#########cccccccccc#######",
        "####cc#########cc######cc#######",
        "####cc#########cc######cc#######",
        "####cc#########cc######cc#######",
        "##xccccc######cccc#####cc#######",
        "##cccccc######cccc###cccccc#####",
        "##cccccc######cccc###cccccc#####",
        "##cccccx######xccc###xccccx#####",
        "###############k################",
        "################################",
    };
    m.npcs.push_back({29, 4, 3, 2, "isra", {}});
    m.chests.push_back({3, 20, "superpotion", 1});
    m.chests.push_back({25, 20, "ether", 2});
    m.chests.push_back({28, 6, "plume", 1});
    m.chests.push_back({8, 2, "", 150});
    m.signs.push_back({13, 19, "Grotte des Échos. Le sol tremble sous vos pieds."});
    m.warps.push_back({15, 22, 1, 44, 30, 1});
    m.zones.push_back({0, 0, 32, 24, 13, 16, 3, {"galetor", "fumerole", "glaconnet", "tisonnel"}});
    m.bosses.push_back({24, 3, "golem", "golem"});
    v.push_back(m);
  }
  return v;
}

const std::vector<MapDef>& maps() {
  static const std::vector<MapDef> v = build();
  return v;
}
