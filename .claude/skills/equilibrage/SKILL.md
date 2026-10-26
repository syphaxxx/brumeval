---
name: equilibrage
description: "Brumeval : mesurer et régler la difficulté (simulations du mode test, taux de victoire des boss, historique des réglages). À utiliser après un changement de combat, de techniques, d'espèces ou de boss."
---

# Équilibrage de Brumeval

Méthode :

- Le mode test simule des combats (fin de la sortie de `--test`, seulement avec `BRUMEVAL_SIMULATIONS` défini depuis le
  2026-10-24) : IA seule, tactiques de départ, équipement des boutiques, forge, Expédition.
- `BRUMEVAL_SIMULATIONS=1` joue les simulations ; `=4` joue 4 fois plus de combats (±4-5 % au lieu de ±10 %).
  Pour comparer deux réglages, copier l'exécutable et `data/` dans deux dossiers temporaires (le jeu cherche `data/` à côté du programme) et lancer les deux mesures en parallèle si besoin : depuis le 2026-10-25, le test multijoueur tire son port au hasard (`BRUMEVAL_PORT_TEST` pour l'imposer).
- `BRUMEVAL_SANS_EVOLUTION=1` : équipes sans évolution (pour mesurer l'effet des évolutions). `BRUMEVAL_JOURNAL=<mot>` affiche le journal d'une simulation.
- `BRUMEVAL_SIM_FILTRE=Golem,Ignarok` (2026-10-25) : seulement les lignes dont le nom contient l'un de ces mots, sans accent ni majuscule (2026-10-26 : « Phenix » trouve « Phénix »)
  (sous Windows, une variable accentuée n'arrive pas telle quelle) ; les tests ordinaires passent quand même avant (~3 min). Plusieurs mesures en même temps se gênaient sur les fichiers de sauvegarde du mode test (échecs « expédition » sans rapport) : depuis le 2026-10-26, donner à chacune son dossier avec `BRUMEVAL_DOSSIER=<dossier>`.
- Plus de PV pour un boss change peu le résultat quand le combat se joue sur la survie de l'équipe : relever son niveau agit davantage.
- Après un réglage, lancer aussi le joueur automatique (`--partie`) : l'histoire doit rester faisable.

## Historique des mesures

- **IA alliée qui lit les intentions** (2026-10-26 ; `autoCommand`, `attackScore`, `posValue` ; rien côté ennemi) :
  abattre d'abord l'ennemi qui a annoncé un coup dangereux (×1,2, ×2 si le coup l'abat ou le brise), garde quand elle
  est acculée (vie sauvée, ou ripostes + part évitée des corps à corps annoncés ≥ son action × 0,75), couvrir un
  soigneur ou un blessé (+30 pour la case qui coupe la route). Mesuré ×2, même graine (avant → après ; IA) : défis La
  garde 27 → 52 %, La Percée 67 → 97, Lire les intentions 90 → 92, Le mur 77 → 65, Dans le dos 100 → 90, Bousculade 52 →
  52 ; ×4 : La garde 36 → 56, Percée 77 → 96, intentions 91 → 96, mur 75 → 72, dos 92 → 88. Boss dans le bruit (IA /
  tactiques / boutiques / forgé, ×2) : Sylvarque 10 → 10 / 18 → 18 / 51 → 47 / 58 → 56, Golem 20 → 40 / – / 37 → 55 / 42
  → 47 (×4 : 35 → 38 / – / 47 → 40 / 52 → 47), Ronce-Mère 13 → 15 / – / 50 → 45 / 36 → 40, Ignarok 16 → 23 / 16 → 20 /
  53 → 51 / 73 → 65, Givrecorne 22 → 18 / 33 → 33 / 62 → 63 / 85 → 82, cristal 75 → 75 / 66 → 68 / 98 → 100 / 100, Mère
  des Brumes 3 → 8 / 0 → 0 / 63 → 61 / 88 → 78 (×4 : 6 → 8 / 0 → 0 / 64 → 56 / 71 → 74), Griffon 43 → 33 / – / 66 → 63 /
  63 → 80, Seigneur 48 → 48 / – / 86 → 86 / 83 → 85, Brume Noire 37 → 35 / – / 75 → 80 / 90 → 85, Orage ancien 100 → 85
  (×4 : 97 → 90), Phénix 85 → 80, le reste à 100 % ; aucun boss nettement plus facile. Essais : la première version
  (garde aussi contre les sorts, pour un quart des dégâts évités ; Lior, qui connaît Soin, compté comme soigneur)
  donnait La garde 65 %, mais la Mère des Brumes équipée 8 à 12 points plus bas sur trois mesures → contre des sorts, la
  garde seulement pour sauver sa vie, et seul un vrai soigneur (`medic`) compte. Garde par les seules ripostes : La
  garde 45 %. Retraits un à un (×4, première version) : sans la garde, La garde 37 % et Percée 78 % ; sans couvrir, Le
  mur 58 % ; sans abattre d'abord, peu de différence (intentions 96 %, mur 76 %) mais rien n'empire : gardé (il sert
  aussi aux tactiques du joueur automatique, par `attackScore`). Partie automatique sans mémoire (première version) :
  tout fini en 12 h 25, 10 défaites, quatre actes en 5 h 51 avec 6 défaites (meutes du Récif, du Gouffre, du Cratère et
  du Sanctuaire, un combat à renforts de Havre-d'Aube deux fois ; aucun boss de l'histoire perdu), estimation joueur
  24 h 31 ; référence bot31 (sans ce travail) : 10 h 22, 10 défaites, quatre actes en 6 h 29. Version retenue : tout fini
  en 12 h 49, 11 défaites, quatre actes en 6 h 04 avec 5 défaites (deux meutes des premières minutes, le Seigneur des
  Orages une fois, meutes de la Forêt Pétrifiée et du Sanctuaire), puis le Vieux Phare ×4 et l'Orage ancien ×2 ;
  estimation joueur 24 h 48. L'histoire arrive plus tôt (5 h 51 et 6 h 04 contre 6 h 29) avec autant de défaites ou
  moins ; le temps total varie de ±2 h d'une partie à l'autre.

- **Mécaniques des gardiens de l'Expédition** (2026-10-26) : dès la région 3, une mécanique de boss selon les types
  (`procgen.cpp`, `genBoss`). Mesuré (BRUMEVAL_SIMULATIONS=4, `BRUMEVAL_SIM_FILTRE=gion,avec` ; IA / avec équipement,
  avant → après) : comme dans l'histoire, une posture coûte un tour d'attaque au gardien : sans réglage, les cinq gardiens
  à posture (région 3 graines 11 et 22, région 8 graines 11, 22, 33) passaient de 61 / 89 % à 65 / 97 % en moyenne ;
  avec **+1 niveau pour une posture** (retenu) : 61 / 94 %. Détail (avant → après) : région 3 graine 11 (cuirasse)
  78 → 81 / 100 → 100, graine 22 (voile) 96 → 91 / 100 → 100 ; région 8 graine 11 (voile) 13 → 13 / 56 → 75, graine 22
  (cuirasse) 21 → 28 / 90 → 95, graine 33 (cuirasse) 95 → 91 / 100 → 100. Les ronces ne changent rien (elles remplacent
  les zones rouges au hasard ; niveau inchangé) : région 3 graine 33 35 → 41 / 93 → 86, région 5 graines 11, 22, 33 :
  8 → 11 / 33 → 23, 60 → 50 / 88 → 95, 1 → 1 / 11 → 16. Aucune charge parmi les graines mesurées. Les lignes « à 2, 3, 4 »
  (gardien à plusieurs) jouent sans mécanique, comme en jeu.
- **Mécaniques des boss, actes 2 à 4** (2026-10-26) : attire du Kraken (nouveau genre : une colonne, ceux qui y restent
  sont tirés jusqu'au front), voile d'un type de la Brume Noire (seule la Lumière passe), et les autres reprises par leurs
  paramètres (Griffon tempête et Givrecorne : charge ; Seigneur des Orages et Phénix d'écho : zone qui reste, foudre et
  braises ; Colosse d'écho : cuirasse ; Ombre du Premier Gardien : voile). Mesuré ×2, graines 2026 puis 7 (avant → après ;
  IA / tactiques / boutiques / forgé) : Givrecorne 22 → 26 et 22 → 33 / 43 → 41 et 40 → 35 / 68 → 63 et 67 → 73 / 83 → 83 et
  86 → 90 %, Kraken 95 → 96 et 95 → 98 / – / 100 → 98 et 100 → 100 / 100 → 100 et 100 → 100, Griffon 38 → 30 et 33 → 31 / – /
  63 → 58 et 51 → 68 / 75 → 70 et 73 → 68, Seigneur 36 → 43 et 41 → 36 / – / 93 → 83 et 91 → 90 / 91 → 91 et 95 → 85, Phénix
  (forgé) 90 → 90 et 90 → 95, Colosse et Ombre (forgé, graine 2026) 100 → 100 : les zones restent dans le bruit (±7 à ×2),
  elles remplacent les zones rouges au hasard. Brume Noire (nouvelle ligne, équipe N.64 ; « avant » = sa mécanique jamais
  due, `tous` 1000) : sans voile 22 / 72 / 82 et 35 / 82 / 92 % (IA / boutiques / forgé), avec (N.68) 70 / 97 / 87 et
  60 / 85 / 95 : le voile lui coûte un tour d'attaque sur trois et l'IA alliée se rabat sur les Aubespectres. N.71 : 45 / 75 /
  92 et 42 / 75 / 92 ; N.72 : 25 / 77 / 75 et 37 / 65 / 90 ; N.73 : 40 / 70 / 77 (graine 2026). Retenu : **Brume Noire N.72**
  (moyenne 31 / 71 / 83 contre 29 / 77 / 87 avant). Colosse et Ombre gardés tels quels (100 % avant comme après, équipe
  forgée au niveau de l'acte 4 : la posture ne se voit pas dans ces mesures).

- **Mécaniques des boss** (2026-10-25, « dur mais très tactique ») : cuirasse du Golem de suie (un tour sur trois, magie
  conseillée), voile de la Mère des Brumes (seul le corps à corps passe), ronces de la Ronce-Mère (zone rouge, terrain qui
  reste), charge d'Ignarok (une ligne, recul). Une posture coûte un tour d'attaque au boss : sans réglage, Golem (N.26) 31 → 46
  / 48 → 61 / 51 → 61 % et Mère des Brumes (N.40) 3 → 20 / 56 → 87 / 67 → 90 % (IA / boutiques / forgé, ×4, graine 7) ; les
  zones (ronces, charge) ne changent presque rien (elles remplacent les zones rouges au hasard). Golem N.27 : 53 / 57 / 53 ;
  Mère N.42 : 14 / 70 / 83, N.44 : 1 / 44 / 57. Retenu : **Golem N.28, Mère des Brumes N.43**. Mesuré (avant → après ; IA /
  tactiques / boutiques / forgé) : ×4 graine 7 : Golem 31 → 48 / – / 48 → 48 / 51 → 47 %, Ronce-Mère 11 → 9 / – / 60 → 45 /
  50 → 50, Ignarok 16 → 18 / 13 → 18 / 60 → 59 / 71 → 66, Mère des Brumes 3 → 5 / 0 → 0 / 56 → 60 / 67 → 69 ; ×2 graine 2026 :
  Golem 35 → 35 / – / 45 → 42 / 42 → 40, Ronce-Mère 6 → 8 / – / 36 → 43 / 53 → 41, Ignarok 16 → 22 / 21 → 11 / 61 → 61 /
  76 → 67, Mère des Brumes 6 → 6 / 0 → 0 / 45 → 66 / 68 → 68 ; les autres boss dans le bruit (±12 à ×2). L'IA alliée évite le
  corps à corps sur un ennemi en garde (×0,4) et les coups qu'une posture arrête ; elle ne se met pas en garde contre la
  charge (le joueur peut riposter). Partie automatique complète avec ces mécaniques (bot29, ancien pilote, sans mémoire) :
  quatre actes en 7 h 23 (boss5 3:15, aube_fin 5:05), 29 défaites dans l'histoire dont la Mère des Brumes une fois (battue
  au deuxième essai), Brume Noire deux fois et le Colosse d'écho trois fois ; Golem, Ronce-Mère et Ignarok battus du
  premier coup. L'histoire reste faisable.
  État complet ensuite (bot30 : nouveau pilote et mécaniques de l'acte 1, sans mémoire) : **tout fini en 13 h 53, 13 défaites**,
  niveau 106, quatre actes en 7 h 26 (boss5 3:46, aube_fin 5:24, echos_fin 7:26) avec 9 défaites : Seigneur des Orages, Brume
  Noire (encore N.68), Phénix d'écho ×2, meutes de la Citadelle ×2, du Gouffre et du Sanctuaire ; après la fin : Vieux Phare ×3,
  Abîme. Estimation joueur 27 h (bot28, 3.7 : 13 h 55, 23 défaites). Simulations de l'état fusionné (2026-10-26, ×4) : Golem
  31 / 46 / 53 % (IA / boutique / forgé), Phénix d'écho forgé 80 %. Avec les mécaniques des actes 2 à 4 (bot31, même pilote) :
  **tout fini en 10 h 22, 10 défaites**, niveau 107, quatre actes en 6 h 29 (boss5 3:01, aube_fin 4:13, echos_fin 6:29), aucun
  boss de l'histoire perdu (défaites : le Braconnier au tout début, deux dresseurs en revanche, l'Ombre des galeries, une
  meute de la Citadelle, cinq au Vieux Phare) ; estimation joueur 20 h 40. D'une partie à l'autre l'écart reste grand.

- **IA plus tactique** (2026-10-25) : les ennemis évitent le corps à corps sur un allié en garde, un protecteur va
  couvrir un blessé des siens, un soigneur reste à l'arrière. Mesuré (BRUMEVAL_SIMULATIONS=2, même graine, avant →
  après ; IA / tactiques / boutiques / forgé) : Sylvarque 11 → 16 / 13 → 16 / 48 → 67 / 62 → 48 %, Golem 25 → 22 / – /
  47 → 55 / 52 → 45, Ronce-Mère 8 → 8 / – / 35 → 46 / 31 → 51, Ignarok 22 → 12 / 15 → 18 / 60 → 61 / 66 → 62,
  Givrecorne 23 → 16 / 33 → 31 / 66 → 78 / 82 → 83, cristal 76 → 66 / 73 → 71 / 100 / 100, Mère des Brumes 1 → 5 / 0 →
  0 / 50 → 51 / 73 → 73, Griffon 31 → 33 / – / 70 → 65 / 75 → 71, Seigneur 45 → 35 / – / 85 → 85 / 83 → 88 ; le reste à
  100 % (Orage ancien 95) : aucun boss ne perd plus de 10 points, les écarts vont dans les deux sens (bruit ±7). Une
  première mesure à ×1 donnait cristal (tactiques) 93 → 66 : refaite à ×2, 73 → 71 (bruit).

- **Joueur automatique plus malin** (2026-10-25, sans changer ni créatures ni boss ni rencontres ; détail : compétence
  `joueur-auto-brumeval`, « Plus malin ») : toute rencontre sauvage à sa mesure répétée (sur le champ de bataille du
  vrai combat) et fuie sous 80 %, fuite en plein combat plus tôt, compagnons revus à chaque carte et devant chaque boss
  (gardés si la répétition le confirme), combats de la Ligue répétés en entier, auberge quand les PM manquent, Soigneur
  à 55 %. Sans mémoire, une partie chacune : avant 16 h 19, 24 défaites, quatre actes en 7 h 47 (référence 13 h 55, 23,
  7 h 27) ; après (quatre parties de versions successives) 14 h 00 / 9 / 5 h 43, 15 h 31 / 18 / 7 h 50, 13 h 14 / 7 /
  6 h 29 et, état final, 14 h 52 / 12 / 6 h 23. Deux fois moins de défaites : la difficulté mesurée par le joueur
  automatique baisse sans que le jeu change ; les mesures d'avant (bot14 à bot27) se comparent à « avant ».
  Points durs restants : Phénix d'écho et Ombre du Gardien (perdus une à deux fois par partie malgré la répétition),
  meutes de 3-4 du Sanctuaire des Échos et du Vieux Phare.
- **Dur mais tactique** (2026-10-24, branche `jeu-tactique`, « un jeu dur mais très tactique ») : garde (« Attendre » :
  dégâts × 0,75 et riposte à chaque coup au corps à corps (`garde_riposte` 1, ×0,7) jusqu'au tour suivant), intentions
  des ennemis (annoncées à la moitié de la jauge (70 % d'abord), exécutées si elles tiennent encore ; les boss aussi, sauf zone rouge en
  attente), ciblage `lure` (soigneurs, lanceurs, blessés attirent ; +0,75 par intention déjà sur l'allié : la meute
  s'acharne), `dangerAt` (l'IA alliée s'écarte d'un corps à corps annoncé). Puis, repris d'un agent : les profils de tactiques soignent tous (un seul soigneur faisait
  perdre les meutes de fin de partie), et les créatures sauvages n'ont plus d'attaque sur toute l'équipe
  (`outils/villes/meutes.py` : zone d'un de leurs types à la place). Mesuré (BRUMEVAL_SIMULATIONS=1, ±10 ; IA / tactiques /
  boutiques / forgé) : Sylvarque 7 / 26 / 57 / 52 %, Golem 25 / – / 75 / 85 %, Ronce-Mère 0 / – / 33 / 53 %, Ignarok
  20 / 10 / 65 / 65 %, Givrecorne 35 / 56 / 85 / 95 %, cristal 93 / 93 / 100 / 100 %, Mère des Brumes 0 / 0 / 56 / 50 %,
  Seigneur 73 / – / 96 / 96 %, Phénix 90 %, Ombre 100 %, Braisaure ancestrale 100 % : les boss ne bougent pas (dans le
  bruit). Meutes du Vieux Phare (3 × N.100 contre N.104) et de l'Abîme (3 × N.96 contre N.100), équipement de Port-Écho :
  100 % en IA, tactiques et profils (avant le script : 70 défaites du joueur automatique en trois parties). Joueur
  automatique sans mémoire (bot14, avant la riposte et les boss qui annoncent) : **tout fini en 8 h 20, 10 défaites,
  niveau 112** (référence tactiques-2 : 7 h 40, 27, 103), quatre actes en 5 h 40 (boss5 2:38, aube_fin 3:31), une seule
  défaite contre un boss (Mère des Brumes N.42, équipe N.46), les autres contre des meutes (Vallée au début, Cendrelys
  et Pétrolin du Vieux Phare à la fin) ; estimation joueur 16 h 46. Les défaites « sans rien à faire » ont disparu. bot15 (riposte, boss qui
  annoncent, mêmes règles) : quatre actes en **4 h 40** (boss5 2:12, aube_fin 3:12), 8 défaites dans l'histoire, puis
  20 de plus au Vieux Phare après la fin (arrêté à 11 h 46 : deux Champignards et un Flammenard contre des Cendrelys qui
  absorbent le Feu, l'équipe ne changeait jamais → `Pilot::adaptTeam`). bot16 (`par_allie` 0,6 et `ecart_niveau` 3) :
  quatre actes en 6 h 20 (boss5 2:58, aube_fin 5:02), 9 défaites dans l'histoire (quatre à la Citadelle contre des meutes
  de 4), même tail au Phare ; arrêté à 10 h. Adopté : rencontres à 0,6 par allié et créatures sauvages au plus 3 niveaux
  sous l'équipe (histoire un peu plus longue et plus dense ; plus d'intentions à lire). bot17 (adaptTeam, zones ≥ 80 à
  mi-fourchette, règles 0,5/4) : tout fini en 12 h 23, 16 défaites, N.118, quatre actes en 6 h 21 ; estimation joueur
  24 h. Point dur de l'histoire dans toutes les parties : la **Citadelle des gardiens** (meutes de 4 Aubespectres,
  Nacrelins, Rocaubes et Loups d'aube au niveau de l'équipe, qui résistent au physique : 4 à 6 défaites par partie, une
  seule ailleurs) → `meute_max` 3 sur sa zone, comme l'Abîme et le Phare. bot18 (règles 0,6/3 + adaptTeam, sans ce
  plafond) : quatre actes en 6 h 48 (boss5 2:57, aube_fin 5:12), 12 défaites dans l'histoire dont 6 à la Citadelle, puis
  5 au Phare (arrêté à 9 h 40, niveau 111). **bot19 (état final : règles 0,6/3, plafond Citadelle, intentions à mi-jauge,
  compagnons adaptés) : tout fini en 9 h 07, 11 défaites, niveau 105** ; quatre actes en 5 h 28 (boss5 2:28, aube_fin 3:34),
  4 défaites dans l'histoire (0 à la Citadelle), 7 après la fin (Abîme, Phare, dont l'Orage ancien et le Veilleur) ;
  estimation joueur 18 h 22. bot20 (+ capture des types absents du vivier) : tout fini en 14 h 26, 17 défaites, quatre
  actes en 7 h 21, 7 captures et 30 changements de compagnons (contre 9 pour bot17) : plus lent et pas moins de défaites
  au Phare (les captures sont de bas niveau, l'expérience se disperse) → capture étendue retirée ; adaptTeam gardé.
  Débuts : chaque partie perdait dans ses dix premières minutes (trois créatures contre deux combattants de niveau 5) →
  `rencontres.meute_par_niveau` 10 : une meute fait au plus 2 + une créature par tranche de 10 niveaux de l'équipe
  (bot22 : plus de défaite contre une meute sauvage dans les dix premières minutes ; mais 17 h 07 et 29 défaites au
  total, niveau 121 : le Phénix d'écho perdu quatre fois de suite à +3..+5 niveaux après une répétition « sûre » jouée
  depuis la ville, sans la canicule du Cratère → répétition dans le décor du lieu du combat ; d'une partie à l'autre,
  l'écart va de 9 h / 11 défaites (bot19) à 17 h / 29 : une seule partie ne suffit pas pour juger un réglage).
  bot23 (état final : répétition dans le décor, meutes par niveau) : tout fini en 12 h 17, 15 défaites, niveau 123,
  quatre actes en 6 h 07, estimation joueur 24 h ; le Phénix n'est plus perdu. Sur quatre parties du même état :
  9 h à 17 h, 11 à 30 défaites, quatre actes en 5 h 30 à 7 h ; les défaites de l'histoire sont la Citadelle (Brume
  Noire), le Cratère (Phénix) et des meutes d'esprits et de carapaces ; après la fin, le Vieux Phare. bot24 (même état
  que bot23) : quatre actes en 5 h 18 avec 10 défaites, puis 29 de plus au Vieux Phare (arrêté à 17 h 18, niveau 134) :
  la traîne d'après la fin vient de l'équipe jamais changée (3 ou 4 créatures captées en tout), pas de l'histoire.
  **Coquille trouvée le 2026-10-24** : la Vitesse de base de Braisenard était 200 (Flammenard 220) depuis la refonte du
  combat du 6 octobre (60 avant ; la deuxième espèce la plus rapide est à 90, la médiane à 56) : le compagnon de départ
  par défaut, celui de toutes les parties automatiques, jouait trois à quatre fois plus souvent que les autres. Remis à
  60 / 66 (×1,1). Toutes les mesures d'avant sont faites avec ce Braisenard-là ; celles d'après sont plus dures.
  Mesuré aussitôt (BRUMEVAL_SIMULATIONS=1 ; boutiques / forgé) : Sylvarque 17 / 25 %, Ronce-Mère 6 / 10, Golem 35 / 30,
  Ignarok 40 / 32, Givrecorne 57 / 67, Mère des Brumes 33 / 30 — contre 57 / 60, 43 / 53, 75 / 85, 65 / 67, 85 / 92,
  56 / 56 juste avant : les boss avaient été réglés contre un Braisenard quatre fois trop rapide. Boss ramenés
  (`evenements.json`) : Sylvarque N.13 ×5,5, Golem N.26 ×12 bouclier 10, Ignarok N.24 ×7,5, Ronce-Mère N.20 ×7, Givrecorne
  N.37 ×10, Mère des Brumes N.40 ×6,5. Mesuré après (deux tours) : Sylvarque 65 / 45 %, Ronce-Mère 43 / 36, Golem 50 / 60,
  Ignarok 62 / 90, Givrecorne 52 / 75, Mère des Brumes 46 / 66, cristal 100, Seigneur 90 / 100 : proche d'avant, un peu
  plus dur au milieu (Givrecorne, Golem). Joueur automatique, sans mémoire : bot25 (vitesse corrigée, anciens boss) : quatre
  actes en 8 h 29, 13 défaites dans l'histoire (Givrecorne, Seigneur, Brume Noire, Phénix une fois chacun, le reste des
  meutes) ; bot26 (boss ramenés) : quatre actes en 7 h 43, 14 défaites dans l'histoire (aucun boss de l'histoire perdu,
  des meutes de 3 à 5 esprits et carapaces, l'Ombre du Gardien une fois). Contre 5 h 30 à 7 h avec le Braisenard trop
  rapide : le jeu corrigé est nettement plus dur pour le joueur automatique. Meutes adoucies au milieu : une créature de
  plus tous les 15 niveaux au lieu de 10 (`meute_par_niveau` 15 : 4 au plus à N.30, 5 à N.45, 6 à N.60). bot27 (cet état) : tout fini en 16 h 41, 26 défaites
  dont 23 dans l'histoire (quatre actes en 8 h 11), niveau 121, estimation joueur 32 h ; boss perdus : Kraken ×2, Griffon,
  Ignarok de la prime ×2, Brume Noire, Colosse, Phénix, Ombre du Gardien, plus des meutes. Sur trois parties corrigées :
  7 h 43 à 8 h 29 pour les quatre actes, 13 à 23 défaites dans l'histoire. Simulations complètes ensuite (IA / tactiques /
  boutiques / forgé) : Kraken 96 / – / 100 / 100 %, Griffon 30 / – / 43 / 70, Seigneur 53 / 83 / 90, Phénix 70 (équipé),
  Colosse, Ombre, Orage ancien, Veilleur 100 : seuls le Griffon et le Phénix avaient baissé → Griffon N.53 ×13 (N.55 ×15
  avant), Phénix N.86 ×4 (N.87 ×4,5) ; mesuré après : Griffon 43 / 66 / 73 %, Phénix 100 %. Braconnier 33 % (IA, N.6 :
  le premier dresseur, évitable), Chef des bandits 26 % (IA), défi « Les ailes » 10 %. bot28 (état publié en **3.7**,
  2026-10-24) : tout fini en 13 h 55, 23 défaites, niveau 120, quatre actes en 7 h 27, estimation joueur 27 h.
- **Rééquilibrage « trop facile »** (2026-10-23, demandé par l'utilisateur, comparé à Expedition 33) : le joueur automatique
  arrivait aux boss du milieu de l'histoire avec 15 à 20 niveaux d'avance (Givrecorne N.39 au niveau 58, Mère des Brumes
  N.44 au niveau 61) et ~2 créatures par rencontre contre jusqu'à 6 alliés. Réglages (`regles.json`) : `par_allie` 0,5
  (ennemis par allié), `ecart_niveau` 4 / `plafond_niveau` 6 (créatures sauvages relevées, gains de leur niveau d'origine :
  `rewardLvl`), `exp_ecart` 0,08 au-delà de `exp_marge` 4 niveaux, au moins `exp_min` 0,25 ; résistances des créatures
  sauvages (`outils/villes/resistances.py`, absorption = résistance négative). Joueur automatique (une partie chacune,
  avec mémoire) : avant 6 h 37, 17 défaites, N.119 ; rencontres seules 5 h 57, 13, N.127 (plus d'expérience : pire) ;
  + résistances 7 h 00, 9, N.130 ; + exp 12 %/2 niveaux : boss à +1/+5 niveaux mais 46 défaites et fin non atteinte en
  10 h (saut de 20 niveaux entre l'acte 3, Citadelle 60-64, et l'acte 4, 84-90) ; réglage final : **7 h 38, 21 défaites,
  N.102**, boss affrontés de −5 à +6 niveaux (Givrecorne 45, Mère des Brumes 49, Kraken 57, Ombre 90, Veilleur 105),
  défaites dans l'Abîme et le Vieux Phare (contenu facultatif). Limite du joueur automatique : 14 h de jeu.
  Avec les embuscades (0,07, un tiers derrière), les profils de tactiques et la répétition systématique des boss : à
  7:18 de jeu, 10 défaites, niveau 90, trois actes finis (Seigneur des Orages 4 h 04, Aube 6 h 11 ; avant : 2 h 29 et
  3 h 39) ; mesure en cours à la publication de la 3.6 ; cette partie a fini à 14 h sans tout faire (50
  défaites, 43 dans l'Abîme et le Phare), comme la vérification GitHub (14 h, 76 défaites). Causes (analyse d'un agent) :
  profil Soigneur sans sorts, Combattant qui impose « le moins de PV », répétition de tous les boss (318 min
  d'entraînement). Corrigé (branche tactiques-2 : action « Attaque, PM gardés », le Combattant n'achève que sous 30 %,
  ready() ne répète que les combats perdus) : **sans mémoire, tout fini en 7 h 40, 27 défaites, niveau 103** (quatre
  actes à 5 h 34). La 3.6 publiée (profils d'avant), même mesure : tout fini mais en 15 h 20, 42 défaites, niveau 128. Embuscades à
  0,1 et la moitié derrière : 48 défaites, fin non atteinte en 14 h (trop dur, retiré). Prime la plus dure : la Braisaure
  ancestrale (trois carapaces : il faut de la magie).

- **Acte 2** (3.0, 2026-10-15) : le Seigneur des Orages battait toutes les équipes (journal : Foudre noire en croix à
  55-90 % des PV de trois alliés par tour, ~3000 PV) ; Magie 80 → 68, PV de base 110 → 100, Foudre noire 82 → 70,
  ×10 → ×7,5, renfort réduit au seul Nuageon soigneur. Kraken N.53 ×17, Griffon N.55 ×15. Mesuré
  (BRUMEVAL_SIMULATIONS=2, avant le dernier renfort du Kraken et du Griffon ; IA / boutiques / forgé) : Kraken 91 / 100
  / 100 %, Griffon 90 / 100 / 100 %, Seigneur 60 / 91 / 90 %. Joueur automatique : histoire (actes 1 et 2) finie en
  1 h 25, 2 défaites, niveau 61.

- **Rôles, phases, gemmes** (2.7, 2026-10-14) : les simulations « équipées » sertissent aussi les gemmes des
  boutiques de la région (`equipParty`). Premier essai trop dur (Ignarok 2 / 8 / 8 %, Mère des Brumes 3 / 5 / 6 %) :
  la canicule de la phase d'Ignarok annulait l'avantage de l'Eau (remplacée par une carapace reformée, bouclier 10),
  la Mère gagnait un Brumeloup chef en renfort (retiré), la Ronce-Mère deux soigneurs (un seul). Ignarok ×8, chef des
  bandits ×1,7, Mère des Brumes ×7,5. Mesuré (BRUMEVAL_SIMULATIONS=2 ; IA sans équipement / boutiques et gemmes /
  forgé) : Sylvarque 5 / 43 / 47 %, Golem 30 / 60 / 60 %, Ronce-Mère 6 / 51 / 58 %, Ignarok 8 / 43 / 56 %,
  Givrecorne 48 / 80 / 92 %, cristal 25 / 96 / 98 %, Mère des Brumes 1 / 26 / 55 %, chef des bandits 1 % (IA).
  Sans équipement on perd presque toujours : s'équiper, sertir et viser soigneurs et faiblesses est indispensable.

- **Pousser, météo, terrain, duos** (2.6, 2026-10-14) : les ennemis poussent et tirent aussi, la canicule de
  Cendrelune renforce Ignarok ; les duos renforcent l'équipe. L'IA dépensait les PM de Maëlle dans des duos (Sylvarque
  forgé 47 → 17 %) : un duo n'est choisi par l'IA ou les tactiques que si le partenaire garde 40 % de ses PM.
  Sylvarque ×7,2 → ×6,5, Ronce-Mère ×9,5 → ×8,5. Mesuré (BRUMEVAL_SIMULATIONS=2 ; IA / boutiques / forgé) :
  Sylvarque 3 / 31 / 27 %, Golem 45 / 57 / 50 %, Ronce-Mère 18 / 56 / 71 %, Ignarok 23 / 55 / 61 %, Givrecorne
  52 / 70 / 78 %, cristal 51 / 93 / 91 %, Mère des Brumes 11 / 46 / 65 % (proche de la 2.5). Météo et terrain sont
  tirés au hasard : ±10 points d'une mesure à l'autre.

- **Brisure et talents** (2.5, 2026-10-14, « plus dur mais que la stratégie soit récompensée ») : sans réglage, les
  boss devenaient trop faciles (brisés, ils perdent des tours : Ronce 19 → 53 %, Ignarok 11 → 52 %) sauf la Mère des
  Brumes (0 % : Intimidation du Brumeloup, Esprit clair). Réglages (`evenements.json` : niveau, `pv`, `bouclier`) :
  Golem N.28 ×16 bouclier 12, Ignarok N.24 ×9, Ronce-Mère N.21 ×9,5, Givrecorne N.39 ×11, gardien de cristal N.39 ×14
  bouclier 10, Mère des Brumes N.40 ×8. Mesuré (BRUMEVAL_SIMULATIONS=2 ; IA / boutiques / forgé) : Sylvarque 3 / 30 /
  47 %, Golem 50 / 57 / 75 %, Ronce-Mère 23 / 63 / 66 %, Ignarok 33 / 52 / 67 %, Givrecorne 33 / 78 / 81 %, cristal
  53 / 86 / 96 %, Mère des Brumes 11 / 43 / 63 %. L'équipement et la forge comptent beaucoup plus qu'avant.
  Le bouclier d'un boss (8 par défaut) règle combien de faiblesses il faut pour le briser : un gros bouclier rend la
  Brisure rare, donc le boss plus dur sans toucher à ses PV.

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
