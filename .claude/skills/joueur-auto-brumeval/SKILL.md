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
Il s'améliore d'une partie à l'autre (demandé le 2026-10-10) : `pilote_memoire.txt` (dossier de l'utilisateur pour la démo ; depuis le 2026-10-25, celui de la partie pour `--partie <dossier>` : deux mesures en parallèle ne s'écrasent plus) garde pour chaque combat difficile la marge de niveaux qu'il lui a fallu (mêlée à la précédente, baisse après une victoire du premier coup) et, pour chaque carte, `nature:<carte>` = les meutes qui l'ont battu : les herbes profondes de cette carte lui coûtent plus cher à traverser et il s'y entraîne moins loin (la crainte baisse d'un quart à chaque partie finie). Mesuré : 6, 6, 4, 4 puis 2 défaites sur cinq parties de suite. Il place son équipe avec la formation automatique, vise les zones les plus rentables (IA) et se met à l'abri des attaques annoncées. Échiquier (3.4) : `autoCommand` choisit d'abord une case (`planMove`/`posValue` : attaque possible de là, dans le dos, danger, Percée), puis l'action ; mesuré : histoire finie en 1 h 25-1 h 32 avec 0 à 3 défaites (contre 1 h 26 et 0 défaite en 3.3). Un combat d'entraînement sans récompense (`entrainement`, sans `victoire` : le maître d'armes une fois les bottes données) n'est ni un objectif ni un effet (`inspectList`) : sinon il le rejoue sans fin (vu le 2026-10-19, bloqué 30 min). Un combat `defi` se répète avec les combattants du défi (`challengeSetup`). Un nouveau genre d'interaction (objet à utiliser, énigme…) demande d'ajouter
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
  trajets, combats difficiles, entraînement, trajets, menus) ; historique des parties dans `pilote_parties.txt` (même dossier)
  (temps, défaites, niveau, compagnon) : « Partie n°X. Record : … », « NOUVEAU RECORD ».
- **Démo** : aucune touche ne reprend la main ; Espace (ou Start) met en pause, Échap quitte, F1/F2/F11 marchent.

## Banc d'essai (pour juger un changement)

`BRUMEVAL_SANS_MEMOIRE=1` : ni mémoire ni historique (chaque partie part de zéro). Jouer **6 parties** (2 par compagnon
de départ, `BRUMEVAL_COMPAGNON`) et comparer les moyennes (temps, défaites, niveau final, combats) : d'une partie à
l'autre, le temps varie de ±15 min, une seule partie ne prouve rien. Pour comparer à l'ancienne version, la compiler
à part (`git stash`, compilation, copie de l'exécutable) et la lancer dans un autre dossier (`--partie <dossier>`) : chaque dossier a sa mémoire.

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

## Démo : tout voir comme un joueur (2026-10-23)

Demandé par l'utilisateur : « en démo je veux voir tout ce que fait le joueur auto, l'équipement, les tactiques, tous
les menus ». En démo seulement (`demo_`), le pilote fait des **visites** (`Pilot::visit`, `visitStep`) : un chemin de
libellés choisis un à un avec le curseur depuis le menu de pause (Échap), puis un temps pour regarder, puis il referme.
`pickItem("Titre|libellé")` choisit le premier libellé après ce titre de section (« Arme|(rien) » : plusieurs
emplacements vides ont le même libellé). Visites : chaque changement d'équipement (`equipBest(true)` après la boutique :
Équipement > membre > emplacement > objet), chaque gemme (`socketGems(true)` : Gemmes > « Arme : (vide) » > gemme),
tactiques puis formation d'un nouveau membre (et les tactiques de Lior au début), Journal à chaque nouvelle quête et
Bestiaire après une capture (`lookAt`, une par minute de jeu au plus). Après la dernière visite d'équipement,
`equipBest(false)` finit ce que les menus n'auraient pas fait. Hors démo, rien ne change (les mesures restent rapides).
`BRUMEVAL_DEMO=1 brumeval --partie dossier` : le pilote de la démo sans fenêtre, une capture `NN_menus.bmp` à chaque
menu visité et une ligne « Menus : Équipement > Lior > … » dans le rapport.

## Tactiques du joueur automatique (2026-10-23)

Demandé : « développe le mode tactique et que le joueur auto l'utilise ». `Pilot::setupTactics` (à chaque `plan`) donne
à chaque membre un profil de `rules().tacticProfiles` : Soigneur (sort de soin et Magie ≥ Attaque), Protecteur (rôle
`protecteur`), Briseur (au moins 3 types d'attaque, pour trouver les faiblesses), sinon Combattant ; refait quand une
ligne de plus sert le profil (`profileOf_` : profil et lignes). En démo, par les menus (Préparation > Tactiques >
membre > Profils… > profil). Rapport : « Tactiques : Lior → Combattant ».

Prêt ou pas (2026-10-23) : `Pilot::ready` répète toujours un boss, ou un combat déjà perdu, même loin au-dessus de son
niveau (avant, 3 niveaux + marge au-dessus suffisaient : la Braisaure ancestrale, trois carapaces, se perdait 8 fois de
suite avec une équipe sans magie, et la partie se bloquait). Simulation de cette prime dans le mode test.

**Dur mais tactique** (2026-10-24, branche `jeu-tactique`) : `Pilot::setupTactics` choisit `bestProfile(f)` (le même
que l'éditeur marque « (conseillé) ») et `profileRules(p, lignes, f)` : les règles de soin sont retirées à qui ne sait pas
soigner, sinon tout le monde réanime et soigne (un seul soigneur faisait perdre les meutes de fin de partie) ; refait
quand ses règles changent (technique apprise, ligne de plus). En combat, l'IA alliée (`autoCommand`, `safeCell`) et la
tactique « Se mettre à l'abri » tiennent compte des intentions annoncées par les ennemis (`dangerAt`) ; « Attendre » met
en garde (× 0,75). Mesure sans mémoire (bot14) : tout fini en 8 h 20, 10 défaites, niveau 112 (7 h 40 / 27 / 103 avant) ; voir la
compétence `equilibrage`. Les zones de niveau 80 et plus (`zoneLo_`) se jaugent au milieu de leur fourchette (lo+hi)/2 :
il entrait dans l'Abîme et le Vieux Phare au bas de la zone et y perdait en boucle (bot15 : 15 défaites au Phare).

**Compagnons selon le lieu** (2026-10-24, `Pilot::adaptTeam`, à chaque `plan`) : pour les espèces des zones de la carte, il
note chaque créature (équipe et réserve) par sa meilleure attaque contre chacune (efficacité × puissance, le niveau compte)
et donne les trois meilleures aux trois héros de tête (réserve échangée contre les moins bonnes), seulement pour +20 % ;
en démo, par Préparation > Réserve et Compagnons. Avant : quinze défaites au Vieux Phare avec deux Champignards et un
Flammenard contre des Cendrelys qui absorbent le Feu. Journal : « Compagnon : Lior → Gouttelin (Le Vieux Phare) ».

**Répétition dans le bon décor** (2026-10-24) : `rehearse` jouait dans le thème de la carte où se tenait le pilote (souvent
une ville) ; météo et terrain, tirés selon le thème, manquaient : la canicule du Cratère des Échos renforce le Phénix, le
combat « sûr à 85 % » se perdait quatre fois de suite (bot22). Le combat d'un objectif porte maintenant sa carte
(`fight["carte"]`, posé dans `goals` et `watchSight`) et la répétition se joue dans son thème.

## Plus malin, comme un bon joueur (2026-10-25)

Demandé : « le rendre plus malin en combat et dans ses choix ». Ce qui a compté (mesures plus bas) :

- **Jauger toute rencontre** (`Pilot::update`, début d'un combat sauvage) : avant, seules les meutes de 3 créatures ou
  plus (ou plus fortes que l'équipe, ou une embuscade) étaient répétées ; il perdait contre deux créatures de son niveau
  (Échoplume et Pétrolin au Vieux Phare, Loup d'aube et Nacrelin à la Citadelle). Maintenant, toute rencontre dont une
  créature est à 5 niveaux de l'équipe ou plus forte : 10 répétitions (`rehearse`, état actuel, alpha compris :
  `boss` passé à `makeFoe`), fuite sous 80 % prudents. Ces répétitions se jouent sur le champ de bataille du vrai
  combat (`rehearse(…, like)` : sa météo, son terrain, ses obstacles ; avant, tirés à nouveau à chaque répétition : une
  canicule ou des cases qui blessent ne comptaient qu'en moyenne). En plein combat (`shouldFlee`), fuite aussi dès la moitié des PV
  perdue quand l'adversaire garde les deux tiers des siens.
- **Compagnons revus à chaque carte** : `walk` appelle `adaptTeam` dès que la carte change (`planMap_`), avant la
  première rencontre (avant, seulement au prochain `plan`, après le premier combat du lieu, souvent perdu), et
  replanifie si l'équipe a changé (les répétitions se refont avec elle), sauf en route vers l'auberge : replanifier à
  chaque carte le faisait tourner en rond entre Havre-d'Aube et l'île de Corail (l'équipe changée changeait la
  répétition du boss « avec l'état actuel » : boss, puis auberge, puis boss… ; BLOQUÉ à 4 h 21). `adaptTeam` note une
  créature par efficacité × puissance × bonus de type × sa statistique (Attaque ou Magie) contre la Défense ou la
  Résistance de base de l'espèce : un attaquant physique ne vaut plus un mage contre les carapaces et les esprits.
- **Devant un boss ou un dresseur** (`interactNow` → `bossTeam`) : il essaie les compagnons choisis contre les
  combattants de ce combat (`adaptTeam(false, &fight)`), répète le combat avec les deux équipes (16 fois chacune) et ne
  garde les nouveaux que s'ils gagnent nettement plus (+5 points) ; journal « Compagnons pour … : 60 → 80 % ». La
  première version les prenait sans répéter : le Seigneur des Orages a été perdu avec eux.
- **Combats enchaînés** (rangs de la Ligue : trois combats sans soin) : `rehearse` joue toute la suite (le premier
  `combat` de chaque `victoire`, PV et PM gardés) et `inspectList` prend le niveau le plus haut de la suite ; avant, il
  ne répétait que le premier et perdait le deuxième ou le troisième.
- **PM** : retour à l'auberge aussi quand les PM de l'équipe tombent sous 25 % (sans PM, ni soins ni magie).
- **Tactiques même mode auto coupé** (`battleStep`) : quand il coupe le mode auto (capture possible, fuite envisagée)
  et que le menu s'ouvre sans capture ni fuite à faire, il joue `tacticTurn` (puis `autoCommand` si aucune règle ne
  s'applique), comme la répétition qui a jugé le combat ; avant, l'IA seule jouait tous les combats de capture du début.
  Il ne capture que sans risque (`startChance_` : répétition parfaite, ou un seul adversaire encore debout) : un tour à
  lancer une lanterne est un tour sans frapper, que la répétition ne compte pas (défaites contre deux créatures de son
  niveau dans la Vallée, au début).
- **Profil Soigneur** (`regles.json`) : soigne sous 55 % des PV (45 % avant) : contre les meutes qui s'acharnent
  (`lure`), 45 % arrivait trop tard.

Pas fait (pistes) : `autoCommand` ne sert presque jamais en mode tactiques (la dernière règle « ennemi → attaque »
s'applique toujours), le changer ne change rien ; viser d'abord l'ennemi qu'un coup abat demanderait une condition de
tactique ; la formation automatique (`formation`) met déjà les lanceurs de sorts derrière.

Mesuré (sans mémoire, une partie chacune, même ordinateur ; d'une partie à l'autre ±2 h et ±8 défaites) : version
d'avant **16 h 19, 24 défaites, quatre actes en 7 h 47** (référence donnée : 13 h 55, 23, 7 h 27) ; premières
améliorations (rencontres jaugées, fuite plus tôt, compagnons, PM, Soigneur) 14 h 00, 9, 5 h 43 ; état intermédiaire
(compagnons sans replanifier, capture prudente) 15 h 31, 18, 7 h 50 ; + replanifier si les compagnons changent 13 h 14,
7, 6 h 29 ; **état final** (+ champ de bataille du vrai combat) 14 h 52, 12, 6 h 23. Moyenne des quatre parties
améliorées : 14 h 24, 11,5 défaites, actes en 6 h 36 (contre 15 h 07, 23,5, 7 h 37 pour les deux mesures d'avant) :
deux fois moins de défaites, l'histoire une heure plus tôt. Restent : le Phénix d'écho et l'Ombre du Gardien (perdus
une ou deux fois malgré la répétition : la canicule du jour du combat n'est pas connue d'avance), des meutes de 3-4 au
Sanctuaire des Échos et au Vieux Phare (répétées à 80 % et plus, perdues quand même), quelques dresseurs vus trop tard.

**Alphas à mécanique** (2026-10-26) : la répétition d'une rencontre sauvage (`rehearse` au début du combat) garde la
`mecanique` de l'alpha (sinon il croyait gagner une meute dont l'alpha se cuirasse ou charge) ; une défaite contre un alpha
est marquée « (alpha <genre>) » dans le rapport. En route vers son entraînement (`Grind`), un changement de compagnons à
l'arrivée sur une carte ne refait plus le plan (comme en route vers l'auberge) : les compagnons changent le niveau moyen,
donc la zone d'entraînement choisie, et bot32 allait et venait entre le Bois des Lueurs et le Cratère des Échos sans
combattre (BLOQUÉ à 6 h 12).
