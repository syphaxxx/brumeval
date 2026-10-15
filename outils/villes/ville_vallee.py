"""Vallée de Brumeval : le village (coin nord-ouest de la carte, x 0..24, y 0..18) redessiné comme un vrai bourg :
une place pavée avec la fontaine au croisement de la grand-route, les bâtiments tournés vers la place ou la rue,
une ruelle devant chaque rangée de maisons, un marché, des jardins clos et une basse-cour à la sortie sud."""
import sys
sys.path.insert(0, '.')
import carte

m = carte.load('vallee')
G = carte.Grid(m['tuiles'])
W = 25  # colonnes redessinées (la 25e et au-delà ne changent pas)

# fond : herbe, bordure d'arbres au nord et à l'ouest
G.rect(0, 0, W - 1, 18, '.')
G.rect(0, 0, W - 1, 0, 'T')
G.rect(0, 0, 0, 18, 'T')
# grand-route est-ouest (rangée 9) : sortie vers Sylve-Noire à l'ouest, pont du Ru-Clair à l'est
G.rect(0, 9, W - 1, 9, '=')
# place pavée au croisement, fontaine au milieu
G.rect(8, 7, 17, 11, 'q')
G.rect(12, 8, 13, 9, 'W')
G.rect(8, 9, 17, 9, 'q')
# ruelle devant les bâtiments du nord (rangée 6) et rue du sud (rangée 16), reliées à la place
G.rect(2, 6, 22, 6, '=')
G.rect(1, 16, 23, 16, '=')
G.rect(12, 12, 12, 15, '=')
G.rect(10, 17, 10, 18, '=')  # vers le Bois Murmurant
G.rect(19, 17, 19, 18, '=')  # vers le lac
G.rect(23, 7, 23, 8, '=')    # ruelle nord-est jusqu'à la grand-route
G.rect(23, 10, 23, 15, '=')  # puis jusqu'à la rue du sud
# réverbères aux coins de la place et le long de la grand-route
for x, y in [(7, 7), (18, 7), (7, 11), (18, 11), (3, 8), (21, 10)]:
    G.put(x, y, 'L')
# marché au sud de la place : deux étals, des caisses, un banc
G.put(9, 12, 'M'), G.put(15, 12, 'M'), G.put(10, 12, 'C'), G.put(14, 12, 'Q')
G.rect(8, 12, 8, 12, 'q'), G.rect(11, 12, 13, 12, 'q'), G.rect(16, 12, 17, 12, 'q')
# jardin clos entre la chapelle et la maison de l'ancien (potager fleuri), portillon sur l'allée centrale
G.rect(8, 13, 16, 13, 'H'), G.rect(8, 15, 11, 15, 'H'), G.rect(13, 15, 16, 15, 'H')
G.rect(8, 14, 8, 14, 'H'), G.rect(16, 14, 16, 14, 'H')
G.rect(9, 14, 11, 14, 'F'), G.rect(13, 14, 15, 14, 'F')
G.put(12, 13, '='), G.put(12, 15, '=')
# abords : buissons, fleurs devant les façades, puits et caisses près de la boutique
for x, y in [(1, 2), (1, 4), (8, 2), (15, 2), (22, 2), (24, 2), (1, 12), (24, 13), (1, 14)]:
    G.put(x, y, 'U')
for x, y in [(2, 7), (8, 5), (14, 5), (22, 7), (2, 11), (8, 17), (13, 17)]:
    G.put(x, y, 'F')
G.put(22, 4, 'Y'), G.put(21, 4, 'C'), G.put(21, 5, 'v')
# basse-cour au sud-ouest (clôture), verger au sud-est
G.rect(2, 17, 7, 17, 'H'), G.put(2, 18, 'H'), G.put(7, 18, 'H'), G.put(5, 17, '.')  # portillon
for x in (21, 23):
    G.put(x, 18, 'T')
G.put(16, 18, 'T'), G.put(14, 18, 'U')
m['tuiles'] = G.rows()

# bâtiments : rangée du nord (portes sur la ruelle), rangée du sud (portes sur la rue)
B = {b['interieur'] if 'interieur' in b else b['nom']: b for b in m['batiments']}
B['vallee_soin'].update(x=3, y=2, l=5, h=4)
B['vallee_boutique'].update(x=16, y=2, l=5, h=4)
B['vallee_chapelle'].update(x=2, y=12, l=5, h=4)
B['vallee_ancien'].update(x=18, y=12, l=5, h=4)
m['batiments'] = [B['vallee_soin'], B['vallee_boutique'], B['vallee_chapelle'], B['vallee_ancien'],
                  {'x': 9, 'y': 3, 'l': 4, 'h': 3, 'genre': 'maison', 'nom': 'Maison', 'toit': '#3f6fb0'}]
for b in m['batiments']:  # sous l'emprise : de l'herbe
    G.rect(b['x'], b['y'], b['x'] + b['l'] - 1, b['y'] + b['h'] - 1, '.')
m['tuiles'] = G.rows()

# sorties des intérieurs : la case devant la porte
for b in m['batiments']:
    if 'interieur' not in b:
        continue
    door = (b['x'] + b['l'] // 2, b['y'] + b['h'])
    inner = carte.load(b['interieur'])
    for p in inner['passages']:
        if p['vers'] == 'vallee':
            p['tx'], p['ty'] = door
    carte.save(b['interieur'], inner)

# habitants, panneaux, coffres du village
pos = {('villageois', 11, 8): (10, 10), ('villageoise', 17, 10): (16, 8), ('ancien', 21, 15): (17, 15), ('maelle', 7, 15): (7, 15)}
for h in m['habitants']:
    k = (h['apparence'], h['x'], h['y'])
    if k in pos:
        h['x'], h['y'] = pos[k]
m['habitants'] += [
    {'x': 4, 'y': 18, 'apparence': 'poule', 'direction': 'gauche', 'dialogue': ['Cot cot !']},
    {'x': 6, 'y': 18, 'apparence': 'poule', 'direction': 'droite', 'dialogue': ['Cot cot cot…']},
    {'x': 22, 'y': 17, 'apparence': 'chien', 'direction': 'gauche', 'dialogue': ["Ouaf ! Le chien de l'ancien remue la queue."]},
    {'x': 14, 'y': 10, 'apparence': 'chat', 'direction': 'bas', 'dialogue': ['Le chat ronronne au bord de la fontaine.']},
    {'x': 8, 'y': 12, 'apparence': 'marchande', 'direction': 'droite',
     'dialogue': ['Des pommes du verger, des œufs frais ! Le jour du marché, tout le village passe par la place.']},
]
for s in m['panneaux']:
    if (s['x'], s['y']) == (12, 10):
        s['x'], s['y'] = 6, 10
for c in m['coffres']:
    if (c['x'], c['y']) == (2, 2):
        c['x'], c['y'] = 2, 3
carte.save('vallee', m)
print('\n'.join(G.rows()[:20]))
