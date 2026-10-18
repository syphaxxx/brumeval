# Sprites en voxels (essai pour un Brumeval en 3D)

Cet outil transforme les sprites du jeu en **modèles 3D**, pour essayer à quoi
ressemblerait Brumeval en 3D avec Godot. Il ne change rien au jeu actuel : il
lit seulement les planches d'origine (`outils/sprites/planches/`) et l'atlas
du jeu (`data/sprites.png`).

Les sprites du jeu sont petits (Lior : 24 pixels de haut) : en 3D, ils donnent
de gros cubes où l'on ne reconnaît plus le visage. Par défaut, l'outil repart
donc des **planches d'origine**, bien plus détaillées, et redécoupe chaque
sprite **trois fois plus grand** (mêmes boîtes et mêmes recolorations que
`outils/sprites/decoupe.py`). Le modèle garde la même taille dans le monde,
mais avec des voxels trois fois plus petits : yeux, mèches, flammes et
feuilles sont visibles.

Chaque pixel devient une colonne de petits cubes (des « voxels ») de sa couleur.
Les colonnes sont fines sur les bords du dessin et plus épaisses au milieu :
Lior, les créatures et les arbres ont du volume au lieu d'être des planches
découpées. Le contour sombre du dessin reste sur le devant et le derrière ; les
côtés prennent la couleur de l'intérieur.

## Fabriquer les modèles

Il faut Python avec Pillow, numpy et scipy (les mêmes que pour
`outils/sprites/decoupe.py`) :

```bash
pip install pillow numpy scipy
python outils/voxels/voxelise.py                 # tous les sprites (environ 5 minutes)
python outils/voxels/voxelise.py lior ronceau    # seulement ceux dont le nom commence par lior ou ronceau
python outils/voxels/voxelise.py --detail 1      # les petits sprites du jeu : 1 minute, mais moins fin
```

Le script écrit :

- un fichier `.glb` par sprite dans `outils/voxels/essai_godot/modeles/` ;
- `outils/voxels/apercu.png` : tous les modèles vus de trois quarts, pour
  vérifier d'un coup d'œil.

Ces fichiers ne sont pas envoyés sur GitHub (ils se refabriquent en une
commande) : il faut lancer le script une fois sur chaque ordinateur.

Les textures de sol et de murs (`sol_…`, `tex_…`) et les portraits des
dialogues (`lior_visage_…`) sont ignorés, sauf si on les demande par leur nom.

### Réglages

| Option | Effet |
|---|---|
| `--detail 3` | Finesse : nombre de voxels par pixel du jeu, en largeur comme en hauteur (3 par défaut ; 1 = les sprites du jeu ; 4 n'apporte presque rien de plus et pèse deux fois plus) |
| `--gonflement 0.6` | Épaisseur gagnée en s'éloignant du bord : plus grand = plus rond et plus épais |
| `--epaisseur-max 12` | Épaisseur maximale, en pixels du jeu |
| `--plat` | Même épaisseur partout (une planche découpée), utile pour un panneau ou une bannière |
| `--sortie dossier` | Écrire les `.glb` ailleurs (par exemple dans un autre projet Godot) |
| `--sans-apercu` | Ne pas refaire `apercu.png` (un peu plus rapide) |

## Voir les modèles dans Godot

1. Installer **Godot 4** (version standard, pas .NET) : https://godotengine.org
2. Lancer `voxelise.py` (ci-dessus).
3. Dans Godot : **Importer**, choisir `outils/voxels/essai_godot/project.godot`.
   La première ouverture prend une minute ou deux : Godot prépare les 460 modèles.
4. Appuyer sur **F5** (lancer).

La galerie range tous les modèles sur un pré, sous un peu de brume :

| Commande | Action |
|---|---|
| Flèches ou ZQSD | Se déplacer |
| Molette, ou A / E | Zoomer, dézoomer |
| Clic droit maintenu + souris | Tourner autour |

## Utiliser un modèle dans un autre projet Godot

Copier le `.glb` dans le projet, puis le glisser dans une scène. Repères :

- 1 pixel du jeu = 1/16 de mètre (Lior mesure 1,5 m), quelle que soit la finesse ;
- l'origine est au milieu des pieds : le modèle se pose directement sur le sol ;
- le modèle regarde vers +Z, c'est-à-dire vers la caméra par défaut de Godot ;
- les couleurs sont dans le modèle (couleurs de sommets) : pas de texture à copier.

## Limites de cette première version

- C'est lourd en finesse 3 : environ 16 000 triangles par modèle en moyenne,
  soit 7 millions de triangles et 500 Mo pour les 460. Pour un vrai jeu, on ne
  fabriquerait que les modèles utiles. Si la galerie rame sur un petit
  ordinateur, ne fabriquer qu'une partie des modèles (`voxelise.py lior
  ronceau…`) ou baisser la finesse (`--detail 2`).
- Les modèles sont symétriques devant/derrière : le dos d'un personnage montre
  son devant. Pour un vrai dos, il faudrait se servir des sprites `_haut_`
  (vus de dos) : une amélioration possible.
- Les animations sont des modèles séparés (`lior_bas_0`, `lior_bas_1`…). Pour
  animer, on affiche l'un après l'autre, comme les images d'un sprite.
- Les maisons sont dessinées en perspective dans les sprites : en voxels elles
  restent des façades épaisses, pas de vraies maisons qu'on peut contourner.
