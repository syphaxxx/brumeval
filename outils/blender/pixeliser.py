"""Réduit les rendus de Blender (outils/blender/rendus/) en sprites du jeu (outils/sprites/rendus/).

    python outils/blender/pixeliser.py [noms…]

Pour chaque personnage de personnages.json : toutes ses images sont recadrées dans la même boîte (les pieds restent
sur la même ligne d'une image à l'autre), puis réduites à sa hauteur dans l'atlas (« hauteur », en pixels de l'atlas,
deux fois ceux du jeu) avec la réduction de decoupe.py (couleur dominante de chaque bloc, contours et yeux gardés).
Ensuite : python outils/sprites/decoupe.py, qui range aussi outils/sprites/rendus/*.png dans l'atlas.
Un contour d'un pixel est tracé au bord de la silhouette. La face dessinée à la main (<id>_bas_0) est gardée : seuls le dos (haut) et le profil (droite ; la gauche est son
miroir) sont écrits, pour ne pas mêler deux styles en marchant vers le bas (option --face : la face aussi).
"""
import json
import os
import sys

import numpy as np
from PIL import Image

ICI = os.path.dirname(os.path.abspath(__file__))
RACINE = os.path.dirname(os.path.dirname(ICI))
sys.path.insert(0, os.path.join(RACINE, 'outils', 'sprites'))
import decoupe  # noqa: E402  (réduction des planches)

RENDUS = os.path.join(ICI, 'rendus')
SORTIE = os.path.join(RACINE, 'outils', 'sprites', 'rendus')


def contour(px):
    """Contour sombre d'un pixel, comme les sprites dessinés : chaque pixel du bord de la silhouette prend sa couleur
    très assombrie (teinte gardée), et les pieds aussi."""
    a = px[..., 3] > 0
    bord = a.copy()
    inner = a.copy()
    inner[1:] &= a[:-1]
    inner[:-1] &= a[1:]
    inner[:, 1:] &= a[:, :-1]
    inner[:, :-1] &= a[:, 1:]
    bord &= ~inner
    out = px.copy()
    out[bord, :3] = (px[bord, :3] * .32 + np.array([12, 8, 10])).astype(np.uint8)
    return out


def main():
    args = sys.argv[1:]
    face = '--face' in args
    args = [a for a in args if not a.startswith('--')]
    os.makedirs(SORTIE, exist_ok=True)
    for p in json.load(open(os.path.join(ICI, 'personnages.json'), encoding='utf-8')):
        if args and p['id'] not in args:
            continue
        noms = sorted(f[:-4] for f in os.listdir(RENDUS) if f.startswith(p['id'] + '_') and f.endswith('.png'))
        imgs = {n: np.asarray(Image.open(os.path.join(RENDUS, n + '.png')).convert('RGBA')).astype(int) for n in noms}
        if not imgs:
            print('aucun rendu :', p['id'])
            continue
        # Boîte commune : celle de toutes les images ; échelle d'après la face au repos
        a = np.zeros(next(iter(imgs.values())).shape[:2], bool)
        for im in imgs.values():
            a |= im[..., 3] > 40
        ys, xs = np.nonzero(a)
        x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
        ref = imgs.get(p['id'] + '_bas_0', next(iter(imgs.values())))
        ry = np.nonzero(ref[..., 3] > 40)[0]
        k = p.get('hauteur', 48) / (ry.max() + 1 - ry.min())
        W, H = max(1, round((x1 - x0) * k)), max(1, round((y1 - y0) * k))
        for n, im in imgs.items():
            if n.startswith(p['id'] + '_bas_') and not face:  # la face reste celle dessinée à la main
                continue
            s = im[y0:y1, x0:x1]
            masque = s[..., 3] > 100
            rgb = s[..., :3]
            out = decoupe.reduire(rgb, masque, W, H, 24)
            out = contour(out)
            ys2, xs2 = np.nonzero(out[..., 3] > 0)
            out = out[ys2.min():] if len(ys2) else out  # le haut vide retiré (le jeu pose le sprite sur son bas)
            Image.fromarray(out.astype(np.uint8), 'RGBA').save(os.path.join(SORTIE, n + '.png'))
        print(p['id'], ':', len(imgs), 'images,', W, 'x', H)


if __name__ == '__main__':
    main()
