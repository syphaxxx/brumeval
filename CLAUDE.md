# Brumeval — contexte pour Claude Code

Petit RPG 2D en C++17 avec SDL2, inspiré de Final Fantasy 7 (combat ATB à trois
combattants, Limites, magie) et de Pokémon (capture de créatures). Tout le contenu
est original. Le projet a été démarré dans l'app Claude puis transféré ici.

## L'utilisateur

- Il est francophone et débute en programmation : réponds en français, explique
  simplement ce que tu changes et pourquoi, sans jargon inutile.
- Il travaille sous **Windows avec Visual Studio Code** (extensions CMake Tools et
  C/C++). Le code a été écrit et testé sous Linux avec GCC ; la première
  compilation avec MSVC sous Windows n'a pas encore été faite (voir « À faire »).
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
| `main.cpp` | Fenêtre SDL, boucle principale, plein écran (F11), option `--test` |
| `game.hpp/.cpp` | Écran titre, exploration, dialogues des habitants, menus (pause, équipe, objets, magie, boutique), sauvegarde, dessin de la carte |
| `battle.hpp/.cpp` | Combat ATB : jauges, menus de commande, dégâts, sorts, objets, capture, Limites, victoire |
| `data.hpp/.cpp` | Types et efficacités, techniques et sorts, espèces (héros et créatures), objets, apparence des humains, formules de stats et d'expérience |
| `world.hpp` | Structures des cartes (tuiles, bâtiments, PNJ, coffres, panneaux, passages, zones, boss) et légende des tuiles |
| `world_data.cpp` | **Généré** par `tools/generate_world.py` : les 3 cartes et leur contenu |
| `sprites.hpp/.cpp` | Dessin en code des créatures, humains, tuiles, bâtiments (aucune image externe) |
| `gfx.hpp/.cpp` | Primitives de dessin (ellipses, polygones, dégradés), police pixel intégrée avec accents, fenêtres bleues |
| `ui.hpp/.cpp` | Clavier, `Script` (file de messages/actions) et `MenuStack` (menus à curseur) |
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
- Sous MSVC, l'option `/utf-8` (déjà dans CMakeLists.txt) est indispensable pour
  les accents.

### Modifier les cartes

Édite `tools/generate_world.py` (fonctions `vallee()`, `cendres()`, `grotte()`),
puis régénère : `python3 tools/generate_world.py src/world_data.cpp`. Le script
vérifie que chaque porte, PNJ, coffre, panneau, passage et boss est accessible et
échoue sinon. Modifier `world_data.cpp` à la main reste possible (garder des
lignes de même longueur), mais la vérification d'accessibilité est alors perdue.

## Contenu actuel

- **Vallée de Brumeval** (64x44) : village (soin, boutique, chapelle, ancien),
  Bois Murmurant (herbe lunaire pour recruter Maëlle), lac et pêcheur, prairies,
  bosquet, col gardé par **Sylvarque** (N.12, Ombre) et deux Brumelins.
- **Monts Cendrelune** (64x44) : ville de Forgeroc (soin, boutique, auberge,
  forge), duel contre **Brann** pour le recruter, champs de cendres, rivières de
  lave, cratère d'**Ignarok** (N.23, Feu, boss final).
- **Grotte des Échos** (32x24) : **Golem de suie** (N.16), puis **Isra** rejoint
  l'équipe.
- Héros : Lior (épée), Maëlle (mage blanche), Brann (hache), Isra (mage noire).
  Starters : Braisenard (Feu), Gouttelin (Eau), Ronceau (Plante). 12 créatures
  sauvages capturables.

Équilibrage mesuré par le mode test (IA automatique simple, sans objets) :
combats normaux gagnés à 100 %, Sylvarque ≈ 80 % avec une équipe N.11,
Ignarok ≈ 35 % avec une équipe N.22 (un joueur avec des objets fait mieux).

## À faire / pistes

1. Compiler et lancer sous Windows avec MSVC dans VS Code, corriger ce qui
   coince (avertissements MSVC, chemin de l'exécutable, débogueur).
2. Ajouter musique et effets sonores (SDL2_mixer via FetchContent, ou l'audio de SDL).
3. Rendre le sprite d'Ignarok plus lisible (aujourd'hui un bloc rouge).
4. Dans la fenêtre d'état du combat, les noms sont coupés à 9 caractères
   (« Braisenar ») : élargir la colonne ou abréger proprement.
5. Idées : intérieurs des maisons, quêtes annexes, équipement, menu d'options,
   manette (SDL_GameController), animations d'attaque.
