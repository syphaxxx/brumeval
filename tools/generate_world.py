#!/usr/bin/env python3
"""Générateur des cartes de Brumeval.

Usage :  python3 tools/generate_world.py src/world_data.cpp

Construit les trois cartes (Vallée de Brumeval, Monts Cendrelune, Grotte des
Échos), y place bâtiments, habitants, coffres, panneaux, passages, zones de
rencontres et boss, vérifie que tout est accessible à pied, puis écrit le
fichier C++ src/world_data.cpp. Le script s'arrête avec une erreur si un lieu
est inaccessible. Les fonctions vallee(), cendres() et grotte() décrivent
chaque carte : c'est l'endroit le plus simple pour agrandir ou modifier le monde.
"""
import random, sys
from collections import deque

WALK = set('.,=FBagbkc')

class M:
    def __init__(s, w, h, fill):
        s.w, s.h = w, h
        s.g = [[fill] * w for _ in range(h)]
        s.buildings, s.npcs, s.chests, s.signs, s.warps, s.zones, s.bosses = [], [], [], [], [], [], []
    def set(s, x, y, c):
        if 0 <= x < s.w and 0 <= y < s.h: s.g[y][x] = c
    def get(s, x, y): return s.g[y][x]
    def rect(s, x, y, w, h, c):
        for yy in range(y, y + h):
            for xx in range(x, x + w): s.set(xx, yy, c)
    def ell(s, cx, cy, rx, ry, c, only=None):
        for y in range(cy - ry, cy + ry + 1):
            for x in range(cx - rx, cx + rx + 1):
                if ((x - cx) / (rx + .5)) ** 2 + ((y - cy) / (ry + .5)) ** 2 <= 1:
                    if 0 <= x < s.w and 0 <= y < s.h and (only is None or s.g[y][x] in only): s.set(x, y, c)
    def path(s, pts, c='=', bridge='B', water='~'):
        for (x1, y1), (x2, y2) in zip(pts, pts[1:]):
            for x in range(min(x1, x2), max(x1, x2) + 1): s.pave(x, y1, c, bridge, water)
            for y in range(min(y1, y2), max(y1, y2) + 1): s.pave(x2, y, c, bridge, water)
    def pave(s, x, y, c, bridge, water):
        s.set(x, y, bridge if s.get(x, y) in water else c)
    def scatter(s, x, y, w, h, c, d, rnd, only='.'):
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                if 0 <= xx < s.w and 0 <= yy < s.h and s.g[yy][xx] in only and rnd.random() < d: s.g[yy][xx] = c
    def border(s, c):
        for x in range(s.w): s.set(x, 0, c); s.set(x, s.h - 1, c)
        for y in range(s.h): s.set(0, y, c); s.set(s.w - 1, y, c)
    def building(s, x, y, w, h, kind, name, roof, ground):
        s.rect(x - 1, y - 1, w + 2, h + 2, ground)  # dégage autour
        s.buildings.append((x, y, w, h, kind, name, roof))
    def npc(s, x, y, look, d, script, lines): s.npcs.append((x, y, look, d, script, lines))
    def chest(s, x, y, item, qty): s.chests.append((x, y, item, qty))
    def sign(s, x, y, text): s.signs.append((x, y, text))
    def warp(s, x, y, m, tx, ty, d): s.warps.append((x, y, m, tx, ty, d))
    def zone(s, x, y, w, h, lo, hi, mx, pool): s.zones.append((x, y, w, h, lo, hi, mx, pool))
    def boss(s, x, y, i, flag): s.bosses.append((x, y, i, flag))

D = {'up': 0, 'down': 1, 'left': 2, 'right': 3}

# ===================================================================== Vallée
def vallee():
    m = M(64, 44, '.'); rnd = random.Random(7)
    m.scatter(1, 19, 21, 24, 'T', .55, rnd)               # Bois Murmurant
    m.scatter(45, 19, 13, 24, 'T', .3, rnd)               # bosquet du col
    m.scatter(26, 1, 37, 18, 'T', .05, rnd)
    m.scatter(26, 1, 37, 18, 'R', .02, rnd)
    m.scatter(22, 19, 23, 24, 'T', .08, rnd)
    m.scatter(1, 1, 62, 42, 'F', .035, rnd)
    m.rect(1, 1, 25, 17, '.')                              # village
    m.scatter(1, 1, 25, 17, 'F', .04, rnd)
    for e in [(30, 4, 4, 2), (41, 5, 5, 3), (53, 5, 6, 3), (47, 14, 5, 2), (58, 13, 3, 3), (29, 14, 3, 2), (39, 16, 4, 2),
              (49, 24, 3, 2), (54, 37, 3, 3), (50, 40, 3, 2), (37, 36, 4, 2), (42, 29, 3, 2), (24, 20, 3, 2)]:
        m.ell(*e, ',', only='.F')
    for e in [(8, 27, 4, 2), (16, 35, 3, 2), (6, 35, 2, 2), (15, 22, 3, 2)]:
        m.ell(*e, ',')
    m.ell(27, 30, 6, 4, '~')                               # lac
    m.rect(35, 0, 2, 24, '~'); m.rect(30, 22, 7, 2, '~'); m.rect(30, 22, 2, 6, '~')
    m.rect(27, 33, 2, 11, '~')                             # déversoir
    m.rect(58, 18, 6, 26, '#'); m.rect(56, 22, 2, 3, '#'); m.rect(57, 38, 1, 5, '#')
    m.border('T')
    for y in range(0, 1): m.set(35, y, '~'); m.set(36, y, '~')
    m.set(27, 43, '~'); m.set(28, 43, '~')
    m.rect(58, 32, 6, 2, '=')                              # col
    # Village
    m.building(5, 3, 5, 4, 'soin', 'Maison de soin', 0xb8483e, '.')
    m.building(18, 3, 5, 4, 'boutique1', 'Boutique', 0x3f6fb0, '.')
    m.building(4, 11, 5, 4, 'chapelle', 'Chapelle', 0x8a6fb8, '.')
    m.building(17, 11, 5, 4, 'maison', "Maison de l'ancien", 0x7a8a4a, '.')
    m.rect(15, 6, 2, 2, 'W')
    m.path([(2, 9), (46, 9)]); m.path([(13, 2), (13, 18), (10, 18), (10, 24), (4, 24), (4, 32), (14, 32), (14, 39), (8, 39)])
    m.path([(7, 7), (7, 8)]); m.path([(20, 7), (20, 8)]); m.path([(6, 15), (6, 16), (23, 16)]); m.path([(19, 15), (19, 16)])
    m.path([(23, 16), (19, 16)]); m.path([(19, 16), (19, 37), (24, 37), (24, 40), (46, 40), (46, 9)]); m.path([(46, 32), (58, 32)])
    m.set(7, 39, '.')
    # Contenu
    m.sign(12, 10, "Village de Brumeval. Maison de soin au nord-ouest, boutique au nord-est.")
    m.sign(11, 19, "Bois Murmurant. On dit qu'une herbe lunaire pousse tout au fond des bois.")
    m.sign(55, 33, "Col de la Brume. Au-delà s'étendent les Monts Cendrelune.")
    m.sign(34, 10, "Pont du Ru-Clair.")
    m.chest(7, 39, 'herbe', 1); m.chest(17, 24, 'potion', 2); m.chest(61, 2, 'ether', 1); m.chest(55, 21, 'plume', 1)
    m.chest(48, 41, 'lanterne', 3); m.chest(3, 24, '', 80); m.chest(31, 41, 'potion', 3); m.chest(2, 2, 'lanterne', 2)
    m.npc(11, 8, 4, D['down'], '', ["Bienvenue à Brumeval !",
          "Appuyez sur Échap pour ouvrir le menu : équipe, objets, magie et sauvegarde."])
    m.npc(17, 10, 8, D['down'], '', ["La maison au toit rouge, c'est celle de la guérisseuse. Elle soigne gratuitement.",
          "La boutique, juste à côté, vend potions, éthers et lanternes."])
    m.npc(21, 15, 5, D['down'], 'ancien', [])
    m.npc(7, 15, 1, D['down'], 'maelle', [])
    m.npc(25, 8, 6, D['left'], '', ["Les créatures affaiblies se capturent beaucoup plus facilement avec une lanterne !",
          "Moi, j'attends qu'il leur reste presque plus de PV."])
    m.npc(25, 38, 9, D['right'], 'pecheur', [])
    m.npc(55, 31, 7, D['down'], 'garde_col', [])
    m.npc(44, 8, 4, D['left'], '', ["Les sorts coûtent des PM.",
          "Un éther en rend 25, et la guérisseuse les restaure aussi."])
    m.npc(41, 24, 8, D['down'], '', ["Contre les créatures d'Ombre, la magie de Lumière fait des merveilles.",
          "La mage de la chapelle en connaît un rayon…"])
    m.warp(63, 32, 1, 1, 33, D['right']); m.warp(63, 33, 1, 1, 34, D['right'])
    m.zone(25, 1, 16, 18, 2, 4, 2, ['piafouine', 'mulotin', 'champichou'])
    m.zone(41, 1, 22, 18, 4, 7, 3, ['lucioline', 'grenouillon', 'piafouine', 'champichou'])
    m.zone(1, 19, 22, 24, 4, 7, 3, ['champichou', 'mulotin', 'lucioline', 'piafouine'])
    m.zone(41, 19, 17, 24, 7, 10, 3, ['rocaillou', 'grenouillon', 'lucioline', 'brumelin'])
    m.zone(0, 0, 64, 44, 3, 6, 2, ['piafouine', 'grenouillon', 'mulotin'])
    m.boss(59, 32, 'sylvarque', 'boss1')
    return m, (12, 9)

# ============================================================ Monts Cendrelune
def cendres():
    m = M(64, 44, 'a'); rnd = random.Random(11)
    m.scatter(1, 1, 62, 42, 'd', .035, rnd, only='a')
    m.scatter(1, 1, 62, 42, 'R', .025, rnd, only='a')
    m.scatter(1, 1, 62, 42, 'm', .015, rnd, only='a')
    m.rect(30, 1, 2, 24, 'l'); m.rect(30, 24, 28, 2, 'l')
    m.rect(38, 1, 25, 8, 'm'); m.rect(42, 2, 14, 3, 'a'); m.set(53, 2, 'l'); m.set(54, 2, 'l'); m.set(54, 3, 'l'); m.set(43, 2, 'l')
    for e in [(10, 8, 5, 3), (18, 15, 4, 3), (6, 18, 3, 2), (22, 4, 3, 2), (36, 13, 4, 2), (50, 14, 6, 3), (58, 19, 3, 2),
              (40, 20, 4, 2), (36, 38, 4, 2), (52, 36, 6, 3), (58, 29, 3, 3), (47, 41, 3, 1), (13, 21, 3, 1)]:
        m.ell(*e, 'g', only='adR')
    m.border('m')
    m.rect(1, 26, 27, 16, 'a')                             # Forgeroc
    m.building(5, 27, 5, 4, 'soin', 'Maison de soin', 0xb8483e, 'a')
    m.building(12, 27, 5, 4, 'boutique2', 'Boutique', 0x3f6fb0, 'a')
    m.building(19, 27, 5, 4, 'auberge', 'Auberge du Tison', 0x9a5a2a, 'a')
    m.building(6, 35, 6, 4, 'forge', 'Forge de Brann', 0x4a4a52, 'a')
    m.building(18, 35, 5, 4, 'maison', 'Maison', 0x7a6a4a, 'a')
    m.rect(41, 27, 7, 3, 'm'); m.set(44, 29, 'k')
    m.path([(0, 33), (44, 33)]); m.path([(0, 34), (2, 34)])
    m.path([(7, 31), (7, 32)]); m.path([(14, 31), (14, 32)]); m.path([(21, 31), (21, 32)])
    m.path([(16, 34), (16, 40), (9, 40)]); m.path([(9, 39), (9, 40)]); m.path([(16, 40), (20, 40), (20, 39)])
    m.path([(44, 30), (44, 33)]); m.path([(26, 33), (26, 10), (46, 10), (46, 5), (48, 5), (48, 4)], bridge='b', water='l')
    m.rect(42, 2, 14, 3, 'a'); m.set(53, 2, 'l'); m.set(54, 2, 'l'); m.set(54, 3, 'l')
    m.sign(3, 32, "Forgeroc, la ville des forges.")
    m.sign(27, 11, "Vers le cratère. Ignarok y dort, dit-on.")
    m.sign(45, 31, "Grotte des Échos.")
    m.chest(60, 41, 'superpotion', 2); m.chest(12, 4, 'ether', 2); m.chest(60, 11, 'plume', 2); m.chest(34, 41, '', 300)
    m.chest(3, 22, 'lanterne_argent', 3); m.chest(55, 30, 'ether', 1); m.chest(24, 2, 'superpotion', 1)
    m.npc(4, 35, 7, D['down'], '', ["Une jeune mage est partie explorer la Grotte des Échos, à l'est.",
          "Elle n'est jamais revenue… On entend des grondements là-dedans."])
    m.npc(11, 32, 9, D['down'], '', ["Ignarok, le cœur du volcan, est une créature de Feu.",
          "L'Eau est sa grande faiblesse. Les sorts Givre et Blizzard devraient le faire vaciller."])
    m.npc(24, 35, 6, D['left'], '', ["Brann, c'est le plus fort de Forgeroc !",
          "Il ne suit que ceux qui le battent en duel."])
    m.npc(11, 40, 2, D['left'], 'brann', [])
    m.npc(47, 10, 7, D['down'], 'garde_volcan', [])
    m.npc(33, 35, 8, D['down'], '', ["Ici, les créatures sont bien plus fortes que dans la vallée.",
          "Pensez à faire tourner votre équipe pour que tout le monde progresse."])
    m.npc(26, 30, 4, D['left'], '', ["Les lanternes d'argent de la boutique attrapent presque tout.",
          "Pratique pour les créatures têtues des montagnes."])
    m.warp(0, 33, 0, 62, 32, D['left']); m.warp(0, 34, 0, 62, 33, D['left'])
    m.warp(44, 29, 2, 15, 21, D['up'])
    m.zone(1, 1, 29, 23, 11, 14, 3, ['tisonnel', 'galetor', 'voltigeon'])
    m.zone(32, 9, 31, 15, 14, 17, 3, ['voltigeon', 'glaconnet', 'fumerole', 'tisonnel'])
    m.zone(28, 26, 35, 17, 12, 15, 3, ['galetor', 'tisonnel', 'voltigeon', 'glaconnet'])
    m.zone(0, 0, 64, 44, 12, 15, 3, ['tisonnel', 'galetor'])
    m.boss(48, 2, 'ignarok', 'boss2')
    return m, (1, 33)

# ===================================================================== Grotte
def grotte():
    m = M(32, 24, '#')
    for r in [(14, 18, 4, 4), (15, 10, 2, 8), (4, 10, 12, 2), (4, 12, 2, 8), (2, 18, 6, 4), (15, 3, 2, 7), (17, 3, 10, 2),
              (27, 2, 4, 5), (17, 13, 8, 2), (23, 15, 2, 5), (21, 19, 6, 3), (8, 5, 7, 2), (8, 2, 2, 3)]:
        m.rect(*r, 'c')
    m.set(15, 22, 'k')
    for p in [(2, 18), (7, 21), (26, 21), (30, 6), (14, 21), (21, 21)]: m.set(*p, 'x')
    m.chest(3, 20, 'superpotion', 1); m.chest(25, 20, 'ether', 2); m.chest(28, 6, 'plume', 1); m.chest(8, 2, '', 150)
    m.npc(29, 4, 3, D['left'], 'isra', [])
    m.sign(13, 19, "Grotte des Échos. Le sol tremble sous vos pieds.")
    m.warp(15, 22, 1, 44, 30, D['down'])
    m.zone(0, 0, 32, 24, 13, 16, 3, ['galetor', 'fumerole', 'glaconnet', 'tisonnel'])
    m.boss(24, 3, 'golem', 'golem')
    return m, (15, 21)

# ================================================================ Vérification
def check(name, m, start):
    blocked = set()
    for (x, y, w, h, *_ ) in m.buildings:
        for yy in range(y, y + h):
            for xx in range(x, x + w): blocked.add((xx, yy))
    for n in m.npcs: blocked.add((n[0], n[1]))
    for c in m.chests: blocked.add((c[0], c[1]))
    for s in m.signs: blocked.add((s[0], s[1]))
    boss = set()
    for (x, y, *_ ) in m.bosses:
        for dx in (0, 1):
            for dy in (0, 1): boss.add((x + dx, y + dy))
    def bfs(passBoss):
        seen = {start}; q = deque([start])
        while q:
            x, y = q.popleft()
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                p = (x + dx, y + dy)
                if not (0 <= p[0] < m.w and 0 <= p[1] < m.h) or p in seen: continue
                if p in blocked or (p in boss and not passBoss) or m.get(*p) not in WALK: continue
                seen.add(p); q.append(p)
        return seen
    r0, r1 = bfs(False), bfs(True)
    ok = True
    def adj(p, R): return any((p[0] + dx, p[1] + dy) in R for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
    for (x, y, w, h, kind, nm, _) in m.buildings:
        d = (x + w // 2, y + h - 1)
        if (d[0], d[1] + 1) not in r0: print(name, 'porte inaccessible', nm, d); ok = False
    for n in m.npcs:
        if not adj(n, r1): print(name, 'PNJ inaccessible', n[:2], n[4]); ok = False
        if m.get(n[0], n[1]) not in WALK: print(name, 'PNJ sur tuile bloquante', n[:2])
    for c in m.chests:
        if not adj(c, r1): print(name, 'coffre inaccessible', c); ok = False
        if m.get(c[0], c[1]) not in WALK: m.set(c[0], c[1], '.' if m.get(c[0], c[1]) != 'c' else 'c')
    for s in m.signs:
        if not adj(s, r1): print(name, 'panneau inaccessible', s); ok = False
    for b in m.bosses:
        cells = [(b[0] + dx, b[1] + dy) for dx in (0, 1) for dy in (0, 1)]
        if not any(adj(c, r0) for c in cells): print(name, 'boss inaccessible', b); ok = False
    for w in m.warps:
        if not adj(w[:2], r1) and w[:2] not in r1: print(name, 'passage inaccessible', w); ok = False
    print(name, 'OK' if ok else 'ERREURS', 'zone atteinte', len(r0), '/', len(r1))
    return ok

def esc(s): return s.replace('\\', '\\\\').replace('"', '\\"')

def emit(defs):
    out = ['// Fichier généré : cartes du jeu. Vous pouvez le modifier à la main.',
           '// Chaque ligne de "rows" est une rangée de tuiles (voir la légende dans world.hpp).',
           '#include "world.hpp"', '',
           'bool tileWalkable(char c) { return c==\'.\'||c==\',\'||c==\'=\'||c==\'F\'||c==\'B\'||c==\'a\'||c==\'g\'||c==\'b\'||c==\'k\'||c==\'c\'; }',
           'bool tileEncounter(char c) { return c==\',\'||c==\'g\'||c==\'c\'; }', '',
           'static std::vector<MapDef> build() {', '  std::vector<MapDef> v;']
    for (title, theme, m) in defs:
        out.append('  {')
        out.append('    MapDef m;')
        out.append(f'    m.name = "{esc(title)}";')
        out.append(f'    m.theme = Theme::{theme};')
        out.append('    m.rows = {')
        for row in m.g: out.append(f'        "{"".join(row)}",')
        out.append('    };')
        for b in m.buildings: out.append(f'    m.buildings.push_back({{{b[0]}, {b[1]}, {b[2]}, {b[3]}, "{b[4]}", "{esc(b[5])}", 0x{b[6]:06x}}});')
        for n in m.npcs:
            lines = ', '.join(f'"{esc(l)}"' for l in n[5])
            out.append(f'    m.npcs.push_back({{{n[0]}, {n[1]}, {n[2]}, {n[3]}, "{n[4]}", {{{lines}}}}});')
        for c in m.chests: out.append(f'    m.chests.push_back({{{c[0]}, {c[1]}, "{c[2]}", {c[3]}}});')
        for s in m.signs: out.append(f'    m.signs.push_back({{{s[0]}, {s[1]}, "{esc(s[2])}"}});')
        for w in m.warps: out.append(f'    m.warps.push_back({{{w[0]}, {w[1]}, {w[2]}, {w[3]}, {w[4]}, {w[5]}}});')
        for z in m.zones:
            pool = ', '.join(f'"{p}"' for p in z[7])
            out.append(f'    m.zones.push_back({{{z[0]}, {z[1]}, {z[2]}, {z[3]}, {z[4]}, {z[5]}, {z[6]}, {{{pool}}}}});')
        for b in m.bosses: out.append(f'    m.bosses.push_back({{{b[0]}, {b[1]}, "{b[2]}", "{b[3]}"}});')
        out.append('    v.push_back(m);')
        out.append('  }')
    out += ['  return v;', '}', '', 'const std::vector<MapDef>& maps() {', '  static const std::vector<MapDef> v = build();', '  return v;', '}', '']
    return '\n'.join(out)

v, sv = vallee(); c, sc = cendres(); g, sg = grotte()
ok = check('Vallée', v, sv) & check('Cendrelune', c, sc) & check('Grotte', g, sg)
for m in (v, c, g):
    print('\n'.join(''.join(r) for r in m.g)); print()
open(sys.argv[1], 'w').write(emit([('Vallée de Brumeval', 'Vallee', v), ('Monts Cendrelune', 'Cendres', c), ('Grotte des Échos', 'Grotte', g)]))
sys.exit(0 if ok else 1)
