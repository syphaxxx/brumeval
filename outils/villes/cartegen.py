"""Petit outil commun pour écrire des cartes de Brumeval par script (acte3.py, donjons.py) : grille, chemins, taches,
habitants, coffres, passages, zones, boss, vérification d'accès à pied, écriture au format de writeJson."""
import random
from collections import deque

import carte

W_OK = set('.,=FBagbkcinzpqOPD')


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
