"""Forgeroc (Monts Cendrelune) : bourg de mineurs. La grand-route devient une rue pavée, une placette devant les
boutiques, la forge entourée de son bois, de ses caisses et de son feu, des réverbères le long de la rue."""
import sys
sys.path.insert(0, '.')
import carte

m = carte.load('cendrelune')
G = carte.Grid(m['tuiles'])
# rue pavée (rangée 33) dans le bourg, placette devant les boutiques (rangées 31-32)
G.rect(1, 33, 27, 33, 'q')
G.rect(3, 31, 25, 32, 'q')
# rangée du sud : ruelle pavée vers la forge et la maison (rangées 39-40 déjà en chemin)
G.rect(8, 39, 21, 39, 'q')
G.rect(9, 34, 9, 34, 'q'), G.rect(16, 34, 16, 38, 'q')
# réverbères, tonneaux de l'auberge, bois et caisses de la forge, feu de forge
for x, y in [(2, 31), (10, 31), (17, 31), (25, 31), (2, 35), (25, 34)]:
    G.put(x, y, 'L')
G.put(24, 29, 'v'), G.put(25, 29, 'v'), G.put(18, 29, 'C')
G.put(12, 36, 'X'), G.put(12, 35, 'C'), G.put(5, 37, 'C'), G.put(5, 38, 'v'), G.put(13, 38, 'K')
G.put(4, 27, 'U'), G.put(25, 27, 'U'), G.put(4, 29, 'G')  # tableau d'annonces près de la maison de soin
m['tuiles'] = G.rows()
for b in m['batiments']:  # sous l'emprise : la cendre
    G.rect(b['x'], b['y'], b['x'] + b['l'] - 1, b['y'] + b['h'] - 1, 'a')
m['tuiles'] = G.rows()

m['habitants'].append({'x': 13, 'y': 37, 'apparence': 'forgeron', 'direction': 'gauche',
                       'dialogue': ["Le feu de la forge ne s'éteint jamais.", "Brann y travaille l'acier des montagnes : demande-lui, à l'enclume."]})
m['habitants'].append({'x': 22, 'y': 31, 'apparence': 'chat', 'direction': 'gauche', 'dialogue': ["Le chat de l'auberge se chauffe sur les pavés tièdes."]})
carte.save('cendrelune', m)
for f, lk in [('cendrelune_soin', 'docteur'), ('cendrelune_boutique', 'marchande'), ('cendrelune_auberge', 'barman')]:
    inner = carte.load(f)
    for h in inner['habitants']:
        if h.get('evenement') in ('soin', 'boutique2', 'auberge'):
            h['apparence'] = lk
    carte.save(f, inner)
print('\n'.join(G.rows()[25:42]))
