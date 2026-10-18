"""Intérieurs (3.3) : des chaises autour des tables, des caisses dans les boutiques, des tonneaux dans les auberges.
Les meubles (étagères en bibliothèques, tables, chaises) sont dessinés avec les sprites de la planche."""
import glob
import json
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import carte

for f in sorted(glob.glob(carte.R + 'cartes/*.json')):
    name = os.path.basename(f)[:-5]
    m = carte.load(name)
    if m['theme'] != 'interieur' or 'chapelle' in name:  # la chapelle : des bancs d'église, sans chaises
        continue
    G = carte.Grid(m['tuiles'])
    busy = {(h['x'], h['y']) for h in m['habitants']} | {(c['x'], c['y']) for c in m['coffres']}
    W, H = len(G.g[0]), len(G.g)

    def free(x, y):
        return 0 < x < W - 1 and 1 < y < H - 2 and G.g[y][x] == '.' and (x, y) not in busy and x != W // 2  # l'allée de la porte

    # chaises de part et d'autre des tables (une seule par côté de table)
    for y in range(H):
        for x in range(W):
            if G.g[y][x] != 't':
                continue
            if x > 0 and G.g[y][x - 1] != 't' and free(x - 1, y):
                G.put(x - 1, y, 'j')
            elif x + 1 < W and G.g[y][x + 1] != 't' and free(x + 1, y):
                G.put(x + 1, y, 'j')
    # boutiques : des caisses de marchandises dans les coins du fond
    if 'boutique' in name:
        for x, y in [(1, 3), (W - 2, 3)]:
            if free(x, y):
                G.put(x, y, 'C')
    m['tuiles'] = G.rows()
    carte.save(name, m)
    print(name)
    for r in m['tuiles']:
        print('   ', r)
