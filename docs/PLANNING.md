# Planning de la suite de Brumeval

État au 13 octobre 2026 : version **2.4** publiée (joueur automatique plus malin, commandes de la démo) ; avant elle 2.3 (code de partie) et **2.2** (les 1.7 à 2.2 en une fois : damier 4x4, zones, vitesse, Aide,
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
