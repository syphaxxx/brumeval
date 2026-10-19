"""Acte 3 (étape « 20 heures », 2026-10-20) : l'île d'Aube, au-delà de l'orage.

    python outils/villes/acte3.py      # depuis la racine du dépôt ; une seule fois (refuse si l'acte 3 existe déjà)

Écrit 5 cartes (Havre-d'Aube, Bois des Lueurs, Marais Pâle, Gouffre d'Aube, Citadelle des Premiers Gardiens), la
jetée du port, les espèces d'Aube (sprites : variantes recolorées dans outils/sprites/variantes.json), les objets,
les événements et les conseils. Pour refaire après une modification : remettre les fichiers de data/ avec git, puis
relancer.
"""
import json
import os
import random
import sys
from collections import deque

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402

D = carte.R
W_OK = set('.,=FBagbkcinzpqOPD')


def load(rel):
    return json.load(open(D + rel, encoding='utf-8'))


if any(m == 'aube.json' for m in os.listdir(D + 'cartes')):
    sys.exit("L'acte 3 existe déjà (data/cartes/aube.json).")


class Map:
    def __init__(self, mid, nom, theme, w, h, fill, seed):
        self.mid, self.nom, self.theme = mid, nom, theme
        self.w, self.h = w, h
        self.g = [[fill] * w for _ in range(h)]
        self.rnd = random.Random(seed)
        self.m = {"id": mid, "nom": nom, "theme": theme, "batiments": [], "habitants": [], "coffres": [], "panneaux": [],
                  "passages": [], "zones": [], "boss": []}

    def put(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.g[y][x] = c

    def get(self, x, y):
        return self.g[y][x] if 0 <= x < self.w and 0 <= y < self.h else '#'

    def rect(self, x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.put(x, y, c)

    def disc(self, cx, cy, r, c, only=None):
        for y in range(cy - r, cy + r + 1):
            for x in range(cx - r - 1, cx + r + 2):
                if (x - cx) ** 2 * .8 + (y - cy) ** 2 <= r * r + .5 and (only is None or self.get(x, y) in only):
                    self.put(x, y, c)

    def blob(self, cx, cy, r, c, only=None):
        """Tache aux bords irréguliers (plusieurs cercles un peu décalés) : mares, vase, bosquets."""
        for _ in range(4):
            ox, oy = self.rnd.randint(-r // 2, r // 2), self.rnd.randint(-r // 2, r // 2)
            rr = max(1, r - self.rnd.randint(0, max(1, r // 2)))
            for y in range(cy + oy - rr, cy + oy + rr + 1):
                for x in range(cx + ox - rr - 1, cx + ox + rr + 2):
                    if (x - cx - ox) ** 2 * .7 + (y - cy - oy) ** 2 <= rr * rr + .3 and (only is None or self.get(x, y) in only):
                        self.put(x, y, c)

    def path(self, pts, c, width=1, keep=''):
        """Chemin d'un point à l'autre (segments horizontaux puis verticaux), largeur 1 à 3 ; sur l'eau : un pont."""
        def lay(x, y):
            for dx in range(width):
                for dy in range(width):
                    cur = self.get(x + dx, y + dy)
                    if cur in keep:
                        continue
                    self.put(x + dx, y + dy, 'B' if cur in '~B' else c)
        for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
            x, y = x0, y0
            while (x, y) != (x1, y1):
                lay(x, y)
                if x != x1:
                    x += 1 if x1 > x else -1
                elif y != y1:
                    y += 1 if y1 > y else -1
            lay(x1, y1)

    def sprinkle(self, n, c, on, x0=0, y0=0, x1=None, y1=None, away=0):
        x1 = self.w - 1 if x1 is None else x1
        y1 = self.h - 1 if y1 is None else y1
        for _ in range(n):
            x, y = self.rnd.randint(x0, x1), self.rnd.randint(y0, y1)
            if self.get(x, y) in on and all(self.get(x + dx, y + dy) in on + c for dx in range(-away, away + 1) for dy in range(-away, away + 1)):
                self.put(x, y, c)

    def warp(self, x, y, vers, tx, ty, d, cond=None, msg=None):
        p = {"x": x, "y": y, "vers": vers, "tx": tx, "ty": ty, "direction": d}
        if cond:
            p["condition"] = cond
        if msg:
            p["message"] = msg
        self.m["passages"].append(p)

    def npc(self, x, y, look, d, ev=None, lines=None, si=None, cache=None, roam=0):
        o = {"x": x, "y": y, "apparence": look, "direction": d}
        if ev:
            o["evenement"] = ev
        if cache:
            o["cache_si"] = cache
        if si:
            o["si"] = si
        if roam:
            o["promenade"] = roam
        if lines:
            o["dialogue"] = lines
        self.m["habitants"].append(o)

    def chest(self, x, y, item, q=1):
        self.m["coffres"].append({"x": x, "y": y, "objet": item, "quantite": q} if item != 'or' else {"x": x, "y": y, "or": q})

    def zone(self, x, y, l, h, lo, hi, n, pool):
        self.m["zones"].append({"x": x, "y": y, "l": l, "h": h, "niveau_min": lo, "niveau_max": hi, "max_ennemis": n, "creatures": pool})

    def boss(self, x, y, sp, flag, ev, si=None):
        b = {"x": x, "y": y, "espece": sp, "drapeau": flag}
        if ev != sp:  # comme mapToJson : l'événement n'est écrit que s'il diffère de l'espèce
            b["evenement"] = ev
        if si:
            b["si"] = si
        self.m["boss"].append(b)

    def reach(self, start):
        """Cases atteignables depuis start (habitants, coffres et boss bloquent)."""
        block = {(o['x'], o['y']) for k in ('habitants', 'coffres', 'panneaux') for o in self.m[k]}
        for b in self.m['batiments']:
            for x in range(b['x'], b['x'] + b['l']):
                for y in range(b['y'], b['y'] + b['h']):
                    block.add((x, y))
        for b in self.m['boss']:
            for dx in (0, 1):
                for dy in (0, 1):
                    block.add((b['x'] + dx, b['y'] + dy))
        seen, q = {start}, deque([start])
        while q:
            x, y = q.popleft()
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                n = (x + dx, y + dy)
                if n not in seen and 0 <= n[0] < self.w and 0 <= n[1] < self.h and self.g[n[1]][n[0]] in W_OK and n not in block:
                    seen.add(n)
                    q.append(n)
        return seen

    def save(self, extra=None):
        self.m["tuiles"] = [''.join(r) for r in self.g]
        order = ["id", "nom", "theme", "ambiance", "tuiles", "batiments", "habitants", "coffres", "panneaux", "passages",
                 "enigme", "zones", "boss", "declencheurs", "carte_monde"]
        m = {k: self.m[k] for k in order if k in self.m}
        carte.save(self.mid, m)

    def show(self):
        print(self.mid)
        for r in self.g:
            print(''.join(r))


# =====================================================================================================================
# 1. Havre-d'Aube : le village, sa plage et la jetée
# =====================================================================================================================
A = Map('aube', "Havre-d'Aube", 'vallee', 40, 30, '.', 31)
A.rect(0, 0, 39, 1, 'T'), A.rect(0, 0, 1, 29, 'T'), A.rect(38, 0, 39, 29, 'T')
A.rect(0, 24, 39, 29, '~')
A.rect(2, 22, 37, 23, '.')
A.path([(20, 2), (20, 23)], '=', 2)            # grand-rue (nord-sud), jusqu'à la jetée
A.path([(20, 0), (20, 2)], '=', 2)             # sortie nord : Bois des Lueurs
A.rect(20, 24, 21, 29, 'B')                    # jetée
A.rect(13, 10, 28, 16, 'q')                    # place pavée
A.rect(19, 12, 20, 13, 'W')                    # fontaine
A.path([(28, 13), (39, 13)], '=', 2)           # route de l'est : Marais Pâle
for x, y in [(12, 9), (29, 9), (12, 17), (29, 17)]:
    A.put(x, y, 'L')
A.put(27, 10, 'G')                             # tableau d'annonces
A.put(15, 16, 'Q'), A.put(24, 16, 'Q')
A.put(33, 20, 'Y')
A.put(15, 19, 'M'), A.put(17, 19, 'M')         # marché aux poissons
A.put(25, 20, 'C'), A.put(26, 20, 'C'), A.put(27, 20, 'v')
A.put(9, 21, 'X')                              # feu de camp sur la plage
for b in [dict(x=4, y=8, l=5, h=4, genre='soin', nom="Maison de soin", toit='#d0806a', evenement='aube_soin'),
          dict(x=31, y=8, l=5, h=4, genre='boutique1', nom="Comptoir d'Aube", toit='#e0b04a', evenement='aube_boutique'),
          dict(x=4, y=15, l=6, h=4, genre='auberge', nom="Auberge du Matin", toit='#c86a8a', evenement='auberge'),
          dict(x=31, y=15, l=5, h=4, genre='maison', nom="Maison d'Ilya", toit='#8a7ac8', evenement='aube_maison'),
          dict(x=25, y=3, l=8, h=4, genre='decor', nom="Sanctuaire des gardiens", toit='#b0a080', dessin='chateau')]:
    A.m['batiments'].append(b)
    A.rect(b['x'], b['y'], b['x'] + b['l'] - 1, b['y'] + b['h'] - 1, '.')
    dx = b['x'] + b['l'] // 2
    if b['genre'] != 'decor':
        if b['y'] < 13:  # maisons du haut : leur porte donne sur la route de la place
            A.path([(dx, b['y'] + b['h']), (dx, 13)], '=', 1, keep='qW')
        else:  # maisons du bas : un sentier vers la grand-rue
            A.path([(dx, b['y'] + b['h']), (dx, 20), (20, 20)], '=', 1, keep='qW=')
A.path([(6, 13), (13, 13)], '=', 1, keep='q')
A.sprinkle(26, 'F', '.', 2, 2, 37, 21)
A.sprinkle(18, 'T', '.', 2, 2, 37, 7, away=1)
A.sprinkle(6, 'U', '.', 2, 17, 37, 21)
A.sprinkle(5, 'R', '.', 2, 22, 37, 23)
A.warp(20, 29, 'port', 35, 20, 'gauche')
A.warp(21, 29, 'port', 35, 20, 'gauche')
A.warp(20, 0, 'lueurs', 23, 38, 'haut')
A.warp(21, 0, 'lueurs', 24, 38, 'haut')
A.warp(39, 13, 'marais', 1, 17, 'droite')
A.warp(39, 14, 'marais', 1, 18, 'droite')
A.npc(22, 14, 'mage_givre', 'bas', 'ilya')
A.npc(18, 20, 'marchande', 'droite', lines=["Poissons d'argent, pêchés au lever du soleil !", "Ici, le soleil se lève deux fois plus beau qu'ailleurs, dit-on."])
A.npc(11, 21, 'pecheur', 'droite', lines=["Mille ans que personne n'avait traversé l'orage. Et vous voilà !", "Les anciens disaient que les premiers gardiens venaient d'ici."])
A.npc(25, 11, 'enfant', 'bas', roam=2, lines=["Au nord, les arbres brillent la nuit ! Maman dit qu'il ne faut pas y aller.", "Le Gardien des Lueurs chasse tous ceux qui sentent la brume."])
A.npc(14, 15, 'ancien', 'droite', lines=["La Brume Noire s'est posée sur la Citadelle, au nord, il y a trois jours.", "Depuis, les créatures de l'île ont peur. Même les oiseaux ne chantent plus."])
A.npc(34, 12, 'villageoise', 'gauche', lines=["Le Marais Pâle, à l'est, est le royaume de sa Reine. Elle garde une larme de lumière…", "On dit que cette larme ouvre les portes de la Citadelle."])
A.npc(23, 21, 'chat', 'gauche', roam=1, lines=["Le chat du port d'Aube dort au soleil."])
A.npc(30, 21, 'villageois', 'bas', 'tom_aube', si={"drapeau": "quete:rival:fin"})
A.npc(21, 23, 'voyageuse', 'droite', 'ysolde_aube')
A.m['declencheurs'] = [{"x": 20, "y": 21, "l": 2, "h": 1, "evenement": "aube_arrivee", "jusqua": "aube_vue"}]
A.m['carte_monde'] = [10, 92]
PROBLEMES = []
r = A.reach((20, 22))
for p in [(20, 28), (20, 2), (38, 13), (6, 12), (33, 12), (7, 19), (33, 19)]:
    if p not in r:
        PROBLEMES.append(('aube', p))

# =====================================================================================================================
# 2. Bois des Lueurs : forêt lumineuse, le Gardien barre le chemin du nord
# =====================================================================================================================
L = Map('lueurs', "Bois des Lueurs", 'foret', 48, 40, 'T', 32)
L.m['ambiance'] = {"effet": "lucioles"}
L.path([(23, 39), (23, 33), (14, 33), (14, 24), (30, 24), (30, 16), (22, 16), (22, 9)], '=', 2)
L.path([(23, 2), (23, 9)], '=', 2)
L.path([(23, 0), (23, 2)], '=', 2)
for cx, cy, r in [(14, 33, 4), (30, 31, 4), (8, 22, 4), (22, 24, 4), (38, 20, 5), (30, 12, 4), (12, 12, 5), (38, 33, 3), (6, 32, 3)]:
    L.disc(cx, cy, r, ',', only='T')
for cx, cy, r in [(22, 16, 3), (23, 34, 2)]:
    L.disc(cx, cy, r, '.', only='T,')
L.path([(30, 24), (38, 24), (38, 20)], '.', 1)
L.path([(14, 24), (8, 24), (8, 22)], '.', 1)
L.path([(22, 16), (12, 16), (12, 12)], '.', 1)
L.path([(30, 16), (30, 12)], '.', 1)
L.path([(14, 33), (6, 33), (6, 32)], '.', 1)
L.path([(30, 31), (38, 31), (38, 33)], '.', 1)
L.path([(23, 31), (30, 31)], '.', 1)
L.sprinkle(40, 'F', ',.', 2, 2, 45, 37)
L.sprinkle(14, 'U', ',', 2, 2, 45, 37, away=1)
L.disc(40, 8, 3, '~', only='T')               # petit étang
L.path([(30, 12), (36, 8)], '.', 1)
for x in range(21, 26):  # le passage du nord : deux cases de large entre les arbres, le Gardien les bouche
    for y in range(3, 9):
        if x not in (23, 24):
            L.put(x, y, 'T')
L.boss(23, 5, 'gardien_lueurs', 'gardien_lueurs', 'gardien_lueurs')
L.warp(23, 39, 'aube', 20, 1, 'bas'), L.warp(24, 39, 'aube', 21, 1, 'bas')
L.warp(23, 0, 'gouffre', 17, 28, 'haut', {"drapeau": "gardien_lueurs"}, "Le Gardien des Lueurs barre le chemin.")
L.warp(24, 0, 'gouffre', 18, 28, 'haut', {"drapeau": "gardien_lueurs"}, "Le Gardien des Lueurs barre le chemin.")
L.zone(0, 18, 48, 22, 55, 59, 4, ['lueurine', 'loup_aube', 'aubelle', 'aubespectre'])
L.zone(0, 0, 48, 18, 57, 60, 4, ['lueurine', 'loup_aube', 'aubespectre', 'rocaube', 'nacrelin'])
L.chest(8, 20, 'elixir'), L.chest(39, 18, 'gemme_esprit'), L.chest(12, 10, 'lanterne_or', 3), L.chest(36, 7, 'or', 3000)
L.chest(6, 31, 'superpotion', 3), L.chest(39, 33, 'plume', 2)
L.npc(20, 34, 'chevalier', 'droite', lines=["Je suis venue avec Ilya pour garder le bois. Le Gardien ne nous laisse plus passer.", "Il croit que tout ce qui vient d'ailleurs apporte la brume."])
L.m['carte_monde'] = [8, 76]
L.m['declencheurs'] = [{"x": 23, "y": 37, "l": 2, "h": 2, "evenement": "lueurs_entree", "jusqua": "lueurs_vu"}]
r = L.reach((23, 38))
for p in [(8, 21), (39, 19), (12, 11), (36, 8), (6, 32), (39, 32), (23, 9)]:
    if p not in r:
        PROBLEMES.append(('lueurs', p))

# =====================================================================================================================
# 3. Marais Pâle : la Reine du Marais garde la Larme d'Aube
# =====================================================================================================================
M = Map('marais', "Marais Pâle", 'foret', 48, 36, '.', 33)
M.m['ambiance'] = {"effet": "brume"}
M.rect(0, 0, 47, 1, 'T'), M.rect(0, 34, 47, 35, 'T'), M.rect(46, 0, 47, 35, 'T'), M.rect(0, 0, 0, 35, 'T')
for _ in range(14):  # la vase (rencontres) en grandes taches
    M.blob(M.rnd.randint(4, 43), M.rnd.randint(4, 31), M.rnd.randint(3, 6), 'z', only='.')
for cx, cy, r in [(10, 7, 3), (23, 28, 4), (34, 8, 3), (39, 27, 3), (15, 21, 2), (27, 16, 2), (5, 28, 2)]:
    M.blob(cx, cy, r, '~', only='.z')
for _ in range(9):  # bosquets
    M.blob(M.rnd.randint(3, 44), M.rnd.randint(3, 32), 2, 'T', only='.z')
M.sprinkle(40, ',', '.', 2, 2, 44, 32)
M.sprinkle(16, 'd', '.z', 2, 2, 44, 32, away=1)
M.sprinkle(20, 'F', '.', 2, 2, 44, 32)
M.path([(0, 17), (8, 17), (8, 12), (20, 12), (20, 22), (32, 22), (32, 15), (40, 15)], '=', 2)
M.rect(39, 12, 44, 20, '.')                   # l'îlot de la Reine
M.path([(8, 17), (8, 28), (14, 28)], '.', 1)
M.path([(20, 12), (20, 4), (28, 4)], '.', 1)
M.path([(32, 22), (32, 30), (42, 30)], '.', 1)
M.boss(41, 15, 'reine_marais', 'reine_marais', 'reine_marais')
M.warp(0, 17, 'aube', 38, 13, 'gauche'), M.warp(0, 18, 'aube', 38, 14, 'gauche')
M.zone(0, 0, 38, 36, 57, 61, 4, ['crapale', 'vasepent', 'aubespectre', 'lueurine'])
M.zone(38, 0, 10, 36, 59, 62, 4, ['crapale', 'vasepent', 'rocaube'])
M.chest(14, 28, 'gemme_vie'), M.chest(28, 4, 'elixir', 2), M.chest(42, 30, 'gemme_force'), M.chest(43, 19, 'or', 4000)
M.npc(6, 18, 'braconnier', 'droite', lines=["Le marais avale les imprudents. Restez sur le chemin !", "La Reine vit sur l'îlot, tout à l'est. Elle ne laisse personne approcher de sa larme."])
M.m['carte_monde'] = [24, 90]
r = M.reach((1, 17))
for p in [(14, 27), (28, 5), (42, 29), (40, 15)]:
    if p not in r:
        PROBLEMES.append(('marais', p))

# =====================================================================================================================
# 4. Gouffre d'Aube : grotte obscure, énigme des rochers, porte de lumière au nord
# =====================================================================================================================
G = Map('gouffre', "Gouffre d'Aube", 'grotte', 36, 30, '#', 34)
G.m['ambiance'] = {"effet": "obscurite"}
G.path([(17, 29), (17, 22), (8, 22), (8, 14), (17, 14), (17, 6), (17, 0)], 'c', 2)
G.disc(8, 18, 3, 'c', only='#')
G.disc(17, 10, 3, 'c', only='#')
G.disc(24, 24, 3, 'c', only='#')
G.path([(19, 23), (24, 23)], 'c', 1)
G.path([(4, 8), (8, 8), (8, 14)], 'c', 1)
G.rect(23, 8, 31, 15, '.')                    # salle de l'énigme
G.path([(19, 11), (23, 11)], '.', 1)
G.put(27, 11, 'O'), G.put(29, 13, 'O')        # rochers
G.put(27, 9, 'P'), G.put(31, 13, 'P')         # dalles
G.rect(32, 9, 34, 11, '#')
G.put(32, 10, 'D'), G.put(33, 10, '.'), G.put(34, 10, '.')   # grille, puis l'alcôve au trésor
G.m['enigme'] = {"drapeau": "gouffre_grille", "x": 24, "y": 11, "message": "Les dalles s'enfoncent : la grille de l'alcôve se lève !",
                 "zone": {"x": 23, "y": 8, "l": 12, "h": 8}}
G.chest(34, 10, 'gemme_maitre')
G.chest(4, 8, 'elixir', 2), G.chest(25, 25, 'gemme_soin'), G.chest(6, 18, 'or', 5000)
G.m['panneaux'] = [{"x": 22, "y": 10, "texte": "Gravé dans la roche : « Deux pierres, deux dalles, et le trésor des gardiens sera à vous. »"}]
G.warp(17, 29, 'lueurs', 23, 1, 'bas'), G.warp(18, 29, 'lueurs', 24, 1, 'bas')
G.warp(17, 0, 'citadelle', 14, 38, 'haut', {"objet": "larme_aube"}, "Une porte de lumière ferme le passage. Elle attend quelque chose qui brille comme l'aube…")
G.warp(18, 0, 'citadelle', 15, 38, 'haut', {"objet": "larme_aube"}, "Une porte de lumière ferme le passage. Elle attend quelque chose qui brille comme l'aube…")
G.zone(0, 0, 36, 30, 60, 63, 4, ['rocaube', 'aubespectre', 'vasepent', 'nacrelin'])
G.m['carte_monde'] = [6, 62]
r = G.reach((17, 28))
for p in [(5, 8), (25, 24), (6, 17), (24, 11), (17, 1)]:
    if p not in r:
        PROBLEMES.append(('gouffre', p))

# =====================================================================================================================
# 5. Citadelle des Premiers Gardiens : salles de cristal, la Brume Noire au sommet
# =====================================================================================================================
C = Map('citadelle', "Citadelle des gardiens", 'grotte', 30, 40, '#', 35)
C.path([(14, 39), (14, 30)], '.', 2)
C.rect(5, 24, 24, 30, 'c')                    # grande salle
C.rect(12, 14, 17, 24, '.')                   # couloir central
C.rect(3, 15, 9, 21, 'c'), C.rect(20, 15, 26, 21, 'c')   # salles latérales
C.path([(9, 18), (12, 18)], 'c', 1), C.path([(17, 18), (20, 18)], 'c', 1)
C.rect(8, 2, 21, 13, '.')                     # salle du trône
for x, y in [(9, 3), (20, 3), (9, 12), (20, 12), (5, 25), (24, 25), (5, 29), (24, 29), (3, 16), (26, 16), (3, 20), (26, 20)]:
    C.put(x, y, 'x')
for y in range(5, 12, 2):
    C.put(10, y, 'x'), C.put(19, y, 'x')
C.boss(14, 4, 'brume_noire', 'aube_fin', 'brume_noire')
C.warp(14, 39, 'gouffre', 17, 1, 'bas'), C.warp(15, 39, 'gouffre', 18, 1, 'bas')
C.zone(0, 14, 30, 26, 62, 66, 4, ['rocaube', 'aubespectre', 'nacrelin', 'loup_aube'])
C.chest(4, 16, 'gemme_orage'), C.chest(25, 16, 'gemme_maitre'), C.chest(6, 29, 'elixir', 3), C.chest(23, 29, 'plume', 3)
C.npc(16, 14, 'mage_givre', 'bas', 'ilya_citadelle')
C.m['carte_monde'] = [16, 60]
r = C.reach((14, 38))
for p in [(4, 17), (25, 17), (6, 28), (23, 28), (14, 6), (16, 13)]:
    if p not in r:
        PROBLEMES.append(('citadelle', p))

if PROBLEMES:
    for mm in (A, L, M, G, C):
        if any(pb[0] == mm.mid for pb in PROBLEMES):
            mm.show()
    sys.exit('cases inaccessibles : %s' % PROBLEMES)
for mm in (A, L, M, G, C):
    mm.save()

# =====================================================================================================================
# La jetée du port (vers Aube, après le Seigneur des Orages)
# =====================================================================================================================
P = carte.load('port')
rows = [list(r) for r in P['tuiles']]
for x in (34, 35, 36):  # la jetée de l'est
    rows[20][x] = 'B'
P['tuiles'] = [''.join(r) for r in rows]
P['passages'].append({"x": 36, "y": 20, "vers": "aube", "tx": 20, "ty": 23, "direction": "haut", "condition": {"drapeau": "boss5"},
                      "message": "Au-delà de la jetée, l'orage tourne encore. Aucun bateau ne passerait."})
P['habitants'].append({"x": 32, "y": 19, "apparence": "voyageuse", "direction": "bas", "evenement": "ysolde"})
carte.save('port', P)

print('acte 3 : cartes écrites')
