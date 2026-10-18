"""Transforme les sprites du jeu en modèles 3D en voxels, pour un futur Brumeval en 3D (Godot).

    python outils/voxels/voxelise.py                  # tous les sprites -> essai_godot/modeles/*.glb + apercu.png
    python outils/voxels/voxelise.py lior ronceau     # seulement les sprites dont le nom commence par lior ou ronceau
    python outils/voxels/voxelise.py --detail 1       # les petits sprites du jeu : plus rapide, moins fin
    python outils/voxels/voxelise.py --plat arbre     # épaisseur constante au lieu d'un volume arrondi

Les sprites du jeu (data/sprites.png) sont petits (Lior : 24 pixels de haut). Par défaut, l'outil les redécoupe dans
les planches d'origine (outils/sprites/planches/), trois fois plus grands (--detail 3), avec l'outil de découpe des
sprites (outils/sprites/decoupe.py : mêmes boîtes, mêmes recolorations) : le modèle garde la même taille, mais ses
voxels sont trois fois plus petits et on voit les yeux, les mèches, les flammes.

Chaque pixel du sprite devient une colonne de petits cubes (voxels) de la couleur du pixel. L'épaisseur d'une
colonne dépend de sa distance au bord du sprite : fine sur les contours (épée, oreilles), plus épaisse au milieu
(ventre, tronc). Le modèle a donc du volume au lieu d'être une planche découpée. Il est symétrique devant/derrière.

Les modèles sont écrits au format glTF binaire (.glb), que Godot, Blender et la plupart des logiciels 3D ouvrent
directement. Ils sont rangés dans le petit projet Godot essai_godot/ : l'ouvrir avec Godot 4 pour se promener
parmi les modèles (voir LISEZMOI.md). Pour un autre projet, il suffit d'y copier les .glb. Repères : 1 pixel du
jeu = 1/16 de mètre (Lior mesure 1,5 m) ; l'origine est au milieu des pieds (le bas du sprite) ; le modèle
regarde vers +Z (vers la caméra de Godot par défaut). Les couleurs sont des couleurs de sommets : aucune texture.

Les faces cachées (entre deux voxels) sont retirées, puis les faces voisines de même couleur sont fusionnées en
grands rectangles : un personnage fait environ 10 000 triangles, une maison 60 000 (5 fois moins avec --detail 1).

apercu.png montre les modèles en vue de trois quarts, avec un éclairage simple, pour vérifier d'un coup d'œil.
Nécessite Pillow, numpy et scipy (pip install pillow numpy scipy), comme outils/sprites/decoupe.py.
"""
import argparse
import json
import math
import os
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage as nd

ICI = os.path.dirname(os.path.abspath(__file__))
RACINE = os.path.dirname(os.path.dirname(ICI))
TAILLE_PIXEL = 1 / 16  # mètres par pixel du jeu (un sprite plus détaillé a des pixels plus petits)
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
        # Les coins doivent tourner dans le sens trigonométrique vus de dehors : cela ne dépend que de la direction.
        normale = [0, 0, 0]
        normale[axe] = sens
        e_u, e_v = np.eye(3)[u], np.eye(3)[v]
        retourner = np.dot(np.cross(e_u, e_v), normale) < 0
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
                if retourner:
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


def ecrire_glb(chemin, nom, quads, palette, largeur, taille_pixel):
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
    col = np.round(np.array(couleurs) * 65535).astype(np.uint16)  # 16 bits par canal : fichiers plus légers
    ind = np.array(indices, np.uint32 if len(pos) > 65535 else np.uint16)
    # Recentrer : milieu des pieds à l'origine, avant/arrière centrés.
    pos[:, 0] -= largeur / 2
    pos[:, 2] -= (pos[:, 2].min() + pos[:, 2].max()) / 2
    pos *= taille_pixel

    col = np.hstack([col, np.full((len(col), 1), 65535, np.uint16)])  # RVBA : chaque sommet sur 8 octets
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
            {'bufferView': 2, 'componentType': 5123, 'normalized': True, 'count': len(col), 'type': 'VEC4'},
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
# Aperçu : vue de trois quarts. Chaque voxel visible est un petit carré, dessiné du plus loin au plus proche
# (les plus proches recouvrent les autres).

LUMIERE = {'dessus': 1.0, 'cote': 0.82, 'devant': 0.68}


def apercu(grille, cotes, palette, echelle=4):
    w, h, p = grille.shape
    plein = grille >= 0
    def libre(decalage, axe):  # le voisin dans cette direction est vide (ou hors de la grille)
        v = np.roll(plein, -decalage, axis=axe)
        bord = [slice(None)] * 3
        bord[axe] = -1
        v[tuple(bord)] = False
        return plein & ~v
    devant, dessus, cote = libre(1, 2), libre(1, 1), libre(1, 0)
    montre = devant | dessus | cote
    xs, ys, zs = np.nonzero(montre)
    ordre = np.lexsort((xs, ys, zs))  # de l'arrière à l'avant, puis de bas en haut, puis de gauche à droite
    xs, ys, zs = xs[ordre], ys[ordre], zs[ordre]
    pal = np.asarray(palette, float)
    # Couleur du carré : la face de devant si on la voit, sinon le dessus, sinon le côté droit.
    av = devant[xs, ys, zs]
    de = dessus[xs, ys, zs] & ~av
    couleur = np.where(av[:, None], pal[grille[xs, ys, zs]] * LUMIERE['devant'],
                       np.where(de[:, None], pal[cotes[xs, ys, zs]] * LUMIERE['dessus'],
                                pal[cotes[xs, ys, zs]] * LUMIERE['cote']))
    # Projection : x vers la droite, z (avant) vers le bas-gauche, y vers le haut.
    sx = np.round((xs - zs * 0.5 + p * 0.5 + 2) * echelle).astype(int)
    sy = np.round((h - 1 - ys + zs * 0.35 + 2) * echelle).astype(int)
    image = np.zeros((int((h + p * 0.35 + 4) * echelle) + 2, int((w + p * 0.5 + 4) * echelle) + 2, 4), np.uint8)
    rgba = np.hstack([np.clip(couleur, 0, 255), np.full((len(xs), 1), 255)]).astype(np.uint8)
    taille = echelle + 1  # un pixel de plus pour ne pas laisser de jours entre les carrés
    for dy in range(taille):
        for dx in range(taille):
            image[sy + dy, sx + dx] = rgba  # les derniers écrits (les plus proches) gagnent
    return Image.fromarray(image, 'RGBA')


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


def sprites_detailles(noms, detail):
    """Redécoupe les sprites demandés dans les planches d'origine (outils/sprites/planches), detail fois plus grands
    que dans le jeu, avec l'outil de découpe des sprites (mêmes boîtes, mêmes recolorations).

    Rend {nom: pixels RGBA} ; les sprites absents de outils/sprites/decoupe.json ne sont pas dedans."""
    sys.path.insert(0, os.path.join(RACINE, 'outils', 'sprites'))
    import decoupe  # outils/sprites/decoupe.py
    spec = json.load(open(os.path.join(RACINE, 'outils', 'sprites', 'decoupe.json'), encoding='utf-8'))
    variantes = json.load(open(os.path.join(RACINE, 'outils', 'sprites', 'variantes.json'), encoding='utf-8'))
    # Les recolorations (maelle_… à partir de mage_…) ont besoin de leur sprite de départ.
    voulus = set(noms)
    for v in variantes:
        if 'prefixe' in v:
            voulus |= {v['de'] + n[len(v['prefixe']):] for n in noms if n.startswith(v['prefixe'] + '_')}
        elif v['id'] in voulus:
            voulus.add(v['de'])
    resultat = {}
    for e in spec:
        if e['id'] not in voulus:
            continue
        e = dict(e)
        for cle in ('w', 'h'):
            if cle in e:
                e[cle] *= detail
        if 'cadre' in e:
            e['cadre'] = [c * detail for c in e['cadre']]
        resultat[e['id']] = decoupe.sprite(e)
    for v in variantes:
        if 'prefixe' in v:
            for n in noms:
                depart = v['de'] + n[len(v['prefixe']):]
                if n.startswith(v['prefixe'] + '_') and depart in resultat:
                    resultat[n] = decoupe.recolorer(resultat[depart], v['teintes'])
        elif v['id'] in voulus and v['de'] in resultat:
            resultat[v['id']] = decoupe.recolorer(resultat[v['de']], v['teintes'])
    return resultat


def main():
    parser = argparse.ArgumentParser(description='Sprites de Brumeval -> modèles 3D en voxels (.glb)')
    parser.add_argument('noms', nargs='*', help='débuts de noms de sprites (par défaut : tous)')
    parser.add_argument('--sortie', default=os.path.join(ICI, 'essai_godot', 'modeles'),
                        help='dossier des .glb (par défaut : celui du projet Godot d\'essai)')
    parser.add_argument('--detail', type=int, default=3,
                        help='finesse : sprites redécoupés dans les planches d\'origine, N fois plus de pixels que dans '
                             'le jeu (3 par défaut ; 1 = les sprites du jeu, plus rapide)')
    parser.add_argument('--gonflement', type=float, default=0.6,
                        help='épaisseur gagnée par pixel de distance au bord (0.6 par défaut ; plus = plus rond)')
    parser.add_argument('--epaisseur-max', type=int, default=12,
                        help='épaisseur maximale, en pixels du jeu (12 par défaut)')
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
    detailles = sprites_detailles(choisis, args.detail) if args.detail > 1 else {}

    os.makedirs(args.sortie, exist_ok=True)
    images, total, du_jeu = [], 0, []
    for nom in choisis:
        if nom in detailles:
            pixels, k = detailles[nom], args.detail
        else:  # pas dans les planches (ou --detail 1) : le sprite du jeu
            x, y, w, h = boites[nom]
            pixels, k = atlas[y:y + h, x:x + w], 1
            if args.detail > 1:
                du_jeu.append(nom)
        grille, cotes, palette = voxeliser(pixels, args.gonflement, args.epaisseur_max * k, args.plat)
        quads = mailler(grille, cotes)
        total += ecrire_glb(os.path.join(args.sortie, nom + '.glb'), nom, quads, palette, pixels.shape[1],
                            TAILLE_PIXEL / k)
        if not args.sans_apercu:
            images.append(apercu(grille, cotes, palette, echelle=max(1, 4 // k)))
    print(f'{len(choisis)} modèles écrits dans {os.path.relpath(args.sortie)} ({total} triangles en tout, '
          f'{total // len(choisis)} en moyenne)')
    if du_jeu:
        print(f'{len(du_jeu)} sprites absents des planches, faits avec le sprite du jeu : ' + ', '.join(du_jeu))
    if images:
        chemin = os.path.join(ICI, 'apercu.png')
        planche_apercus(images, choisis).save(chemin)
        print('Aperçu :', os.path.relpath(chemin))


if __name__ == '__main__':
    main()
