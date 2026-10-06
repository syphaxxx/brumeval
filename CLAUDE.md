# Brumeval — contexte pour Claude Code

Petit RPG 2D en C++17 avec SDL2, inspiré de Final Fantasy 7 (combat ATB à trois
combattants, Limites, magie) et de Pokémon (capture de créatures). Tout le contenu
est original. Le projet a été démarré dans l'app Claude puis transféré ici.

## L'utilisateur

- Il est francophone et débute en programmation : réponds en français, explique
  simplement ce que tu changes et pourquoi, sans jargon inutile.
- Il travaille sous **Windows avec Visual Studio Code** (extensions CMake Tools et
  C/C++). Le code a été écrit sous Linux avec GCC, puis compilé et testé sous
  Windows avec MSVC (Build Tools 2022) : tous les tests passent.
- Tous les textes visibles par le joueur sont en français, avec accents.

## Compiler et lancer

```bash
cmake -S . -B build              # la première fois, télécharge et compile SDL2 (Internet requis)
cmake --build build
./build/brumeval                 # Windows : build\Debug\brumeval.exe (selon le générateur)
```

Dans VS Code : CMake Tools configure le projet à l'ouverture ; F7 compile,
Maj+F5 lance sans débogueur. `.vscode/launch.json` contient une configuration
de débogage MSVC (`cppvsdbg`) et une GDB.

## Tester (à faire après chaque modification)

```bash
./build/brumeval --test captures
```

Le mode test (src/test.cpp) vérifie les données, joue des scènes (titre, village,
dialogue, recrutement, combat piloté par les menus, sauvegarde/chargement),
enregistre des captures `.bmp` dans `captures/` et simule des combats pour
l'équilibrage. Il doit finir par « TOUS LES TESTS PASSENT ». Regarde les captures
après un changement visuel. Il tourne sans fenêtre (rendu logiciel).

## Architecture (src/)

| Fichier | Rôle |
|---|---|
| `main.cpp` | Fenêtre SDL, boucle principale, plein écran (F11), option `--test`, chargement de data/ |
| `game.hpp/.cpp` | Écran titre, exploration, menus (pause, équipe, objets, magie, boutique), sauvegarde, dessin de la carte |
| `battle.hpp/.cpp` | Combat ATB : jauges, menus, dégâts physiques/magiques, précision, critiques, états, bonus/malus, renforts ennemis, IA (`think`), journal (`log`), capture, Limites, victoire |
| `store.hpp/.cpp` | Dossier data/ : recherche, lecture et écriture JSON (nlohmann/json, `Json` = `ordered_json`) |
| `data.hpp/.cpp` | Chargement des types, techniques, espèces, objets, apparences et règles (`Rules`, `ruleFields()`), formules de stats et d'expérience |
| `world.hpp/.cpp` | Structures des cartes (thèmes, passages avec condition, zones déclencheuses), lecture/écriture de data/cartes/, vérification d'accessibilité (`checkMaps`) |
| `events.hpp/.cpp` | Événements de l'histoire : chargement, vérification et exécution des actions (`Game::runEvent`) |
| `sprites.hpp/.cpp` | Dessin en code des créatures (19 formes), humains (coiffes, armes), tuiles selon le thème, bâtiments (aucune image externe) |
| `gfx.hpp/.cpp` | Primitives de dessin (ellipses, polygones, dégradés), police pixel intégrée avec accents, fenêtres bleues |
| `ui.hpp/.cpp` | Clavier, `Script` (file de messages/actions) et `MenuStack` (menus à curseur ; `MenuItem::adjust` pour régler une valeur avec gauche/droite, `rightFn` pour un texte recalculé, `menuHeader` pour un titre de section) |
| `settings.hpp/.cpp` | Réglages (écran titre > Outils) : éditeurs des règles, espèces, techniques, types (grille) et objets ; chaque modification passe par `Settings::change` (document JSON puis `rebuildData`) |
| `mapedit.hpp/.cpp` | Éditeur de cartes (écran titre > Outils) : calques tuiles/objets/zones, outils, menus de chaque objet, annuler/rétablir, test en jeu (`testHere`, `editorTest_`) |
| `arena.hpp/.cpp` | Arène de combat (écran titre > Outils) : composition, combat à la main, simulation progressive (`stepSim`), modèles tirés des événements et des zones, journal |
| `test.cpp` | Mode test automatique |

### Principes à connaître

- Résolution logique 320x240, agrandie à l'écran en nombre entier de fois.
- `Script` : `say(texte, auto)` affiche un message (auto = 0 attend Entrée),
  `call(fn)` exécute du code, `wait(s)` fait une pause. Les étapes ajoutées
  *pendant* un `call` sont insérées juste après lui : c'est ce qui enchaîne les
  scènes et les tours de combat.
- `MenuStack` : pile de menus ; chaque `MenuItem` a un libellé, une valeur à
  droite, un texte d'aide, une action et un rappel `hover` (ex. curseur de cible).
- Équipe : `Game::team` (8 membres maximum par capture, les humains s'ajoutent
  toujours). Les 3 premiers membres valides combattent (`Game::front()`).
- Progression : `Game::flags` (`boss1`, `golem`, `boss2`, `maelle`, `brann`,
  `isra`, `pecheur`, `coffre:<carte>:<index>`).
- Sauvegarde : fichier texte `sauvegarde.txt` dans `SDL_GetPrefPath("Brumeval",
  "Brumeval")` (sous Windows : `%APPDATA%\Brumeval\Brumeval\`).
- Police : `gfx.cpp`, fonction `buildFont()`. Un caractère absent s'affiche « ? » ;
  ajoute son dessin si tu utilises un nouveau symbole.
- Données : tout le contenu est dans `data/` (voir `data/LISEZMOI.md`), chargé au
  démarrage par `loadData()`, `loadMaps()`, `loadEvents()`. Aucune donnée de jeu
  ne doit être écrite en dur dans le code : ajouter un champ au JSON et au
  chargeur. Les cartes sont désignées par leur identifiant (`MapDef::id`), pas
  par leur numéro (ordre alphabétique des fichiers).
- Événements : habitants (`evenement`), portes des bâtiments (genre ou
  `evenement`) et boss lancent un événement de `data/evenements.json`. Les
  actions sont exécutées par `Game::execAction` (events.cpp) via le `Script` ;
  `Script::runNow` fait passer la suite (réponse à une question, fin de combat)
  avant les étapes déjà en attente.
- Combat : dégâts physiques = Attaque contre Défense, magiques = Magie contre
  Résistance ; `moveEff()` combine les deux types de la cible et ses
  `resistances` propres. Un seul état à la fois (`Fighter::status`), bonus/malus
  de -3 à +3 (`Fighter::stage`, effet = `etage` des règles), tout est effacé à la
  fin du combat (`clearBattle`). `Battle::think` choisit les actions de
  l'ordinateur (ennemis, et alliés en mode test) ; `Battle::log` garde un journal
  lisible (`BRUMEVAL_JOURNAL=Sylvarque brumeval --test captures` l'affiche pour
  la simulation qui contient ce mot).
- Données en mémoire : `dataDoc(DF_…)` garde chaque fichier de data/ sous forme
  de JSON ; `rebuildData()` reconstruit les structures du jeu à partir de ces
  documents, `saveDataDoc` les écrit. Les Réglages ne touchent qu'aux documents,
  jamais directement aux structures. Le mode test vérifie que réécrire un
  document sans changement redonne exactement le fichier.
- Souris : `Input` reçoit position, boutons (`mclick`, `mdown`) et molette
  (coordonnées déjà ramenées à 320x240 par SDL). `MenuStack::update` gère le
  survol et les clics. Raccourcis des éditeurs dans `Input` : `tab`, `undo`,
  `redo`, `saveKey` (Ctrl+Z/Y/S selon la disposition du clavier), `prev`/`next`
  (Page préc./suiv.), `del` (Suppr).
- Saisie de texte : `Game::editText(titre, texte, max, rappel)` (SDL_TEXTINPUT
  transmis par main.cpp à `Game::onText`). Pendant la saisie, `onKey` ne sert
  qu'à écrire (Retour arrière, Entrée, Échap).
- Arène : mode `Mode::Arena` ; un combat lancé depuis l'Arène (`arenaBattle_`)
  y revient sans défaite « réelle ». La simulation crée des `Battle` en mode
  automatique et avance de quelques millisecondes par image. `BattleSetup::theme`
  impose un décor.
- Adversaires : `BattleSetup` (ennemis, `reserve` de renforts, `foeName`).
  Habitants avec `vue` : `Game::checkSight` les déclenche quand le joueur passe
  devant eux.
- Piège C++ : ne jamais écrire `for (auto& x : j.value(...).items())` (objet
  temporaire détruit avant la boucle) ; ranger d'abord le JSON dans une variable.
- Sous MSVC, l'option `/utf-8` (déjà dans CMakeLists.txt) est indispensable pour
  les accents. `/wd4244` coupe les centaines d'avertissements « int en float »
  des appels de dessin ; garder 0 avertissement de compilation.

### Modifier les données

Modifier les fichiers de `data/` puis relancer le jeu (pas besoin de recompiler).
Le mode test vérifie les références et que chaque porte, habitant, coffre,
panneau, passage et boss est accessible à pied (`checkMaps`). Les fichiers sont
réécrits par `writeJson` (petites listes sur une ligne) : garder ce format.

## Contenu actuel

- **Vallée de Brumeval** (64x44) : village (soin, boutique, chapelle, ancien),
  Bois Murmurant (herbe lunaire pour recruter Maëlle), lac et pêcheur, prairies,
  bosquet, col gardé par **Sylvarque** (N.12, Ombre) et deux Brumelins.
- **Monts Cendrelune** (64x44) : ville de Forgeroc (soin, boutique, auberge,
  forge), duel contre **Brann** pour le recruter, champs de cendres, rivières de
  lave, cratère d'**Ignarok** (N.23, Feu, boss final).
- **Grotte des Échos** (32x24) : **Golem de suie** (N.16), puis **Isra** rejoint
  l'équipe.
- **Forêt de Sylve-Noire** (56x40, ouverte après Sylvarque, entrée à l'ouest du
  village) : bandits dresseurs, chef des bandits qui retient **Kael** (archer,
  Vent), ermite qui soigne, marais, boss facultatif **Ronce-Mère** (N.18,
  Plante/Poison).
- **Pics Givrés** (60x44, ouverts après Ignarok, col au nord de Cendrelune) :
  village de Givreval (soin, boutique, auberge), duel contre **Sélène**
  (chevalière, Métal), chevaliers du givre, lac gelé.
- **Temple gelé** (labyrinthe, clé de givre au fond) et **Sanctuaire** : boss
  final **Givrecorne** (N.31, Glace/Roche).
- 13 types (dont Glace, Roche, Vent, Poison, Métal, Esprit). Héros : Lior,
  Maëlle, Brann, Isra, Kael, Sélène. Starters : Braisenard, Gouttelin, Ronceau.
  23 créatures sauvages capturables, 5 boss, 5 sortes d'ennemis humains.
- Les cartes se modifient avec l'éditeur de cartes (Outils) ou directement
  dans les JSON.

Équilibrage mesuré par le mode test (IA automatique, sans objets) : combats
normaux, bandits, chevaliers et duels gagnés à ~95-100 %, Sylvarque ≈ 80 %
(N.11), Ronce-Mère ≈ 55-70 % (N.16), Ignarok ≈ 30-45 % (N.22), Givrecorne
≈ 35 % (N.30, avec Sélène). Un boss trop facile vient souvent de sa lenteur ou
d'acolytes trop faibles, pas de ses PV : regarder le journal (BRUMEVAL_JOURNAL).
Le Vent fait ×4 à Plante/Poison et le Métal ×4 à Glace/Roche : la Ronce-Mère et
le Givrecorne ont une résistance propre pour ramener cela à ×2. La Lumière fait ×2 à l'Ombre mais l'Ombre est neutre sur la Lumière :
sinon Maëlle, ciblée en priorité par l'IA, tombe dès le début contre Sylvarque.

## À faire / pistes

1. Windows : la compilation MSVC (générateurs Visual Studio et Ninja) et le mode
   test marchent en ligne de commande. Reste à confirmer dans VS Code même :
   F7, Maj+F5 et le débogueur (`cppvsdbg`).
2. Ajouter musique et effets sonores (SDL2_mixer via FetchContent, ou l'audio de SDL).
3. Rendre le sprite d'Ignarok plus lisible (aujourd'hui un bloc rouge).
4. Idées : intérieurs des maisons, quêtes annexes, équipement, menu d'options,
   manette (SDL_GameController), animations d'attaque.
