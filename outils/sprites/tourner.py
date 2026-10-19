"""Poses de dos et de profil d'un personnage dessiné seulement de face (habitants de la planche « decor »).

Appelé par decoupe.py pour une entrée qui a « tourner » : "haut" (de dos) ou "droite" (trois quarts vers la droite),
et « pas » (0 à 3 : debout, pied gauche, debout, pied droit). Le travail se fait sur le dessin de la planche, avant
la réduction : le résultat passe par la même réduction que tous les sprites et garde leur style (contours sombres,
couleurs de la planche). Remplace les rendus 3D de la 3.4, trop différents du reste du jeu.

Principe :
  - la peau du visage est trouvée par sa couleur dans le haut du dessin (tête) ; de dos, tout le visage (yeux, bouche,
    barbe comprise) devient des cheveux, avec des mèches ; de trois quarts, le visage glisse vers la droite, sa partie
    gauche devient des cheveux et le corps se resserre un peu ;
  - la marche décale les pieds : de face ou de dos un pied se lève, de profil les jambes s'écartent.
Options de l'entrée : "chapeau" (fraction de la hauteur occupée par le chapeau : jamais touchée), "cheveux" ([r, g, b]
imposée), "tablier" (true : de dos, le blanc du bas du corps prend la couleur voisine), "tourne" (le dessin source est
déjà de trois quarts : le visage glisse moins).
"""
import numpy as np
from scipy import ndimage as nd


def _hsv(s):
    f = s.astype(float) / 255
    mx, mn = f.max(2), f.min(2)
    sat = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-6), 0)
    r, g, b = f[..., 0], f[..., 1], f[..., 2]
    d = np.maximum(mx - mn, 1e-6)
    h = np.where(mx == r, ((g - b) / d) % 6, np.where(mx == g, (b - r) / d + 2, (r - g) / d + 4)) * 60
    return h, sat, mx


def _peau(s):
    h, sat, v = _hsv(s)
    return (h > 8) & (h < 42) & (sat > 0.18) & (sat < 0.62) & (v > 0.62)


def _visage(s, masque, chap):
    """Bas de la tête et masque du visage (peau et traits : yeux, bouche, barbe)."""
    H, W = masque.shape
    pk = _peau(s) & masque
    pk[:chap] = False
    n = pk[:int(H * 0.52)].sum(1)
    if n.max() < 2:
        return None, None
    # le cœur du visage : les rangées d'un seul tenant autour de la plus large (le cou, plus étroit, n'en est pas)
    y0 = y1 = int(np.argmax(n))
    while y0 > chap and n[y0 - 1] >= 0.3 * n.max():
        y0 -= 1
    while y1 + 1 < len(n) and n[y1 + 1] >= 0.3 * n.max():
        y1 += 1
    xs = np.nonzero(pk[y0:y1 + 1].any(0))[0]
    x0, x1 = int(xs.min()), int(xs.max())
    visage = np.zeros_like(masque)
    visage[y0:y1 + 1, x0:x1 + 1] = True
    # le menton (et la barbe) : quelques rangées de plus, de plus en plus étroites
    for k in range(1, max(2, round((y1 - y0 + 1) * 0.3)) + 1):
        a, b = x0 + 2 * k, x1 - 2 * k
        if y1 + k >= H or a > b:
            break
        visage[y1 + k, a:b + 1] = True
    visage &= nd.binary_erosion(masque, iterations=2)  # le contour du dessin reste
    return int(np.nonzero(visage.any(1))[0].max()), visage


def _couleur_cheveux(s, masque, visage, y1, chap, e):
    if 'cheveux' in e:
        return np.array(e['cheveux'], float)
    zone = nd.binary_erosion(masque, iterations=2) & ~visage & ~_peau(s)
    zone[y1 + 1:] = False
    zone[:chap] = False
    px = s[zone].astype(float)
    lum = px.mean(1) if len(px) else px
    if len(px) and ((lum > 25) & (lum < 200)).any():
        px = px[(lum > 25) & (lum < 200)]
    return np.median(px, axis=0) if len(px) else np.array([70, 50, 35], float)


def _peindre_cheveux(s, zone, base, y_haut, y_bas, graine):
    """Cheveux : la couleur de base, plus claire en haut, plus sombre vers le bas et sur les côtés, mèches."""
    ys, xs = np.nonzero(zone)
    if len(ys) == 0:
        return
    xc = (xs.min() + xs.max()) / 2
    demi = max(1, (xs.max() - xs.min()) / 2)
    rnd = np.random.RandomState(graine)
    meches = rnd.rand(s.shape[1] + 4) * 0.16 - 0.08
    for y, x in zip(ys, xs):
        t = (y - y_haut) / max(1, y_bas - y_haut)
        k = 1.22 - 0.24 * t - 0.22 * abs(x - xc) / demi + meches[x + int(t * 3)]
        s[y, x] = np.clip(base * k, 0, 255)


def _resserrer(s, masque, y_de, k):
    """Corps de trois quarts : les rangées sous y_de sont resserrées vers le milieu (plus proches voisins)."""
    H, W = masque.shape
    cols = np.nonzero(masque[y_de:].any(0))[0]
    xc = (cols.min() + cols.max()) / 2
    src = np.clip(np.round(xc + (np.arange(W) - xc) / k).astype(int), 0, W - 1)
    ok = np.abs(np.arange(W) - xc) / k <= W
    s2, m2 = s.copy(), masque.copy()
    s2[y_de:] = s[y_de:, src]
    m2[y_de:] = masque[y_de:, src] & ok[None, :]
    return s2, m2


def tourner(s, masque, e):
    vue, pas = e['tourner'], e.get('pas', 0)
    s, masque = s.copy(), masque.copy()
    H, W = masque.shape
    chap = int(H * e.get('chapeau', 0))
    y1, visage = _visage(s, masque, chap)
    if visage is not None:
        base = _couleur_cheveux(s, masque, visage, y1, chap, e)
        y0 = np.nonzero(visage.any(1))[0].min()
        if vue == 'haut':
            pk = _peau(s) & visage
            nuque = np.median(s[pk], axis=0) * 0.8 if pk.any() else None
            _peindre_cheveux(s, visage, base, y0 - 3, y1 + 1, 7)
            # la nuque : un peu de peau sous les cheveux, au milieu
            xs = np.nonzero(visage[y1])[0]
            if nuque is not None and len(xs) >= 3:
                xc, r = (xs.min() + xs.max()) // 2, max(1, len(xs) // 4)
                for y in (y1, y1 - 1):
                    s[y, xc - r:xc + r + 1] = nuque
        else:  # trois quarts vers la droite : le visage glisse, sa partie gauche devient des cheveux
            xs = np.nonzero(visage.any(0))[0]
            larg = xs.max() - xs.min() + 1
            dx = max(1, round(larg * (0.12 if e.get('tourne') else 0.24)))
            face = np.zeros_like(visage)
            face[:, dx:] = visage[:, :-dx]
            face &= visage
            avant = s.copy()
            s[face] = avant[np.roll(face, -dx, axis=1)]
            libre = visage & ~face
            libre |= visage & (np.arange(W)[None, :] < xs.min() + dx + larg // 6)
            # en haut les cheveux, plus bas la joue et la mâchoire vues de côté (peau un peu plus sombre)
            y_oreille = y0 + int((y1 - y0) * 0.62)
            cheveux = libre.copy()
            cheveux[y_oreille + 1:] = False
            _peindre_cheveux(s, cheveux, base, y0 - 3, y1 + 1, 11)
            joue = libre & ~cheveux
            pk = _peau(avant) & visage
            if joue.any() and pk.any():
                s[joue] = np.median(avant[pk], axis=0) * 0.92
            if not e.get('tourne'):
                s, masque = _resserrer(s, masque, y1 + 1, 0.88)
    if vue == 'haut' and e.get('tablier'):
        _, sat, v = _hsv(s)
        blanc = masque & (sat < 0.18) & (v > 0.72)
        blanc[: int(H * 0.45)] = False
        autour = masque & ~blanc & (v > 0.25)
        autour[: int(H * 0.45)] = False
        if autour.any():
            iy, ix = nd.distance_transform_edt(~autour, return_distances=False, return_indices=True)
            s[blanc] = s[iy[blanc], ix[blanc]]
    if pas in (1, 3):
        s, masque = _marche(s, masque, vue, pas)
    return s, masque


def _decaler(s, masque, s2, m2, y_de, cols, dx, dy):
    """Copie le bas (rangées y_de…) des colonnes cols, décalé de (dx, dy), par-dessus s2/m2."""
    H, W = masque.shape
    for x in cols:
        nx = x + dx
        if not 0 <= nx < W:
            continue
        for y in range(y_de, H):
            ny = y + dy
            if 0 <= ny < H and masque[y, x]:
                s2[ny, nx] = s[y, x]
                m2[ny, nx] = True


def _marche(s, masque, vue, pas):
    """Pied gauche (pas 1) ou droit (pas 3) : le bas d'une moitié des jambes bouge."""
    H, W = masque.shape
    y_pieds = int(H * 0.82)
    cols = np.nonzero(masque[y_pieds:].any(0))[0]
    if len(cols) == 0:
        return s, masque
    xm = (cols.min() + cols.max()) // 2
    g, d = range(0, xm + 1), range(xm + 1, W)
    s2, m2 = s.copy(), masque.copy()
    m2[y_pieds:] = False
    if vue == 'droite':  # jambes écartées : une vers l'avant (droite), l'autre vers l'arrière
        av, ar = (d, g) if pas == 1 else (g, d)
        e = max(1, round(W * 0.07))
        _decaler(s, masque, s2, m2, y_pieds, ar, -e, 0)
        _decaler(s, masque, s2, m2, y_pieds, av, e, 0)
    else:  # de face ou de dos : un pied se lève
        lev = max(1, round(H * 0.045))
        haut, bas = (g, d) if pas == 1 else (d, g)
        _decaler(s, masque, s2, m2, y_pieds, bas, 0, 0)
        _decaler(s, masque, s2, m2, y_pieds, haut, 0, -lev)
    return s2, m2
