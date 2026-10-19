"""Range les effets rendus par Godot (outils/godot/effets/rendus/) dans les sprites du jeu (outils/sprites/rendus/).

    python outils/godot/pixeliser_effets.py           # les effets utilisés par le jeu (UTILISES)
    python outils/godot/pixeliser_effets.py feu glace # d'autres (ajouter leur nom à UTILISES et au jeu : spriteFx)

Chaque image <type>_<k>.png (déjà à la finesse de l'atlas) devient gfx_<type>_<k>.png : palette réduite (24 couleurs),
transparence franche (le jeu dessine en pixels nets), bords vides retirés autour de l'ensemble des images d'un effet
(les images gardent toutes la même taille : l'effet ne tremble pas). Ensuite : python outils/sprites/decoupe.py.
"""
import os
import re

import numpy as np
from PIL import Image

ICI = os.path.dirname(os.path.abspath(__file__))
RACINE = os.path.dirname(os.path.dirname(ICI))
RENDUS = os.path.join(ICI, 'effets', 'rendus')
SORTIE = os.path.join(RACINE, 'outils', 'sprites', 'rendus')
# Effets dessinés par Godot que le jeu utilise (les autres types gardent ceux de la planche de l'utilisateur)
UTILISES = ['metal', 'normal', 'percee', 'brisure', 'poussiere', 'echange', 'cercle']


def main():
    import sys
    voulus = sys.argv[1:] or UTILISES
    os.makedirs(SORTIE, exist_ok=True)
    groupes = {}
    for f in sorted(os.listdir(RENDUS)):
        m = re.match(r'(.+)_(\d+)\.png$', f)
        if m:
            groupes.setdefault(m.group(1), []).append((int(m.group(2)), f))
    for t, imgs in groupes.items():
        if t not in voulus:
            continue
        arr = [np.asarray(Image.open(os.path.join(RENDUS, f)).convert('RGBA')).copy() for _, f in sorted(imgs)]
        for a in arr:
            a[a[..., 3] < 70] = 0          # transparence franche
            a[a[..., 3] > 0, 3] = np.clip(a[a[..., 3] > 0, 3].astype(int) * 3 // 2, 0, 255)
        vis = np.zeros(arr[0].shape[:2], bool)
        for a in arr:
            vis |= a[..., 3] > 0
        if not vis.any():
            continue
        ys, xs = np.nonzero(vis)
        y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
        # même centre horizontal que la scène (le jeu centre l'effet sur la cible) : on rogne autant de chaque côté
        w = arr[0].shape[1]
        m = min(x0, w - x1)
        for k, a in enumerate(arr):
            a = a[:, m:w - m]
            rgb = Image.fromarray(a[..., :3]).quantize(colors=24, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).convert('RGB')
            out = np.dstack([np.asarray(rgb), a[..., 3]])[y0:, :]  # le bas reste fixe (le jeu pose l'effet sur la cible)
            Image.fromarray(out.astype(np.uint8), 'RGBA').save(os.path.join(SORTIE, 'gfx_%s_%d.png' % (t, k)))
        print(t, ':', len(arr), 'images')


if __name__ == '__main__':
    main()
