# Sprites en voxels (essai pour un Brumeval en 3D)

Cet outil transforme les sprites du jeu en **modèles 3D**, pour essayer à quoi
ressemblerait Brumeval en 3D avec Godot. Il ne change rien au jeu actuel : il
lit seulement `data/sprites.png` et `data/sprites.json`.

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
python outils/voxels/voxelise.py                 # tous les sprites (environ 1 minute)
python outils/voxels/voxelise.py lior ronceau    # seulement ceux dont le nom commence par lior ou ronceau
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
| `--gonflement 0.6` | Épaisseur gagnée en s'éloignant du bord : plus grand = plus rond et plus épais |
| `--epaisseur-max 12` | Épaisseur maximale, en voxels |
| `--plat` | Même épaisseur partout (une planche découpée), utile pour un panneau ou une bannière |
| `--sortie dossier` | Écrire les `.glb` ailleurs (par exemple dans un autre projet Godot) |
| `--sans-apercu` | Ne pas refaire `apercu.png` (un peu plus rapide) |

## Voir les modèles dans Godot

1. Installer **Godot 4** (version standard, pas .NET) : https://godotengine.org
2. Lancer `voxelise.py` (ci-dessus).
3. Dans Godot : **Importer**, choisir `outils/voxels/essai_godot/project.godot`.
   La première ouverture prend un moment : Godot prépare les 460 modèles.
4. Appuyer sur **F5** (lancer).

La galerie range tous les modèles sur un pré, sous un peu de brume :

| Commande | Action |
|---|---|
| Flèches ou ZQSD | Se déplacer |
| Molette, ou A / E | Zoomer, dézoomer |
| Clic droit maintenu + souris | Tourner autour |

## Utiliser un modèle dans un autre projet Godot

Copier le `.glb` dans le projet, puis le glisser dans une scène. Repères :

- 1 pixel du sprite = 1/16 de mètre (Lior, 24 pixels de haut, mesure 1,5 m) ;
- l'origine est au milieu des pieds : le modèle se pose directement sur le sol ;
- le modèle regarde vers +Z, c'est-à-dire vers la caméra par défaut de Godot ;
- les couleurs sont dans le modèle (couleurs de sommets) : pas de texture à copier.

## Limites de cette première version

- Les modèles sont symétriques devant/derrière : le dos d'un personnage montre
  son devant. Pour un vrai dos, il faudrait se servir des sprites `_haut_`
  (vus de dos) : une amélioration possible.
- Les animations sont des modèles séparés (`lior_bas_0`, `lior_bas_1`…). Pour
  animer, on affiche l'un après l'autre, comme les images d'un sprite.
- Les maisons sont dessinées en perspective dans les sprites : en voxels elles
  restent des façades épaisses, pas de vraies maisons qu'on peut contourner.
