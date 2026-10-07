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
| `especes.json` | Héros, créatures et boss : statistiques de base, techniques apprises |
| `objets.json` | Objets : prix, effets (soin, PM, réanimation, capture) |
| `apparences.json` | Couleurs et accessoires des personnages |
| `regles.json` | Départ de la partie, rencontres, formules de combat, récompenses, tactiques de départ… |
| `evenements.json` | Dialogues et événements de l'histoire |
| `cartes/*.json` | Une carte par fichier : tuiles, bâtiments, habitants, coffres… |

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

Un type peut rendre insensible à un état (`types.json`, `"immunites"`). Les
valeurs générales (critiques, effet d'un bonus, dégâts du poison…) sont dans
`regles.json`, sections `combat` et `etats`.

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

- `theme` : `vallee`, `cendres`, `grotte`, `foret` ou `neige` (couleur du sol,
  arbres, chemins, parois et décor des combats).
- `ambiance` : `brume`, `cendres`, `obscurite`, `neige` ou `lucioles`, avec
  `jusqua` pour la faire disparaître quand un drapeau est posé.
- `passages` : `condition` (mêmes conditions que `si` dans les événements) et
  `message` pour un passage fermé, par exemple
  `"condition": {"objet": "cle_givre"}`.
- `declencheurs` : une zone (`x`, `y`, `l`, `h`) qui lance un `evenement` quand
  on y entre, tant que le drapeau `jusqua` n'est pas posé.
- `habitants` : `dialogue` (phrases simples) ou `evenement`, `cache_si` (disparaît
  quand le drapeau est posé), `vue` et `vue_jusqua` pour les dresseurs.

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
| `combat` | Lance un combat | `ennemis` (1 à 3 : `espece`, `niveau`, `pv` multiplicateur, `boss`), `renforts`, `nom`, `boss`, `fuite`, `capture`, `victoire`, `defaite` |
| `question` | Question Oui / Non | `texte`, `oui`, `non` |
| `si` | Condition | `condition`, `alors`, `sinon` |
| `soigner` | Soigne l'équipe | `pv`, `pm` (true par défaut) |
| `boutique` | Ouvre une boutique | `objets` |
| `reveil` | Point de réveil après une défaite | |
| `teleporter` | Change de carte | `carte`, `x`, `y`, `direction` |
| `evenement` | Lance un autre événement | `id` |
| `attendre` | Pause | `secondes` |
| `fin` | Écran de fin | |

Conditions possibles (toutes doivent être vraies) : `drapeau`, `sans_drapeau`,
`objet`, `sans_objet`, `or_min`, `membre` (une espèce présente dans l'équipe).
