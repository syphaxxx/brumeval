# Planning de la suite de Brumeval

État au 19 octobre 2026 : version **3.3** publiée ; la **3.4** (échiquier, personnages rendus avec Blender, effets faits avec Godot) est prête sur la branche `echiquier`, pas encore publiée. Historique : 2.4 (joueur automatique plus malin, commandes de la démo), 2.3 (code de partie), **2.2** (les 1.7 à 2.2 en une fois : damier 4x4, zones, vitesse, Aide,
forge, évolution, graphismes). Avant elle : 1.6 (réserve, bestiaire, Source des Brumes), 1.5 (boss renforcés, compagnons).
Avant elle : 1.3 (son, options, manette, animations d'attaque, Ignarok
redessiné, intérieurs, équipement, quêtes annexes), 1.3.1 (musique) et 1.4
(gardien à plusieurs, équipement en expédition et en duel).

Les durées sont données en **séances de travail avec Claude** (une séance :
une demande comme « ajoute l'équipement », avec ses tests). Les étapes sont
dans l'ordre conseillé ; chacune finit par une version publiée.

## Étape 0 — Publier la 1.3 (fait : 1.3 à 1.5 publiées)

- Relire la branche (PR), essayer le jeu, fusionner, publier `v1.3`.
- Essayer **une fois** dans VS Code : F7, Maj+F5, F5 (voir CLAUDE.md, Compiler et lancer).
- Faire passer les amis en 1.3 (la 1.2 ne se connecte plus à la 1.3).

**Décisions prises le 9 octobre** (faites en version 1.4) : gardien à plusieurs
un peu renforcé (+15 % de PV par joueur en plus), équipement dans les
boutiques de l'Expédition, duels avec l'équipement. Version 1.5 : boss
renforcés (il faut s'équiper) et **compagnons** (chaque héros combat avec une
créature, qui agit seule).

## Étape 1 — Essais et finitions (1 à 2 séances)

- Fait : **joueur automatique** (`brumeval --partie`, `--demo`, Outils >
  Démo). Il finit l'histoire à chaque fois (12 parties sur 12) ; il a trouvé
  un message trop large après l'écran de fin (corrigé). Fait le 2026-10-09 : multijoueur entre **deux vrais ordinateurs** (via Tailscale ; « délai dépassé » en réseau local à cause de NordVPN sur l'hôte). Reste : une partie complète par un vrai joueur.
- Fait : la victoire contre le Givrecorne (vraie fin) affiche aussi l'écran
  de fin ; son texte vient des données (action `fin`, champ `texte`).
- Idée : faire jouer aussi l'Expédition au joueur automatique.
- Fait en 1.5 : équilibrage des boss **avec l'équipement** (`simGear` dans
  test.cpp ; chiffres dans `.claude/skills/equilibrage`).
- Mode auto (Tab) : un joueur a vu le menu s'ouvrir quand même pour Lior
  (combats d'histoire et boss). Pas reproduit, ni par les tests ni avec une
  version qui l'enregistrait : si ça revient, noter le combat exact.
- Fait en 1.5 : marcher dans une porte fait entrer dans la maison (avant,
  seulement Entrée).

- Petits plus du retour des joueurs.

## Étape 2 — Plus de contenu (fait en 1.6, le 2026-10-10)

Fait : quêtes de Sylve-Noire (grimoire de l'ermite, cabane et tente en
intérieurs) et du Temple gelé (gardien de cristal), région facultative
après le Givrecorne (Source des Brumes, 3 créatures, Mère des Brumes),
bestiaire. Idées pour la suite :

- Des quêtes dans la vallée et à Forgeroc qui utilisent le bestiaire
  (« montrez-moi un Nébulis »), ou une récompense quand il est complet.
- Nouvelles créatures pour les nouvelles zones (formes existantes, nouvelles
  couleurs : rien à dessiner).

## Étape 2 bis — Refonte des combats (demandée le 2026-10-10)

« Les combats ne sont pas vraiment stratégiques » : plan validé avec
l'utilisateur, en deux versions.

- Fait (2026-10-10) : **1.7** et **1.8** ci-dessous, plus les retours du
  premier essai (compagnons commandés, menus en bas, ATH discret, fenêtre des
  gains, recrue à la suite, moins d'expérience, boss +10 % de PV) et les
  **cartes agrandies** (vallée, Cendrelune, Sylve-Noire, Pics) avec des meutes
  selon la profondeur.
- **1.7** : damier de 9 cases par camp, corps à corps limité à
  l'avant, arrière protégé, zones d'effet des techniques (ligne, colonne,
  croix, tout), « Se déplacer » (action rapide), menu Formation, jusqu'à 6
  ennemis, boss remesurés.
- **1.8** : IA qui place et déplace ses combattants, vise les zones les plus
  rentables, protège ses lanceurs ; attaques des boss annoncées un tour à
  l'avance (on peut s'écarter de la zone) ; tactiques liées au damier
  (« ennemis groupés », « allié à l'avant blessé »…).
- **1.9** (demandée le 2026-10-12 après l'essai de la 1.8) : vitesse du jeu
  ×1/×2/×3 (Options, F2), écran **Aide** (F1 : statistiques, dégâts, damier,
  zones, rythme, états, tableau des types, touches), descriptions complètes
  des techniques (cible, zone, portée, coût, puissance, effets, rythme) et
  des objets (effets chiffrés), faiblesses dans le Bestiaire, et **plus
  d'attaques de zone** (10 nouvelles techniques pour les héros et les
  créatures, 4 techniques de créatures qui gagnent une zone). Boss et meutes
  inchangés (« ça va pour le moment »).
- **2.0** (demandée le 2026-10-12) : damier **4 × 4** (16 cases par camp ; les
  anciennes formations sont converties) et graphismes des cartes : maisons
  (tuiles, cheminée, volets, jardinières, soubassement, ombre), arbres en trois
  variantes avec du volume (fruitiers dans la vallée), chemins aux bords
  adoucis, herbe plus variée.
- Ensuite : le joueur automatique mis à jour à chaque étape ; étape 3.
- **Stratégie 2.5 → 2.7** (demandée le 2026-10-14 : « complexifie toute la
  stratégie en t'inspirant d'autres jeux, améliore les graphismes, rends le jeu
  plus dur mais que la stratégie soit récompensée ») :
  - **2.5** : Brisure (boucliers et faiblesses à découvrir, Octopath), frise de
    l'ordre des tours (FFX), talents par espèce (Pokémon), effets visuels par
    type, écran qui tremble, entrée en combat ; boss plus durs.
  - **2.6** : pousser / tirer sur le damier (Into the Breach), terrain et météo
    du combat, techniques combinées entre deux alliés (Chrono Trigger) ; jour et
    nuit sur les cartes, fumée, feuilles, papillons.
  - **2.7** : gemmes à sertir (matéria de FF7), rôles des ennemis et phases des
    boss ; portraits dans les dialogues, écran titre animé.
  - **2.8 → 3.0** (validé le 2026-10-14, sans niveaux de difficulté) : 2.8 astuces en situation, conseils avant les
    boss, menus ; 2.9 Expédition à jour (talents, rôles, phases, gemmes) ; 3.0 deuxième acte (archipel d'Écume,
    énigmes de rochers, Kraken, Griffon, Seigneur des Orages). Ensuite : page itch.io et traduction anglaise.
  - À chaque version : graphismes (cartes vivantes, jour et nuit, portraits,
    icônes de type, titre animé), le joueur automatique apprend la mécanique,
    équilibrage mesuré, une page d'Aide par règle.

## Étape 2 ter — Refonte graphique (3.2, demandée le 2026-10-15, publiée le 2026-10-15)

« Refonte complète des graphismes » à partir des planches de sprites de l'utilisateur, plus un ATH plus clair, des
descriptions plus courtes mais précises, la zone d'effet visible en survolant une attaque, la case et la cible
choisies directement sur le damier, et des villes plus réalistes.

- Fait : outil de découpe des planches (`outils/sprites/`), atlas lu par le jeu ; Lior, héros, habitants, animaux et
  les 62 créatures de l'histoire en sprites (boss en grand) ; arbres, rochers, objets et bâtiments triés avec les
  personnages ; effets des sorts, invocations des Limites, icônes d'objets, visages de Lior, textures.
- Fait : combat avec ATH en tableau, descriptions brèves, aperçu de la zone, cible et case choisies sur le damier.
- Fait : Vallée (place, marché, jardins, ferme au moulin), Forgeroc, Givreval, port et camp de Sylve-Noire
  redessinés ; nouvelles créatures d'après les monstres restants (Tisseline, Osselet, Gobelard, Brutor, Draconnet,
  Coffrelin).
- Publiée en 3.2 (2026-10-15). Ensuite : les retours de l'utilisateur après son essai.

## Étape 2 quater — Carte blanche (demandée le 2026-10-16, l'utilisateur absent 2 à 3 jours)

Retours sur la 3.2 : sprites trop flous, l'équipe doit être à gauche en combat et chacun tourné vers le camp adverse,
des textes cachent encore le terrain. Puis carte blanche pour améliorer le jeu à tous les niveaux, sans questions, et
sans fusionner les branches (rien n'est publié avant son retour). Plan, dans l'ordre :

**Publié en version 3.3 le 2026-10-18** (branche `ameliorations`, qui contient `combat-hd`).

**Branche `combat-hd` (3.2.1 : ses demandes) — fait le 2026-10-16** (revue Codex : 3 remarques justes corrigées ;
joueur automatique : histoire terminée, 0 à 1 défaite sur deux parties)
1. Sprites en double définition : l'image du jeu est rendue en 640×480 (les dessins en code restent identiques,
   agrandis deux fois), les sprites sont découpés deux fois plus fins et dessinés à leur pleine finesse.
2. Combat : l'équipe à gauche, les ennemis à droite, chacun tourné vers l'adversaire.
3. Combat : plus aucun texte sur le terrain : une bande compacte en haut (aide, ordre des tours, météo), menus et ATH
   dans un panneau en bas.
4. Revue par Codex, mode test, joueur automatique, captures.

**Branche `ameliorations` (à partir de `combat-hd`)** — fait le 2026-10-16 : 5 (détails du sol, hautes herbes,
éboulis), 6 (bibliothèques, tables, chaises), 7 (courir ; promeneurs pas encore), 8, 9 (lancer, victoire, K.O.),
11 (Coffrelin N.54, bouclier 6), 12 (tests du choix sur le damier et de la course). En plus : décors de combat en
sprites, ennemis vaincus qui s'effacent, dégâts prévus sur la cible (« ≈ 8-10 PV · 92 % »), portraits dans l'ordre des
tours, habitants qui regardent ailleurs, écran titre. Correction aussi sur `combat-hd` : l'éditeur de cartes ne
dessinait plus les arbres et objets hauts depuis la 3.2. Revues Codex : 3 + 2 remarques justes, corrigées.
Compilation Windows / Mac / Linux vérifiée par une demande de fusion en brouillon (#28, à ne pas fusionner).
Ensuite : compagnon qui suit le joueur, objets hauts transparents devant le joueur, murs de pierre (grotte, glace,
falaises), fontaine ronde de la Vallée (départ déplacé en 12, 10), carte du monde (Échap > Carte). Page de bilan pour
le retour de l'utilisateur : https://claude.ai/artifact/5xTAs74tkfYvtVvfycHCye
Puis : fondu au changement de carte, réverbères allumés la nuit, Bestiaire (petites créatures en grand), mode test sans
aucun échec en anglais aussi (il en avait 32 : les tests comparaient des libellés français), test de l'attaque annoncée
fiabilisé (il échouait parfois au hasard). Puis : promeneurs (enfants et animaux des villes marchent autour de leur
place, champ `promenade`), et chaque habitant se tourne vers le joueur qui lui parle. Fiche d'équipe : la créature à la
taille de la place (`creatureFit`), une Limite au nom long passe à la ligne (elle dépassait de la fenêtre). Eau plus belle (bords peu profonds,
écume en tirets, vaguelettes, reflets). Neige plus vivante (rides de vent, congères, brins secs), chemins de neige
tassée, dalles de glace au Temple. Intérieurs : fenêtres, lumière au sol, ombre des murs. Combat : météo et
renforts sur une étiquette lisible, étiquette d'état de l'allié du bord gauche dans l'écran. Quêtes : bulle « ! »
au-dessus de qui en propose une, « ? » dorée quand on peut la rendre. Aide : page « Exploration » (courir, carte du
monde, bulles, Journal). Empreintes de pas dans la neige, qui s'effacent.
Point ouvert : un plantage du mode test sous Windows, une seule fois sur GitHub (16 octobre, juste après le test
« terrain et météo »), jamais reproduit ici (5 passages complets réussis) : à surveiller dans les prochaines
compilations.
Retours de l'utilisateur (2026-10-18) : Braisenard, Fournézard, Piafouine, Voltigeon et Ferrolin étaient rangés à
l'envers dans l'atlas (ils regardaient du mauvais côté en combat) : `miroir` corrigé ; le compagnon trottine (petits
bonds) ; un habitant sursaute quand on lui parle. Les habitants des planches qui n'ont qu'une pose de face (enfant,
femme, homme, aubergiste, paysan, pêcheur, forgeron, soigneur) ne peuvent pas se tourner : il faudrait leurs poses de
profil et de dos. Police : guillemets anglais “ ” ‘ (47 phrases anglaises les affichaient en « ? »).
5. Cartes plus vivantes : touffes d'herbe, fleurs et cailloux en sprites, eau et falaises plus belles.
6. Intérieurs meublés avec les sprites (étagères, tables, chaises, tonneaux, lits).
7. Personnages : courir (touche maintenue, sprites de course de Lior), animaux et promeneurs qui bougent.
8. Expédition : créatures et héros générés dessinés avec les sprites (selon leur forme, recoloriés).
9. Combat : animations (lancer de Lior à la capture, éclat au coup reçu, K.O.).
10. Menus : fiche des créatures avec le grand sprite, écran titre avec les sprites.
11. Équilibrage des nouveautés (Coffrelin, nouvelles créatures), mesuré.
12. Tests du choix sur le damier et des nouvelles tuiles ; documentation et traduction anglaise à jour.

## Étape 2 quinquies — Échiquier, Blender et Godot (demandé le 2026-10-18)

Demande : « améliorer encore le jeu à tous les niveaux ; que le combat marche comme un échiquier, que les combattants
puissent passer du côté adverse et que les déplacements deviennent importants pour tous ; apprendre à utiliser
Blender et Godot pour créer des sprites et des animations ». Ordre laissé à Claude (« dans l'ordre le plus rentable »).
Branche `echiquier` (version 3.4), pas encore publiée.

1. **Échiquier** (fait) : un seul plateau de 8 × 4 cases pour les deux camps ; « Se déplacer » (2 cases, une fois par
   tour) ne finit plus le tour ; « Attendre » ; corps à corps au contact (on s'approche d'abord si on peut) ; coup
   dans le dos ×1,3 ; encerclé +10 % par adversaire en plus ; distance au contact ×0,75 ; Percée au bord adverse
   (+1 Attaque, Magie, Vitesse) ; poussée dans l'axe du lanceur ; Vent et Esprit volent, Vent et Foudre vont plus
   loin. IA (ennemis, alliés en auto, joueur automatique) qui choisit sa case avant d'agir. Duel en ligne en miroir.
   Aide, astuces (échiquier, dans le dos, Percée), traduction anglaise, tests.
2. **Équilibrage** (fait, mesuré) : les boss étaient trop faciles (encerclés par toute l'équipe) ; ils ne sont plus
   encerclés ni gênés au contact, encerclement 15 → 10 %, trois boss +1 niveau. Joueur automatique : histoire
   finie (actes 1 et 2, 1 h 32, 1 défaite).
3. **Blender** (fait) : personnages chibi en 3D rendus en pixel art ; les 8 habitants qui n'avaient qu'une pose de
   face (homme, enfant, femme, pêcheur, barman, docteur, forgeron, paysan) ont maintenant dos et profil, en 4 images
   de marche : ils se tournent et marchent. Jugés trop différents du reste du jeu (19 octobre au soir) : refaits
   d'après leur dessin de face (`outils/sprites/tourner.py`), même style que les planches.
4. **Godot** (fait) : effets en particules rendus image par image : types Métal et Normal, éclats de la Percée et de
   la Brisure.
5. Pistes suivantes : poses de combat rendues avec Blender (attaque, garde) pour les héros qui n'en ont pas ; plus
   d'effets Godot si l'utilisateur veut remplacer ou compléter ceux de sa planche ; obstacles sur l'échiquier
   (rochers, arbres) ; techniques qui font bondir ou échanger de case.

## Étape 2 sexies — Carte blanche du 19 octobre (7 h → 19 h, l'utilisateur absent)

« Fais la même chose que la dernière fois que je suis parti » : améliorer le jeu à tous les niveaux, sans questions,
tout permis sauf fusionner. Suite de la branche `echiquier` (3.4). Plan, dans l'ordre :

1. Compilation Windows / Mac / Linux vérifiée par une demande de fusion en brouillon (à ne pas fusionner).
2. Échiquier : obstacles sur le plateau selon le décor (rochers, souches, cristaux : on ne peut ni s'y arrêter ni les
   traverser, sauf en volant).
3. Échiquier : en mode auto (tactiques), les alliés choisissent aussi leur case avant d'agir, comme l'ordinateur.
4. Échiquier : en choisissant sa case, les cases où un ennemi pourra frapper au prochain tour sont marquées (danger).
5. Techniques de mouvement : des bonds (frapper puis sauter derrière la cible) et des échanges de place, pour des
   créatures agiles et des héros ; l'IA et l'aide les connaissent.
6. Graphismes : obstacles et nouvelles poses ; petits défauts vus sur les captures (étiquette d'état sur un nombre).
7. Équilibrage mesuré, joueur automatique, tests en français et en anglais, revue Codex, documentation.
8. Page de bilan pour le retour de l'utilisateur.

Avancement : 1 impossible (envoi sur GitHub refusé par les permissions de Claude Code : tout est resté en local, compilé
et testé sous Windows seulement) ; 2 fait (obstacles) ; 3 fait (placement auto en mode tactiques) ; 4 fait (losanges
rouges, aide « Danger ») ; 5 fait (bond : Dague, Coup de bec, Piqué tonnerre, Lame d'ombre ; échange : Coup bas ;
le duo « Percée » renommé « Assaut perçant ») ; 6 : effets Godot de poussière (bond) et d'échange.
Revue Codex : 3 remarques justes corrigées (colonne bouchée par les obstacles, terrain à l'arrivée d'un échange,
menace en diagonale). Mesuré (BRUMEVAL_SIMULATIONS=2) : difficulté dans le bruit de la 3.3 ; joueur automatique :
histoire finie (1 h 26, 3 défaites).

Deuxième série (7 h 35) :
9. Portée visible : quand un allié choisit son action, les cases où il peut aller ce tour s'allument doucement.
10. Sons : un pas quand on change de case, un souffle pour le bond, un son pour l'échange.
11. Tactiques de l'échiquier : actions « S'approcher », « Protéger un allié » (se placer devant le plus blessé) et
    « Contourner » (passer dans le dos de la cible), dans l'éditeur de tactiques et pour le joueur automatique.
12. Formation : choisir la case de chacun sur un dessin du plateau (flèches et souris), comme en combat.
Avancement : 9 à 12 faits (portée en bleu ; sons « pas », « bond », « echange » ; tactiques S'approcher, Contourner,
Protéger ; formation choisie sur le dessin, flèches dans les quatre directions et souris).

Troisième série (7 h 50) :
13. En choisissant sa case, le chemin s'affiche (pointillés le long des pas).
14. « Revenir » : annuler son déplacement tant qu'on n'a pas agi (pas en ligne).
15. Animaux des villes (chien, chat, poule, vache, cheval) : marche animée, rendue avec Blender.
16. Mesures, joueur automatique, revue, bilan.
Avancement : 13 et 14 faits (chemin en pointillés dorés ; « Revenir », pas en ligne ni après une Percée).
15 abandonné : des animaux rendus en 3D seraient bien moins beaux que leurs sprites dessinés (très détaillés) ; à la
place, ils trottinent en marchant (petit bond à chaque pas, dessin seul).
En plus : bottes qui font avancer d'une case de plus (Bottes de vent à Forgeroc, Bottes ailées à Givreval) ; défis
de l'échiquier dans l'Arène (data/defis.json : Dans le dos, La Percée, Les ailes, Le mur, Bousculade) ; maître d'armes dans la
Vallée (premier défi en entraînement, Bottes de vent) ; cercle d'invocation des Limites (Godot) ; fiches : « Pas ».
Point ouvert du 16 octobre résolu : le plantage rare du mode test venait du test de l'énigme, qui appelait l'action de
« Recommencer l'énigme » alors qu'elle détruit son propre menu (relu ensuite en mémoire libérée) ; trouvé avec cdb.
Aussi corrigé : le joueur automatique lisait la liste « ennemis » d'un combat sans vérifier qu'elle existe, et
remplaçait l'équipe par des copies quand un combat n'avait pas d'ennemis.
Après-midi : IA qui ne s'attend plus sans fin (test de non-blocage), cinquième défi « Les ailes » ; relecture Codex :
3 remarques justes corrigées (un défi est toujours un entraînement, pas d'objets du sac, PV et PM remis). Mesuré
(BRUMEVAL_SIMULATIONS=2) : difficulté des boss dans le bruit de la 3.3 ; joueur automatique : histoire finie (1 h 32,
2 défaites), bottes du maître d'armes gagnées une fois.

Cinquième série (soir du 19, « continue comme ça ») :
17. Ménage : le code de l'échiquier sort de battle.cpp (plus de 3 400 lignes) dans src/echiquier.cpp, sans rien
    changer au jeu ; relecture Codex.
18. Défis dans l'histoire : un instructeur à Forgeroc (« Bousculade ») et un à Givreval (« Le mur »), en entraînement,
    avec une récompense la première fois.
19. Aide : une page « Nouveautés 3.4 » pour les joueurs qui reprennent une partie.
20. Vérifications, mesures, page de bilan.
Avancement : 17 fait (src/echiquier.cpp, 391 lignes déplacées à l'identique, vérifié ligne à ligne) ; 18 fait
(instructeurs à Forgeroc et Givreval ; le joueur automatique les fait une fois chacun) ; 19 fait (page « Nouveautés
3.4 » en tête de l'Aide). En plus : dans les dialogues, une 4e ligne longue ne passe plus sous la flèche « ▼ ».
Joueur automatique : 5 puis 6 défaites (meutes du début) avec le réglage « s'approcher deux fois plus vite sans
cible » de l'après-midi ; ce réglage retiré (le compteur de 40 s sans dégâts suffit contre les blocages, test de
non-blocage toujours vert) : deux parties finies avec 0 et 1 défaite (1 h 20 et 1 h 34).
Relecture Codex de la série : 2 remarques justes corrigées (Protéger avec une condition « Soi », « Revenir » gardé
après un remplacement). Page de bilan : https://claude.ai/artifact/KbjqopS4iS7eXWjgjLL5my

## Étape 2 septies — 20 heures de jeu (demandé le 2026-10-19 au soir)

« Améliore tout sans exception » : au moins **20 heures** de durée de vie, meilleurs graphismes, animations, sprites,
gameplay et musiques ; liberté totale d'inspiration ; étudier d'autres jeux du genre ; travailler sans arrêt, sans
demander d'avis. Petits détails d'abord : allié K.O. visible (fait : couché, grisé, étoiles et « K.O. ») ; habitants
de dos et de profil trop différents du reste du jeu (fait : tirés de leur dessin de face, `outils/sprites/tourner.py`).

Ce qu'apprennent les jeux du genre (recherches du 19 au soir) :
- Cassette Beasts (capture de créatures, indépendant) : ~20 h pour l'histoire, 27 h avec les à-côtés ; son
  après-histoire (quêtes, boss, tableau de quêtes tirées au hasard) sert aussi à compléter la collection.
- Sea of Stars : beaucoup de joueurs sautent les à-côtés, la récompense doit donc compter (objet unique, coquillages
  à collectionner, chaque quête allume un pilier utile pour la fin).
- Chained Echoes : quêtes annexes ouvertes au début de chaque acte ; une quête qui donne un équipier vaut le détour.
- Dragon Fantasy, Ni no Kuni : des **primes** (monstres recherchés, mini-boss) ; les dernières, après la fin, mènent
  à un super-boss. Super Mario RPG : trésors cachés à collectionner, super-boss (Culex).
- Pixel art (Slynyrd) : 4 images de marche par direction ; le dos montre ce qui reconnaît le personnage (cheveux,
  sac) ; un pied posé, le passage, l'autre pied.

Durée actuelle : le joueur automatique finit l'histoire en ~1 h 30 de jeu (il ne lit rien, ne se perd jamais, ne fait
que l'utile) ; un joueur, à peu près 4 à 6 h avec les à-côtés. Pour 20 h, il faut environ quatre fois plus de contenu
**qui vaut la peine**, pas du remplissage. Plan, dans l'ordre (chaque morceau testé, joueur automatique, bilan) :

1. **Primes** : tableau d'annonces dans chaque village ; monstres recherchés (variantes plus fortes, talents, phases)
   placés sur les cartes déjà visitées ; on accepte, on chasse, on touche la prime ; rang de chasseur et récompenses
   uniques ; les dernières après la fin.
2. **Quêtes annexes** : de 6 à 20 au moins, dans les villages et les intérieurs, avec des personnages qui reviennent ;
   certaines donnent un équipier, une évolution rare, une technique.
3. **Collection** : éclats de brume cachés sur toutes les cartes (coins, énigmes), échangés contre des objets uniques ;
   Bestiaire complété = récompense.
4. **Acte 3** : nouvelle région (îles ou terres d'au-delà de la tempête), nouveaux villages, créatures, boss, et une
   vraie fin ; histoire des héros approfondie (passé de Lior, de Maëlle, d'Isra).
5. **Après la fin** : donjon des Abysses de brume (étages, de plus en plus durs), super-boss, primes d'élite, ligue de
   l'Arène.
6. **Musiques** : morceaux plus riches (mélodie, contre-chant, basse, percussions ; un thème par région), étudiés sur
   les musiques du genre.
7. **Graphismes et animations** : poses d'attaque des héros, effets, décors des nouvelles régions, en gardant le style
   des planches de l'utilisateur.
8. Équilibrage de la progression sur la nouvelle longueur (niveaux, prix, butin), mesures, documentation.

Branches : `echiquier` (3.4) puis la suite sur la même branche ; jamais de fusion ni de publication sans demande.

Avancement (nuit du 19 au 20) : 1 fait (15 primes, tableaux dans 5 villages, rangs et récompenses uniques, 3 super-boss
après la fin) ; 2 fait (20 quêtes : 14 nouvelles, dont un rival en trois combats, une quête qui donne un Draconnet,
un naturaliste qui demande de montrer des créatures, un conte qui explique la brume après la fin) ; 3 fait (30 éclats
de brume dans des coffres reculés, collectionneuse de la Vallée : récompenses à 5, 10, 20, 30) ; 6 fait (13 morceaux
réécrits ou nouveaux avec `outils/musique/compose.py` : vibrato, écho, contre-chants ; port, abysses, orage, dernier
combat). Le rapport du joueur automatique donne maintenant une estimation pour un vrai joueur (très approximative).
Joueur automatique : 2 h 07 (au lieu de 1 h 30), 19 quêtes sur 20 (le naturaliste demande des captures précises),
toutes les primes, 30 éclats.
4 fait (20 octobre) : l'île d'Aube après le Seigneur des Orages : Havre-d'Aube, Bois des Lueurs, Marais Pâle, Gouffre
d'Aube (énigme), Citadelle des gardiens ; 8 créatures et 3 boss d'Aube (recolorés), Ilya, le Premier Gardien et une
vraie fin (le passé des gardiens et de la brume). Scripts `outils/villes/acte3.py` et `acte3_histoire.py`. Joueur
automatique : tout fini en 2 h 14 ; estimation pour un vrai joueur : environ 5 h (lecture 1 h 05). Relecture Codex :
3 remarques justes corrigées (drapeaux des déclencheurs d'arrivée, champ « evenement » des boss en double, enlevé des
nouvelles cartes ; écho de la musique alloué une fois hors du fil du son).
Ensuite : 5 en partie (les épreuves des gardiens : huit combats puis Ilya, au sanctuaire d'Aube) ; la pêche (18 poissons,
carnet et maître pêcheur au port). Joueur automatique : 3 h 34 avec les épreuves (Ilya demande trois essais) ;
estimation pour un vrai joueur : 7 h 30, sans compter la pêche.
Puis quatre donjons optionnels, un par moment de l'histoire : Mines de Forgeroc, Glacier de Givreval, Épave du
Léviathan, et l'Abîme (trois étages, super-boss) après l'acte 3.
Réglage : l'acte 3 était trop dur pour certaines équipes (meutes de 4 à 6 au Gouffre et à la Citadelle, Brume Noire
battue au 7e essai) : meutes de 3 au plus, Gouffre N.58-61, Citadelle N.60-64, Brume Noire PV ×6,5 (au lieu de 8).
Joueur automatique : tout fini (Orage ancien compris) en 6 h 01, 14 défaites ; estimation pour un vrai joueur : 12 h,
sans compter la pêche ni l'Expédition. Combats : nuages et montagnes au fond de la vallée, stalactites dans les grottes.
Le chemin de l'histoire s'allonge : le Col des Hurlevents (camp de bergers, Harpie de givre) entre les Monts
Cendrelune et les Pics Givrés. Le grand bestiaire : la professeure récompense 15, 30, 45 et 60 espèces.
Acte 2 plus long : l'île de Corail et la Grotte des Marées (le Trident d'écume ouvre les Ruines englouties).
Mesures : d'une partie à l'autre, le joueur automatique met de 4 h 50 à 6 h (surtout le temps passé à s'entraîner
avant l'Orage ancien) ; estimation pour un vrai joueur de 9 h 40 à 12 h. Musique propre à l'île d'Aube.
Le Val des Murmures (trois esprits à apaiser) entre le sanctuaire et la Source des Brumes. Effets Plante et Esprit
refaits avec Godot.
Dernière mesure (Val compris) : joueur automatique 5 h 21, 11 défaites ; estimation pour un vrai joueur 10 h 36.
Nouvelle partie + : après l'acte 3, recommencer l'histoire avec son équipe, ennemis +12 niveaux par tour.
Mesure : joueur automatique 6 h 15 (il refuse la Nouvelle partie +) ; estimation pour un vrai joueur 12 h 20, sans la
pêche, le grand bestiaire, l'Expédition ni la Nouvelle partie +.
Cinq quêtes de plus dans les nouveaux lieux (`outils/villes/quetes_lieux.py`) : le chien de la bergère (Col), le
collier de corail (Corail), le dernier message du pêcheur (Val → Port), le tournoi de Forgeroc (trois manches), les
créatures d'Aube (montrer trois créatures). 33 quêtes en tout.
Sept formes régionales de plus (`outils/villes/formes.py` : Bourrasquin, Givroc, Lanternelle, Fongombre, Corailleur,
Perlin, Murénix) dans le Col, le Val, la Grotte des Marées et les herbes de l'île de Corail. Animations : papillons et
poissons sur la carte, silhouettes d'élan en combat.
Discussions entre les héros (38 saynètes, Échap > Discussions), au fil de l'histoire.
La Plaine des Moulins (`outils/villes/plaine.py`) entre la Vallée et les Monts Cendrelune : village de Moulinbourg,
pont rongé par une bande de Rongecrins (quête `pont`).
28 dresseurs le long des routes (`outils/villes/dresseurs.py`), de la Vallée à la Citadelle d'Aube, et des revanches
(`outils/villes/revanches.py`) après le Seigneur des Orages et après l'acte 3. La Ligue d'Écume au port
(`outils/villes/ligue.py`) : quatre rangs de trois combats d'affilée, du début de l'acte 2 à l'après-fin.
Acte 4 (`outils/villes/acte4.py`) : l'île des Échos après la Brume Noire (Port-Écho, Forêt Pétrifiée, Cratère,
Sanctuaire), 6 créatures et 3 boss, l'Ombre du Premier Gardien ; deux primes et deux quêtes de plus sur l'île
(`outils/villes/echos_annexes.py`). Créatures chromatiques (couleurs rares, environ une rencontre sur 170).
Cinq quêtes de plus pour les actes 2 et 3 (`outils/villes/quetes_actes.py`) : le phare d'Aube (trois éclats de lueur),
l'enfant du marais, le spectre du Gouffre, les pages de l'orage (Ruines, Tour, Grotte des Marées), Coquillon et Perlin.
L'éleveuse de Forgeroc (`outils/villes/eleveuse.py`) : huit créatures évoluées à lui montrer, une par une.
Créatures de la nuit (`outils/villes/nuit.py`), écran titre en paysage de nuit, rafales dans les hautes herbes.
Faits depuis la liste de pistes : acte 4, Ligue, pêche, captures demandées par des quêtes, alphas des meutes (combats
plus riches), le Vieux Phare (`outils/villes/phare.py` : cinq étages après l'acte 4). Restent : d'autres animations.
Mesure (20 octobre, tout compris : 4 actes, Ligue, revanches, primes, 5 donjons) : joueur automatique 4 h 56 à 6 h 38
selon les parties, 11 à 33 défaites ; estimation pour un vrai joueur 11 h à 13 h 40, sans la pêche, les captures
demandées (éleveuse, chromatiques, bestiaire), l'Expédition ni la Nouvelle partie +.
Ensuite : arbres des lieux (pierre, lueur, pâle, brume ; aussi au fond des combats), coffres animés, bulles d'humeur,
étincelles de niveau, Journal > Progression, quiz du conteur, 17 intérieurs pour les nouveaux villages, quête des
couleurs rares (créatures chromatiques), musiques de la Plaine et du Vieux Phare, une description pour chaque
créature du Bestiaire ; Phénix et Ombre réglés (plus d'attaque sur toute l'équipe à chaque tour).
Puis : pluie sur la vallée et les forêts (météo des combats), alphas des meutes, sauvegarde automatique, voyage
rapide (Échap > Carte), et 28 exploits (Journal > Exploits : fin des actes, Bestiaire, pêche, quêtes, Ligue…).
Quêtes des héros (`outils/villes/liens.py`) : après l'acte 3, une par héros (quelqu'un de son passé, un boss, une arme
ou une armure qu'il est seul à pouvoir porter, champ `pour`), suivies chacune d'une discussion (44 en tout).
Cartes au trésor (`outils/villes/tresors.py`) : huit trésors cachés de la Vallée au Récif (coffres « si »), chacun
contient la carte du suivant ; Corbin, au port.
Cartes des gardiens (`src/cartes.cpp`, `outils/villes/cartes.py`) : un jeu de cartes à la Triple Triad, neuf joueurs
de la Vallée à Port-Écho, puis le Maître des cartes. Échos des légendes (`outils/villes/legendes.py`) : après l'acte 4,
cinq cercles de trois grands boss d'affilée (N.98 à 120). Pension de Moulinbourg (`src/pension.cpp`) : élevage et œufs ; fête des lanternes de la Vallée (la nuit) ;
32 exploits.

## Étape 2 octies — Graphismes sans limite (demandé le 2026-10-21)

« Améliore les graphismes sans limite », mêmes règles de travail (sans arrêt, captures vérifiées). Fait, dans l'ordre :
relief des murs des grottes et des lieux de glace (`wallDepth`), roche propre à chaque donjon (`teinte_murs`), lanterne
des lieux obscurs en fondu, cristaux qui brillent ; sol des combats en perspective, brume d'horizon, vignette ; carte du
monde illustrée ; sol volcanique et lave qui rougeoit ; nom des lieux en ruban, titre doré (`textBigFancy`) ; fenêtres
(ombre, coins arrondis) ; nombres des combats cernés ; fontaine, mouettes, Expédition étoilée ; éclair d'orage,
éclaboussures, flocons ; combats au bord de la mer (`Game::coastal`) et aux couleurs du donjon ; flaques sous la pluie ;
neige ondulée ; ponts ; rochers, plaques, herses et escaliers des énigmes ; cristaux à facettes ; coffres ouverts ;
traînée de l'élan ; intérieurs selon le bâtiment (`style`), fenêtres sombres et bougies la nuit, halo autour des pièces ;
panneau des combats et jauges. Captures du mode test : `donjon_<carte>`, `carte_monde_fin`, `combat_port`,
`combat_mines`, `meteo_orage`, `interieur_nuit`. Pistes : poses d'attaque des héros, effets de sorts, décors des cartes.
Après la 3.5 (branche `graphismes-2`) : Marais Pâle (`eau`), lumière des lieux (`lumiere`), nom des boss et « Victoire ! » en
grand, onglet du nom dans les dialogues, écran de fin à l'aube, icônes et jauges dans les menus.
Après la 3.7 (2026-10-26, « que les coups se sentent ») : arrêt sur image de 50 à 90 ms sur un critique, une faiblesse, une
Brisure ou un coup qui abat (dessin seul : ni l'ATB ni le Script ni le réseau n'attendent), étirement du lanceur à l'élan,
cible écrasée et repoussée selon la force du coup, éclair blanc d'une image (critique, Brisure), nombres qui rebondissent
(critique doré en grand, faiblesse orange, résistance gris-bleu). Captures `coup_critique`, `coup_brisure`.

## Étape 2 nonies — Gameplay et menus (demandé le 2026-10-23)

« Continue en priorisant le gameplay et le game design » ; deux demandes : les attaques dans le dos selon la direction où
regarde le personnage, et des menus « peut-être un peu simples et surchargés ». Fait :
- Regard des combattants (`Battle::facing_`) : vers où il marche, vers sa cible, vers l'adversaire le plus proche s'il attend ;
  « dans le dos » = du côté opposé à son regard ; flèche au sol ; dague dorée sur les cases d'où l'on frappe un dos
  (« Se déplacer ») ; IA, tactique « Contourner », aperçu des dégâts, réseau. Relu par Codex. Difficulté inchangée.
- Menu de pause en 12 entrées et quatre groupes ; Échap > Préparation (Formation, Compagnons, Tactiques, Réserve) ;
  Discussions et Bestiaire dans le Journal ; menu de commande du combat plus court (Limite prête, Duo possible,
  Changer avec un remplaçant) ; volumes en crans ; « Gains ».
Pistes : l'ordinateur qui protège son dos (« Attendre » pour se retourner), d'autres menus à regrouper.

## Étape 2 decies — Rééquilibrage (demandé le 2026-10-23)

« Le jeu est trop facile pour le joueur auto ; les ennemis sont parfois trop peu nombreux pour la puissance de l'équipe
et un peu faibles ; équilibrer les faiblesses pour obliger à adapter son équipe et sa formation (comparer avec
Expedition 33). » Constat : une rencontre faisait ~2 ennemis contre jusqu'à 6 combattants alliés ; le joueur automatique
finissait au niveau 119 pour des boss de fin au niveau 100-110. Fait :
1. Rencontres à la mesure de l'équipe (`par_allie` 0,6 ennemi par allié ; `ecart_niveau` 4 : une créature sauvage au plus
   4 niveaux sous l'équipe, `plafond_niveau` 6 au-dessus du haut de la zone), gains de son niveau d'origine (`rewardLvl`).
2. Résistances des créatures sauvages (`outils/villes/resistances.py`) : carapaces (physique ×0,6), esprits (physique
   ×0,55), peaux d'enchantement (magie ×0,6), élémentaires qui absorbent leur élément (résistance négative : le coup soigne).
3. Formation : la moitié des créatures sauvages ont déjà des attaques en ligne, colonne ou croix ; plus d'ennemis suffit
   à punir une équipe groupée.
4. Expérience : un adversaire plus de 4 niveaux sous l'équipe rapporte 8 % de moins par niveau (au moins 25 %).
Mesuré (joueur automatique) : 7 h 38, 21 défaites, niveau 102, boss affrontés de −5 à +6 niveaux (avant : +15 à +20) ;
estimation pour un joueur : 16 h. Piste : le saut de niveaux entre l'acte 3 (60-64) et l'acte 4 (84-90).
5. Embuscades (une rencontre sur quatorze, un tiers des créatures au fond du camp allié), profils de tactiques, le
   joueur automatique répète toujours un boss. Partie automatique finale, en cours à la publication : 7 h 18 de jeu,
   10 défaites, niveau 90, trois actes finis (Seigneur des Orages à 4 h 04, île d'Aube à 6 h 11 : plus lent qu'avant, le
   jeu est plus exigeant). Publié dans la 3.6 (2026-10-23). Ensuite (branche tactiques-2) : profils corrigés
   d'après l'analyse d'un agent (le Soigneur lance ses sorts en gardant 40 % de PM, action « Attaque, PM gardés » ; le
   Combattant n'achève qu'un ennemi sous 30 %) : sans mémoire, tout fini en 7 h 40, 27 défaites, niveau 103.

## Étape 2 undecies — Dur mais très tactique (demandé le 2026-10-23 au soir, branche `jeu-tactique`)

« Améliore le jeu : graphisme, gameplay, game design ; je veux un jeu dur mais très tactique. » Principe : la difficulté
doit venir de choix lisibles, pas du hasard. Ce qui rend un combat tactique (Into the Breach, Expedition 33, Octopath) :
voir venir le danger, avoir plusieurs réponses, et payer cher une équipe mal placée. Fait :
1. **Intentions des ennemis** : à la moitié de sa jauge (70 % d'abord : trop tard pour être lues), chaque ennemi annonce ce qu'il va faire (icône au-dessus de lui,
   chevron orange sur l'allié visé, zone en orange au sol, « Prépare Crocs sur Lior » dans l'aide de la cible). Il
   l'exécute à son tour si c'est encore possible, sinon il réfléchit à nouveau : écarter la cible, la couvrir ou abattre
   l'ennemi d'abord sont trois vraies réponses. Les boss aussi (sauf quand leur zone rouge est déjà annoncée).
2. **Garde** : « Attendre » n'est plus un tour perdu : le combattant encaisse un quart de dégâts en moins et riposte à
   chaque coup au corps à corps jusqu'à son prochain tour (bouclier dessiné à côté de sa tête), face à l'ennemi le plus
   proche, et rejoue plus tôt. Un ennemi qui annonce une garde (sablier) se frappe de loin.
3. **Ciblage** : quand un ennemi frappe « au hasard », les soigneurs, les lanceurs de sorts et les blessés l'attirent
   (`Battle::lure`), et un allié déjà visé par un des leurs attire les autres (la meute s'acharne, deux chevrons) :
   couvrir sa ligne arrière compte, un blessé laissé devant est achevé. Ennemi survolé : pointillé vers sa cible.
Astuces « intention » et « garde », aide de l'échiquier, tests (garde ≈ 0,75 ; une intention annoncée vise un allié).
4. **Menace** (`dangerAt`) : l'IA alliée et la tactique « Se mettre à l'abri » s'écartent d'un corps à corps annoncé (hors
   de portée de l'ennemi : il devra choisir autre chose) et de la zone annoncée sur un voisin.
5. Repris d'un agent d'analyse : chaque profil de tactiques réanime et soigne (un seul soigneur faisait perdre les meutes de
   fin de partie), « (conseillé) » dans Profils… ; les créatures sauvages n'ont plus d'attaque sur toute l'équipe
   (`outils/villes/meutes.py`, zone d'un de leurs types à la place : la formation compte) ; simulations des meutes du
   Vieux Phare et de l'Abîme (100 %).
6. Garde qui riposte (chaque coup au corps à corps reçu), meute qui s'acharne sur un allié déjà visé, boss qui annoncent
   aussi leurs coups ordinaires, « Déjoué ! » quand une annonce ne tient plus ; tactiques « Soi : visé » (le Protecteur se
   met en garde quand une attaque est annoncée sur lui) et « Allié : visé » (il va couvrir l'allié visé) ; l'icône d'intention apparaît en se posant ; au-dessus de l'allié
   visé, les dégâts qu'il va prendre (« ≈ 61-68 », additionnés si plusieurs ennemis le visent) : de quoi décider.
   À l'œil (2026-10-25) : le bouclier se déplie et encaisse (arc bleu), la riposte part d'un élan, l'icône d'intention
   pulse quand l'ennemi l'exécute et vole en éclats quand elle est déjouée.
7. Joueur automatique : il adapte ses compagnons aux créatures du lieu (`Pilot::adaptTeam` ; en démo, par les menus Réserve
   et Compagnons) et jauge les zones de fin de partie au milieu de leur fourchette : ses défaites au Vieux Phare (quinze dans
   bot15, deux Champignards et un Flammenard contre des Cendrelys qui absorbent le Feu) étaient celles d'un joueur qui ne
   change jamais d'équipe. Deux relectures Codex (5 remarques corrigées).
8. (2026-10-25) Deux défis d'entraînement pour apprendre la 3.7 par la pratique (Arène > Défis, et le maître d'armes de
   la Vallée après son premier défi) : « Lire les intentions » (un Ferrolin lent annonce Coup d'acier sur Isra : l'écarter
   à temps ; simulé sur 100 combats : IA 96 %, Isra qui ne bouge jamais 20 %) et « La garde » (Brann acculé par quatre
   Mulotins rapides : IA qui frappe 37 %, Brann toujours en garde 79 %). Un défi qui mêle les deux a été essayé puis
   écarté : la garde seule y perd contre le Ferrolin, les mesures ne départageaient rien.
9. (2026-10-25 et 26) **Mécaniques des boss**, toujours annoncées, une parade chacune : Golem de suie et Colosse d'écho se
   cuirassent (la magie), Mère des Brumes et Ombre du Premier Gardien se voilent (le corps à corps), la Brume Noire aussi mais
   seule la Lumière passe, Ronce-Mère, Seigneur des Orages et Phénix d'écho couvrent une zone de ronces, de foudre ou de
   braises (s'en écarter et ne pas y revenir), Ignarok, Griffon tempête et Givrecorne chargent une ligne (changer de ligne),
   le Kraken happe une colonne et tire vers le front (s'en écarter ; un allié devant arrête la traction).
Mesuré (joueur automatique sans mémoire, bot14) : tout fini en 8 h 20, 10 défaites (27 avant), niveau 112, quatre actes en
5 h 40, estimation joueur 16 h 46 ; les défaites restantes : meutes du Vieux Phare. Mesuré ensuite : bot15 (version
finale, mêmes règles) quatre actes en 4 h 40 et 8 défaites dans l'histoire, puis 20 au Vieux Phare (équipe jamais changée :
d'où `adaptTeam`) ; bot16 (0,6 créature par allié, écart 3) quatre actes en 6 h 20, 9 défaites dans l'histoire : réglage
adopté ; bot17 (compagnons adaptés, zones de fin jaugées à mi-fourchette) tout fini en 12 h 23, 16 défaites, niveau 118,
estimation joueur 24 h. Point dur commun à toutes les parties : la Citadelle des gardiens (meutes de 4 esprits et carapaces
au niveau de l'équipe : 4 à 6 défaites par partie) → meutes de 3 au plus, comme l'Abîme et le Phare. bot18 (règles denses,
compagnons adaptés, sans ce plafond) : quatre actes en 6 h 48, 12 défaites dans l'histoire dont 6 à la Citadelle. **État final
(bot19) : tout fini en 9 h 07, 11 défaites (4 dans l'histoire, 0 à la Citadelle), niveau 105, quatre actes en 5 h 28,
estimation joueur 18 h 22.** Essai retiré : capturer aussi les créatures d'un type absent du vivier (bot20 : 14 h 26,
17 défaites, 30 changements de compagnons ; plus lent, pas mieux au Phare). Débuts : une meute fait au plus 2 créatures +
une par tranche de 10 niveaux de l'équipe (plus de défaite sauvage dans les dix premières minutes, bot22). Le pilote
répète un combat dans le décor de son lieu (bot22 : Phénix perdu quatre fois après une répétition sans la canicule du
Cratère). D'une partie à l'autre l'écart est grand (9 h / 11 défaites à 17 h / 29 ; bot23 : 12 h 17, 15 défaites, estimation
joueur 24 h) : juger un réglage sur plusieurs parties. La garde et les intentions voyagent en réseau (invité du duel,
joueurs du gardien). Coquille corrigée : la Vitesse de Braisenard était 200 depuis le 6 octobre (60 avant, les autres
créatures entre 24 et 90) : le compagnon de départ par défaut jouait trois à quatre fois plus souvent ; remis à 60
(Flammenard 66), et les boss, réglés contre ce Braisenard-là, ramenés d'un ou deux niveaux (Sylvarque 13, Golem 26,
Ignarok 24, Ronce-Mère 20, Givrecorne 37, Mère des Brumes 40) pour retrouver les taux d'avant. Mesuré ensuite : quatre
actes en 8 h 29 (anciens boss) et 7 h 43 (boss ramenés), 13 à 14 défaites dans l'histoire, contre 5 h 30 à 7 h avec le
Braisenard trop rapide : le jeu corrigé est bien plus dur ; meutes adoucies au milieu (une créature de plus tous les 15
niveaux au lieu de 10) ; Griffon et Phénix ramenés aussi (les autres boss tardifs tiennent). **Publié dans la 3.7
(2026-10-24, PR #32)** ; dernière mesure : tout fini en 13 h 55, quatre actes en 7 h 27, estimation joueur 27 h.
Mesures : compétence `equilibrage`.

## Étape 3 — Systèmes de jeu (3 à 4 séances)

- Fait en 1.6 : **réserve de créatures** (au-delà de 8 membres) et
  **vente** en boutique (moitié du prix).
- Fait en **2.1** (plan validé le 2026-10-12) : **forge de Forgeroc**, armes
  et armures +1 à +3 avec minerai, cœurs de braise, cristaux de givre et un
  trophée par boss. Idée : un forgeron à Givreval.
- Fait en **2.2** (plan validé le 2026-10-12) : **évolution** des créatures
  (22 lignées, question Oui/Non après le combat, Échap > Évolution pour un
  refus, formes évoluées sauvages en fin de partie). Idées : une seconde
  évolution pour les compagnons de départ, l'évolution dans l'Expédition.

## Étape 4 — Multijoueur plus simple (2 à 3 séances)

- Fait en **2.3** (2026-10-12) : **code de partie** sans serveur, la box est
  ouverte automatiquement (UPnP). Reste, si des amis ont une box sans UPnP ou
  sans adresse à elle : un **serveur relais** (environ 4 € par mois, compte à
  créer par l'utilisateur), à décider.
- Échange : voir les fiches détaillées des deux créatures côte à côte.
- Duel : plusieurs manches, tableau des victoires dans le salon.

## Étape 5 — Présenter le jeu (2 à 3 séances)

- Page **itch.io** (ou une page GitHub plus jolie) avec captures et petite
  vidéo.
- **Anglais** : les textes des données sont déjà dans `data/` ; ceux du code
  (menus) sont à sortir dans un fichier de textes. Gros travail, mais
  mécanique.
- Accessibilité : taille du texte, couleurs des types lisibles par tous. Fait le 2026-10-25 : option « Types lisibles
  sans couleur » (un motif sur chaque pastille de type, pour les daltoniens). Reste : la taille du texte.
- Mac : application signée (compte développeur Apple payant), sinon garder
  la manipulation « Ouvrir quand même ».

## Étape 6 — Technique (au fil de l'eau)

- Fait le 2026-10-10 : `game.cpp` découpé (900 lignes) en `menus.cpp` et
  `save.cpp`.
- Le mode test prenait 43 s, puis 7 minutes avec les simulations d'équilibrage : depuis le 2026-10-24 elles ne se jouent
  qu'avec `BRUMEVAL_SIMULATIONS=1`, et le test repasse à 1 min 50 (FR) ; graine fixe (`BRUMEVAL_GRAINE`) : un échec se
  rejoue. Audit du 2026-10-24 (ponytail) : sain ; restent à faire le découpage de `Game::selfTest()` par domaine et le
  dessin des combattants en un seul bloc (ennemis et alliés), la mémoire du pilote par dossier de partie.
- Relancer `/graphify . --update` et l'audit (`/ponytail-audit`) après chaque
  grosse étape.

## Résumé

| Étape | Contenu | Durée |
|---|---|---|
| 0 | Publier la 1.3, trois décisions | maintenant |
| 1 | Essais réels, équilibrage avec équipement | 1-2 séances |
| 2 | Quêtes, région finale, bestiaire | 3-5 séances |
| 3 | Réserve, vente, forge, (évolution) | 3-4 séances |
| 4 | Multijoueur sans Tailscale | 2-3 séances |
| 5 | Page du jeu, anglais, accessibilité | 2-3 séances |
| 6 | Rangement du code | au fil de l'eau |
