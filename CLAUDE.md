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

## Publier une version pour les joueurs

`.github/workflows/compilation.yml` compile le jeu sous Windows (MSVC), Mac
(application universelle `Brumeval.app`, macOS 11+) et Linux (Ubuntu 22.04) à
chaque envoi sur GitHub, lance le mode test sur les trois, et fabrique les
paquets (exécutable + `data/` + `distribution/LISEZ-MOI.txt`). Pour publier :
augmenter `BRUMEVAL_VERSION` dans `src/version.hpp`, fusionner dans `main`,
puis `git tag v1.2` et `git push origin v1.2` : la page Releases est créée avec
le texte de `distribution/notes.md`. Sous Windows, la bibliothèque C++ est
intégrée (`CMAKE_MSVC_RUNTIME_LIBRARY`, `SDL_FORCE_STATIC_VCRT`) ; sous Linux,
`-static-libstdc++`. Sur Mac, `data/` va dans `Contents/Resources` (trouvé par
`SDL_GetBasePath`). Pour essayer une version Release ici : `cmake -S . -B
build-release` puis `cmake --build build-release --config Release`.

## Tester (à faire après chaque modification)

```bash
./build/brumeval --test captures
```

Le mode test (src/test.cpp) vérifie les données, joue des scènes (titre, village,
dialogue, recrutement, combat piloté par les menus, sauvegarde/chargement),
enregistre des captures `.bmp` dans `captures/` et simule des combats pour
l'équilibrage. Il doit finir par « TOUS LES TESTS PASSENT ». Regarde les captures
après un changement visuel. Il tourne sans fenêtre (rendu logiciel).

Chaque image dessinée pendant les tests est vérifiée (`Gfx::checkLayout`,
gfx.cpp) : texte qui sort de sa fenêtre, textes qui se chevauchent, fenêtre
posée de travers sur une autre ou hors de l'écran, libellé de menu coupé. Les
problèmes sont listés avec la dernière capture (« après 05_equipe : … »). Le
« tour des menus » de test.cpp ouvre en plus la plupart des menus avec les noms
les plus longs : y ajouter tout nouvel écran.

## Architecture (src/)

| Fichier | Rôle |
|---|---|
| `main.cpp` | Fenêtre SDL (taille adaptée à l'écran), affichage agrandi (`Screen`), boucle principale, plein écran (F11, gardé dans `options.txt`), option `--test`, chargement de data/ |
| `game.hpp/.cpp` | Écran titre, exploration, menus (pause, équipe, objets, magie, boutique), sauvegarde, dessin de la carte |
| `battle.hpp/.cpp` | Combat ATB : jauges, menus, dégâts physiques/magiques, précision, critiques, états, bonus/malus, renforts ennemis, IA (`think`), tactiques des alliés (`tacticPlan`, touche Tab), journal (`log`), capture, Limites, victoire |
| `tactics.hpp/.cpp` | Tactiques (gambits) : catalogue des conditions et actions automatiques, lecture/écriture JSON, vérifications (`tacticProblem`), lignes selon le niveau (`tacticSlots`), éditeur réutilisable (`openTacticsEditor`) |
| `store.hpp/.cpp` | Dossier data/ : recherche, lecture et écriture JSON (nlohmann/json, `Json` = `ordered_json`) |
| `data.hpp/.cpp` | Chargement des types, techniques, espèces, objets, apparences et règles (`Rules`, `ruleFields()`), formules de stats et d'expérience |
| `world.hpp/.cpp` | Structures des cartes (thèmes, passages avec condition, zones déclencheuses), lecture/écriture de data/cartes/, vérification d'accessibilité (`checkMaps`) |
| `events.hpp/.cpp` | Événements de l'histoire : chargement, vérification et exécution des actions (`Game::runEvent`) |
| `sprites.hpp/.cpp` | Dessin en code des créatures (19 formes), humains (coiffes, armes), tuiles selon le thème, bâtiments (aucune image externe) |
| `audio.hpp/.cpp` | Son fabriqué par le programme (synthèse) : musiques et bruitages décrits dans `data/sons.json`, mélangés dans le fil audio de SDL (`Mixer`) |
| `gfx.hpp/.cpp` | Primitives de dessin (ellipses, polygones, dégradés), police pixel intégrée avec accents, fenêtres bleues |
| `ui.hpp/.cpp` | Clavier, `Script` (file de messages/actions) et `MenuStack` (menus à curseur ; `MenuItem::adjust` pour régler une valeur avec gauche/droite, `rightFn` pour un texte recalculé, `menuHeader` pour un titre de section) |
| `settings.hpp/.cpp` | Réglages (écran titre > Outils) : éditeurs des règles, espèces, techniques, types (grille) et objets ; chaque modification passe par `Settings::change` (document JSON puis `rebuildData`) |
| `mapedit.hpp/.cpp` | Éditeur de cartes (écran titre > Outils) : calques tuiles/objets/zones, outils, menus de chaque objet, annuler/rétablir, test en jeu (`testHere`, `editorTest_`) |
| `storyedit.hpp/.cpp` | Éditeur d'histoire (écran titre > Outils) : événements, pages, conditions, actions imbriquées ; chaque écran est un chemin JSON (`goTo`, `goUp`) ; « Jouer l'événement » (`play`, `storyTest_`) |
| `arena.hpp/.cpp` | Arène de combat (écran titre > Outils) : composition, combat à la main, simulation progressive (`stepSim`), modèles tirés des événements et des zones, journal |
| `procgen.hpp/.cpp` | Génération procédurale du mode Expédition : hasard reproductible (`Rng`, graine), techniques par type (`Pool`), soutiens, créatures, gardiens, personnages par classe, apparences, régions (carte, village, chemin, rivière, dresseurs, coffres, gardien, événements), au format de data/ |
| `expedition.hpp/.cpp` | Mode Expédition (écran titre) : menus, choix du héros et de la créature, contenu généré installé à la place des données du jeu (`install`/`restore`), passage de région, défaite roguelite (éclats, record), Camp (améliorations), sauvegarde à part |
| `net.hpp/.cpp` | Réseau du multijoueur : connexions TCP non bloquantes (Windows, Mac, Linux), messages JSON précédés de leur longueur, adresses de l'ordinateur |
| `online.hpp/.cpp` | Multijoueur (écran titre) : héberger une partie (jusqu'à 4 joueurs), rejoindre, salon, vérification de la version et des données (`dataHash`), relais des messages par l'hôte, duel en ligne |
| `coop.cpp` | Expédition à plusieurs (fonctions de `Online`) : lancement d'une graine pour tous, état de chaque joueur (message `ou`), autres joueurs sur la carte (`avatars`), attente et combat du gardien à plusieurs |
| `trade.cpp` | Échange de créatures (fonctions de `Online`) : dans le salon (partie principale) ou pendant l'expédition à plusieurs, une contre une ou en cadeau |
| `options.hpp/.cpp` | Options du joueur (`options()`, fichier `options.txt`) : volumes, plein écran, touches choisies (`K_UP`…) |
| `events.hpp/.cpp` (quêtes) | Quêtes annexes : `quests()`, `findQuest`, `questFlag` ; action `quete` |
| `version.hpp` | Numéro de version (`BRUMEVAL_VERSION`), affiché sur l'écran titre et comparé en multijoueur |
| `test.cpp` | Mode test automatique |

### Principes à connaître

- Résolution logique 320x240, élargie à la forme de l'écran : l'image fait
  240 pixels de haut et `Gfx::fullW` de large (320 à 576 ; 384 en 16:10,
  428 en 16:9). Tout le code dessine dans la zone de 320x240 du milieu
  (`Gfx` ajoute le décalage `ox` dans `span`, `rect` et `textBig`) : menus,
  fenêtres et combattants ne bougent pas. Un décor qui doit remplir tout
  l'écran va de `g.left()` (négatif) à `g.right()` ; la carte, les combats,
  l'écran titre, la fin et les outils le font. Tout nouveau décor plein écran
  doit utiliser `g.left()`/`g.fullW` au lieu de 0/`SCREEN_W`.
- `Screen` (main.cpp) choisit cette largeur selon la fenêtre, puis agrandit
  l'image pour la remplir sans bandes noires (agrandissement entier puis
  ajustement linéaire : pixels nets à toute taille). La souris est ramenée en
  coordonnées de la zone du milieu par `Screen::toGame` (négatives à gauche).
  Le jeu tient compte du zoom de Windows (`SDL_HINT_WINDOWS_DPI_AWARENESS`) ; la
  fenêtre de départ a la forme de l'écran (`fitWindow`). Lancé par un
  double-clic, il ferme la console noire (`FreeConsole`).
- `BRUMEVAL_LARGEUR=384 brumeval --test dossier` prend les captures du mode
  test au format large (à vérifier après un changement de décor).
- Lancé depuis l'application Claude (application empaquetée), le jeu lit et
  écrit `%APPDATA%` dans un dossier privé de Claude
  (`%LOCALAPPDATA%\Packages\Claude_…\LocalCache\Roaming\`) : sauvegarde et
  options y sont séparées de celles du jeu lancé par un double-clic ou VS Code.
  Pour lancer le jeu pour l'utilisateur : `Start-Process explorer.exe
  -ArgumentList <chemin de brumeval.exe>`.
- `Script` : `say(texte, auto)` affiche un message (auto = 0 attend Entrée),
  `call(fn)` exécute du code, `wait(s)` fait une pause. Les étapes ajoutées
  *pendant* un `call` sont insérées juste après lui : c'est ce qui enchaîne les
  scènes et les tours de combat.
- `MenuStack` : pile de menus ; chaque `MenuItem` a un libellé, une valeur à
  droite, un texte d'aide, une action et un rappel `hover` (ex. curseur de cible).
  Un menu du dessous n'est pas dessiné si un sous-menu le chevauche sans être
  bien à l'intérieur (4 px de marge). Un libellé trop long est coupé avec « … »
  (signalé par le mode test, sauf `MenuItem::shrink`). Les panneaux à droite
  (Réglages, Arène) se cachent quand `menus.maxRight()` dépasse 158. Une
  fenêtre modale sur fond assombri appelle `g.newLayer()`. Largeur d'un
  caractère : 6 px ; un libellé commence 13 px après le bord du menu.
- Équipement : `Fighter::gear` (arme, armure, accessoire : identifiants d'objets
  avec `ItemDef::slot` et `bonus`), ajouté aux statistiques par
  `Fighter::recalc` ; `canEquip` (armes et armures : humains seulement). Menu
  `Game::gearMenu` > `gearSlots` > `gearPick` ; ligne `equipement` après
  `membre` dans la sauvegarde ; un échange emporte l'équipement de la créature.
- Équipe : `Game::team` (8 membres maximum par capture, les humains s'ajoutent
  toujours). Les 3 premiers membres valides combattent (`Game::front()`).
- Progression : `Game::flags` (`boss1`, `golem`, `boss2`, `maelle`, `brann`,
  `isra`, `pecheur`, `coffre:<carte>:<index>`).
- Sauvegarde : fichier texte `sauvegarde.txt` dans `SDL_GetPrefPath("Brumeval",
  "Brumeval")` (sous Windows : `%APPDATA%\Brumeval\Brumeval\`). Chaque ligne
  `membre` est suivie de `tactiques` (interrupteur) et des lignes `tactique` du
  membre ; `auto` garde le mode auto. Le mode test écrit dans
  `sauvegarde_test.txt` (`saveName_`) pour ne jamais toucher à la vraie partie.
- Expédition (expedition.hpp, procgen.hpp) : tout le contenu est généré depuis
  une graine (`procgen::generateBase`, puis `generateRegion(graine, k, contenu)`
  région par région : la région k ne dépend que de la graine et de k). Le
  contenu prend la place des données en mémoire (`dataDoc` + `rebuildData`,
  `maps()` = la seule région courante, `events()`), avec les types et objets du
  jeu ; `Game::titleMenu` appelle `Expedition::leave` qui remet tout en place.
  Identifiants générés : `x_<type>_p1`… (techniques), `xh1`… (héros), `xs1`…
  (créatures de départ), `x<k>c1`, `x<k>b` (gardien), `x<k>t1` (dresseurs),
  `x<k>r` (recrue), `lk_…` (apparences), `expedition_<k>` (carte). La sortie
  (derrière le gardien) est testée dans `Game::arrive` ; `Game::defeat` finit
  l'expédition. Sauvegarde : `expedition.txt` (ligne `expedition <graine>
  <région>` en premier : le chargement régénère le monde), progression du Camp
  dans `expedition_progres.txt` ; le mode test utilise des fichiers `_test`.
  Changer le générateur change le monde des graines déjà jouées (et donc les
  sauvegardes d'expédition en cours).
  À plusieurs, voir « Expédition à plusieurs » plus bas.
  `BRUMEVAL_MONDE=brume brumeval --test captures` affiche le monde de la graine
  « brume » (héros, créatures, gardiens et techniques des trois premières régions).
- Multijoueur (online.hpp, net.hpp) : TCP sur le port 47474, l'hôte (numéro 0)
  et jusqu'à trois invités (1 à 3), reliés seulement à l'hôte : `sendTo(à qui,
  message)` ajoute `de` (expéditeur) et, chez un invité, `a` (destinataire, -1 :
  tous) ; l'hôte fait suivre. Chacun envoie `bonjour` (version, empreinte des
  données, pseudo, équipe : 3 premiers membres de la partie principale) ;
  l'hôte répond `bienvenue` (numéro, liste des joueurs, expédition en cours) et
  envoie `joueurs` aux autres. Le duel n'est possible qu'à deux. En duel, l'**hôte**
  calcule tout le combat (`Battle::goOnline`, `Net::Host` : les « ennemis » sont
  l'équipe de l'invité ; quand la jauge de l'un d'eux est pleine, il envoie
  `tour` et attend `ordre`, qu'il vérifie) et envoie son état dix fois par
  seconde (`netState` : PV, PM, jauges, états, message en cours), plus les
  nombres flottants, éclairs et clignotements. L'**invité** (`Net::Guest`) ne
  fait pas avancer le temps : il affiche cet état et répond à `tour` par son
  menu ou ses tactiques ; `useMove` envoie alors l'ordre au lieu de le jouer.
  Un combattant dans un message : `[0, rang]` pour l'équipe de l'expéditeur,
  `[1, rang]` pour l'autre (`Battle::mine`, `Battle::other`). Pas d'objets, ni
  remplaçants, ni expérience en duel. Le mode test lance deux `Game` reliés
  par 127.0.0.1 (port 47475), joue un duel complet puis une expédition à deux.
  Augmenter `BRUMEVAL_VERSION` à chaque version publiée (et si les messages
  changent).
- Expédition à plusieurs (coop.cpp) : l'hôte envoie `expedition` (graine,
  empreinte du monde `Expedition::worldHash`, région du groupe) ; chacun joue sa
  propre `Expedition` (`group()`, sauvegarde `expedition_groupe_<pseudo>.txt`,
  `groupTag` : deux fenêtres sur le même ordinateur ne se gênent pas) : il
  reprend sa sauvegarde de cette graine, sinon part avec un nouveau héros de la
  région du groupe. Chacun envoie son état quand il change (`ou` : région, case,
  héros, gardien vaincu, en combat, `p` : prêt devant le gardien avec ses
  combattants). `Game::bossTouched` : le gardien attend tous ceux qui ne l'ont
  pas encore battu (`Online::missing`) ; l'hôte vérifie (`checkGuardians`) puis
  envoie `gardien` (groupe, combattants, chef = plus petit numéro). Chez chacun,
  le combat a les mêmes alliés dans le même ordre (`BattleSetup::allies` : ses
  vrais combattants, des copies pour ceux des autres ; 3 seul, 2 chacun à deux,
  1 chacun à trois ou quatre ; PV des ennemis ×alliés/3 au-delà de trois). Le
  chef (`Net::Lead`) calcule le combat comme l'hôte d'un duel, envoie `tour`
  au joueur de l'allié (`Battle::owner`), vérifie son `ordre` (technique, ou
  objet déjà pris dans son sac), envoie `renfort` et `issue` ; ceux qui suivent
  (`Net::Follow`) jouent alors la fin chez eux (`victory` : expérience de leurs
  combattants, or). Joueur parti : l'ordinateur joue ses combattants
  (`dropPlayer`) ; chef parti : le combat s'arrête sans conséquence. Une
  défaite en solo ramène au village (`Expedition::knockedOut`, moitié de l'or) ;
  `groupBattle_` : défaite contre le gardien, l'expédition s'arrête
  (`onDefeat`, puis `Online::runEnded` : retour au salon). Dans le mode test,
  les deux `Game` partagent les données en mémoire : l'invité met de côté les
  vraies données (celles de l'hôte), voir `shareBackup`.
  Le monde généré doit être identique partout : `-ffp-contract=off` (GCC,
  Clang), aucun `sin`/`pow`/`std::shuffle`/`unordered_map` dans procgen.cpp,
  et jamais deux tirages au hasard dans les arguments d'un même appel (Clang
  les fait de gauche à droite, MSVC et GCC de droite à gauche) ;
  le mode test compare l'empreinte de la graine 2026 à une valeur fixe (à
  mettre à jour si on change le générateur).
- Échange de créatures (trade.cpp, `Online::trade_`) : message `echange`, champ
  `e` : `offre` (créature, `cadeau`, `run` : pendant l'expédition), puis
  `contre` (créature en retour) ou `accepte` (cadeau), `fait`, `refus`
  (`raison`). Celui qui a proposé fait l'échange chez lui en premier
  (`commitTrade`), puis envoie `fait` ; l'autre le fait alors. Une créature dans
  un message : `creatureJson` / `creatureFrom` (vérifiée : espèce connue ici,
  pas un humain). Au salon, l'équipe est lue et réécrite dans la sauvegarde de
  la partie principale (`loadMine` = `Game::loadGame`, puis `saveGame`) ;
  pendant l'expédition, c'est `G.team`, sauvegardée aussitôt. Le menu de
  l'échange (titre « Échange : … ») s'ouvre dans `updateTrade` dès que le joueur
  est libre (ni combat, ni dialogue) ; l'échange s'arrête si l'autre part.
- Tactiques (tactics.hpp) : `Fighter::tactics` (liste de `Tactic` : condition,
  seuil, action automatique / technique / objet), `Fighter::tacticsOn`,
  `Game::tacticsAuto` (touche Tab en combat). Quand la jauge d'un allié est
  pleine, `Battle::tacticTurn` joue la première règle possible
  (`Battle::tacticPlan` choisit la cible et la technique) ; sinon le menu de
  commande s'ouvre. Ajouter une condition : l'entrée dans `CONDS`
  (tactics.cpp) et son test dans `holds` (battle.cpp). Les simulations
  (`autoPlay`) n'utilisent les tactiques que si `simTactics` est vrai.
- Son (audio.hpp) : `audio::play("coup")` joue un bruitage, `audio::music("combat")`
  change de morceau (fondu ; le même morceau continue). `Game::updateMusic`
  choisit la musique à chaque image (lieu selon `themeName`, combat, boss,
  silence pendant la fanfare de fin). En combat, passer par `Battle::sound`
  (seulement le combat affiché, pas les simulations ; l'hôte ou le chef envoie
  `son` aux autres). Les menus font leurs bruits eux-mêmes (`MenuStack::update`).
  Le mode test n'ouvre pas de sortie son mais fabrique chaque morceau et chaque
  effet (`audio::render`) pour vérifier qu'ils s'entendent.
- Commandes : `Game::onKey` lit les touches choisies (`options().keys`) en plus
  des flèches, d'Entrée et d'Échap (toujours actives). Manette :
  `Game::onPad` / `onStick` (appelés par main.cpp, qui ouvre les manettes
  branchées) remplissent le même `Input` ; une direction tenue se répète
  (`padRepeat_`). Plein écran : `options().fullscreen`, appliqué par main.cpp.
- Animations de combat : `Battle::startAnim` (appelé par `useMove` et
  `useItem`) retient le lanceur, les cibles, l'effet (`Battle::Fx` : coup,
  sort, soin, statut, capture) et la couleur du type ; `offset` décale le dessin
  (élan, secousse quand on est touché) et `drawAnim` dessine l'effet. En ligne,
  l'hôte ou le chef envoie `anim` aux autres.
- Police : `gfx.cpp`, fonction `buildFont()`. Un caractère absent s'affiche « ? » ;
  ajoute son dessin si tu utilises un nouveau symbole.
- Données : tout le contenu est dans `data/` (voir `data/LISEZMOI.md`), chargé au
  démarrage par `loadData()`, `loadMaps()`, `loadEvents()`. Aucune donnée de jeu
  ne doit être écrite en dur dans le code : ajouter un champ au JSON et au
  chargeur. Les cartes sont désignées par leur identifiant (`MapDef::id`), pas
  par leur numéro (ordre alphabétique des fichiers).
- Intérieurs : thème `Theme::Interieur` (parquet, murs, meubles `p t h e o v`,
  dessinés dans sprites.cpp). Une porte de bâtiment avec `interieur` fait entrer
  dans cette carte (`Game::interact`, case d'arrivée `interiorEntry` : au-dessus
  du passage de sortie) ; `checkMaps` vérifie la sortie et part de cette case
  pour l'accessibilité. Une carte moins haute que l'écran est centrée
  (`drawMap`). Les 12 intérieurs des villages sont `data/cartes/<village>_<lieu>.json`.
- Quêtes annexes : `data/quetes.json` (lu par `loadEvents`, vérifié par
  `checkEvents`), action `quete` (drapeaux `quete:<id>` et `quete:<id>:fin`,
  `questFlag`), journal `Game::journalMenu`. Trois quêtes : `medaillon`
  (vallée), `braises` (Forgeroc, boss de carte `tisonnels`), `tisane` (Givreval) ;
  leurs habitants sont dans les intérieurs.
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
- Rythme (`Move::pace`, champ `rythme`) : après une technique rapide, la jauge ATB
  du lanceur repart à `jauge_rapide` % ; après une lourde, à -`retard_lourde` %
  (`paceGauge`, passé à `Battle::afterAction`). `Battle::attackScore` note une
  attaque pour `think` et les tactiques : dégâts attendus divisés par
  `paceTime` ; pour les alliés seulement, critiques et effets en plus comptent
  aussi (les ennemis gardent l'ancienne notation, sur laquelle la difficulté
  est réglée). Principe des techniques : une faible est rapide et souvent avec
  un effet, une forte est lourde, pour qu'aucune ne devienne inutile.
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
- Éditeurs : chacun a son mode (`Mode::Arena`, `Settings`, `Editor`, `Story`),
  ses menus dans `MenuStack` et ne modifie que la mémoire jusqu'à
  « Enregistrer ». Les tests en jeu (`editorTest_`, `storyTest_`) ajoutent
  « Fin du test » au menu de pause. Les identifiants créés passent par
  `makeSlug` (minuscules, sans accents, tirets bas).
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
Avec les tactiques de départ (mêmes combats, `simTactics`), l'équipe soigne
plus tôt que l'IA : Sylvarque ≈ 85 %, Ignarok ≈ 65 %, mais Givrecorne ≈ 90 %
(Lior, Maëlle et Sélène savent tous soigner).
Chaque ligne ne joue que 20 à 40 combats : d'une fois à l'autre, un taux varie
de ±10 %. Pour comparer deux réglages, `BRUMEVAL_SIMULATIONS=5` joue cinq fois
plus de combats (±4 %), et c'est plus long.
Expédition (équipe générée : héros, créature de départ et recrue, deux niveaux
au-dessus des créatures sauvages) : dresseurs et sauvages gagnés à 100 %,
gardiens à 85-100 % pour une équipe bien assortie, moins quand les types se
prêtent mal au gardien (le joueur peut capturer d'autres créatures ; les
habitants donnent son point faible). Réglages du gardien dans
`procgen::generateRegion` (niveau, multiplicateur de PV, acolytes, renforts).
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
