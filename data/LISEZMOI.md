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
| `combat` | Lance un combat | `ennemis` (1 à 3 : `espece`, `niveau`, `pv` multiplicateur, `boss`), `boss`, `fuite`, `capture`, `victoire`, `defaite` |
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
