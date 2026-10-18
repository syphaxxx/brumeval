"""Découpe les planches de sprites (outils/sprites/planches/*.png) et fabrique l'atlas du jeu.

    python outils/sprites/decoupe.py            # écrit data/sprites.png, data/sprites.json et outils/sprites/apercu.png

Les planches sont des images « pixel art » agrandies et un peu floues, sur un fond gris uni. Chaque sprite de
outils/sprites/decoupe.json donne sa planche, sa boîte (en pixels de la planche) et sa taille dans le jeu
(h : hauteur ou w : largeur, en pixels du jeu). L'outil :
  1. retire le fond (et le liseré clair autour des sprites) en partant des bords de la boîte ;
  2. réduit le sprite : chaque pixel du jeu prend la couleur dominante de son bloc (les contours sombres et les yeux
     comptent plus, sinon ils disparaissent) ;
  3. range tous les sprites dans une seule image (atlas).
Options d'un sprite : rot (pivoté de n degrés), cadre ([largeur, hauteur] maximales), seul (garde seulement le plus gros morceau), fragments (garde toutes les particules : effets),
plein (pas de fond à retirer : textures), couleurs (taille de la palette), miroir (retourné gauche-droite).
Les variantes (outils/sprites/variantes.json) recolorent un sprite déjà découpé : {"id", "de", "teintes": [[teinte
min, teinte max, nouvelle teinte, saturation x, luminosité x, saturation minimale], ...]} (teintes en degrés ;
teinte min -1 = toutes les couleurs ; nouvelle teinte -1 = garder ; la saturation minimale colore aussi les gris).
(« miroir » : true la retourne aussi). {"prefixe", "de", "teintes"} recolore tous les sprites d'un personnage (mage_bas_0 -> maelle_bas_0…).
Les images de outils/sprites/rendus/ (déjà à la finesse de l'atlas : rendus de Blender, outils/blender/) sont ajoutées
telles quelles, sous le nom de leur fichier.
Nécessite Pillow, numpy et scipy (pip install pillow numpy scipy).
"""
import colorsys
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageEnhance
from scipy import ndimage as nd

ICI = os.path.dirname(os.path.abspath(__file__))
RACINE = os.path.dirname(os.path.dirname(ICI))
FOND = np.array([217, 222, 228])
# Double définition (3.2.1) : les tailles de decoupe.json sont en pixels du jeu (320x240) ; chaque sprite est fabriqué
# ECHELLE fois plus fin, et le jeu le dessine à sa taille mais à pleine finesse (image rendue à la taille de l'écran)
ECHELLE = 2
_planches = {}


def planche(nom):
    if nom not in _planches:
        _planches[nom] = np.asarray(Image.open(os.path.join(ICI, 'planches', nom + '.png')).convert('RGB')).astype(int)
    return _planches[nom]


def decouper(e):
    """Pixels du sprite (RGB) et masque de ce qui n'est pas du fond, recadrés."""
    a = planche(e['planche'])
    x0, y0, x1, y1 = e['boite']
    if 'part' in e:  # une partie de la boîte (fractions) : un sprite d'une colonne collée
        fx0, fy0, fx1, fy1 = e['part']
        x0, y0, x1, y1 = (round(x0 + (x1 - x0) * fx0), round(y0 + (y1 - y0) * fy0),
                          round(x0 + (x1 - x0) * fx1), round(y0 + (y1 - y0) * fy1))
    m = 3
    x0, y0 = max(0, x0 - m), max(0, y0 - m)
    x1, y1 = min(a.shape[1], x1 + m), min(a.shape[0], y1 + m)
    s = a[y0:y1, x0:x1]
    if e.get('plein'):
        s = s[m + 3:-m - 3, m + 3:-m - 3]  # le liseré de la grille des textures
        return s, np.ones(s.shape[:2], bool)
    bord = np.concatenate([s[0], s[-1], s[:, 0], s[:, -1]])
    fond_local = np.median(bord, axis=0) if np.abs(np.median(bord, axis=0) - FOND).sum() < 40 else FOND  # fond un peu dégradé
    ecart = np.abs(s - fond_local).sum(2)
    clair = (s.min(2) > 175) & ((s.max(2) - s.min(2)) < 45)  # liseré blanc autour des sprites
    fond = (ecart < 45) | clair
    lab, n = nd.label(fond)
    bords = set(np.unique(np.concatenate([lab[0], lab[-1], lab[:, 0], lab[:, -1]]))) - {0}
    masque = ~np.isin(lab, list(bords))
    # fond visible à l'intérieur du sprite (anneau, arche) : retiré s'il est assez grand
    trou, nt = nd.label((ecart < 22) | (clair & bool(e.get("trous"))))
    if nt:
        taille = nd.sum(np.ones_like(ecart), trou, range(1, nt + 1))
        masque &= ~np.isin(trou, [i + 1 for i, v in enumerate(taille) if v >= (8 if e.get('fragments') else 60)])
    masque = nd.binary_opening(masque)
    lab2, n2 = nd.label(masque)
    if n2 > 1 and not e.get('fragments'):
        taille = nd.sum(masque, lab2, range(1, n2 + 1))
        if e.get('seul'):
            masque = lab2 == (int(np.argmax(taille)) + 1)
        else:
            garde = [i + 1 for i, v in enumerate(taille) if v >= max(12, 0.03 * taille.max())]
            masque = np.isin(lab2, garde)
    ys, xs = np.nonzero(masque)
    if len(ys) == 0:
        raise SystemExit(f"{e['id']} : rien trouvé dans la boîte")
    s = s[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
    masque = masque[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
    return s, masque


def reduire(s, masque, W, H, couleurs=20, fragments=False):
    h, w = masque.shape
    s = s.copy()
    if masque.any():
        s[~masque] = np.median(s[masque], axis=0)  # le fond ne prend pas de place dans la palette
    pal = Image.fromarray(s.astype(np.uint8), 'RGB').quantize(colors=couleurs, method=Image.Quantize.MEDIANCUT,
                                                             dither=Image.Dither.NONE)
    idx = np.asarray(pal)
    P = np.array(pal.getpalette()[:couleurs * 3]).reshape(-1, 3)
    lum = P.mean(1)
    poids = 1 + 1.6 * (lum < 70)  # contours et yeux
    seuil = 0.30 if fragments else 0.45
    out = np.zeros((H, W, 4), np.uint8)
    for ty in range(H):
        ya, yb = int(ty * h / H), max(int(ty * h / H) + 1, int((ty + 1) * h / H))
        for tx in range(W):
            xa, xb = int(tx * w / W), max(int(tx * w / W) + 1, int((tx + 1) * w / W))
            mk = masque[ya:yb, xa:xb]
            if mk.mean() < seuil:
                continue
            cnt = np.bincount(idx[ya:yb, xa:xb][mk], minlength=len(P)) * poids
            out[ty, tx, :3] = P[cnt.argmax()]
            out[ty, tx, 3] = 255
    return out


def reduire_plein(s, W, H, couleurs):
    im = Image.fromarray(s.astype(np.uint8), 'RGB').resize((W, H), Image.BOX)
    im = im.quantize(colors=couleurs, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).convert('RGBA')
    return np.asarray(im).copy()


def sprite(e):
    s, masque = decouper(e)
    if e.get('rot'):  # pivoté (armes longues en diagonale : icônes carrées)
        im = Image.fromarray(np.dstack([s.astype(np.uint8), (masque * 255).astype(np.uint8)]), 'RGBA')
        im = im.rotate(e['rot'], resample=Image.NEAREST, expand=True)
        a = np.asarray(im)
        masque = a[..., 3] > 127
        ys, xs = np.nonzero(masque)
        s = a[ys.min():ys.max() + 1, xs.min():xs.max() + 1, :3].astype(int)
        masque = masque[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
    if e.get('vif'):  # petits sprites flous : couleurs et contraste relevés
        im = Image.fromarray(s.astype(np.uint8), 'RGB')
        im = ImageEnhance.Contrast(ImageEnhance.Color(im).enhance(1.35)).enhance(1.25)
        s = np.asarray(im).astype(int)
    h, w = masque.shape
    if 'cadre' in e:  # tient dans un cadre (largeur, hauteur) en gardant ses proportions
        k = min(e['cadre'][0] / w, e['cadre'][1] / h)
        W, H = max(1, round(w * k)), max(1, round(h * k))
    elif 'w' in e and 'h' in e:
        W, H = e['w'], e['h']
    elif 'w' in e:
        W, H = e['w'], max(1, round(h * e['w'] / w))
    else:
        W, H = max(1, round(w * e['h'] / h)), e['h']
    W, H = W * ECHELLE, H * ECHELLE
    if e.get('plein'):
        out = reduire_plein(s, W, H, e.get('couleurs', 24))
    else:
        out = reduire(s, masque, W, H, e.get('couleurs', 28), e.get('fragments', False))
    if e.get('miroir'):
        out = out[:, ::-1].copy()
    return out


def recolorer(px, teintes):
    """Chaque couleur du sprite (palette réduite : peu de couleurs) n'est convertie qu'une fois."""
    out = px.copy()
    vis = px[..., 3] > 0
    if not vis.any():
        return out
    for r, g, b in np.unique(px[vis][:, :3], axis=0):
        m = vis & (px[..., 0] == r) & (px[..., 1] == g) & (px[..., 2] == b)
        out[m, :3] = recolorer_couleur(int(r), int(g), int(b), teintes)
    return out


def recolorer_couleur(r, g, b, teintes):
    hh, ss, vv = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
    deg = hh * 360
    for t in teintes:
        t0, t1, nh, ks, kv = t[:5]
        if t0 < 0 or (t0 <= deg <= t1 if t0 <= t1 else (deg >= t0 or deg <= t1)):
            if ss < 0.12 and t0 >= 0:
                continue  # gris, blancs et noirs gardent leur couleur
            if nh >= 0:
                hh = nh / 360
            ss = min(1, ss * ks)
            if len(t) > 5 and vv > 0.12:
                ss = max(ss, t[5])  # teinte donnée aussi aux gris
            vv = min(1, vv * kv)
            break
    r2, g2, b2 = colorsys.hsv_to_rgb(hh, ss, vv)
    return [round(r2 * 255), round(g2 * 255), round(b2 * 255)]


def ranger(sprites, largeur=2048):
    """Rangement en étagères (du plus haut au plus bas) ; 1 px d'écart entre les sprites."""
    ordre = sorted(sprites, key=lambda k: -sprites[k].shape[0])
    x = y = haut = 0
    place = {}
    for k in ordre:
        h, w = sprites[k].shape[:2]
        if x + w > largeur:
            x, y, haut = 0, y + haut + 1, 0
        place[k] = (x, y, w, h)
        x += w + 1
        haut = max(haut, h)
    H = y + haut
    atlas = np.zeros((H, largeur, 4), np.uint8)
    for k, (x, y, w, h) in place.items():
        atlas[y:y + h, x:x + w] = sprites[k]
    return atlas, place


def apercu(sprites, chemin):
    """Planche de contrôle : chaque sprite agrandi x3 avec son nom."""
    k, col, lh = 2, 8, 0
    cases = list(sprites.items())
    largeur = 8 * 150
    im = Image.new('RGB', (largeur, 100), (110, 150, 90))
    x = y = 0
    blocs = []
    for nom, px in cases:
        w, h = px.shape[1] * k, px.shape[0] * k
        cw = max(w, 6 * len(nom)) + 10
        if x + cw > largeur:
            x, y, lh = 0, y + lh + 14, 0
        blocs.append((nom, px, x, y))
        x += cw
        lh = max(lh, h)
    im = im.resize((largeur, y + lh + 16))
    d = ImageDraw.Draw(im)
    for nom, px, x, y in blocs:
        g = Image.fromarray(px, 'RGBA')
        g = g.resize((g.width * k, g.height * k), Image.NEAREST)
        im.paste(g, (x, y + 11), g)
        d.text((x, y), nom, fill=(255, 255, 255))
    im.save(chemin)


def main():
    spec = json.load(open(os.path.join(ICI, 'decoupe.json'), encoding='utf-8'))
    seuls = set(sys.argv[1:])
    sprites = {}
    for e in spec:
        sprites[e['id']] = sprite(e)
    # Sprites déjà prêts (outils/sprites/rendus/*.png, à la finesse de l'atlas) : rendus de Blender (pixeliser.py)
    rdir = os.path.join(ICI, 'rendus')
    if os.path.isdir(rdir):
        for f in sorted(os.listdir(rdir)):
            if f.endswith('.png'):
                sprites[f[:-4]] = np.asarray(Image.open(os.path.join(rdir, f)).convert('RGBA')).copy()
    vpath = os.path.join(ICI, 'variantes.json')
    if os.path.exists(vpath):
        for v in json.load(open(vpath, encoding='utf-8')):
            if 'prefixe' in v:  # tous les sprites d'un personnage : mage_bas_0 -> maelle_bas_0…
                for k in [k for k in sprites if k.startswith(v['de'] + '_')]:
                    suite = k[len(v['de']) + 1:]
                    if suite.split('_')[0] in ('bas', 'haut', 'droite', 'gauche', 'combat'):
                        sprites[v['prefixe'] + '_' + suite] = recolorer(sprites[k], v['teintes'])
            elif v['de'] in sprites:
                sprites[v['id']] = recolorer(sprites[v['de']], v['teintes'])
                if v.get('miroir'):  # retourné gauche-droite (regard vers la gauche, comme les autres)
                    sprites[v['id']] = sprites[v['id']][:, ::-1].copy()
    if seuls:  # aperçu des seuls sprites dont le nom commence par un des arguments
        apercu({k: v for k, v in sprites.items() if any(k.startswith(p) for p in seuls)}, os.path.join(ICI, 'apercu.png'))
        print(len(sprites), 'sprites (aperçu seulement)')
        return
    apercu(sprites, os.path.join(ICI, 'apercu.png'))
    atlas, place = ranger(sprites)
    Image.fromarray(atlas, 'RGBA').save(os.path.join(RACINE, 'data', 'sprites.png'), optimize=True)
    with open(os.path.join(RACINE, 'data', 'sprites.json'), 'w', encoding='utf-8') as f:
        f.write('{\n' + f'  "_echelle": {ECHELLE},\n' + ',\n'.join(f'  "{k}": [{x}, {y}, {w}, {h}]' for k, (x, y, w, h) in sorted(place.items()))
                + '\n}\n')
    print(len(sprites), 'sprites, atlas', atlas.shape[1], 'x', atlas.shape[0])


if __name__ == '__main__':
    main()
