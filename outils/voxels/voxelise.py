"""Transforme les sprites du jeu (data/sprites.png) en modèles 3D en voxels, pour un futur Brumeval en 3D (Godot).

    python outils/voxels/voxelise.py                  # tous les sprites -> essai_godot/modeles/*.glb + apercu.png
    python outils/voxels/voxelise.py lior ronceau     # seulement les sprites dont le nom commence par lior ou ronceau
    python outils/voxels/voxelise.py --plat arbre     # épaisseur constante au lieu d'un volume arrondi

Chaque pixel du sprite devient une colonne de petits cubes (voxels) de la couleur du pixel. L'épaisseur d'une
colonne dépend de sa distance au bord du sprite : fine sur les contours (épée, oreilles), plus épaisse au milieu
(ventre, tronc). Le modèle a donc du volume au lieu d'être une planche découpée. Il est symétrique devant/derrière.

Les modèles sont écrits au format glTF binaire (.glb), que Godot, Blender et la plupart des logiciels 3D ouvrent
directement. Ils sont rangés dans le petit projet Godot essai_godot/ : l'ouvrir avec Godot 4 pour se promener
parmi les modèles (voir LISEZMOI.md). Pour un autre projet, il suffit d'y copier les .glb. Repères : 1 pixel =
1/16 de mètre (Lior, 24 pixels, mesure 1,5 m) ; l'origine est au milieu des pieds (le bas du sprite) ; le modèle
regarde vers +Z (vers la caméra de Godot par défaut). Les couleurs sont des couleurs de sommets : aucune texture.

Les faces cachées (entre deux voxels) sont retirées, puis les faces voisines de même couleur sont fusionnées en
grands rectangles : un personnage fait environ 2 000 triangles, une maison 15 000.

apercu.png montre les modèles en vue de trois quarts, avec un éclairage simple, pour vérifier d'un coup d'œil.
Nécessite Pillow, numpy et scipy (pip install pillow numpy scipy), comme outils/sprites/decoupe.py.
"""
import argparse
import json
import math
import os
import struct

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage as nd

ICI = os.path.dirname(os.path.abspath(__file__))
RACINE = os.path.dirname(os.path.dirname(ICI))
TAILLE_PIXEL = 1 / 16  # mètres par pixel du jeu
# Sprites ignorés sans demande explicite : textures de sol et de murs (elles iront sur des surfaces, pas en volume)
# et portraits des dialogues.
IGNORES = ('sol_', 'tex_', 'lior_visage_')


# ---------------------------------------------------------------------------------------------------------------
# Du sprite aux voxels


def voxeliser(rgba, gonflement, epaisseur_max, plat):
    """Voxels d'un sprite : deux tableaux [x, y, z] d'indices de couleur (-1 = vide) et la palette (RGB 0-255).

    Le premier donne la couleur des faces de devant et de derrière (le dessin, contours compris), le second celle des
    autres faces (dessus, dessous, côtés : sans les contours). x va vers la droite, y vers le haut (le bas du sprite
    est y = 0), z de l'arrière vers l'avant."""
    masque = rgba[:, :, 3] >= 128
    h_img, w_img = masque.shape
    if plat or masque.all():  # une image pleine (panneau, dalle) : une simple plaque
        demi = np.where(masque, max(1, epaisseur_max // 4 if not plat else epaisseur_max // 2), 0)
    else:
        # Distance de chaque pixel au bord : 1 sur le contour, plus au milieu.
        distance = nd.distance_transform_edt(np.pad(masque, 1))[1:-1, 1:-1]
        demi = np.where(masque, np.clip(np.round(distance * gonflement), 1, max(1, epaisseur_max // 2)), 0)
        # Lisser un peu le relief pour éviter les marches d'escalier d'un pixel à l'autre.
        demi = np.where(masque, np.maximum(1, np.round(nd.uniform_filter(demi.astype(float), 3))), 0)
    demi = demi.astype(int)
    # Le contour sombre du dessin ne doit rester que sur les faces avant et arrière : dans l'épaisseur (les côtés
    # du modèle), chaque pixel de contour prend la couleur du pixel clair le plus proche, sinon le modèle aurait
    # l'air entouré d'un mur noir.
    rgb = rgba[:, :, :3].astype(int)
    sombre = masque & (rgb.max(2) < 70)
    interieur = masque & ~sombre
    if interieur.any():
        _, (iy, ix) = nd.distance_transform_edt(~interieur, return_indices=True)
        remplissage = rgb[iy, ix]
    else:
        remplissage = rgb
    couleurs, index = np.unique(np.concatenate([rgb[masque], remplissage[masque]]), axis=0, return_inverse=True)
    index = index.ravel()
    n = int(masque.sum())
    face, coeur = index[:n], index[n:]
    profondeur = 2 * int(demi.max())
    grille = np.full((w_img, h_img, profondeur), -1, np.int32)
    cotes = grille.copy()
    ys, xs = np.nonzero(masque)
    milieu = profondeur // 2
    for (y, x, cf, cc) in zip(ys, xs, face, coeur):
        d = demi[y, x]
        grille[x, h_img - 1 - y, milieu - d:milieu + d] = cf
        cotes[x, h_img - 1 - y, milieu - d:milieu + d] = cc
    return grille, cotes, couleurs


# ---------------------------------------------------------------------------------------------------------------
# Des voxels au maillage : faces visibles, fusionnées en rectangles (« greedy meshing »)

# Les six directions des faces : axe de la normale (0 = x, 1 = y, 2 = z) et sens (-1 ou +1).
DIRECTIONS = [(0, -1), (0, 1), (1, -1), (1, 1), (2, -1), (2, 1)]


def mailler(grille, cotes):
    """Liste de quadrilatères : (4 coins dans l'ordre trigonométrique vus de dehors, normale, indice de couleur).

    Les faces avant et arrière prennent leur couleur dans grille, les autres dans cotes."""
    quads = []
    taille = grille.shape
    plein = grille >= 0
    for axe, sens in DIRECTIONS:
        u, v = [a for a in range(3) if a != axe]
        # Face visible : voxel plein dont le voisin dans la direction de la normale est vide (ou hors de la grille).
        voisin = np.roll(plein, -sens, axis=axe)
        bord = [slice(None)] * 3
        bord[axe] = -1 if sens == 1 else 0
        voisin[tuple(bord)] = False  # np.roll fait le tour : au bord de la grille, le voisin est vide
        visible = plein & ~voisin
        for k in range(taille[axe]):
            tranche = [slice(None)] * 3
            tranche[axe] = k
            source = grille if axe == 2 else cotes
            couleur = np.where(visible[tuple(tranche)], source[tuple(tranche)], -1)  # tableau [u, v]
            for (i, j, di, dj, c) in rectangles(couleur):
                plan = k + (1 if sens == 1 else 0)
                coins = []
                for (a, b) in ((i, j), (i + di, j), (i + di, j + dj), (i, j + dj)):
                    p = [0, 0, 0]
                    p[axe], p[u], p[v] = plan, a, b
                    coins.append(p)
                # Les coins doivent tourner dans le sens trigonométrique vus de dehors : on retourne sinon.
                normale = [0, 0, 0]
                normale[axe] = sens
                if np.dot(np.cross(np.subtract(coins[1], coins[0]), np.subtract(coins[2], coins[0])), normale) < 0:
                    coins.reverse()
                quads.append((coins, normale, c))
    return quads


def rectangles(couleur):
    """Découpe un tableau 2D de couleurs (-1 = rien) en rectangles de même couleur, le plus grands possible."""
    couleur = couleur.copy()
    n_u, n_v = couleur.shape
    resultat = []
    for i in range(n_u):
        j = 0
        while j < n_v:
            c = couleur[i, j]
            if c < 0:
                j += 1
                continue
            dj = 1
            while j + dj < n_v and couleur[i, j + dj] == c:
                dj += 1
            di = 1
            while i + di < n_u and np.all(couleur[i + di, j:j + dj] == c):
                di += 1
            couleur[i:i + di, j:j + dj] = -1
            resultat.append((i, j, di, dj, int(c)))
            j += dj
    return resultat


# ---------------------------------------------------------------------------------------------------------------
# Écriture glTF binaire (.glb), sans bibliothèque


def srgb_vers_lineaire(c):
    c = np.asarray(c, float) / 255
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def ecrire_glb(chemin, nom, quads, palette, largeur):
    positions, normales, couleurs, indices = [], [], [], []
    lin = srgb_vers_lineaire(palette)
    for coins, normale, c in quads:
        base = len(positions)
        for p in coins:
            positions.append(p)
            normales.append(normale)
            couleurs.append(lin[c])
        indices += [base, base + 1, base + 2, base, base + 2, base + 3]
    pos = np.array(positions, np.float32)
    nor = np.array(normales, np.float32)
    col = np.array(couleurs, np.float32)
    ind = np.array(indices, np.uint32 if len(pos) > 65535 else np.uint16)
    # Recentrer : milieu des pieds à l'origine, avant/arrière centrés.
    pos[:, 0] -= largeur / 2
    pos[:, 2] -= (pos[:, 2].min() + pos[:, 2].max()) / 2
    pos *= TAILLE_PIXEL

    blocs = [pos.tobytes(), nor.tobytes(), col.tobytes(), ind.tobytes()]
    binaire = b''
    vues = []
    for b in blocs:
        vues.append({'buffer': 0, 'byteOffset': len(binaire), 'byteLength': len(b)})
        binaire += b + b'\0' * (-len(b) % 4)
    gltf = {
        'asset': {'version': '2.0', 'generator': 'Brumeval outils/voxels/voxelise.py'},
        'scene': 0,
        'scenes': [{'nodes': [0]}],
        'nodes': [{'name': nom, 'mesh': 0}],
        'meshes': [{'name': nom, 'primitives': [{
            'attributes': {'POSITION': 0, 'NORMAL': 1, 'COLOR_0': 2}, 'indices': 3, 'material': 0}]}],
        'materials': [{'name': 'voxels', 'pbrMetallicRoughness': {
            'baseColorFactor': [1, 1, 1, 1], 'metallicFactor': 0, 'roughnessFactor': 1}}],
        'buffers': [{'byteLength': len(binaire)}],
        'bufferViews': [dict(vues[0], target=34962), dict(vues[1], target=34962), dict(vues[2], target=34962),
                        dict(vues[3], target=34963)],
        'accessors': [
            {'bufferView': 0, 'componentType': 5126, 'count': len(pos), 'type': 'VEC3',
             'min': pos.min(0).tolist(), 'max': pos.max(0).tolist()},
            {'bufferView': 1, 'componentType': 5126, 'count': len(nor), 'type': 'VEC3'},
            {'bufferView': 2, 'componentType': 5126, 'count': len(col), 'type': 'VEC3'},
            {'bufferView': 3, 'componentType': 5125 if ind.dtype == np.uint32 else 5123, 'count': len(ind),
             'type': 'SCALAR'},
        ],
    }
    texte = json.dumps(gltf, separators=(',', ':')).encode()
    texte += b' ' * (-len(texte) % 4)
    with open(chemin, 'wb') as f:
        f.write(struct.pack('<III', 0x46546C67, 2, 12 + 8 + len(texte) + 8 + len(binaire)))
        f.write(struct.pack('<II', len(texte), 0x4E4F534A) + texte)
        f.write(struct.pack('<II', len(binaire), 0x004E4942) + binaire)
    return len(ind) // 3


# ---------------------------------------------------------------------------------------------------------------
# Aperçu : vue de trois quarts, voxel par voxel (algorithme du peintre : du plus loin au plus proche)

LUMIERE = {(1, 1): 1.0, (0, 1): 0.82, (2, 1): 0.68}  # dessus, droite, devant


def apercu(grille, cotes, palette, echelle=4):
    w, h, p = grille.shape
    # Projection : x vers la droite, z (avant) vers le bas-gauche, y vers le haut. On voit donc le devant, le
    # dessus et le côté droit.
    def ecran(x, y, z):
        return ((x - z * 0.5) * echelle + (p * 0.5 + 2) * echelle, (h - y + z * 0.35 + 2) * echelle)
    image = Image.new('RGBA', (int((w + p * 0.5 + 4) * echelle), int((h + p * 0.35 + 4) * echelle)), (0, 0, 0, 0))
    dessin = ImageDraw.Draw(image)
    plein = grille >= 0
    xs, ys, zs = np.nonzero(plein)
    ordre = np.lexsort((xs, ys, zs))  # z croissant (de l'arrière à l'avant), puis y, puis x
    for n in ordre:
        x, y, z = xs[n], ys[n], zs[n]
        faces = []
        if y + 1 >= h or not plein[x, y + 1, z]:
            faces.append(((1, 1), [(x, y + 1, z), (x + 1, y + 1, z), (x + 1, y + 1, z + 1), (x, y + 1, z + 1)]))
        if x + 1 >= w or not plein[x + 1, y, z]:
            faces.append(((0, 1), [(x + 1, y, z), (x + 1, y + 1, z), (x + 1, y + 1, z + 1), (x + 1, y, z + 1)]))
        if z + 1 >= p or not plein[x, y, z + 1]:
            faces.append(((2, 1), [(x, y, z + 1), (x + 1, y, z + 1), (x + 1, y + 1, z + 1), (x, y + 1, z + 1)]))
        for cle, coins in faces:
            c = np.array(palette[(grille if cle[0] == 2 else cotes)[x, y, z]], float)
            teinte = tuple(int(v) for v in np.clip(c * LUMIERE[cle], 0, 255)) + (255,)
            dessin.polygon([ecran(*q) for q in coins], fill=teinte)
    return image


def planche_apercus(images, noms, colonnes=12, case=150):
    """Range les aperçus dans une grille de cases de même taille (les grands modèles sont réduits)."""
    lignes = math.ceil(len(images) / colonnes)
    planche = Image.new('RGB', (case * min(colonnes, len(images)), (case + 16) * lignes), (40, 44, 52))
    dessin = ImageDraw.Draw(planche)
    for n, (img, nom) in enumerate(zip(images, noms)):
        img = img.copy()
        img.thumbnail((case - 8, case - 8), Image.NEAREST)
        cx, cy = (n % colonnes) * case, (n // colonnes) * (case + 16)
        dessin.text((cx + 4, cy + 2), nom[:case // 6], fill=(220, 220, 220))  # le nom au-dessus de son modèle
        planche.paste(img, (cx + (case - img.width) // 2, cy + 16 + case - 4 - img.height), img)
    return planche


# ---------------------------------------------------------------------------------------------------------------


def main():
    parser = argparse.ArgumentParser(description='Sprites de Brumeval -> modèles 3D en voxels (.glb)')
    parser.add_argument('noms', nargs='*', help='débuts de noms de sprites (par défaut : tous)')
    parser.add_argument('--sortie', default=os.path.join(ICI, 'essai_godot', 'modeles'),
                        help='dossier des .glb (par défaut : celui du projet Godot d\'essai)')
    parser.add_argument('--gonflement', type=float, default=0.6,
                        help='épaisseur gagnée par pixel de distance au bord (0.6 par défaut ; plus = plus rond)')
    parser.add_argument('--epaisseur-max', type=int, default=12, help='épaisseur maximale en voxels (12 par défaut)')
    parser.add_argument('--plat', action='store_true', help='épaisseur constante (--epaisseur-max) partout')
    parser.add_argument('--sans-apercu', action='store_true', help="ne pas fabriquer apercu.png")
    args = parser.parse_args()

    boites = json.load(open(os.path.join(RACINE, 'data', 'sprites.json'), encoding='utf-8'))
    atlas = np.asarray(Image.open(os.path.join(RACINE, 'data', 'sprites.png')).convert('RGBA'))
    if args.noms:
        choisis = [n for n in sorted(boites) if any(n.startswith(p) for p in args.noms)]
    else:
        choisis = [n for n in sorted(boites) if not n.startswith(IGNORES)]
    if not choisis:
        raise SystemExit('Aucun sprite ne commence par : ' + ', '.join(args.noms))

    os.makedirs(args.sortie, exist_ok=True)
    images, total = [], 0
    for nom in choisis:
        x, y, w, h = boites[nom]
        grille, cotes, palette = voxeliser(atlas[y:y + h, x:x + w], args.gonflement, args.epaisseur_max, args.plat)
        quads = mailler(grille, cotes)
        total += ecrire_glb(os.path.join(args.sortie, nom + '.glb'), nom, quads, palette, w)
        if not args.sans_apercu:
            images.append(apercu(grille, cotes, palette))
    print(f'{len(choisis)} modèles écrits dans {os.path.relpath(args.sortie)} ({total} triangles en tout, '
          f'{total // len(choisis)} en moyenne)')
    if images:
        chemin = os.path.join(ICI, 'apercu.png')
        planche_apercus(images, choisis).save(chemin)
        print('Aperçu :', os.path.relpath(chemin))


if __name__ == '__main__':
    main()
