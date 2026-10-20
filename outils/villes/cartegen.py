"""Petit outil commun pour écrire des cartes de Brumeval par script (acte3.py, donjons.py) : grille, chemins, taches,
habitants, coffres, passages, zones, boss, vérification d'accès à pied, écriture au format de writeJson."""
import random
import sys
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
        order = ["id", "nom", "theme", "ambiance", "arbres", "tuiles", "batiments", "habitants", "coffres", "panneaux", "passages",
                 "enigme", "zones", "boss", "declencheurs", "carte_monde"]
        m = {k: self.m[k] for k in order if k in self.m}
        carte.save(self.mid, m)

    def show(self):
        print(self.mid)
        for r in self.g:
            print(''.join(r))


# Donjons : salles reliées par des couloirs, entrée en bas, boss dans la salle la plus loin (donjons.py, corail.py)
def dungeon(mid, nom, theme, w, h, seed, floor, enc, wall='#', nrooms=9, puzzle=False, decor='R'):
    """Salles + couloirs. floor : sol des salles ; enc : sol des couloirs (rencontres). Rend (carte, entrée, salle du
    boss, autres salles)."""
    M = Map(mid, nom, theme, w, h, wall, seed)
    rnd = M.rnd
    rooms = []
    tries = 0
    while len(rooms) < nrooms and tries < 2000:
        tries += 1
        rw, rh = rnd.randint(5, 9), rnd.randint(4, 7)
        x, y = rnd.randint(2, w - rw - 3), rnd.randint(2, h - rh - 6)
        if any(x < r[0] + r[2] + 2 and r[0] < x + rw + 2 and y < r[1] + r[3] + 2 and r[1] < y + rh + 2 for r in rooms):
            continue
        rooms.append((x, y, rw, rh))
    centers = [(x + rw // 2, y + rh // 2) for x, y, rw, rh in rooms]
    # couloirs : arbre couvrant (le plus proche déjà relié), puis deux boucles
    linked = [0]
    for _ in range(1, len(rooms)):
        best = None
        for i in range(len(rooms)):
            if i in linked:
                continue
            for j in linked:
                d = abs(centers[i][0] - centers[j][0]) + abs(centers[i][1] - centers[j][1])
                if best is None or d < best[0]:
                    best = (d, i, j)
        _, i, j = best
        M.path([centers[j], (centers[i][0], centers[j][1]), centers[i]], enc, 2)
        linked.append(i)
    for _ in range(2):
        i, j = rnd.sample(range(len(rooms)), 2)
        M.path([centers[i], (centers[j][0], centers[i][1]), centers[j]], enc, 1)
    for x, y, rw, rh in rooms:
        M.rect(x, y, x + rw - 1, y + rh - 1, floor)
        for cx, cy in ((x, y), (x + rw - 1, y), (x, y + rh - 1), (x + rw - 1, y + rh - 1)):  # décor dans les coins
            if rnd.random() < .7:
                M.put(cx, cy, rnd.choice(decor))
    # entrée : la salle la plus basse, reliée au bord du bas
    low = max(range(len(rooms)), key=lambda i: rooms[i][1] + rooms[i][3])
    ex = centers[low][0]
    M.path([(ex, h - 1), centers[low]], floor, 2)
    # boss : la salle la plus loin de l'entrée (à pied)
    dist = {}
    q = deque([(ex, h - 2)])
    dist[(ex, h - 2)] = 0
    while q:
        x, y = q.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            n = (x + dx, y + dy)
            if n not in dist and 0 <= n[0] < w and 0 <= n[1] < h and M.get(*n) in W_OK:
                dist[n] = dist[(x, y)] + 1
                q.append(n)
    far = max(range(len(rooms)), key=lambda i: dist.get(centers[i], -1))
    others = [i for i in range(len(rooms)) if i not in (far, low)]
    if puzzle:  # le gabarit de l'énigme dans la plus grande autre salle (agrandie à 9 × 8, alcôve à droite)
        pi = max(others, key=lambda i: rooms[i][2] * rooms[i][3])
        others.remove(pi)
        x, y = rooms[pi][0], rooms[pi][1]
        x = min(x, w - 15)
        y = min(y, h - 14)
        M.path([centers[pi], (x + 1, y + 6)], floor, 1)  # relié au reste d'abord : le gabarit passe par-dessus
        M.rect(x, y, x + 8, y + 7, '.')
        M.put(x + 4, y + 3, 'O'), M.put(x + 6, y + 5, 'O')
        M.put(x + 4, y + 1, 'P'), M.put(x + 8, y + 5, 'P')
        M.rect(x + 9, y + 1, x + 11, y + 3, wall)
        M.put(x + 9, y + 2, 'D'), M.put(x + 10, y + 2, '.'), M.put(x + 11, y + 2, '.')
        M.m['enigme'] = {"drapeau": mid + "_grille", "x": x + 1, "y": y + 3,
                         "message": "Les dalles s'enfoncent : la grille de l'alcôve se lève !", "zone": {"x": x, "y": y, "l": 12, "h": 8}}
        M.chest(x + 11, y + 2, 'gemme_maitre' if mid != 'mines' else 'gemme_esprit')
        rooms[pi] = (x, y, 9, 8)
    return M, ex, rooms[far], [rooms[i] for i in others]


def finish(M, ex, boss_room, others, boss, chests, back, cond, msg, zone, look_room_npc=None):
    bx, by = boss_room[0] + boss_room[2] // 2 - 1, boss_room[1] + 1
    M.rect(boss_room[0], boss_room[1], boss_room[0] + boss_room[2] - 1, boss_room[1] + boss_room[3] - 1, '.')
    M.boss(bx, by, boss, M.mid + '_boss', M.mid + '_boss')
    for (x, y, rw, rh), item in zip(others, chests):
        M.chest(x + rw - 2, y + 1, *(item if isinstance(item, tuple) else (item,)))
    M.warp(ex, M.h - 1, back[0], back[1], back[2], 'bas')
    M.warp(ex + 1, M.h - 1, back[0], back[1], back[2], 'bas')
    M.zone(0, 0, M.w, M.h, *zone)
    r = M.reach((ex, M.h - 2))
    for o in M.m['coffres']:
        if not any(abs(o['x'] - x) + abs(o['y'] - y) == 1 for x, y in r) and not M.m.get('enigme'):
            raise SystemExit('%s : coffre inaccessible %s' % (M.mid, o))
    if not any(abs(bx - x) <= 2 and abs(by - y) <= 2 for x, y in r):
        raise SystemExit('%s : boss inaccessible' % M.mid)
    return M
