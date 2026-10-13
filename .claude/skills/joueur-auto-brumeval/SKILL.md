---
name: joueur-auto-brumeval
description: "Brumeval : fonctionnement du joueur automatique (src/pilot.cpp, --partie, --demo), son rapport et sa mémoire d'une partie à l'autre. À lire avant de le modifier ou d'analyser un rapport."
---

# Joueur automatique de Brumeval

`src/pilot.hpp/.cpp` : un joueur qui joue avec les **mêmes touches** qu'un
humain (marcher, Entrée, menus ; en combat, l'IA `autoCommand` ou une
lanterne). Rien n'est écrit d'avance : à chaque décision, `Pilot::goals`
liste ce qu'il y a à faire sur toutes les cartes (habitant dont la réponse a
changé : signature `Pilot::inspect` = page + conditions ; coffre, panneau,
porte sans intérieur, boss, carte jamais vue), et `plan` va au plus proche
(Dijkstra sur toutes les cartes, passages et portes compris). Ordre : se
soigner si besoin, boutique (meilleur équipement, lanternes, potions),
le reste, puis les combats difficiles quand l'équipe est prête (niveau du
combat −2 pour un boss, −3 sinon, +2 par défaite), sinon entraînement dans
les hautes herbes. Il capture des créatures tant que les héros manquent de
compagnons (mode auto coupé pour ces combats). Il passe les écrans de fin
(Ignarok, puis Givrecorne) et s'arrête quand il n'y a plus rien à faire.
Rapport : journal à l'écran et `partie/rapport.txt`, captures `.bmp` des
moments marquants, problèmes de mise en page ; code 0 si l'histoire est finie.
« BLOQUÉ » : plus rien à faire sans écran de fin, 30 minutes de jeu sans
progrès, ou 8 défaites contre le même combat. `BRUMEVAL_TRACE=1` affiche
chaque décision ; `BRUMEVAL_COMPAGNON=gouttelin` choisit le premier
compagnon. Fichiers : `sauvegarde_partie_auto.txt`, `sauvegarde_demo.txt`
(jamais la vraie partie). En démo, une touche rend la main au joueur. La
vérification GitHub lance aussi `--partie`. Mesuré le 2026-10-09 (12 parties,
3 premiers compagnons) : toujours finie, 45 à 60 minutes de jeu, niveau ~48.
Il s'améliore d'une partie à l'autre (demandé le 2026-10-10) : `pilote_memoire.txt` (dossier de l'utilisateur) garde pour chaque combat difficile la marge de niveaux qu'il lui a fallu (mêlée à la précédente, baisse après une victoire du premier coup) et, pour chaque carte, `nature:<carte>` = les meutes qui l'ont battu : les herbes profondes de cette carte lui coûtent plus cher à traverser et il s'y entraîne moins loin (la crainte baisse d'un quart à chaque partie finie). Mesuré : 6, 6, 4, 4 puis 2 défaites sur cinq parties de suite. Il place son équipe avec la formation automatique, vise les zones les plus rentables (IA) et se met à l'abri des attaques annoncées. Un nouveau genre d'interaction (objet à utiliser, énigme…) demande d'ajouter
un objectif dans `Pilot::goals`. Méthode réutilisable pour d'autres jeux :
compétence `joueur-automatique` de Claude Code.
