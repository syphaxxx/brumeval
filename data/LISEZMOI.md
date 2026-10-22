# Les données du jeu

Tout le contenu de Brumeval est rangé ici, au format JSON. Le jeu lit ces
fichiers à chaque démarrage : modifiez-en un, relancez le jeu, et le changement
apparaît. Pas besoin de recompiler.

Vous pouvez aussi tout modifier depuis le jeu (écran titre > Outils) :
**Réglages** pour les règles (dont les tactiques de départ), espèces, techniques, types et objets, **Éditeur
de cartes** pour `cartes/`, **Éditeur d'histoire** pour `evenements.json`.

Si un fichier contient une erreur, le jeu l'indique au démarrage (nom du fichier
et numéro de ligne). Le mode test (`brumeval --test captures`) vérifie aussi que
toutes les références existent et que chaque lieu des cartes est accessible.

| Fichier | Contenu |
|---|---|
| `types.json` | Les 13 types (Feu, Eau, Glace, Roche, Vent, Poison, Métal, Esprit…), leur couleur, leurs immunités et la table d'efficacité |
| `techniques.json` | Techniques, sorts et Limites |
| `especes.json` | Héros, créatures et boss : statistiques de base, techniques apprises, `evolution` (`espece` et `niveau` : la forme qu'elle peut prendre), `taille` (dessin agrandi, 1.15 pour les formes évoluées), `sprite` (son dessin : un nom de `sprites.json`), `talent` (identifiant d'un talent de `regles.json`), `bouclier` (bouclier de Brisure en ennemi ; absent : valeur des règles), `description` (le mot du Bestiaire, montré une fois l'espèce obtenue) |
| `objets.json` | Objets : prix, effets (soin, PM, réanimation, capture), équipement (`equipement` : `arme`, `armure` ou `accessoire`, et `bonus` : `pv`, `pm`, `attaque`, `defense`, `magie`, `resistance`, `vitesse`, `precision`, `esquive`, `critique`) ; `"trophee": true` pour un trophée de boss (forge) ; `pour` : l'identifiant du seul héros qui peut le porter (armes des quêtes des héros) ; `icone` : son icône dans les menus (un nom de `sprites.json`). Les armes et armures forgées (`epee_fer+1`…) sont créées par le jeu, pas écrites ici |
| `apparences.json` | Couleurs et accessoires des personnages ; `sprite` : le début du nom de ses sprites (`lior` → `lior_bas_0`…) |
| `sprites.png`, `sprites.json` | L'atlas des sprites et la place de chacun, fabriqués par `outils/sprites/decoupe.py` (ne pas modifier à la main) |
| `regles.json` | Départ de la partie, rencontres, formules de combat, récompenses (dont `butin_type` : matériaux selon le type de la créature vaincue), forge, tactiques de départ… |
| `evenements.json` | Dialogues et événements de l'histoire |
| `cartes/*.json` | Une carte par fichier : tuiles, bâtiments, habitants, coffres… |
| `sons.json` | Musiques et bruitages (fabriqués par le jeu, sans fichier audio) |
| `quetes.json` | Quêtes annexes : nom, lieu, étapes affichées dans le journal |
| `exploits.json` | Exploits (Journal > Exploits) : `id`, `nom`, `description`, `si` (condition) ; atteint, il pose le drapeau `exploit:<id>` et reste acquis |
| `discussions.json` | Discussions entre héros (Échap > Discussions) : `id`, `titre`, `si` (condition), `membres` (espèces dans l'équipe), `dialogue` (lignes « Nom : texte ») |
| `cartes_jeu.json` | Cartes des gardiens : `depart` (les cinq cartes données à tous), `adversaires` : `id`, `nom`, `cartes` (cinq espèces), `niveau` (0 à 2 : force de l'ordinateur), `or` et `carte` (espèce) gagnés à la première victoire, `regles` (`["identique"]` : deux côtés égaux ou plus retournent aussi) |
| `peche.json` | Poissons de la pêche : `objet` (un objet de objets.json), `eaux` (identifiants de carte, sinon thèmes), `poids` (fréquence), `difficulte` (1 à 5) |
| `primes.json` | Primes (monstres recherchés) du tableau d'annonces et rangs de chasseur (voir plus bas) |

## Le son (`sons.json`)

Le jeu fabrique lui-même ses sons, comme les vieilles consoles : aucune
musique enregistrée, seulement des partitions et des recettes de bruitages.

**Ondes** (`onde`) : `carree` (son de console, `rapport` de 0.05 à 0.95 change
son timbre), `triangle` (doux, bon pour les basses), `scie` (dur), `sinus`
(très doux, cloches) et `bruit` (souffle, percussions).

**Musiques** (`musiques`) : `tempo` (temps par minute), `pas_par_temps` (2 par
défaut : chaque case de la partition dure une croche) et une liste de `voix`.
Chaque voix a son `onde`, son `volume` (0 à 1) et ses `notes`, une case par
mot :

- une note : `A4`, `C#5`, `Bb3` (lettre anglaise A–G, dièse `#` ou bémol `b`,
  octave ; `A4` = le la du diapason) ;
- `.` prolonge la note précédente, `-` est un silence ;
- `x` : un coup de percussion (voix `bruit`, hauteur réglée par `frequence`) ;
- `|` sépare les mesures, seulement pour s'y retrouver.

Chaque voix tourne en boucle sur ses propres cases : une mélodie de 16 mesures
peut jouer sur des basses de 8 mesures (deux fois les mêmes accords), ce qui
rend le morceau moins répétitif. Garder des longueurs multiples l'une de
l'autre. `extinction` (en secondes) fait s'éteindre chaque note toute seule,
comme une corde pincée.

Options d'une voix (2026-10-19) : `vibrato` (en demi-tons, 0.1 à 0.3 ; il s'installe sur
les notes tenues), `vitesse_vibrato` (Hz, 5.5 par défaut), `attaque` (secondes de montée
du son : 0.2 pour une nappe douce), `ondulation` (le `rapport` d'une onde carrée ondule :
timbre vivant). Option d'un morceau : `echo` (`delai` en secondes, `retour` 0 à 0.8,
`melange` 0 à 1) : la musique se répète plus faiblement, comme dans une grotte.
Les morceaux sont écrits par `python outils/musique/compose.py` à partir des partitions
de `outils/musique/morceaux.py` (accords, mélodie, accompagnement fabriqué d'après les
accords) : modifier plutôt ce fichier, puis relancer le script.

**Lieux** (`lieux`) : la musique de chaque thème de carte (`vallee`, `foret`,
`cendres`, `grotte`, `neige`, `interieur`), et d'une carte précise par son identifiant
(`port`, `recif`, `ruines`, `tour`), qui passe avant son thème. Les morceaux `titre`,
`combat` et `boss` sont joués à l'écran titre et en combat ; l'action `combat` peut
imposer le sien (`"musique": "boss_final"`).

**Effets** (`effets`) : une liste de sons simples joués l'un après l'autre
(ou `{"variation": 0.1, "sons": [...]}` : la hauteur change un peu, au hasard,
à chaque fois, pour que les bruits répétés ne lassent pas) :
`onde`, `de` et `a` (fréquence en Hz ou note, la hauteur glisse de l'une à
l'autre), `duree` (secondes), `volume`. `depart` (secondes) place un son à un
moment précis : deux sons avec `"depart": 0` jouent ensemble. Le jeu utilise
`curseur`, `valider`, `retour`, `refus` (menus), `coup`, `critique`, `sort`,
`soin`, `rate`, `ko`, `statut`, `bonus`, `malus`, `lancer`, `capture`,
`coup_rapide` et `coup_lourd` (selon le rythme de la technique), `sort_<type>`
(un sort de cet élément, par exemple `sort_feu` ; sinon `sort`),
`capture_rate`, `limite`, `fuite`, `niveau`, `victoire`, `defaite`,
`rencontre` (combat), `coffre`, `achat` et `porte`.

## Le combat

**Statistiques des espèces** (`especes.json`, bloc `base`, multipliées par le
niveau) : `pv`, `pm`, `attaque` et `defense` (coups physiques), `magie` et
`resistance` (sorts), `vitesse`. Champs facultatifs, en pourcentage et
indépendants du niveau : `precision` (100 par défaut), `esquive` (contre les
coups physiques), `critique`. Une espèce peut avoir deux types
(`"types": ["eau", "glace"]` au lieu de `"type"`), des `resistances` propres
(`{"physique": 0.75, "magique": 1.25, "feu": 0.5}` : multiplicateurs de dégâts)
et des `immunites` à certains états (`["poison", "sommeil"]`).

**Techniques** (`techniques.json`) : `genre` vaut `physique` (Attaque contre
Défense), `magique` (Magie contre Résistance), `soin`, `rappel` ou `statut` (pas
de dégâts, seulement l'effet). Champs facultatifs : `precision` (%), `critique`
(chance en plus, %), `effet` :

| Effet | Exemple |
|---|---|
| Infliger un état | `{"statut": "poison", "chance": 30}` (`poison`, `brulure`, `paralysie`, `sommeil`) |
| Bonus ou malus | `{"stat": "defense", "niveaux": -1}` (`attaque`, `defense`, `magie`, `resistance`, `vitesse`) |
| Sur le lanceur | ajouter `"sur": "lanceur"` |
| Guérir les états | `{"guerison": true}` |

`portee` (facultatif) : `corps` (corps à corps : ne vise que la colonne ennemie
la plus avancée encore debout) ou `distance` (n'importe quelle case). Par défaut,
une technique physique est au corps à corps, les autres à distance. `zone`
(facultatif) : `case`, `ligne` (la ligne de la cible, en profondeur), `colonne`
(sa colonne), `croix` (elle et ses voisines) ou `tout` ; par défaut `tout` pour
une cible `tous_ennemis`/`tous_allies`, sinon `case`. Les cases voisines de la
cible prennent `zone_voisins` (règles, combat : 0,75) des dégâts ; au corps à
corps, l'arrière donne et reçoit `arriere` (0,75).

`rythme` (facultatif) règle le moment où le lanceur rejoue : `rapide` (sa jauge
ATB repart à `jauge_rapide` %, 25 par défaut) ou `lourde` (elle repart à
-`retard_lourde` %, -25 par défaut) ; sans ce champ, la jauge repart de zéro.
Une technique rapide frappe moins fort mais plus souvent, une lourde frappe plus
fort mais laisse le temps à l'adversaire. L'ordinateur et les tactiques en
tiennent compte (dégâts divisés par le temps que prend la technique).

Un type peut rendre insensible à un état (`types.json`, `"immunites"`). Les
valeurs générales (critiques, effet d'un bonus, dégâts du poison, rythme…) sont
dans `regles.json`, sections `combat` et `etats`.

**Forge** (`regles.json`, `forge`) : une entrée par niveau (+1, +2, +3) avec
`bonus` (part du bonus de l'objet ajoutée, arrondie au-dessus : 0.3 = +30 %),
`or` et `objets` (matériaux à donner ; `"trophee"` = n'importe quel objet avec
`"trophee": true`). Chaque arme et armure de `objets.json` existe aussitôt en
+1, +2… ; son prix de vente monte de l'or de la forge. Les matériaux tombent
des créatures selon `recompenses.butin_type` (`type`, `objet`, `chance`) et se
trouvent dans des coffres ; chaque boss donne son trophée (action `donner`).

**Brisure** (`regles.json`, `brisure`) : `bouclier` (bouclier d'un ennemi
ordinaire), `bouclier_boss`, `degats` (multiplicateur des dégâts reçus par un
ennemi brisé). Une espèce (`especes.json`) ou un ennemi d'un combat
(`evenements.json`) peut avoir son propre `bouclier`.

**Talents** (`regles.json`, `talents`) : `id`, `nom`, `description`, `effet` et
`valeur`. Les effets possibles : `resiste_physique` et `resiste_magie` (part des
dégâts retirée), `rage` (dégâts en plus avec une technique de son type sous un
tiers des PV), `soin_voisins`, `regeneration` et `meditation` (part des PV ou PM
max rendue à la fin du tour), `esquive_arriere`, `vif` (points de jauge ATB au
début du combat), `briseur` (points de bouclier retirés en plus), `critique`,
`sans_etats`, `intimidation`, `piquant` (part des PV max de l'attaquant au corps
à corps), `cuirasse` (points de bouclier en plus). Chaque espèce choisit le sien
avec `"talent"`.

**Pousser / tirer** (2.6) : une technique avec `"deplace": "pousse"` repousse sa
cible d'une case vers l'arrière, `"tire"` la tire vers l'avant. Case prise ou bord
arrière : choc (`regles.json`, `damier.choc` : part des dégâts du coup, pour les
deux). Les boss ne bougent pas.

**Météo** (`regles.json`, `meteo`) : `id`, `nom`, `description`, `effet` (dessin :
`pluie`, `soleil`, `chaleur`, `neige`, `orage`, `brume`), `decors` (où elle tombe :
`vallee`, `cendres`, `grotte`, `foret`, `neige`), `chance`, `types` (multiplicateur
des dégâts par type), `precision` (coups physiques, en points).

**Terrain** (`regles.json`, `terrains`, et `damier.terrain_min` / `terrain_max`
cases par camp) : `id`, `nom`, `description`, `effet` (`degats` et `soin` : part
des PV max à la fin du tour ; `poison` : chance ; `lent` : jauge ATB plus lente ;
`esquive` : points d'esquive ; `magie` : Magie en plus), `valeur`, `insensibles`
(types), `decors`, `couleur`.

**Duos** (`regles.json`, `duo`) : `jauge` (jauge ATB du partenaire, en %) et
`liste` : `membres` (deux : identifiant d'espèce, `humain` ou `type:feu`) et
`technique` (dans `techniques.json`, comme les autres ; chacun paie son `cout`).

**Gemmes** (2.7, `objets.json`) : un objet avec `"gemme": {…}` : `technique` (donnée
à qui la porte), `bonus` (`{"pv": 0.15}` : pourcentage), ou `soutien` (`vol_vie`,
`contre`, `eveil`, `briseur`) et `valeur`. Chatons : `regles.json`, `gemmes`
(`chatons_arme`, `chatons_armure`, `chatons_accessoire`, `forge_chaton` : niveau de
forge qui en ajoute un).

**Rôles** (2.7) : `"role_combat"` d'une espèce (`protecteur`, `soigneur`, `chef`) ;
réglages dans `regles.json`, `roles` (`protection`, `chef`). À ne pas confondre avec
`role`, la courte description d'un humain.

**Phases d'un boss** (2.7) : dans un combat, un ennemi peut avoir `"phases"` : une
liste de `{"pv": 0.5, "message": …, "bouclier": 14, "bonus": {"attaque": 1},
"techniques": […], "meteo": "canicule", "renforts": [{"espece": …, "niveau": …}],
"soin": 0.1}`. Chaque phase se déclenche une fois, quand ses PV passent sous la part
`pv` (jamais en ligne).

**Portraits** : un dialogue « Nom : … » montre le portrait de l'apparence de ce nom
(`apparences.json`, ou un de ses autres noms dans `"aussi"`).

**Astuces** (2.8, `regles.json`, `astuces`) : `id` (le moment où elle apparaît,
choisi par le code : `bouclier`, `faiblesse`, `brisure`, `ordre`, `role`, `meteo`,
`terrain`, `pousser`, `duo`, `phase`, `gemme`, `talent`), `aide` (titre de la page
d'Aide que F1 ouvre ensuite) et `texte`. Chacune n'est montrée qu'une fois (ligne
`astuce` de la sauvegarde) ; Options > Astuces les coupe.

**Condition `objet_min`** (3.0) : `{"objet_min": {"perle": 4}}` = au moins quatre
perles dans le sac (pour les quêtes qui demandent plusieurs objets).

**Conseils** (2.8, `regles.json`, `conseils`), dans l'ordre de l'histoire :
`drapeau` (posé quand le boss est battu), `boss` (espèce), `lieu`, `gemme`
conseillée, `texte`. Le premier dont le drapeau manque est donné par l'action
`conseil` (les aubergistes) et en tête du Journal, avec les faiblesses déjà
découvertes.

**Adversaires** : l'action `combat` accepte `nom` (« Le braconnier vous défie ! »)
et `renforts`, une liste d'ennemis qui entrent un par un quand un adversaire
tombe. Un habitant avec `"vue": 3` repère le joueur jusqu'à 3 cases devant lui
et lance son événement tout seul, tant que le drapeau `vue_jusqua` n'est pas
posé (voir le braconnier dans `cartes/vallee.json`).

## Les tactiques (`regles.json`, section `tactiques`)

Les tactiques sont les règles de combat automatiques de l'équipe (menu Échap >
Tactiques, touche Tab en combat). `lignes_depart`, `niveaux_par_ligne` et
`lignes_max` fixent le nombre de lignes de chaque membre selon son niveau.
`defaut` est la liste donnée à chaque nouveau membre ; une espèce peut avoir la
sienne (`"tactiques": [ … ]` dans `especes.json`). Une règle s'écrit :

```json
{"si": "allie_pv_moins", "valeur": 40, "faire": "soin"}
```

- `si` : la condition. Ennemis : `ennemi` (n'importe lequel : celui où l'action
  fait le plus d'effet), `ennemi_pv_bas`, `ennemi_pv_haut`, `ennemi_pv_moins`,
  `ennemi_pv_plus`, `ennemi_faible` (action super efficace), `ennemi_sans_effet`
  (n'a pas encore l'état ou le malus), `ennemi_boss`, `ennemis_nombre`.
  Alliés : `allie_pv_moins`, `allie_ko`, `allie_etat`, `allie_pm_moins`,
  `allie_sans_effet` (n'a pas encore le bonus), `allie_chef`. Le membre
  lui-même : `soi`, `soi_pv_moins`, `soi_pm_moins`, `soi_limite`,
  `soi_sans_effet`.
- `valeur` : le seuil, pour les conditions qui en ont un (PV ou PM en %, nombre
  d'ennemis).
- L'action, au choix : `"faire"` (`attaque`, `technique` = attaque sans PM,
  `soin`, `reanimation`, `guerison`, `limite`), `"technique": "feu"` (une
  technique ou un sort précis) ou `"objet": "potion"` (pris dans le sac).
- `"active": false` garde la règle dans la liste sans la jouer.

Une action qui vise un ennemi demande une condition « ennemi », un soin ou un
objet de soin une condition « allié » ou « soi », et une réanimation la
condition `allie_ko`. Le mode test signale les règles qui ne vont pas ensemble.

## Les langues (`langues/*.json`)

Le jeu est écrit en français. `langues/en.json` donne la traduction anglaise de
chaque phrase : `"Équipe": "Team"`. Une phrase absente ou vide reste en français.
Après avoir ajouté du texte (un dialogue, un objet…), lancer
`python outils/textes.py` : il ajoute les nouvelles phrases avec une traduction
vide, à remplir. Le jeu traduit les noms, descriptions, dialogues, panneaux,
messages et quêtes au moment de les afficher : les fichiers de `data/` restent en
français (c'est pourquoi les éditeurs ne s'ouvrent qu'en français).

## Les cartes (`cartes/*.json`)

Chaque ligne de `tuiles` est une rangée de cases, toutes de même longueur :

| Tuile | Signification | Tuile | Signification |
|---|---|---|---|
| `.` | sol (herbe, neige… selon le thème) | `,` | hautes herbes (rencontres) |
| `=` | chemin | `T` | arbre (sapin enneigé, arbre sombre… selon le thème) |
| `~` | eau | `B` | pont de bois |
| `R` | rocher | `F` | fleurs |
| `W` | fontaine | `#` | falaise, paroi, mur de glace |
| `a` | cendre | `g` | herbes sèches (rencontres) |
| `l` | lave | `b` | pont de pierre |
| `m` | montagne | `k` | entrée de grotte, escalier |
| `c` | sol de grotte (rencontres) | `x` | cristal |
| `d` | arbre mort | `i` | glace |
| `n` | neige profonde (rencontres) | `z` | marais (rencontres) |
| `O` | rocher à pousser (énigme) | `P` | dalle de l'énigme |
| `D` | grille (ouverte par l'énigme) | `q` | pavés (places, rues ; praticable) |
| `H` | clôture | `L` | réverbère |
| `Y` | puits | `C` | caisses |
| `U` | buisson | `Q` | banc |
| `K` | charrette | `M` | étal de marché |
| `X` | feu de camp | `G` | tableau d'annonces |
| `J` | bannière | | |

Meubles des intérieurs (thème `interieur`) : `p` tapis (praticable), `t`
table, `h` lit, `e` étagère (une bibliothèque pour deux cases), `o` comptoir, `v` tonneau, `j` chaise.

- `carte_monde` : `[x, y]` de 0 à 100, la place du lieu sur la carte du monde (Échap > Carte) ; absent : le lieu n'y
  figure pas (intérieurs).
- `theme` : `vallee`, `cendres`, `grotte`, `foret`, `neige` ou `interieur`
  (couleur du sol, arbres, chemins, parois et décor des combats ; `interieur` :
  parquet, murs tapissés et meubles).
- `batiments` : `genre` (apparence et enseigne ; `decor` : un décor sans porte), `dessin` (facultatif : le sprite à
  la place de celui du genre, ex. `entrepot`, `moulin`, `ruines`, `chateau`), `nom`, `toit`, `evenement`
  (lancé à la porte) ou `interieur` : l'identifiant d'une carte au thème
  `interieur`, où la porte fait entrer. Cette carte a un `passage` vers le
  village, sur la case de sa porte (souvent un tapis `p`) ; on arrive juste
  au-dessus. Le service (soins, boutique…) est alors l'`evenement` d'un
  habitant de l'intérieur.
- `zones` : rencontres (`x`, `y`, `l`, `h`, `niveau_min`, `niveau_max`, `max_ennemis`, `creatures`) ; `nuit`
  (facultatif) : les créatures de la nuit, la moitié des rencontres quand il fait nuit.
  `meute_max` (facultatif) : la taille maximale d'une meute dans la zone (sinon la règle générale, jusqu'à 6 loin des
  entrées) ; 3 dans l'Abîme et le Vieux Phare.
- `arbres` (facultatif) : une autre sorte d'arbres, par exemple `pierre` (arbres pétrifiés : sprites
  `<arbre>_pierre`, recolorés dans `outils/sprites/variantes.json`).
- `ambiance` : `brume`, `cendres`, `obscurite`, `neige` ou `lucioles`, avec
  `jusqua` pour la faire disparaître quand un drapeau est posé.
- `passages` : `condition` (mêmes conditions que `si` dans les événements) et
  `message` pour un passage fermé, par exemple
  `"condition": {"objet": "cle_givre"}`. Un passage vers sa propre case (même carte, `tx`, `ty` = `x`, `y`) est
  une simple barrière : fermée tant que la condition est fausse, on y marche ensuite (le pont de la Plaine).
- `enigme` (3.0) : `drapeau` posé quand chaque dalle `P` porte un rocher `O` ; il
  ouvre les grilles `D` de la carte. `x`, `y` : la case de départ de l'énigme
  (« Recommencer l'énigme » y ramène ; le joueur automatique y résout l'énigme).
  Le mode test vérifie qu'il y a autant de rochers que de dalles et qu'une
  solution existe.
- `declencheurs` : une zone (`x`, `y`, `l`, `h`) qui lance un `evenement` quand
  on y entre, tant que le drapeau `jusqua` n'est pas posé.
- `habitants` : `dialogue` (phrases simples) ou `evenement`, `cache_si` (disparaît
  quand le drapeau est posé), `si` (présent seulement si ces conditions sont vraies, comme le `si` des événements ;
  aussi pour un `boss` de carte), `vue` et `vue_jusqua` pour les dresseurs,
  `promenade` (un promeneur marche jusqu'à cette distance de sa place ; jamais
  un dresseur).
- `coffres` : `objet` et `quantite`, ou `or` ; `si` : le coffre n'apparaît que si ces conditions sont vraies (une fois
  ouvert, il reste là) : cartes au trésor de Corbin.
- `lumiere` (carte) : couleur de la lumière du lieu (`"#ff9a7a"`), posée en voile léger le jour (aube rose d'Aube,
  lumière froide des Échos, rouge du Cratère).
- `eau` (carte) : `marais` = eau trouble et verte, avec des nénuphars (Marais Pâle).
- `style` (intérieur) : `chapelle` (dalles, vitraux), `auberge`, `boutique`, `soin` (carrelage) ; vide : une maison.
- `teinte_murs` (carte, thème grotte) : couleur de la roche (`"#6a5040"`) ; les murs et le sol en prennent la teinte.
- `fete` (carte) : une condition ; vraie, des lanternes montent dans le ciel la nuit (fête des lanternes de la Vallée).

## Rappels sur le format JSON

- Les textes sont entre guillemets droits : `"Bonjour !"`.
- Les éléments d'une liste ou d'un objet sont séparés par des virgules, **sans
  virgule après le dernier**.
- Les accents s'écrivent normalement (fichiers en UTF-8).

## Les événements (`evenements.json`)

Un habitant, une porte ou un boss peut lancer un événement. Un événement est une
liste d'actions, éventuellement découpée en **pages** : le jeu joue la première
page dont la condition `si` est vraie.

```json
"pecheur": {
  "pages": [
    {"si": {"sans_drapeau": "pecheur"}, "actions": [
      {"action": "drapeau", "nom": "pecheur"},
      {"action": "dire", "texte": "Pêcheur : Tenez, des plumes !"},
      {"action": "donner", "objet": "plume", "quantite": 2}
    ]},
    {"actions": [{"action": "dire", "texte": "Pêcheur : Le lac est calme."}]}
  ]
}
```

| Action | Effet | Champs |
|---|---|---|
| `dire` | Affiche un message | `texte` ({heros}, {compagnon} et {or} sont remplacés) |
| `donner` | Donne un objet | `objet`, `quantite` |
| `retirer` | Retire un objet | `objet`, `quantite` (absent : tout) |
| `or` | Ajoute ou retire de l'or | `quantite` (négative pour payer) |
| `drapeau` | Retient qu'un événement a eu lieu | `nom`, `valeur` (false pour l'effacer) |
| `recruter` | Un personnage rejoint l'équipe | `espece`, `niveau` |
| `combat` | Lance un combat | `ennemis` (1 à 6 : `espece`, `niveau`, `pv` multiplicateur, `bouclier`, `boss`, `phases`), `renforts`, `nom`, `boss`, `fuite`, `capture`, `victoire`, `defaite` |
| `question` | Question Oui / Non | `texte`, `oui`, `non` |
| `si` | Condition | `condition`, `alors`, `sinon` |
| `soigner` | Soigne l'équipe | `pv`, `pm` (true par défaut) |
| `boutique` | Ouvre une boutique | `objets` |
| `pension` | Ouvre la pension (laisser, reprendre deux créatures, prendre un œuf) ; règles `pension` de regles.json | — |
| `cartes` | Propose une partie de Cartes des gardiens (Jouer, Choisir ma main, Règles) ; la suite va dans `victoire`, `defaite`, `egalite` | `adversaire` (un `id` de cartes_jeu.json) |
| `forge` | Ouvre la forge (renforcer armes et armures : coûts dans `regles.json`, section `forge`) | |
| `reveil` | Point de réveil après une défaite | |
| `teleporter` | Change de carte | `carte`, `x`, `y`, `direction` |
| `evenement` | Lance un autre événement | `id` |
| `attendre` | Pause | `secondes` |
| `fin` | Écran de fin, avec son texte (vide : texte par défaut) ; le joueur peut ensuite continuer à explorer | `texte` |
| `quete` | Commence une quête (elle s'inscrit au journal), ou la termine avec `"fin": true` | `id`, `fin` |

**Quêtes annexes** (`quetes.json`) : une liste de quêtes, chacune avec `id`,
`nom`, `lieu`, `etapes` et `fin_texte`. Une étape a un `texte` et peut avoir
une condition `si` ; le journal montre la dernière étape dont la condition est
vraie. Une quête commencée pose le drapeau `quete:<id>`, une quête terminée
`quete:<id>:fin` : les pages des événements s'en servent (voir
`quete_medaillon` dans `evenements.json`).

Conditions possibles (toutes doivent être vraies) : `drapeau`, `sans_drapeau`,
`objet`, `sans_objet`, `objet_min`, `or_min`, `membre` (une espèce présente dans l'équipe), `poissons_min`
(espèces de poissons pêchées, carnet de pêche), `especes_min` (espèces obtenues du Bestiaire), `chromatiques_min` (créatures chromatiques dans l'équipe et la réserve), `quetes_min` (quêtes terminées), `cartes_min` (cartes des gardiens obtenues), `nuit` (`true` : seulement la nuit, `false` : le jour ; l'heure du jeu).

## primes.json (primes, 2026-10-19)

Le tableau d'annonces (tuile `G` d'une carte) montre les primes. `rangs` : `primes` (nombre de primes touchées),
`titre`, `objets` (donnés en atteignant ce rang). Chaque prime de `primes` : `id`, `nom`, `etoiles` (1 à 5),
`carte`, `x`, `y` (coin haut-gauche du monstre, un bloc de 2×2 cases libres), `espece` (dessin sur la carte),
`lieu`, `description`, `arrivee` (phrase au début du combat), `si` (condition pour qu'elle soit offerte), `combat`
(comme l'action `combat` : `ennemis` avec `pv`, `boss`, `bouclier`, `phases`…, `renforts`, `fuite`), `or`, `objets`
(liste de `{id, quantite}`). Drapeaux : `prime:<id>` (acceptée : le monstre apparaît), `prime:<id>:vaincue`,
`prime:<id>:fin` (touchée). Le mode test vérifie que chaque monstre est accessible à pied.

## defis.json (3.4)

Les défis de l'échiquier (Arène > Défis de l'échiquier…). Chaque défi : `id`, `nom`, `description` (la leçon, montrée
dans le menu), `decor` (vallee, foret, neige, cendres, grotte), `allies` et `ennemis` (comme les ennemis d'un combat :
`espece`, `niveau`, `pv`, `boss`…, plus `case` : [ligne 0-3, colonne 0-7] du plateau ; les alliés partent à gauche,
colonnes 0 à 3), `obstacles` : liste de [ligne, colonne].
