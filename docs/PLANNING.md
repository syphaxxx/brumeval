# Planning de la suite de Brumeval

État au 9 octobre 2026 : version **1.5** publiée (boss renforcés, compagnons).
Avant elle : 1.3 (son, options, manette, animations d'attaque, Ignarok
redessiné, intérieurs, équipement, quêtes annexes), 1.3.1 (musique) et 1.4
(gardien à plusieurs, équipement en expédition et en duel).

Les durées sont données en **séances de travail avec Claude** (une séance :
une demande comme « ajoute l'équipement », avec ses tests). Les étapes sont
dans l'ordre conseillé ; chacune finit par une version publiée.

## Étape 0 — Publier la 1.3 (fait : 1.3 à 1.5 publiées)

- Relire la branche (PR), essayer le jeu, fusionner, publier `v1.3`.
- Essayer **une fois** dans VS Code : F7, Maj+F5, F5 (voir CLAUDE.md, À faire).
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
  test.cpp ; chiffres dans CLAUDE.md, section Équilibrage).
- Mode auto (Tab) : un joueur a vu le menu s'ouvrir quand même pour Lior
  (combats d'histoire et boss). Pas reproduit, ni par les tests ni avec une
  version qui l'enregistrait : si ça revient, noter le combat exact.
- Fait en 1.5 : marcher dans une porte fait entrer dans la maison (avant,
  seulement Entrée).

- Petits plus du retour des joueurs.

## Étape 2 — Plus de contenu (3 à 5 séances)

- **Une quête par région** de plus : Sylve-Noire (l'ermite, les bandits) et
  le Temple gelé, avec les intérieurs de la cabane de l'ermite et des tentes.
- **Après le Givrecorne** : une région finale facultative (épreuve ou donjon)
  pour l'équipe au niveau 35-40.
- **Bestiaire** : la liste des créatures vues et capturées (menu de pause),
  avec leur fiche. Donne un but à la capture.
- Nouvelles créatures pour les nouvelles zones (formes existantes, nouvelles
  couleurs : rien à dessiner).

## Étape 3 — Systèmes de jeu (3 à 4 séances)

- **Réserve de créatures** (comme le « PC » de Pokémon) : au-delà de 8
  membres, les captures et les cadeaux vont en réserve au lieu d'être
  refusés. Devenue utile avec les échanges.
- **Vendre** ses objets en boutique (la moitié du prix).
- **Forge de Forgeroc** : améliorer une arme avec des objets trouvés.
- **Évolution** des créatures à un niveau donné (nouvelle espèce plus forte) :
  à décider, car cela change l'équilibrage de toute l'histoire.

## Étape 4 — Multijoueur plus simple (2 à 3 séances)

- Aujourd'hui il faut Tailscale (ou ouvrir un port). Piste : un petit
  **serveur relais** avec un code de partie à 6 lettres. Demande un serveur en
  ligne (coût, entretien) : à discuter avant.
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

- Découper `game.cpp` (1 500 lignes) : les menus (équipe, équipement,
  journal, options) dans un fichier `menus.cpp`.
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
