# Guide pour reprendre Brumeval

Ce guide explique comment le programme est construit et comment y ajouter des
choses. Il complète trois autres documents :

- `README.md` : jouer, compiler, les outils intégrés.
- `data/LISEZMOI.md` : le format de chaque fichier de données.
- `CLAUDE.md` : la référence détaillée (rôle de chaque fichier, principes,
  pièges, équilibrage mesuré). C'est le document que Claude Code lit en premier.

Règle de base du projet : **tout le contenu est dans `data/`**. Le code sait
*comment* jouer ; les fichiers JSON disent *quoi* (créatures, cartes,
dialogues, objets, sons…). Avant d'écrire du code pour un nouveau contenu,
vérifier s'il suffit de modifier `data/`.

## 1. Le programme en une page

```
main.cpp ── ouvre la fenêtre, le son, les manettes ; lit data/ ; boucle :
   │          événements SDL → Game::onKey / onPad / onMouse
   │          Game::update(dt) → Game::draw() → Screen::present()
   ▼
Game (game.hpp) ── l'état du jeu : mode (Mode::Title, Map, Battle…), équipe,
   │               sac, or, drapeaux de progression, carte et position
   ├── Script sc       file d'étapes : messages, appels, pauses (dialogues, scènes)
   ├── MenuStack menus menus à curseur (clavier, souris, manette)
   ├── Battle          combat en cours (battle.hpp)
   ├── Expedition      mode roguelite (expedition.hpp, procgen.hpp)
   ├── Online          multijoueur (online.hpp, coop.cpp, trade.cpp, net.hpp)
   └── Arena, Settings, MapEditor, StoryEditor : les outils (écran titre > Outils)
```

Chaque image : `Game::update` fait avancer ce qui est actif (menu ouvert,
sinon scène en cours, sinon déplacement sur la carte), choisit la musique
(`updateMusic`), puis `Game::draw` dessine l'écran du mode actuel.

L'image fait 320×240 pixels (élargie à la forme de l'écran), puis elle est
agrandie à la taille de la fenêtre. Tout est dessiné par le code (`gfx.cpp`,
`sprites.cpp`) : aucune image ni aucun son n'est chargé depuis un fichier.

**Convention des fichiers** : chaque `.hpp` commence par un commentaire qui
explique son module ; le `.cpp` du même nom contient le code. Les fonctions
importantes ont un commentaire d'une ligne au-dessus.

## 2. Les modules

| Domaine | Fichiers | Ce qu'on y trouve |
|---|---|---|
| Démarrage, fenêtre | `main.cpp` | Taille de l'image, plein écran, manettes, boucle |
| Jeu | `game.hpp/.cpp` | Écran titre, carte, menus (équipe, équipement, journal, objets, options…), sauvegarde |
| Événements, quêtes | `events.hpp/.cpp` | Actions des événements (`Game::execAction`), conditions, quêtes |
| Combat | `battle.hpp/.cpp`, `tactics.hpp/.cpp` | Jauges ATB, dégâts, IA, tactiques, animations, combat en ligne |
| Données | `data.hpp/.cpp`, `world.hpp/.cpp`, `store.hpp/.cpp` | Lecture et vérification de `data/`, cartes, fichiers du joueur |
| Dessin | `gfx.hpp/.cpp`, `sprites.hpp/.cpp`, `ui.hpp/.cpp` | Formes, police, fenêtres ; créatures, humains, tuiles ; menus |
| Son | `audio.hpp/.cpp` | Synthèse des musiques et bruitages de `data/sons.json` |
| Options | `options.hpp/.cpp` | Volumes, plein écran, touches (`options.txt`) |
| Expédition | `expedition.hpp/.cpp`, `procgen.hpp/.cpp` | Monde généré à partir d'une graine, Camp, sauvegarde à part |
| Multijoueur | `net.hpp/.cpp`, `online.hpp/.cpp`, `coop.cpp`, `trade.cpp` | Connexions, salon, duel, expédition à plusieurs, échanges |
| Outils | `arena`, `settings`, `mapedit`, `storyedit` | Arène, réglages, éditeur de cartes, éditeur d'histoire |
| Tests | `test.cpp` | Mode test automatique (`--test`) |

## 3. Les données

**Au démarrage**, `main.cpp` appelle `loadData()` (types, techniques,
espèces, objets, apparences, règles), `loadMaps()` (`data/cartes/`),
`loadEvents()` (événements et quêtes) et `audio::loadSounds()`. Une erreur de
format arrête le jeu avec un message clair ; une incohérence (référence
inconnue, lieu inaccessible) est signalée sans arrêter (`checkData`,
`checkMaps`, `checkEvents`, `audio::checkSounds`).

**En mémoire**, chaque fichier de `data/` est gardé sous forme de document
JSON (`dataDoc`). Les Réglages ne modifient que ces documents, puis
`rebuildData()` reconstruit les structures du jeu (`Species`, `Move`,
`ItemDef`…). L'expédition met son contenu généré à la place de ces documents
le temps d'une partie, puis remet tout (`Expedition::install` / `restore`).

**Fichiers du joueur** (sauvegarde, options, progrès de l'expédition, pseudo) :
dans son dossier personnel, via `userFile(nom)`, et toujours écrits par
`writeUserFile` (fichier temporaire puis remplacement : jamais de fichier à
moitié écrit). Le mode test utilise des noms en `_test`.

**La sauvegarde** (`sauvegarde.txt`) est un texte, une information par ligne :
`carte`, `reveil`, `or`, `drapeau`, `objet`, `auto`, puis pour chaque membre
`membre`, `equipement`, `compagnon` (héros), `tactiques` et ses lignes `tactique` (lues par
`readMemberLine`, partagé avec le multijoueur). La progression de l'histoire,
les coffres ouverts et les quêtes sont des **drapeaux** (`Game::flags`).

**Qui combat** : `frontOf` (data.cpp) — les 3 premiers héros valides, chacun
suivi de sa créature compagnon. Les compagnons agissent seuls en combat.

## 4. Recettes

Après chaque recette : compiler, lancer `build\Debug\brumeval.exe --test
captures`, lire la fin (« TOUS LES TESTS PASSENT ») et regarder les captures
concernées dans `captures/`.

**Une créature** : une entrée dans `data/especes.json` (statistiques,
`forme`, `couleurs`, `apprend`), puis l'ajouter à une zone de rencontres d'une
carte (`zones`, champ `pool`). Une nouvelle *forme* (dessin) : ajouter une
valeur à `Shape` (data.hpp) **avant** `Human`, son nom dans `SHAPES`
(data.cpp), et son dessin dans `drawCreature` (sprites.cpp).

**Un objet ou un équipement** : une entrée dans `data/objets.json` ; pour un
équipement, `"equipement": "arme"` (ou `armure`, `accessoire`) et `"bonus"`.
Le mettre en vente dans l'événement d'une boutique (`evenements.json`,
action `boutique`).

**Une carte** : l'éditeur de cartes (écran titre > Outils) fait tout, ou un
fichier dans `data/cartes/`. Un **intérieur** : une carte au thème
`interieur` avec un `passage` sur la case de sa porte vers le village, et le
champ `interieur` sur le bâtiment du village. `checkMaps` vérifie que tout est
accessible à pied.

**Un dialogue, une scène** : l'éditeur d'histoire, ou `data/evenements.json`
(pages avec conditions, actions). Le lancer depuis un habitant (`evenement`),
une porte, un boss de carte ou une zone déclencheuse.

**Une quête annexe** : une entrée dans `data/quetes.json` (étapes du journal),
puis un événement qui la commence (`{"action": "quete", "id": …}`) et la
termine (`"fin": true`), avec des pages qui testent les drapeaux
`quete:<id>` et `quete:<id>:fin`. Exemple complet : `quete_medaillon`.

**Une musique ou un bruitage** : `data/sons.json` (format dans
`data/LISEZMOI.md`). Une musique de lieu se choisit par le thème de la carte
(`lieux`). Un nouveau bruitage joué par le code : `audio::play("nom")` (en
combat : `Battle::sound`), et l'ajouter à `REQUIRED_EFFECTS` (audio.cpp) pour
que le mode test vérifie qu'il existe.

**Une action d'événement** : son exécution dans `Game::execAction`
(events.cpp), sa vérification dans `checkAction`, et dans l'éditeur
d'histoire (`ACTIONS`, `ACTIONS_FR`, `ACTIONS_HELP`, le résumé, la valeur par
défaut et le menu de réglage, storyedit.cpp).

**Une option** : un champ dans `Options` (options.hpp), sa lecture et son
écriture (`loadOptions` / `saveOptions`), une ligne dans `Game::optionsMenu`.

**Un message réseau** : l'envoyer avec `Online::sendTo` (champ `t` : son nom),
le traiter dans `Online::onMessage` (ou `Battle::netMessage` en combat).
Augmenter `BRUMEVAL_VERSION` (version.hpp) : deux versions différentes ne se
connectent pas. Un message mal formé ne peut pas faire planter le jeu
(`Online::update` coupe la connexion) ; lire les listes avec `at()`.

**Un écran ou un menu** : `Menu` + `MenuItem` (ui.hpp), `menus.push(m)`. Le
« tour des menus » du mode test (`test.cpp`) ouvre les menus avec les noms
les plus longs : y ajouter tout nouvel écran pour que la mise en page soit
vérifiée.

## 5. Tester

Le mode test (`--test DOSSIER`) tourne sans fenêtre ni son :

1. vérifie toutes les données (références, accessibilité, sons) ;
2. joue des scènes par les menus (titre, carte, combat, boutique, équipement,
   quêtes, intérieurs, options, manette, éditeurs, expédition, multijoueur à
   deux par 127.0.0.1) et enregistre des captures `.bmp` ;
3. vérifie la mise en page de **chaque image** dessinée (texte qui dépasse,
   fenêtres qui se chevauchent) ;
4. simule des centaines de combats pour l'équilibrage (taux de victoire), y
   compris l'expédition et le gardien à plusieurs.

Ajouter un test : un bloc dans `Game::selfTest` qui prépare une situation,
fait avancer le jeu (`frame()`, `run(secondes)`, `in.confirm = true` pour
appuyer sur Entrée), puis `check(condition, "description")`. Les tests
touchent seulement des fichiers `_test`.

**Partie complète** : `brumeval --partie DOSSIER` (version Release : une
dizaine de secondes) fait jouer toute l'histoire au joueur automatique
(`src/pilot.cpp`, voir CLAUDE.md) ; il dit où il reste bloqué. À lancer après
un changement d'histoire, de cartes ou d'équilibrage. `brumeval --demo` : la
même chose à l'écran.

`BRUMEVAL_LARGEUR=384` (captures en écran large), `BRUMEVAL_SIMULATIONS=5`
(mesures d'équilibrage plus précises), `BRUMEVAL_JOURNAL=Sylvarque` (journal
détaillé d'un combat simulé), `BRUMEVAL_MONDE=brume` (monde d'une graine).

## 6. Pièges connus

- **JSON temporaire** : ne jamais écrire `for (auto& x : j.value(...).items())`
  (l'objet est détruit avant la boucle : plantage). Ranger d'abord le JSON dans
  une variable.
- **Monde identique partout** : dans `procgen.cpp`, pas de `sin`, `pow`,
  `std::shuffle`, `unordered_map`, ni deux tirages au hasard dans les arguments
  d'un même appel. Le mode test compare l'empreinte de la graine 2026.
- **Accents** : `/utf-8` sous MSVC (déjà dans CMakeLists.txt). Un caractère
  absent de la police s'affiche « ? » : l'ajouter dans `buildFont()`.
- **Mise en page** : 6 pixels par caractère ; un libellé commence 13 pixels
  après le bord du menu. Le mode test signale tout débordement.
- **Fins de ligne** : les fichiers sont en CRLF sous Windows (git les range en
  LF) ; garder 0 avertissement de compilation.
- **Équilibrage** : ne pas retoucher les boss sans demander (choix du
  2026-10-07) ; comparer avec `BRUMEVAL_SIMULATIONS=5`.

## 7. Carte du code (graphify)

`/graphify .` dans Claude Code fabrique une carte du code dans
`graphify-out/` (ignorée par git) : `graph.html` à ouvrir dans un navigateur,
`GRAPH_REPORT.md` (modules les plus reliés, groupes de fonctions). Pour une
question sur le code : `/graphify query "comment marche l'échange ?"`. Après
de gros changements : `/graphify . --update`.
