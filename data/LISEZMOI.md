# Les données du jeu

Tout le contenu de Brumeval est rangé ici, au format JSON. Le jeu lit ces
fichiers à chaque démarrage : modifiez-en un, relancez le jeu, et le changement
apparaît. Pas besoin de recompiler.

Si un fichier contient une erreur, le jeu l'indique au démarrage (nom du fichier
et numéro de ligne). Le mode test (`brumeval --test captures`) vérifie aussi que
toutes les références existent et que chaque lieu des cartes est accessible.

| Fichier | Contenu |
|---|---|
| `types.json` | Les types (Feu, Eau…), leur couleur et la table d'efficacité |
| `techniques.json` | Techniques, sorts et Limites |
| `especes.json` | Héros, créatures et boss : statistiques de base, techniques apprises |
| `objets.json` | Objets : prix, effets (soin, PM, réanimation, capture) |
| `apparences.json` | Couleurs et accessoires des personnages |
| `regles.json` | Départ de la partie, rencontres, formules de combat, récompenses… |
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
