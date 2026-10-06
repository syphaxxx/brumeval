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
| Échap | Annuler, ouvrir le menu (équipe, objets, magie, sauvegarde) |
| F11 ou Alt+Entrée | Plein écran |

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
  rencontres ; **Journal** détaille le dernier combat coup par coup.
- **Réglages** : modifiez depuis le jeu les règles (départ, rencontres, formules
  de combat, états, récompenses, butin), les espèces (types, statistiques,
  résistances, immunités, techniques apprises), les techniques, la table des
  types et les objets. Les changements s'appliquent tout de suite : essayez-les
  avec **Tester dans l'Arène**, puis **Enregistrer** (ou **Tout annuler**).
  Gauche/droite règle une valeur, Entrée ouvre un texte à saisir au clavier.

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
- La sauvegarde est écrite dans `%APPDATA%\Brumeval\Brumeval\sauvegarde.txt`
