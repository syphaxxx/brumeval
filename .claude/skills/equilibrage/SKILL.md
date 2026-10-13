---
name: equilibrage
description: "Brumeval : mesurer et régler la difficulté (simulations du mode test, taux de victoire des boss, historique des réglages). À utiliser après un changement de combat, de techniques, d'espèces ou de boss."
---

# Équilibrage de Brumeval

Méthode :

- Le mode test simule des combats (fin de la sortie de `--test`) : IA seule, tactiques de départ, équipement des boutiques, forge, Expédition.
- `BRUMEVAL_SIMULATIONS=4` joue 4 fois plus de combats (±4-5 % au lieu de ±10 %). Pour comparer deux réglages, copier l'exécutable et `data/` dans deux dossiers temporaires (le jeu cherche `data/` à côté du programme) et lancer les deux mesures **l'une après l'autre** : en parallèle, le test multijoueur se dispute le port 47475 et une mesure s'arrête.
- `BRUMEVAL_SANS_EVOLUTION=1` : équipes sans évolution (pour mesurer l'effet des évolutions). `BRUMEVAL_JOURNAL=<mot>` affiche le journal d'une simulation.
- Plus de PV pour un boss change peu le résultat quand le combat se joue sur la survie de l'équipe : relever son niveau agit davantage.
- Après un réglage, lancer aussi le joueur automatique (`--partie`) : l'histoire doit rester faisable.

## Historique des mesures

- **Attaques de zone** (1.9, demandées le 2026-10-12) : Tourbillon (Lior, ligne), Onde de choc (Brann, colonne), Nova
  (Maëlle, croix), Salve (Kael, colonne, rapide), Éclats d'acier (Sélène, croix) ; pour les créatures Ruée, Nuée de braises,
  Torrent, Pluie de spores, Avalanche, et Brasier, Rafale, Décharge, Morsure de gel qui gagnent une zone (aucune technique
  de boss changée). Mesuré (BRUMEVAL_SIMULATIONS=4, avant → après ; IA / tactiques / équipement des boutiques) :
  Givrecorne 27 → 46 / 36 → 53 / 68 → 74 %, Golem 63 → 51 / – / 71 → 55 %, les autres dans le bruit (±8) ;
  joueur automatique : histoire finie, 1 défaite.

**Boss renforcés le 2026-10-09** (demande de l'utilisateur : « obligé de se
stuffer avant les boss »), puis réglés pour les compagnons (équipes des
simulations : héros de ce moment de l'histoire + 2 ou 3 créatures). Réglages
(`evenements.json`, niveau et `pv`) : Sylvarque N.15 ×6,5, Golem N.24 ×12,
Ronce-Mère N.21 ×7, Ignarok N.24 ×7, Givrecorne N.36 ×10. Mesuré
(`BRUMEVAL_SIMULATIONS=2`, sans / avec le meilleur équipement de la boutique de
la région) : Sylvarque 12 / 42 %, Golem 55 / 70 %, Ronce-Mère 21 / 46 %,
Ignarok 32 / 56 %, Givrecorne 26-36 / 72 %. Sans les compagnons ralentis
(`compagnon_atb` 1), tout passait à ~100 %. Ajoutés le 2026-10-10 (même
mesure) : gardien de cristal (équipe N.28) 48 % / 100 %, Mère des Brumes
(équipe N.38) 36 % / 75 %. Remesuré avec le damier (1.7, 2026-10-10, BRUMEVAL_SIMULATIONS=3 pour les trois premiers) : Sylvarque 12 / 50 %, Ronce-Mère 27 / 56 %, Ignarok 17 / 48 %, Golem 70 / 85 %, Givrecorne 35 / 75 %, gardien de cristal 33 / 100 %, Mère des Brumes 46 / 86 % ; l'IA ennemie frappe au corps à corps le meilleur adversaire à portée quand sa cible préférée est à l'arrière (sinon elle abusait des attaques de zone). Le Golem réagit peu à
l'équipement. Le mode test lit les boss dans les événements (`bossEvent`) :
ses mesures suivent toujours le jeu.
Avant ce renfort (IA automatique, sans objets) : combats
normaux, bandits, chevaliers et duels gagnés à ~95-100 %, Sylvarque ≈ 80 %
(N.11), Ronce-Mère ≈ 55-70 % (N.16), Ignarok ≈ 30-45 % (N.22), Givrecorne
≈ 35 % (N.30, avec Sélène). Un boss trop facile vient souvent de sa lenteur ou
d'acolytes trop faibles, pas de ses PV : regarder le journal (BRUMEVAL_JOURNAL).
Avec les tactiques de départ (mêmes combats, `simTactics`), l'équipe soigne
plus tôt que l'IA : Sylvarque ≈ 85 %, Ignarok ≈ 65 %, mais Givrecorne ≈ 90 %
(Lior, Maëlle et Sélène savent tous soigner).
Chaque ligne ne joue que 20 à 40 combats : d'une fois à l'autre, un taux varie
de ±10 %. Pour comparer deux réglages, `BRUMEVAL_SIMULATIONS=5` joue cinq fois
plus de combats (±4 %), et c'est plus long.
Expédition (équipe générée : héros, créature de départ et recrue, deux niveaux
au-dessus des créatures sauvages) : dresseurs et sauvages gagnés à 100 %,
gardiens à 85-100 % pour une équipe bien assortie, moins quand les types se
prêtent mal au gardien (le joueur peut capturer d'autres créatures ; les
habitants donnent son point faible). Réglages du gardien dans
`procgen::generateRegion` (niveau, multiplicateur de PV, acolytes, renforts).
Le Vent fait ×4 à Plante/Poison et le Métal ×4 à Glace/Roche : la Ronce-Mère et
le Givrecorne ont une résistance propre pour ramener cela à ×2. La Lumière fait ×2 à l'Ombre mais l'Ombre est neutre sur la Lumière :
sinon Maëlle, ciblée en priorité par l'IA, tombe dès le début contre Sylvarque.
Gardien à plusieurs : sans renfort, il était souvent plus facile qu'en solo
(région 5, graine 33 : 13 % seul, 100 % à 2, 3 ou 4 joueurs). Renforcé « un
peu » à la demande de l'utilisateur (2026-10-09) : +15 % de PV par joueur en
plus. Mesuré ensuite (15 combats par ligne, moyenne des graines 11/22/33) :
région 3 : 100 % seul, 84-93 % à plusieurs ; région 5 : 42 % seul, 55-64 % à
plusieurs. +20 % rendait le groupe plus dur que le solo en région 3. Les
simulations ordinaires n'utilisent pas l'équipement.
Avec l'équipement (mesuré le 2026-10-09, `simGear` dans test.cpp : le meilleur
achat de la boutique de la région pour chaque membre, borne haute) : Sylvarque
75 → 100 %, Ronce-Mère 73 → 93 %, Ignarok 25 → 90 %, Givrecorne 35 → 92 %
(sans tactiques) ; gardiens de l'Expédition : région 5 graine 33 6 → 40 %,
région 8 graine 11 73 → 93 %. Tout acheter rend les boss d'Ignarok et du
Givrecorne faciles : réglé en 1.5 (boss renforcés, voir plus haut ; ces
chiffres-ci sont ceux d'avant le renfort).
Duel avec l'équipement (`Online::Member::gear`, équipe `[espèce, niveau,
équipement]`), décision du 2026-10-09.
