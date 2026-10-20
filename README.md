# Brumeval — Les gardiens de la vallée

Un petit RPG en C++ et SDL2. Lior, apprenti gardien, part chasser la brume qui
rend les créatures sauvages. Combats au tour par tour actif à la Final Fantasy 7
(jusqu'à trois combattants, jauges ATB, Limites, magie), créatures à capturer
pour agrandir l'équipe, quatre héros recrutables, deux régions et une grotte.

## Télécharger et jouer

Pas besoin de compiler : les versions prêtes à jouer pour **Windows, Mac et
Linux** sont sur la page
[Releases](https://github.com/syphaxxx/brumeval/releases/latest). Téléchargez le
fichier de votre système, décompressez-le et lancez le jeu ; le fichier
`LISEZ-MOI.txt` du dossier explique tout (y compris les avertissements de
Windows et de macOS, normaux pour un jeu non signé).

## Commandes

| Touche | Action |
|---|---|
| Flèches ou ZQSD | Se déplacer, naviguer dans les menus |
| Maj (maintenue) | Courir |
| Échap > Carte du monde | Les lieux où aller, et où vous êtes |
| Entrée ou Espace | Parler, valider, faire défiler les dialogues |
| Échap | Annuler, ouvrir le menu (équipe, tactiques, objets, magie, sauvegarde) |
| Tab (en combat) | Mode auto : les tactiques jouent à votre place, ou vous reprenez la main |
| F1 | Aide : statistiques, dégâts, échiquier et zones, états, tableau des types, exploration, touches |
| F2 | Vitesse du jeu ×1, ×2 ou ×3 (marche, combats, textes ; gardée pour la prochaine fois) |
| F11 ou Alt+Entrée | Plein écran ou fenêtre (le choix est gardé pour la prochaine fois). L'image s'adapte à la forme de l'écran ou de la fenêtre, sans bandes noires |

Les touches se changent dans **Options** (écran titre, ou Échap > Options),
avec le volume de la musique et des effets sonores et la vitesse du jeu ; les flèches, Entrée et
Échap marchent toujours. **Manette** : branchez-la, même en cours de partie.
Croix ou stick gauche pour se déplacer, **A** valider, **B** annuler,
**Start** menu, **Select** (ou **Y**) mode auto en combat.

## L'évolution

Vingt-deux créatures peuvent **évoluer** quand elles atteignent un niveau
(Ronceau en Ronceflore au niveau 16, Rocaillou en Galetor…) : plus fortes, un
peu plus grandes, avec de nouvelles techniques. Le jeu demande « Évoluer ? »
après le combat ; si vous répondez Non, Échap > Évolution le fera plus tard.

## La forge

Une fois Brann dans l'équipe, l'enclume de **Forgeroc** renforce une arme ou une
armure (+1, +2, +3) contre de l'or et des matériaux : **minerai de fer** (créatures
Roche et Métal), **cœur de braise** (créatures Feu), **cristal de givre**
(créatures Glace), quelques coffres, et un **trophée** laissé par chaque boss
pour le +3. Coûts et effets : Échap > Aide > Forge.

## L'échiquier de combat (3.4)

Les deux camps partagent **un seul plateau de 8 × 4 cases**, comme un échiquier : votre équipe part de la moitié
gauche, les ennemis de la droite, et **chacun peut passer de l'autre côté**.

- **Se déplacer** ne finit plus le tour : une fois par tour, avant d'agir, on avance de 2 cases (3 pour les types
  Vent et Foudre). On traverse ses alliés, pas les ennemis, et on s'arrête en arrivant à leur contact : une ligne
  de combattants protège ceux qui sont derrière. Les types Vent et Esprit volent par-dessus tout. **Attendre**
  finit le tour sans agir (on rejoue plus tôt).
- Les coups **au corps à corps** visent un ennemi **au contact** (les 8 cases autour ; un boss frappe plus loin).
  Si la cible est plus loin mais qu'on peut la rejoindre ce tour, le combattant s'approche d'abord. Les attaques
  **à distance** visent n'importe quelle case, mais perdent un quart de leur force quand un ennemi est au contact
  du lanceur : allez gêner les mages !
- **Dans le dos** : frapper un ennemi depuis le côté de son propre camp fait ×1,3 de dégâts. **Encerclé** : chaque
  adversaire au contact au-delà du premier ajoute 10 % aux dégâts reçus (pas un boss).
- **Percée** : le premier qui atteint le bord du camp adverse gagne +1 en Attaque, Magie et Vitesse pour tout le
  combat. Les ennemis aussi : barrez-leur la route.
- **Obstacles** : selon le décor, des rochers, buissons ou tonneaux occupent quelques cases du milieu. On ne peut
  ni s'y arrêter ni les traverser (sauf en volant) ; un ennemi poussé contre l'un d'eux subit un choc.
- En choisissant sa case, de petits **losanges rouges** montrent combien d'ennemis pourront y frapper au prochain tour,
  et le chemin s'affiche en pointillés. Tant qu'on n'a pas agi, **Revenir** annule le déplacement.
- **Tactiques** de l'échiquier : S'approcher, Contourner (passer dans le dos) et Protéger un allié ; le tour continue
  ensuite avec la règle suivante. En mode auto, chacun choisit aussi sa case avant d'agir.
- **Bottes** (accessoires) : les Bottes de vent (Forgeroc) et les Bottes ailées (Givreval) font avancer d'une case de plus.
- **Défis de l'échiquier** (écran titre > Outils > Arène) : cinq positions imposées, une leçon chacune (dans le dos, voler par-dessus une ligne,
  Percée, tenir une ligne, pousser contre un rocher). Dans la Vallée, le **maître d'armes** propose le premier en
  entraînement (une défaite ne coûte rien) et offre des Bottes de vent au vainqueur.
- Certaines techniques **bougent** : Dague, Coup de bec, Piqué tonnerre et Lame d'ombre font **bondir** derrière la
  cible ; Coup bas fait **échanger** sa case avec celle de la cible.
- Chaque technique a une **zone** : une cible, une **ligne** (d'un bord du plateau à l'autre), une **colonne**, une
  **croix** (la cible et ses voisines) ou tout le camp. Les cases autour de la cible prennent un peu moins. En
  choisissant la cible, les cases touchées s'allument en or.
- Échap > **Formation** : la case de chacun au début du combat, dans votre moitié. En combat, la case et la cible
  se choisissent directement sur le plateau, aux flèches ou à la souris.
- Les **boss annoncent** parfois leur plus grosse attaque : une zone s'allume en **rouge**. Au tour suivant, elle
  frappe très fort ceux qui y sont restés : écartez-vous ! En mode auto, la tactique « Soi : zone annoncée → Se
  mettre à l'abri » le fait pour vous.

## Brisure, ordre des tours et talents (2.5)

Inspiré d'Octopath Traveler, de Final Fantasy X et de Pokémon :

- **Brisure** : chaque ennemi a un **bouclier** (le chiffre dans l'écu bleu,
  sous lui ; 2 pour une créature, 8 à 12 pour un boss). Chaque coup sur une de
  ses **faiblesses** en retire 1 (2 si c'est très efficace). À zéro, il est
  **BRISÉ** : il perd son prochain tour, ses attaques annoncées sont annulées,
  et il encaisse **50 % de dégâts en plus** jusqu'à ce qu'il se ressaisisse
  (bouclier plein). Viser les faiblesses et garder ses gros coups pour le
  moment où il est brisé, c'est la clé des boss.
- Les faiblesses se **découvrent** : un point de couleur apparaît sous la barre
  de PV pour chaque faiblesse trouvée, et le Bestiaire s'en souvient (celles
  d'une créature capturée sont toutes connues).
- **Ordre des tours** : la frise en haut de l'écran montre qui joue ensuite
  (bleu : vos alliés, rouge : les ennemis, or : le boss ; barré : un tour perdu
  par un ennemi brisé). Les techniques rapides et lourdes la font bouger.
- **Talents** : chaque espèce a un pouvoir permanent, gardé quand elle évolue :
  Carapace et Écorce runique (moins de dégâts physiques ou magiques), Rage,
  Soigneur (soigne ses voisins sur le plateau), Régénération, Méditation, Furtif
  (esquive à l'arrière), Vif (commence avec la jauge avancée), Briseur (use
  plus les boucliers), Œil perçant, Esprit clair (insensible aux états),
  Intimidation (baisse l'Attaque adverse en début de combat), Peau piquante,
  Cuirasse (gros bouclier). Visible sur la fiche (Échap > Équipe) et dans
  le Bestiaire ; détails dans l'Aide (F1).
- Chaque type a son effet visuel (flammes, gouttes, éclairs, cristaux…) ;
  l'écran tremble sur un coup critique ou une brisure, et les boss entrent en
  scène avec leur nom.

Les boss sont plus durs qu'avant : sans viser leurs faiblesses ni s'équiper,
on perd souvent.

## Pousser, météo, terrain et duos (2.6)

Inspiré d'Into the Breach et de Chrono Trigger :

- **Pousser / tirer** : Charge, Ruée, Rafale, Vague, Fracas… repoussent la cible
  d'une case vers l'arrière ; Flèche-grappin (Kael, niveau 11), Tornade,
  Étreinte… la tirent vers l'avant. Si la case est prise, les deux se heurtent
  (**choc** : un tiers des dégâts du coup, pour chacun) ; poussé contre le bord
  arrière, choc aussi. Tirer un mage vers l'avant le met à portée de l'épée.
  Les boss sont inébranlables, mais les ennemis aussi poussent et tirent.
- **Météo** (affichée sous l'ordre des tours) : pluie (Eau +25 %, Feu -25 %),
  grand soleil, canicule à Cendrelune, blizzard dans les Pics, orage, brume…
- **Terrain** : des cases colorées sur le plateau. Braises (brûlent à la fin du
  tour, ou dès qu'on y est poussé), ronces (poison), verglas (jauge plus lente),
  herbes hautes (esquive), source (soigne), cristaux (Magie +20 %). « Se
  déplacer » dit le terrain de chaque case ; en mode auto, « Se mettre à l'abri »
  quitte une case qui blesse.
- **Duos** : deux alliés qui vont ensemble lancent une technique à deux, bien
  plus forte (menu **Duo**). Le partenaire doit avoir sa jauge ATB à 50 % au
  moins : il la dépense. Lior + Maëlle (Lame sacrée), Lior + Brann, Isra + Kael,
  Maëlle + Isra, Sélène + Brann, Sélène + Kael, chaque héros avec sa créature
  (Lame ardente avec une créature de Feu, déferlante avec l'Eau…), et des
  créatures entre elles (Feu + Vent, Eau + Foudre). La tactique « Meilleure
  attaque » s'en sert aussi.
- **Jour et nuit** sur les cartes (un jour dure 12 minutes) : fenêtres allumées,
  lanterne, lucioles ; fumée des cheminées, feuilles en forêt, papillons dans la
  vallée.

## Gemmes, rôles et phases des boss (2.7)

Inspiré des matérias de Final Fantasy VII et des combats de boss en plusieurs
temps :

- **Gemmes** : chaque équipement porté a des **chatons** (1 par arme, armure et
  accessoire ; un de plus forgé à +2). Échap > Équipement > un membre > **Gemmes**.
  Gemmes de technique (Feu, Givre, Foudre, Soin, Rempart, Brasier : même une
  créature peut les lancer), de bonus (+15 % PV, +10 % Attaque ou Magie) et de
  soutien (éveil : jauge avancée ; vampire : rend une part des dégâts ; riposte ;
  brise-garde : use plus les boucliers). En boutique, et chaque grand boss en
  laisse une.
- **Rôles des ennemis** (icône à droite de leur barre de vie) : **protecteur**
  (écu : ses voisins prennent 30 % de dégâts en moins), **soigneur** (croix verte :
  soigne ses alliés), **chef** (couronne : ses alliés vont 20 % plus vite). L'IA et
  les tactiques les visent en priorité. Vos créatures protectrices et chefs aident
  aussi votre équipe.
- **Phases des boss** : sous un seuil de PV, un boss change : bouclier rechargé,
  bonus, nouvelles techniques, autre météo, renforts (Ignarok reforme sa carapace
  (sa carapace se reforme) puis appelle un tisonnel, la Ronce-Mère fait germer des soigneurs…).
- **Portraits** dans les dialogues, **écran titre** animé (étoiles, étoile filante,
  lucioles, reflet sur le titre).

## Astuces et conseils (2.8)

- **Astuces** : la première fois qu'on rencontre une règle (bouclier, faiblesse,
  rôle, météo, terrain, duo, phase d'un boss, gemme…), une courte explication
  s'affiche ; **F1** ouvre aussitôt la page d'Aide qui va avec. Options >
  Astuces les coupe.
- **Conseils** : les aubergistes, et le Journal (Échap > Journal), parlent du
  prochain gardien : où il est, ses faiblesses déjà découvertes, quoi viser et
  quelle gemme apporter.
- **Menus** : le sac est rangé par catégories (soins, capture, gemmes,
  équipement, matériaux, objets importants) ; en choisissant un équipement, on
  voit ce qu'il change par rapport à l'actuel et ses chatons ; le menu Duo
  affiche le coût en PM.

## Acte 2 : l'archipel d'Écume (3.0)

Une fois la Mère des Brumes apaisée, le **pêcheur** de la vallée peut enfin
traverser : son bateau mène à **Port d'Écume** (soins, boutique avec de nouvelles
armes, armures et gemmes, auberge, passeur pour rentrer).

- **Récif d'Écume** : nouvelles créatures (Écumet, Coquillon et son évolution
  Bastillon, Voltanguille et Fulgoray, Nuageon, Orageline, Noyelin), deux
  dresseurs, des perles d'écume pour la quête du phare.
- **Ruines englouties** : une **énigme** (rochers à pousser sur des dalles pour
  ouvrir une grille), puis le **Kraken des récifs**.
- **Tour de l'Orage** : le **Griffon tempête**, une seconde énigme à trois
  rochers, et au sommet le **Seigneur des Orages**, en trois phases : le vrai
  final.
- **Énigmes** : marchez dans un rocher pour le pousser. Coincé ? Échap >
  **Recommencer l'énigme**.
- Quête **Les perles du phare** (quatre perles d'écume) ; la Gemme du maître et
  la Gemme d'orage se trouvent en chemin.

## Langue : français ou anglais (3.1)

Options > **Langue** : le jeu passe aussitôt en anglais (ou en français). Au premier
lancement, il choisit la langue de l'ordinateur. Les éditeurs (Réglages, cartes,
histoire) restent en français. Le monde généré de l'Expédition garde des noms
français.

Pour les curieux : toutes les phrases sont dans `data/langues/en.json` (phrase
française → traduction). `python outils/textes.py` y ajoute les nouvelles phrases
après un changement (avec une traduction vide : le jeu affiche alors le français),
et `python outils/textes.py en --manque` liste celles qui restent à traduire.

## Techniques rapides et lourdes

Chaque technique a un rythme, indiqué dans l'aide en bas de l'écran :

- **rapide** (Lame d'acier, Charge, Jet d'eau…) : moins de dégâts, mais la jauge
  ATB repart à 25 % et le personnage rejoue plus tôt ;
- **lourde** (Taillade éclair, Fracas, Charge héroïque) : un gros coup, mais la
  jauge repart en dessous de zéro (barre rouge) et le personnage rejoue plus
  tard ;
- **normale** : la jauge repart de zéro.

Les techniques faibles ont aussi souvent un effet en plus : Lame d'acier peut
augmenter la vitesse de Lior, Jet d'eau baisser l'attaque de l'ennemi,
Fouet-ronce sa vitesse. Les premières techniques apprises restent ainsi utiles,
par exemple pour achever un ennemi vite ou préparer le combat.

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

## Compagnons

Chaque héros combat avec une **créature compagnon** à ses côtés : jusqu'à
3 héros et leurs 3 compagnons sur le terrain. Vous les commandez tous ; un
compagnon joue un peu moins souvent qu'un héros. En mode auto (Tab), héros et
compagnons suivent leurs tactiques.
Échap > **Compagnons** : choisir la créature de chaque héros (sinon, la
première créature libre de l'équipe). Les boss sont réglés pour une équipe
complète et **équipée**.

## Réserve et bestiaire

L'équipe compte 8 membres au plus. Une créature capturée (ou reçue en cadeau)
quand l'équipe est complète part dans la **Réserve** (Échap > Réserve) : on y
dépose une créature, on la reprend, ou on l'échange contre un membre de
l'équipe.

Échap > **Bestiaire** : toutes les créatures du jeu. Celles que vous n'avez
jamais rencontrées restent « ??? » ; une étoile marque celles que vous avez
obtenues, avec leur fiche (dessin, types, statistiques, lieux).

Dans les boutiques, **Vendre…** rachète vos objets à la moitié de leur prix.

## Équipement

Échap > **Équipement** : chaque membre porte une **arme**, une **armure** et
un **accessoire**. Armes et armures sont réservées aux héros ; un accessoire
va aussi à une créature. Les boutiques de la vallée, de Forgeroc et de
Givreval en vendent de plus en plus puissants (bonus d'Attaque, de Magie, de
PV…). La fiche à droite montre les statistiques qui changent en direct.
L'équipement est gardé par la sauvegarde ; il compte aussi en duel et dans
l'Expédition, dont les boutiques en vendent.

## Quêtes annexes

Certains habitants ont besoin d'aide : un médaillon perdu dans la vallée, des
Tisonnels enragés près de Forgeroc, une fleur de givre pour Givreval, le
grimoire volé de l'ermite de Sylve-Noire, un gardien de cristal à apaiser dans
le Temple gelé. Les quêtes acceptées s'inscrivent dans le **Journal** (Échap >
Journal), avec l'étape où vous en êtes. Elles rapportent or et équipement.

## Après la fin

Loin des villes et des entrées des cartes, la nature est plus dense : les
rencontres sont plus fréquentes et les créatures viennent en **meutes** (jusqu'à
six). Les attaques de zone y font merveille.

Le Givrecorne vaincu, une faille s'ouvre au fond de son sanctuaire : la
**Source des Brumes**, d'où vient la brume qui rend les créatures sauvages.
Nouvelles créatures, gardiens et un dernier boss facultatif, pour une équipe
de niveau 35-40 bien équipée.

## Une aventure bien plus longue (3.5, en préparation)

- **Nouveaux lieux sur le chemin** : la Plaine des Moulins (un pont rongé par des
  Rongecrins), le Col des Hurlevents (la Harpie de givre), le Val des Murmures
  (trois esprits à apaiser), l'île de Corail et la Grotte des Marées (le Trident
  d'écume ouvre les Ruines).
- **Acte 3, l'île d'Aube** : après le Seigneur des Orages, la jetée de l'est du
  port mène à Havre-d'Aube ; Bois des Lueurs, Marais Pâle, Gouffre et Citadelle,
  jusqu'à la Brume Noire.
- **Acte 4, l'île des Échos** : après la Brume Noire, la jetée ouest de
  Havre-d'Aube ; le Colosse et le Phénix d'écho, puis l'Ombre du Premier Gardien.
- **Primes** (tableaux d'annonces des villages), **43 quêtes** (Échap > Journal),
  **30 éclats de brume** à collectionner, le **grand bestiaire** de la
  professeure.
- **28 dresseurs** sur les routes ; après le Seigneur des Orages, les dresseurs
  battus proposent une **revanche** (épées croisées au-dessus d'eux). La **Ligue
  d'Écume** au port : quatre rangs, trois combats d'affilée chacun.
- **Après l'histoire** : les épreuves des gardiens, cinq donjons (Mines,
  Glacier, Épave, Abîme, et le Vieux Phare de l'île des Échos, cinq étages), la **pêche** (canne du maître pêcheur du port, 18
  poissons) et la **Nouvelle partie +**.
- **Discussions** entre les héros (Échap > Discussions), pluie de temps en temps,
  musiques réécrites.
- **Quêtes des héros** : après l'acte 3, une quête pour chacun des six héros (son passé, un boss, une arme rien qu'à lui).
- **Cartes au trésor** : Corbin, au port, a caché huit trésors dans des coins perdus ; chacun contient la carte du suivant.
- **Cartes des gardiens** : un jeu de cartes (plateau de 3 × 3, comme le Triple Triad) contre neuf joueurs des
  auberges ; chaque créature obtenue donne sa carte, les boss se gagnent.
- **Échos des légendes** : après l'acte 4, à Port-Écho, les grands boss de l'aventure reviennent, cinq cercles de trois
  combats d'affilée.
- **Pension de Moulinbourg** : deux créatures grandissent pendant que vous marchez ; deux d'un même type laissent
  parfois un œuf, qui éclot dans l'équipe.
- **Exploits** : 32 défis (finir les actes, remplir le Bestiaire, devenir champion…), cochés dans Journal > Exploits.
- **Sauvegarde automatique** à chaque passage et porte (Options > Sauvegarde auto pour la couper).
- **Voyage rapide** : Échap > Carte, choisir un village déjà visité (flèches), Entrée pour y aller.
- **Créatures de la nuit** : la nuit, d'autres créatures sortent dans les herbes ; parfois, une meute est menée
  par un **alpha** qui se bat comme un boss.
- **Créatures chromatiques** : très rarement, une créature sauvage a des couleurs
  rares (et brille) : à capturer pour les collectionneurs.

## Mode Expédition (roguelite)

Depuis l'écran titre, **Expédition** lance une aventure où tout est généré à
partir d'une **graine** (un nombre, ou un mot via « Graine… ») : les techniques
de chaque type, les créatures, les héros, les dresseurs et les régions. La même
graine redonne exactement le même monde : on peut la noter et la partager.

1. Choisissez votre héros parmi plusieurs (classe, type, statistiques, Limite),
   puis une créature parmi trois.
2. Chaque région a un village (guérisseuse, boutique, habitants qui donnent des
   conseils, parfois un héros à recruter), des herbes à rencontres, des
   dresseurs, des coffres, et un **gardien** qui bloque la route. Une fois
   vaincu, la sortie à l'est mène à la région suivante, plus forte. Les régions
   sont sans fin : allez le plus loin possible.
3. Une défaite termine l'expédition. Elle rapporte des **éclats de brume**
   selon la distance parcourue (et les dresseurs battus), et le record de
   région est gardé.
4. Au **Camp**, les éclats achètent des améliorations permanentes : plus de héros
   et de créatures proposés au départ, or et objets de départ, niveau de
   départ, captures plus faciles, plus d'expérience.

Depuis la 2.9, l'Expédition profite aussi des nouveautés des versions 2.5 à 2.7 :
les créatures générées ont un **talent** (selon leur type) et parfois un **rôle**
(protecteur, soigneur, chef), les héros un talent selon leur classe ; chaque
**gardien a des phases** (il se déchaîne à mi-vie, change le temps dès la région 2,
appelle à l'aide dès la région 4) ; il laisse une **gemme**, et les boutiques en
vendent de plus en plus. Les duos d'un héros et de sa créature marchent aussi.
(Les mondes des graines déjà jouées gagnent ces nouveautés ; le reste ne change
pas.)

L'expédition se sauvegarde (Échap > Sauvegarder, et à chaque nouvelle région)
dans un fichier à part, `expedition.txt` : la partie principale n'est jamais
touchée.

## Multijoueur : expédition à plusieurs, duels et échanges

Écran titre > **Multijoueur**, jusqu'à **4 joueurs**. Tous doivent avoir **la
même version du jeu** (indiquée en bas à droite de l'écran titre) et les mêmes
données.

1. L'un choisit **Héberger une partie** : le jeu affiche ses adresses.
2. Les autres choisissent **Rejoindre une partie** et tapent une de ces adresses.
3. Dans le salon, l'hôte lance une **expédition** ou un **duel**.

**Expédition à plusieurs.** L'hôte choisit Nouvelle, Graine… ou Continuer :
tout le monde part dans le même monde généré.

- Chacun choisit son héros et sa créature, puis joue **de son côté** : combats
  contre les créatures et les dresseurs, captures, coffres, boutique, son or et
  ses objets. On voit les autres joueurs sur la carte, avec leur pseudo.
- Le **gardien** de chaque région se combat **ensemble** : le premier arrivé
  attend devant lui les joueurs qui ne l'ont pas encore battu (Échap > Groupe
  pour voir où ils en sont). À deux, chacun envoie deux combattants ; à trois
  ou quatre, un chacun. Le gardien est plus robuste à plusieurs : ses PV
  augmentent de 15 % par joueur en plus (Réglages > Règles > Multijoueur), et
  encore avec quatre alliés.
  Chacun commande ses combattants et peut utiliser ses objets.
- Une défaite en solo ramène au village de la région, en perdant la moitié de
  son or. Seule une défaite contre le gardien, à plusieurs, arrête l'expédition
  (avec les éclats de brume gagnés, comme en solo).
- Chacun garde sa sauvegarde (`expedition_groupe_<pseudo>.txt` : gardez le même
  pseudo pour la reprendre). Un ami qui arrive en
  cours de route part avec un nouveau héros dans la région du groupe ; avec
  Continuer, chacun reprend la sienne.

**Duel** (à deux). Chacun combat avec les trois premiers membres de l'équipe de
sa partie principale (ou, sans partie, le héros et deux compagnons de départ).
L'hôte peut mettre tous les combattants au niveau 50 (« Niveaux : égaux »).
Chacun commande son équipe, avec son équipement ; Tab active le mode auto
(tactiques). Ni objets ni fuite.

**Échanger des créatures.** Dans le salon (**Échanger**), avec les créatures de
sa partie principale ; pendant une expédition à plusieurs (Échap > Groupe >
**Échanger une créature**), avec celles de l'expédition. On choisit le joueur et
sa créature, puis :

- **Échange** : l'autre choisit la créature qu'il donne en retour, puis on
  accepte ou non ;
- **Cadeau** : l'autre la reçoit sans rien donner (il lui faut une place dans
  son équipe, 8 membres au plus).

La créature garde son niveau, son expérience et ses tactiques ; les héros ne
s'échangent pas. Chaque échange est aussitôt sauvegardé des deux côtés. Pendant
une expédition, on ne peut recevoir une créature que d'une région déjà atteinte.

**Jouer par Internet, chacun chez soi : le code de partie.** Quand vous
hébergez, le jeu demande à votre box d'ouvrir son port (UPnP) et affiche un
**code de partie** (par exemple `BJMV-Z6FX`) ; « Copier le code » le met dans le
presse-papiers. Vos amis choisissent « Rejoindre une partie » et le tapent. Le
port est refermé quand vous quittez. Pour vérifier votre box sans lancer le
jeu : `brumeval --upnp` (dans un terminal, à côté du jeu).

Si la box refuse (UPnP désactivé : il s'active dans son interface) ou n'a pas
d'adresse Internet à elle (certaines offres fibre la partagent entre abonnés),
le jeu l'écrit en bas de l'écran. Il reste alors une appli gratuite de réseau
privé, avec l'adresse qu'elle donne :

- **Tailscale** (https://tailscale.com) : créez un compte, installez-la sur les
  deux ordinateurs, invitez votre ami dans votre réseau ; l'adresse de l'hôte
  commence par `100.`.
- ou **Radmin VPN** (Windows) : l'un crée un réseau, l'autre le rejoint ;
  l'adresse commence par `26.`.

Sans code ni VPN, l'hôte peut aussi ouvrir le port **47474** (TCP) à la main
sur sa box et donner son adresse publique. La première fois qu'on héberge, Windows demande s'il faut
autoriser Brumeval sur le réseau : cochez **privé et public** puis
**Autoriser**. Sur le même Wi-Fi, l'adresse locale (`192.168.…`) suffit.

« Délai dépassé : personne ne répond » : un VPN (NordVPN…) sur l'ordinateur de l'hôte bloque souvent les connexions, même sur le même Wi-Fi. Quittez-le pendant la partie, inversez les rôles, ou passez par Tailscale sur les deux ordinateurs (même compte) avec l'adresse en `100.`.

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
5. Pour déboguer (points d'arrêt) : **F5**, configuration « Brumeval
   (Windows, MSVC) ».

En cas de souci : si CMake Tools parle d'un « generator » différent ou d'un
cache à effacer, faites **Ctrl+Maj+P > CMake: Delete Cache and Reconfigure**
(ou supprimez le dossier `build`), puis F7.

## Outils

Depuis l'écran titre, **Outils** donne accès à :

- **Démo** : le jeu joue tout seul une nouvelle partie, du début à la fin
  (Espace : pause et reprise, Échap : quitter, F2 : plus vite ; sa sauvegarde est à part).
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

Pour reprendre le projet soi-même : **`docs/GUIDE.md`** explique comment le
programme est construit et comment ajouter une créature, une carte, une quête,
une musique… ; **`docs/PLANNING.md`** donne la suite prévue.

## Tester

```
build\Debug\brumeval.exe --test captures
```

(ou `build\brumeval.exe --test captures`, selon la configuration de CMake)

Le mode test vérifie les données, joue quelques scènes, simule des combats pour
l'équilibrage et enregistre des captures d'écran dans `captures/`.

```
build-release\Release\brumeval.exe --partie partie
```

Le joueur automatique joue toute l'histoire en une dizaine de secondes (version
Release) et écrit son journal dans `partie\rapport.txt`, avec des captures des
moments marquants. `brumeval --demo` le montre jouer dans la fenêtre.

## Organisation

- `src/` : le code du jeu (chaque `.hpp` explique son module en tête ; vue d'ensemble dans `docs/GUIDE.md`)
- `data/` : tout le contenu du jeu (créatures, techniques, objets, cartes,
  dialogues, règles) en fichiers JSON modifiables, lus à chaque démarrage. Voir
  `data/LISEZMOI.md`.
- `docs/` : guide pour reprendre le projet et planning.
- La sauvegarde est écrite dans `%APPDATA%\Brumeval\Brumeval\sauvegarde.txt`,
  les options (volumes, plein écran, vitesse, touches) dans `options.txt` à côté
  (le mode test utilise `sauvegarde_test.txt` et l'efface à la fin)
