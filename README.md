# Brumeval — Les gardiens de la vallée

Un petit RPG en C++ et SDL2. Lior, apprenti gardien, part chasser la brume qui
rend les créatures sauvages. Combats au tour par tour actif à la Final Fantasy 7
(jusqu'à trois combattants, jauges ATB, Limites, magie), créatures à capturer
pour agrandir l'équipe, quatre héros recrutables, deux régions et une grotte.

## Commandes

| Touche | Action |
|---|---|
| Flèches ou ZQSD | Se déplacer, naviguer dans les menus |
| Entrée ou Espace | Parler, valider, faire défiler les dialogues |
| Échap | Annuler, ouvrir le menu (équipe, tactiques, objets, magie, sauvegarde) |
| Tab (en combat) | Mode auto : les tactiques jouent à votre place, ou vous reprenez la main |
| F11 ou Alt+Entrée | Plein écran ou fenêtre (le choix est gardé pour la prochaine fois) |

## Tactiques (combat automatique)

Comme les « gambits » de Final Fantasy XII, chaque membre de l'équipe a une
liste de règles « condition → action » (menu Échap > **Tactiques**), par
exemple :

1. Allié : K.O. → Réanimation
2. Allié : PV < 40 % → Meilleur soin
3. Soi : Limite prête → Limite
4. Ennemi : n'importe lequel → Meilleure attaque

En combat, quand la jauge d'un membre est pleine et que le **mode auto** est
actif (touche **Tab**, indiqué « Auto (Tab) » en bas de l'écran), la première
règle possible, de haut en bas, est jouée toute seule. Si aucune ne l'est, le
menu de commande s'ouvre comme d'habitude. Appuyer sur Tab pendant que le menu
attend un ordre laisse les tactiques jouer ce tour.

Les conditions visent un ennemi (le moins de PV, PV < x %, faible à l'action,
sans l'effet, boss…), un allié (PV < x %, K.O., avec un état, PM < x %…) ou le
membre lui-même. Les actions sont automatiques (meilleure attaque, attaque sans
PM, meilleur soin, réanimation, guérison, Limite), une technique ou un sort
précis, ou un objet du sac (potion, éther, lanterne de capture…). Chaque
personnage a 4 lignes au départ, puis une de plus tous les 6 niveaux (10 au
maximum). Une règle grise ne peut pas encore servir (sort pas encore appris),
une règle orange ne marchera pas (par exemple une attaque avec une condition
« Allié »).

## Installer et lancer sous Windows avec VS Code

1. Installez les outils (une seule fois) :
   - **Git** : https://git-scm.com
   - **CMake** : https://cmake.org/download (cochez « Add CMake to the PATH »)
   - **Build Tools pour Visual Studio** (le compilateur C++ de Microsoft) :
     https://visualstudio.microsoft.com/fr/downloads/ , section « Outils pour
     Visual Studio ». À l'installation, cochez la charge de travail
     **Développement Desktop en C++**.
   - Dans VS Code, les extensions **C/C++** et **CMake Tools** (VS Code les
     propose automatiquement à l'ouverture du projet).
2. Récupérez le projet :
   ```
   git clone https://github.com/syphaxxx/brumeval
   ```
3. Dans VS Code : **Fichier > Ouvrir le dossier…** et choisissez `brumeval`.
   CMake Tools vous demande un kit de compilation : choisissez celui de
   Visual Studio (amd64). La première configuration télécharge SDL2 : il faut
   Internet et quelques minutes.
4. **F7** pour compiler, puis **Maj+F5** pour lancer le jeu (ou le bouton ▶ de
   la barre d'état en bas).

## Outils

Depuis l'écran titre, **Outils** donne accès à :

- **Arène de combat** : composez deux équipes (espèces, niveaux, PV des boss,
  renforts, décor), puis **Combattre !** pour jouer vous-même ou **Simuler**
  pour faire jouer des dizaines de combats à l'ordinateur et voir le taux de
  victoire. **Modèles…** reprend tous les combats de l'histoire et les zones de
  rencontres ; **Journal** détaille le dernier combat coup par coup. Chaque
  allié a ses **Tactiques…**, et l'option **Alliés** choisit si les
  simulations suivent les tactiques ou l'IA de l'ordinateur.
- **Réglages** : modifiez depuis le jeu les règles (départ, rencontres, formules
  de combat, états, récompenses, butin, tactiques de départ et nombre de
  lignes), les espèces (types, statistiques,
  résistances, immunités, techniques apprises), les techniques, la table des
  types et les objets. Les changements s'appliquent tout de suite : essayez-les
  avec **Tester dans l'Arène**, puis **Enregistrer** (ou **Tout annuler**).
  Gauche/droite règle une valeur, Entrée ouvre un texte à saisir au clavier.
- **Éditeur de cartes** : peignez les tuiles à la souris (clic gauche, clic
  droit = pipette, molette = tuile suivante) ou au clavier (flèches + Entrée,
  Maj + flèches pour peindre en marchant), avec pinceau, rectangle ou
  remplissage. **Tab** passe aux calques Objets (habitants, coffres, panneaux,
  passages, boss, bâtiments) et Zones (rencontres, zones déclencheuses).
  **Échap** ouvre le menu : propriétés et taille de la carte, nouvelle carte,
  vérification, **Tester ici** (Échap > Fin du test), enregistrer
  (Ctrl+S). Ctrl+Z / Ctrl+Y annulent et rétablissent.

- **Éditeur d'histoire** : tous les événements (dialogues, scènes, boss,
  dresseurs, boutiques…). Un événement est une liste d'actions (dire, donner,
  combat, question, si…) ou plusieurs **pages** avec une condition (drapeau,
  objet, or, membre de l'équipe). Les actions qui en contiennent d'autres
  (réponses Oui/Non, victoire/défaite d'un combat, alors/sinon) s'ouvrent comme
  des sous-listes. **Jouer l'événement** le lance à l'endroit où il est utilisé,
  avec une équipe et des drapeaux de test ; un combat peut s'essayer dans
  l'Arène. Ctrl+Z / Ctrl+Y / Ctrl+S comme dans l'éditeur de cartes.

Les menus se manient aussi à la souris : survol, clic gauche pour valider, clic
droit pour revenir, molette pour défiler ou régler une valeur.

## Continuer le projet avec Claude Code

Ouvrez un terminal dans le dossier du projet (dans VS Code : **Terminal >
Nouveau terminal**) et lancez `claude`. Le fichier `CLAUDE.md` lui donne tout le
contexte : architecture, commandes, état du projet et idées pour la suite.

## Tester

```
build\Debug\brumeval.exe --test captures
```

(ou `build\brumeval.exe --test captures`, selon la configuration de CMake)

Le mode test vérifie les données, joue quelques scènes, simule des combats pour
l'équilibrage et enregistre des captures d'écran dans `captures/`.

## Organisation

- `src/` : le code du jeu (voir `CLAUDE.md` pour le rôle de chaque fichier)
- `data/` : tout le contenu du jeu (créatures, techniques, objets, cartes,
  dialogues, règles) en fichiers JSON modifiables, lus à chaque démarrage. Voir
  `data/LISEZMOI.md`.
- La sauvegarde est écrite dans `%APPDATA%\Brumeval\Brumeval\sauvegarde.txt`,
  le choix plein écran / fenêtre dans `options.txt` à côté
  (le mode test utilise `sauvegarde_test.txt` et l'efface à la fin)
