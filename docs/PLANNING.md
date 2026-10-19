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
Prochaines pistes pour approcher 20 h : un acte 4 / une région de plus, une ligue de l'Arène, un donjon d'après la
fin à étages, la pêche, des captures demandées par plus de quêtes, des combats plus riches à mi-parcours.

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
- Accessibilité : taille du texte, couleurs des types lisibles par tous.
- Mac : application signée (compte développeur Apple payant), sinon garder
  la manipulation « Ouvrir quand même ».

## Étape 6 — Technique (au fil de l'eau)

- Fait le 2026-10-10 : `game.cpp` découpé (900 lignes) en `menus.cpp` et
  `save.cpp`.
- Le mode test prend 43 s : c'est correct ; le garder sous la minute.
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
