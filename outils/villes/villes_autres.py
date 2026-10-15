"""Givreval (Pics), le camp de Sylve-Noire et le port : places, abords et objets de la vie de tous les jours."""
import sys
sys.path.insert(0, '.')
import carte


def under_buildings(G, m, c):
    for b in m['batiments']:
        G.rect(b['x'], b['y'], b['x'] + b['l'] - 1, b['y'] + b['h'] - 1, c)


def free(G, x, y):
    return G.g[y][x] in '.,'


def put_many(G, pts, c):
    for x, y in pts:
        if free(G, x, y):
            G.put(x, y, c)


def npc(m, x, y, look, d, lines):
    m['habitants'].append({'x': x, 'y': y, 'apparence': look, 'direction': d, 'dialogue': lines})


def shopkeepers(pairs):
    for f, ev, lk in pairs:
        inner = carte.load(f)
        for h in inner['habitants']:
            if h.get('evenement') == ev:
                h['apparence'] = lk
        carte.save(f, inner)


# ---------------- Givreval (Pics) ----------------
m = carte.load('pics')
G = carte.Grid(m['tuiles'])
G.rect(27, 32, 34, 35, 'q')       # place pavée au croisement
G.rect(18, 34, 42, 34, 'q')       # grand-rue
put_many(G, [(26, 32), (35, 32), (26, 35), (35, 35), (19, 33), (41, 33)], 'L')
put_many(G, [(40, 30), (40, 29)], 'C')
put_many(G, [(40, 31)], 'v')
put_many(G, [(24, 31), (17, 31)], 'U')
put_many(G, [(28, 37)], 'Y')
put_many(G, [(33, 37)], 'Q')
G.rect(19, 40, 25, 40, 'H'), G.rect(34, 40, 40, 40, 'H')  # potagers clos derrière les maisons
put_many(G, [(16, 29), (16, 38), (43, 38), (44, 33), (15, 35)], 'T')
under_buildings(G, m, '.')
m['tuiles'] = G.rows()
npc(m, 29, 33, 'voyageuse', 'bas', ["Je viens du port, de l'autre côté du lac.", 'Ici, même les pavés ont froid !'])
npc(m, 38, 39, 'chien', 'gauche', ['Le chien a de la neige plein les moustaches.'])
carte.save('pics', m)
shopkeepers([('pics_soin', 'soin', 'docteur'), ('pics_boutique', 'boutique3', 'marchande'), ('pics_auberge', 'auberge', 'barman')])

# ---------------- Sylve-Noire : le camp des bandits ----------------
m = carte.load('sylvenoire')
G = carte.Grid(m['tuiles'])
put_many(G, [(10, 17)], 'X')                       # feu de camp
put_many(G, [(2, 15), (3, 15), (14, 19)], 'C')     # butin entassé
put_many(G, [(2, 17), (15, 19)], 'v')
put_many(G, [(9, 15)], 'J')                        # bannière des bandits
put_many(G, [(16, 21)], 'G')                       # avis de recherche
m['tuiles'] = G.rows()
carte.save('sylvenoire', m)

# ---------------- Le port ----------------
m = carte.load('port')
G = carte.Grid(m['tuiles'])
G.rect(3, 12, 33, 13, 'q')                         # quai pavé
G.rect(19, 3, 20, 11, 'q')                         # rue du phare
put_many(G, [(2, 11), (9, 11), (17, 11), (22, 11), (29, 11), (9, 14), (21, 14)], 'L')
put_many(G, [(13, 21), (14, 21), (8, 22), (9, 22), (25, 22)], 'C')   # caisses du port
put_many(G, [(15, 21), (7, 22), (26, 22)], 'v')
put_many(G, [(14, 16), (17, 16)], 'M')             # marché aux poissons
put_many(G, [(30, 14), (3, 20)], 'Q')
put_many(G, [(29, 9)], 'Y')
under_buildings(G, m, '.')
m['tuiles'] = G.rows()
npc(m, 15, 17, 'marchande', 'gauche', ['Poisson frais du lac, pêché à l\'aube !'])
npc(m, 27, 21, 'voyageuse', 'bas', ['J\'attends le bateau pour Givreval.', 'Les récifs sont dangereux par temps d\'orage.'])
npc(m, 5, 18, 'chat', 'droite', ['Le chat du port guette les barques.'])
carte.save('port', m)
for f in ('pics', 'sylvenoire', 'port'):
    print(f, 'ok')
