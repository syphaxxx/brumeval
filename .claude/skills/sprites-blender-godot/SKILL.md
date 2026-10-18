---
name: sprites-blender-godot
description: "Fabriquer des sprites et des animations de pixel art avec Blender (personnages 3D rendus en sprites : poses, directions, marche) et Godot (effets en particules rendus image par image), sans fenêtre, puis les ranger dans l'atlas de Brumeval. À utiliser pour une nouvelle pose, un personnage, un effet animé, ou pour un autre jeu en pixel art."
---

# Sprites et animations avec Blender et Godot

Installés le 2026-10-18 avec winget (`BlenderFoundation.Blender`, `GodotEngine.GodotEngine`) :

- Blender 5.2 : `"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe"`
- Godot 4.7 : `%LOCALAPPDATA%\Microsoft\WinGet\Packages\GodotEngine.GodotEngine_Microsoft.Winget.Source_8wekyb3d8bbwe\Godot_v4.7.2-stable_win64_console.exe`
  (la version `_console` écrit dans le terminal ; l'alias `godot` n'existe pas sans droits d'administrateur)

## Chaîne de Brumeval

```bash
# Personnages (Blender) : fiche dans outils/blender/personnages.json
"C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup -P outils/blender/personnages.py -- [ids…]
"…/blender.exe" -b --factory-startup -P outils/blender/personnages.py -- paysan --grand   # contrôle : grand, une image par direction
python outils/blender/pixeliser.py [ids…]        # → outils/sprites/rendus/<id>_haut_k.png, <id>_droite_k.png

# Effets (Godot) : une gerbe dans outils/godot/effets/rendu.gd (fonction _effet)
"$GODOT" --path outils/godot/effets --script res://rendu.gd --fixed-fps 16 -- [types…]
python outils/godot/pixeliser_effets.py [types…]  # → outils/sprites/rendus/gfx_<type>_k.png (UTILISES par défaut)

python outils/sprites/decoupe.py                  # l'atlas data/sprites.png + .json (planches + rendus/)
```

Puis le mode test (captures `habitants_poses`, `echiquier_percee`) et regarder les images.

- Le jeu trouve les poses par leur nom : `<sprite>_<bas|haut|droite|gauche>_<0..3>` (gauche = droite retournée ;
  0 et 2 au repos, 1 et 3 un pas). La face dessinée par l'utilisateur (`_bas_0`) est gardée : pixeliser.py n'écrit
  que le dos et le profil, pour ne pas mêler deux styles en marchant vers le bas (`--face` sinon).
- Les effets Godot servent là où la planche de l'utilisateur n'a rien (Métal, Normal) et pour les nouveautés
  (Percée, Brisure : `Battle::bursts_`). Ne pas remplacer ses dessins sans le lui demander.
- Tailles : l'atlas est deux fois plus fin que le jeu (`_echelle` 2). Personnage : 48 px de haut dans l'atlas (24 en
  jeu ; enfant 44). Effet : toile de 80 × 96 (40 × 48 en jeu), centre de la cible à 62 % de la hauteur.

## Style « pixel art » à partir de la 3D (ce qui a marché)

1. Formes simples assemblées en script (sphères, cylindres, cubes) : chibi = tête énorme (rayon 1,15 pour 5 unités
   de haut), gros yeux sombres avec un reflet, frange taillée par un booléen.
2. Matière « dessin animé » : Diffuse → Shader to RGB → ColorRamp CONSTANT (ombre 0,55, lumière 1) → Mix MULTIPLY avec
   la couleur → Emission. Deux tons nets, aucune brillance. Couleurs prises sur le sprite dessiné (sRGB → linéaire).
3. Caméra orthographique, 14° au-dessus de l'horizon ; rendu 4 fois plus grand que le sprite final, fond transparent.
4. Réduction avec `decoupe.reduire` (couleur dominante de chaque bloc, les sombres pèsent plus), puis contour d'un
   pixel au bord de la silhouette (couleur × 0,32) : c'est ce contour qui donne l'air « dessiné ».
5. Toutes les images d'un personnage dans la même boîte (pieds sur la même ligne), échelle d'après la face au repos.

## Pièges rencontrés

- **Blender** : un objet parenté juste après sa création perd sa taille (`matrix_world` pas encore recalculée) :
  `bpy.context.view_layer.update()` avant de copier la matrice. `read_factory_settings` détruit les matières :
  vider tout cache de matières à chaque nouvelle scène. Une nouvelle lineset Freestyle n'a pas de `linestyle`
  (le créer). Freestyle est **trop épais** une fois réduit à 48 px (tout devient noir) : le contour se fait après.
  `Material.use_nodes` est annoncé obsolète (Blender 6) : simple avertissement.
- **Godot** : `--headless` ne dessine rien ; il faut le vrai rendu (`gl_compatibility`) avec une petite fenêtre sans
  bord poussée hors écran. Capturer une `SubViewport` (`transparent_bg`) après `await RenderingServer.frame_post_draw`.
  `--fixed-fps 16` : chaque image avance d'1/16 s quelle que soit la machine ; `use_fixed_seed` + `seed` sur chaque
  `CPUParticles2D` : même résultat à chaque fois. Un `Gradient.new()` a déjà deux points (noir, blanc) : affecter
  `offsets` et `colors` d'un coup au lieu d'`add_point`. Particules nettes : texture minuscule faite en code (carré,
  disque, croix) et filtre `NEAREST`.
- Rendus bruts (`outils/blender/rendus/`, `outils/godot/effets/rendus/`) : non suivis par git, ils se refont en une
  commande. Les sprites finaux (`outils/sprites/rendus/`) sont suivis : ce sont des sources de l'atlas.

## Pour un autre jeu

Même méthode : fiche de personnage en JSON → script Blender → réduction + contour → atlas. Les scripts de Brumeval
(`outils/blender/`, `outils/godot/`) se copient tels quels ; seuls les tailles, la caméra (vue de dessus ou de
côté) et la palette changent. Toujours comparer côte à côte avec les sprites existants avant d'intégrer.
