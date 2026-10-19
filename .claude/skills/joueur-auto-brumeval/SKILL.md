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
Il s'améliore d'une partie à l'autre (demandé le 2026-10-10) : `pilote_memoire.txt` (dossier de l'utilisateur) garde pour chaque combat difficile la marge de niveaux qu'il lui a fallu (mêlée à la précédente, baisse après une victoire du premier coup) et, pour chaque carte, `nature:<carte>` = les meutes qui l'ont battu : les herbes profondes de cette carte lui coûtent plus cher à traverser et il s'y entraîne moins loin (la crainte baisse d'un quart à chaque partie finie). Mesuré : 6, 6, 4, 4 puis 2 défaites sur cinq parties de suite. Il place son équipe avec la formation automatique, vise les zones les plus rentables (IA) et se met à l'abri des attaques annoncées. Échiquier (3.4) : `autoCommand` choisit d'abord une case (`planMove`/`posValue` : attaque possible de là, dans le dos, danger, Percée), puis l'action ; mesuré : histoire finie en 1 h 25-1 h 32 avec 0 à 3 défaites (contre 1 h 26 et 0 défaite en 3.3). Un nouveau genre d'interaction (objet à utiliser, énigme…) demande d'ajouter
un objectif dans `Pilot::goals`. Méthode réutilisable pour d'autres jeux :
compétence `joueur-automatique` de Claude Code.

## Améliorations du 2026-10-13 (version 2.4)

- **Répétition des combats** (`Pilot::ready`, `rehearse`) : avant un combat difficile (boss, dresseur) dont le niveau
  est proche, il le simule 8 à 12 fois avec une copie de son équipe (PV pleins) sans toucher à la partie (équipe, sac,
  or, bestiaire mis de côté puis remis) et n'y va que s'il gagne au moins 85 % du temps (+5 % par défaite de la partie,
  +3 % par point de mémoire). Résultat gardé tant que l'équipe ne change pas (`teamSignature`). Sinon : un niveau de plus.
- **Meutes** : au début d'un combat sauvage de 3 créatures ou plus (ou plus fortes que lui), il le répète 6 fois et fuit
  s'il gagne moins d'une fois sur deux (`fleeNow_`) ; il fuit aussi un combat qui tourne mal (`shouldFlee`). Le mode auto
  est alors coupé pour ce combat : sinon les tactiques jouent le tour et le menu « Fuir » ne s'ouvre jamais.
- **Herbes trop fortes** (`zoneLo_`, `pathRisk`) : il ne prend pas un chemin qui traverse des herbes dont les créatures
  dépassent son niveau de plus de 1 ; il s'entraîne d'abord. Ces herbes coûtent aussi plus cher dans Dijkstra.
- **Petite équipe** (3 combattants ou moins) : soins dès 60 % des PV.
- **Avant un combat difficile** : PV et PM presque pleins (la répétition les suppose pleins) ; sinon il répète le
  combat avec son état actuel (`nowChances_`) et y va quand même si c'est assez sûr. Exiger des PV et PM pleins
  sans cette répétition faisait des centaines d'allers-retours entre l'auberge des Pics et le boss du Temple.
- **Regard des dresseurs** (`watched_`, `watchSight`) : les cases vues par un dresseur (`Npc::sight`) qu'il n'est
  pas prêt à battre coûtent 500 : il passe ailleurs (le Braconnier le battait à chaque début de partie).
- **Herbes** : coût 1 de plus qu'un chemin (mesuré : 1, 3 et 8 se valent à peu près, 1 un peu mieux : les combats
  en route lui évitent de s'entraîner) ; bien plus cher si les créatures dépassent son niveau.
- **Boutique** (`worthTrip`) : il n'y va exprès que pour un meilleur équipement, des lanternes qui manquent ou presque
  plus de soins ; il achète d'abord 2 potions (le meilleur soin qu'il peut payer), et plus quand il est riche.
- **Rapport** : chaque défaite dit contre quoi (adversaires, carte, niveau) ; « temps par activité » (combats sur les
  trajets, combats difficiles, entraînement, trajets, menus) ; historique des parties dans `pilote_parties.txt`
  (temps, défaites, niveau, compagnon) : « Partie n°X. Record : … », « NOUVEAU RECORD ».
- **Démo** : aucune touche ne reprend la main ; Espace (ou Start) met en pause, Échap quitte, F1/F2/F11 marchent.

## Banc d'essai (pour juger un changement)

`BRUMEVAL_SANS_MEMOIRE=1` : ni mémoire ni historique (chaque partie part de zéro). Jouer **6 parties** (2 par compagnon
de départ, `BRUMEVAL_COMPAGNON`) et comparer les moyennes (temps, défaites, niveau final, combats) : d'une partie à
l'autre, le temps varie de ±15 min, une seule partie ne prouve rien. Pour comparer à l'ancienne version, la compiler
à part (`git stash`, compilation, copie de l'exécutable) et mettre de côté `pilote_memoire.txt` pendant ses parties.

Mesuré le 2026-10-13 (6 parties chacune) : ancienne version 73 min, 5,8 défaites, niveau 58, 240 combats ; version 2.4
68 min, 2,5 défaites, niveau 53, 218 combats (meilleure partie : 53 min, 1 défaite). Le temps se partage à peu près
en trois : combats sur les trajets, trajets (les coffres d'abord), combats difficiles.

- **Ordre des trajets** : un objectif d'une autre carte compte 40 cases de plus (finir la carte en cours d'abord).
  Mesuré : même temps (68 min), un peu moins de défaites ; la marche dépend surtout de la carte du jeu.
- **Circuit** (`tour_`, `distFrom`) : les 10 objectifs ordinaires les plus proches sont ordonnés avec les vraies
  distances à pied entre eux (le plus proche d'abord, puis 2-opt) ; il suit ce circuit tant que la liste ne change pas.
- **Conversations sans effet** (`Info::effect`) : une page qui ne fait que parler (dire, attendre, réveil) ne vaut
  pas le trajet ; il ne lit plus les panneaux ni les simples dialogues.
- **Estimation prudente** (`prudent`) : la part de victoires répétée moins un écart type ; 16 répétitions pour un boss.
  Sans elle, à force de répéter après chaque petit changement, un tirage chanceux passait le seuil et il perdait.
  Meutes : 8 répétitions, fuite si l'estimation prudente est sous 70 %.
- **Entraînement avec mémoire** (2026-10-13, 5 parties de suite) : 1 h 02, 1 h 20, 1 h 19, 0 h 58 (record, 0 défaite),
  1 h 17. Braisenard est le compagnon le plus rapide (environ 1 h), Gouttelin et Ronceau environ 1 h 18.

Mesuré ensuite sur **12 parties** (plus fiable que 6) : circuit 69 min et 3,1 défaites ; + estimation prudente 68 min
et 3,0 ; + fuite des meutes à 70 % **67 min et 1,5 défaite** (de 54 à 78 min).

**Essais ratés** (mesurés puis retirés) : l'IA du jeu à la place des tactiques pour chaque action (69 min, 3,5
défaites : les tactiques soignent plus tôt) ; fuir toute rencontre de 8 niveaux sous le sien (il fuyait aussi dans ses
zones d'entraînement : 5 parties sur 6 bloquées) ; seuil de répétition à 60 % (plus de défaites que de temps gagné).

**Leçon** : se soigner sur place avec les potions (au lieu de rentrer à l'auberge) a été essayé puis retiré : 92 min et
11,8 défaites. Les potions ne rendent pas les PM : les lanceurs de sorts tombaient à court au combat suivant.
L'auberge rend tout. Toujours mesurer chaque idée au banc d'essai, même celles qui semblent évidentes.

**Version 2.5** (Brisure, talents, boss plus durs ; 2026-10-14, 3 parties sans mémoire, une par compagnon) : toujours
finie, 63, 75 et 76 min, 0, 2 et 1 défaite, niveau ~52. Il vise déjà les faiblesses et les ennemis brisés : son IA
(`attackScore`, alliés) et ses répétitions (`rehearse`, boucliers des événements) en tiennent compte.

**Version 2.6** (pousser, météo, terrain, duos ; 3 parties sans mémoire) : toujours finie, 71, 75 et 76 min, mais 5, 2
et 6 défaites, presque toutes contre des meutes sauvages au début (les créatures qui poussent provoquent des chocs).
Choc ramené de 0,5 à 0,35 des dégâts. Les duos passent par la tactique « Meilleure attaque » ; « Se mettre à l'abri »
quitte les cases qui blessent.

**Version 2.7** (gemmes, rôles, boss en phases ; 3 parties sans mémoire) : toujours finie, 62, 94 et 83 min, 2, 4 et 0
défaite, niveau 50 à 63 (le jeu est plus dur : il s'entraîne plus longtemps). Il sertit les gemmes reçues
(`Pilot::socketGems`, à chaque `plan`), vise d'abord soigneurs, chefs et protecteurs (`attackScore`), et ses
répétitions de boss jouent leurs phases (`makeFoe`). Il n'achète pas encore de gemmes en boutique (piste).

**Version 3.0** (acte 2) : le bateau était d'abord une téléportation d'événement (`teleporter`) : le joueur automatique
n'en connaît pas le chemin (son graphe ne suit que les `passages` des cartes), il arrivait au port, repartait par le
passeur et ne revenait jamais. Le bateau est donc un vrai passage, fermé par la condition `boss4`. Leçon : tout
chemin nécessaire à l'histoire doit être un passage de carte, pas une téléportation. Énigmes : objectif
`enigme:<carte>` (case de départ de l'énigme), puis `solvePuzzle` (27 et 41 pas). Partie complète : 1 h 25, 2 défaites.
